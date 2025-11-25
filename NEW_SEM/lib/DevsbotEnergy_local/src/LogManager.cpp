#include "LogManager.h"
#include "DebugConfig.h"
#include "DebugMacro.h"
#include <SD.h>
#include <Preferences.h>

#define LOG_MUTEX_TIMEOUT_MS 2000

LogManager::LogManager() : processingInProgress(false), devsbotInstance(nullptr) {}

void LogManager::initialize(
    Devsbot* devsbotRef, 
    const char* name, 
    const char* f1, 
    const char* f2, 
    const char* prefs_ns, 
    SendDataCallback cb,
    SemaphoreHandle_t* mutexPtr
) {
    devsbotInstance = devsbotRef;
    logName = name;
    file1Path = f1;
    file2Path = f2;
    prefsNamespace = prefs_ns;
    sendDataCallback = cb;
    spiMutexPtr = mutexPtr;

    preferences.begin(prefsNamespace.c_str(), false);
    currentWriteFile = preferences.getString("writeFile", file1Path.c_str());
    preferences.end();

    currentReadFile = (currentWriteFile == file1Path) ? file2Path : file1Path;

    DEBUG("LogManager [" + logName + "] initialized. Writing to: " + currentWriteFile + ", Reading from: " + currentReadFile);
}

void LogManager::saveState() {
    preferences.begin(prefsNamespace.c_str(), false);
    preferences.putString("writeFile", currentWriteFile.c_str());
    preferences.end();
}

void LogManager::switchFiles() {
    // MUST be called while holding mutex
    currentWriteFile = (currentWriteFile == file1Path) ? file2Path : file1Path;
    currentReadFile = (currentWriteFile == file1Path) ? file2Path : file1Path;
    saveState();
    DEBUG("LogManager [" + logName + "] switched files. Now writing to: " + currentWriteFile + ", Reading from: " + currentReadFile);
}

// ============================================================================
// FIXED: saveLog - assumes caller holds mutex (no nested mutex acquisition)
// ============================================================================
bool LogManager::saveLog(const String& logData) {
    if (!spiMutexPtr) {
        DEBUG("LogManager [" + logName + "] ERROR: No mutex pointer set!");
        return false;
    }
    
    // IMPORTANT: This function is called by sdcard::saveLog, 
    // which ALREADY holds the mutex. Do NOT acquire it again.
    
    DEBUG("LogManager [" + logName + "] Opening file: " + currentWriteFile);
    unsigned long openStart = millis();
    
    File dataFile = SD.open(currentWriteFile.c_str(), FILE_APPEND);
    unsigned long openTime = millis() - openStart;
    
    if (dataFile) {
        DEBUG("LogManager [" + logName + "] File opened in " + String(openTime) + "ms");
        
        unsigned long writeStart = millis();
        dataFile.println(logData);
        dataFile.close();
        delay(10); // <-- NEW: Add small delay after closing file
        unsigned long writeTime = millis() - writeStart;
        
        DEBUG("LogManager [" + logName + "] *************** SAVED to SD CARD *************** (write: " + String(writeTime) + "ms)"); 
        return true;
    } else {
        DEBUG("LogManager [" + logName + "] FAILED to open " + currentWriteFile + " for writing (after " + String(openTime) + "ms).");
        
        // DIAGNOSTIC: Check card status
        uint8_t cardType = SD.cardType();
        if (cardType == CARD_NONE) {
            DEBUG("LogManager [" + logName + "] ERROR: SD card disappeared!");
        } else {
            DEBUG("LogManager [" + logName + "] Card is present but file open failed. Possible corruption.");
        }
        
        return false;
    }
}

// ============================================================================
// FIXED: clearLogs - assumes caller holds mutex
// ============================================================================
void LogManager::clearLogs() {
    if (!spiMutexPtr) {
        DEBUG("LogManager [" + logName + "] ERROR: No mutex pointer set!");
        return;
    }

    // IMPORTANT: Caller (sdcard::clearDigitalInputLogs) already holds mutex
    
    DEBUG("LogManager [" + logName + "]: Clearing log files...");

    if (SD.exists(file1Path.c_str())) {
        if (!SD.remove(file1Path.c_str())) {
             DEBUG("  - FAILED to delete: " + file1Path);
        } else {
             DEBUG("  - Deleted: " + file1Path); 
             delay(10); // <-- NEW: Add small delay after remove
        }
    }
    if (SD.exists(file2Path.c_str())) {
        if (!SD.remove(file2Path.c_str())) {
             DEBUG("  - FAILED to delete: " + file2Path);
        } else {
             DEBUG("  - Deleted: " + file2Path);
             delay(10); // <-- NEW: Add small delay after remove
        }
    }

    currentWriteFile = file1Path;
    currentReadFile = file2Path; 
    saveState();
    DEBUG("LogManager [" + logName + "] state reset after clear.");
}



