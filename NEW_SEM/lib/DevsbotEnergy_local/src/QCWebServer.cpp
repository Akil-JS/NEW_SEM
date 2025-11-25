

/*
 * QCWebServer.cpp
 * 
 * QC Testing Web Server Library Implementation
 * 
 * Author: NTS Development Team
 * Version: 1.0
 */

#include "QCWebServer.h"
#include "DebugConfig.h"

// --- THIS IS THE FIX ---
// Make the global rtcManager object available to this file.
extern HardwareRTC rtcManager;
// --- END OF FIX ---

// Make sure these are declared extern if they are global
extern sdcard card;

// FIXED: Updated debug message structure
struct DebugMessage {
    String message;
    String level;
    unsigned long timestamp;
    bool sent;
    uint32_t id;
};

static std::vector<DebugMessage> debugMessages;
static uint32_t nextMessageId = 1;
static const size_t MAX_DEBUG_MESSAGES = 100;

QCWebServer* QCWebServer::activeInstance = nullptr;

QCWebServer::QCWebServer(AsyncWebServer* webServer, AsyncWebSocket* webSocket) 
    : server(webServer), ws(webSocket), devsbotRef(nullptr), modbus(nullptr), //rtc(nullptr),
      dioPins(nullptr), dioModes(nullptr), numDioPins(0),
      rs485SlaveIds(nullptr), numRs485Slaves(0), rs485Registers(nullptr), numRs485Registers(0),
      rs485BaudRate(9600), rs485DataBits(8), rs485Parity(0), rs485StopBits(1),
      rs485TxPin(17), rs485RxPin(16), rs485DePin(4), rs485RePin(2),
      sdCsPin(5), sdMosiPin(23), sdMisoPin(19), sdSckPin(18),
      i2cSdaPin(21), i2cSclPin(22), debugVerbose(true) 
{
    activeInstance = this;  
}

QCWebServer::~QCWebServer() {
    // Destructor - cleanup if needed
}

void QCWebServer::sendLogToWeb(const String &msg) {
    if (ws && ws->count() > 0) {
        sendSerialToWeb(msg, "info");
    }
}

// FIXED: New method to add debug messages to persistent storage
void QCWebServer::addDebugMessage(const String &message, const String &level) {
    DebugMessage msg;
    msg.message = message;
    msg.level = level;
    msg.timestamp = millis();
    msg.sent = false;
    msg.id = nextMessageId++;
    
    debugMessages.push_back(msg);
    
    // Clean up old messages
    if (debugMessages.size() > MAX_DEBUG_MESSAGES) {
        debugMessages.erase(debugMessages.begin());
    }
    
    // Serial.printf("[DEBUG] Added message ID %d: %s\n", msg.id, message.substring(0, 30).c_str());
}


// FIXED: Improved sendSerialToWeb - no more duplicates
void QCWebServer::sendSerialToWeb(const String &message, const String &level) {
    // Always add to persistent storage first
    addDebugMessage(message, level);
    
    // Send to currently connected WebSocket clients
    if (ws && ws->count() > 0) {
        JsonDocument doc;
        doc["type"] = "esp32_debug";
        doc["level"] = level;
        doc["message"] = message;
        doc["timestamp"] = millis();
        doc["source"] = "ESP32";
        doc["id"] = nextMessageId - 1; // Use the ID of the message just added
        
        String jsonStr;
        serializeJson(doc, jsonStr);
        
        ws->textAll(jsonStr);
        Serial.printf("[WS] Sent to %d clients: %s\n", ws->count(), message.substring(0, 50).c_str());
    }
}


void QCWebServer::queueDebugMessage(const String &message, const String &level) {
    JsonDocument doc;
    doc["type"] = "esp32_debug";
    doc["level"] = level;
    doc["message"] = message;
    doc["timestamp"] = millis();
    doc["source"] = "ESP32";
    
    String jsonStr;
    serializeJson(doc, jsonStr);
    
    debugQueue.push_back(jsonStr);
    
    // Keep queue size manageable
    if (debugQueue.size() > MAX_DEBUG_QUEUE_SIZE) {
        debugQueue.erase(debugQueue.begin());
    }
}

void QCWebServer::acknowledgeMessage(const String &message) {
    for (auto it = debugQueue.begin(); it != debugQueue.end(); ++it) {
        if (*it == message) {
            debugQueue.erase(it);
            Serial.printf("[WS] Acknowledged and removed message: %s\n", message.c_str());
            break;
        }
    }
}
void QCWebServer::setDevsbotReference(Devsbot* devsbot) {
    devsbotRef = devsbot;
}

void QCWebServer::setDIOConfiguration(int* pins, String* modes, int numPins) {
    dioPins = pins;
    dioModes = modes;
    numDioPins = numPins;
}

void QCWebServer::setRS485Configuration(uint8_t* slaveIds, int numSlaves, 
                                       uint16_t* registers, int numRegisters,
                                       uint32_t baudRate, uint8_t dataBits, 
                                       uint8_t parity, uint8_t stopBits,
                                       int txPin, int rxPin, int dePin, int rePin) {
    rs485SlaveIds = slaveIds;
    numRs485Slaves = numSlaves;
    rs485Registers = registers;
    numRs485Registers = numRegisters;
    rs485BaudRate = baudRate;
    rs485DataBits = dataBits;
    rs485Parity = parity;
    rs485StopBits = stopBits;
    rs485TxPin = txPin;
    rs485RxPin = rxPin;
    rs485DePin = dePin;
    rs485RePin = rePin;
}

void QCWebServer::setSDCardConfiguration(int csPin, int mosiPin, int misoPin, int sckPin) {
    sdCsPin = csPin;
    sdMosiPin = mosiPin;
    sdMisoPin = misoPin;
    sdSckPin = sckPin;
}

void QCWebServer::setI2CConfiguration(int sdaPin, int sclPin) {
    i2cSdaPin = sdaPin;
    i2cSclPin = sclPin;
}

void QCWebServer::setModbusReference(ModbusMaster* modbusRef) {
    modbus = modbusRef;
}

// void QCWebServer::setRTCReference(RTC_DS3231* rtcRef) {
//     rtc = rtcRef;
// }

void QCWebServer::setDebugMode(bool verbose) {
    debugVerbose = verbose;
}

void QCWebServer::logRequest(const char* tag, AsyncWebServerRequest *r, const String& body) {
    if (!body.isEmpty() && debugVerbose) {
        Serial.printf("[%s] %s\n", tag, body.c_str());
    }
}

int QCWebServer::getResetReason() {
    #ifdef ESP32
    esp_reset_reason_t reason = esp_reset_reason();
    
    switch(reason) {
        case ESP_RST_POWERON:    return 1;
        case ESP_RST_SW:         return 3;
        case ESP_RST_PANIC:      return 5;
        case ESP_RST_INT_WDT:    return 4;
        case ESP_RST_TASK_WDT:   return 4;
        case ESP_RST_WDT:        return 4;
        case ESP_RST_DEEPSLEEP:  return 7;
        case ESP_RST_BROWNOUT:   return 6;
        case ESP_RST_SDIO:       return 8;
        default:                 return 1;
    }
    #else
    return 1;
    #endif
}

void QCWebServer::setupModbusCallbacks() {
    if (modbus) {
        modbus->preTransmission([]() {
            // This will be set by the calling code with proper pin references
        });
        modbus->postTransmission([]() {
            // This will be set by the calling code with proper pin references
        });
    }
}

