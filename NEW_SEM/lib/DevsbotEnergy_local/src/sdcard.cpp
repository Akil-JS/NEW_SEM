
#include "sdcard.h"
#include "DevsbotEnergyLocal.h"
#include "DebugMacro.h"

#define SD_CS_PIN   15
#define SD_CLK_PIN  14
#define SD_MOSI_PIN 13
#define SD_MISO_PIN 2

#define SD_MUTEX_TIMEOUT_MS 3000

SPIClass spi2(HSPI);
sdcard card;
extern Devsbot dBot; 

// ============================================================================
// CRITICAL FIX: Enhanced saveEnergyLog with better debugging
// ============================================================================
bool sdcard::saveEnergyLog(const String& logData) {
    if (!cardMounted) {
        DEBUG("!! SD card not mounted, cannot save energy log.");
        return false;
    }
    
    if (isProcessing) {
        DEBUG("!! SD is busy processing, energy log will retry.");
        return false;
    }
    
    DEBUG("[saveEnergyLog] Attempting to acquire mutex...");
    unsigned long startTime = millis();
    
    if (xSemaphoreTake(spiMutex, pdMS_TO_TICKS(SD_MUTEX_TIMEOUT_MS)) == pdTRUE) {
        unsigned long waitTime = millis() - startTime;
        DEBUG("[saveEnergyLog] Mutex acquired in " + String(waitTime) + "ms");
        
        // Double-check isProcessing inside mutex
        if (isProcessing) {
            DEBUG("!! SD processing started during mutex wait, aborting energy save.");
            xSemaphoreGive(spiMutex);
            return false;
        }
        
        bool saveResult = energyLogs.saveLog(logData);
        xSemaphoreGive(spiMutex);
        DEBUG("[saveEnergyLog] Mutex released.");
        
        return saveResult;
    } else {
        unsigned long waitTime = millis() - startTime;
        DEBUG("!! [saveEnergyLog] FAILED to get SPI mutex (timeout after " + String(waitTime) + "ms).");
        DEBUG("!! isProcessing = " + String(isProcessing));
        return false;
    }
}

// ============================================================================
// CRITICAL FIX: Enhanced saveStatusLog with better debugging
// ============================================================================
bool sdcard::saveStatusLog(const String& logData) {
    if (!cardMounted) {
        DEBUG("!! SD card not mounted, cannot save status log.");
        return false;
    }
    
    if (isProcessing) {
        DEBUG("!! SD is busy processing, status log will retry.");
        return false;
    }
    
    DEBUG("[saveStatusLog] Attempting to acquire mutex...");
    unsigned long startTime = millis();
    
    if (xSemaphoreTake(spiMutex, pdMS_TO_TICKS(SD_MUTEX_TIMEOUT_MS)) == pdTRUE) {
        unsigned long waitTime = millis() - startTime;
        DEBUG("[saveStatusLog] Mutex acquired in " + String(waitTime) + "ms");
        
        // Double-check isProcessing inside mutex
        if (isProcessing) {
            DEBUG("!! SD processing started during mutex wait, aborting status save.");
            xSemaphoreGive(spiMutex);
            return false;
        }
        
        bool saveResult = statusLogs.saveLog(logData);
        xSemaphoreGive(spiMutex);
        DEBUG("[saveStatusLog] Mutex released.");
        
        return saveResult;
    } else {
        unsigned long waitTime = millis() - startTime;
        DEBUG("!! [saveStatusLog] FAILED to get SPI mutex (timeout after " + String(waitTime) + "ms).");
        DEBUG("!! isProcessing = " + String(isProcessing));
        return false;
    }
}

// ============================================================================
// Constructor
// ============================================================================
sdcard::sdcard() : cardMountFail(true) { 
    cardMounted = false;
    isProcessing = false;

    spiMutex = xSemaphoreCreateMutex();
    if (spiMutex == NULL) {
        DEBUG("CRITICAL: Failed to create SPI mutex!");
    }

    energyLogs.initialize(
        &dBot,
        "Energy", 
        "/energylog1.txt", 
        "/energylog2.txt", 
        "sdEnergy",
        [&](String& payload) { return dBot.sentEnergyMeterData(payload); },
        &spiMutex
    );

    statusLogs.initialize(
        &dBot,
        "DI_Status", 
        "/DI_Statuslog1.txt", 
        "/DI_Statuslog2.txt", 
        "sdDIStatus",
        [&](String& payload) { return dBot.sendDIStatusData(payload); },
        &spiMutex
    );
}

