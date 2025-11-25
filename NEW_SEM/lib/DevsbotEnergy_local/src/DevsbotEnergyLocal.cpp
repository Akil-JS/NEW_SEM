#include "esp32-hal-gpio.h"
#include "DevsbotEnergyLocal.h"
#include "QCWebServer.h"
#include "DebugConfig.h"
#include "DebugMacro.h"

//TaskHandle_t sdCardTaskHandle = NULL;
TaskHandle_t dataTaskHandle=NULL;
TaskHandle_t TaskHandle_PostData_Start_S1=NULL;  // Task handle
SemaphoreHandle_t xHttpMutex=NULL;


//DynamicJsonDocument  doc(1024);
SocketIOclient socketIO;
Preferences preferences;
extern readMeterData energy;
meterParams mtrparam[12];
extern sdcard card;
AsyncWebServer myserver(80);

Devsbot::Devsbot() {
    // No additional initialization needed
}

// Devsbot::Devsbot() {
//     // Initialize dynamic configuration arrays
//     dynamicDioPins = nullptr;
//     dynamicDioModes = nullptr;
//     dynamicDioPinCount = 0;
    
//     dynamicSlaveIds = nullptr;
//     dynamicSlaveCount = 0;
//     dynamicRegisters = nullptr;
//     dynamicRegisterCount = 0;
    
//     // Default RS485 configuration
//     dynamicBaudRate = 9600;
//     dynamicDataBits = 8;
//     dynamicParity = 0;
//     dynamicStopBits = 1;
// }

Devsbot::~Devsbot() {
    // Cleanup dynamic arrays
    if (dynamicDioPins) delete[] dynamicDioPins;
    if (dynamicDioModes) delete[] dynamicDioModes;
    if (dynamicSlaveIds) delete[] dynamicSlaveIds;
    if (dynamicRegisters) delete[] dynamicRegisters;
}


// NEW METHOD: Parse widget configuration for QC testing
bool Devsbot::parseWidgetConfiguration() {
    if (widgetData.isEmpty()) return false;
    
    JsonDocument tempDoc;
    DeserializationError error = deserializeJson(tempDoc, widgetData);
    
    if (error) {
        DEBUG("Failed to parse widget data for QC");
        return false;
    }
    
    // Count total pins first
    int totalPins = 0;
    for (JsonVariant elem : tempDoc.as<JsonArray>()) {
        if (elem["pin"].is<JsonArray>()) {
            totalPins += elem["pin"].size();
        }
    }
    
    if (totalPins == 0) return false;
    
    // Allocate arrays
    dynamicDioPins = new int[totalPins];
    dynamicDioModes = new String[totalPins];
    dynamicDioPinCount = 0;
    
    // Parse pins and modes
    for (JsonVariant elem : tempDoc.as<JsonArray>()) {
        String datastreamName = elem["datastream_name"];
        String pinMode = elem["pinmode"];
        
        if (elem["pin"].is<JsonArray>()) {
            JsonArray pins = elem["pin"];
            
            for (int i = 0; i < pins.size(); i++) {
                dynamicDioPins[dynamicDioPinCount] = pins[i];
                
                if (datastreamName == "Digital") {
                    if (pinMode == "INPUT") {
                        dynamicDioModes[dynamicDioPinCount] = "Input";
                    } else if (pinMode == "INPUT_PULLUP") {
                        dynamicDioModes[dynamicDioPinCount] = "Input_Pullup";
                    } else if (pinMode == "OUTPUT") {
                        dynamicDioModes[dynamicDioPinCount] = "Output";
                    }
                } else if (datastreamName == "Analog") {
                    dynamicDioModes[dynamicDioPinCount] = "Input";
                }
                
                dynamicDioPinCount++;
            }
        }
    }
    
    return true;
}

// NEW METHOD: Parse meter configuration for QC testing
bool Devsbot::parseMeterConfiguration() {
    if (!SPIFFS.exists("/meter_Address.json")) return false;
    
    File file = SPIFFS.open("/meter_Address.json", "r");
    String addressData = file.readString();
    file.close();
    
    JsonDocument tempDoc;
    DeserializationError error = deserializeJson(tempDoc, addressData);
    
    if (error) {
        DEBUG("Failed to parse meter data for QC");
        return false;
    }
    
    int statusMtrAdd = tempDoc["status"].as<int>();
    if (statusMtrAdd != 200) return false;
    
    // Parse slave IDs
    JsonArray jsonslaveids = tempDoc["slave_id"];
    if (jsonslaveids.size() > 0) {
        dynamicSlaveCount = jsonslaveids.size();
        dynamicSlaveIds = new uint8_t[dynamicSlaveCount];
        
        for (int i = 0; i < dynamicSlaveCount; i++) {
            dynamicSlaveIds[i] = jsonslaveids[i];
        }
    }
    
    // Parse registers from first configuration
    JsonArray conf = tempDoc["conf"];
    if (conf.size() > 0) {
        JsonObject item = conf[0];
        
        // Get baud rate
        dynamicBaudRate = item["baudrate"].as<unsigned long>();
        
        // Get parity and stop bits
        const char *tempch = item["parity"];
        if (tempch != NULL) {
            uint32_t parityStopbit = strtoul(tempch, NULL, 16);
            // Parse parity and stop bits from hex value
            dynamicParity = (parityStopbit >> 8) & 0xFF;
            dynamicStopBits = parityStopbit & 0xFF;
        }
        
        // Get register addresses
        JsonArray mtrAdd = item["address"];
        if (mtrAdd.size() > 0) {
            dynamicRegisterCount = mtrAdd.size();
            dynamicRegisters = new uint16_t[dynamicRegisterCount];
            
            for (int i = 0; i < dynamicRegisterCount; i++) {
                dynamicRegisters[i] = mtrAdd[i];
            }
        }
    }
    
    return true;
}

// NEW METHOD: Initialize DIO pins
void Devsbot::initializeDIOPins() {
    for (int i = 0; i < dynamicDioPinCount; i++) {
        int pin = dynamicDioPins[i];
        String mode = dynamicDioModes[i];
        
        if (mode == "Output") {
            pinMode(pin, OUTPUT);
            digitalWrite(pin, LOW);
        } else if (mode == "Input") {
            pinMode(pin, INPUT);
        } else if (mode == "Input_Pullup") {
            pinMode(pin, INPUT_PULLUP);
        }
        
        Serial.printf("QC: GPIO%02d %-12s state=%d\n", pin, mode.c_str(), digitalRead(pin));
    }
}

// NEW METHOD: Initialize RS485 configuration
void Devsbot::initializeRS485Config() {
    Serial.printf("QC: RS485 Config - Baud: %lu, Slaves: %d, Registers: %d\n", 
                  dynamicBaudRate, dynamicSlaveCount, dynamicRegisterCount);
}

// NEW METHOD: Get DIO configuration for QC testing
void Devsbot::getDIOConfiguration(int*& pins, String*& modes, int& count) {
    pins = dynamicDioPins;
    modes = dynamicDioModes;
    count = dynamicDioPinCount;
}

// NEW METHOD: Get RS485 configuration for QC testing
void Devsbot::getRS485Configuration(uint8_t*& slaveIds, int& slaveCount,
                                              uint16_t*& registers, int& regCount,
                                              uint32_t& baudRate, uint8_t& dataBits,
                                              uint8_t& parity, uint8_t& stopBits) {
    slaveIds = dynamicSlaveIds;
    slaveCount = dynamicSlaveCount;
    registers = dynamicRegisters;
    regCount = dynamicRegisterCount;
    baudRate = dynamicBaudRate;
    dataBits = dynamicDataBits;
    parity = dynamicParity;
    stopBits = dynamicStopBits;
}

// NEW METHOD: Get current pin state
int Devsbot::getPinState(int pin) {
    return digitalRead(pin);
}

// NEW METHOD: Set pin mode
void Devsbot::setPinMode(int pin, int mode) {
    pinMode(pin, mode);
}

// NEW METHOD: Set pin state
void Devsbot::setPinState(int pin, int state) {
    digitalWrite(pin, state);
}


bool Devsbot::loadServerConfig() {
    DEBUG("[Devsbot] Loading server configuration...");
    
    if (!SPIFFS.exists("/serverCfg.json")) {
        DEBUG("[Devsbot] No server config file found");
        // Don't set defaults - return false to indicate no server config
        serverConfigLoaded = false;
        localModeOnly = true;  // Set local mode flag
        return false;
    }
    
    File configFile = SPIFFS.open("/serverCfg.json", "r");
    if (!configFile) {
        DEBUG("[Devsbot] Failed to open server config file");
        serverConfigLoaded = false;
        localModeOnly = true;  // Set local mode flag
        return false;
    }
    
    String configData = configFile.readString();
    configFile.close();
    
    if (configData.isEmpty()) {
        DEBUG("[Devsbot] Server config file is empty");
        serverConfigLoaded = false;
        localModeOnly = true;  // Set local mode flag
        return false;
    }
    
    bool parseResult = parseServerConfigFromJson(configData);
    if (!parseResult) {
        DEBUG("[Devsbot] Failed to parse server config");
        serverConfigLoaded = false;
        localModeOnly = true;  // Set local mode flag
        return false;
    }
    
    serverConfigLoaded = true;
    localModeOnly = false;  // Enable server operations
    DEBUG("[Devsbot] Server configuration loaded successfully");
    Serial.printf("Hostname: %s, Port: %d\n", dynamicHostname.c_str(), dynamicPort);
    
    return true;
}

bool Devsbot::parseServerConfigFromJson(const String& jsonData) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, jsonData);
    
    if (error) {
        Serial.printf("[Devsbot] JSON parsing error: %s\n", error.f_str());  // preferred in v6
        return false;
    }

    // Parse hostname and port
    if (doc.containsKey("hostname")) {
        dynamicHostname = doc["hostname"].as<String>();
    } else {
        DEBUG("[Devsbot] Missing hostname in config");
        return false;
    }
    
    if (doc.containsKey("port")) {
        // Handle port as either string or integer
        if (doc["port"].is<String>()) {
            dynamicPort = doc["port"].as<String>().toInt();
        } else {
            dynamicPort = doc["port"].as<uint16_t>();
        }
    } else {
        DEBUG("[Devsbot] Missing port in config");
        return false;
    }
    
    // Construct base URL for fallback
    String protocol = (dynamicPort == 443) ? "https://" : "http://";
    String baseUrl = protocol + dynamicHostname;
    if ((dynamicPort != 80 && dynamicPort != 443)) {
        baseUrl += ":" + String(dynamicPort);
    }
    
    // Initialize with default paths (fallback)
    dynamicAuthTokenApiURL = baseUrl + "/api/device/first/provision";
    dynamicDeviceProvisionURL = baseUrl + "/api/device/provision/data";
    dynamicDevsbotWidgetURL = baseUrl + "/api/cluster/widget/pin";
    dynamicDevsbotOTAUpdateURL = baseUrl + "/api/OTAfile/download";
    dynamicFirmwareVersionURL = baseUrl + "/api/device/update/version";
    dynamicDevsbotDeviceStatusURL = baseUrl + "/api/device/status";
    dynamicDevsbotDeviceLogURL = baseUrl + "/api/iot/device/post";
    dynamicEnergyMeterJsonURL = baseUrl + "/api/meterjsonData";
    dynamicLedStartStopApiURL = baseUrl + "/api/activity-trigger";
    dynamicMeterAddressURL = baseUrl + "/api/device/slaveid";
    dynamicReverseURL = baseUrl + "/api/config/reset";
    
    // Parse custom endpoints if provided
    if (doc.containsKey("endpoints")) {
        JsonObject endpoints = doc["endpoints"];
        
        // Helper function to determine if endpoint is full URL or path
        auto processEndpoint = [&](const String& key, String& targetUrl) {
            if (endpoints.containsKey(key)) {
                String endpoint = endpoints[key].as<String>();
                
                // Check if endpoint is already a full URL
                if (endpoint.startsWith("http://") || endpoint.startsWith("https://")) {
                    targetUrl = endpoint;
                } else {
                    // It's a path, concatenate with base URL
                    targetUrl = baseUrl + endpoint;
                }
            }
        };
        
        // Map JavaScript endpoint names to our internal URLs
        processEndpoint("Auth Token API", dynamicAuthTokenApiURL);
        processEndpoint("Device Provision", dynamicDeviceProvisionURL);
        processEndpoint("Widget API", dynamicDevsbotWidgetURL);
        processEndpoint("OTA Update", dynamicDevsbotOTAUpdateURL);
        processEndpoint("Firmware Version", dynamicFirmwareVersionURL);
        processEndpoint("Device Status", dynamicDevsbotDeviceStatusURL);
        processEndpoint("Device Log", dynamicDevsbotDeviceLogURL);
        processEndpoint("Energy Meter JSON", dynamicEnergyMeterJsonURL);
        processEndpoint("Activity Trigger", dynamicLedStartStopApiURL);
        processEndpoint("Meter Address", dynamicMeterAddressURL);
        processEndpoint("Reverse URL", dynamicReverseURL);
    }
    
    // Debug output
    DEBUG("[Devsbot] Parsed server configuration:");
    Serial.printf("  Auth Token API: %s\n", dynamicAuthTokenApiURL.c_str());
    Serial.printf("  Device Provision: %s\n", dynamicDeviceProvisionURL.c_str());
    Serial.printf("  Widget API: %s\n", dynamicDevsbotWidgetURL.c_str());
    Serial.printf("  Device Status: %s\n", dynamicDevsbotDeviceStatusURL.c_str());
    Serial.printf("  Device Log: %s\n", dynamicDevsbotDeviceLogURL.c_str());
    
    return true;
}
void Devsbot::setDefaultServerConfig() {
    // Set default configuration (your current hardcoded values)
    dynamicHostname = "sem-demo.devsbot.com";
    dynamicPort = 443;
    
    String baseUrl = "https://" + dynamicHostname;
    String apiPrefix = "/api";
    
    dynamicAuthTokenApiURL = baseUrl + apiPrefix + "/device/first/provision";
    dynamicDeviceProvisionURL = baseUrl + apiPrefix + "/device/provision/data";
    dynamicDevsbotWidgetURL = baseUrl + apiPrefix + "/cluster/widget/pin";
    dynamicDevsbotOTAUpdateURL = baseUrl + apiPrefix + "/OTAfile/download";
    dynamicFirmwareVersionURL = baseUrl + apiPrefix + "/device/update/version";
    dynamicDevsbotDeviceStatusURL = baseUrl + apiPrefix + "/device/status";
    dynamicDevsbotDeviceLogURL = baseUrl + apiPrefix + "/iot/device/post";
    dynamicEnergyMeterJsonURL = baseUrl + apiPrefix + "/meterjsonData";
    dynamicLedStartStopApiURL = baseUrl + apiPrefix + "/activity-trigger";
    dynamicMeterAddressURL = baseUrl + apiPrefix + "/device/slaveid";
    dynamicReverseURL = baseUrl + apiPrefix + "/config/reset";
    
    serverConfigLoaded = false;
    DEBUG("[Devsbot] Using default server configuration");
}




void Devsbot::WiFiEvent(WiFiEvent_t event) {
    // Print the raw event ID first
    //Serial.printf("[WiFi Event] ID: %d", event);

    switch (event) {
        case ARDUINO_EVENT_WIFI_STA_CONNECTED: // ID: 4
            //Serial.println(" - Meaning: STA Connected to AP (Link Layer)");
            DEBUG("[<<<<<<<<<<<<<<<<<<<<<<<---WiFi CONNECTED--->>>>>>>>>>>>>>>>>>>>>]"); // Your existing debug message
            break;

        case ARDUINO_EVENT_WIFI_STA_DISCONNECTED: // ID: 5
            //Serial.println(" - Meaning: STA Disconnected from AP");
            DEBUG("[XXXXXXXXXXXXXXX---WiFi DISCONNECTED---XXXXXXXXXXXXXXX]"); // Your existing debug message
            dBot.wifiStatus = 0;
            dBot.notConnetedtoNetwork = 1;
            _lastStaDisconnectTime = millis();
            _lastStaRetryTime = millis();
            nonBlockingConnectSTA();
            break;

        case ARDUINO_EVENT_WIFI_STA_LOST_IP: // ID: 6
            //Serial.println(" - Meaning: STA Lost IP Address");
            DEBUG("Lost IP address"); // Your existing debug message
            dBot.wifiStatus = 0;
            dBot.notConnetedtoNetwork = 1;
            _lastStaDisconnectTime = millis();
            _lastStaRetryTime = millis();
            break;

        case ARDUINO_EVENT_WIFI_STA_GOT_IP: // ID: 7
            Serial.println(" - Meaning: STA Got IP Address (Ready)");
            DEBUG("Got IP address"); // Your existing debug message
            DEBUG("Device connected to SSID: " + String(WiFi.SSID())); // Combined debug message
            DEBUG("IP address: " + WiFi.localIP().toString());       // Combined debug message
            dBot.wifiStatus = 1;
            dBot.notConnetedtoNetwork = 0;

            // --- NEW: Trigger RTC Sync now that we have internet ---
            rtcManager.performNTPSync();
            // -------------------------------------------------------

            if (!localModeOnly) {
                // *** CRITICAL FIX: Only run AuthToken ONCE ***
                if (!_isInitialized) {
                    DEBUG(">> FIRST CONNECTION: Running full initialization...");
                    AuthToken(NULL);
                } else {
                    DEBUG(">> RECONNECTION: Skipping hardware re-init. Resuming normal operations.");
                    // The main loop will automatically resume socketIO.loop() 
                    // now that wifiStatus is 1.
                }
            }
            break;

        case ARDUINO_EVENT_WIFI_AP_START: // ID: 15
            Serial.println(" - Meaning: AP Started");
            DEBUG("[WiFi] AP Event: Started."); // Your existing debug message
            _apEnabled = true;
            break;

        case ARDUINO_EVENT_WIFI_AP_STOP: // ID: 16
            Serial.println(" - Meaning: AP Stopped");
            DEBUG("[WiFi] AP Event: Stopped."); // Your existing debug message
            _apEnabled = false;
            break;

        // --- You can add more cases here for other events if needed ---
        // Example:
        // case ARDUINO_EVENT_WIFI_AP_STACONNECTED: // ID: 18
        //     Serial.println(" - Meaning: Client Connected to AP");
        //     break;
        // case ARDUINO_EVENT_WIFI_AP_STADISCONNECTED: // ID: 19
        //     Serial.println(" - Meaning: Client Disconnected from AP");
        //     break;

        default:
            // Print meaning for unhandled events
            Serial.println(" - Meaning: Unhandled or Unknown Event");
            break;
    }
}

String Devsbot::readFileFromSPIFFS(const char* path) {
    if (!SPIFFS.exists(path)) {
        return "";
    }

    File file = SPIFFS.open(path, "r");
    if (!file) {
        return "";
    }

    String fileContent = file.readString();
    file.close();
    return fileContent;
}


