#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClient.h>
#include <Arduino_JSON.h>
#include <DHT.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include "AiEsp32RotaryEncoder.h"
#include "Arduino.h"
#include "EEPROM.h"
#include "User_Setup.h"

#include "interface_config.h"

#define EEPROM_SIZE 16

#define TFT_EN_PIN 21

#define IR_SENSOR_PIN 13

#define FAN_RELAY_PIN 27
#define COOLING_RELAY_PIN 33
#define HEATING_RELAY_PIN 32

#define ROTARY_ENCODER_A_PIN 4
#define ROTARY_ENCODER_B_PIN 26
#define ROTARY_ENCODER_BUTTON_PIN 35

WiFiServer server(80);
String header;

config keys;

unsigned long currentTime = millis();
unsigned long previousTime = 0;

TFT_eSPI tft = TFT_eSPI(240, 320);
bool updateLCDFlag = false;

int lastRotaryEncoder = HIGH;
int lastRotaryEncoderBtnPress = 0;
AiEsp32RotaryEncoder rotaryEncoder = AiEsp32RotaryEncoder(ROTARY_ENCODER_A_PIN, ROTARY_ENCODER_B_PIN, ROTARY_ENCODER_BUTTON_PIN, -1, 4);

bool cursorLocation = false; // false = tmpSet, true = mode
bool cursorSelected = false;

int timeElapsed = 0;

enum fanModes
{
  COOL = 0,
  HEAT = 1,
  OFF = 2,
  AUTO = 3
};
fanModes currentMode = OFF;
fanModes autoMode = OFF;
bool ACState = false;

int currentTemp = 0, currentHumidity = 0, outsideTemp = 0, outsideHumidity = 0, tempSet = 75;
DHT sensor(25, DHT22);
int prevStatUpdate = 0;

void updateLCD()
{
  int textClr = TFT_BLACK;
  int backgroundClr = TFT_DARKGREY;

  if (currentMode == OFF)
    backgroundClr = TFT_DARKGREY;
  else if (currentMode == COOL)
    backgroundClr = TFT_SKYBLUE;
  else if (currentMode == HEAT)
    backgroundClr = TFT_RED;
  else if (currentMode == AUTO)
    backgroundClr = TFT_VIOLET;

  tft.fillScreen(backgroundClr);
  tft.setTextColor(textClr);
  tft.setTextPadding(240);

  tft.setTextSize(2);
  tft.setCursor(65, 2, 2); // x, y, font 2
  tft.println("Currently ");
  tft.setCursor(200, 2, 2); // x, y, font 2
  tft.setTextColor((ACState) ? TFT_GREEN : TFT_MAROON, backgroundClr);
  tft.println((ACState) ? "ON" : "OFF");
  tft.setTextColor(textClr);

  tft.setCursor(30, 30, 2); // x, y, font 2
  tft.println("Temp In:");
  tft.setCursor(150, 30, 2);
  tft.println("Temp Out:");
  tft.setTextSize(3);
  tft.setCursor(80, 57, 2); // x, y, font 2
  tft.println(currentTemp);
  tft.setCursor(200, 57, 2);
  tft.println(outsideTemp);

  tft.setCursor(90, 100, 2); // x, y, font 2
  tft.setTextSize(2);
  tft.println("Temp Set:");
  tft.setTextSize(5);
  tft.setCursor(125, 125, 2); // x, y, font 2
  if (cursorLocation == false)
    tft.setTextColor(TFT_WHITE);
  tft.println(tempSet);
  tft.setTextColor(TFT_BLACK);

  tft.setTextSize(2);
  tft.setCursor(85, 200, 2); // x, y, font 2
  tft.println("Mode:");
  tft.setCursor(165, 200, 2); // x, y, font 2
  if (cursorLocation == true)
    tft.setTextColor(TFT_WHITE);
  if (currentMode == HEAT)
    tft.println("HEAT");
  else if (currentMode == OFF)
    tft.println("OFF");
  else if (currentMode == AUTO)
  {
    tft.print("AUTO: ");
    if (autoMode == HEAT)
    {
      tft.setTextColor(TFT_RED);
      tft.println("HEAT");
    }
    else if (autoMode == OFF)
    {
      tft.setTextColor(TFT_BLACK);
      tft.println("OFF");
    }
    else if (autoMode == COOL)
    {
      tft.setTextColor(TFT_SKYBLUE);
      tft.println("COOL");
    }
  }
  else
    tft.println("COOL");
  tft.setTextColor(TFT_BLACK);
}