void QCWebServer::begin() {
    Serial.println("[QCWebServer] Initializing QC Web Server Library");

    // --- NEW: Increment Boot Count ---
    prefs.begin("system", false); // Open in Read/Write mode
    uint32_t bootCount = prefs.getUInt("bootCount", 0);
    bootCount++;
    prefs.putUInt("bootCount", bootCount);
    prefs.end();
    Serial.printf("[QCWebServer] Boot Count updated to: %u\n", bootCount);
    // ---------------------------------
    setupRoutes();
    setupWebSocket();
}

void QCWebServer::setupRoutes() 
{
    // Hardware and system info
    server->on("/hardware/config", HTTP_GET, [this](AsyncWebServerRequest *request) {
        this->handleGetHardwareConfig(request);
    });
    
    server->on("/wifi/status", HTTP_GET, [this](AsyncWebServerRequest *request) {
        this->handleWiFiStatus(request);
    });
    
    server->on("/system/info", HTTP_GET, [this](AsyncWebServerRequest *request) {
        this->handleSystemInfo(request);
    });
    
    server->on("/restart", HTTP_POST, [this](AsyncWebServerRequest *request) {
        this->handleRestart(request);
    });
    
    // QC Test routes
    server->on("/test/rs485", HTTP_POST, [this](AsyncWebServerRequest *request) {
        this->handleTestRS485(request);
    });
    
    server->on("/test/dio", HTTP_POST, [this](AsyncWebServerRequest *request) {
        this->handleTestDIO(request);
    });
    
    server->on("/test/sdcard", HTTP_POST, [this](AsyncWebServerRequest *request) {
        this->handleTestSDCard(request);
    });
    
    server->on("/test/rtc", HTTP_POST, [this](AsyncWebServerRequest *request) {
        this->handleTestRTC(request);
    });
    
    server->on("/test/ethernet", HTTP_POST, [this](AsyncWebServerRequest *request) {
        this->handleTestEthernet(request);
    });
    
    server->on("/test/gprs", HTTP_POST, [this](AsyncWebServerRequest *request) {
        this->handleTestGPRS(request);
    });
    
    server->on("/test/wifi", HTTP_POST, [this](AsyncWebServerRequest *request) {
        this->handleTestWiFi(request);
    });
    
    server->on("/test/memory", HTTP_POST, [this](AsyncWebServerRequest *request) {
        this->handleTestMemory(request);
    });
    
    // Configuration management
    server->on("/config/backup", HTTP_GET, [this](AsyncWebServerRequest *request) {
        this->handleConfigBackup(request);
    });
    
    server->on("/logs/clear", HTTP_POST, [this](AsyncWebServerRequest *request) {
        this->handleClearLogs(request);
    });
    
    server->on("/spiffs/format", HTTP_POST, [this](AsyncWebServerRequest *request) {
        this->handleFormatSPIFFS(request);
    });
    
    // server->on("/at/command", HTTP_POST, [this](AsyncWebServerRequest *request) {
    //     this->handleATCommand(request);
    // });

    // server->on("/at/command", HTTP_GET, [this](AsyncWebServerRequest *request) {
    //     this->handleATCommand(request);
    // });
    
    server->on("/erase/eeprom", HTTP_POST, [this](AsyncWebServerRequest *request) {
        this->handleEraseEEPROM(request);
    });
    
    server->on("/erase/spiffs", HTTP_POST, [this](AsyncWebServerRequest *request) {
        this->handleEraseSPIFFS(request);
    });
    
    server->on("/erase/provision", HTTP_POST, [this](AsyncWebServerRequest *request) {
        this->handleEraseProvision(request);
    });
    
    server->on("/factory/reset", HTTP_POST, [this](AsyncWebServerRequest *request) {
        this->handleFactoryReset(request);
    });
    server->on("/acknowledge", HTTP_POST, [this](AsyncWebServerRequest *request) {
        this->handleAcknowledge(request);
    });


    // Handle AT commands - NEW approach
    server->on("/at/command", HTTP_POST, 
        [this](AsyncWebServerRequest *request) {
            // The POST handler - body will be in postBodyBuffer
            this->handleATCommand(request);
        },
        NULL, // No upload handler
        [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            // This captures the POST body
            if (index == 0) {
                postBodyBuffer = ""; // Reset buffer for new request
            }
            // Append received data
            for (size_t i = 0; i < len; i++) {
                postBodyBuffer += (char)data[i];
            }
            
            Serial.printf("[BODY] Received %d bytes, total so far: %d\n", len, postBodyBuffer.length());
        }
    );
    
    // Also handle GET requests  
    server->on("/at/command", HTTP_GET, [this](AsyncWebServerRequest *request) {
        this->handleATCommand(request);
    });

    
    // Body handling routes
    server->on("/config/restore", HTTP_POST,
        [](AsyncWebServerRequest *r) {},
        nullptr,
        [this](AsyncWebServerRequest *r, uint8_t *data, size_t len, size_t index, size_t total) {
            static String body;
            if (index == 0) body = "";
            body += String((char*)data, len);
            if (index + len == total) {
                this->handleConfigRestore(r, body);
            }
        });
    
    server->on("/server/config", HTTP_POST,
        [](AsyncWebServerRequest *r) {},
        nullptr,
        [this](AsyncWebServerRequest *r, uint8_t *data, size_t len, size_t index, size_t total) {
            static String body;
            if (index == 0) body = "";
            body += String((char*)data, len);
            if (index + len == total) {
                this->handleServerConfig(r, body);
            }
        });
    
        server->on("/provision", HTTP_POST,
            [](AsyncWebServerRequest *r) {},
            nullptr,
            [this](AsyncWebServerRequest *r, uint8_t *data, size_t len, size_t index, size_t total) {
                static String body;
                if (index == 0) body = "";
                body += String((char*)data, len);
                if (index + len == total) {
                    this->handleProvision(r, body);
                }
            });
    // Debug messages endpoint for the Python Flask app
    server->on("/debug/messages", HTTP_GET, [this](AsyncWebServerRequest *request) {
        this->handleDebugMessages(request);
    });

    server->on("/debug/clear_sent", HTTP_POST,
        [this](AsyncWebServerRequest *request) {
            // This is the onRequest handler (called immediately)
        },
        nullptr, // No file upload handler
        [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            // Collect POST body
            if (index == 0) {
                request->_tempObject = new String();
            }
            String *body = reinterpret_cast<String *>(request->_tempObject);
            for (size_t i = 0; i < len; i++) {
                (*body) += (char)data[i];
            }

            if (index + len == total) {
                this->handleDebugClearSent(request, *body);
                delete body;
                request->_tempObject = nullptr;
            }
        }
    );

    
    // WebSocket endpoint info
    server->on("/ws/info", HTTP_GET, [this](AsyncWebServerRequest *request) {
        this->handleWebSocketInfo(request);
    });
    

}

// FIXED: Updated handleDebugMessages - only return new messages
void QCWebServer::handleDebugMessages(AsyncWebServerRequest *request) {
    logRequest("DEBUG_MSG", request);
    
    // Get 'since' parameter to only return new messages
    unsigned long since = 0;
    if (request->hasParam("since")) {
        since = request->getParam("since")->value().toInt();
    }
    
    int limit = 50; // Default limit
    if (request->hasParam("limit")) {
        limit = request->getParam("limit")->value().toInt();
    }
    
    JsonDocument doc;
    JsonArray messages = doc.to<JsonArray>();
    
    int count = 0;
    // Return only messages newer than 'since' timestamp
    for (auto &msg : debugMessages) {
        if (msg.timestamp > since && count < limit) {
            JsonDocument msgDoc;
            msgDoc["message"] = msg.message;
            msgDoc["level"] = msg.level;
            msgDoc["timestamp"] = msg.timestamp;
            msgDoc["id"] = msg.id;
            msgDoc["source"] = "ESP32";
            
            messages.add(msgDoc);
            count++;
        }
    }
    
    String out;
    serializeJson(doc, out);
    
    // Serial.printf("[DEBUG] Returning %d new messages since %lu\n", count, since);
    request->send(200, "application/json", out);
}