void Devsbot::parseNetworkData(const String& data) {
    // Parse the network data JSON
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, data);
    if (error) {
        DEBUG("Failed to parse network data");
        DEBUG(error.c_str());
        return;
    }

    const char* connectivityType = doc["connectivityType"].as<const char*>();
    if (connectivityType == nullptr) {
        DEBUG("Connectivity type is missing");
        return;
    }

    if (strcmp(connectivityType, "wifi") == 0) {
        if (!doc.containsKey("wifi") || !doc["wifi"].containsKey("ssid") || !doc["wifi"].containsKey("password")) {
            DEBUG("WiFi configuration is missing or incomplete");
            return;
        }

        JsonObject wifiConfig = doc["wifi"];
        
        // Load credentials into class variables
        cnfSsid = wifiConfig["ssid"].as<String>();
        cnfPass = wifiConfig["password"].as<String>();
        DEBUG("Loaded credentials for SSID: " + cnfSsid);
        DEBUG("Loaded Credentials for PASSWORD: "+ cnfPass);

        // Handle static IP config (this part is fine)
        if (wifiConfig.containsKey("ipMode") && strcmp(wifiConfig["ipMode"], "static") == 0) {
            if (wifiConfig.containsKey("ip") && wifiConfig.containsKey("gateway") && 
                wifiConfig.containsKey("subnet") && wifiConfig.containsKey("dns")) {
                
                IPAddress ip, gateway, subnet, dns;
                ip.fromString(wifiConfig["ip"].as<String>());
                gateway.fromString(wifiConfig["gateway"].as<String>());
                subnet.fromString(wifiConfig["subnet"].as<String>());
                dns.fromString(wifiConfig["dns"].as<String>());
                
                if (!WiFi.config(ip, gateway, subnet, dns)) {
                    DEBUG("STA Failed to configure static IP");
                }
            } else {
                 DEBUG("Static IP configuration is incomplete");
            }
        }

        // --- MODIFIED LOGIC ---
        // Trigger a non-blocking connection attempt
        nonBlockingConnectSTA();
        // We no longer wait here. The event handler will catch the connection.

    } else {
        DEBUG("Ethernet/GPRS is not supported yet");
        notConnetedtoNetwork = 1;
    }
}

// void Devsbot::begin() {
//     WiFi.onEvent(std::bind(&Devsbot::WiFiEvent, this, std::placeholders::_1));
//     DEBUG("Hello i am devsbot...!\n");
//     DEBUG("passing only a macid\n");
//     Serial.print("MAC ID : "); Serial.println(WiFi.macAddress());

//     // Initialize the SPIFFS file system
//     while (!SPIFFS.begin(true)) {
//         DEBUG("Failed to initialize SPIFFS, retrying...");
//         delay(1000);
//     }
//     DEBUG("SPIFFS was Successfully initialized\n");
    
//     // Create HTTP mutex
//     xHttpMutex = xSemaphoreCreateMutex();
//     if (xHttpMutex == NULL) {
//         DEBUG("Failed to create mutex");
//     }

//     // Load server configuration - this determines if we proceed with server operations
//     bool serverConfigExists = loadServerConfig();
    
//     // If no server config, don't set defaults and don't proceed with server operations
//     if (!serverConfigExists) {
//         DEBUG("[Devsbot] No server configuration available");
//         DEBUG("[Devsbot] Running in local mode - AP and WiFi only");

//         // Set local mode flag to prevent server API calls
//         localModeOnly = true;
        
//         // Still handle network configuration for local operations
//         EEPROMfirmwareVersion();
        
//         // Check if network data file exists for WiFi connection
//         if (SPIFFS.exists("/network_data.json")) {
//             String networkData = readFileFromSPIFFS("/network_data.json");
//             DEBUG("Network data found: " + networkData);
            
//             // Parse and connect to WiFi (for local operations)
//             parseNetworkData(networkData);
            
//             if (notConnetedtoNetwork || WiFi.status() != WL_CONNECTED) {
//                 DEBUG("WiFi connection failed, starting AP mode");
//                 startAPMode();
//             } else {
//                 DEBUG ("WiFi connected - ready for local operations");
//             }
//         } else {
//             // No network config found, start AP mode immediately
//             DEBUG("No network configuration found, starting AP mode");
//             notConnetedtoNetwork = 1;
//             startAPMode();
//         }
        
//         // Handle meter configuration if exists (for local operations)
//         if (SPIFFS.exists("/meter_Address.json")) {
//             meterAddressData();
//             meterAddInSpiff = 1;
//         }
        
//         // Don't call AuthToken or any server-related functions
//         return;
//     }

//     // Server config exists - proceed with full initialization
//     Serial.println("[Devsbot] Server configuration loaded - full mode enabled");
//     localModeOnly = false;  // Enable server operations
    
//     // Check pre-provision data and save to network file if needed
//     if ((!devicePreProvisionData()) && (!SPIFFS.exists("/network_data.json"))) {
//         Serial.printf("Save the SSID and password from preprovision to network file\n");
//         saveNetworkData(preSsid, prePassword, String(deviceconnectivity));
//     }

//     EEPROMfirmwareVersion();

//     // Check if network data file exists
//     if (SPIFFS.exists("/network_data.json")) {
//         String networkData = readFileFromSPIFFS("/network_data.json");
//         Serial.println("Network data found: " + networkData);
        
//         // Parse the network data and configure the network settings
//         parseNetworkData(networkData);

//         // Only call AuthToken if network configuration is successful AND server config exists
//         if (!notConnetedtoNetwork && WiFi.status() == WL_CONNECTED) {
//             AuthToken(NULL);
//         } else {
//             Serial.println("Network connection failed, starting AP mode");
//             startAPMode();
//         }
//     } else {
//         notConnetedtoNetwork = 1;
//         if (SPIFFS.exists("/meter_Address.json")) {
//             meterAddressData();
//             meterAddInSpiff = 1;
//         }
        
//         // No network config found, start AP mode immediately
//         Serial.println("No network configuration found, starting AP mode");
//         startAPMode();
//     }
//     initialize();
//         // --- ADD THIS LINE ---
//     // Start the sensor task once, after all initial setup is complete.
//     startSensorTask();

// }


void Devsbot::begin() {
    Serial.println("\n========================================");
    Serial.println("      Devsbot Initialization Start      ");
    Serial.println("========================================");

    WiFi.onEvent(std::bind(&Devsbot::WiFiEvent, this, std::placeholders::_1));
    DEBUG("MAC Address: " + String(WiFi.macAddress()));

    // Initialize SPIFFS
    DEBUG("Initializing SPIFFS...");
    while (!SPIFFS.begin(true)) {
        DEBUG(" - Failed, retrying in 1s...");
        delay(1000);
    }
    DEBUG(" - SPIFFS Initialized successfully");
    
    // Create HTTP mutex
    xHttpMutex = xSemaphoreCreateMutex();
    if (xHttpMutex == NULL) {
        DEBUG("[ERROR] Failed to create HTTP mutex!");
    } else {
        DEBUG("HTTP mutex created");
    }

    // --- NEW WIFI LOGIC ---
    _bootTime = millis();
    _lastStaDisconnectTime = millis(); 
    _lastStaRetryTime = 0;
    _needsPostConnectionSetup = false;
    _isInitialized = false; 

    DEBUG("[WiFi] Setting mode to AP+STA and starting AP...");
    WiFi.mode(WIFI_AP_STA);
    startAP(); 

    // Load server configuration
    DEBUG("[Config] Loading server configuration...");
    bool serverConfigExists = loadServerConfig();
    
    // Check for pre-provision data
    if ((!devicePreProvisionData()) && (!SPIFFS.exists("/network_data.json"))) {
        DEBUG("[Config] First-time setup: Saving pre-provisioned credentials to network file.");
        saveNetworkData(preSsid, prePassword, String(deviceconnectivity));
    }

    EEPROMfirmwareVersion();

    // Check if network data file exists
    if (SPIFFS.exists("/network_data.json")) {
        DEBUG("[WiFi] Found network_data.json. Parsing...");
        String networkData = readFileFromSPIFFS("/network_data.json");
        // DEBUG("Raw Network Data: " + networkData); // Optional: uncomment if needed
        parseNetworkData(networkData);
    } else {
        DEBUG("[WiFi] No network_data.json found. Staying in AP mode.");
        notConnetedtoNetwork = 1;
    }

    // If no server config, run in local mode
    if (!serverConfigExists) {
        DEBUG("\n[MODE] >> RUNNING IN LOCAL MODE (No server config found) <<");
        localModeOnly = true;
        if (SPIFFS.exists("/meter_Address.json")) {
            meterAddressData();
            meterAddInSpiff = 1;
        }
        DEBUG("========================================");
        DEBUG("      Devsbot Init Complete (Local)     ");
        DEBUG("========================================");
        return; 
    }

    // Server config exists
    DEBUG("\n[MODE] >> RUNNING IN FULL MODE (Server config loaded) <<");
    localModeOnly = false;
    
    // Handle fast-connect scenario where we might already have IP
    if (wifiStatus == 1) { 
         DEBUG("[WiFi] Already connected! Triggering AuthToken immediately.");
         AuthToken(NULL);
    }

    initialize();
    startSensorTask();

    DEBUG("========================================");
    DEBUG("      Devsbot Init Complete (Full)      ");
    DEBUG("========================================");
}

// The call to digitalInput.initialize is now simpler.
void Devsbot::initialize() {
    // ... your existing initialization code ...
    
    // The sdcard object is now global and manages its own resources,
    // so we don't need to pass pointers to it or its mutex anymore.
    digitalInput.initialize(this); 
}
// And finally, create the public function that calls the private one.
void Devsbot::startSensorTask() {
    digitalInput.startSensorTask();
}


// void Devsbot::initialize() {
//     digitalInput.initialize(this, &card, xHttpMutex);
// }

/**
 * @brief Starts the Access Point in a non-blocking way.
 * Uses the class's existing apSSID and apPassword variables.
 */
void Devsbot::startAP() {
    // Make sure mode is AP+STA before starting AP
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(apSSID, apPassword);
    _apEnabled = true;
    DEBUG("[WiFi] AP Started. SSID: " + apSSID);
    DEBUG("[WiFi] AP IP: " + WiFi.softAPIP().toString());
}

/**
 * @brief Stops the Access Point and switches mode to STA only.
 */
void Devsbot::stopAP() {
    if (!_apEnabled) return; // Already off
    
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
    _apEnabled = false;
    DEBUG("[WiFi] AP Stopped.");
}

/**
 * @brief Attempts to connect to the STA network non-blockingly.
 * Uses the class's existing cnfSsid and cnfPass variables.
 */
void Devsbot::nonBlockingConnectSTA() {
    if (cnfSsid.isEmpty()) {
        DEBUG("[WiFi] No STA credentials. Skipping connect.");
        return;
    }
    
    DEBUG("[WiFi] Attempting STA connection to: " + cnfSsid);
    _lastStaRetryTime = millis();
    WiFi.begin(cnfSsid.c_str(), cnfPass.c_str());
}

// Add this helper method to your Devsbot class
void Devsbot::startAPMode() {
    const char* apSSID = "Devsbot-AP";
    const char* apPassword = "devsbot123";
    
    DEBUG("Starting Access Point mode...");
    WiFi.softAP(apSSID, apPassword);
    DEBUG("AP IP address: " + String(WiFi.softAPIP()));
    DEBUG("Connect to the AP using SSID: " + String(apSSID) + " Password: " + String(apPassword));
    // Set the flag to indicate we're in AP mode
    notConnetedtoNetwork = 1;
    wifiStatus = 0;
}

void Devsbot:: RWToEEPROM(const char* authToken) //perform both read and write 
{
  bool checkval;
  preferences.begin("devsbot", false);
  if(!preferences.getBool("check",0)) // writing a authtoke to EEPROM once ,if check is not there in namespcae it will return a 0 ,while opening a eeprom for a first time it will return a 0
  {
    DEBUG("writing a authtoken to a EEPROM...\n"); //put devcie log here
    preferences.putBool("check",1); // check variable make a check key value to 1 again if we open a  namespce devsbot it value set to 1
    preferences.putString("authtoken",authToken);
  }
  checkval=preferences.getBool("check",defaultCheckVal);
  devsbotAuthToken=preferences.getString("authtoken","");
  DEBUG("authtoken read from RWEEPROM :  ");DEBUG(devsbotAuthToken);
  DEBUG("checkval :  ");DEBUG(checkval);
  preferences.end();
}

void Devsbot:: ReadOnlyEEPROM()
{
  preferences.begin("devsbot", true);
  devsbotAuthToken=preferences.getString("authtoken","");
  DEBUG("authtoken read from Read only EEPROM :  ");DEBUG(devsbotAuthToken);
  preferences.end();
}


void Devsbot ::wifiConnectionChecking(String ssid,String passwrd)
{
  WiFi.begin(ssid.c_str(),passwrd.c_str());
  DEBUG("wifi connection cheking begin..\n");
  uint64_t wifiStartMillis = millis();
  uint64_t wifiEndMillis = wifiStartMillis + wifiWating;
  if(wifiStatus != 1)
  {
    while(wifiStartMillis <= wifiEndMillis)
    {
      wifiStartMillis = millis();
      if (wifiStatus == 1)
      {
        DEBUG("Device connected to: ");DEBUG(WiFi.SSID());
        delay(100);
        //vTaskDelay(200/portTICK_PERIOD_MS);
        DEBUG("device local IP : ");DEBUG(WiFi.localIP());
        if(devsbotAuthToken!="")
          DEBUG("Device connected to:  " + String(WiFi.SSID()) + "password: " + String(WiFi.psk()));
       // deviceLog("Device connected to:  " + String(WiFi.SSID()));
        break;
      }
    }
  }
  DEBUG("wifi connection End..\n");
}



void Devsbot::EEPROMfirmwareVersion() {
    DEBUG("\n--- [Checking Firmware Version] ---");
    preferences.begin("version", false); // Open read/write

    if (!preferences.getBool("tempVal", 0)) {
        DEBUG("First boot detected. Initializing firmware version to default (1.0).");
        preferences.putBool("tempVal", 1);
        preferences.putFloat("firmVersion", 1.0);
    }

    deviceFirmwareVersion = preferences.getFloat("firmVersion", defaultFirmwareVersion);
    preferences.end();

    DEBUG("Current Firmware Version: " + String(deviceFirmwareVersion, 2)); // Print with 2 decimal places
    DEBUG("-----------------------------------\n");
}

/**
 * @brief Sends an HTTP request with proper headers, retry logic, OTA handling, and logging.
 *
 * @param endpoint The full URL for the request.
 * @param payload The data to send (body of the request).
 * @param method The HTTP method (e.g., "POST", "GET", "OTA").
 * @param contentType The Content-Type header for the request. THIS IS THE FIX.
 * @param callerName A descriptive name for the calling function, used for logging.
 * @return The server's response string, or an error code/status as a string.
 */
// String Devsbot::sendHttpRequest(String endpoint, String payload, const char* method, const char* contentType, const char* callerName) 
// {
//     // --- 1. Structured Request Logging ---
//     String logHeader = "\n========================================\n";
//     logHeader += "[HTTP Request] Caller: [" + String(callerName) + "]\n";
//     logHeader += "----------------------------------------\n";
//     logHeader += "  - Method:     " + String(method) + "\n";
//     logHeader += "  - Endpoint:   " + endpoint + "\n";
//     if (payload != "") {
//         logHeader += "  - Payload:    " + payload + "\n";
//     } else {
//         logHeader += "  - Payload:    <none for GET/OTA request>\n";
//     }
//     logHeader += "----------------------------------------";
//     DEBUG(logHeader);

//     String responseString = "";
//     HTTPClient httpClient;
//     const TickType_t mutexTimeout = pdMS_TO_TICKS(5000);
    
//     if (xSemaphoreTake(xHttpMutex, mutexTimeout) == pdTRUE) 
//     {
//         if (wifiStatus == 0) {
//             DEBUG("  - Result:     ABORTED (WiFi Disconnected)");
//             DEBUG("========================================");
//             xSemaphoreGive(xHttpMutex);
//             return "WIFI_DISCONNECTED";
//         }

//         httpMethodTaken = 1;
//         uint8_t provisionCnt = 0;
//         int statusCode = 0;

//         // --- Special Handling for OTA ---
//         // (This logic remains verbose as OTA is a critical, long-running process)
//         if (strcmp(method, "OTA") == 0) 
//         {
//             logHeader = "\n========================================\n";
//             logHeader += "[HTTP Request] Caller: [" + String(callerName) + "]\n";
//             logHeader += "----------------------------------------\n";
//             logHeader += "  - Method:     " + String(method) + "\n";
//             logHeader += "  - Endpoint:   " + endpoint + "\n";
//             logHeader += "----------------------------------------";
//             DEBUG(logHeader);
            
//             httpClient.begin(endpoint);
//             httpClient.addHeader("Authorization", "Bearer " + jwtToken);
//             statusCode = httpClient.GET();

//             if (statusCode == 200) 
//               {
//                 int contentLength = httpClient.getSize();
//                 if (contentLength <= 0) {
//                     DEBUG("  - Result:     FAILED | OTA Error: Invalid content length received from server.");
//                     responseString = "OTA_BAD_LENGTH";
//                 } else if (!Update.begin(contentLength)) {
//                     DEBUG("  - Result:     FAILED | OTA Error: Not enough space.");
//                     responseString = "OTA_NO_SPACE";
//                 } 
//                 else 
//                  {
// // Replace the progress bar section in your OTA download while loop

// DEBUG("  - OTA Update: Starting download of " + String(contentLength) + " bytes...");
// WiFiClient* stream = httpClient.getStreamPtr();
// size_t written = 0;
// uint8_t buff[1024] = { 0 };

// // --- Progress Bar Variables ---
// const int progressBarWidth = 40;
// int lastPrintedPercentage = -1;
// const int updateInterval = 10; // Update every 10% instead of every 1%
// // --- End Progress Bar Variables ---

// while (httpClient.connected() && written < contentLength) {
//     if (wifiStatus == 0) {
//         DEBUG("  - Result:     FAILED | OTA Error: WiFi lost during download.");
//         Update.end(false);
//         responseString = "OTA_WIFI_LOST";
//         goto cleanup;
//     }
    
//     size_t available = stream->available();
//     if (available) {
//         int len = stream->readBytes(buff, min((size_t)1024, available));
//         if (Update.write(buff, len) != len) {
//             DEBUG("  - Result:     FAILED | OTA Error: Flash write failed.");
//             Update.end(false);
//             responseString = "OTA_WRITE_FAIL";
//             goto cleanup;
//         }
//         written += len;

//         // --- Update Progress Bar Every 10% ---
//         int currentPercentage = (written * 100) / contentLength;
//         int roundedPercentage = (currentPercentage / updateInterval) * updateInterval;
        
//         // Print only at 10%, 20%, 30%, ... 90%, 100%
//         if (roundedPercentage > lastPrintedPercentage && roundedPercentage >= updateInterval) {
//             lastPrintedPercentage = roundedPercentage;
//             int progressChars = (roundedPercentage * progressBarWidth) / 100;