void showTempSet()
{
  int backgroundClr = TFT_DARKGREY;

  if (currentMode == OFF)
    backgroundClr = TFT_DARKGREY;
  else if (currentMode == COOL)
    backgroundClr = TFT_SKYBLUE;
  else if (currentMode == AUTO)
    backgroundClr = TFT_VIOLET;
  else if (currentMode == HEAT)
    backgroundClr = TFT_RED;
  tft.fillScreen(backgroundClr);

  tft.setCursor(90, 100, 2); // x, y, font 2
  tft.setTextSize(2);
  tft.println("Temp Set:");
  tft.setTextSize(5);
  tft.setCursor(115, 125, 2); // x, y, font 2
  if (cursorLocation == false)
    tft.setTextColor(TFT_WHITE);
  tft.println(tempSet);
  tft.setTextColor(TFT_BLACK);
}

void showModeSet()
{
  int backgroundClr = TFT_DARKGREY;

  if (currentMode == OFF)
    backgroundClr = TFT_DARKGREY;
  else if (currentMode == COOL)
    backgroundClr = TFT_SKYBLUE;
  else if (currentMode == AUTO)
    backgroundClr = TFT_VIOLET;
  else if (currentMode == HEAT)
    backgroundClr = TFT_RED;
  tft.fillScreen(backgroundClr);

  tft.setCursor(90, 100, 2); // x, y, font 2
  tft.setTextSize(2);
  tft.println("Mode:");
  tft.setTextSize(5);
  tft.setCursor(115, 125, 2); // x, y, font 2
  if (cursorLocation == false)
    tft.setTextColor(TFT_WHITE);
  if (currentMode == HEAT)
    tft.println("HEAT");
  else if (currentMode == OFF)
    tft.println("OFF");
  else if (currentMode == AUTO)
    tft.print("AUTO");

  else
    tft.println("COOL");
  tft.setTextColor(TFT_BLACK);
}

void IRAM_ATTR readEncoderISR()
{
  rotaryEncoder.readEncoder_ISR();
}

void setup()
{
  sensor.begin();

  pinMode(ROTARY_ENCODER_A_PIN, INPUT_PULLUP);
  pinMode(ROTARY_ENCODER_B_PIN, INPUT_PULLUP);
  pinMode(ROTARY_ENCODER_BUTTON_PIN, INPUT_PULLUP);

  pinMode(TFT_EN_PIN, OUTPUT);
  pinMode(IR_SENSOR_PIN, INPUT_PULLUP);
  digitalWrite(TFT_EN_PIN, HIGH);

  rotaryEncoder.begin();
  rotaryEncoder.setup(readEncoderISR);
  // rotaryEncoder.setBoundaries(0, 1000, false);
  rotaryEncoder.setAcceleration(250);

  pinMode(FAN_RELAY_PIN, OUTPUT);
  pinMode(COOLING_RELAY_PIN, OUTPUT);
  pinMode(HEATING_RELAY_PIN, OUTPUT);

  digitalWrite(FAN_RELAY_PIN, LOW);
  digitalWrite(COOLING_RELAY_PIN, LOW);
  digitalWrite(HEATING_RELAY_PIN, LOW);

  WiFi.begin(keys.ssid, keys.password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    if (millis() >= 123000)
      break;
  }
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);
  server.begin();

  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_DARKGREY);
  updateLCD();

  if (EEPROM.begin(EEPROM_SIZE))
  {
    tempSet = EEPROM.read(0);
    currentMode = (fanModes)EEPROM.read(1);
  }
}