void QCWebServer::handleDebugClearSent(AsyncWebServerRequest *request, const String &body) {
    logRequest("DEBUG_CLEAR", request);

    JsonDocument doc;
    if (deserializeJson(doc, body) == DeserializationError::Ok) {
        unsigned long lastId = doc["last_id"];

        int cleared = 0;
        for (auto &msg : debugMessages) {
            if (msg.timestamp <= lastId) {
                msg.sent = true;
                cleared++;
            }
        }

        unsigned long cutoff = millis() - 300000; // 5 minutes
        debugMessages.erase(
            std::remove_if(debugMessages.begin(), debugMessages.end(),
                [cutoff](const DebugMessage &msg) {
                    return msg.sent && msg.timestamp < cutoff;
                }),
            debugMessages.end()
        );

        // Serial.printf("[DEBUG] Cleared %d messages, keeping recent ones\n", cleared);
    }

    JsonDocument response;
    response["success"] = true;
    response["remaining"] = debugMessages.size();

    String out;
    serializeJson(response, out);
    request->send(200, "application/json", out);
}


void QCWebServer::handleWebSocketInfo(AsyncWebServerRequest *request) {
    logRequest("WS_INFO", request);
    
    JsonDocument doc;
    doc["wsEndpoint"] = "/ws";
    doc["connectedClients"] = ws->count();
    doc["maxClients"] = "10"; // Example static value
    doc["protocol"] = "WebSocket";
    doc["supportedMessages"] = JsonArray();
    doc["supportedMessages"].add("at_command");
    doc["supportedMessages"].add("ping");
    doc["supportedMessages"].add("system_status");
    
    String out;
    serializeJson(doc, out);
    request->send(200, "application/json", out);
}

void QCWebServer::setupWebSocket() {
    ws->onEvent([this](AsyncWebSocket *s, AsyncWebSocketClient *c, AwsEventType type, void *arg, uint8_t *data, size_t len) {
        this->onWsEvent(s, c, type, arg, data, len);
    });
}

void QCWebServer::handleGetHardwareConfig(AsyncWebServerRequest *request) {
    logRequest("CFG_DUMP", request);
    JsonDocument doc;

    // --- FIX: Refresh configuration from Devsbot before displaying ---
    if (devsbotRef) {
        devsbotRef->getDIOConfiguration(dioPins, dioModes, numDioPins);
        devsbotRef->getRS485Configuration(rs485SlaveIds, numRs485Slaves, 
                                         rs485Registers, numRs485Registers,
                                         rs485BaudRate, rs485DataBits, 
                                         rs485Parity, rs485StopBits);
    }
    // -----------------------------------------------------------------

    if (dioPins && dioModes && numDioPins > 0) {
        JsonArray dioArray = doc["dioConfig"].to<JsonArray>();
        for (int i = 0; i < numDioPins; i++) {
            JsonObject o = dioArray.add<JsonObject>();
            o["pin"] = dioPins[i];
            o["mode"] = dioModes[i];
            o["currentState"] = digitalRead(dioPins[i]);
        }
    }

    JsonObject rs = doc["rs485Config"].to<JsonObject>();
    rs["baudRate"] = rs485BaudRate;
    rs["dataBits"] = rs485DataBits;
    rs["parity"] = rs485Parity;
    rs["stopBits"] = rs485StopBits;

    if (rs485SlaveIds && numRs485Slaves > 0) {
        JsonArray sArr = rs["slaveIds"].to<JsonArray>();
        for (int i = 0; i < numRs485Slaves; i++) {
            sArr.add(rs485SlaveIds[i]);
        }
    }

    if (rs485Registers && numRs485Registers > 0) {
        JsonArray rArr = rs["registers"].to<JsonArray>();
        for (int i = 0; i < numRs485Registers; i++) {
            rArr.add("0x" + String(rs485Registers[i], HEX));
        }
    }

    String out;
    serializeJson(doc, out);
    request->send(200, "application/json", out);
    if (debugVerbose) { Serial.println(out); }
}

void QCWebServer::handleWiFiStatus(AsyncWebServerRequest *request) {
    logRequest("WIFI_STATUS", request);
    JsonDocument doc;
    if (WiFi.isConnected()) {
        doc["status"] = "Online";
        doc["network"] = WiFi.SSID();
        doc["rssi"] = WiFi.RSSI();
        doc["ip"] = WiFi.localIP().toString();
        doc["gateway"] = WiFi.gatewayIP().toString();
        doc["dns"] = WiFi.dnsIP().toString();
        doc["channel"] = WiFi.channel();
    } else {
        doc["status"] = "Offline";
    }
    String out; 
    serializeJson(doc, out); 
    request->send(200, "application/json", out);
}

void QCWebServer::handleSystemInfo(AsyncWebServerRequest *request) {
    logRequest("SYS_INFO", request);
    JsonDocument doc;
    
    // Basic Hardware Info
    doc["macAddress"] = WiFi.macAddress();
    doc["chipModel"] = ESP.getChipModel();
    doc["cpuFreqMHz"] = ESP.getCpuFreqMHz();
    doc["flashSize"] = ESP.getFlashChipSize();
    doc["sketchSize"] = ESP.getSketchSize();
    doc["freeSketch"] = ESP.getFreeSketchSpace();
    
    // RAM Info
    doc["freeHeap"] = ESP.getFreeHeap();
    doc["totalHeap"] = ESP.getHeapSize();
    doc["minFreeHeap"] = ESP.getMinFreeHeap();
    
    // System Status
    doc["uptime"] = millis() / 1000;
    doc["temperature"] = temperatureRead();
    doc["resetReason"] = getResetReason();

    // --- 1. GET DYNAMIC FIRMWARE VERSION ---
    if (devsbotRef) {
        // Read directly from the main Devsbot instance
        doc["firmwareVersion"] = String(devsbotRef->deviceFirmwareVersion, 1); 
    } else {
        doc["firmwareVersion"] = "Unknown";
    }

    // --- 2. GET WORKING BOOT COUNT ---
    prefs.begin("system", true); // Read-only mode is enough here
    doc["bootCount"] = prefs.getUInt("bootCount", 0);
    prefs.end();

    // --- 3. GET DETAILED NVS (EEPROM) STATS ---
    nvs_stats_t nvs_stats;
    if (nvs_get_stats(NULL, &nvs_stats) == ESP_OK) {
        // NVS entries are typically 32 bytes each. 
        // This gives a good approximation of actual bytes used/free.
        uint32_t totalBytes = nvs_stats.total_entries * 32;
        uint32_t usedBytes = nvs_stats.used_entries * 32;
        uint32_t freeBytes = nvs_stats.free_entries * 32;

        doc["nvsTotal"] = totalBytes;
        doc["nvsUsed"] = usedBytes;
        doc["nvsFree"] = freeBytes;
        doc["nvsEntriesUsed"] = nvs_stats.used_entries; // "Writes" currently active
        
        // Update the legacy field for your frontend
        doc["eepromSize"] = String(usedBytes) + " / " + String(totalBytes) + " bytes used";
    } else {
        doc["eepromSize"] = "NVS Error";
    }

    #ifdef ESP32
    doc["spiffsTotal"] = SPIFFS.totalBytes();
    doc["spiffsUsed"] = SPIFFS.usedBytes();
    #endif
    
    String out; 
    serializeJson(doc, out); 
    request->send(200, "application/json", out);
}
void QCWebServer::handleRestart(AsyncWebServerRequest *request) {
    logRequest("RESTART", request);
    request->send(200, "text/plain", "Restarting…");
    delay(100);
    ESP.restart();
}