//             // Build progress bar string
//             String progressBar = "  [";
//             for (int i = 0; i < progressBarWidth; ++i) {
//                 progressBar += (i < progressChars) ? "=" : " ";
//             }
//             progressBar += "] " + String(roundedPercentage) + "%";
            
//             // Use DEBUG macro for consistent logging
//             DEBUG(progressBar);
//         }
//         // --- End Progress Bar Update ---

//     } else {
//         vTaskDelay(pdMS_TO_TICKS(5));
//     }
// }

// // Continue with the rest of your completion checks...
// if (written != contentLength) {
//     DEBUG("  - Result:     FAILED | OTA Error: Download incomplete. Expected " + String(contentLength) + ", got " + String(written));
//     Update.end(false);
//     responseString = "OTA_INCOMPLETE_DOWNLOAD";
// } else if (Update.end(true)) {
//     if(Update.isFinished()) {
//         DEBUG("  - Result:     SUCCESS | OTA Update Completed & Verified.");
//         responseString = "OTA_SUCCESS";
//     } else {
//         DEBUG("  - Result:     FAILED | OTA Error: Update.end() successful, but isFinished() is false.");
//         responseString = "OTA_INCOMPLETE";
//     }
// } else {
//     DEBUG("  - Result:     FAILED | OTA Error Code: " + String(Update.getError()));
//     responseString = "OTA_ERROR";
// }
//                   }
//                 } 
//                   else 
//                   {
//                  // Handle initial GET request failure
//                  if (statusCode < 0) {
//                     DEBUG("  - Result:     FAILED | Code: " + String(statusCode) + " (" + httpClient.errorToString(statusCode) + ")");
//                  } else {
//                     DEBUG("  - Result:     FAILED | Server Error Code: " + String(statusCode));
//                  }
//                  responseString = "OTA_START_FAILED_" + String(statusCode); // More specific error
//             }
//         } // End if (strcmp(method, "OTA") == 0)

//         // --- Standard GET/POST with Retry Logic ---
//         else 
//         {
//             while (provisionCnt < apiHitCnt) 
//             {
//                 provisionCnt++;
//                 DEBUG("  - Attempt " + String(provisionCnt) + "/" + String(apiHitCnt) + "...");

//                 if (wifiStatus == 0) {
//                     DEBUG("  - Result:     ABORTED (WiFi lost during retry)");
//                     statusCode = -100; // Custom code for wifi lost
//                     goto cleanup;
//                 }

//                 httpClient.setTimeout(10000);
                
//                 httpClient.begin(endpoint);
//                 httpClient.addHeader("Authorization", "Bearer " + jwtToken);
                
//                 // *** THIS IS THE CRITICAL FIX ***
//                 // The Content-Type is now passed in as a parameter, not hardcoded.
//                 httpClient.addHeader("Content-Type", contentType);

//                 if (strcmp(method, "POST") == 0) {
//                     statusCode = httpClient.POST(payload);
//                 } else { // GET
//                     statusCode = httpClient.GET();
//                 }

//                 if (statusCode > 0) { // Check for a valid HTTP response code
//                     responseString = httpClient.getString();
//                     if (statusCode == 200 || statusCode == 201) {
//                         DEBUG("  - Result:     SUCCESS | Code: " + String(statusCode));
//                         DEBUG("  - Response: " + responseString);
//                         break; 
//                     } else {
//                          DEBUG("  - Result:     FAILED | Server Error Code: " + String(statusCode));
//                          DEBUG("  - Response: " + responseString);
//                     }
//                 } else { // Handle negative error codes from the library
//                     DEBUG("  - Result:     FAILED | Code: " + String(statusCode) + " (" + httpClient.errorToString(statusCode) + ")");
//                 }
                
//                 httpClient.end();
//                 if (provisionCnt >= apiHitCnt) {
//                     DEBUG("----------------------------------------");
//                     DEBUG("--> Request failed after all " + String(apiHitCnt) + " retries.");
//                     break;
//                 }
//                 vTaskDelay(pdMS_TO_TICKS(1000));
//             }
//         }

//     cleanup: // Single exit point for cleanup
//         httpClient.end();
//         httpMethodTaken = 0;
//         xSemaphoreGive(xHttpMutex);
//         DEBUG("========================================");

//         return (responseString != "") ? responseString : String(statusCode);
//     } 
//     else 
//     {
//         DEBUG("\n--> DEADLOCK DETECTED: HTTP mutex was busy for >5 seconds! Request from [" + String(callerName) + "] was blocked.");
//         httpMethodTaken = 0;
//         return "MUTEX_TIMEOUT_DEADLOCK";
//     }
// }


// --- MODIFIED sendHttpRequest to skip retries for batch sends ---
String Devsbot::sendHttpRequest(String endpoint, String payload, const char* method, const char* contentType, const char* callerName)
{
    // --- 1. Structured Request Logging ---
    String logHeader = "\n==============================================================================\n";
    logHeader += "[HTTP Request] Caller: [" + String(callerName) + "]\n";
    logHeader += "----------------------------------------\n";
    logHeader += "  - Method:     " + String(method) + "\n";
    logHeader += "  - Endpoint:   " + endpoint + "\n";
    if (payload != "") {
        // // Only log a snippet of potentially large batch payloads
        // if (strstr(callerName, "_Batch") != NULL && payload.length() > 200) {
        //     logHeader += "  - Payload:    (Batch Data Snippet) " + payload.substring(0, 100) + "..." + payload.substring(payload.length() - 100) + "\n";
        // } else {
        //     logHeader += "  - Payload:    " + payload + "\n";
        // }
        logHeader += "  - Payload:    " + payload + "\n";
    } else {
        logHeader += "  - Payload:    <none for GET/OTA request>\n";
    }
    logHeader += "----------------------------------------";
    DEBUG(logHeader);

    String responseString = "";
    HTTPClient httpClient;
    const TickType_t mutexTimeout = pdMS_TO_TICKS(5000);

    if (xSemaphoreTake(xHttpMutex, mutexTimeout) == pdTRUE)
    {
        if (wifiStatus == 0) {
            DEBUG("  - Result:     ABORTED (WiFi Disconnected)");
            DEBUG("==============================================================================");
            xSemaphoreGive(xHttpMutex);
            return "WIFI_DISCONNECTED";
        }

        httpMethodTaken = 1;
        int statusCode = 0;

        // *** NEW: Determine max attempts based on caller ***
        uint8_t maxAttempts = apiHitCnt; // Default retry count
        bool isBatchSend = (strstr(callerName, "_Batch") != NULL);
        if (isBatchSend) {
            maxAttempts = 1; // Only try ONCE for batch sends
            DEBUG("  - Batch send detected, attempting only once.");
        }
        // *** END NEW ***

        uint8_t attemptCnt = 0; // Renamed from provisionCnt for clarity

        // --- Special Handling for OTA ---
        if (strcmp(method, "OTA") == 0)
        {
            // ... (OTA logic remains the same) ...
            logHeader = "\n==============================================================================\n";
            logHeader += "[HTTP Request] Caller: [" + String(callerName) + "]\n";
            logHeader += "----------------------------------------\n";
            logHeader += "  - Method:     " + String(method) + "\n";
            logHeader += "  - Endpoint:   " + endpoint + "\n";
            logHeader += "----------------------------------------";
            DEBUG(logHeader);

            httpClient.begin(endpoint);
            httpClient.addHeader("Authorization", "Bearer " + jwtToken);
            statusCode = httpClient.GET();

            if (statusCode == 200)
              {
                int contentLength = httpClient.getSize();
                if (contentLength <= 0) {
                    DEBUG("  - Result:     FAILED | OTA Error: Invalid content length received from server.");
                    responseString = "OTA_BAD_LENGTH";
                } else if (!Update.begin(contentLength)) {
                    DEBUG("  - Result:     FAILED | OTA Error: Not enough space.");
                    responseString = "OTA_NO_SPACE";
                }
                else
                 {
                    DEBUG("  - OTA Update: Starting download of " + String(contentLength) + " bytes...");
                    WiFiClient* stream = httpClient.getStreamPtr();
                    size_t written = 0;
                    uint8_t buff[1024] = { 0 };

                    const int progressBarWidth = 40;
                    int lastPrintedPercentage = -1;
                    const int updateInterval = 10;

                    while (httpClient.connected() && written < contentLength) {
                        if (wifiStatus == 0) {
                            DEBUG("  - Result:     FAILED | OTA Error: WiFi lost during download.");
                            Update.end(false);
                            responseString = "OTA_WIFI_LOST";
                            goto cleanup; // Use goto for cleanup in OTA case
                        }

                        size_t available = stream->available();
                        if (available) {
                            int len = stream->readBytes(buff, min((size_t)1024, available));
                            if (Update.write(buff, len) != len) {
                                DEBUG("  - Result:     FAILED | OTA Error: Flash write failed.");
                                Update.end(false);
                                responseString = "OTA_WRITE_FAIL";
                                goto cleanup;
                            }
                            written += len;

                            int currentPercentage = (written * 100) / contentLength;
                            int roundedPercentage = (currentPercentage / updateInterval) * updateInterval;

                            if (roundedPercentage > lastPrintedPercentage && roundedPercentage >= updateInterval) {
                                lastPrintedPercentage = roundedPercentage;
                                String progressBar = "  [";
                                int progressChars = (roundedPercentage * progressBarWidth) / 100;
                                for (int i = 0; i < progressBarWidth; ++i) { progressBar += (i < progressChars) ? "=" : " "; }
                                progressBar += "] " + String(roundedPercentage) + "%";
                                DEBUG(progressBar);
                            }

                        } else {
                            vTaskDelay(pdMS_TO_TICKS(5));
                        }
                    }

                    if (written != contentLength) {
                        DEBUG("  - Result:     FAILED | OTA Error: Download incomplete. Expected " + String(contentLength) + ", got " + String(written));
                        Update.end(false);
                        responseString = "OTA_INCOMPLETE_DOWNLOAD";
                    } else if (Update.end(true)) {
                        if(Update.isFinished()) {
                            DEBUG("  - Result:     SUCCESS | OTA Update Completed & Verified.");
                            responseString = "OTA_SUCCESS";
                        } else {
                            DEBUG("  - Result:     FAILED | OTA Error: Update.end() successful, but isFinished() is false.");
                            responseString = "OTA_INCOMPLETE";
                        }
                    } else {
                        DEBUG("  - Result:     FAILED | OTA Error Code: " + String(Update.getError()));
                        responseString = "OTA_ERROR";
                    }
                  }
                }
                  else
                  {
                 if (statusCode < 0) { DEBUG("  - Result:     FAILED | Code: " + String(statusCode) + " (" + httpClient.errorToString(statusCode) + ")"); }
                 else { DEBUG("  - Result:     FAILED | Server Error Code: " + String(statusCode)); }
                 responseString = "OTA_START_FAILED_" + String(statusCode);
            }
        } // End OTA handling

        // --- Standard GET/POST Loop ---
        else
        {
            while (attemptCnt < maxAttempts) // Use maxAttempts here
            {
                attemptCnt++;
                DEBUG("  - Attempt " + String(attemptCnt) + "/" + String(maxAttempts) + "...");

                if (wifiStatus == 0) {
                    DEBUG("  - Result:     ABORTED (WiFi lost during retry)");
                    statusCode = -100; // Custom code
                    break; // Exit loop immediately on wifi loss
                }

                httpClient.setTimeout(10000);
                httpClient.begin(endpoint);
                httpClient.addHeader("Authorization", "Bearer " + jwtToken);
                httpClient.addHeader("Content-Type", contentType);

                if (strcmp(method, "POST") == 0) {
                    statusCode = httpClient.POST(payload);
                } else { // GET
                    statusCode = httpClient.GET();
                }

                if (statusCode > 0) {
                    responseString = httpClient.getString();
                    if (statusCode == 200 || statusCode == 201) {
                        DEBUG("  - Result:     SUCCESS | Code: " + String(statusCode));
                        // Shorten logged response if it's potentially very long (like batch data response)
                        if (responseString.length() > 300) {
                             DEBUG("  - Response: (Snippet) " + responseString.substring(0, 150) + "..." + responseString.substring(responseString.length()-150));
                        } else {
                             DEBUG("  - Response: " + responseString);
                        }
                        break; // Success, exit loop
                    } else {
                         DEBUG("  - Result:     FAILED | Server Error Code: " + String(statusCode));
                         if (responseString.length() > 300) {
                              DEBUG("  - Response: (Snippet) " + responseString.substring(0, 150) + "..." + responseString.substring(responseString.length()-150));
                         } else {
                              DEBUG("  - Response: " + responseString);
                         }
                    }
                } else {
                    DEBUG("  - Result:     FAILED | Code: " + String(statusCode) + " (" + httpClient.errorToString(statusCode) + ")");
                    responseString = String(statusCode); // Store error code as string for return
                }

                httpClient.end(); // End connection after each attempt

                if (attemptCnt >= maxAttempts) {
                    DEBUG("----------------------------------------");
                    DEBUG("--> Request failed after all " + String(maxAttempts) + " attempts.");
                    break; // Exit loop
                }
                vTaskDelay(pdMS_TO_TICKS(1000)); // Delay before next attempt (if any)
            }
        } // End Standard GET/POST

    cleanup: // Label for OTA cleanup path
        httpClient.end();
        httpMethodTaken = 0;
        xSemaphoreGive(xHttpMutex);
        DEBUG("==============================================================================");

        // Return response string on success, or error code/status string on failure
        return responseString;
    }
    else
    {
        DEBUG("\n--> DEADLOCK DETECTED: HTTP mutex busy >5s! Request [" + String(callerName) + "] blocked.");
        httpMethodTaken = 0;
        return "MUTEX_TIMEOUT_DEADLOCK";
    }
}



bool Devsbot::postDataToServer(uint8_t slaveid,uint8_t virtualpin,bool Status)
{
  uint8_t postflag=0;
  if( xSemaphoreTake(xHttpMutex, portMAX_DELAY) == pdTRUE)
  {
    if(wifiStatus==1)
    {
                  // --- THREAD-SAFE FIX ---
            // Declare the JsonDocument locally.
            DynamicJsonDocument doc(256);
            // --- END OF FIX ---
            
      HTTPClient httpMadura;
      DEBUG("##############post data to Server###############\n");
      Serial.printf("slave id : %d, virtualpin : %d , status : %d\n",slaveid,virtualpin,Status);
      
      String ledStatusResponse;
      int ledStausResponseCode;
      uint8_t apiJsonResponse;
      String ledStatusRequest= "slave_id=" + String(slaveid) + "&gateway_api_id=" + devsbotAuthToken + "&pin=" + String(virtualpin) + "&activity_status=" + Status;

      Serial.print("ledStatusRequest : ");Serial.println(ledStatusRequest);
      // Use dynamic URL instead of hardcoded ledStartStopApiURL
      Serial.println(getLedStartStopApiURL() + ledStatusRequest);
      httpMadura.begin(getLedStartStopApiURL());
      
      httpMadura.addHeader("Authorization", "Bearer " + jwtToken);
      httpMadura.addHeader("Content-Type", "application/x-www-form-urlencoded");
      ledStausResponseCode=httpMadura.POST(ledStatusRequest);
      Serial.printf("led status posted to server\n\n");
      
      if(ledStausResponseCode!=200)
      {
        Serial.print("ledStatus Response code : ");Serial.println(ledStausResponseCode);
        httpMadura.end();
        postflag=0;
      }
      else if(ledStausResponseCode==200)
      {
        ledStatusResponse=httpMadura.getString();
        Serial.print("ledStatusResponse String : ");Serial.println(ledStatusResponse);
        
        DeserializationError error = deserializeJson(doc,ledStatusResponse);
        if(!error)
        {
          apiJsonResponse=doc["status"].as<int>();
          if(apiJsonResponse==201 || apiJsonResponse==0)
          {
            httpMadura.end();
            postflag=1;
          }
          else
          {
            httpMadura.end();
            postflag=0;
          }
        }
      }
    }
    else
    {
      Serial.println("WifiState disconnected button state can't able to post to Server ");
      postflag=0;
    }

    if(postflag==1)
    {
      Serial.printf("led post data end\n");
      xSemaphoreGive(xHttpMutex);
      return 1;
    }
    else if(postflag==0)
    {
      Serial.printf("led post data end\n");
      xSemaphoreGive(xHttpMutex);
      return 0;
    }
  }
  else
    Serial.printf("postDataToServer xHttpMutex is occupied by some other task\n");
}



// void Devsbot::AuthToken(const char* authToken) // authtoken is sent form a macro hardcoded
// {
//     if (!canPerformServerOperations()) {
//         DEBUG("[Devsbot] Skipping AuthToken - server operations disabled");
//         return;
//     }
    
//     // Check if device is already provisioned (has files) or is being newly provisioned
//     if (authToken == NULL || SPIFFS.exists("/Devsbot_Document.json"))
//     {
//         // --- CASE 1: Device is already provisioned and reconnecting ---
//         if (SPIFFS.exists("/Devsbot_Document.json"))
//         {
//             DEBUG("\n========================================");
//             DEBUG("  START: Post-Connection Setup  ");
//             DEBUG("========================================");
//             DEBUG("Device file available. Initializing...");

//             // 1. Initialize Meter Configuration
//             if (SPIFFS.exists("/meter_Address.json"))
//             {
//                 meterAddressData(); // This function now prints its own clean log
//                 meterAddInSpiff = 1;
//             }
//             else
//             {
//                 DEBUG("[WARN] meter_Address.json not found! Attempting to fetch...");
//                 if(meterAddressInit())
//                 {
//                     meterAddressData(); // This function now prints its own clean log
//                 }
//             }
            
//             // 2. Initialize Widget/Pin Configuration
//             widgetPinInitialize(); // This function now prints its own clean log

//             // 3. Start live connection
//             socketIOConnection(); // This function prints its own "Attempting to connect..."
            
//             DEBUG("MAC ID: "  + String(WiFi.macAddress()));
            
//             // [CRITICAL FIX] Removed the call to Loop() here.
//             // This function will now complete and return, allowing the
//             // main program loop to run normally.

//             DEBUG("========================================");
//             DEBUG("   END: Post-Connection Setup   ");
//             DEBUG("========================================");
//         }
//         // --- CASE 2: Device is in developer mode (no files, no auth token) ---
//         else 
//         {
//             DEBUG("\n========================================");
//             DEBUG("  START: First Time Provisioning (Dev Mode)  ");
//             DEBUG("========================================");
//             DEBUG("Device in developer mode. Provisioning...");
//             if(AuthTokenAPI())
//             { 
//                 deviceProvision();
//                 deviceProvisionData();
//                 meterAddressInit();
//                 widgetBegin();
//             }
//             DEBUG("Provisioning complete. Restarting...");
//             ESP.restart();
//         }
//     }
//     // --- CASE 3: Device is being provisioned for the first time with a hardcoded token ---
//     else 
//     {
//         DEBUG("\n========================================");
//         DEBUG("  START: First Time Provisioning (Token)  ");
//         DEBUG("========================================");
//         DEBUG("Auth token provided. Provisioning...");
//         deviceProvision();
//         deviceProvisionData();
//         widgetBegin();
//         DEBUG("Provisioning complete. Restarting...");
//         ESP.restart();
//     }
// }


