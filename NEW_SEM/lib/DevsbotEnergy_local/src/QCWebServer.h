/*
 * QCWebServer.h
 * 
 * QC Testing Web Server Library for NTS IoT Gateway
 * Provides web-based quality control testing functionality
 * 
 * Author: NTS Development Team
 * Version: 1.0
 */

#ifndef QC_WEBSERVER_H
#define QC_WEBSERVER_H

#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <AsyncTCP.h>
#include <Preferences.h>
#include <SD.h>
#include <SPI.h>
#include <Wire.h>
#include <ModbusMaster.h>
// #include <RTClib.h>
#include <ETH.h>
#include <algorithm>
#include "esp_system.h"
#include "DevsbotEnergyLocal.h"
#include "HardwareRTC.h" // <-- ADDED: Include the new RTC manager
#include <nvs_flash.h>
#include <nvs.h>


class QCWebServer {
private:
    AsyncWebServer* server;
    AsyncWebSocket* ws;
    Preferences prefs;
    ModbusMaster* modbus;
    //RTC_DS3231* rtc;
    
    // Configuration data from Devsbot
    Devsbot* devsbotRef;
    
    // Dynamic pin configuration
    int* dioPins;
    String* dioModes;
    int numDioPins;
    
    // RS485 configuration
    uint8_t* rs485SlaveIds;
    int numRs485Slaves;
    uint16_t* rs485Registers;
    int numRs485Registers;
    uint32_t rs485BaudRate;
    uint8_t rs485DataBits;
    uint8_t rs485Parity;
    uint8_t rs485StopBits;
    int rs485TxPin;
    int rs485RxPin;
    int rs485DePin;
    int rs485RePin;
    
    // SD Card configuration
    int sdCsPin;
    int sdMosiPin;
    int sdMisoPin;
    int sdSckPin;
    
    // I2C configuration
    int i2cSdaPin;
    int i2cSclPin;
    
    // Debug flag
    bool debugVerbose;
    
    // Helper methods
    void logRequest(const char* tag, AsyncWebServerRequest *r, const String& body = "");
    int getResetReason();
    void setupModbusCallbacks();


        // New members for debug logging
    String debugBuffer;
    unsigned long lastDebugSend;
    static const size_t MAX_DEBUG_BUFFER_SIZE = 1024;
    static const unsigned long DEBUG_SEND_INTERVAL = 100; // Send every 100ms
    
    // Queue for storing debug messages
    std::vector<String> debugQueue;
    static const size_t MAX_DEBUG_QUEUE_SIZE = 50;

    String postBodyBuffer = "";
    
public:
    QCWebServer(AsyncWebServer* webServer, AsyncWebSocket* webSocket);
    ~QCWebServer();

    void sendLogToWeb(const String &msg);
    static QCWebServer* activeInstance;
    
    // Configuration methods
    void setDevsbotReference(Devsbot* devsbot);
    void setDIOConfiguration(int* pins, String* modes, int numPins);
    void setRS485Configuration(uint8_t* slaveIds, int numSlaves, 
                              uint16_t* registers, int numRegisters,
                              uint32_t baudRate, uint8_t dataBits, 
                              uint8_t parity, uint8_t stopBits,
                              int txPin, int rxPin, int dePin, int rePin);
    void setSDCardConfiguration(int csPin, int mosiPin, int misoPin, int sckPin);
    void setI2CConfiguration(int sdaPin, int sclPin);
    void setModbusReference(ModbusMaster* modbusRef);
    //void setRTCReference(RTC_DS3231* rtcRef);
    void setDebugMode(bool verbose);
    
    // Initialization
    void begin();
    void setupRoutes();
    void setupWebSocket();
    
    // Handler methods for QC tests
    void handleGetHardwareConfig(AsyncWebServerRequest *request);
    void handleWiFiStatus(AsyncWebServerRequest *request);
    void handleSystemInfo(AsyncWebServerRequest *request);
    void handleRestart(AsyncWebServerRequest *request);
    
    // QC Test handlers
    void handleTestRS485(AsyncWebServerRequest *request);
    void handleTestDIO(AsyncWebServerRequest *request);
    void handleTestSDCard(AsyncWebServerRequest *request);
    void handleTestRTC(AsyncWebServerRequest *request);
    void handleTestEthernet(AsyncWebServerRequest *request);
    void handleTestGPRS(AsyncWebServerRequest *request);
    void handleTestWiFi(AsyncWebServerRequest *request);
    void handleTestMemory(AsyncWebServerRequest *request);
    
    // Configuration management handlers
    void handleConfigBackup(AsyncWebServerRequest *request);
    void handleConfigRestore(AsyncWebServerRequest *request, const String &body);
    void handleServerConfig(AsyncWebServerRequest *request, const String &body);
    void handleProvision(AsyncWebServerRequest *request, const String &body);

    // Utility handlers
    void handleClearLogs(AsyncWebServerRequest *request);
    void handleFormatSPIFFS(AsyncWebServerRequest *request);
    void handleEraseProvision(AsyncWebServerRequest *request);
    void handleFactoryReset(AsyncWebServerRequest *request);
    void handleATCommand(AsyncWebServerRequest *request);
    void handleEraseEEPROM(AsyncWebServerRequest *request);
    void handleEraseSPIFFS(AsyncWebServerRequest *request);
    
    // WebSocket event handler
    void onWsEvent(AsyncWebSocket *s, AsyncWebSocketClient *c,
                   AwsEventType type, void *arg, uint8_t *data, size_t len);
    
    // Loop method for periodic tasks
    void loop();
    
    // Status broadcast
    void broadcastSystemStatus();


        // Enhanced debug methods
    void sendSerialToWeb(const String &message, const String &level = "info");
    void flushDebugBuffer();
    void queueDebugMessage(const String &message, const String &level = "info");
    void acknowledgeMessage(const String &message);
     void handleAcknowledge(AsyncWebServerRequest *request);
    // Static method for global access to debug logging
    static QCWebServer* getInstance() { return activeInstance; }
    void handleATCommandWS(AsyncWebSocketClient *client, const String &command);
    void handleDebugMessages(AsyncWebServerRequest *request);
    void handleWebSocketInfo(AsyncWebServerRequest *request);
    void addDebugMessage(const String &message, const String &level);
    void handleDebugClearSent(AsyncWebServerRequest *request, const String &body);
    void cleanupOldMessages();
};

#endif // QC_WEBSERVER_H