void QCWebServer::handleTestRS485(AsyncWebServerRequest *request) {
    logRequest("TEST_RS485", request);
    Serial.println("[TEST_RS485] Start");
    JsonDocument doc;
    
    if (!modbus || !rs485SlaveIds || numRs485Slaves == 0) {
        doc["success"] = false;
        doc["error"] = "RS485 not configured properly";
        String out; 
        serializeJson(doc, out); 
        request->send(200, "application/json", out);
        return;
    }
    
    // Get dynamic RS485 configuration from Devsbot if available
    if (devsbotRef) {
        // Use Devsbot configuration
        // This would need to be implemented based on your Devsbot interface
    }
    
    Serial2.end();
    Serial2.begin(rs485BaudRate, SERIAL_8N1, rs485RxPin, rs485TxPin);
    
    String foundSlaves = "";
    int slaveCount = 0;
    bool success = false;
    String sampleData = "";
    String detailedLog = "";
    
    for (int i = 0; i < numRs485Slaves; i++) {
        uint8_t slaveId = rs485SlaveIds[i];
        
        modbus->begin(slaveId, Serial2);
        
        Serial.printf("Testing Slave ID: %d\n", slaveId);
        detailedLog += "Testing Slave " + String(slaveId) + "... ";
        
        uint8_t result = modbus->readHoldingRegisters(rs485Registers[0], 3);
        
        if (result == modbus->ku8MBSuccess) {
            if (slaveCount > 0) foundSlaves += ", ";
            foundSlaves += String(slaveId);
            slaveCount++;
            success = true;
            
            detailedLog += "SUCCESS\n";
            Serial.printf("Slave %d responded successfully\n", slaveId);
            
            String slaveData = "";
            for (int reg = 0; reg < 3 && reg < numRs485Registers; reg++) {
                uint16_t value = modbus->getResponseBuffer(reg);
                float realValue = value / 100.0;
                
                if (reg > 0) slaveData += ", ";
                slaveData += String(rs485Registers[reg], HEX) + ":" + String(realValue, 2);
            }
            
            if (sampleData.length() > 0) sampleData += " | ";
            sampleData += "S" + String(slaveId) + "(" + slaveData + ")";
            
        } else {
            detailedLog += "NO RESPONSE\n";
            Serial.printf("Slave %d no response (Error: 0x%02X)\n", slaveId, result);
        }
        
        delay(100);
    }
    
    if (success) {
        doc["success"] = true;
        doc["slaves"] = foundSlaves;
        doc["slaveCount"] = slaveCount;
        doc["baud"] = String(rs485BaudRate);
        doc["dataBits"] = rs485DataBits;
        doc["parity"] = (rs485Parity == 0) ? "None" : (rs485Parity == 1) ? "Odd" : "Even";
        doc["stopBits"] = rs485StopBits;
        doc["sampleData"] = sampleData.length() > 0 ? sampleData : "No data read";
        doc["registers"] = String(numRs485Registers) + " registers tested";
        doc["testDetails"] = detailedLog;
    } else {
        doc["success"] = false;
        doc["error"] = "No Modbus slaves responding";
        doc["testedSlaves"] = String(numRs485Slaves) + " slave IDs tested";
        doc["testDetails"] = detailedLog;
    }
    
    Serial2.end();
    Serial2.begin(rs485BaudRate, SERIAL_8N1, rs485RxPin, rs485TxPin);
    
    String out; 
    serializeJson(doc, out); 
    request->send(200, "application/json", out);
}

void QCWebServer::handleTestDIO(AsyncWebServerRequest *request) {
    logRequest("TEST_DIO", request);
    Serial.println("[TEST_DIO] Start");
    JsonDocument doc;
    
    // --- FIX: Refresh configuration from Devsbot before testing ---
    if (devsbotRef) {
        devsbotRef->getDIOConfiguration(dioPins, dioModes, numDioPins);
    }
    // --------------------------------------------------------------

    if (!dioPins || !dioModes || numDioPins == 0) {
        doc["success"] = false;
        doc["error"] = "DIO not configured properly. (No pins found in widget config)";
        String out; 
        serializeJson(doc, out); 
        request->send(200, "application/json", out);
        return;
    }
    
    bool allPinsOK = true;
    String pinDetails = "";
    
    for (int i = 0; i < numDioPins; i++) {
        int pin = dioPins[i];
        String mode = dioModes[i];
        String pinInfo = "";
        
        // --- SAFETY CHECK ---
        // Ensure we don't try to test pins that might crash the ESP32 if toggled improperly
        if (pin < 0 || pin > 39) continue; // Basic valid range for ESP32
        
        if (mode == "Output") {
            pinMode(pin, OUTPUT);
            
            digitalWrite(pin, HIGH);
            delay(10);
            bool highState = digitalRead(pin);
            
            digitalWrite(pin, LOW);
            delay(10);
            bool lowState = digitalRead(pin);
            
            if (highState == HIGH && lowState == LOW) {
                pinInfo = "GPIO" + String(pin) + ": OK";
                doc["gpio" + String(pin)] = "Output - Pass";
            } else {
                pinInfo = "GPIO" + String(pin) + ": FAIL";
                doc["gpio" + String(pin)] = "Output - Fail (Stuck)";
                allPinsOK = false;
            }
            
        } else {
            // For inputs, just read the current state
            if (mode == "Input_Pullup") pinMode(pin, INPUT_PULLUP);
            else pinMode(pin, INPUT);
            
            int state = digitalRead(pin);
            pinInfo = "GPIO" + String(pin) + ": Input=" + String(state);
            doc["gpio" + String(pin)] = "Input - " + String(state == HIGH ? "HIGH" : "LOW");
        }
        
        if (i > 0) pinDetails += ", ";
        pinDetails += pinInfo;
    }
    
    doc["success"] = allPinsOK;
    doc["totalPins"] = numDioPins;
    doc["testedPins"] = pinDetails;
    
    if (!allPinsOK) {
        doc["error"] = "One or more output pins failed toggle test";
    }
    
    String out; 
    serializeJson(doc, out); 
    request->send(200, "application/json", out);
}

void QCWebServer::handleTestSDCard(AsyncWebServerRequest *request) {
    logRequest("TEST_SD", request);
    Serial.println("[TEST_SD] Start");
    JsonDocument doc;
    
    SPI.begin(sdSckPin, sdMisoPin, sdMosiPin, sdCsPin);
    
    if (SD.begin(sdCsPin)) {
        uint8_t cardType = SD.cardType();
        uint64_t cardSize = SD.cardSize() / (1024 * 1024);
        uint64_t usedBytes = SD.usedBytes() / (1024 * 1024);
        uint64_t freeBytes = cardSize - usedBytes;
        
        doc["success"] = true;
        doc["cardType"] = (cardType == CARD_SD) ? "SD" : 
                         (cardType == CARD_SDHC) ? "SDHC" : 
                         (cardType == CARD_MMC) ? "MMC" : "Unknown";
        doc["totalSize"] = String(cardSize) + "MB";
        doc["usedSize"] = String(usedBytes) + "MB (" + String((usedBytes * 100) / cardSize) + "%)";
        doc["freeSize"] = String(freeBytes) + "MB";
        
        File testFile = SD.open("/test.txt", FILE_WRITE);
        if (testFile) {
            testFile.println("NTS Gateway Test");
            testFile.close();
            
            testFile = SD.open("/test.txt", FILE_READ);
            if (testFile) {
                String content = testFile.readString();
                testFile.close();
                SD.remove("/test.txt");
                doc["writeTest"] = content.indexOf("NTS Gateway Test") >= 0 ? "Pass" : "Fail";
            }
        }
    } else {
        doc["success"] = false;
        doc["error"] = "SD Card not detected";
    }
    
    String out; 
    serializeJson(doc, out); 
    request->send(200, "application/json", out);
}