// In DevsbotEnergyLocal.cpp
void Devsbot::AuthToken(const char* authToken)
{
    if (!canPerformServerOperations()) {
        DEBUG("[Devsbot] Skipping AuthToken - server operations disabled");
        _isInitialized = false; // Ensure it's false if skipped
        return;
    }

    // Check if device is already provisioned or needs provisioning
    if (authToken == NULL || SPIFFS.exists("/Devsbot_Document.json"))
    {
        // --- CASE 1: Device is already provisioned ---
        if (SPIFFS.exists("/Devsbot_Document.json"))
        {
            DEBUG("\n========================================");
            DEBUG("  START: Post-Connection Setup  ");
            DEBUG("========================================");
            DEBUG("Device file available. Initializing...");

            // --- *** ADDED: Load token from EEPROM *** ---
            ReadOnlyEEPROM(); // Load devsbotAuthToken *before* proceeding
            if (devsbotAuthToken.isEmpty()) {
                DEBUG("[ERROR] AuthToken is empty after reading EEPROM! Cannot initialize fully.");
                 _isInitialized = false; // Cannot proceed without token
                 DEBUG("========================================");
                 DEBUG("   END: Post-Connection Setup (FAILED) ");
                 DEBUG("========================================");
                 return; // Stop initialization here
            }
            // --- *** END ADDED *** ---


            // Initialize Meter Config
            if (SPIFFS.exists("/meter_Address.json")) {
                meterAddressData();
                meterAddInSpiff = 1;
            } else {
                DEBUG("[WARN] meter_Address.json not found! Attempting to fetch...");
                if(meterAddressInit()) meterAddressData();
            }

            // Initialize Widget/Pin Config
            widgetPinInitialize();

            // Start live connection
            socketIOConnection();

            DEBUG("MAC ID: "  + String(WiFi.macAddress()));

            // --- *** SET FLAG ON SUCCESS *** ---
            _isInitialized = true; // Mark initialization complete
            // --- *** END SET FLAG *** ---

            DEBUG("========================================");
            DEBUG("   END: Post-Connection Setup   ");
            DEBUG("========================================");
        }
        // --- CASE 2: Device in developer mode (Needs API fetch) ---
        else
        {
            DEBUG("\n========================================");
            DEBUG("  START: First Time Provisioning (Dev Mode)  ");
            DEBUG("========================================");
            DEBUG("Device in developer mode. Provisioning...");
            if(AuthTokenAPI()) // AuthTokenAPI calls RWToEEPROM on success, populating devsbotAuthToken
            {
                // Token fetched and saved, proceed with rest of provisioning
                deviceProvision();
                deviceProvisionData();
                meterAddressInit();
                widgetBegin();
                // Although restarting, set flag conceptually for completeness *before* restart
                _isInitialized = true;
                DEBUG("Provisioning complete. Restarting...");
            } else {
                 DEBUG("[ERROR] AuthTokenAPI failed. Cannot complete provisioning.");
                 _isInitialized = false; // Failed to get token
                 DEBUG("========================================");
                 DEBUG("   END: Provisioning (Dev Mode FAILED)  ");
                 DEBUG("========================================");
                 // Consider not restarting if API failed, stay in AP mode? Or just let it restart.
            }
             // ESP.restart() happens regardless of AuthTokenAPI success in original code
             delay(1000); // Short delay before restart
             ESP.restart();
        }
    }
    // --- CASE 3: First time provisioning with hardcoded token ---
    else
    {
        DEBUG("\n========================================");
        DEBUG("  START: First Time Provisioning (Token)  ");
        DEBUG("========================================");
        DEBUG("Auth token provided. Provisioning...");
        // Save the provided token
        RWToEEPROM(authToken);
        if (devsbotAuthToken.isEmpty()) { // Verify token was saved/read back
             DEBUG("[ERROR] Failed to save/read provided AuthToken! Cannot provision.");
             _isInitialized = false;
             DEBUG("========================================");
             DEBUG("   END: Provisioning (Token FAILED)    ");
             DEBUG("========================================");
             // Consider not restarting here.
        } else {
            deviceProvision();
            deviceProvisionData();
            widgetBegin();
             // Although restarting, set flag conceptually
            _isInitialized = true;
            DEBUG("Provisioning complete. Restarting...");
        }
        // ESP.restart() happens regardless in original code
        delay(1000); // Short delay before restart
        ESP.restart();
    }
}

void Devsbot::wifiAfterProvision()
{ 
  DEBUG("wifiAfterProvision begin");
  if(deviceSsid!=devsbotSsid || devicePassword!=devsbotPassword)
  {
    DEBUG("devicessid and devsbotssid is not same");
    deviceLog("devicessid and devsbotssid is not same");
    WiFi.disconnect(); //disconnected from previously connected network/
    //while(wifiStatus!=0){}
    //vTaskDelay(500/portTICK_PERIOD_MS);
    delay(100);
    while(wifiStatus==1)
    {
      DEBUG("disconnecting form a previous connected network");
    }
    DEBUG("device disconnected from previously connected network");
    deviceSsid=devsbotSsid; //got a value form provision webzone. 
    devicePassword=devsbotPassword;
    if(userSsid!=NULL && userPassword!=NULL)
    {
      wifiBegin(userSsid,userPassword);
    }
    else
    {
      wifiBegin(NULL,NULL);
    }
  }
  else
  {
    DEBUG("devicessid and devsbotssid is same continue with connected network");
  }
  DEBUG("wifiAfterProvision End");
}

bool Devsbot::AuthTokenAPI()
{
  String authTokenRequest = "mac_id=" + WiFi.macAddress();
  // Use dynamic URL instead of hardcoded one
  String authtokenResponse = sendHttpRequest(getAuthTokenApiURL(), authTokenRequest, "POST","application/x-www-form-urlencoded", "AuthTokenAPI");
  DEBUG("Authtoken response :");DEBUG(authtokenResponse);

  if(authtokenResponse != "")
  {
        // --- THREAD-SAFE FIX ---
    DynamicJsonDocument doc(256);
    // --- END OF FIX ---

    DeserializationError error = deserializeJson(doc, authtokenResponse);   
    if(!error)
    {
      String status = doc["status"];
      if(status == "201")
      {
        String authToken = doc["cluster_api_id"]; //here gateway ID
        RWToEEPROM(authToken.c_str());
        DEBUG("Authentication token: "+ devsbotAuthToken+"\n");
        return 1;
      }
      else
      {
        DEBUG("Authentication token not received...!\n");
        ESP.restart();
      }
    }
    else
    {
      DEBUG("API response file error\n");
      deviceLog("deserialization error in authtokenapi ");
      return 0;
    }
  }
  else
  {
    DEBUG("Authtokenapi responsecode: " +  authtokenResponse);
    deviceLog("Authtokenapi responsecode: " + authtokenResponse);
    return 0;
  }
  DEBUG("AuthTokenAPI End");
}


bool Devsbot::deviceProvision()
{
  uint8_t provisionCnt=0;
  int responseStatus;
  DEBUG("Device provision starts...!\n");DEBUG("Authtoken => " + devsbotAuthToken + "\n");
  
  if(devsbotAuthToken.length() == 10)
  {
    // Use dynamic URL instead of hardcoded deviceProvisionURL
    String deviceProvisionAPI = getDeviceProvisionURL() + "?gateway_api_Id=" + String(devsbotAuthToken);
    DEBUG("Device provision request => "+ deviceProvisionAPI + "\n");   

    String provisionResponse = sendHttpRequest(deviceProvisionAPI, "", "GET","application/x-www-form-urlencoded", "deviceProvision");
    DEBUG("apiresponse String : ");DEBUG(provisionResponse);

    if(provisionResponse!="" && provisionResponse.startsWith("{"))
    {
      // --- THREAD-SAFE FIX ---
      DynamicJsonDocument doc(2048);
      // --- END OF FIX ---

      DeserializationError error = deserializeJson(doc, provisionResponse);
      if(!error)
      {
        responseStatus=doc["status"].as<int>();

        if(responseStatus!=200)
        {
          DEBUG("device not provisioned response status is not 200 read a old spiff data");
          DEBUG("device provision API response : " + String(apiResponse) + "\n");
          deviceLog("device not provisioned response status is not 200 read a old spiff data");
          deviceLog("device provision API response :  " +  apiResponse + "\n");
          return 0;
        }
        if (SPIFFS.exists("/Devsbot_Document.json"))
          SPIFFS.remove("/Devsbot_Document.json"); 
        File file = SPIFFS.open("/Devsbot_Document.json", "w");
        if (!file) 
        {
          DEBUG("Failed to open a provision file for reading");
          return 0;
        }

        file.print(provisionResponse);
        file.close();
        delay(100); // <<< ADD THIS LINE (100ms delay)
        DEBUG("Device Provisioned...!\n");
        deviceLog("Device Provisioned...!\n");   
        return 1;
      }
      else
      {
        DEBUG("apiResponse data file error\n");
        deviceLog("DeserializationError in deviceProvision" + String(error.f_str()));
        return 0;
      }
    }
    else
    {
      DEBUG("deviceProvision responsecode: " + provisionResponse);
      deviceLog("deviceProvision responsecode: " + provisionResponse);
      return 0;
    }
  }
  else
  {
    DEBUG("Error in parsing a devsbotAuthToken  length : ");DEBUG(devsbotAuthToken.length());
    deviceLog("Error in parsing a devsbotAuthToken  length : " +  devsbotAuthToken + "\n");
    return 0;
  }
}
void Devsbot:: deviceProvisionData()
{
  DEBUG("deviceProvisionData begin");

  if (SPIFFS.exists("/Devsbot_Document.json"))
  {
    File file = SPIFFS.open("/Devsbot_Document.json", "r");

    // while (file.available())
    // {
    //   provisionData= file.readStringUntil('\n');
    // }    
    // file.close();

    if (!file) {
        DEBUG("[ERROR] Failed to open /Devsbot_Document.json for reading!");
        return;
    }

    // --- Change this ---
    // provisionData= file.readStringUntil('\n');
    // --- To this ---
    provisionData = file.readString();
    // --- End Change ---

    file.close();

    if (provisionData.isEmpty()) {
         DEBUG("[ERROR] Read empty data from /Devsbot_Document.json!");
         return;
    }

    DEBUG("Read device provision data=> "+ provisionData+"\n");
    // --- THREAD-SAFE FIX ---
    DynamicJsonDocument doc(2048);
    // --- END OF FIX ---
    DeserializationError error = deserializeJson(doc, provisionData);

    if (!error)
    {
      
      jwtToken=doc["jwttoken"].as<String>();
      //jwtToken=doc["authtoken"].as<String>();

      devsbotconnectivity= doc["Id"].as<uint8_t>();

      // String postSsid="";
      // postSsid=doc["SSID"].as<String>();
      devsbotSsid=doc["SSID"].as<String>();
      DEBUG("Devsbot SSID=> "+ String(devsbotSsid)+ "\n");
      
      // String postPassword="";
      // postPassword=doc["password"].as<String>();

      devsbotPassword=doc["password"].as<String>();
      DEBUG("Devsbot password=> "+ String(devsbotPassword)+ "\n");

      // devsbotGprsApn = doc["devsApn"].as<String>();

      String heartBeat=doc["heartbeat"];
      heartBeatInterval=heartBeat.toInt();
      DEBUG("heartBeatInterval=> "+ String(heartBeatInterval)+ "\n");

      instantaneousReading= doc["live_data"];
      DEBUG("instantaneousReading=> "+ String(instantaneousReading)+ "\n");

      String dataInterval=doc["data_interval"];
      completeJsonReading=dataInterval.toInt();
      DEBUG("completeJsonReading=> "+ String(completeJsonReading)+ "\n");

      storeDataToSd=doc["store_type"].as<bool>();

      if(storeDataToSd)
        DEBUG("storeDataToSd is set to 1");




      //mtrMake=doc["device_Model"].as<uint8_t>();

      // JsonArray jsondevicemodel=doc["device_Model"];
      // uint8_t i;
      // for(i=0;i<jsondevicemodel.size();i++)

      // numSlave=doc["slave_id_count"];
      // Serial.print("no of slaves : ");Serial.println(numSlave);

      // JsonArray slaveId;
      // slaveId.clear(); //clear the past value in a array
      // slaveId = doc["slave_id"];
      // Serial.print("no of slave : ");Serial.println(slaveId.size());

      // memset(slaveIdArray, 0, sizeof(slaveIdArray));//clear the past array value 

      // for (uint8_t i = 0; i < slaveId.size(); i++) 
      // {
      //   slaveIdArray[i] = slaveId[i]; // Assuming values are within the byte range (0-255)
      //   Serial.println(slaveIdArray[i]); // Printing the values to the serial monitor
      // }

    // if(connectivity==0)
    //   wifiAfterProvision();

      //readNetworkData(); //by calling a read network data device ssid ,pass,connectivity got updated
      // /** store a ssid ,pass ,connectivity to a common file */
      // if((devsbotSsid != "") && ((deviceconnectivity != devsbotconnectivity) || (preSsid != devsbotSsid) || (prePassword != devsbotPassword)))
      //   saveNetworkData(devsbotSsid,devsbotPassword,apnName,String(devsbotconnectivity));
      

    }
    else
    {
      DEBUG("Provision data file error\n");
    }

  }
  DEBUG("deviceProvisionData End");
}

void Devsbot :: saveNetworkData(String ssid, String password,String type)
{
  String saveNetworkData="";
  
  StaticJsonDocument<200> netDoc;
  if (SPIFFS.exists("/network_data.json"))
      SPIFFS.remove("/network_data.json"); 


  DEBUG("ssid : ");DEBUG(ssid);
  DEBUG("pass : ");DEBUG(password);
  DEBUG("type : ");DEBUG(type);
  DEBUG("apssid : ");DEBUG(apSSID);


  File file = SPIFFS.open("/network_data.json", "w");
  if (!file)
  {
    DEBUG("Failed to open provision_data.txt for writing.");
    return;
  }

  DEBUG("Writing provisioning data to SPIFFS...");
 
  if(type == "" || ssid =="" || password == "" )
  {
    netDoc["ssid"]=cnfSsid;
    netDoc["password"]=cnfPass;
    netDoc["connectivity"]=connectivityType;

  }
  else
  {
    netDoc["ssid"]=ssid;
    netDoc["password"]=password;
    netDoc["connectivity"]=type;
  }
  netDoc["apssid"]=apSSID;

   // Serialize JSON to the file
  // if (serializeJson(doc, file) == 0) 
  if (serializeJson(netDoc, saveNetworkData) == 0) 
    DEBUG("Failed to write to file");
  else 
    DEBUG("JSON saved successfully");
  
  DEBUG("saveNetworkData : ");DEBUG(saveNetworkData);
  file.print(saveNetworkData);
  file.close();

  //delay(500); // Delay before restarting ESP32
  if(SPIFFS.exists("/Devsbot_Document.json") && ApNetworkFlag==0 )
  {
    readNetworkData();
    connectToNetwork();
  }
  else
  {
    DEBUG("Network data saved, restarting ESP32...");
    //ESP.restart(); // Restart the ESP32 to apply new configurations for first time
  }
}



bool Devsbot::devicePreProvisionData() {
    DEBUG("\n--- [Reading Pre-Provision Data] ---");

    if (!SPIFFS.exists("/Devsbot_Document.json")) {
        DEBUG("[WARN] /Devsbot_Document.json does not exist.");
        return true; // Return true to indicate provision data is missing
    }

    File file = SPIFFS.open("/Devsbot_Document.json", "r");
    if (!file) {
        DEBUG("[ERROR] Failed to open /Devsbot_Document.json for reading.");
        return true;
    }

    provisionData = file.readString();
    file.close();

    if (provisionData.isEmpty()) {
         DEBUG("[ERROR] /Devsbot_Document.json is empty!");
         return true;
    }

    DEBUG("Raw Provision Data: " + provisionData); // Optional: keep commented unless debugging JSON

    DynamicJsonDocument doc(2048);
    DeserializationError error = deserializeJson(doc, provisionData);

    if (error) {
        DEBUG("[ERROR] JSON Deserialization failed: " + String(error.c_str()));
        return true;
    }

    // --- Parsing successful, load data ---
    if(devsbotAuthToken == "") {
        DEBUG("AuthToken not in RAM, reading from EEPROM...");
        ReadOnlyEEPROM();
    }

    jwtToken = doc["jwttoken"].as<String>();

    // Safely get ID
    if (doc["Id"].is<int>()) {
        deviceconnectivity = doc["Id"].as<uint8_t>();
    } else {
        DEBUG("[WARN] 'Id' field missing/invalid, defaulting to 0.");
        deviceconnectivity = 0;
    }

    // Load other fields with defaults if missing
    preSsid = doc["SSID"] | "";
    prePassword = doc["password"] | "";
    heartBeatInterval = doc["heartbeat"] | 60;
    completeJsonReading = doc["data_interval"] | 60;
    
    if (doc.containsKey("live_data")) {
        instantaneousReading = doc["live_data"].as<bool>();
    } else {
        instantaneousReading = false; // Default
    }

    if (doc.containsKey("store_type")) {
        storeDataToSd = doc["store_type"].as<bool>();
    } else {
        storeDataToSd = false; // Default
    }

    // --- Print Summary of Loaded Data ---
    DEBUG("Data Loaded Successfully:");
    DEBUG(" - Connectivity ID: " + String(deviceconnectivity));
    DEBUG(" - SSID: " + preSsid);
    DEBUG(" - Pass: " + prePassword); // Usually better to hide password in standard logs
    DEBUG(" - Heartbeat Interval: " + String(heartBeatInterval) + "s");
    DEBUG(" - Data Interval: " + String(completeJsonReading) + "s");
    DEBUG(" - Live Data: " + String(instantaneousReading ? "YES" : "NO"));
    DEBUG(" - Store to SD: " + String(storeDataToSd ? "YES" : "NO"));
    DEBUG("------------------------------------\n");

    return false; // Return false to indicate success (provision data EXISTS)
}


void Devsbot :: disconnectFromNetwork() 
{
  Serial.println("Disconnecting from Network...");
  if(wifiStatus==1)
  {
    WiFi.disconnect();
    delay(200);
  }

}

