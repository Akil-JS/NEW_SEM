#include "DigitalInput.h"
#include "sdcard.h"
#include "DevsbotEnergyLocal.h" // Required to access the Devsbot class definition
#include "HardwareRTC.h"      // <-- ADDED: Include the new RTC manager
#include "DebugMacro.h"

// Make the global 'card' object (which contains our LogManagers) available in this file.
extern sdcard card;

// --- CONSTANTS for Debouncing ---
const unsigned long DEBOUNCE_DELAY_MS = 50; // 50 milliseconds

// Constructor
DigitalInput::DigitalInput() {
    statusInputCount = 0;
    pulseInputCount = 0;
    jobEnabled = false;
    pulseCountEnabled = false;
    resetPulseCount = false;
    currentPulseCount = 0;
    lastPulseCount = 0;
    pcntCounterPin = -1;
    sensorTaskHandle = NULL;
    devsbotInstance = nullptr;
    
    digitalInputPin = "";
    widgetData = "";

    // --- Initialize wear-leveling variables ---
    lastPulseSaveTime = 0;
    pulseDataDirty = false;

        // --- START MODIFICATION ---
    currentJobId = 0; // Default to 0
    // --- END MODIFICATION ---
    lastOfflinePulseCount = -1; // Initialize to -1 to force first save

    for (int i = 0; i < 20; i++) {
        statusChanged[i] = false;
        statusStates[i] = 0;
    }
}


// Destructor
DigitalInput::~DigitalInput() {
    stopSensorTask();
}