// void QCWebServer::handleTestRTC(AsyncWebServerRequest *request) {
//     logRequest("TEST_RTC", request);
//     Serial.println("[TEST_RTC] Start");
//     JsonDocument doc;
    
//     if (!rtc) {
//         doc["success"] = false;
//         doc["error"] = "RTC not configured";
//         String out; 
//         serializeJson(doc, out); 
//         request->send(200, "application/json", out);
//         return;
//     }
    
//     //Wire.begin(i2cSdaPin, i2cSclPin);
    
//     if (rtc->begin()) {
//         DateTime now = rtc->now();
//         doc["success"] = true;
//         doc["currentTime"] = String(now.year()) + "-" +
//                              String(now.month()) + "-" +
//                              String(now.day()) + " " +
//                              String(now.hour()) + ":" +
//                              String(now.minute()) + ":" +
//                              String(now.second());
//     } else {
//         doc["success"] = false;
//         doc["error"] = "RTC not found";
//     }
    
//     String out; 
//     serializeJson(doc, out); 
//     request->send(200, "application/json", out);
// }


// --- MODIFIED: This function now uses the public isReady() method ---
void QCWebServer::handleTestRTC(AsyncWebServerRequest *request) {
    logRequest("TEST_RTC", request);
    Serial.println("[TEST_RTC] Start");
    JsonDocument doc;
    
    // The check is now against the manager's public getter function.
    if (rtcManager.isReady()) {
        // We get the time directly from the manager.
        DateTime now = rtcManager.now();
        doc["success"] = true;
        doc["currentTime"] = String(now.year()) + "-" +
                             String(now.month()) + "-" +
                             String(now.day()) + " " +
                             String(now.hour()) + ":" +
                             String(now.minute()) + ":" +
                             String(now.second());
    } else {
        doc["success"] = false;
        doc["error"] = "RTC not found or failed to initialize.";
    }
    
    String out; 
    serializeJson(doc, out); 
    request->send(200, "application/json", out);
}


void QCWebServer::handleTestEthernet(AsyncWebServerRequest *request) {
    logRequest("TEST_ETH", request);
    Serial.println("[TEST_ETH] Start");
    JsonDocument doc;
    
    if (ETH.begin()) {
        doc["success"] = true;
        doc["linkStatus"] = ETH.linkUp() ? "Up" : "Down";
        doc["speed"] = String(ETH.linkSpeed()) + " Mbps";
        doc["fullDuplex"] = ETH.fullDuplex() ? "Yes" : "No";
        doc["ipAddress"] = ETH.localIP().toString();
        doc["macAddress"] = ETH.macAddress();
    } else {
        doc["success"] = false;
        doc["error"] = "Ethernet init failed";
    }
    
    String out; 
    serializeJson(doc, out); 
    request->send(200, "application/json", out);
}

void QCWebServer::handleTestGPRS(AsyncWebServerRequest *request) {
    logRequest("TEST_GPRS", request);
    Serial.println("[TEST_GPRS] Start");
    JsonDocument doc;
    doc["success"] = false;
    doc["error"] = "GPRS module not connected";
    String out; 
    serializeJson(doc, out); 
    request->send(200, "application/json", out);
}

void QCWebServer::handleTestWiFi(AsyncWebServerRequest *request) {
    logRequest("TEST_WIFI", request);
    Serial.println("[TEST_WIFI] Start");
    JsonDocument doc;
    
    if (WiFi.isConnected()) {
        doc["success"] = true;
        doc["ssid"] = WiFi.SSID();
        doc["rssi"] = WiFi.RSSI();
        doc["signalQuality"] = (WiFi.RSSI() > -50) ? "Excellent" :
                              (WiFi.RSSI() > -60) ? "Good" :
                              (WiFi.RSSI() > -70) ? "Fair" : "Poor";
        doc["ipAddress"] = WiFi.localIP().toString();
        doc["gateway"] = WiFi.gatewayIP().toString();
        doc["dns"] = WiFi.dnsIP().toString();
        doc["channel"] = WiFi.channel();
        doc["macAddress"] = WiFi.macAddress();
        
        WiFiClient client;
        if (client.connect("8.8.8.8", 53)) {
            doc["internetTest"] = "Pass";
            client.stop();
        } else {
            doc["internetTest"] = "Fail";
        }
    } else {
        doc["success"] = false;
        doc["error"] = "Wi-Fi not connected";
    }
    
    String out; 
    serializeJson(doc, out); 
    request->send(200, "application/json", out);
}

void QCWebServer::handleTestMemory(AsyncWebServerRequest *request) {
    logRequest("TEST_MEM", request);
    Serial.println("[TEST_MEM] Start");
    JsonDocument doc;
    doc["success"] = true;
    doc["flashSize"] = String(ESP.getFlashChipSize() / 1024 / 1024) + "MB";
    doc["heapFree"] = String(ESP.getFreeHeap() / 1024) + "KB";
    doc["spiffsTotal"] = String(SPIFFS.totalBytes() / 1024) + "KB";
    doc["spiffsUsed"] = String(SPIFFS.usedBytes() / 1024) + "KB";
    String out; 
    serializeJson(doc, out); 
    request->send(200, "application/json", out);
}

void QCWebServer::handleConfigBackup(AsyncWebServerRequest *request) {
    logRequest("CFG_BACKUP", request);
    JsonDocument doc;
    
    // Read connectivity config
    if (SPIFFS.exists("/provision.json")) {
        File f = SPIFFS.open("/provision.json", "r");
        String connCfg = f.readString();
        f.close();
        JsonDocument connDoc;
        if (!deserializeJson(connDoc, connCfg)) {
            doc["connectivity"] = connDoc;
        }
    }
    
    // Read server config  
    if (SPIFFS.exists("/serverCfg.json")) {
        File f = SPIFFS.open("/serverCfg.json", "r");
        String serverCfg = f.readString();
        f.close();
        JsonDocument serverDoc;
        if (!deserializeJson(serverDoc, serverCfg)) {
            doc["server"] = serverDoc;
        }
    }
    
    doc["timestamp"] = millis();
    String out; 
    serializeJson(doc, out); 
    request->send(200, "application/json", out);
}

void QCWebServer::handleConfigRestore(AsyncWebServerRequest *request, const String& body) {
    logRequest("CFG_RESTORE", request, body);
    JsonDocument doc;
    
    if (deserializeJson(doc, body)) {
        request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        return;
    }
    
    // Restore connectivity config to /provision.json
    if (doc["connectivity"]) {
        File f = SPIFFS.open("/provision.json", FILE_WRITE);
        if (f) {
            serializeJson(doc["connectivity"], f);
            f.close();
            Serial.println("[CFG_RESTORE] Saved /provision.json");
        }
    }
    
    // Restore server config to /serverCfg.json
    if (doc["server"]) {
        File f = SPIFFS.open("/serverCfg.json", FILE_WRITE);
        if (f) {
            serializeJson(doc["server"], f);
            f.close();
            Serial.println("[CFG_RESTORE] Saved /serverCfg.json");
        }
    }
    
    request->send(200, "application/json", "{\"success\":true}");
}