bool Devsbot :: readNetworkData()
{

  bool networkReadSuccess=0;
  StaticJsonDocument<200> netDoc;
  // String cnfSsid="";
  // String cnfPass="";
  // String cnfApn="";
  String networkString="";
  DEBUG("reading network Data...");
  File file = SPIFFS.open("/network_data.json", "r");
  if (!file)
  {
    DEBUG("fail to open a networkData file ");
    file.close();
    return networkReadSuccess;
  }

  while (file.available())
  {
    networkString= file.readStringUntil('\n');
  } 

  DEBUG("networkString : ");DEBUG(networkString );


  // DeserializationError error = deserializeJson(doc,file);
  DeserializationError error = deserializeJson(netDoc,networkString);
  file.close();

  
  if(!error)
  {
   cnfSsid =netDoc["ssid"].as<String>();
   cnfPass =netDoc["password"].as<String>();
  //  cnfApn = netDoc["apn"].as<String>();
   connectivityType=netDoc["connectivity"].as<String>();
   apSSID=netDoc["apssid"].as<String>();
  

    DEBUG("connectivityType : " +  connectivityType);
    DEBUG("ssid : " + cnfSsid);
    DEBUG("pass : " + cnfPass);
    DEBUG("apssid: " + apSSID );
    // Serial.println("apn : " + cnfApn);
    networkReadSuccess=1;
  }
  else
    DEBUG("deserialization error in storing a network credentials\n");

  return networkReadSuccess;

}

bool Devsbot :: connectToNetwork()
{
  bool connectionSucess=0;
  disconnectFromNetwork();
  if (connectivityType == "0")
  { 
    DEBUG("Connecting via WiFi...");
    DEBUG("SSID: " + cnfSsid);
    if(cnfSsid!="" && cnfPass!="" )
    {
      while(wifiStatus!=1)
      {
        wifiConnectionChecking(cnfSsid,cnfPass);
        connectionSucess=(wifiStatus==1)?1:0;
        if(storeDataToSd)
          break;
      }
    }

  }
  else 
  {
    /** Again start a AP mode */
    Serial.println("conectivity is empty ");
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(apSSID.c_str(), apPassword);
  }
  return connectionSucess;
}


void Devsbot::deviceFirmwareUpdate()
{
  if (devsbotFirmwareVersion > deviceFirmwareVersion)
  {
    DEBUG("The device is going to update the firmware\n");
    DEBUG("The device is going to update the firmware");
    firmwareUpdate();
  }

  // else if (devsbotFirmwareVersion == deviceFirmwareVersion)
  // {
  //   DEBUG("The firmware version is the same and the device is going to initialize a devspot connection\n");
  //   deviceLog("The firmware version is the same and the device is going to initialize a devspot connection");
  // }

  else if (devsbotFirmwareVersion < deviceFirmwareVersion)
  {
    DEBUG("The device is going to rollback the firmware\n");
    DEBUG("The device is going to rollback the firmware");
    firmwareUpdate();
  }
}

void Devsbot:: widgetBegin()
{
  DEBUG("Widget begin=> Devsbot widget version are checking...!\n");
  widgetAPI();
  widgetPinInitialize();
}

// ... inside the Devsbot::widgetAPI() function ...

bool Devsbot::widgetAPI() {
    DEBUG("widgetAPI begin");

    // --- NEW LOGIC: Make a live request to the server ---
    // 1. Build the correct URL for the request.
    String devsbotWidgetAPI = getDevsbotWidgetURL() + "?cluster_id=" + devsbotAuthToken;

    // 2. Call the sendHttpRequest function to get the widget configuration.
    String widgetResponse = sendHttpRequest(devsbotWidgetAPI, "", "GET","application/x-www-form-urlencoded", "widgetAPI");
    
    // 3. Check if the response is valid. It should be a JSON array.
    if (widgetResponse.isEmpty() || !widgetResponse.startsWith("[")) {
        DEBUG("Widget API request failed or received invalid data.");
        deviceLog("Widget API response error: " + widgetResponse);
        return false; // Indicate failure
    }
    // --- END OF NEW LOGIC ---
    
    DEBUG("Widget API response=> " + widgetResponse + "\n");

    if (SPIFFS.exists("/Devsbot_Widget.json")) {
        SPIFFS.remove("/Devsbot_Widget.json");
    }

    File file = SPIFFS.open("/Devsbot_Widget.json", "w");
    if (file) {
        file.print(widgetResponse);
        file.close();
        DEBUG("Device widget data updated...!\n");
        deviceLog("Device widget data updated\n");
        
        // This function call is part of the original design, allowing the
        // DigitalInput class to know that a refresh happened.
        digitalInput.widgetAPI();
    } else {
        DEBUG("Failed to open file for writing");
        deviceLog("Failed to open file for writing");
        return false; // Return false on failure
    }

    DEBUG("widgetAPI End");
    return true; // Return true on success
}

void Devsbot::widgetPinInitialize() {
    DEBUG("widgetPinInitialize begin");
    digitalInputPin = "";
    digitalInputPullupPin = "";
    analogInputPin = "";
    widgetData = "";

    if (SPIFFS.exists("/Devsbot_Widget.json")) {
        File file = SPIFFS.open("/Devsbot_Widget.json", "r");

        while (file.available()) {
            widgetData += file.readStringUntil('\n');
        }
        file.close();
        // DEBUG("Read devsbot widget data=> " + widgetData);

        // Parse the configuration for QC testing
        parseWidgetConfiguration();
        
        // Initialize pins based on parsed configuration
        initializeDIOPins();
        DEBUG("WidgetData: " + widgetData);
        // Initialize digital inputs using the DigitalInput class
        // Pass the data we just read to the DigitalInput class
        digitalInput.widgetPinInitialize(widgetData); // <--- Pass the string here


        // Start the digital input monitoring task
        //digitalInput.startSensorTask();

        // Print configuration for debugging
        digitalInput.printPinConfiguration();
    } else {
        DEBUG("Widget data file not found");
    }
    DEBUG("widgetPinInitialize End");
}



// --- 4. MODIFIED sendActivityTrackerData() ---
// This now saves to the SD card if the live HTTP request fails and includes the pin type.
// void Devsbot::sendActivityTrackerData() {
//     // Optimize memory: Declare JsonDocument once outside the loop.
//     JsonDocument jsonDoc;

//     for (int i = 0; i < digitalInput.getStatusInputCount(); i++) {
//         StatusInputData* status = digitalInput.getStatusInput(i);

//         // Process only if the status is valid and its state has changed.
//         if (status && status->stateChanged) {
            
//             // a. Prepare the JSON payload.
//             jsonDoc.clear(); // Clear previous data before reuse.
//             jsonDoc["slave_id"] = 1;
//             jsonDoc["gateway_api_id"] = devsbotAuthToken;
//             jsonDoc["pin"] = status->pin;
//             jsonDoc["activity_status"] = status->currentState;
//             jsonDoc["time_stamp"] = status->lastChangeTime;

//             // b. Set the type directly from the enum's integer value.
//             // This correctly creates a JSON number, e.g., "type": 2, not "type": "2".
//             // This assumes your enum is defined like: enum InputType { MACHINE = 1, MOTOR = 2, HEATER = 3 };
//             jsonDoc["type"] = status->type_name; 

//             String jsonString;
//             serializeJson(jsonDoc, jsonString);

//             DEBUG("Attempting to send LIVE Activity Data: " + jsonString);

//             // c. Try to send the data to the server.
//             String response = sendHttpRequest(getLedStartStopApiURL(), jsonString, "POST","application/json", "ActivityTrackerData");

//             // d. Check the server response.
//             JsonDocument responseDoc;
//             // NEW (Correct)
//             if (!response.isEmpty() && deserializeJson(responseDoc, response).code() == DeserializationError::Ok && responseDoc["status"] == 200) {
//                 DEBUG("Live Activity Data sent successfully.");
                
//                 // It's important to reset the flag on success, too!
//                 status->stateChanged = false; 

//             } else {
//                 // e. If sending failed, save to the SD card as a fallback.
//                 DEBUG("Live send failed.");
//                 if (storeDataToSd && card.cardMounted) {
//                     DEBUG("Saving to SD card.");
//                     card.statusLogs.saveLog(jsonString);
//                 } else {
//                     DEBUG("SD storage disabled or card not mounted. Data was lost.");
//                 }
                
//                 // Reset the flag even on failure to avoid sending the same old data again.
//                 // You might want to handle this differently, e.g., only reset on success,
//                 // but that could lead to an infinite loop of trying to send the same failed data.
//                 status->stateChanged = false;
//             }
//         }
//     }
// }


// // [REPLACE THE ENTIRE FUNCTION]
// void Devsbot::sendActivityTrackerData() {
//     // Optimize memory: Declare JsonDocument once outside the loop.
//     JsonDocument jsonDoc;

//     for (int i = 0; i < digitalInput.getStatusInputCount(); i++) {
//         StatusInputData* status = digitalInput.getStatusInput(i);

//         // Process only if the status is valid and its state has changed.
//         if (status && status->stateChanged) {

//             // 1. Prepare payload
//             jsonDoc.clear(); // Clear previous data before reuse.
//             jsonDoc["slave_id"] = 1;
//             jsonDoc["gateway_api_id"] = devsbotAuthToken;
//             jsonDoc["pin"] = status->pin;
//             jsonDoc["activity_status"] = status->currentState;
//             jsonDoc["time_stamp"] = status->lastChangeTime;
//             jsonDoc["type"] = status->type_name;

//             String jsonString;
//             serializeJson(jsonDoc, jsonString);

//             // --- THIS IS THE FIX ---
//             // This new logic ensures all SD card access is thread-safe.

//             if (storeDataToSd && card.cardMounted)
//             {
//                 // 2. Always store to SD card first using the MUTEX-PROTECTED wrapper.
//                 DEBUG("Status change detected. Saving to SD card...");
//                 // Use the wrapper function from sdcard.cpp
//                 bool saveSuccess = card.saveStatusLog(jsonString); 

//                 // 3. If WiFi is on AND save was successful, trigger a MUTEX-PROTECTED process.
//                 if (wifiStatus && saveSuccess) {
//                     DEBUG("[DI URGENT] Saved. Triggering immediate SD processing...");
//                     // Use the main, mutex-protected processing function
//                     card.processStatusLogQueue(); // <-- WITH THIS
//                     DEBUG("[DI URGENT] Immediate send complete.");
//                 } else if (!saveSuccess) {
//                     DEBUG("[DI URGENT] Status save FAILED (mutex busy or card error).");
//                 } else {
//                     DEBUG("[DI URGENT] Status saved to SD (WiFi is offline). Will be sent later.");
//                 }
//             }
//             else if (wifiStatus) // Not storing to SD, but WiFi is on
//             {
//                 // 4. Fallback to live-only send (this was already correct)
//                 DEBUG("SD storage disabled. Sending Activity data live...");
//                 String response = sendHttpRequest(getLedStartStopApiURL(), jsonString, "POST", "application/json", "ActivityTrackerData");
                
//                 JsonDocument responseDoc;
//                 DeserializationError error = deserializeJson(responseDoc, response);
//                 if (error == DeserializationError::Ok && (responseDoc["status"] == 200 || responseDoc["status"] == 201)) {
//                     DEBUG("Live Activity Data sent successfully.");
//                 } else {
//                     DEBUG("Live Activity Data send failed. Data was lost.");
//                 }
//             }
//             else
//             {
//                 DEBUG("SD storage disabled and WiFi offline. Status data lost.");
//             }
//             // --- END OF FIX ---
            
//             // 5. Reset the flag after attempting to save/send
//             status->stateChanged = false;
//         }
//     }
// }

// [REPLACE THE ENTIRE sendActivityTrackerData FUNCTION WITH THIS ROBUST VERSION]
void Devsbot::sendActivityTrackerData() {
    // Optimize memory: Declare JsonDocument once outside the loop.
    DynamicJsonDocument jsonDoc(512);

    for (int i = 0; i < digitalInput.getStatusInputCount(); i++) {
        StatusInputData* status = digitalInput.getStatusInput(i);

        // Process only if the status is valid and its state has changed.
        if (status && status->stateChanged) {
            
            // --- 1. Prepare the Payload ---
            jsonDoc.clear();
            jsonDoc["slave_id"] = 1;
            jsonDoc["gateway_api_id"] = devsbotAuthToken;
            jsonDoc["pin"] = status->pin;
            jsonDoc["activity_status"] = status->currentState;
            jsonDoc["time_stamp"] = status->lastChangeTime;
            jsonDoc["type"] = status->type_name;

            String jsonString;
            serializeJson(jsonDoc, jsonString);

            bool dataWasSecured = false;

            // --- 2. ATTEMPT 1: Save to SD Card (Preferred) ---
            // We prefer SD card so we have a hard copy even if WiFi is shaky.
            if (storeDataToSd && card.cardMounted) {
                // Try to save to SD with mutex protection
                DEBUG("Status change detected. Saving to SD card...");
                if (card.saveStatusLog(jsonString)) {
                    dataWasSecured = true;
                    DEBUG("[DI URGENT] Saved. Triggering immediate SD processing...");
                    // Optional: If WiFi is UP, trigger immediate processing for real-time feel
                    // This is safe because processStatusLogQueue handles its own mutex.
                    if (wifiStatus) {
                         card.processStatusLogQueue();
                         DEBUG("[DI URGENT] Immediate send complete.");
                    }
                } else {
                    DEBUG("[DI WARN] SD Card save FAILED (Mutex busy or card error). attempting fallback...");
                }
            }

            // --- 3. ATTEMPT 2: Live Send Fallback ---
            // IF data wasn't secured yet (because SD is disabled OR SD failed/was busy), 
            // AND we have WiFi, try sending directly to server right now.
            if (!dataWasSecured && wifiStatus) {
                DEBUG("[DI URGENT] Attempting LIVE send fallback...");
                
                // Use the new Content-Type for JSON
                String response = sendHttpRequest(getLedStartStopApiURL(), jsonString, "POST", "application/json", "ActivityTrackerData_FALLBACK");
                
                DynamicJsonDocument responseDoc(256);
                DeserializationError error = deserializeJson(responseDoc, response);
                
                if (error == DeserializationError::Ok && (responseDoc["status"] == 200 || responseDoc["status"] == 201)) {
                    DEBUG("[DI] Live Activity Data sent successfully..");
                    dataWasSecured = true;
                } else {
                    DEBUG("[DI ERROR] LIVE fallback send FAILED.");
                }
            }

            // --- 4. Final Decision (The "Zero Data Loss" Logic) ---
            if (dataWasSecured) {
                // Success! We can safely reset the flag.
                status->stateChanged = false;
            } else {
                // CRITICAL: Both SD and Live send failed (or no WiFi and no SD).
                // DO NOT reset status->stateChanged! 
                // By leaving it 'true', the main loop will call this function again 
                // in 5 seconds to retry automatically.
                DEBUG("[DI CRITICAL] Could not save or send status data! Will retry next cycle.");
            }
        }
    }
}

// --- 5. UNCHANGED sendAliveStatusData() ---
// The detailed logging for pulses is done in DigitalInput.cpp. This function's role
// is simply to be called by the heartbeat. It does not need modification.
void Devsbot::sendAliveStatusData() {
    int wifiStrength = WiFi.RSSI();
    for (int i = 0; i < digitalInput.getPulseInputCount(); i++) {
        PulseInputData* pulse = digitalInput.getPulseInput(i);
        if (pulse) {
            JsonDocument jsonDoc;
            jsonDoc["device_auth_token"] = devsbotAuthToken;
            jsonDoc["status_Id"] = 1;
            jsonDoc["wifi"] = wifiStrength;
            jsonDoc["pulse"] = pulse->pulseCount;
            String jsonString;
            serializeJson(jsonDoc, jsonString);
            DEBUG("Sending Alive Status Data: " + jsonString);

            String response = sendHttpRequest(aliveStatusURL, jsonString,"POST","application/x-www-form-urlencoded","AliveStatusData");
            DEBUG("Alive Status Response: " + response);
            
            digitalInput.processAliveStatusResponse(response);
            pulse->lastSentCount = pulse->pulseCount;
        }
    }
}

// /**
//  * @brief Sends a BATCH of historical status data from the SD card.
//  * @param payload A string containing a JSON array of status log objects.
//  * @return True if the server accepted the data, false otherwise.
//  */
// // --- Batch DI Status Sending ---
// bool Devsbot::sendDIStatusData(String& payload) {
//     DEBUG("Sending BATCH of DI Status data from SD card.");
//     String finalPayload = "device_auth_token=" + devsbotAuthToken + "&status_logs=" + payload;
//     // *** USE DYNAMIC URL ***
//     String response = sendHttpRequest(getLedStartStopApiURL(), finalPayload, "POST", "application/x-www-form-urlencoded", "sendDIStatusData_Batch");

//     JsonDocument responseDoc;
//     // Check for successful response (status 200 or 201)
//     DeserializationError error = deserializeJson(responseDoc, response);
//     if (error == DeserializationError::Ok && (responseDoc["status"] == 200 || responseDoc["status"] == 201)) {
//         DEBUG("Batch DI Status data sent successfully.");
//         return true;
//     } else {
//         DEBUG("Failed to send batch DI Status data. Server response: " + response);
//         if(error) DEBUG("  - JSON Parse Error: " + String(error.c_str()));
//         return false;
//     }
// }

// /**
//  * @brief Sends a BATCH of historical pulse data from the SD card.
//  * @param payload A string containing a JSON array of pulse log objects.
//  * @return True if the server accepted the data, false otherwise.
//  */
// bool Devsbot::sendDIPulseData(String& payload) {
//     DEBUG("Sending BATCH of DI Pulse data from SD card.");
//     String finalPayload = "device_auth_token=" + devsbotAuthToken + "&pulse_logs=" + payload;
//     // *** USE DYNAMIC URL ***
//     String response = sendHttpRequest(getDevsbotDeviceStatusURL(), finalPayload, "POST", "application/x-www-form-urlencoded", "sendDIPulseData_Batch");

//     JsonDocument responseDoc;
//     // Check for successful response (status 200 or 201)
//      DeserializationError error = deserializeJson(responseDoc, response);
//     if (error == DeserializationError::Ok && (responseDoc["status"] == 200 || responseDoc["status"] == 201)) {
//         DEBUG("Batch DI Pulse data sent successfully.");
//         return true;
//     } else {
//         DEBUG("Failed to send batch DI Pulse data. Server response: " + response);
//          if(error) DEBUG("  - JSON Parse Error: " + String(error.c_str()));
//         return false;
//     }
// }



/**
 * @brief Sends a BATCH of historical status data from the SD card.
 * @param payload A string containing the FULL JSON payload from LogManager.
 * @return True if the server accepted the data, false otherwise.
 */
bool Devsbot::sendDIStatusData(String& payload) {
    DEBUG("Sending BATCH of DI Status data from SD card.");
    
    // --- MODIFICATION ---
    // The payload is now the complete JSON object, not just a URL parameter.
    // We also change the Content-Type to application/json.
    String response = sendHttpRequest(
        getLedStartStopApiURL(),    // Correct URL
        payload,                    // The full JSON payload
        "POST",                     // Method
        "application/json",         // New Content-Type
        "sendDIStatusData_Batch"    // Caller Name
    );
    // --- END MODIFICATION ---

    JsonDocument responseDoc;
    DeserializationError error = deserializeJson(responseDoc, response);
    if (error == DeserializationError::Ok && (responseDoc["status"] == 200 || responseDoc["status"] == 201)) {
        DEBUG("Batch DI Status data sent successfully.");
        return true;
    } else {
        DEBUG("Failed to send batch DI Status data. Server response: " + response);
        if(error) DEBUG("  - JSON Parse Error: " + String(error.c_str()));
        return false;
    }
}

