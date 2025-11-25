#ifndef DIGITALINPUT_H
#define DIGITALINPUT_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <SPIFFS.h>
#include <WiFi.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/pcnt.h"
#include <Preferences.h> // <-- ADDED: Include the Preferences library
#include <nvs_flash.h>     // For nvs_flash_init, nvs_flash_erase
#include <esp_err.h>       // For ESP_ERROR_CHECK and error codes
#include <esp_partition.h> // For the debug function printNVSPartitionInfo

// Forward declaration of the sdcard class
class sdcard;

// === PCNT Settings ===
#define PCNT_UNIT               PCNT_UNIT_0
#define PCNT_CHANNEL            PCNT_CHANNEL_0
#define PCNT_H_LIM_VAL          10000
#define PCNT_L_LIM_VAL          0

// === Type definitions for industrial monitoring ===
enum InputType {
    MACHINE = 1,
    MOTOR = 2,
    HEATER = 3,
    PULSE = 4
};
enum TypeId {
    TYPE_STATUS = 1,
    TYPE_PULSE_COUNT = 2
};

// === Structures for different input types ===
struct StatusInputData {
    int pin;
    int type_name;      // 1-machine, 2-motor, 3-heater
    int type_id;        // Always 1 for status
    int currentState;
    int lastState;
    bool stateChanged;
    String lastChangeTime;
    unsigned long lastDebounceTime;
};

struct PulseInputData {
    int pin;
    int type_name;      // Always 4 for pulse
    int type_id;        // Always 2 for pulse
    int pulseCount;
    int lastSentCount;
    bool usePCNT;
    bool initialized;
};

// Forward declarations
class Devsbot;

class DigitalInput {
private:
    // Arrays for managing inputs
    StatusInputData statusInputs[20];
    PulseInputData pulseInputs[10];
    int statusInputCount;
    int pulseInputCount;

    // Job control variables
    bool jobEnabled;
    bool pulseCountEnabled;
    bool resetPulseCount;

    // PCNT Variables
    int16_t currentPulseCount;
    int16_t lastPulseCount;
    int pcntCounterPin;

    // Status Pin Change Variables
    volatile bool statusChanged[20];
    volatile int statusStates[20];

    // Task handle for sensor monitoring
    TaskHandle_t sensorTaskHandle;
    volatile bool dataSendRequired;

    // Reference to main Devsbot instance for API calls
    Devsbot* devsbotInstance;

    // SD card instance for logging
    sdcard* sdCardInstance;
    SemaphoreHandle_t fileMutex;

    // Preferences object for persistent state
    Preferences jobStatePrefs;

        // Add a variable to store the current job ID
    int currentJobId;
    // --- NEW: Wear-Leveling for SPIFFS Pulse Count ---
    unsigned long lastPulseSaveTime;
    bool pulseDataDirty; // Flag to indicate if there's new, unsaved pulse data
    static const unsigned long PULSE_SAVE_INTERVAL_MS = 5000; // Save every 5 seconds

    int lastOfflinePulseCount;
    Preferences pulsePrefs;  // NVS storage object

    // Private helper methods
    String getCurrentTimestamp();
    void initializePCNT(int pin);
    // Replace SPIFFS functions with NVS functions
    void savePulseCountsToNVS();      // Replaces savePulseCountsToSPIFFS()
    void loadPulseCountsFromNVS();    // Replaces loadPulseCountsFromSPIFFS()
    void resetAllPulseCounts();
    
    // Sensor reading methods
    void readStatusInputs();
    bool readPulseInputs();
    void migrateSPIFFStoNVS();
    // FreeRTOS task function
    static void sensorTask(void* pvParameters);

public:
    // Constructor
    DigitalInput();
    
    // Destructor
    ~DigitalInput();

    // Initialization methods
    void initialize(Devsbot* devsbotRef);
    void widgetPinInitialize(const String& widgetJsonData);
    bool widgetAPI();
    
    // Main processing methods
    void startSensorTask();
    void stopSensorTask();
    void processLoop();
    
    // Data sending methods
    void sendActivityTrackerData();
    void sendAliveStatusData();
    void logOfflinePulseCount(int pin, int pulseCount); // <-- ADD THIS LINE
    void clearOfflinePulseLog();
    int getCurrentJobId() const { return currentJobId; }
    void processAliveStatusResponse(String response);
    
    // Job control methods
    void setJobEnabled(bool enabled);
    void setPulseCountEnabled(bool enabled);
    bool isJobEnabled() const { return jobEnabled; }
    bool isPulseCountEnabled() const { return pulseCountEnabled; }
    
    // Configuration methods
    void printPinConfiguration();
    
    // Data logging methods
    void logStatusChange(int pin, int state, String timestamp, int type_name); 
    void logPulseData(int pin, int pulseCount);
    
    // Getters for status
    int getStatusInputCount() const { return statusInputCount; }
    int getPulseInputCount() const { return pulseInputCount; }
    
    // Get specific input data
    StatusInputData* getStatusInput(int index);
    PulseInputData* getPulseInput(int index);
    
    // Reset methods
    void resetAllInputs();
    
    // Widget data string (for compatibility with existing code)
    String digitalInputPin;
    String widgetData;
};

#endif // DIGITALINPUT_H

