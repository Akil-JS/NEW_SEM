#ifndef DEBUG_CONFIG_H
#define DEBUG_CONFIG_H

#include <ArduinoJson.h>
#include <SPIFFS.h>
#include "sdcard.h" // Include your SD card header


class DebugConfig {
private:
    static const char* DEBUG_CONFIG_FILE;
    
public:
    static bool DEBUG_SERIAL;
    static bool DEBUG_WEB;
    static bool DEBUG_BOTH;
    
    static void init();
    static void loadFromFile();
    static void saveToFile();
    static void setDebugMode(const String& mode);
    static String getDebugStatus();
};

#endif