void DigitalInput::initialize(Devsbot* devsbotRef) {
    devsbotInstance = devsbotRef;
    
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        Serial.println("NVS: Erasing and reinitializing...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    Serial.println("✅ NVS initialized successfully");
    
    // ONE-TIME: Migrate old SPIFFS data to NVS
    // This will automatically run on first boot after firmware update
    // After all devices are updated, you can remove this line
    migrateSPIFFStoNVS();
    
    // Load pulse data from NVS
    loadPulseCountsFromNVS();
}



// --- THIS IS THE FIX ---
// This function now formats the LOCAL time from the RTC manager, not UTC.
String DigitalInput::getCurrentTimestamp() {
    DateTime localTime = rtcManager.now(); // Get the local DateTime object
    char timestampBuffer[20];
    
    // Format the LOCAL time into the "YYYY-MM-DD HH:MM:SS" format.
    sprintf(timestampBuffer, "%04d-%02d-%02d %02d:%02d:%02d", 
            localTime.year(), 
            localTime.month(), 
            localTime.day(), 
            localTime.hour(), 
            localTime.minute(), 
            localTime.second());
            
    return String(timestampBuffer);
}
// --- END OF FIX ---



void DigitalInput::initializePCNT(int pin) {
    // 1. Configure all hardware settings
    pcnt_config_t pcntConfig = {};
    pcntConfig.pulse_gpio_num = pin;
    pcntConfig.ctrl_gpio_num = PCNT_PIN_NOT_USED;
    pcntConfig.unit = PCNT_UNIT;
    pcntConfig.channel = PCNT_CHANNEL;
    pcntConfig.pos_mode = PCNT_COUNT_DIS;
    pcntConfig.neg_mode = PCNT_COUNT_INC;
    pcntConfig.lctrl_mode = PCNT_MODE_KEEP;
    pcntConfig.hctrl_mode = PCNT_MODE_KEEP;
    pcntConfig.counter_h_lim = PCNT_H_LIM_VAL;
    pcntConfig.counter_l_lim = PCNT_L_LIM_VAL;

    pcnt_unit_config(&pcntConfig);

    // 2. Enable the hardware filter
    pcnt_set_filter_value(PCNT_UNIT, 1023);
    pcnt_filter_enable(PCNT_UNIT);

    // 3. Pause, clear, and resume the counter
    pcnt_counter_pause(PCNT_UNIT);
    pcnt_counter_clear(PCNT_UNIT);
    
    // *** CRITICAL FIX: Add a small delay to ensure clear completes ***
    delayMicroseconds(100); // Give hardware time to actually clear
    
    pcnt_counter_resume(PCNT_UNIT);

    pcntCounterPin = pin;

    // 4. Read the hardware counter AFTER clear and set both variables to match
    pcnt_get_counter_value(PCNT_UNIT, &currentPulseCount);
    lastPulseCount = currentPulseCount;
    
    Serial.printf("PCNT initialized for pin %d. Hardware counter cleared and synced. "
                  "currentPulseCount=%d, lastPulseCount=%d\n", 
                  pin, currentPulseCount, lastPulseCount);
}

// === REVISED: This is the updated function to handle the SERVER's JSON format ===
void DigitalInput::widgetPinInitialize(const String& widgetJsonData) {
    Serial.println("DigitalInput widgetPinInitialize begin");
    digitalInputPin = "";
    statusInputCount = 0;
    pulseInputCount = 0;

    // --- THIS IS THE FIX: Load the last known job state from persistent storage ---
    jobStatePrefs.begin("DI_JobState", true); // Open in read-only mode first
    jobEnabled = jobStatePrefs.getBool("jobEnabled", false); // Default to false if not found
    currentJobId = jobStatePrefs.getInt("job_id", 0);
    pulseCountEnabled = jobEnabled; // Sync pulse counting with the loaded job state
    jobStatePrefs.end();
    Serial.println("Loaded persistent job state. Job Enabled: " + String(jobEnabled ? "Yes" : "No") + ", Job ID: " + String(currentJobId));

    // --- END OF FIX ---
    if (!widgetJsonData.isEmpty()) {
        DynamicJsonDocument doc(2048);
        DeserializationError error = deserializeJson(doc, widgetJsonData);

        if (!error) {
            for (JsonVariant elem : doc.as<JsonArray>()) {
                String datastreamName = elem["datastream_name"];
                String pinModeStr = elem["pinmode"];

                if (datastreamName == "Digital") {
                    JsonArray pinArray = elem["pin"];
                    
                    // --- THIS IS THE FIX ---
                    // Read the arrays using the keys that the server is actually sending.
                    JsonArray inputMethodArray = elem["input_method"];
                    JsonArray typeStringArray = elem["type"]; // Changed from "type_id" to "type"
                    // --- END OF FIX ---

                    for (size_t i = 0; i < pinArray.size(); i++) {
                        int pin = pinArray[i];
                        String inputMethodStr = inputMethodArray[i].as<String>();
                        String typeStr = typeStringArray[i].as<String>();

                        digitalInputPin += String(pin) + (i < pinArray.size() - 1 ? "," : "");

                        if (inputMethodStr == "status") {
                            // Map the type string (e.g., "heater") to an internal enum
                            int mappedTypeName = 0;
                            if (typeStr == "motor") mappedTypeName = MOTOR;
                            else if (typeStr == "heater") mappedTypeName = HEATER;
                            else if (typeStr == "machine") mappedTypeName = MACHINE;
                            
                            if (mappedTypeName == 0) {
                                Serial.println("Warning: Unknown status type '" + typeStr + "' for pin " + String(pin));
                                continue;
                            }

                            if (pinModeStr == "INPUT") pinMode(pin, INPUT);
                            else if (pinModeStr == "INPUT_PULLUP") pinMode(pin, INPUT_PULLUP);
                            
                            statusInputs[statusInputCount].pin = pin;
                            statusInputs[statusInputCount].type_name = mappedTypeName;
                            statusInputs[statusInputCount].type_id = TYPE_STATUS;
                            
                            // statusInputs[statusInputCount].currentState = digitalRead(pin);
                            statusInputs[statusInputCount].currentState = !digitalRead(pin);

                            statusInputs[statusInputCount].lastState = statusInputs[statusInputCount].currentState;
                            statusInputs[statusInputCount].stateChanged = false;
                            statusInputs[statusInputCount].lastChangeTime = "";
                            statusInputs[statusInputCount].lastDebounceTime = 0;
                            
                            Serial.println("Status Input Pin=> " + String(pin) + " Type: " + typeStr);
                            statusInputCount++;

                        // --- THIS IS THE SECOND FIX ---
                        // Check for "pcount" (lowercase) to match the server's response.
                        } else if (inputMethodStr == "pcount") {
                        // --- END OF FIX ---
                            pinMode(pin, INPUT_PULLUP); // Pulse pins should default to pullup for falling edge
                            //pinMode(pin, INPUT); // Pulse pins should default to pullup for falling edge
                            
                            
                            pulseInputs[pulseInputCount].pin = pin;
                            pulseInputs[pulseInputCount].type_name = PULSE;
                            pulseInputs[pulseInputCount].type_id = TYPE_PULSE_COUNT;
                            // ... (rest of pulse config) ...
                            pulseInputs[pulseInputCount].pulseCount = 0;
                            pulseInputs[pulseInputCount].lastSentCount = 0;
                            pulseInputs[pulseInputCount].usePCNT = true;
                            pulseInputs[pulseInputCount].initialized = false;
                            
                            Serial.println("Pulse Input Pin=> " + String(pin) + " (Cycle Counter)");
                            pulseInputCount++;
                        }
                    }
                }
            }
             loadPulseCountsFromNVS();  // Was: loadPulseCountsFromSPIFFS()
            for (int i = 0; i < pulseInputCount; i++) {
                if (pulseInputs[i].usePCNT) {
                    initializePCNT(pulseInputs[i].pin);
                    pulseInputs[i].initialized = true;
                    
                    // *** ADD THIS: Explicitly prevent phantom pulses on reconnection ***
                    // Reset the baseline to ignore any pulses during WiFi disconnection
                    currentPulseCount = 0;
                    lastPulseCount = 0;
                    
                    Serial.printf("Pulse input on pin %d initialized. "
                                "Saved count from NVS: %d (will continue from here)\n",
                                pulseInputs[i].pin, pulseInputs[i].pulseCount);
                    break;
                }
            }

        } else {
            Serial.println("Error: Failed to parse widget JSON data.");
        }
    } else {
        Serial.println("Warning: No widget data provided for initialization.");
    }
    Serial.println("DigitalInput widgetPinInitialize End");
}




// Widget API function
bool DigitalInput::widgetAPI() {
    Serial.println("DigitalInput widgetAPI begin");
    
    // NOTE: This function should not contain the actual API call
    // The API call should be made from DevsbotEnergyLocal.cpp
    // This function just processes the received widget data
    
    // For now, return true - the actual API call will be handled by DevsbotEnergyLocal
    Serial.println("DigitalInput widgetAPI End");
    return true;
}

// --- NEW: NVS Save (Power-Fail-Safe is BUILT-IN!) ---
void DigitalInput::savePulseCountsToNVS() {
    // Open NVS in read-write mode
    if (!pulsePrefs.begin("pulse_data", false)) {
        DEBUG("--> ERROR: Failed to open NVS namespace for writing.");
        return;
    }
    
    // Write each pulse input's data
    for (int i = 0; i < pulseInputCount; i++) {
        // Create unique keys for each pin
        String countKey = "cnt_" + String(pulseInputs[i].pin);
        String sentKey = "sent_" + String(pulseInputs[i].pin);
        
        // NVS automatically handles:
        // ✅ Atomic writes (no temp file needed!)
        // ✅ Power-fail safety (hardware level)
        // ✅ Wear leveling (across multiple sectors)
        // ✅ Only writes if value changed (built-in optimization!)
        
        size_t written1 = pulsePrefs.putInt(countKey.c_str(), pulseInputs[i].pulseCount);
        size_t written2 = pulsePrefs.putInt(sentKey.c_str(), pulseInputs[i].lastSentCount);
        
        if (written1 == 0 || written2 == 0) {
            DEBUG("--> WARNING: NVS write may have failed for pin " + String(pulseInputs[i].pin));
        }
    }
    
    pulsePrefs.end();
    
    DEBUG("Pulse counts saved to NVS (Power-Fail-Safe).");
}

// --- NEW: NVS Load (Simple & Fast) ---
void DigitalInput::loadPulseCountsFromNVS() {
    // Open NVS in read-only mode
    if (!pulsePrefs.begin("pulse_data", true)) {
        DEBUG("No pulse count data found in NVS. Starting from 0.");
        return;
    }
    
    bool dataFound = false;
    
    for (int i = 0; i < pulseInputCount; i++) {
        String countKey = "cnt_" + String(pulseInputs[i].pin);
        String sentKey = "sent_" + String(pulseInputs[i].pin);
        
        // Check if keys exist (NVS returns 0 if key doesn't exist)
        if (pulsePrefs.isKey(countKey.c_str())) {
            pulseInputs[i].pulseCount = pulsePrefs.getInt(countKey.c_str(), 0);
            pulseInputs[i].lastSentCount = pulsePrefs.getInt(sentKey.c_str(), 0);
            
            // Sync with hardware counter if this is the PCNT pin
            if (pulseInputs[i].pin == pcntCounterPin) {
                currentPulseCount = pulseInputs[i].pulseCount;
                lastPulseCount = currentPulseCount;
            }
            
            dataFound = true;
            Serial.printf("Loaded from NVS - Pin %d: Count=%d, LastSent=%d\n",
                         pulseInputs[i].pin, 
                         pulseInputs[i].pulseCount,
                         pulseInputs[i].lastSentCount);
        }
    }
    
    pulsePrefs.end();
    
    if (dataFound) {
        Serial.println("Pulse counts loaded from NVS");
    } else {
        DEBUG("No pulse count data found in NVS. Starting from 0.");
    }
}

// --- NEW: One-Time Migration from SPIFFS to NVS ---
void DigitalInput::migrateSPIFFStoNVS() {
    // Check if old SPIFFS files exist
    bool hasOldData = SPIFFS.exists("/pulse_counts.json") || 
                      SPIFFS.exists("/pulse_counts_new.json");
    
    if (!hasOldData) {
        // No old data to migrate
        return;
    }
    
    Serial.println("========================================");
    Serial.println("SPIFFS to NVS Migration Starting...");
    Serial.println("========================================");
    
    // Step 1: Check for leftover temp file (power failure during last SPIFFS save)
    if (SPIFFS.exists("/pulse_counts_new.json")) {
        Serial.println("--> RECOVERY: Power loss detected during last SPIFFS save.");
        Serial.println("--> Attempting to recover from temp file...");
        SPIFFS.remove("/pulse_counts.json"); 
        SPIFFS.rename("/pulse_counts_new.json", "/pulse_counts.json");
    }
    
    // Step 2: Load data from SPIFFS
    if (!SPIFFS.exists("/pulse_counts.json")) {
        Serial.println("--> No valid SPIFFS data found. Skipping migration.");
        return;
    }
    
    File file = SPIFFS.open("/pulse_counts.json", "r");
    if (!file) {
        Serial.println("--> ERROR: Failed to open SPIFFS file for migration.");
        return;
    }
    
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    
    if (error) {
        Serial.println("--> ERROR: Failed to parse SPIFFS data. Migration aborted.");
        return;
    }
    
    // Step 3: Migrate to NVS
    pulsePrefs.begin("pulse_data", false); // Open for writing
    
    JsonArray arr = doc["pulse_counts"];
    int migratedCount = 0;
    
    for (JsonObject obj : arr) {
        int pin = obj["pin"];
        int count = obj["count"] | 0;
        int lastSent = obj["last_sent"] | 0;
        
        String countKey = "cnt_" + String(pin);
        String sentKey = "sent_" + String(pin);
        
        pulsePrefs.putInt(countKey.c_str(), count);
        pulsePrefs.putInt(sentKey.c_str(), lastSent);
        
        migratedCount++;
        Serial.printf("--> Migrated Pin %d: count=%d, lastSent=%d\n", pin, count, lastSent);
    }
    
    pulsePrefs.end();
    
    // Step 4: Delete old SPIFFS files
    if (SPIFFS.remove("/pulse_counts.json")) {
        Serial.println("--> Deleted old SPIFFS file: /pulse_counts.json");
    }
    if (SPIFFS.exists("/pulse_counts_new.json")) {
        if (SPIFFS.remove("/pulse_counts_new.json")) {
            Serial.println("--> Deleted old SPIFFS temp file: /pulse_counts_new.json");
        }
    }
    
    Serial.println("========================================");
    Serial.printf("Migration Complete! %d pins migrated.\n", migratedCount);
    Serial.println("========================================");
}

// --- MODIFIED: Clear NVS instead of SPIFFS ---
void DigitalInput::resetAllPulseCounts() {
    // 1. Reset all the in-memory (RAM) variables to zero.
    for (int i = 0; i < pulseInputCount; i++) {
        pulseInputs[i].pulseCount = 0;
        pulseInputs[i].lastSentCount = 0;
    }
    
    // 2. Reset the physical hardware counter on the chip.
    if (pcntCounterPin != -1) {
        pcnt_counter_pause(PCNT_UNIT);
        pcnt_counter_clear(PCNT_UNIT);
        pcnt_counter_resume(PCNT_UNIT);
        currentPulseCount = 0;
        lastPulseCount = 0;
    }
    
    // 3. Clear NVS storage (replaces SPIFFS removal)
    if (pulsePrefs.begin("pulse_data", false)) {
        pulsePrefs.clear(); // Removes all keys in this namespace
        pulsePrefs.end();
        Serial.println("Pulse count data cleared from NVS.");
    }
    
    // 4. Clean up old SPIFFS files if they still exist (safety cleanup)
    if (SPIFFS.exists("/pulse_counts.json")) {
        if (SPIFFS.remove("/pulse_counts.json")) {
            Serial.println("Cleaned up old SPIFFS file: /pulse_counts.json");
        }
    }
    if (SPIFFS.exists("/pulse_counts_new.json")) {
        if (SPIFFS.remove("/pulse_counts_new.json")) {
            Serial.println("Cleaned up old SPIFFS temp file: /pulse_counts_new.json");
        }
    }
    
    // 5. Clear SD card logs
    if (card.cardMounted) {
        card.clearDigitalInputLogs();
        card.statusLogs.clearLogs();
    }
    
    Serial.println("All pulse counts have been reset to 0.");
}


// --- MODIFIED: Includes Debouncing Logic and uses RTC timestamp ---
void DigitalInput::readStatusInputs() {
    if (!jobEnabled) return;
    for (int i = 0; i < statusInputCount; i++) {
        //int newState = digitalRead(statusInputs[i].pin);
                // --- THIS IS THE FIX ---
        // Invert the logic using the '!' (NOT) operator.
        int newState = !digitalRead(statusInputs[i].pin);
        // --- END OF FIX ---
        // Simplified debouncing logic
        if (newState != statusInputs[i].lastState) {
            statusInputs[i].lastDebounceTime = millis();
        }

        if ((millis() - statusInputs[i].lastDebounceTime) > DEBOUNCE_DELAY_MS) {
            if (newState != statusInputs[i].currentState) {
                statusInputs[i].currentState = newState;
                statusInputs[i].lastState = newState; // Sync lastState here
                statusInputs[i].stateChanged = true;
                statusInputs[i].lastChangeTime = getCurrentTimestamp(); // Use the RTC-aware function
                String typeName = (statusInputs[i].type_name == MOTOR) ? "Motor" : (statusInputs[i].type_name == HEATER) ? "Heater" : "Machine";
                Serial.println("==============================================================================");
                Serial.println("Status change detected - Pin: " + String(statusInputs[i].pin) + " (" + typeName + ") State: " + String(newState));
                Serial.println("==============================================================================");
                //logStatusChange(statusInputs[i].pin, newState, statusInputs[i].lastChangeTime, statusInputs[i].type_name);
            }
        }
        statusInputs[i].lastState = newState; // Update lastState on every check
    }
}



bool DigitalInput::readPulseInputs() {
    bool dataChanged = false;
    
    for (int i = 0; i < pulseInputCount; i++) {
        if (pulseInputs[i].usePCNT && pulseInputs[i].pin == pcntCounterPin) {
            // *** ALWAYS READ THE HARDWARE COUNTER (even if disabled) ***
            pcnt_get_counter_value(PCNT_UNIT, &currentPulseCount);

            if (currentPulseCount != lastPulseCount) {
                int16_t newPulses;
                if (currentPulseCount < lastPulseCount) {
                    newPulses = (PCNT_H_LIM_VAL - lastPulseCount) + currentPulseCount;
                } else {
                    newPulses = currentPulseCount - lastPulseCount;
                }

                if (newPulses > 0) {
                    // *** NOW CHECK IF WE SHOULD COUNT THEM ***
                    if (pulseCountEnabled) {
                        Serial.printf("==> HARDWARE PULSE DETECTED: %d new pulses. New total: %d\n", 
                                      newPulses, pulseInputs[i].pulseCount + newPulses);
                        pulseInputs[i].pulseCount += newPulses;
                        dataChanged = true;
                        pulseDataDirty = true;
                        //logPulseData(pulseInputs[i].pin, pulseInputs[i].pulseCount);
                    } else {
                        // Job is disabled - just discard these pulses silently
                        Serial.printf("==> HARDWARE PULSE DETECTED: %d new pulses (DISCARDED - Job Disabled)\n", newPulses);
                    }
                        // else {
                        // // Only log every 100 discarded pulses to reduce spam
                        // static uint16_t discardedCount = 0;
                        // discardedCount += newPulses;
                        // if (discardedCount >= 100) {
                        //     Serial.printf("==> %d pulses discarded (Job Disabled)\n", discardedCount);
                        //     discardedCount = 0;
                        // }
                }
                
                // *** CRITICAL: ALWAYS UPDATE lastPulseCount ***
                // This keeps it in sync with hardware regardless of job state
                lastPulseCount = currentPulseCount;
            }
        }
    }
    
    return dataChanged;
}

void DigitalInput::clearOfflinePulseLog() {
    // Pulse data SD logging is disabled per requirement.
    lastOfflinePulseCount = -1; // Just reset the tracker
}



// FreeRTOS sensor task
void DigitalInput::sensorTask(void* pvParameters) {
    DigitalInput* self = static_cast<DigitalInput*>(pvParameters);
    TickType_t lastWakeTime = xTaskGetTickCount();

    for (;;) {
        // Read status inputs and pulse inputs
        self->readStatusInputs();
        self->readPulseInputs();
        
        // 10ms delay for continuous monitoring
        vTaskDelayUntil(&lastWakeTime, pdMS_TO_TICKS(10));
    }
}

// Start the sensor task
void DigitalInput::startSensorTask() {
    if (sensorTaskHandle == NULL) {
        xTaskCreatePinnedToCore(
            sensorTask,
            "DigitalInputTask",
            4096,
            this,
            1,
            &sensorTaskHandle,
            0 // use core 0
        );
        Serial.println("Digital Input sensor task started");
    }
}

// Stop the sensor task
void DigitalInput::stopSensorTask() {
    if (sensorTaskHandle != NULL) {
        vTaskDelete(sensorTaskHandle);
        sensorTaskHandle = NULL;
        Serial.println("Digital Input sensor task stopped");
    }
}

// --- UNCHANGED: This function logic remains the same ---
void DigitalInput::processLoop() {
    if (pulseDataDirty && (millis() - lastPulseSaveTime > PULSE_SAVE_INTERVAL_MS)) {
        savePulseCountsToNVS();  // CHANGED: From savePulseCountsToSPIFFS()
        lastPulseSaveTime = millis();
        pulseDataDirty = false;
    }
}




// Function to send alive status with pulse data
void DigitalInput::sendAliveStatusData() {
    if (!devsbotInstance) return;
    
    // The actual API calls should be made by DevsbotEnergyLocal
    // This function prepares the pulse data
    
    Serial.println("Preparing alive status data with pulse counts");
    
    // Update last sent counts
    for (int i = 0; i < pulseInputCount; i++) {
        pulseInputs[i].lastSentCount = pulseInputs[i].pulseCount;
    }
}



// --- *** REQUIREMENT 2: ADDED NEW FUNCTION *** ---
// Log pulse data to SD
void DigitalInput::logOfflinePulseCount(int pin, int pulseCount) {
    // Pulse data SD logging is disabled per requirement.
    // We still reset the tracker to prevent log spam.
    if (lastOfflinePulseCount != -1) {
        lastOfflinePulseCount = -1; 
    }
}
// --- *** END NEW FUNCTION *** ---

void DigitalInput::processAliveStatusResponse(String response) {
    DynamicJsonDocument doc(512);
    DeserializationError error = deserializeJson(doc, response);

    if (!error) {
        String jobStr = doc["job_status"].as<String>();
        bool newJobState = (jobStr == "1");

        // Get the new job_id from the response, default to 0 if not present
        int newJobId = doc["job_id"] | 0;


        // Only act if the state has actually changed
        if (newJobState != jobEnabled || newJobId != currentJobId) {
            bool previouslyEnabled = jobEnabled; // Store the old state
            jobEnabled = newJobState;
            pulseCountEnabled = newJobState;
            currentJobId = newJobId;

            // --- SAVE THE NEW STATE ---
            jobStatePrefs.begin("DI_JobState", false); // Open in read-write mode
            jobStatePrefs.putBool("jobEnabled", jobEnabled);
            // Save the new job_id to persistent storage
            jobStatePrefs.putInt("job_id", currentJobId);
            jobStatePrefs.end();
            Serial.println("--> Saved new persistent job state: " + String(jobEnabled ? "Enabled" : "Disabled") + ", Job ID: " + String(currentJobId));
            // --- END OF SAVE LOGIC ---

            if (jobEnabled) {
                Serial.println("Job enabled - digital input monitoring active");

                // *** ADD THIS BLOCK TO DISCARD PULSES DURING DISABLE ***
                if (!previouslyEnabled && pcntCounterPin != -1) {
                    // Job was just re-enabled. Reset the baseline.
                    int16_t currentHardwareCount;
                    pcnt_get_counter_value(PCNT_UNIT, &currentHardwareCount);
                    lastPulseCount = currentHardwareCount; // Sync software baseline to hardware NOW
                    Serial.printf("--> Job re-enabled. Resetting pulse baseline. Discarding pulses accumulated while disabled. New baseline (lastPulseCount) = %d\n", lastPulseCount);
                }
                // *** END OF ADDED BLOCK ***

            } else {
                // Job was just disabled
                resetAllPulseCounts(); // Reset pulse counts only when job becomes 0
                Serial.println("Job disabled - digital input monitoring stopped, pulse counts reset");
            }
        }

        int status = doc["status"];
        if (status == 201) {
            Serial.println("Digital input alive status successful");
        }
    } else {
        Serial.println("Failed to parse digital input alive status response");
    }
}

// --- 5. In setJobEnabled() ---
void DigitalInput::setJobEnabled(bool enabled) {
    if (jobEnabled != enabled) { // Only act on change
        jobEnabled = enabled;
        pulseCountEnabled = enabled;

        // --- SAVE THE NEW STATE ---
        jobStatePrefs.begin("DI_JobState", false); // Open in read-write mode
        jobStatePrefs.putBool("jobEnabled", jobEnabled);
        
        // --- START MODIFICATION ---
        if (!enabled) {
            currentJobId = 0; // Reset job ID if manually disabled
            resetAllPulseCounts();
        }
        // Save the job_id (either the new '0' or the existing one)
        jobStatePrefs.putInt("job_id", currentJobId);
        // --- END MODIFICATION ---

        jobStatePrefs.end();
        
        // --- START MODIFICATION ---
        Serial.println("--> Manually set and saved persistent job state: " + String(jobEnabled ? "Enabled" : "Disabled") + ", Job ID: " + String(currentJobId));
        // --- END MODIFICATION ---
    }
}

void DigitalInput::setPulseCountEnabled(bool enabled) {
    pulseCountEnabled = enabled;
}

// Function to print current pin configuration (for debugging)
void DigitalInput::printPinConfiguration() {
    Serial.println("========= Digital Input Pin Configuration =========");
    Serial.println("Total Status Inputs: " + String(statusInputCount));
    Serial.println("Total Pulse Inputs: " + String(pulseInputCount));
    Serial.println("Job Enabled: " + String(jobEnabled ? "Yes" : "No"));
    Serial.println("Pulse Count Enabled: " + String(pulseCountEnabled ? "Yes" : "No"));
    
    Serial.println("=== Status Inputs ===");
    for (int i = 0; i < statusInputCount; i++) {
        String typeName = "";
        switch(statusInputs[i].type_name) {
            case MOTOR: typeName = "Motor"; break;
            case HEATER: typeName = "Heater"; break;
            case MACHINE: typeName = "Machine"; break;
        }
        Serial.println("Pin " + String(statusInputs[i].pin) + 
                       " - Type: " + typeName + 
                       " - State: " + String(statusInputs[i].currentState));
    }
    
    Serial.println("=== Pulse Inputs ===");
    for (int i = 0; i < pulseInputCount; i++) {
        Serial.println("Pin " + String(pulseInputs[i].pin) + 
                       " - Count: " + String(pulseInputs[i].pulseCount) +
                       " - PCNT: " + String(pulseInputs[i].usePCNT ? "Yes" : "No"));
    }
    Serial.println("==========================================");
}



// --- MODIFY logStatusChange ---
void DigitalInput::logStatusChange(int pin, int state, String timestamp, int type_name) {
    if (devsbotInstance && devsbotInstance->storeDataToSd && card.cardMounted) {
        DynamicJsonDocument logDoc(256);
        
        logDoc["pin"] = pin;
        logDoc["activity_status"] = state;
        logDoc["timestamp"] = timestamp;
        logDoc["type"] = type_name; 
        
        String logString;
        serializeJson(logDoc, logString);
        
        // card.statusLogs.saveLog(logString); // <-- REPLACE THIS
        card.saveStatusLog(logString);     // <-- WITH THIS
    }
}

void DigitalInput::logPulseData(int pin, int pulseCount) {
    // Use the internal devsbotInstance pointer to check flags
    // if (devsbotInstance && devsbotInstance->storeDataToSd && card.cardMounted) {
    //     DynamicJsonDocument logDoc(256);
    //     logDoc["pin"] = pin;
    //     logDoc["total_count"] = pulseCount;
    //     logDoc["timestamp"] = getCurrentTimestamp();

    //     String logString;
    //     serializeJson(logDoc, logString);

    //     card.pulseLogs.saveLog(logString);
    // }
}



// Getter methods for input data
StatusInputData* DigitalInput::getStatusInput(int index) {
    if (index >= 0 && index < statusInputCount) {
        return &statusInputs[index];
    }
    return nullptr;
}

PulseInputData* DigitalInput::getPulseInput(int index) {
    if (index >= 0 && index < pulseInputCount) {
        return &pulseInputs[index];
    }
    return nullptr;
}

// Reset all inputs
void DigitalInput::resetAllInputs() {
    // Reset status inputs
    for (int i = 0; i < statusInputCount; i++) {
        statusInputs[i].stateChanged = false;
        statusInputs[i].lastChangeTime = "";
    }
    
    // Reset pulse inputs
    resetAllPulseCounts();
}