/**
 * @brief Sends a BATCH of historical pulse data from the SD card.
 * @param payload A string containing the FULL JSON payload from LogManager.
 * @return True if the server accepted the data, false otherwise.
 */
bool Devsbot::sendDIPulseData(String& payload) {
    DEBUG("Pulse data SD storage is disabled. Ignoring batch send.");
    return true; // Return true so nothing thinks it failed
}


void Devsbot::firmwareUpdate()
{
    // Use dynamic URL instead of hardcoded devsbotOTAUpdateURL
    String devsbotOTA = getDevsbotOTAUpdateURL() + "?cluster_api_Id=" + devsbotAuthToken + "&version=" + String(devsbotFirmwareVersion, 1);
    // DEBUG("\nOTA Request=> " + devsbotOTA + "\n"); // DEBUG is handled inside sendHttpRequest

    String firmwareResponse = sendHttpRequest(devsbotOTA, "", "OTA", "application/x-www-form-urlencoded", "firmwareUpdate");

    // DEBUG("firmwareResponse: ");Serial.println(firmwareResponse); // DEBUG handled inside sendHttpRequest

    // --- THIS IS THE FIX ---
    // Check for the actual success string returned by sendHttpRequest
    if (firmwareResponse == "OTA_SUCCESS")
    {
        DEBUG("[OTA] Update successful. Sending new version to server...");
        firmwareVersionSend(); // Proceed to notify the server
    }
    else
    {
        // Log the specific error returned by sendHttpRequest
        DEBUG("[OTA] Update FAILED. Reason: " + firmwareResponse);
    }
}




void Devsbot::firmwareVersionSend()
{
  String firmwareVersion = "device_auth_token=" + String(devsbotAuthToken) + "&version=" + String(devsbotFirmwareVersion, 1);
  byte firmversionCnt=0;
  DEBUG("firmwareVersionSend Api post requested to Server\n");
  // Use dynamic URL instead of hardcoded firmwareVersionURL
  String firmverSendResponse = sendHttpRequest(getFirmwareVersionURL(), firmwareVersion, "POST","application/x-www-form-urlencoded", "firmwareVersionSend");
  
  DEBUG("firmwareVersionSend : ");DEBUG(firmverSendResponse);
  
  if(firmverSendResponse!="")
  {
        // --- THREAD-SAFE FIX ---
    DynamicJsonDocument doc(256);
    // --- END OF FIX ---

    DeserializationError error = deserializeJson(doc, firmverSendResponse);
    if (error)
    {
      DEBUG(F("deserializeJson() failed: firmverSendResponse"));
      DEBUG(error.f_str());
      return;
    }

    int statusJson=doc["status"].as<int>();
    DEBUG("status key value in json response : ");DEBUG(statusJson);
    if(statusJson==201)
    {
      preferences.begin("version", false);
      preferences.putFloat("firmVersion",devsbotFirmwareVersion);
      preferences.end();
      ESP.restart();
    }
  }
}

void Devsbot::socketIOConnection()
{
  //DEBUG("socketIOConnection begin");
  DEBUG("Devsbot connection initializing\n");
  // --- THIS IS THE FIX ---
  // Add a detailed log to show exactly where the device is trying to connect.
  Serial.printf("[Socket.IO] Attempting to connect to host: %s, port: %u\n", dynamicHostname.c_str(), dynamicPort);
  // --- END OF FIX ---
  // Use the dynamic configuration variables
  socketIO.beginSSL(dynamicHostname.c_str(), dynamicPort, "/socket.io/?EIO=4");
  // socketIO.begin(dynamicHostname.c_str(), dynamicPort, "/socket.io/?EIO=4");
  // Test A: EIO=4 (Modern)
  //socketIO.begin(dynamicHostname.c_str(), dynamicPort, "/socket.io/?transport=websocket&EIO=4");
  //socketIO.begin("68.183.85.221",80,"/niraltek/socket.io/?EIO=4");
  //socketIO.beginSSL("energy.devsbot.com",443,"/socket.io/?EIO=4");
  //socketIO.beginSSL("sem-demo.devsbot.com",443,"/niraltek/socket.io/?EIO=4");
  //socketIO.beginSSL("sem-demo.devsbot.com",443,"/socket.io/?EIO=4");
  socketIO.onEvent([this](socketIOmessageType_t type, uint8_t * payload, size_t length) {
    // This lambda now safely calls our real handler function.
    this->socketIOEventHandler(type, payload, length);
  });
  //DEBUG("socketIOConnection End");
}



// The old static function has been renamed to a non-static member function.
void Devsbot::socketIOEventHandler(socketIOmessageType_t type, uint8_t * payload, size_t length)
{
    //Serial.println("socketIOEvent begin");

    // if (payload == nullptr || length == 0) {
    //     DEBUG("Received Socket.IO event type " + String(type) + " with empty payload. Ignoring.");
    //     DEBUG("socketIOEvent End");
    //     return;
    // }

    //Serial.println("Devsbot connection status=> "+ String(type)+"\n");
    static String dbOutputPin;

    switch (type)
    {
        case sIOtype_DISCONNECT:
            sIOConnectionStatus = type;
            //Serial.println("Device disconnected with Devsbot API\n");
            // Now we safely use 'this->' instead of the global 'dBot'
            //dBot.deviceLog("Device disconnected with Devsbot API\n");
            this->sioDisconnect = 1;
            break;

        case sIOtype_CONNECT:
            sIOConnectionStatus = type;
            Serial.println("Device connected with Devsbot API\n");
            //dBot.deviceLog("Device connected with Devsbot API\n");
            // --- THIS IS THE FIX ---
            // 'socketIO' is a global variable, not a member of the Devsbot class.
            // Access it directly without 'this->'.
            socketIO.send(sIOtype_CONNECT, "/");
            // --- END OF FIX ---

            // We can safely call other member functions using 'this->'
            this->devsbotAuthentication();
            this->sioDisconnectFlag = 1;
            this->sioDisconnect = 0;
            break;

        case sIOtype_EVENT:
            {
                String payloadStr = String((const char*)payload);
                int jsonStart = payloadStr.indexOf('{');
                if (jsonStart == -1) jsonStart = payloadStr.indexOf('[');
                if (jsonStart == -1) {
                    DEBUG("--> ERROR: No valid JSON found in payload.");
                    return;
                }
                String jsonContent = payloadStr.substring(jsonStart);
                DynamicJsonDocument doc(1024);
                DeserializationError error = deserializeJson(doc, jsonContent);

                if (error) {
                    DEBUG("deserializeJson() failed: " + String(error.c_str()));
                    return;
                }
                
                String dbMethod = doc["Method"];
                if (dbMethod == "Restart") {
                    DEBUG("Device restart\n");
                    this->deviceLog("Device restart\n");
                    ESP.restart();
                }
                // ... other event handling ...
            }
            break;
    }
    //DEBUG("socketIOEvent End");
}
// --- END OF FIX ---



bool Devsbot::checkServerConnection() 
{
  uint64_t checkServerStart = millis();

  // Create the WiFiClient object dynamically to ensure control over memory usage.
  WiFiClient* clientServer = new WiFiClient();
  if (!clientServer) 
  {
    // Handle memory allocation failure
    return 0;
  }

  // Use the dynamic configuration variables
  while (!clientServer->connect(dynamicHostname.c_str(), dynamicPort)) 
  {
    if (millis() - checkServerStart > 10000) 
    { 
        // 10 seconds timeout
        clientServer->stop();
        delete clientServer; // Free heap memory
        return 0;
    }
    delay(100); // Small delay to prevent overwhelming the server with connection attempts
  }

  clientServer->stop(); // Close the connection if it was successful
  delete clientServer; // Free heap memory
  return 1;
}



// void Devsbot::Loop() {
//     //DEBUG("devsbotLoop begin");
//     currentmillis = millis();

//         // === DIGITAL INPUT PROCESSING ===
//     digitalInput.processLoop();



//         // === SEND ACTIVITY TRACKER DATA (5 seconds interval) ===
//     if (currentmillis - lastActivitySentMillis >= activitySendInterval) {
//         if (digitalInput.isJobEnabled()) {
//             sendActivityTrackerData(); // Send only changed status pins
//         }
//         lastActivitySentMillis = currentmillis;
//     }

//     // // === SEND ALIVE STATUS DATA (20 seconds interval) ===
//     // if (currentmillis - lastDataSentMillis >= dataSendInterval) {
//     //     DEBUG("Sending alive status data");
//     //     sendAliveStatusData(); // Send pulse counts
//     //     lastDataSentMillis = currentmillis;
//     // }

//     // Skip server operations if in local mode only
//     if (localModeOnly) {
//         DEBUG("[Devsbot] Running in local mode - skipping server operations");
//         // Handle only local operations here
//         // You can add local device management code here
//         delay(5000); // Prevent excessive logging
//         return;
//     }


//     if (!card.isBusy()) {
//         // === NEW: PROCESS STORED SD CARD DATA ===
//         if (wifiStatus) {
//             card.processLogQueues();
//         }
        
//         // === SEND LIVE ACTIVITY TRACKER DATA ===
//         if (currentmillis - lastActivitySentMillis >= activitySendInterval) {
//             if (digitalInput.isJobEnabled()) {
//                 sendActivityTrackerData();
//             }
//             lastActivitySentMillis = currentmillis;
//         }

//         // === HANDLE SOCKET AND HEARTBEAT ===
//         if (wifiStatus && serverConfigLoaded) {
//             devsbotStatus();
//             socketIO.loop();
//             // // ... (rest of your socket logic) ...
//             // while(sIOConnectionStatus!=48) // do a safty block of timeout if channel not connected perform timeout block
//             // {
//             //   socketIO.loop();
//             //   delay(100);
//             //   if (millis() - currentmillis >= (20*1000)) 
//             //     break;
          
//             //   if(sIOConnectionStatus==48)
//             //     Serial.printf("socketio is connected");
//             // }
//             // devsbotAuthentication();
//         }
//     }
//     else if (wifiStatus == 0 && SPIFFS.exists("/network_data.json") && 
//                millis() - previousmillis1 >= (1000 * 60)) {
//         DEBUG("***trying to connect with WIFI***\n");
//         uint64_t premillis1 = millis();
        
//         while (wifiStatus != 1) {
//             wifiConnectionChecking(cnfSsid, cnfPass);
//             if (storeDataToSd)
//                 break;
//         }
//         vTaskDelay(pdMS_TO_TICKS(1000));
//         previousmillis1 = currentmillis + (millis() - premillis1);
//     }
// }


void Devsbot::Loop() {
    currentmillis = millis();

    // === 1. CORE DEVICE LOGIC (Always Runs) ===
    // This logic runs on every loop iteration, regardless of network status.
    digitalInput.processLoop();

    // Note: The sendActivityTrackerData() function was moved from the original code.
    // It's better to check for sending data regardless of SD card status.
    // The function itself will handle saving to SD if the live send fails.
    if (currentmillis - lastActivitySentMillis >= activitySendInterval) {
        if (digitalInput.isJobEnabled()) {
            sendActivityTrackerData(); // Send only changed status pins
        }
        lastActivitySentMillis = currentmillis;
    }

    // === 2. LOCAL MODE CHECK (Early Return) ===
    // If no server configuration was loaded, we operate in a limited local mode.
    if (localModeOnly) {
        // This debug message and delay prevent spamming the serial monitor in local mode.
        // Note: The delay() call is blocking and will pause all other operations.
        DEBUG("[Devsbot] Running in local mode - skipping server operations");
        delay(5000);
        return;
    }

    // === 3. WIFI STATE MANAGEMENT (Non-Blocking) ===
    // This block continuously manages the WiFi connection state.
    if (wifiStatus == 0) {
        // --- STA is DISCONNECTED ---

        // a) AP Re-enable Logic: If the AP is off and STA has been disconnected for 5 minutes, turn the AP back on for configuration.
        if (!_apEnabled && (millis() - _lastStaDisconnectTime > AP_REENABLE_TIMEOUT_MS)) {
            DEBUG("[WiFi] STA disconnected for 5 mins. Re-enabling AP for configuration.");
            startAP(); // This also sets the mode to WIFI_AP_STA
        }

        // b) STA Retry Logic: If we have credentials, periodically try to reconnect in the background.
        if (!cnfSsid.isEmpty() && (millis() - _lastStaRetryTime > STA_RETRY_INTERVAL_MS)) {
            nonBlockingConnectSTA(); // This is a non-blocking connection attempt
        }

    } else {
        // --- STA is CONNECTED ---

        // c) AP Timeout Logic: If the AP is still enabled and 10 minutes have passed since boot, disable it to save power and reduce interference.
        if (_apEnabled && (millis() - _bootTime > AP_TIMEOUT_MS)) {
            DEBUG("[WiFi] STA connected and AP has timed out (10 mins). Disabling AP.");
            stopAP(); // This switches the mode to WIFI_STA only
        }
    }

    // === 4. ONLINE OPERATIONS ===
    // These tasks are only performed when the device is online and not busy with critical SD card operations.
    // if (!card.isBusy()) {
        
    //     // a) Process Stored Data: If connected to WiFi, process any data queues stored on the SD card.
    //     if (wifiStatus) {
    //         card.processLogQueues();
    //     }

    //     // b) Handle Socket.IO and Heartbeat: Maintain the live connection and send heartbeat data to the server.
    //     if (wifiStatus && serverConfigLoaded) {
    //         devsbotStatus(); // Sends heartbeat and checks for commands
    //         socketIO.loop(); // Manages the WebSocket connection
    //     }
    // }

        // === 4. ONLINE OPERATIONS ===
    if (!card.isBusy()) {
        if (wifiStatus) {
            //card.processLogQueues();
        }
        // Heartbeat and Socket.IO only run if initialized, connected, config loaded, and not doing setup
        if (_isInitialized && wifiStatus && serverConfigLoaded && !_needsPostConnectionSetup) { // <-- ADDED _isInitialized CHECK
            devsbotStatus(); // Send heartbeat
            socketIO.loop(); // Handle live connection
        } else if (wifiStatus && serverConfigLoaded && !_needsPostConnectionSetup) {
             // Optional: Log why heartbeat is skipped if needed for debugging
             DEBUG("[Loop] Skipping Heartbeat/SocketIO: Not initialized yet.");
        }
    }
}

// void Devsbot::Loop() {
//     currentmillis = millis();

//     // === 1. CORE DEVICE LOGIC (Always Runs) ===
//     // This logic runs on every loop iteration, regardless of network status.
//     digitalInput.processLoop();

//     // --- sendActivityTrackerData() call is REMOVED from here ---
//     // It is now in its own task (activityTask) on Core 0.

//     // === 2. LOCAL MODE CHECK (Early Return) ===
//     // If no server configuration was loaded, we operate in a limited local mode.
//     if (localModeOnly) {
//         // This debug message and delay prevent spamming the serial monitor in local mode.
//         // vTaskDelay is better than delay()
//         vTaskDelay(pdMS_TO_TICKS(5000));
//         return;
//     }

//     // === 3. WIFI STATE MANAGEMENT (Non-Blocking) ===
//     // This block continuously manages the WiFi connection state.
//     if (wifiStatus == 0) {
//         // --- STA is DISCONNECTED ---

//         // a) AP Re-enable Logic
//         if (!_apEnabled && (millis() - _lastStaDisconnectTime > AP_REENABLE_TIMEOUT_MS)) {
//             DEBUG("[WiFi] STA disconnected for 5 mins. Re-enabling AP for configuration.");
//             startAP(); // This also sets the mode to WIFI_AP_STA
//         }

//         // b) STA Retry Logic
//         if (!cnfSsid.isEmpty() && (millis() - _lastStaRetryTime > STA_RETRY_INTERVAL_MS)) {
//             nonBlockingConnectSTA(); // This is a non-blocking connection attempt
//         }

//     } else {
//         // --- STA is CONNECTED ---

//         // c) AP Timeout Logic
//         if (_apEnabled && (millis() - _bootTime > AP_TIMEOUT_MS)) {
//             DEBUG("[WiFi] STA connected and AP has timed out (10 mins). Disabling AP.");
//             stopAP(); // This switches the mode to WIFI_STA only
//         }
//     }

//     // === 4. ONLINE OPERATIONS ===
//     // These tasks are only performed when the device is online.
//     // The main loop is now only responsible for the Socket.IO loop.
    
//     // --- card.isBusy() check is REMOVED ---
//     // --- card.processLogQueues() call is REMOVED ---
//     // (This is now handled *only* by sdProcessingTask on Core 0)

//     // Heartbeat and Socket.IO only run if initialized, connected, config loaded
//     if (_isInitialized && wifiStatus && serverConfigLoaded && !_needsPostConnectionSetup) {
        
//         // --- devsbotStatus() call is REMOVED from here ---
//         // (It is now in its own task, heartbeatTask, on Core 0)
        
//         socketIO.loop(); // Handle live connection
    
//     } else if (wifiStatus && serverConfigLoaded && !_needsPostConnectionSetup) {
//          // Optional: Log why heartbeat is skipped if needed for debugging
//          // DEBUG("[Loop] Skipping SocketIO: Not initialized yet.");
//     }
// }

/*!
 *    @brief Device verification to establish connection and initiate communication between device and devsbot app
*/

void Devsbot::devsbotAuthentication() {
    // Skip if in local mode only
    // if (localModeOnly || !serverConfigLoaded) {
    //     DEBUG("[Devsbot] Skipping authentication - local mode only");
    //     return;
    // }
    
    // Create the payload
    DynamicJsonDocument doc(256);
    JsonArray array = doc.to<JsonArray>();
    array.add("joinchannel");

    JsonObject param1 = array.createNestedObject();
    param1["clusterid"] = devsbotAuthToken;
    String dbInitalAuthentication;
    serializeJson(doc, dbInitalAuthentication);

    Serial.println("Device authentication data=> "+ dbInitalAuthentication+ "\n");
    //bool tempConnectivity = checkServerConnection(); // This call is blocking/slow!

    // --- CRITICAL FIX 2: Ensure authentication is sent, ignoring the slower checkServerConnection ---
    // The previous implementation was: call checkServerConnection (HTTP check), THEN attempt sendEVENT.
    // The delay from checkServerConnection is what kills the Socket.IO connection.
    // We remove the dependence on the HTTP check here, as sIOtype_CONNECT implies network is good enough.
    
    if (socketIO.sendEVENT(dbInitalAuthentication)) {
        Serial.println("dbInitalAuthentication was success");
    } else {
        // The log shows this failed because of the immediate disconnect, not a network error.
        Serial.println("dbInitalAuthentication was failed (Socket closed too fast)");
    }
    
    Serial.println("devsbotAuthentication End\n");
}