// void LogManager::processAndSendData() {
//     DEBUG("LogManager [" + logName + "] processAndSendData called.");
    
//     if (processingInProgress) {
//         DEBUG("  - Exiting: LogManager [" + logName + "] is already processing.");
//         return;
//     }
//     if (!spiMutexPtr) {
//         DEBUG("LogManager [" + logName + "] ERROR: No mutex pointer!");
//         return;
//     }
    
//     processingInProgress = true; 

//     // --- 1. ACQUIRE MUTEX for file operations ---
//     DEBUG("LogManager [" + logName + "] Attempting to acquire mutex...");
//     unsigned long mutexWaitStart = millis();
    
//     if (xSemaphoreTake(*spiMutexPtr, pdMS_TO_TICKS(LOG_MUTEX_TIMEOUT_MS)) != pdTRUE) {
//         DEBUG("LogManager [" + logName + "] Failed to acquire mutex for file check.");
//         processingInProgress = false;
//         return;
//     }
    
//     unsigned long mutexWaitTime = millis() - mutexWaitStart;
//     DEBUG("LogManager [" + logName + "] Mutex acquired in " + String(mutexWaitTime) + "ms");

//     // --- 2. CHECK READ FILE, SWITCH IF NEEDED ---
//     if (!SD.exists(currentReadFile.c_str())) {
//         DEBUG("  - Read file missing, switching files.");
//         switchFiles(); // Safe, holds mutex
        
//         // *** CRITICAL FIX: DO NOT RETURN ***
//         // Instead, immediately check if the *new* read file exists.
//         if (!SD.exists(currentReadFile.c_str())) {
//             DEBUG("  - New read file is also missing. Exiting.");
//             xSemaphoreGive(*spiMutexPtr);
//             processingInProgress = false;
//             return;
//         }
//     }

//     File readFile = SD.open(currentReadFile.c_str(), FILE_READ);
//     if (!readFile) {
//         DEBUG("  - Exiting: FAILED to open read file: " + currentReadFile);
//         xSemaphoreGive(*spiMutexPtr);
//         processingInProgress = false;
//         return;
//     }

//     if (readFile.size() == 0) {
//         readFile.close();
//         DEBUG("  - Read file is empty. Deleting and switching.");
//         SD.remove(currentReadFile.c_str());
//         delay(10); // Settle time
//         switchFiles(); // Safe, holds mutex

//         // *** CRITICAL FIX: DO NOT RETURN ***
//         // We switched, so now we must check the *new* file.
//         DEBUG("  - Re-checking after empty file switch...");
        
//         // Re-check existence of the new read file
//         if (!SD.exists(currentReadFile.c_str())) {
//             DEBUG("  - New read file is missing. Exiting.");
//             xSemaphoreGive(*spiMutexPtr);
//             processingInProgress = false;
//             return;
//         }
        
//         // Try to open the new read file
//         readFile = SD.open(currentReadFile.c_str(), FILE_READ);
//         if (!readFile || readFile.size() == 0) {
//              if(readFile) readFile.close();
//              DEBUG("  - New read file is also empty or failed to open. Exiting.");
//              xSemaphoreGive(*spiMutexPtr);
//              processingInProgress = false;
//              return;
//         }
//     }

//     // --- 3. PROCEED WITH BATCH READ (if file is valid) ---
//     DEBUG("LogManager [" + logName + "] Processing offline data from: " + currentReadFile);

//     const int MAX_RECORDS_PER_BATCH = 10; 
//     bool fileFullyProcessed = false;
//     bool anySendFailed = false; 
//     String line;
    
//     while (readFile.available() && !anySendFailed) {
//         DynamicJsonDocument batchDoc(4096);
//         JsonArray dataArray;
//         String payload;
//         bool batchHasData = false;

