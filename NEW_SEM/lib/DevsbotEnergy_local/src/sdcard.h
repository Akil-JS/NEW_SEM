// #ifndef SDCARDRTC_H
// #define SDCARDRTC_H

// #include <SPI.h>
// #include <FS.h>
// #include <SD.h>
// #include "LogManager.h"

// class sdcard {
// public:
//     bool isProcessing;
//     LogManager energyLogs;
//     LogManager statusLogs;
//     LogManager pulseLogs;
//     // --- The Mutex now correctly belongs to the sdcard class ---
//     // SemaphoreHandle_t fileMutex;
//     // --- NEW: Add a flag to track if the SD card is mounted ---
//     bool cardMounted;

//     sdcard();
//     bool sdInit();
//     void processLogQueues();
//     // --- MODIFIED: Replaced the general clear function with a specific one ---
//     void clearDigitalInputLogs();
    
//     // --- ADD THIS NEW FUNCTION DECLARATION ---
//     void saveEnergyLog(const String& logData);

//         // Public state and methods
//     bool cardMountFail;
//     bool isBusy() const {
//         return isProcessing;
//     }
// };

// #endif



#ifndef SDCARDRTC_H
#define SDCARDRTC_H

#include <SPI.h>
#include <FS.h>
#include <SD.h>
#include "LogManager.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

class sdcard {
public:
    bool isProcessing;
    LogManager energyLogs;
    LogManager statusLogs;
    //LogManager pulseLogs;
    
    // --- STABILITY FIX: The Mutex now belongs to the sdcard class ---
    // This mutex will protect all access to the shared SPI bus.
    SemaphoreHandle_t spiMutex;
    
    bool cardMounted;

    sdcard();
    bool sdInit();
    void processLogQueues();

        // --- ADD THESE NEW FUNCTION DECLARATIONS ---
    void processEnergyLogQueue();  // This will process ONLY energy
    void processStatusLogQueue();  // This will process ONLY status
    void processPulseLogQueue();   // This will process ONLY pulse
    // --- END OF ADDITION ---
    void clearDigitalInputLogs();
    
    bool saveEnergyLog(const String& logData);
    bool saveStatusLog(const String& logData);

    bool cardMountFail;
    bool isBusy() const {
        return isProcessing;
    }
};

#endif