// Add this method to safely check if server operations should proceed
bool Devsbot::canPerformServerOperations() {
    return serverConfigLoaded && !localModeOnly && wifiStatus;
}




/*!
 *    @brief The device will send the live status to the devsbot app and get the widget version from the devsbot app server
*/

// void Devsbot::devsbotStatus() 
// {
//     // This function no longer needs its own 'statusCnt' or 'doc' variables.
//     uint64_t devsbotStatus_now = millis();

//     if (!canPerformServerOperations()) {
//         // DEBUG("[Devsbot] Skipping status update - server operations disabled");
//         return;
//     }

//     if ((devsbotStatus_now - devsbotStatus_Time > (heartBeatInterval * 1000)) || (aliveState == true)) {
//         aliveState = false;
//         int8_t wifiSignal = WiFi.RSSI();
        
//         // --- *** REQUIREMENT 2: Log current pulse count to SD *** ---
//         digitalInput.logCurrentPulseCountToSD();
//         // --- *** END OF CHANGE *** ---

//         // 1. Build the request string, just as before.
//         String devsbotAliveData = "device_auth_token=" + devsbotAuthToken + "&status_Id=1" + "&wifi=" + String(wifiSignal);
//         if (digitalInput.getPulseInputCount() > 0) {
//             PulseInputData* pulse = digitalInput.getPulseInput(0);
//             if (pulse) {
//                 devsbotAliveData += "&pulse=" + String(pulse->pulseCount);
//             }
//         }

//         devsbotStatus_Time = devsbotStatus_now;
        
//         // --- THIS IS THE CRITICAL CHANGE ---
//         // 2. Make the real HTTP request to the server. The hardcoded line is removed.
//         String statusResponse = sendHttpRequest(getDevsbotDeviceStatusURL(), devsbotAliveData, "POST","application/x-www-form-urlencoded", "devsbotStatus_Heartbeat");
        
//         // 3. Add robust error handling. If the request failed or the response is not valid JSON, stop here.
//         if (statusResponse.isEmpty() || !statusResponse.startsWith("{")) {
//             DEBUG("Heartbeat failed or received invalid response. Will retry on next interval.");
//             // If the response is an error code like "-11" or "WIFI_DISCONNECTED", it will be logged by sendHttpRequest.
//             return; 
//         }
//         // --- END OF CHANGE ---

//        // --- THREAD-SAFE FIX ---
//         DynamicJsonDocument doc(512);
//         // --- END OF FIX ---

//         DeserializationError error = deserializeJson(doc, statusResponse);
//         if (error) {
//             DEBUG("Failed to parse JSON from heartbeat response: " + statusResponse);
//             return;
//         }

//         int statusJson = doc["status"].as<int>();
//         if (statusJson == 201) 
//         {
//             // The rest of your proven logic remains exactly the same.
//             devsbotFirmwareVersion = doc["device_version"].as<float>();
//             SendDataToServer = doc["active"];
            
//             JsonArray jsonConfigArray = doc["config"];
//             for (int i = 0; i < jsonConfigArray.size(); i++) {
//                 configArray[i] = jsonConfigArray[i];
//             }

//             String newJobStr = doc["job_status"].as<String>();
//             bool newJobState = (newJobStr == "1");
//             bool currentJobState = digitalInput.isJobEnabled();

//             if (newJobState != currentJobState) {
//                 DEBUG("Job status has changed. Processing response...");
//                 digitalInput.processAliveStatusResponse(statusResponse);
//             }

//             if (digitalInput.getPulseInputCount() > 0) {
//                 PulseInputData* pulse = digitalInput.getPulseInput(0);
//                 if (pulse) {
//                     pulse->lastSentCount = pulse->pulseCount;
//                 }
//             }
            
//             widgetUpdate();
//         } else {
//             // Log if the server returns a status other than 201
//             DEBUG("Heartbeat response received, but status was not 201.");
//             deviceLog("Heartbeat response status: " + String(statusJson));
//         }
//     }
// }

// --- Replace the entire devsbotStatus() function with this new version ---
void Devsbot::devsbotStatus() 
{
    uint64_t devsbotStatus_now = millis();

    if (!canPerformServerOperations()) {
        return;
    }

    if ((devsbotStatus_now - devsbotStatus_Time > (heartBeatInterval * 1000)) || (aliveState == true)) {
        aliveState = false;
        int8_t wifiSignal = WiFi.RSSI();
        
        // --- START MODIFICATION ---

        // 1. Build the request string, getting pulse data first
        String devsbotAliveData = "device_auth_token=" + devsbotAuthToken + "&status_Id=1" + "&wifi=" + String(wifiSignal);
        
        int currentPin = 0;
        int currentPulseCount = 0;
        
        if (digitalInput.getPulseInputCount() > 0) {
            PulseInputData* pulse = digitalInput.getPulseInput(0);
            if (pulse) {
                currentPin = pulse->pin; // Get the pin for logging on failure
                currentPulseCount = pulse->pulseCount; // Get the count
                devsbotAliveData += "&pulse=" + String(currentPulseCount);
            }
        }

        devsbotStatus_Time = devsbotStatus_now;
        
        // 2. Attempt to send the data
        String statusResponse = sendHttpRequest(getDevsbotDeviceStatusURL(), devsbotAliveData, "POST","application/x-www-form-urlencoded", "devsbotStatus_Heartbeat");
        
        // 3. Check for all failure conditions
        bool sendFailed = false;

        if (statusResponse.isEmpty() || !statusResponse.startsWith("{")) {
            DEBUG("Heartbeat failed (no response). Logging pulse data offline.");
            sendFailed = true;
        }

        DynamicJsonDocument doc(512);
        DeserializationError error = deserializeJson(doc, statusResponse);
        
        if (error) {
            DEBUG("Failed to parse JSON from heartbeat response. Logging pulse data offline.");
            sendFailed = true;
        }

        int statusJson = doc["status"].as<int>();
        if (statusJson != 201) 
        {
            DEBUG("Heartbeat response was not 201. Logging pulse data offline.");
            //deviceLog("Heartbeat response status: " + String(statusJson));
            sendFailed = true;
        }

        // 4. Act on the failure or success
        if (sendFailed) {
            // --- FAILURE PATH ---
            // Call the new overwrite function
            if (dBot.storeDataToSd) {
                digitalInput.logOfflinePulseCount(currentPin, currentPulseCount);
            }
        } else {
            // --- SUCCESS PATH ---
            DEBUG("Heartbeat send SUCCESS.");
            
            // --- LOGIC 3 (Smart Delete) ---
            // We successfully sent the latest pulse count, so delete any old offline log.
            digitalInput.clearOfflinePulseLog();

            // The send was successful, so we update the job status, etc.
            devsbotFirmwareVersion = doc["device_version"].as<float>();
            SendDataToServer = doc["active"];
            
            JsonArray jsonConfigArray = doc["config"];
            for (int i = 0; i < jsonConfigArray.size(); i++) {
                configArray[i] = jsonConfigArray[i];
            }

            // Update Job Status
            String newJobStr = doc["job_status"].as<String>();
            bool newJobState = (newJobStr == "1");
            int newJobId = doc["job_id"] | 0; // Get job_id from success response
            bool currentJobState = digitalInput.isJobEnabled();

            // if (newJobState != currentJobState || newJobId != digitalInput.isJobEnabled()) {
            //     DEBUG("Job status or ID has changed. Processing response...");
            //     digitalInput.processAliveStatusResponse(statusResponse);
            // }
            if (newJobState != currentJobState || newJobId != digitalInput.getCurrentJobId()) {
            // --- END MODIFICATION ---

                DEBUG("Job status or ID has changed. Processing response...");
                digitalInput.processAliveStatusResponse(statusResponse);
            }


            // Update last sent count *only on success*
            if (digitalInput.getPulseInputCount() > 0) {
                PulseInputData* pulse = digitalInput.getPulseInput(0);
                if (pulse) {
                    pulse->lastSentCount = pulse->pulseCount;
                }
            }
            
            widgetUpdate();
        }
        // *** NEW: Trigger SD Processing after Heartbeat finishes ***
        // This ensures it runs only when the main loop is otherwise idle.
        // We use a separate timer tracker to avoid running it every single minute.
        static unsigned long lastSDProcessTime = 0;
        if (dBot.storeDataToSd && (millis() - lastSDProcessTime > (180 * 1000))) { // Run every ~3 minutes
             if (sdCardTaskHandle != NULL) {
                 DEBUG("[Heartbeat] Triggering background SD processing task...");
                 xTaskNotifyGive(sdCardTaskHandle);
                 lastSDProcessTime = millis();
             }
        }
    }
}

void Devsbot::widgetUpdate()
{
 // DEBUG("widgetUpdate begin");
  if(devsbotFirmwareVersion!=deviceFirmwareVersion)
  {
    // vTaskSuspend(TaskHandle_PostData_Start_S1);
    DEBUG("devicefirmware: " + String(deviceFirmwareVersion) + "devsbotfirmware: " + String(devsbotFirmwareVersion) + "version is not same" );
    DEBUG("devicefirmware and devsbotfirmware version is not same");
    deviceFirmwareUpdate();
    // vTaskResume(TaskHandle_PostData_Start_S1);

  }

  for(uint8_t i=0 ;i<(sizeof(configArray)/sizeof(configArray[0]));i++)
  {
    if(configArray[i]==1)
    {
      switch(i)
      {
        case 0 : 
          DEBUG("widget flag got changed from 0->1");
          if(widgetAPI())
          {
            widgetPinInitialize();
            (acknowldgeApi(i))?configArray[i]=0:configArray[i]=1;//make a ack to change the bit in the server 
          }
          else
            configArray[i]=1;

          break;

        case 1:
          DEBUG("meterAddressInit flag got changed from 0->1");
          if(meterAddressInit())
          {
            meterAddressData();
            (acknowldgeApi(i))?configArray[i]=0:configArray[i]=1;//make a ack to change the bit in the server 
            // acknowldgeApi(i);//make a ack to change the bit in the server 
            // configArray[i]=0;
          }
          else
            configArray[i]=1;
          break;


        case 2:
          DEBUG("deviceProvision flag got changed from 0->1");
          devicePreProvisionData();
          if(deviceProvision())//make a device provision request on set of a 3rd bit 
          {
            deviceProvisionData();//update a global variable
            (acknowldgeApi(i))?configArray[i]=0:configArray[i]=1;//make a ack to change the bit in the server 
            //acknowldgeApi(i);//make a ack to change the bit in the server 
            // configArray[i]=0;
              /** check for reconnecting to network needed */
             /** store a ssid ,pass ,connectivity to a common file */
              if((devsbotSsid != "") && ((deviceconnectivity != devsbotconnectivity) || (preSsid != devsbotSsid) || (prePassword != devsbotPassword)))
              {
                DEBUG("wifi ssid password got chnaged form a webzone");
                saveNetworkData(devsbotSsid,devsbotPassword,String(devsbotconnectivity));
              }
          }
          else
            configArray[i]=1;
          break;
      }
    }
  }
 // DEBUG("widgetUpdate End\n");
}

bool Devsbot::acknowldgeApi(uint8_t value)
{
  String ackAPI = "gateway_api_Id=" + devsbotAuthToken + "&config_type=" + String(value);
  
  DEBUG("acknowldgeApi post requested to Server\n");

  // Use dynamic URL instead of hardcoded reverseURL
    String ackResponse = sendHttpRequest(getReverseURL(), ackAPI, "POST","application/x-www-form-urlencoded", "acknowldgeApi");
  DEBUG("ackResponse : ");DEBUG(ackResponse);
  
  if(ackResponse!="" && ackResponse.startsWith("{"))
    return 1;
  else
    return 0;
}

// bool Devsbot::meterAddressInit()
// {
//   DEBUG("meter Address Init api\n");
//   byte addressCnt=0;
  
//   // Use dynamic URL instead of hardcoded meterAddressURL
//   String meterInitAPI = getMeterAddressURL() + "?gateway_api_Id=" + String(devsbotAuthToken);

//   DEBUG("meterInit Api request to server => "+ meterInitAPI + "\n");   

//   String meterAddResponse = sendHttpRequest(meterInitAPI, "", "GET","application/x-www-form-urlencoded", "meterAddressInit");
//   if(meterAddResponse!="" && meterAddResponse.startsWith("{"))
//   {
//     File file = SPIFFS.open("/meter_Address.json", "w");
//     if (!file) 
//     {
//       DEBUG("Failed to open meter_Address file for writing");
//       return 0;
//     }
//     file.print(meterAddResponse);
//     file.close();
//     return 1;
//   }
//   else
//   {
//     DEBUG("meteraddr api responsecode: " + meterAddResponse);
//     DEBUG("meteraddr api responsecode: " + meterAddResponse);
//     return 0;
//   }
// }

// *** MODIFIED meterAddressInit function ***
bool Devsbot::meterAddressInit()
{
    DEBUG("\n========================================");
    DEBUG("  START: Meter Address Init (API Fetch)  ");
    DEBUG("========================================");

    byte addressCnt=0; // Note: This variable seems unused

    // Use dynamic URL
    String meterInitAPI = getMeterAddressURL() + "?gateway_api_Id=" + String(devsbotAuthToken);

    DEBUG("API Request: "+ meterInitAPI);

    // Fetch data from server
    String meterAddResponse = sendHttpRequest(meterInitAPI, "", "GET","application/x-www-form-urlencoded", "meterAddressInit");

    // Check if the response seems valid before writing
    if(meterAddResponse!="" && meterAddResponse.startsWith("{"))
    {
        File file = SPIFFS.open("/meter_Address.json", "w");
        if (!file)
        {
            DEBUG("[ERROR] Failed to open /meter_Address.json for writing");
            DEBUG("========================================");
            return 0; // Return failure
        }
        file.print(meterAddResponse);
        file.close(); // Close the file first

        // *** ADDED DELAY ***
        // Add a small delay to allow SPIFFS write operation to complete
        delay(100);

        DEBUG("Successfully fetched and saved meter_Address.json");
        DEBUG("========================================");
        return 1; // Return success
    }
    else
    {
        // sendHttpRequest already logs the failure details (like 500 error)
        DEBUG("[ERROR] Failed to fetch meter address data or received invalid response.");
        // Log the actual invalid response if needed for debugging
        // DEBUG("Invalid Response: " + meterAddResponse);
        DEBUG("========================================");
        return 0; // Return failure
    }
}


// void Devsbot::meterAddressData()
// {
//     DEBUG("meter Address data");
//     String addressData="";
//     File file = SPIFFS.open("/meter_Address.json", "r");

//     if (!file) 
//     {
//       DEBUG("Failed to open meter_Address file for reading");
//       if(!notConnetedtoNetwork)
//         DEBUG("Failed to open meter_Address file for reading");
//       return;
//     }

//     while (file.available())
//     {
//       addressData= file.readStringUntil('\n');
//     }    
//     file.close();

//     DEBUG("address data : " + addressData);
//     // --- THREAD-SAFE FIX ---
//     DynamicJsonDocument doc(2048);
//     // --- END OF FIX ---

//     DeserializationError error = deserializeJson(doc,addressData);

//     if (error)
//     {
//       DEBUG(F("deserializeJson() failed: meterAddressData"));DEBUG(error.f_str());
//       if(!notConnetedtoNetwork)
//         DEBUG("deserializeJson() failed: meterAddressData" + String(error.f_str()));
//       return;
//     }

    

//     int statusMtrAdd = doc["status"].as<int>();
    
//     if(statusMtrAdd == 200)
//     {
//       JsonArray jsonslaveid = doc["slave_id"];

//       for (uint8_t i = 0; i < jsonslaveid.size(); i++) 
//       {
//         slaveIdArray[i] = jsonslaveid[i]; // Assuming values are within the byte range (0-255)
//         Serial.println(slaveIdArray[i]); // Printing the values to the serial monitor
//       }

//       numSlave=jsonslaveid.size();
//       Serial.print("no of slaves : ");Serial.println(numSlave);


//       JsonArray conf = doc["conf"];
//       int i;
//       byte objIndex=0;
//       for (JsonObject item : conf) 
//       {
//         Serial.print("objindex : ");Serial.println(objIndex);
//         JsonArray mtrAdd = item["address"];
//         JsonArray regLen= item["offset"];
//         JsonArray pins=item["pin"];
//         JsonArray type=item["datatype"];
//         JsonArray rybVal=item["params"];
//         JsonArray typeOfReg=item["holding_register"];
//         JsonArray endianess=item["endianness"];
//         JsonArray arrProgram=item["program"];


//         energy.baudRate=item["baudrate"].as<unsigned long>();
//         const char *tempch=item["parity"];
//         if(tempch!=NULL)
//         {
//           energy.parityStopbit=strtoul(tempch,NULL,16);
//           Serial.print("parity and stop bit : " );Serial.println(energy.parityStopbit);
//           if(objIndex==0) //need to call only once
//             energy.serialInit();
//         }

//         // mtrparam[objIndex].regType=item["holding_register"];
//         // mtrparam[objIndex].endian=item["endianness"];
//         mtrparam[objIndex].size=mtrAdd.size();
//         mtrparam[objIndex].sizevPins=pins.size();

//         Serial.print("size of vpins : ");Serial.println(pins.size());
//         Serial.print("size of rybVal : ");Serial.println(rybVal.size());
//         Serial.print("no of address : ");Serial.println(mtrAdd.size());
//         Serial.print("no of data type : ");Serial.println(type.size());

//         for(i=0;i<type.size();i++)
//         {
//           mtrparam[objIndex].dataType[i]=type[i];
//           Serial.print("data type : ");Serial.println(mtrparam[objIndex].dataType[i]);
//         }

//         for (i = 0; i < mtrAdd.size(); i++) 
//         {
//           mtrparam[objIndex].noRegToRead[i]=regLen[i];
//           mtrparam[objIndex].regAddr[i] = mtrAdd[i]; // Assuming values are within the byte range (0-255)
//           mtrparam[objIndex].regType[i] = typeOfReg[i];
//           mtrparam[objIndex].endian[i] = endianess[i];
//           mtrparam[objIndex].operation[i]=arrProgram[i];
//           //Serial.println(energy.regAddr[i]); // Printing the values to the serial monitor
//           Serial.print("regAdd : ");Serial.println( mtrparam[objIndex].regAddr[i]);
//           Serial.print("noRegToRead : ");Serial.println( mtrparam[objIndex].noRegToRead[i]);
//           Serial.print("RegType : ");Serial.println( mtrparam[objIndex].regType[i]);
//           Serial.print("endianess : ");Serial.println( mtrparam[objIndex].endian[i]);
//           Serial.print("operation : ");Serial.println( mtrparam[objIndex].operation[i]);

//         }