//         // Setup JSON structure
//         if (logName == "Energy") {
//             dataArray = batchDoc.to<JsonArray>(); 
//         } else {
//             batchDoc["slave_id"] = 1; 
//             if (devsbotInstance) {
//                 batchDoc["gateway_api_id"] = devsbotInstance->getAuthToken();
//             }
//             if (logName == "DI_Status") {
//                 dataArray = batchDoc.createNestedArray("DIStatusOffline");
//             }
//         }
        
//         // Fill the batch (while still holding mutex)
//         while (readFile.available() && dataArray.size() < MAX_RECORDS_PER_BATCH) {
//             line = readFile.readStringUntil('\n');
//             line.trim();
            
//             if (line.length() > 0) {
//                 if (logName == "Energy" && line.startsWith("[")) {
//                     DynamicJsonDocument lineDoc(1024);
//                     if (deserializeJson(lineDoc, line) == DeserializationError::Ok) {
//                         dataArray.add(lineDoc.as<JsonArray>());
//                         batchHasData = true;
//                     }
//                 } else if (line.startsWith("{")) {
//                     DynamicJsonDocument lineDoc(512);
//                     if (deserializeJson(lineDoc, line) == DeserializationError::Ok) {
//                         dataArray.add(lineDoc.as<JsonObject>());
//                         batchHasData = true;
//                     }
//                 }
//             }
//         } 
        
//         if (dataArray.size() >= MAX_RECORDS_PER_BATCH) {
//             DEBUG("  - Batch record limit (" + String(MAX_RECORDS_PER_BATCH) + ") reached.");
//         }

//         fileFullyProcessed = !readFile.available();
        
//         // --- 4. RELEASE MUTEX *BEFORE* network operation ---
//         DEBUG("LogManager [" + logName + "] Releasing mutex before network call...");
//         xSemaphoreGive(*spiMutexPtr);
//         DEBUG("LogManager [" + logName + "] Mutex released. Proceeding with network operation...");
        
//         // --- 5. PROCESS BATCH (NO MUTEX HELD) ---
//         if (batchHasData) {
//             serializeJson(batchDoc, payload);
            
//             DEBUG("  - Calling sendDataCallback for " + logName + " (Records: " + String(dataArray.size()) + ")");
//             bool sendSuccess = sendDataCallback(payload); // <-- NO MUTEX HELD HERE
            
//             if (!sendSuccess) {
//                 DEBUG("  - FAILED to send batch. Will retry file later.");
//                 anySendFailed = true;
//             } else {
//                 DEBUG("  - Batch send SUCCESS.");
//             }
//         }
        
//         // --- 6. RE-ACQUIRE MUTEX if continuing loop ---
//         if (!fileFullyProcessed && !anySendFailed) {
//             if (xSemaphoreTake(*spiMutexPtr, pdMS_TO_TICKS(LOG_MUTEX_TIMEOUT_MS)) != pdTRUE) {
//                 DEBUG("  - Failed to re-acquire mutex for next batch. Aborting.");
//                 readFile.close();
//                 processingInProgress = false;
//                 return;
//             }
//         }
//     } // End batch loop
    
//     // --- 7. FINAL CLEANUP (file deletion if successful) ---
//     readFile.close(); // Close file handle first
    
//     if (!anySendFailed && fileFullyProcessed) {
//         // Need mutex for file deletion
//         if (xSemaphoreTake(*spiMutexPtr, pdMS_TO_TICKS(LOG_MUTEX_TIMEOUT_MS)) == pdTRUE) {
//             DEBUG("  - File fully processed. Deleting file: " + currentReadFile);
//             if (SD.remove(currentReadFile.c_str())) {
//                 DEBUG("  - File deleted successfully.");
//                 delay(10); // Settle time
//             } else {
//                 DEBUG("  - FAILED to delete file after processing!");
//             }
//             switchFiles();
//             xSemaphoreGive(*spiMutexPtr);
//         } else {
//             DEBUG("  - Failed to get mutex for file cleanup.");
//         }
//     } else if (anySendFailed) {
//         DEBUG("  - Send failed, file retained: " + currentReadFile);
//     }

//     processingInProgress = false;
//     DEBUG("LogManager [" + logName + "] processAndSendData finished.");
// }