void QCWebServer::handleServerConfig(AsyncWebServerRequest *request, const String& body) {
    logRequest("SRV_CFG", request, body);
    JsonDocument doc;
    
    if (deserializeJson(doc, body)) {
        request->send(400, "application/json", "{\"error\":\"JSON invalid\"}");
        return;
    }
    
    File f = SPIFFS.open("/serverCfg.json", FILE_WRITE);
    if (!f) {
        request->send(500, "application/json", "{\"error\":\"SPIFFS open failed\"}");
        return;
    }
    
    serializeJson(doc, f);
    f.close();
    Serial.println("[SRV_CFG] Saved to /serverCfg.json");
    request->send(200, "application/json", "{\"success\":true}");
}

void QCWebServer::handleProvision(AsyncWebServerRequest *request, const String& body) {
    logRequest("PROVISION", request, body);
    JsonDocument doc;

    if (deserializeJson(doc, body)) {
        request->send(400, "application/json", "{\"error\":\"JSON invalid\"}");
        return;
    }

    File f = SPIFFS.open("/network_data.json", FILE_WRITE);
    if (!f) {
        request->send(500, "application/json", "{\"error\":\"SPIFFS open failed\"}");
        return;
    }

    serializeJson(doc, f);
    f.close();
    Serial.println("[PROVISION] Saved to /network_data.json");
    request->send(200, "application/json", "{\"success\":true}");
}

void QCWebServer::handleClearLogs(AsyncWebServerRequest *request) {
    logRequest("LOG_CLEAR", request);
    File root = SPIFFS.open("/");
    File f = root.openNextFile();
    int deleted = 0;
    
    while (f) {
        String n = f.name();
        if (n.endsWith(".log")) { 
            SPIFFS.remove("/" + n); 
            deleted++; 
        }
        f.close(); 
        f = root.openNextFile();
    }
    
    JsonDocument doc; 
    doc["success"] = true; 
    doc["deletedFiles"] = deleted;
    String out; 
    serializeJson(doc, out); 
    request->send(200, "application/json", out);
}

// void QCWebServer::handleFormatSPIFFS(AsyncWebServerRequest *request) {
//     logRequest("FORMAT_SPIFFS", request);
//     // Keep commented to avoid accident
//     request->send(200, "application/json", "{\"success\":true}");
// }

/**
 * @brief Formats the entire SPIFFS filesystem. USE WITH CAUTION.
 */
void QCWebServer::handleFormatSPIFFS(AsyncWebServerRequest *request) {
    logRequest("FORMAT_SPIFFS", request);
    // WARNING: This erases EVERYTHING on the SPIFFS filesystem.
    Serial.println("[SPIFFS] Formatting filesystem via handleFormatSPIFFS...");
    if (SPIFFS.format()) { // <-- Uncommented
         Serial.println("[SPIFFS] Format successful.");
         request->send(200, "application/json", "{\"success\":true, \"message\":\"SPIFFS formatted successfully.\"}");
    } else {
         Serial.println("[SPIFFS] [ERROR] Format failed!");
         request->send(500, "application/json", "{\"success\":false, \"message\":\"SPIFFS format failed.\"}");
    }
}


// void QCWebServer::handleEraseProvision(AsyncWebServerRequest *request) {
//     logRequest("ERASE_PROV", request);
//     if (SPIFFS.exists("/provision.json")) {
//         SPIFFS.remove("/provision.json");
//     }
//     request->send(200, "text/plain", "Provision data erased");
// }


/**
 * @brief Erases specific provisioning and configuration files from SPIFFS.
 */
void QCWebServer::handleEraseProvision(AsyncWebServerRequest *request) {
    logRequest("ERASE_PROV", request);
    Serial.println("[SPIFFS] Erasing specific provisioning/config files...");

    // List of specific config files to delete (ADJUST THIS LIST AS NEEDED)
    const char* filesToDelete[] = {
        "/provision.json",        // Original file targeted (if used)
        "/Devsbot_Document.json",
        "/wifi_config.json",      // Or "/network_data.json"
        "/serverCfg.json",
        "/Devsbot_Widget.json",
        "/meter_Address.json"
        // Add any other specific config files here
            // Add these missing ones
        "/network_data.json",     // The current network config file
        "/debug_config.json",     // The debug settings file
        "/pulse_counts.json",     // Legacy pulse file
        "/pulse_counts_new.json"  // Legacy pulse file
    };

    bool allRemoved = true;
    int filesFound = 0;
    int filesRemoved = 0;

    for (const char* filename : filesToDelete) {
        if (SPIFFS.exists(filename)) {
            filesFound++;
            if (SPIFFS.remove(filename)) {
                Serial.printf("  - Removed: %s\n", filename);
                filesRemoved++;
            } else {
                Serial.printf("  - [ERROR] Failed to remove: %s\n", filename);
                allRemoved = false;
            }
        } else {
             // Optional: Log files not found if needed for debugging
             // Serial.printf("  - Not found, skipping: %s\n", filename);
        }
    }
    Serial.printf("[SPIFFS] Erase Provision: %d/%d files removed.\n", filesRemoved, filesFound);

    if (allRemoved) {
         request->send(200, "text/plain", "All specified provisioning/config files erased.");
    } else {
         request->send(200, "text/plain", "Attempted to erase provisioning/config files, errors occurred (check logs).");
    }
}


// void QCWebServer::handleFactoryReset(AsyncWebServerRequest *request) {
//     logRequest("FACTORY_RESET", request);
//     SPIFFS.format();
//     request->send(200, "text/plain", "Factory reset completed. Restarting…");
//     delay(1000); 
//     ESP.restart();
// }


/**
 * @brief Performs a Factory Reset: Formats SPIFFS, Clears all Preferences, Clears SD Logs, Restarts.
 */
void QCWebServer::handleFactoryReset(AsyncWebServerRequest *request) {
    logRequest("FACTORY_RESET", request);
    Serial.println("\n========================================");
    Serial.println("         PERFORMING FACTORY RESET        ");
    Serial.println("========================================");

    // 1. Format SPIFFS
    Serial.println("[RESET] Formatting SPIFFS...");
    if (SPIFFS.format()) {
         Serial.println("  -> SPIFFS format successful.");
    } else {
         Serial.println("  -> [ERROR] SPIFFS format failed!");
         // Optionally send error and return early if SPIFFS format is critical
         // request->send(500, "text/plain", "Factory reset failed: Could not format SPIFFS.");
         // return;
    }

    // 2. Clear All Preferences (EEPROM/NVS)
    Serial.println("[RESET] Clearing All Preferences (NVS)...");
    Preferences tempPrefs; // Use a temporary object
    // ADD ALL NAMESPACES USED IN YOUR ENTIRE PROJECT HERE
    const char* namespaces[] = {
        "conn", "server", "system", "devsbot", "version", "ledState", "DI_JobState",
        // Add these missing ones
            "pulse_data",  // From DigitalInput.cpp
            "sdEnergy",    // From sdcard.cpp -> LogManager
            "sdDIStatus"   // From sdcard.cpp -> LogManager
        // Add others like "pulse_counts" if used by DigitalInput::savePulseCountsToSPIFFS
    };
    int clearedCount = 0;
    for (const char* ns : namespaces) {
        tempPrefs.begin(ns, false); // Open R/W
        if(tempPrefs.clear()) {
            // Serial.printf("    - Namespace '%s' cleared.\n", ns); // Verbose
            clearedCount++;
        } else {
            // Only log if clearing actually fails (it usually doesn't unless NVS is corrupt)
            Serial.printf("    - [ERROR] Failed to clear namespace '%s'.\n", ns);
        }
        tempPrefs.end();
    }
     Serial.printf("  -> Cleared %d preference namespaces.\n", clearedCount);


    // 3. Clear SD Card Logs
    // Serial.println("[RESET] Clearing SD Card Logs...");
    // if (card.cardMounted) {
    //    if (card.clearAllLogs()) { // clearAllLogs now prints details
    //        Serial.println("  -> SD Card log clearing initiated successfully.");
    //    } else {
    //         Serial.println("  -> [WARN] SD Card log clearing reported errors (check SD logs).");
    //    }
    // } else {
    //      Serial.println("  -> SD Card not mounted, skipping log clearing.");
    // }

    Serial.println("========================================");
    Serial.println("      FACTORY RESET COMPLETE          ");
    Serial.println("========================================");
    request->send(200, "text/plain", "Factory reset initiated. Config, Preferences, and Logs cleared. Restarting...");
    Serial.println("\n[RESET] Restarting device NOW...");
    delay(2000); // Allow time for response to send and logs to flush
    ESP.restart();
}