// ============================================================================
// SD Initialization
// ============================================================================
bool sdcard::sdInit() {
    DEBUG("[SD] Attempting to initialize SD card...");
    
    if (xSemaphoreTake(spiMutex, pdMS_TO_TICKS(5000)) != pdTRUE) {
        DEBUG("[ERROR] SD Init: Failed to acquire SPI mutex (timeout)");
        cardMounted = false;
        return false;
    }
    
    DEBUG("[SD] SPI mutex acquired. Initializing SPI bus...");
    
    // *** NEW FIX: Initialize SPI Bus ONCE, as per manufacturer's example ***
    // This is the correct way to initialize the HSPI bus.
    spi2.begin(SD_CLK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
    delay(100); // Give SPI bus time to stabilize
    
    const uint32_t clockSpeeds[] = {4000000, 10000000, 20000000, 80000000}; // Added 80MHz from manufacturer
    bool mountSuccess = false;
    
    for (int attempt = 1; attempt <= 3; attempt++) {
        DEBUG("[SD] Mount Attempt " + String(attempt) + "/3...");

        for (int i = 0; i < 4; i++) {
            DEBUG("[SD] Trying clock speed: " + String(clockSpeeds[i] / 1000000) + " MHz");
            
            // We pass spi2, but SD.begin() handles the bus.
            // We do NOT call spi2.end() or spi2.begin() inside this loop.
            if (SD.begin(SD_CS_PIN, spi2, clockSpeeds[i])) {
                mountSuccess = true;
                DEBUG("[SD] SD.begin() succeeded at " + String(clockSpeeds[i] / 1000000) + " MHz");
                break;
            }
            SD.end(); // End card before retrying speed
        }
        
        if(mountSuccess) break; // Exit retry loop if successful

        if (attempt < 3) {
           DEBUG("[SD] Mount Attempt " + String(attempt) + " failed. Retrying in 1s...");
           delay(1000); // Wait 1 second before next full attempt
        }
    }
    
    if (!mountSuccess) {
        DEBUG("[ERROR] SD Card Mount Failed at all clock speeds after 3 attempts.");
        spi2.end(); // Clean up SPI bus on total failure
        cardMounted = false;
        xSemaphoreGive(spiMutex);
        return false;
    }
    
    uint8_t cardType = SD.cardType();
    if (cardType == CARD_NONE) {
        DEBUG("[ERROR] No SD card attached");
        cardMounted = false;
        SD.end();
        spi2.end();
        xSemaphoreGive(spiMutex);
        return false;
    }
    
    String cardTypeStr = (cardType == CARD_MMC) ? "MMC" :
                         (cardType == CARD_SD) ? "SDSC" :
                         (cardType == CARD_SDHC) ? "SDHC" : "UNKNOWN";
    
    DEBUG("[SD] SD card initialized successfully. Type: " + cardTypeStr);
    
    uint64_t cardSize = SD.cardSize() / (1024 * 1024);
    DEBUG("[SD] Card Size: " + String((uint32_t)cardSize) + " MB");
    
    cardMounted = true;
    xSemaphoreGive(spiMutex);
    return true;
}

// ============================================================================
// FIXED: Process functions with enhanced debugging
// ============================================================================
void sdcard::processEnergyLogQueue() {
    if (isProcessing) { 
        DEBUG("[SD] Already processing, skipping Energy queue.");
        return;
    }
    
    if (!cardMounted) {
        DEBUG("!! SD card not mounted, cannot process energy log queue.");
        return;
    }
    
    isProcessing = true;
    DEBUG("[SD] Processing Energy log queue ONLY...");
    DEBUG("[SD] isProcessing flag SET");
    
    energyLogs.processAndSendData();
    
    isProcessing = false;
    DEBUG("[SD] isProcessing flag CLEARED");
    DEBUG("[SD] Finished processing Energy log queue.");
}

void sdcard::processStatusLogQueue() {
    if (isProcessing) { 
        DEBUG("[SD] Already processing, skipping Status queue.");
        return;
    }

    if (!cardMounted) {
        DEBUG("!! SD card not mounted, cannot process status log queue.");
        return;
    }
    
    isProcessing = true;
    DEBUG("[SD] Processing DI_Status log queue ONLY...");
    DEBUG("[SD] isProcessing flag SET");
    
    statusLogs.processAndSendData();
    
    isProcessing = false;
    DEBUG("[SD] isProcessing flag CLEARED");
    DEBUG("[SD] Finished processing DI_Status log queue.");
                    Serial.println("==============================================================================");
}

void sdcard::processLogQueues() {
    if (isProcessing) {
        DEBUG("[SD] Already processing, skipping full queue processing.");
        return;
    }

    if (!cardMounted) {
        DEBUG("!! SD card not mounted, cannot process log queues.");
        return;
    }
    
    isProcessing = true;
    DEBUG("[SD] isProcessing flag SET");
    
    DEBUG("[SD] Processing Energy log queue...");
    energyLogs.processAndSendData();
    
    DEBUG("[SD] Processing DI_Status log queue...");
    statusLogs.processAndSendData();

    isProcessing = false;
    DEBUG("[SD] isProcessing flag CLEARED");
    DEBUG("[SD] Finished processing all log queues.");
}

void sdcard::clearDigitalInputLogs() {
    DEBUG("[clearDigitalInputLogs] Attempting to acquire mutex...");
    
    if (xSemaphoreTake(spiMutex, pdMS_TO_TICKS(SD_MUTEX_TIMEOUT_MS)) == pdTRUE) {
        DEBUG("[clearDigitalInputLogs] Mutex acquired.");
        
        if (!cardMounted) {
            DEBUG("!! SD card not mounted, cannot clear logs.");
        } else {
            Serial.println("Clearing Digital Input log files for a new job...");
            statusLogs.clearLogs();
        }
        
        xSemaphoreGive(spiMutex);
        DEBUG("[clearDigitalInputLogs] Mutex released.");
    } else {
        DEBUG("!! FAILED to get SPI mutex in clearDigitalInputLogs (timeout).");
    }
}