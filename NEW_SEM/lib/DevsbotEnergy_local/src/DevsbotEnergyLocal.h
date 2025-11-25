//gateway concept 
//change authtoken----> gateWayId
//remove devsbotClusterID(global variable),clusterID(function parameter)
#ifndef DEVSBOTENERGYLOCAL_H
#define DEVSBOTENERGYLOCAL_H

// /** config for GSM/GPRS Module  */
// #define TINY_GSM_MODEM_SIM7600  // SIM7600 AT instruction is compatible with A7672
// #define SerialAT Serial1
// #define TINY_GSM_RX_BUFFER   1024  // Set RX buffer to 1Kb
// #define TINY_GSM_USE_GPRS true
// #define RXD1 27    //4G MODULE RXD INTERNALLY CONNECTED, Hardware Serial 1
// #define TXD1 26    //4G MODULE TXD INTERNALLY CONNECTED, Hardware Serial 1
// #define powerPin 4 ////4G MODULE ESP32 PIN D4 CONNECTED TO POWER PIN OF A7670C CHIPSET, INTERNALLY CONNECTED


#include <Arduino.h>
#include <Preferences.h>

// WiFi includes - include WiFi first, then its dependencies
#include <WiFi.h>
#include <Update.h>
#include "SPIFFS.h"
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

// FreeRTOS includes
#include <freertos/FreeRTOS.h>
#include <freertos/timers.h>
#include <freertos/semphr.h>

// Other includes
#include <WebSocketsClient.h>
#include <SocketIOclient.h>
#include <ArduinoJson.h>
#include <readmodbusdata.h>
#include <meterparams.h>
#include <sdcard.h>
#include <ESPAsyncWebServer.h>
#include "DigitalInput.h"


extern TaskHandle_t sdCardTaskHandle;
extern TaskHandle_t dataTaskHandle;
extern TaskHandle_t TaskHandle_PostData_Start_S1;  // Task handle
extern SemaphoreHandle_t xHttpMutex;

// ADD THESE CONSTANTS near the top of your file or inside the class
#define AP_TIMEOUT_MS (10 * 60 * 1000) // 10 minutes
#define AP_REENABLE_TIMEOUT_MS (5 * 60 * 1000) // 5 minutes
#define STA_RETRY_INTERVAL_MS 15000 // 15 seconds

class Devsbot
{
  public:
          const float defaultFirmwareVersion=1.0; // not a valid firmware version
          const bool defaultCheckVal=0;
          float deviceFirmwareVersion;
          float devsbotFirmwareVersion;

          String devicePassword= "";
          String deviceSsid= "";

     
          const char* userSsid= NULL;
          const char* userPassword= NULL;

          byte apiHitCnt=3;
          
          void begin();
          void begin(const char* authToken); //connecting to default ssid and password
          void begin(const char* authToken, const char* ssid, const char* password);
          void wifiBegin(const char* ssid, const char* password);
          void Loop();
          void sendVirtualWrite(byte dbVirtualPin, int dbVirtualData);
          void sendVirtualWrite(byte dbVirtualPin, String dbVirtualData);
          void devsbotDelay(uint64_t dbMilliSeconds);
          bool sentEnergyMeterData(String &EnergyString);
          void deviceLog(String dbDeviceLogData);

          bool instantaneousReading; //live_data
          int completeJsonReading=60;//data_interval
          uint8_t numSlave; //number of slave by default is 1
          byte slaveIdArray[32];
          void deviceContinue();
          void wifiAfterProvision();
          void RWToEEPROM(const char* authToken);
          void ReadOnlyEEPROM();
          bool checkServerConnection();
          void reconnectingToWifi();
          void WiFiEvent(WiFiEvent_t event);
          void EEPROMfirmwareVersion();
          bool acknowldgeApi(uint8_t value);
          bool meterAddressInit();
          void meterAddressData();
          String sendHttpRequest(String endpoint, String payload, const char* method, const char* contentType, const char* callerName); 

          bool wifiStatus=0;
          bool poorWifi=0;
          bool sioDisconnect=1;//by default socket io is disconnected
          bool sioDisconnectFlag=1;
          uint64_t sioDisconnectStart=0;
          uint8_t mtrMake=0;

          uint64_t calPreTime=0;
          volatile bool httpMethodTaken=0;
          bool meterAddInSpiff=0;
          bool SendDataToServer=1;


           /** functions and variable related to configuration of network form a webpage */
           String cnfSsid="";
           String cnfPass="";
           