void loop()
{
  if (currentMode != OFF)
  {
    if (currentMode == COOL)
    {
      if (currentTemp > tempSet && ACState == false)
      {
        updateLCDFlag = true;
        ACState = true;
        digitalWrite(FAN_RELAY_PIN, HIGH);
        digitalWrite(COOLING_RELAY_PIN, HIGH);
        digitalWrite(HEATING_RELAY_PIN, LOW);
      }
      else if (currentTemp <= tempSet - 1 && ACState == true)
      {
        updateLCDFlag = true;
        ACState = false;
        digitalWrite(FAN_RELAY_PIN, LOW);
        digitalWrite(COOLING_RELAY_PIN, LOW);
        digitalWrite(HEATING_RELAY_PIN, LOW);
      }
    }
    else if (currentMode == HEAT)
    {
      if (currentTemp < tempSet && ACState == false)
      {
        updateLCDFlag = true;
        ACState = true;
        digitalWrite(FAN_RELAY_PIN, HIGH);
        digitalWrite(COOLING_RELAY_PIN, LOW);
        digitalWrite(HEATING_RELAY_PIN, HIGH);
      }
      else if (currentTemp >= tempSet + 1 && ACState == true)
      {
        updateLCDFlag = true;
        ACState = false;
        digitalWrite(FAN_RELAY_PIN, LOW);
        digitalWrite(COOLING_RELAY_PIN, LOW);
        digitalWrite(HEATING_RELAY_PIN, LOW);
      }
    }
    if (currentMode == AUTO)
    {
      if (currentTemp > tempSet + 1 && ACState == false)
      {
        updateLCDFlag = true;
        ACState = true;
        autoMode = COOL;
        digitalWrite(FAN_RELAY_PIN, HIGH);
        digitalWrite(COOLING_RELAY_PIN, HIGH);
        digitalWrite(HEATING_RELAY_PIN, LOW);
      }
      else if (currentTemp < tempSet - 1 && ACState == false)
      {
        updateLCDFlag = true;
        ACState = true;
        autoMode = HEAT;
        digitalWrite(FAN_RELAY_PIN, HIGH);
        digitalWrite(COOLING_RELAY_PIN, LOW);
        digitalWrite(HEATING_RELAY_PIN, HIGH);
      }
      else if ((currentTemp == tempSet || currentTemp == tempSet + 1 || currentTemp == tempSet - 1) && ACState == true)
      {
        updateLCDFlag = true;
        ACState = false;
        autoMode = OFF;
        digitalWrite(FAN_RELAY_PIN, LOW);
        digitalWrite(COOLING_RELAY_PIN, LOW);
        digitalWrite(HEATING_RELAY_PIN, LOW);
      }
    }
  }
  else
  {
    if (ACState != false)
    {
      digitalWrite(FAN_RELAY_PIN, LOW);
      digitalWrite(COOLING_RELAY_PIN, LOW);
      digitalWrite(HEATING_RELAY_PIN, LOW);
      updateLCDFlag = true;
    }
    ACState = false;
  }

  if (millis() - lastRotaryEncoderBtnPress >= 1500)
  {
    if (rotaryEncoder.isEncoderButtonClicked())
    {
      cursorSelected = !cursorSelected;
      timeElapsed = millis();
      if (cursorSelected == true)
      {
        if (cursorLocation == false)
          showTempSet();
        else
          showModeSet();
      }
      else
        updateLCD();
    }
  }

  if (digitalRead(IR_SENSOR_PIN) == HIGH)
    timeElapsed = millis();

  if (millis() - timeElapsed >= 60000)
  {
    if (cursorSelected == true)
      cursorSelected = false;
    else
    {
      digitalWrite(TFT_EN_PIN, LOW);
      updateLCDFlag = false;
    }
  }
  else
    digitalWrite(TFT_EN_PIN, HIGH);

  if (cursorSelected == false)
  {
    if (rotaryEncoder.encoderChanged()) // If the knob moves
    {
      timeElapsed = millis();
      cursorLocation = !cursorLocation;
      updateLCDFlag = true;
    }
    if (updateLCDFlag == true)
    {
      updateLCD();
      updateLCDFlag = false;
    }
  }
  else
  {
    if (rotaryEncoder.encoderChanged()) // If the knob moves
    {
      timeElapsed = millis();
      if (cursorLocation == false)
      {
        if (rotaryEncoder.readEncoder() < lastRotaryEncoder)
          tempSet--;
        else if (rotaryEncoder.readEncoder() > lastRotaryEncoder)
          tempSet++;
        lastRotaryEncoder = rotaryEncoder.readEncoder();
        EEPROM.write(0, tempSet);
        EEPROM.commit();
        showTempSet();
      }
      else
      {
        if (currentMode == COOL)
        {
          currentMode = HEAT;
          ACState = false;
          digitalWrite(FAN_RELAY_PIN, LOW);
          digitalWrite(COOLING_RELAY_PIN, LOW);
          digitalWrite(HEATING_RELAY_PIN, LOW);
        }
        else if (currentMode == HEAT)
        {
          currentMode = AUTO;
          ACState = false;
          digitalWrite(FAN_RELAY_PIN, LOW);
          digitalWrite(COOLING_RELAY_PIN, LOW);
          digitalWrite(HEATING_RELAY_PIN, LOW);
        }
        else if (currentMode == AUTO)
          currentMode = OFF;
        else if (currentMode == OFF)
          currentMode = COOL;
        EEPROM.write(1, currentMode);
        EEPROM.commit();
        showModeSet();
      }
    }
  }

  WiFiClient client = server.available();
  if (millis() - prevStatUpdate >= 10000) // UPDATES STATS FROM SENSORS AND FROM WEB
  {
    if (WiFi.status() == WL_CONNECTED)
    {
      HTTPClient http;
      http.begin(client, keys.weatherApiKey);
      int httpResponseCode = http.GET();

      String payload = "{}";
      payload = http.getString(); // stores server value as json

      if (http.GET() > 0) // no error with connecting to server
      {
        JSONVar myObject = JSON.parse(payload);
        int *receivedTemp = new int((int)(((double)(myObject["main"]["temp"]) - 273.15f) * (9.0f / 5.0f) + 32.0f));

        if (outsideTemp != *receivedTemp)
          updateLCDFlag = true;

        outsideTemp = (*receivedTemp < 120) ? *receivedTemp : 0;
        outsideHumidity = ((int)myObject["main"]["humidity"] < 101) ? (int)myObject["main"]["humidity"] : 0;

        delete receivedTemp;
      }
      else
      {
        if (outsideTemp != 0)
          updateLCDFlag = true;
        outsideTemp = 0;
        outsideHumidity = 0;
      }
      http.end();
    }
    else
    {
      if (outsideTemp != 0)
        updateLCDFlag = true;
      outsideTemp = 0;
      outsideHumidity = 0;
    }
    int *tempRead = new int(sensor.readTemperature(true));
    int *humRead = new int(sensor.readHumidity());

    if (isnan(*tempRead))
    { // fails to read temp
      if (currentTemp != 0)
        updateLCDFlag = true;
      currentTemp = 0;
    }
    else
    {
      if (currentTemp != *tempRead)
        updateLCDFlag = true;
      currentTemp = *tempRead;
    }
    if (isnan(*humRead)) // fails to read humiditity
      currentHumidity = 0;

    else
      currentHumidity = *humRead;

    delete tempRead;
    delete humRead;
    prevStatUpdate = millis();
  }

  if (client)
  {
    String currentLine = "";
    currentTime = millis();
    previousTime = currentTime;
    while (client.connected() && currentTime - previousTime <= 2000)
    {
      currentTime = millis();
      if (client.available())
      {
        char c = client.read();
        header += c;
        if (c == '\n')
        {
          if (currentLine.length() == 0)
          {
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println("Connection: close");
            client.println();

            if (header.indexOf("GET /heaterON") >= 0)
            {
              ACState = false;
              digitalWrite(FAN_RELAY_PIN, LOW);
              digitalWrite(COOLING_RELAY_PIN, LOW);
              digitalWrite(HEATING_RELAY_PIN, LOW);
              currentMode = HEAT;
              EEPROM.write(1, currentMode);
              EEPROM.commit();
              updateLCDFlag = true;
            }
            else if (header.indexOf("GET /coolerON") >= 0)
            {
              currentMode = COOL;
              EEPROM.write(1, currentMode);
              EEPROM.commit();
              updateLCDFlag = true;
            }
            else if (header.indexOf("GET /autoON") >= 0)
            {
              ACState = false;
              digitalWrite(FAN_RELAY_PIN, LOW);
              digitalWrite(COOLING_RELAY_PIN, LOW);
              digitalWrite(HEATING_RELAY_PIN, LOW);
              currentMode = AUTO;
              EEPROM.write(1, currentMode);
              EEPROM.commit();
              updateLCDFlag = true;
            }
            else if (header.indexOf("GET /fanOFF") >= 0)
            {
              currentMode = OFF;
              EEPROM.write(1, currentMode);
              EEPROM.commit();
              updateLCDFlag = true;
            }

            if (header.indexOf("GET /tempUP") >= 0)
            {
              tempSet++;
              updateLCDFlag = true;
              EEPROM.write(0, tempSet);
              EEPROM.commit();
            }
            else if (header.indexOf("GET /tempDOWN") >= 0)
            {
              tempSet--;
              updateLCDFlag = true;
              EEPROM.write(0, tempSet);
              EEPROM.commit();
            }

            client.println("<!DOCTYPE html><html>");
            client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"> <Title>Air Condition</Title>");
            client.println("<link rel=\"icon\" href=\"data:,\">");
            client.println("<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}");
            client.println(".modeOffButton { background-color: grey; border: none; color: white; padding: 10px 22px;");
            client.println("text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}");
            client.println(".modeHeatButton {background-color: red;}");
            client.println(".modeCoolButton {background-color: #00FFFF;}");
            client.println(".modeAutoButton {background-color: #EE82EE;}");
            client.println(".upButton { background-color: green; border: none; color: white; padding: 25px 40px;");
            client.println("text-decoration: none; font-size: 30px; margin: 5px; cursor: pointer;}");
            client.println(".downButton {background-color: red;}</style></head>");

            client.println("<body><h1>Remote Air Condition</h1>");
            client.println("<hr><hr>");

            if (ACState == true)
              client.println("<h2><p><b>AC is currently: <b style = \"color: green\">ON</b></p></h3>");
            else
              client.println("<p><b>AC is currently: <b style = \"color: red\">OFF</b></p>");

            client.println("<hr><p style=\"font-size:30px;\"><b>Temp In&nbsp&nbsp&nbsp&nbsp&nbspTemp Out</b></p>");
            client.println("<h1><b style=\"font-size:50px;>\"> " + String(currentTemp) + "</b><sup>o</sup>F&nbsp&nbsp&nbsp&nbsp&nbsp<b style=\"font-size:50px;>\">" + String(outsideTemp) + "</b><sup>o</sup>F</ h1>");
            client.println("<p style=\"font-size:30px;\"><b>Humid In&nbsp&nbsp&nbsp&nbsp&nbspHumid Out</b></p>");
            client.println("<h1><b style=\"font-size:50px;>\"> " + String(currentHumidity) + "</b>%&nbsp&nbsp&nbsp&nbsp&nbsp<b style=\"font-size:50px;>\">" + String(outsideHumidity) + "</b>%</ h1><hr>");

            client.println("<p style=\"font-size:30px;\"><b>Temp Set</b></p>");
            client.println("<h1><b style=\"font-size:75px;>\"> " + String(tempSet) + "</b><sup>o</sup>F</ h1>");
            client.println("<p><a href=\"/tempDOWN\"> <button class=\"upButton downButton\">&darr;</button></a><a href=\"/tempUP\"><button class=\"upButton\">&uarr;</button></a></p>");

            client.println("<p style=\"font-size:30px;>\"><b>Mode</b></p>");
            if (currentMode == COOL)
              client.println("<p><a href=\"/heaterON\"><button class=\"modeOffButton modeCoolButton\">COOL</button></a></p>");
            else if (currentMode == HEAT)
              client.println("<p><a href=\"/autoON\"><button class=\"modeOffButton modeHeatButton\">HEAT</button></a></p>");
            else if (currentMode == AUTO)
            {
              if (currentTemp < tempSet - 1)
                client.println("<p><a href=\"/fanOFF\"><button class=\"modeOffButton modeAutoButton\">AUTO: HEAT</button></a></p>");
              else if (currentTemp > tempSet + 1)
                client.println("<p><a href=\"/fanOFF\"><button class=\"modeOffButton modeAutoButton\">AUTO: COOL</button></a></p>");
              else
                client.println("<p><a href=\"/fanOFF\"><button class=\"modeOffButton modeAutoButton\">AUTO: OFF</button></a></p>");
            }
            else if (currentMode == OFF)
              client.println("<p><a href=\"/coolerON\"><button class=\"modeOffButton\">OFF</button></a></p>");

            client.println("<hr><hr>");
            client.println("<h3 style = \"color: red\"><i><b>Comyar Dehlavi 2023</i></b></h3>");
            client.println("</body></html>");
            client.println();
            break;
          }
          else
            currentLine = "";
        }
        else if (c != '\r')
          currentLine += c;
      }
    }

    header = "";
    client.stop();
  }
}