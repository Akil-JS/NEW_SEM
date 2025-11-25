#include "DebugConfig.h"

const char* DebugConfig::DEBUG_CONFIG_FILE = "/debug_config.json";

// Initialize static variables - DEFAULT TO SERIAL
bool DebugConfig::DEBUG_SERIAL = true;   // DEFAULT: Serial enabled
bool DebugConfig::DEBUG_WEB = false;     // DEFAULT: Web disabled
bool DebugConfig::DEBUG_BOTH = false;    // DEFAULT: Both disabled

void DebugConfig::init() {
    if (!SPIFFS.begin(true)) {
        Serial.println("SPIFFS Mount Failed");
        return;
    }
    loadFromFile();
}

void DebugConfig::loadFromFile() {
    if (!SPIFFS.exists(DEBUG_CONFIG_FILE)) {
        // Create default config (Serial only) if file doesn't exist
        Serial.println("Debug config not found, using default: SERIAL");
        saveToFile();
        return;
    }
    
    File file = SPIFFS.open(DEBUG_CONFIG_FILE, "r");
    if (!file) {
        Serial.println("Failed to open debug config file, using default: SERIAL");
        return;
    }
    
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    
    if (error) {
        Serial.println("Failed to parse debug config JSON, using default: SERIAL");
        return;
    }
    
    DEBUG_SERIAL = doc["debug_serial"] | true;   // Default to true if not found
    DEBUG_WEB = doc["debug_web"] | false;        // Default to false if not found
    DEBUG_BOTH = doc["debug_both"] | false;      // Default to false if not found
    
    Serial.println("Debug config loaded from SPIFFS");
    Serial.printf("Current mode: SERIAL=%s, WEB=%s, BOTH=%s\n", 
                  DEBUG_SERIAL ? "ON" : "OFF",
                  DEBUG_WEB ? "ON" : "OFF", 
                  DEBUG_BOTH ? "ON" : "OFF");
}

void DebugConfig::saveToFile() {
    JsonDocument doc;
    doc["debug_serial"] = DEBUG_SERIAL;
    doc["debug_web"] = DEBUG_WEB;
    doc["debug_both"] = DEBUG_BOTH;
    doc["timestamp"] = millis();
    
    File file = SPIFFS.open(DEBUG_CONFIG_FILE, "w");
    if (!file) {
        Serial.println("Failed to create debug config file");
        return;
    }
    
    serializeJson(doc, file);
    file.close();
    Serial.println("Debug config saved to SPIFFS");
}

void DebugConfig::setDebugMode(const String& mode) {
    // Reset all flags
    DEBUG_SERIAL = false;
    DEBUG_WEB = false;
    DEBUG_BOTH = false;
    
    if (mode == "SERIAL") {
        DEBUG_SERIAL = true;
        Serial.println("Debug mode set to: SERIAL");
    } else if (mode == "WEB") {
        DEBUG_WEB = true;
        Serial.println("Debug mode set to: WEB");
    } else if (mode == "BOTH") {
        DEBUG_BOTH = true;
        Serial.println("Debug mode set to: BOTH");
    } else if (mode == "OFF") {
        Serial.println("Debug mode set to: OFF");
    }
    
    saveToFile();
}

String DebugConfig::getDebugStatus() {
    JsonDocument doc;
    doc["debug_serial"] = DEBUG_SERIAL;
    doc["debug_web"] = DEBUG_WEB;
    doc["debug_both"] = DEBUG_BOTH;
    
    if (DEBUG_BOTH) {
        doc["active_mode"] = "BOTH";
    } else if (DEBUG_SERIAL) {
        doc["active_mode"] = "SERIAL";
    } else if (DEBUG_WEB) {
        doc["active_mode"] = "WEB";
    } else {
        doc["active_mode"] = "OFF";
    }
    
    String result;
    serializeJson(doc, result);
    return result;
}