          bool ApNetworkFlag=0;
          bool notConnetedtoNetwork=0;
          bool storeDataToSd=0;
          void saveNetworkData(String ssid, String password,String type);
          void chooseNetworkConnectivity();
          bool getNetworkDetails();
          bool readNetworkData();
          bool connectToNetwork();
          void disconnectFromNetwork();
          void wifiConnectionChecking(String ssid,String passwrd);
          static void handleProvision(AsyncWebServerRequest *request);
          bool postDataToServer(uint8_t slaveid,uint8_t virtualpin,bool Status);

          /**for gprs module */
          // void chooseNetworkConnectivity();
          // bool check4Gmodule();
          // void gsmgetmethod();
          // void gsmPostmethod();

          uint64_t previousmillis1=0;
          uint64_t currentmillis=0;

          bool maduraSteelOn=0;

          /// QC WEB SERVER OBJECT ///

          Devsbot();   // constructor
          ~Devsbot();  // destructor
                      // NEW: QC Web Server support methods
            // Get DIO configuration for QC testing
            void getDIOConfiguration(int*& pins, String*& modes, int& count);
            
            // Get RS485 configuration for QC testing  
            void getRS485Configuration(uint8_t*& slaveIds, int& slaveCount,
                                      uint16_t*& registers, int& regCount,
                                      uint32_t& baudRate, uint8_t& dataBits,
                                      uint8_t& parity, uint8_t& stopBits);
            
            // Initialize DIO pins based on widget configuration
            void initializeDIOPins();
            
            // Initialize RS485 configuration based on meter address data
            void initializeRS485Config();
            
            // Helper method to parse widget JSON and extract pin configurations
            bool parseWidgetConfiguration();
            
            // Helper method to parse meter address JSON and extract RS485 config
            bool parseMeterConfiguration();
            
            // Get current pin state (for QC testing)
            int getPinState(int pin);
            
            // Set pin mode and state (for QC testing)
            void setPinMode(int pin, int mode);
            void setPinState(int pin, int state);

            void parseNetworkData(const String& data);
            String readFileFromSPIFFS(const char* path);
            void startAPMode();

            bool loadServerConfig();
            void setDefaultServerConfig();
            bool parseServerConfigFromJson(const String& jsonData);

                // Getter methods to access the dynamic URLs
            const String& getAuthTokenApiURL() const { return dynamicAuthTokenApiURL; }
            const String& getDeviceProvisionURL() const { return dynamicDeviceProvisionURL; }
            const String& getDevsbotWidgetURL() const { return dynamicDevsbotWidgetURL; }
            const String& getDevsbotOTAUpdateURL() const { return dynamicDevsbotOTAUpdateURL; }
            const String& getFirmwareVersionURL() const { return dynamicFirmwareVersionURL; }
            const String& getDevsbotDeviceStatusURL() const { return dynamicDevsbotDeviceStatusURL; }
            const String& getDevsbotDeviceLogURL() const { return dynamicDevsbotDeviceLogURL; }
            const String& getEnergyMeterJsonURL() const { return dynamicEnergyMeterJsonURL; }
            const String& getLedStartStopApiURL() const { return dynamicLedStartStopApiURL; }
            const String& getMeterAddressURL() const { return dynamicMeterAddressURL; }
            const String& getReverseURL() const { return dynamicReverseURL; }
            const String& getHostname() const { return dynamicHostname; }
            uint16_t getPort() const { return dynamicPort; }

            bool canPerformServerOperations();
            ////QC WEB SERVER OBJECT ////


            // Digital Input Lib //////
            String activityTrackerURL = "your_activity_tracker_api_url";
            String aliveStatusURL = "your_alive_status_api_url";
            // Method to get auth token (needed by DigitalInput)
            String getAuthToken() const { return devsbotAuthToken; }
            void sendActivityTrackerData();
            void sendAliveStatusData();
            void initialize();
            void startSensorTask();
            // Digital Input Lib //////

            bool sendDIStatusData(String& payload);
            bool sendDIPulseData(String& payload);


          

  private:
         
          HTTPClient httpota;
          WiFiClientSecure client;
          static inline byte sIOConnectionStatus;
          
          String devsbotPassword= "";
          String devsbotSsid= "";

          String jwtToken="";
          
          const char* devsbotDefaultSsid= "Niraltek";
          const char* devsbotDefaultPassword= "Niraltek@SEM";

           // Wi-Fi credentials for AP mode
          String apSSID = "Devsbot-AP";         // SSID for the Access Point
          const char* apPassword = "devsbot123";     // Password for the Access Point

          String preSsid="";
          String prePassword="";
          