//         for (i = 0; i <pins.size(); i++) 
//         {
//           const char* ptrpin = pins[i];
//           mtrparam[objIndex].vPins[i]= strdup(ptrpin);
//           mtrparam[objIndex].noParam[i]  =rybVal[i];
//           Serial.print("vpins : ");Serial.println(mtrparam[objIndex].vPins[i]);
//           Serial.print("noofparams : ");Serial.println(mtrparam[objIndex].noParam[i]);
//         }
//         objIndex++;
//         Serial.print("obj index at last :");Serial.println(objIndex);
//         Serial.println();
//       }
//     }
//     else
//     {
//       if(!notConnetedtoNetwork)
//         deviceLog("meterAddrdata status key is not 200");
//     }

//     if(!notConnetedtoNetwork)
//       deviceLog("meterAddrdata config successfully status key: " + String(statusMtrAdd));

//         if(SPIFFS.exists("/meter_Address.json")) {
//         File file = SPIFFS.open("/meter_Address.json", "r");
//         String addressData = file.readString();
//         file.close();
        
//         // Parse the configuration for QC testing
//         parseMeterConfiguration();
        
//         // Initialize RS485 configuration
//         initializeRS485Config();
        
//         // ... rest of existing implementation
//     }
//     Serial.printf("meter Address data end\n");
// }


// *** MODIFIED meterAddressData function ***
void Devsbot::meterAddressData()
{
    DEBUG("\n========================================");
    DEBUG("  START: Meter Address Configuration  ");
    DEBUG("========================================");

    String addressData=""; // Initialize string
    File file = SPIFFS.open("/meter_Address.json", "r");

    // *** ADDED FILE OPEN CHECK ***
    if (!file)
    {
        DEBUG("[ERROR] Failed to open /meter_Address.json for reading");
        DEBUG("========================================");
        // Log to server if connected
        if(!notConnetedtoNetwork)
            deviceLog("Failed to open meter_Address file for reading");
        return; // Exit the function
    }

    // *** MODIFIED FILE READING METHOD ***
    // Read the entire file content instead of just until newline
    addressData = file.readString();
    file.close(); // Close the file immediately after reading

    // *** ADDED EMPTY CHECK ***
    if (addressData.isEmpty()) {
        DEBUG("[ERROR] Read empty data from /meter_Address.json!");
        DEBUG("========================================");
         if(!notConnetedtoNetwork)
            deviceLog("Read empty data from meter_Address file");
        return; // Exit the function
    }


    DEBUG("Raw JSON: " + addressData);

    DynamicJsonDocument doc(2048); // Increased size slightly just in case
    DeserializationError error = deserializeJson(doc, addressData);

    if (error)
    {
        // *** ADDED SPECIFIC ERROR LOGGING ***
        DEBUG("[ERROR] Failed to parse JSON: " + String(error.c_str()));
        DEBUG("========================================");
        if(!notConnetedtoNetwork)
            deviceLog("deserializeJson() failed in meterAddressData: " + String(error.c_str()));
        return; // Exit the function on parsing error
    }

    // --- JSON Parsing Logic (remains the same as your correct version) ---
    int statusMtrAdd = doc["status"].as<int>();
    if(statusMtrAdd != 200)
    {
        DEBUG("[WARN] Server returned status " + String(statusMtrAdd) + ", not 200.");
        // Continue processing if file exists, but log the warning.
    }

    // --- Summary Section ---
    DEBUG("\n[ Summary ]");
    JsonArray jsonslaveid = doc["slave_id"];
    numSlave = jsonslaveid.size();
    Serial.printf("Total Slaves: %d\n", numSlave);
    // Ensure not writing past array bounds
    for (uint8_t i = 0; i < numSlave && i < (sizeof(slaveIdArray)/sizeof(slaveIdArray[0])); i++)
    {
        slaveIdArray[i] = jsonslaveid[i];
        Serial.printf("  - Slave ID[%d]: %d\n", i, slaveIdArray[i]);
    }

    // --- Configuration Objects Section ---
    JsonArray conf = doc["conf"];
    int i;
    byte objIndex=0;

    for (JsonObject item : conf)
    {
        // Limit objIndex to prevent writing past mtrparam bounds
        if (objIndex >= (sizeof(mtrparam)/sizeof(mtrparam[0]))) {
            DEBUG("[ERROR] More configuration objects in JSON than mtrparam array size allows. Skipping rest.");
            break;
        }

        Serial.printf("\n---[ Parsing Config Object %d ]---\n", objIndex);

        // RS485 Config
        energy.baudRate=item["baudrate"].as<unsigned long>();
        const char *tempch=item["parity"];
        if(tempch!=NULL)
        {
            energy.parityStopbit=strtoul(tempch,NULL,16);
            Serial.printf("RS485 Config : Baud=%-6lu | Parity/Stop=0x%X\n", energy.baudRate, energy.parityStopbit);
            if(objIndex==0) // Call only once
            {
                energy.serialInit();
            }
        } else {
             DEBUG("[WARN] Parity field missing or null in config object " + String(objIndex));
        }


        // --- Get Arrays ---
        JsonArray mtrAdd    = item["address"];
        JsonArray regLen    = item["offset"];
        JsonArray pins      = item["pin"];
        JsonArray type      = item["datatype"];
        JsonArray rybVal    = item["params"];
        JsonArray typeOfReg = item["holding_register"];
        JsonArray endianess = item["endianness"];
        JsonArray arrProgram= item["program"];

        // Check if arrays exist and have sizes before accessing
        if (!mtrAdd || !regLen || !pins || !type || !rybVal || !typeOfReg || !endianess || !arrProgram) {
            DEBUG("[ERROR] One or more required arrays are missing in config object " + String(objIndex) + ". Skipping this object.");
            objIndex++; // Increment index even if skipping
            continue;
        }

        // Check for size mismatches that would cause crashes
        size_t expectedRegSize = mtrAdd.size();
        if (regLen.size() != expectedRegSize || type.size() != expectedRegSize ||
            typeOfReg.size() != expectedRegSize || endianess.size() != expectedRegSize ||
            arrProgram.size() != expectedRegSize) {
             DEBUG("[ERROR] Array size mismatch for register data in config object " + String(objIndex) + ". Skipping this object.");
             objIndex++;
             continue;
        }
        size_t expectedPinSize = pins.size();
         if (rybVal.size() != expectedPinSize) {
             DEBUG("[ERROR] Array size mismatch for pin data (pins vs params) in config object " + String(objIndex) + ". Skipping this object.");
             objIndex++;
             continue;
         }


        mtrparam[objIndex].size = expectedRegSize;
        mtrparam[objIndex].sizevPins = expectedPinSize;

        // --- Data Types Table ---
        if (mtrparam[objIndex].size > 0) {
            Serial.println("  [Data Types]");
            Serial.println("  | Idx | Value |");
            Serial.println("  |-----|-------|");
             // Limit loop to array bounds
            for(i=0; i < mtrparam[objIndex].size && i < (sizeof(mtrparam[objIndex].dataType)/sizeof(mtrparam[objIndex].dataType[0])); i++)
            {
                mtrparam[objIndex].dataType[i] = type[i];
                Serial.printf("  | %-3d | %-5d |\n", i, mtrparam[objIndex].dataType[i]);
            }
        } else {
            Serial.println("  [Data Types]: None");
        }

        // --- Register Address Table ---
        if (mtrparam[objIndex].size > 0) {
            Serial.println("\n  [Register Mappings]");
            Serial.println("  | Idx | Addr  | Len | Type | Endian | Op |");
            Serial.println("  |-----|-------|-----|------|--------|----|");
            // Limit loop to array bounds
            for (i = 0; i < mtrparam[objIndex].size && i < (sizeof(mtrparam[objIndex].regAddr)/sizeof(mtrparam[objIndex].regAddr[0])); i++)
            {
                mtrparam[objIndex].noRegToRead[i] = regLen[i];
                mtrparam[objIndex].regAddr[i]     = mtrAdd[i];
                mtrparam[objIndex].regType[i]     = typeOfReg[i];
                mtrparam[objIndex].endian[i]      = endianess[i];
                mtrparam[objIndex].operation[i]   = arrProgram[i];

                Serial.printf("  | %-3d | %-5d | %-3d | %-4d | %-6d | %-2d |\n",
                              i, mtrparam[objIndex].regAddr[i], mtrparam[objIndex].noRegToRead[i],
                              mtrparam[objIndex].regType[i], mtrparam[objIndex].endian[i],
                              mtrparam[objIndex].operation[i]);
            }
        } else {
            Serial.println("\n  [Register Mappings]: None");
        }

        // --- Virtual Pin Table ---
        if (mtrparam[objIndex].sizevPins > 0) {
            Serial.println("\n  [Virtual Pin Mappings]");
            Serial.println("  | Idx | vPin Name  | Params |");
            Serial.println("  |-----|------------|--------|");
             // Limit loop to array bounds
            for (i = 0; i < mtrparam[objIndex].sizevPins && i < (sizeof(mtrparam[objIndex].vPins)/sizeof(mtrparam[objIndex].vPins[0])); i++)
            {
                const char* ptrpin = pins[i];
                // Free previous string if re-parsing to avoid memory leak
                if (mtrparam[objIndex].vPins[i] != NULL) {
                     free(mtrparam[objIndex].vPins[i]);
                }
                mtrparam[objIndex].vPins[i] = (ptrpin != NULL) ? strdup(ptrpin) : NULL; // Handle null pins
                mtrparam[objIndex].noParam[i]  = rybVal[i];

                Serial.printf("  | %-3d | %-10s | %-6d |\n",
                              i, (mtrparam[objIndex].vPins[i] ? mtrparam[objIndex].vPins[i] : "NULL"), mtrparam[objIndex].noParam[i]);
            }
        } else {
            Serial.println("\n  [Virtual Pin Mappings]: None");
        }

        objIndex++; // Increment after successful processing
    } // End loop through conf objects

    // Log success if parsing happened without critical errors
    if(!notConnetedtoNetwork)
        //deviceLog("meterAddrdata config parsed successfully status key: " + String(statusMtrAdd));

    // --- QC Testing Section (remains the same) ---
    if(SPIFFS.exists("/meter_Address.json")) {
        // Re-open and read for QC parsing (could optimize by passing 'doc' if needed)
        File qcFile = SPIFFS.open("/meter_Address.json", "r");
        if (qcFile) {
            String qcAddressData = qcFile.readString();
            qcFile.close();
            // Parse the configuration for QC testing
            parseMeterConfiguration();
            // Initialize RS485 configuration (this will print its own QC message)
            initializeRS485Config();
        } else {
             DEBUG("[QC WARN] Could not re-open meter_Address.json for QC parsing.");
        }
    } else {
         DEBUG("[QC WARN] meter_Address.json not found for QC parsing.");
    }
    // --- End QC Section ---

    DEBUG("========================================");
    DEBUG("   END: Meter Address Configuration   ");
    DEBUG("========================================");
}

/*void Devsbot::sensorInput()
{
  //DEBUG("sensorInput begin");

  // if (deviceWidgetVersion == devsbotWidgetVersion)
  // {
    if(digitalInputPin != NULL)
    {
      DEBUG("Digital Sensor Pins=> "+ digitalInputPin+"\n");

      char dataPin[digitalInputPin.length() + 1];
      digitalInputPin.toCharArray(dataPin, digitalInputPin.length() + 1);
      const char* digitalSensorInput = strtok(dataPin, ",");

      while (digitalSensorInput != NULL)
      {
        String digitalPin= String(digitalSensorInput);
        jsonInputSend("digitalinput", digitalPin, String(digitalRead(digitalPin.toInt())));
        //DEBUG("Digital Input data=> "+ String(digitalRead(digitalPin.toInt())));
        digitalSensorInput = strtok(NULL, ",");
      }
    }

    if(digitalInputPullupPin != NULL)
    {
      DEBUG("Digital Pullup Sensor Pins=> "+ digitalInputPullupPin+"\n");

      char dataPin[digitalInputPullupPin.length() + 1];
      digitalInputPullupPin.toCharArray(dataPin, digitalInputPullupPin.length() + 1);
      const char* digitalSensorInputPullup = strtok(dataPin, ",");

      while (digitalSensorInputPullup != NULL)
      {
        String digitalPullupPin= String(digitalSensorInputPullup);
        jsonInputSend("digitalinput", digitalPullupPin, String(digitalRead(digitalPullupPin.toInt())));
        //DEBUG("Digital Input Pullup data=> "+ String(digitalRead(digitalPullupPin.toInt())));
        digitalSensorInputPullup = strtok(NULL, ",");
      }   
    }

    if(analogInputPin != NULL)
    {
      DEBUG("Analog Sensor Pins=> "+ analogInputPin+"\n");

      char dataPin[analogInputPin.length() + 1];
      analogInputPin.toCharArray(dataPin, analogInputPin.length() + 1);
      const char* analogSensorInput = strtok(dataPin, ",");

      while (analogSensorInput != NULL)
      {
        String analogPin= String(analogSensorInput);
        jsonInputSend("analoginput", analogPin, String(analogRead(analogPin.toInt())));
        //DEBUG("Analog Input data=> "+ String(analogRead(analogPin.toInt())));
        analogSensorInput = strtok(NULL, ",");
      }  
    }
  //}
  delay(100);
 // DEBUG("sensorInput End\n");
}*/


/*!
 * @brief Send custom sensor data and other required data to devsbot server for monitoring purpose
 * @param dbVirtualPin, A widget's virtual pin is required to send data to the devsbot server
 * @param dbVirtualData, Device tracking integer data is sent to the devsbot app and displayed in a widget on the dashboard
*/
/*void Devsbot::sendVirtualWrite(byte dbVirtualPin, int dbVirtualData) // only send numerical value in virtualpin and send data in virtualData.
{
  jsonInputSend("virtualinput", ("V" + String(dbVirtualPin)), String(dbVirtualData));
}*/

/*!
 * @brief Send custom sensor data and other required data to devsbot server for monitoring purpose
 * @param dbVirtualPin, A widget's virtual pin is required to send data to the devsbot server
 * @param dbVirtualData, Device tracking string data is sent to the devsbot app and displayed in a widget on the dashboard
*/
/*void Devsbot::sendVirtualWrite(byte dbVirtualPin, String dbVirtualData) // only send numerical value in virtualpin and send data in virtualData.
{
  jsonInputSend("virtualinput", ("V" + String(dbVirtualPin)), dbVirtualData);
}*/

// void Devsbot::sentEnergyMeterData(String EnergyString)
// {
//   jsonInputSend("MeterJsonData",EnergyString);  //meterjsondata is event handler 
// }

bool Devsbot::sentEnergyMeterData(String &EnergyString)
{
  httpMethodTaken=1;
  byte meterdataCount=0;
  uint32_t retVal=0;
  DEBUG("sentEnergyMeterData begin");
  String meterString = "device_auth_token=" + devsbotAuthToken + "&device_value=" + EnergyString;
  DEBUG("energymeterdata Api post requested to Server\n");

  // Use dynamic URL instead of hardcoded energymeterJsonURL
  String meterResponse = sendHttpRequest(getEnergyMeterJsonURL(), meterString, "POST","application/x-www-form-urlencoded", "sentEnergyMeterData");

  // DEBUG("meterResponse : ");DEBUG(meterResponse);
    // --- THREAD-SAFE FIX ---
    DynamicJsonDocument doc(256);
    // --- END OF FIX ---
  DeserializationError error = deserializeJson(doc, meterResponse);
  if (error)
  {
      Serial.print("Input JSON deserialization failed: ");
      Serial.println(error.c_str());
  }
  else
  {
    retVal=doc["status"].as<int>();
  }

  if(retVal==201)
    return 1;
  else
    return 0;
  DEBUG("sentEnergyMeterData End");
}


/*!
 *    @brief Here device will send information about input method, input pin number and tracking data to the devsbot server
 *    @param dbInputMethod, The input type is useful for identifying which data came from the device to the devsbot server
 *    @param dbInputPin, The input pin description is useful to match the widget pin description on the dashboard in the devsbot app
 *    @param dbDeviceValue, The tracking data will be sent to the devsbot server and will view the data on the dashboard
*/
void Devsbot::jsonInputSend(String dbInputMethod, String dbInputPin, String dbDeviceValue)
{
  String devsbotInput;

  // --- THREAD-SAFE FIX ---
  DynamicJsonDocument doc(256);
  // --- END OF FIX ---

  JsonArray array1 = doc.to<JsonArray>();

  array1.add(dbInputMethod);
  JsonObject param2 = array1.createNestedObject();

  param2["devicelink"] = devsbotAuthToken;
  param2["input_pin"] = dbInputPin;
  param2["device_value"] = dbDeviceValue;

  serializeJson(doc, devsbotInput);
  DEBUG(devsbotInput+"\n");

  socketIO.sendEVENT(devsbotInput);
}

void Devsbot::jsonInputSend(String MeterJsonData,String Energyjson )
{
  String jsonString;
  // --- THREAD-SAFE FIX ---
  DynamicJsonDocument doc(2048);
  // --- END OF FIX ---

  JsonArray array1 = doc.to<JsonArray>();

  array1.add(MeterJsonData);
  JsonObject param2 = array1.createNestedObject();

  param2["devicelink"] = devsbotAuthToken;
  param2["device_value"] = Energyjson;

  serializeJson(doc, jsonString);
  DEBUG(jsonString+"\n");

  socketIO.sendEVENT(jsonString);
  DEBUG(" Energymeter data sent successfully ");
}




/*!
 *    @brief The device logs data to the server to know what is happening with the device while it is running
 *    @param dbDeviceLogData, It contains information about the operational information of the device while the device is running
*/

void Devsbot::deviceLog(String dbDeviceLogData)
{
  String deviceLogapiResponse;
  String devsbotData = "gateway_api_Id=" + devsbotAuthToken + "&device_log=" + dbDeviceLogData;
  DEBUG("DeviceLog Api post requested to Server\n");
  
  if(httpMethodTaken == 0)
  {
    // Use dynamic URL instead of hardcoded devsbotDeviceLogURL
    String deviceLogapiResponse = sendHttpRequest(getDevsbotDeviceLogURL(), devsbotData, "POST", "application/x-www-form-urlencoded", "deviceLog");

    DEBUG("deviceLogapiResponse : ");DEBUG(deviceLogapiResponse);
  }
  else
  {
    Serial.printf("http function is already busy \n");
  }
}






// Replace the contents of your deviceContinue() function
void Devsbot::deviceContinue() {
    // --- THE FIX in action ---
    if (!card.isBusy()) {
        socketIO.loop();
        devsbotStatus();
    }
}





/*!
 *    @brief Custom devsbot delay function to maintain stable connection between devsbot server and device
*/
void Devsbot::devsbotDelay(uint64_t dbMilliSeconds)
{
  uint64_t dbPauseNow = millis();
  uint64_t dbPauseDelay = dbPauseNow + dbMilliSeconds;
  while (dbPauseNow <= dbPauseDelay)
  {
    dbPauseNow = millis();
    socketIO.loop();
    devsbotStatus();
  }
}

Devsbot dBot;