void LogManager::processAndSendData() {
    DEBUG("LogManager [" + logName + "] processAndSendData called.");
    
    if (processingInProgress) {
        DEBUG("  - Exiting: Already processing.");
        return;
    }
    if (!spiMutexPtr) {
        DEBUG("LogManager [" + logName + "] ERROR: No mutex pointer!");
        return;
    }
    
    processingInProgress = true; 

    // --- 1. ACQUIRE MUTEX ---
    DEBUG("LogManager [" + logName + "] Acquiring mutex...");
    unsigned long mutexWaitStart = millis();
    
    if (xSemaphoreTake(*spiMutexPtr, pdMS_TO_TICKS(LOG_MUTEX_TIMEOUT_MS)) != pdTRUE) {
        DEBUG("LogManager [" + logName + "] Failed to acquire mutex.");
        processingInProgress = false;
        return;
    }
    
    unsigned long mutexWaitTime = millis() - mutexWaitStart;
    DEBUG("LogManager [" + logName + "] Mutex acquired (" + String(mutexWaitTime) + "ms)");

    // --- 2. FIND VALID READ FILE ---
    int maxSwitchAttempts = 2; // Prevent infinite loop
    for (int attempt = 0; attempt < maxSwitchAttempts; attempt++) {
        
        if (!SD.exists(currentReadFile.c_str())) {
            DEBUG("  - Read file missing: " + currentReadFile);
            
            if (attempt < maxSwitchAttempts - 1) {
                switchFiles();
                DEBUG("  - Switched to: " + currentReadFile + " (attempt " + String(attempt + 1) + ")");
                continue; // Try new file
            } else {
                DEBUG("  - No valid files found after switching. Exiting.");
                xSemaphoreGive(*spiMutexPtr);
                processingInProgress = false;
                return;
            }
        }
        
        // File exists, try to open it
        File readFile = SD.open(currentReadFile.c_str(), FILE_READ);
        if (!readFile) {
            DEBUG("  - FAILED to open: " + currentReadFile);
            xSemaphoreGive(*spiMutexPtr);
            processingInProgress = false;
            return;
        }

        // Check if file is empty
        if (readFile.size() == 0) {
            readFile.close();
            DEBUG("  - File is empty, deleting: " + currentReadFile);
            SD.remove(currentReadFile.c_str());
            delay(10);
            
            if (attempt < maxSwitchAttempts - 1) {
                switchFiles();
                DEBUG("  - Switched after empty file (attempt " + String(attempt + 1) + ")");
                continue; // Try new file
            } else {
                DEBUG("  - All files empty. Exiting.");
                xSemaphoreGive(*spiMutexPtr);
                processingInProgress = false;
                return;
            }
        }

        // --- 3. VALID FILE FOUND - PROCESS IT ---
        DEBUG("LogManager [" + logName + "] Processing: " + currentReadFile + " (" + String(readFile.size()) + " bytes)");
        
        const int MAX_RECORDS_PER_BATCH = 10;
        bool fileFullyProcessed = false;
        bool anySendFailed = false;
        
        while (readFile.available() && !anySendFailed) {
            // Prepare batch JSON
            DynamicJsonDocument batchDoc(8192);
            JsonArray dataArray;
            
            if (logName == "Energy") {
                dataArray = batchDoc.to<JsonArray>();
            } else {
                batchDoc["slave_id"] = 1;
                if (devsbotInstance) {
                    batchDoc["gateway_api_id"] = devsbotInstance->getAuthToken();
                }
                if (logName == "DI_Status") {
                    dataArray = batchDoc.createNestedArray("DIStatusOffline");
                }
            }
            
            // Read batch (while holding mutex)
            int recordsRead = 0;
            while (readFile.available() && recordsRead < MAX_RECORDS_PER_BATCH) {
                String line = readFile.readStringUntil('\n');
                line.trim();
                
                if (line.length() > 0) 
                {
                    // if (logName == "Energy" && line.startsWith("[")) {
                    //     DynamicJsonDocument lineDoc(1024);
                    //     if (deserializeJson(lineDoc, line) == DeserializationError::Ok) {
                    //         dataArray.add(lineDoc.as<JsonArray>());
                    //         recordsRead++;
                    //     }
                    // } else if (line.startsWith("{")) {
                    //     DynamicJsonDocument lineDoc(512);
                    //     if (deserializeJson(lineDoc, line) == DeserializationError::Ok) {
                    //         dataArray.add(lineDoc.as<JsonObject>());
                    //         recordsRead++;
                    //     }
                    // }
                                    // Try to deserialize the line
                DynamicJsonDocument lineDoc(1024);
                DeserializationError error = deserializeJson(lineDoc, line);

                if (error == DeserializationError::Ok) {
                    bool added = false;
                    if (logName == "Energy" && line.startsWith("[")) {
                         added = dataArray.add(lineDoc.as<JsonArray>());
                    } else if (line.startsWith("{")) {
                         added = dataArray.add(lineDoc.as<JsonObject>());
                    }
                    
                    if (added) {
                        recordsRead++;
                    } else {
                        DEBUG("  - WARNING: batchDoc memory full! Stopped reading early.");
                        break; // Stop reading if we can't add more to JSON
                    }
                } else {
                     DEBUG("  - WARNING: Failed to parse line in file: " + line);
                }
                }
            }
            
            fileFullyProcessed = !readFile.available();
            
            // --- 4. RELEASE MUTEX BEFORE NETWORK CALL ---
            DEBUG("  - Read " + String(recordsRead) + " records. Releasing mutex for network...");
            xSemaphoreGive(*spiMutexPtr);
            
            // --- 5. SEND DATA (NO MUTEX HELD) ---
            // if (recordsRead > 0) 
            // {
            //     String payload;
            //     serializeJson(batchDoc, payload);
                
            //     DEBUG("  - Sending batch (" + String(dataArray.size()) + " records)...");
            //     bool sendSuccess = sendDataCallback(payload);
                
            //     if (!sendSuccess) {
            //         DEBUG("  -  Send FAILED. File will be retried later.");
            //         anySendFailed = true;
            //     } else {
            //         DEBUG("  -  Send SUCCESS.");
            //     }
            // }

            // FIX 2: Only send if we actually added data to the array
            if (dataArray.size() > 0) 
            {
                String payload;
                serializeJson(batchDoc, payload);
                
                DEBUG("  - Sending batch (" + String(dataArray.size()) + " records)...");
                bool sendSuccess = sendDataCallback(payload);
                
                if (!sendSuccess) {
                    DEBUG("  -  Send FAILED. File will be retried later.");
                    anySendFailed = true;
                } else {
                    DEBUG("  -  Send SUCCESS.");
                }
            } else if (recordsRead > 0) {
                // This happens if records were read but failed to add to JSON (should be caught above, but good safety)
                DEBUG("  - ERROR: Records read but JSON array is empty. Skipping send.");
            }
            
            // --- 6. RE-ACQUIRE MUTEX if continuing ---
            if (!fileFullyProcessed && !anySendFailed) {
                if (xSemaphoreTake(*spiMutexPtr, pdMS_TO_TICKS(LOG_MUTEX_TIMEOUT_MS)) != pdTRUE) {
                    DEBUG("  - Failed to re-acquire mutex. Aborting.");
                    readFile.close();
                    processingInProgress = false;
                    return;
                }
            }
        } // End batch loop
        
        // --- 7. CLEANUP ---
        readFile.close();
        
        if (!anySendFailed && fileFullyProcessed) {
            // Need mutex for file deletion
            if (xSemaphoreTake(*spiMutexPtr, pdMS_TO_TICKS(LOG_MUTEX_TIMEOUT_MS)) == pdTRUE) {
                DEBUG("  -  File fully sent. Deleting: " + currentReadFile);
                if (SD.remove(currentReadFile.c_str())) {
                    delay(10);
                    DEBUG("  - File deleted successfully.");
                } else {
                    DEBUG("  -  Failed to delete file!");
                }
                switchFiles();
                xSemaphoreGive(*spiMutexPtr);
            }
        } else if (anySendFailed) {
            DEBUG("  -  Send failed. File retained: " + currentReadFile);
        }
        
        processingInProgress = false;
        DEBUG("LogManager [" + logName + "] Processing complete.");
        return; // Success - exit function
        
    } // End file search loop
    
    // Should never reach here
    xSemaphoreGive(*spiMutexPtr);
    processingInProgress = false;
}