// STEP 3: REPLACE your existing handleATCommand with this:
void QCWebServer::handleATCommand(AsyncWebServerRequest *request) {
    String cmd = "";
    
    // For POST requests, get command from captured body
    if (request->method() == HTTP_POST) {
        cmd = postBodyBuffer;
        cmd.trim(); // Remove any whitespace/newlines
        postBodyBuffer = ""; // Clear buffer for next use
        
        Serial.printf("[AT_CMD_POST] Command from body: '%s'\n", cmd.c_str());
    }
    // For GET requests, get from URL parameter
    else if (request->hasParam("cmd")) {
        cmd = request->getParam("cmd")->value();
        Serial.printf("[AT_CMD_GET] Command from URL: '%s'\n", cmd.c_str());
    }
    
    if (cmd.isEmpty()) {
        Serial.println("[AT_CMD] No command found!");
        request->send(400, "text/plain", "Missing command. POST body is empty or no ?cmd= parameter");
        return;
    }

    if (debugVerbose) Serial.printf("[AT_CMD_HTTP] Processing: %s\n", cmd.c_str());
    
    // Also send to WebSocket clients for real-time feedback
    sendSerialToWeb("AT Command (HTTP): " + cmd, "info");

    String response;
    if (cmd == "AT") {
        response = "OK";
    } else if (cmd == "AT+GMR") {
        response = "ESP32 QC Web Server V1.0\nOK";
    } else if (cmd == "AT+CIFSR") {
        response = "+CIFSR:STAIP,\"" + WiFi.localIP().toString() + "\"\n"
                   "+CIFSR:STAMAC,\"" + WiFi.macAddress() + "\"\nOK";
    } else if (cmd.startsWith("AT+CWJAP=")) {
        response = "WIFI CONNECT\nWIFI GOT IP\nOK";
        sendSerialToWeb("WiFi connection attempt via AT command", "info");
    } else if (cmd.startsWith("AT+DEBUG=")) {
        String mode = cmd.substring(9); // Remove "AT+DEBUG="
        mode.toUpperCase();
        
        if (mode == "SERIAL" || mode == "WEB" || mode == "BOTH" || mode == "OFF") {
            DebugConfig::setDebugMode(mode);
            response = "+DEBUG:" + mode + "\nOK";
        } else {
            response = "ERROR: Invalid debug mode. Use SERIAL/WEB/BOTH/OFF";
        }
    } else if (cmd == "AT+DEBUG?") {
        response = "+DEBUG:" + DebugConfig::getDebugStatus() + "\nOK";
    } else if (cmd == "AT+RST") {
        response = "OK\n\nready";
        sendSerialToWeb("ESP32 restart requested via AT command", "warn");
        request->send(200, "text/plain", response);
        delay(100);
        ESP.restart();
        return;
    } else {
        response = "ERROR";
    }
    
    Serial.printf("[AT_CMD] Sending response: %s\n", response.c_str());
    
    // Send AT response to WebSocket clients
    sendSerialToWeb("AT Response: " + response, "info");
    request->send(200, "text/plain", response);
}
// // --- MODIFIED: This function now clears the "devsbot" namespace as well ---
// void QCWebServer::handleEraseEEPROM(AsyncWebServerRequest *request) {
//     logRequest("ERASE_EEPROM", request);
    
//     // Log the action to the debug console
//     Serial.println("Clearing all preferences from EEPROM...");

//     // Clear the existing namespaces
//     prefs.begin("conn", false); prefs.clear(); prefs.end();
//     Serial.println("  - 'conn' namespace cleared.");
    
//     prefs.begin("server", false); prefs.clear(); prefs.end();
//     Serial.println("  - 'server' namespace cleared.");
    
//     prefs.begin("system", false); prefs.clear(); prefs.end();
//     Serial.println("  - 'system' namespace cleared.");

//     // --- THIS IS THE NEW LOGIC ---
//     // Add the "devsbot" namespace to the erase list.
//     // This is the namespace where the authToken is stored.
//     prefs.begin("devsbot", false); prefs.clear(); prefs.end();
//     Serial.println("  - 'devsbot' (Auth Token) namespace cleared.");
//     // --- END OF NEW LOGIC ---

//     request->send(200, "text/plain", "All preferences, including Auth Token, have been cleared.");
// }

/**
 * @brief Clears all known Preferences namespaces (NVS/EEPROM).
 */
void QCWebServer::handleEraseEEPROM(AsyncWebServerRequest *request) {
    logRequest("ERASE_EEPROM", request);
    Serial.println("\n========================================");
    Serial.println(" Clearing All Preferences (NVS/EEPROM) ");
    Serial.println("========================================");
    Preferences tempPrefs; // Use a temporary object

    // ADD ALL NAMESPACES USED IN YOUR ENTIRE PROJECT HERE
    const char* namespaces[] = {
        "conn", "server", "system", "devsbot", "version", "ledState", "DI_JobState",
            "pulse_data",  // From DigitalInput.cpp
            "sdEnergy",    // From sdcard.cpp -> LogManager
            "sdDIStatus"   // From sdcard.cpp -> LogManager
        // Add others like "pulse_counts" if used by DigitalInput::savePulseCountsToSPIFFS
    };
    int clearedCount = 0;
    int failedCount = 0;

    for (const char* ns : namespaces) {
        tempPrefs.begin(ns, false); // Open R/W
        if (tempPrefs.clear()) {
             Serial.printf("  - Namespace '%s' cleared.\n", ns);
             clearedCount++;
        } else {
             Serial.printf("  - [ERROR] Failed to clear namespace '%s'.\n", ns);
             failedCount++;
        }
        tempPrefs.end();
    }
    Serial.println("----------------------------------------");
    Serial.printf("Finished clearing preferences. Success: %d, Failed: %d\n", clearedCount, failedCount);
    Serial.println("========================================");

    request->send(200, "text/plain", "All known preferences namespaces have been cleared.");
}

// void QCWebServer::handleEraseSPIFFS(AsyncWebServerRequest *request) {
//     logRequest("ERASE_SPIFFS", request);
//     if (SPIFFS.format()) {
//         request->send(200, "text/plain", "SPIFFS formatted");
//     } else {
//         request->send(500, "text/plain", "SPIFFS format failed");
//     }
// }

/**
 * @brief Formats the entire SPIFFS filesystem. USE WITH CAUTION.
 */
