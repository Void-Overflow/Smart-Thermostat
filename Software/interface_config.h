#ifndef CONFIG_H
#define CONFIG_H

#include "Arduino.h"

struct config 
{
    const char *ssid = "WIFI_SSID";
    const char *password = "WIFI_PASSWORD";
    String weatherApiKey = "API KEY FROM api.openweathermap.org";
};

#endif