          String devsbotClusterID;
          String devsbotAuthToken;

          bool configArray[4]={0};
          
          //uint16_t apiResponseCode; 
          int apiResponseCode;
          String apiResponse;
          String provisionData;
          String widgetData;

          ////////// QC WEB SERVER /////////////

          
              // Pin configuration arrays (make these accessible)
          int*    dynamicDioPins   = nullptr;
          String* dynamicDioModes  = nullptr;
          int     dynamicDioPinCount = 0;

          uint8_t* dynamicSlaveIds   = nullptr;
          int      dynamicSlaveCount = 0;
          uint16_t* dynamicRegisters = nullptr;
          int      dynamicRegisterCount = 0;

          uint32_t dynamicBaudRate = 9600;
          uint8_t  dynamicDataBits = 8;
          uint8_t  dynamicParity   = 0;
          uint8_t  dynamicStopBits = 1;

          bool localModeOnly = false;
          ///////////// QC WEB SERVER ////////////////

          int heartBeatInterval;
          bool aliveState=true;
          int devsbotheartBeatInterval;
          bool devsbotliveData;
          int devsbotDataInterval;
          bool connected =false;
          bool gotIp=false;
          IPAddress ip;
          IPAddress serverIP;
          int serverIpResult=0;
          bool dnstoip=0;
          bool devsbotauthenticationcheck;


          const int wifiWating=15000;
          uint8_t devsbotconnectivity=0;//network connectivity by default it is wifi
          uint8_t deviceconnectivity=0;//network connectivity by default it is wifi
          String connectivityType ="";
          

          unsigned long dbMessageTimestamp = 0;
          unsigned long devsbotStatus_Time = 0;
         
          

          String digitalInputPin;
          String digitalInputPullupPin;
          String analogInputPin;

          // Dynamic server configuration variables
          String dynamicHostname;
          uint16_t dynamicPort;
          String dynamicAuthTokenApiURL;
          String dynamicDeviceProvisionURL;
          String dynamicDevsbotWidgetURL;
          String dynamicDevsbotOTAUpdateURL;
          String dynamicFirmwareVersionURL;
          String dynamicDevsbotDeviceStatusURL;
          String dynamicDevsbotDeviceLogURL;
          String dynamicEnergyMeterJsonURL;
          String dynamicLedStartStopApiURL;
          String dynamicMeterAddressURL;
          String dynamicReverseURL;
          
          bool serverConfigLoaded;
  
 
      

          void wifiConnectionChecking();
          void AuthToken(const char* authToken);
          bool AuthTokenAPI();
          bool deviceProvision();
          void deviceProvisionData();
          bool devicePreProvisionData();
          void firmwareUpdate();
          void widgetBegin();
          bool widgetAPI();
          void widgetPinInitialize();
          void firmwareVersionSend();
          void deviceFirmwareUpdate();
          void socketIOConnection();
          static void socketIOEvent(socketIOmessageType_t type, uint8_t * payload, size_t length);
          void devsbotAuthentication();
          void devsbotStatus();
          void widgetUpdate();
          void widgetVersionEdit();
          void sensorInput();
          void jsonInputSend(String dbInputMethod, String dbInputPin, String dbDeviceValue);
          void jsonInputSend(String MeterJsonData,String Energyjson);
          void deviceLogOTA(String dbDeviceLogOTAData);

          // Digital Input Lib ///////
          // Digital Input instance
          DigitalInput digitalInput;
          
          // Activity tracker and alive status intervals
          unsigned long lastActivitySentMillis = 0;
          unsigned long activitySendInterval = 5000; // 5 seconds for activity tracker

          unsigned long lastDataSentMillis = 0;
          unsigned long dataSendInterval = 60000; // 60 seconds for alive status

          //   sdcard card; // Instance of sdcard
          // Digital Input Lib ///////

          // --- THIS IS THE FIX ---
          // The event handler is no longer static. It is now a regular private member function.
          void socketIOEventHandler(socketIOmessageType_t type, uint8_t * payload, size_t length);
          // --- END OF FIX ---

    // ADD THESE NEW PRIVATE HELPER FUNCTIONS
    void startAP();
    void stopAP();
    void nonBlockingConnectSTA();

    // ADD THESE NEW PRIVATE MEMBER VARIABLES
    bool _apEnabled;
    unsigned long _bootTime;
    unsigned long _lastStaDisconnectTime;
    unsigned long _lastStaRetryTime;

    bool _needsPostConnectionSetup;
    bool _isInitialized; // <-- ADD THIS FLAG



};

extern Devsbot dBot;
#endif