void QCWebServer::handleEraseSPIFFS(AsyncWebServerRequest *request) {
    logRequest("ERASE_SPIFFS", request);
    Serial.println("\n========================================");
    Serial.println("         Formatting SPIFFS...         ");
    Serial.println("========================================");
    if (SPIFFS.format()) {
        Serial.println("[SPIFFS] Format successful.");
        Serial.println("========================================");
        request->send(200, "text/plain", "SPIFFS formatted successfully.");
    } else {
        Serial.println("[SPIFFS] [ERROR] Format failed!");
        Serial.println("========================================");
        request->send(500, "text/plain", "SPIFFS format failed.");
    }
}

void QCWebServer::handleAcknowledge(AsyncWebServerRequest *request) {
    String message = request->arg("plain");
    QCWebServer::getInstance()->acknowledgeMessage(message);
    request->send(200, "text/plain", "Acknowledged");
}


// Add this to your handleATCommandWS function
void QCWebServer::handleATCommandWS(AsyncWebSocketClient *client, const String &command) {
    Serial.printf("[AT_CMD_WS] Received: %s\n", command.c_str());
    
    String response;
    if (command == "AT") {
        response = "OK";
    } else if (command == "AT+GMR") {
        response = "ESP32 QC Web Server V1.0\\nOK";
    } else if (command.startsWith("AT+DEBUG=")) {
        // Extract debug mode: AT+DEBUG=SERIAL, AT+DEBUG=WEB, AT+DEBUG=BOTH, AT+DEBUG=OFF
        String mode = command.substring(9); // Remove "AT+DEBUG="
        mode.toUpperCase();
        
        if (mode == "SERIAL" || mode == "WEB" || mode == "BOTH" || mode == "OFF") {
            DebugConfig::setDebugMode(mode);
            response = "+DEBUG:" + mode + "\\nOK";
        } else {
            response = "ERROR: Invalid debug mode. Use SERIAL/WEB/BOTH/OFF";
        }
    } else if (command == "AT+DEBUG?") {
        response = "+DEBUG:" + DebugConfig::getDebugStatus() + "\\nOK";
    } else if (command == "AT+CIFSR") {
        response = "+CIFSR:STAIP,\"" + WiFi.localIP().toString() + "\"\\n"
                   "+CIFSR:STAMAC,\"" + WiFi.macAddress() + "\"\\nOK";
    } else if (command == "AT+RST") {
        response = "OK\\n\\nready";
        // Send response first, then restart
        JsonDocument doc;
        doc["type"] = "at_response";
        doc["command"] = command;
        doc["response"] = response;
        String jsonStr;
        serializeJson(doc, jsonStr);
        client->text(jsonStr);
        
        delay(100);
        ESP.restart();
        return;
    } else {
        response = "ERROR";
    }
    
    // Send AT command response back via WebSocket
    JsonDocument doc;
    doc["type"] = "at_response";
    doc["command"] = command;
    doc["response"] = response;
    doc["timestamp"] = millis();
    
    String jsonStr;
    serializeJson(doc, jsonStr);
    client->text(jsonStr);
}

// FIXED: Updated WebSocket event handler - no more duplicate sending
void QCWebServer::onWsEvent(AsyncWebSocket *s, AsyncWebSocketClient *c,
                           AwsEventType type, void *arg, uint8_t *data, size_t len) {
    if (type == WS_EVT_CONNECT) {
        Serial.printf("[WS] Client #%u connected from %s\n", c->id(), c->remoteIP().toString().c_str());
        
        // Send welcome message
        JsonDocument welcome;
        welcome["type"] = "system";
        welcome["level"] = "info";
        welcome["message"] = "WebSocket connected to ESP32 Debug Console";
        welcome["timestamp"] = millis();
        
        String welcomeStr;
        serializeJson(welcome, welcomeStr);
        c->text(welcomeStr);
        
        // Send only recent unsent messages to new client (last 10 messages)
        int sent = 0;
        for (auto it = debugMessages.rbegin(); it != debugMessages.rend() && sent < 10; ++it) {
            JsonDocument msgDoc;
            msgDoc["type"] = "esp32_debug";
            msgDoc["level"] = it->level;
            msgDoc["message"] = it->message;
            msgDoc["timestamp"] = it->timestamp;
            msgDoc["id"] = it->id;
            
            String msgStr;
            serializeJson(msgDoc, msgStr);
            c->text(msgStr);
            sent++;
        }
        
        Serial.printf("[WS] Sent %d recent messages to new client\n", sent);
        
    } else if (type == WS_EVT_DISCONNECT) {
        Serial.printf("[WS] Client #%u disconnected\n", c->id());
        
    } else if (type == WS_EVT_DATA) {
        AwsFrameInfo *info = (AwsFrameInfo*)arg;
        if (info->final && info->index == 0 && info->len == len) {
            if (info->opcode == WS_TEXT) {
                String message((char*)data, len);
                Serial.printf("[WS] Received: %s\n", message.c_str());
                
                JsonDocument doc;
                if (deserializeJson(doc, message) == DeserializationError::Ok) {
                    String msgType = doc["type"].as<String>();
                    
                    if (msgType == "at_command") {
                        String command = doc["command"].as<String>();
                        handleATCommandWS(c, command);
                    } else if (msgType == "ping") {
                        JsonDocument pong;
                        pong["type"] = "pong";
                        pong["timestamp"] = millis();
                        String pongStr;
                        serializeJson(pong, pongStr);
                        c->text(pongStr);
                    }
                }
            }
        }
    }
}

// Enhanced system status broadcast
void QCWebServer::broadcastSystemStatus() {
    if (!ws || ws->count() == 0) return;
    
    JsonDocument doc;
    doc["type"] = "system_status";
    doc["uptime"] = millis() / 1000;
    doc["freeHeap"] = ESP.getFreeHeap();
    doc["temperature"] = temperatureRead();
    doc["wifi_connected"] = WiFi.isConnected();
    doc["wifi_rssi"] = WiFi.isConnected() ? WiFi.RSSI() : 0;
    doc["ap_clients"] = WiFi.softAPgetStationNum();
    doc["ws_clients"] = ws->count();
    doc["timestamp"] = millis();
    
    String statusStr;
    serializeJson(doc, statusStr);
    ws->textAll(statusStr);
}
// Flush debug buffer periodically
void QCWebServer::flushDebugBuffer() {
    if (millis() - lastDebugSend > DEBUG_SEND_INTERVAL && !debugBuffer.isEmpty()) {
        sendSerialToWeb(debugBuffer, "debug");
        debugBuffer = "";
        lastDebugSend = millis();
    }
}

// NEW: Cleanup old messages
void QCWebServer::cleanupOldMessages() {
    unsigned long cutoff = millis() - 600000; // 10 minutes
    int before = debugMessages.size();
    
    debugMessages.erase(
        std::remove_if(debugMessages.begin(), debugMessages.end(),
            [cutoff](const DebugMessage& msg) {
                return msg.timestamp < cutoff;
            }),
        debugMessages.end()
    );
    
    int after = debugMessages.size();
    if (before != after) {
        Serial.printf("[DEBUG] Cleaned up %d old messages (%d remaining)\n", before - after, after);
    }
}

// Updated loop method - cleaner
void QCWebServer::loop() {
    ws->cleanupClients();
    
    // Send periodic status updates (less frequent)
    static unsigned long lastStatus = 0;
    if (millis() - lastStatus > 60000) { // Every 60 seconds instead of 30
        lastStatus = millis();
        broadcastSystemStatus();
    }
    
    // Periodic cleanup of old messages
    static unsigned long lastCleanup = 0;
    if (millis() - lastCleanup > 300000) { // Every 5 minutes
        lastCleanup = millis();
        cleanupOldMessages();
    }
}



