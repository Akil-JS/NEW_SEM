#ifndef DEBUG_MACRO_H
#define DEBUG_MACRO_H

#include "DebugConfig.h"
#include "QCWebServer.h"

// Enable debug globally
#define DEBUG_devsbot

#ifdef DEBUG_devsbot
#define DEBUG(x) do { \
    static unsigned long lastDebugTime = 0; \
    static String lastDebugMessage = ""; \
    String currentMessage = String(x); \
    unsigned long currentTime = millis(); \
    \
    if (currentMessage != lastDebugMessage || (currentTime - lastDebugTime) > 1000) { \
        if (DebugConfig::DEBUG_BOTH) { \
            Serial.println(currentMessage); \
            if (QCWebServer::getInstance()) { \
                QCWebServer::getInstance()->sendSerialToWeb(currentMessage, "debug"); \
            } \
        } else if (DebugConfig::DEBUG_SERIAL) { \
            Serial.println(currentMessage); \
        } else if (DebugConfig::DEBUG_WEB) { \
            if (QCWebServer::getInstance()) { \
                QCWebServer::getInstance()->sendSerialToWeb(currentMessage, "debug"); \
            } \
        } \
        lastDebugMessage = currentMessage; \
        lastDebugTime = currentTime; \
    } \
} while(0)
#else
#define DEBUG(...)
#endif

#endif // DEBUG_MACRO_H
