#include <readmodbusdata.h>
#include "sdcard.h"
#include "DebugConfig.h"
#include "DebugMacro.h"
#include "HardwareRTC.h" 

/* ==========  QC WEB SERVER INCLUDES  ========== */
#include <AsyncTCP.h>
#include "QCWebServer.h"
/* ============================================== */

// =========================================================================
// === TASK HANDLES - All declared at top ===
// =========================================================================
TaskHandle_t sdCardTaskHandle = NULL;            // SD processing task
TaskHandle_t mainAppTaskHandle = NULL;           // Main application task
TaskHandle_t buttonTaskHandle = NULL;            // Button read task (Madura Steel)

// =========================================================================
// === TIMER HANDLES ===
// =========================================================================
TimerHandle_t myTimer1 = NULL;  // Energy timer
TimerHandle_t myTimer2 = NULL;  // SD timer

// =========================================================================
// === OTHER GLOBALS ===
// =========================================================================
unsigned long previousmillis = 0;
unsigned long previousmillis1 = 0;
unsigned long currentmillis;
bool previousmillisFlag = 0;

int modbusTimeOut = 2000;

const uint8_t ledPins[4] = {13, 23, 25, 4};
const uint8_t buttonPins[4] = {26, 27, 22, 21};

bool ledPinsState[4] = {0};
bool buttonPinsStatus[4] = {1, 1, 1, 1};
uint64_t debounceDelay = 5000;
uint64_t lastDebounce = 0;
uint64_t lastDebounce1 = 0;
bool firstGoVal = 0;

Preferences ledState;

extern readMeterData energy;
extern sdcard card;

/* ==========  QC WEB SERVER GLOBALS  ========== */
AsyncWebServer  server(80);
AsyncWebSocket  ws("/ws");
QCWebServer     qc(&server, &ws);
/* ============================================ */

// =========================================================================
// === LED/EEPROM FUNCTIONS ===
// =========================================================================
void writeLedStatusToEEPROM(uint8_t tempVal)
{
    uint8_t ledToTurnOn = tempVal;
    uint8_t ledToTurnOff = (tempVal % 2 == 0) ? (tempVal + 1) : (tempVal - 1);

    if (ledToTurnOn < 4 && ledToTurnOff < 4) {
        DEBUG("[LED] Button " + String(tempVal) + ": Turning ON LED " + String(ledToTurnOn) + ", Turning OFF LED " + String(ledToTurnOff));
        ledPinsState[ledToTurnOn] = 1;
        ledPinsState[ledToTurnOff] = 0;
    } else {
        DEBUG("[ERROR] Invalid LED index in writeLedStatusToEEPROM for tempVal=" + String(tempVal));
        return;
    }

    ledState.begin("ledState", false);
    DEBUG("[LED] Saving LED states to EEPROM:");
    for (uint8_t i = 0; i < 4; i++) {
        ledState.putBool(String("LED" + String(i)).c_str(), ledPinsState[i]);
        DEBUG("  - LED" + String(i) + " = " + (ledPinsState[i] ? "ON" : "OFF"));
    }
    ledState.end();
}

void readLedStatusEEPROM()
{
    ledState.begin("ledState", false);
    DEBUG("[LED] Reading LED states from EEPROM...");
    
    for (uint8_t i = 0; i < (sizeof(ledPins) / sizeof(ledPins[0])); i++)
    {
        ledPinsState[i] = ledState.getBool(String("LED" + String(i)).c_str(), 0);
        DEBUG("  - Pin[" + String(i) + "] = " + (ledPinsState[i] ? "ON" : "OFF"));
        digitalWrite(ledPins[i], ledPinsState[i]);
        buttonPinsStatus[i] = !ledPinsState[i];
    }

    if (!ledState.getBool("checkState", 0))
    {
        DEBUG("[LED] First-time setup detected. Writing default states.");
        ledState.putBool("checkState", 1);
        ledState.putBool("firstgo", 1);
        firstGoVal = ledState.getBool("firstgo", 0);
        
        ledPinsState[1] = 1;
        ledPinsState[3] = 1;
        ledState.putBool("LED1", ledPinsState[1]);
        ledState.putBool("LED3", ledPinsState[3]);
        
        buttonPinsStatus[1] = 0;
        buttonPinsStatus[3] = 0;
        DEBUG("[LED] First Go: " + String(firstGoVal) + " (LEDs 1 & 3 ON by default)");
    }
    else
    {
        firstGoVal = ledState.getBool("firstgo", 0);
    }

    ledState.end();
}

// =========================================================================
// === BUTTON READ TASK (Madura Steel) ===
// =========================================================================
void buttonReadTask(void * parameter)
{
    DEBUG("[Task] ButtonReadTask started on Core 0.");
    TickType_t lastStackCheck = 0;

    for(;;)
    {
        if (xTaskGetTickCount() - lastStackCheck > pdMS_TO_TICKS(30000)) {
            UBaseType_t stackHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
            DEBUG("[Task] ButtonReadTask Stack HWM: " + String(stackHighWaterMark) + " bytes free");
            lastStackCheck = xTaskGetTickCount();
        }

        if (dBot.wifiStatus == 1)
        {
            // Button 0 - Start S1
            if (buttonPinsStatus[0] && digitalRead(buttonPins[0]) == LOW && (millis() - lastDebounce) > debounceDelay)
            {
                DEBUG("[Button] Start S1 PRESSED.");
                if (dBot.postDataToServer(1, 20, 1))
                {
                    writeLedStatusToEEPROM(0);
                    buttonPinsStatus[0] = 0;
                    buttonPinsStatus[1] = 1;
                    digitalWrite(ledPins[0], 1);
                    digitalWrite(ledPins[1], 0);
                    lastDebounce = millis();
                    DEBUG("[Button] Start S1 SUCCESS.");
                }
            }
            // Button 1 - Stop S1
            else if (buttonPinsStatus[1] && digitalRead(buttonPins[1]) == LOW && (millis() - lastDebounce) > debounceDelay)
            {
                DEBUG("[Button] Stop S1 PRESSED.");
                if (dBot.postDataToServer(1, 20, 0))
                {
                    writeLedStatusToEEPROM(1);
                    buttonPinsStatus[1] = 0;
                    buttonPinsStatus[0] = 1;
                    digitalWrite(ledPins[0], 0);
                    digitalWrite(ledPins[1], 1);
                    lastDebounce = millis();
                    DEBUG("[Button] Stop S1 SUCCESS.");
                }
            }
            // Button 2 - Start S2
            else if (buttonPinsStatus[2] && digitalRead(buttonPins[2]) == LOW && (millis() - lastDebounce1) > debounceDelay)
            {
                DEBUG("[Button] Start S2 PRESSED.");
                if (dBot.postDataToServer(2, 21, 1))
                {
                    writeLedStatusToEEPROM(2);
                    buttonPinsStatus[2] = 0;
                    buttonPinsStatus[3] = 1;
                    digitalWrite(ledPins[2], 1);
                    digitalWrite(ledPins[3], 0);
                    lastDebounce1 = millis();
                    DEBUG("[Button] Start S2 SUCCESS.");
                }
            }
            // Button 3 - Stop S2
            else if (buttonPinsStatus[3] && digitalRead(buttonPins[3]) == LOW && (millis() - lastDebounce1) > debounceDelay)
            {
                DEBUG("[Button] Stop S2 PRESSED.");
                if (dBot.postDataToServer(2, 21, 0))
                {
                    writeLedStatusToEEPROM(3);
                    buttonPinsStatus[3] = 0;
                    buttonPinsStatus[2] = 1;
                    digitalWrite(ledPins[2], 0);
                    digitalWrite(ledPins[3], 1);
                    lastDebounce1 = millis();
                    DEBUG("[Button] Stop S2 SUCCESS.");
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// =========================================================================
// === ENERGY READ TASK (Timer-driven, always runs) ===
// =========================================================================
void readEnergyData(void *pvParameters) {
    DEBUG("[Task] EnergyReadTask initialized and waiting...");
    
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        Serial.println("******************************************************************************");
        DEBUG("[Task] EnergyReadTask: Woke up.");
        
        if (energy.readDataFlag == 0 && dBot.meterAddInSpiff) {
            energy.readModbusJson(dBot.numSlave);
        } else {
            DEBUG("[Task] EnergyReadTask: Skipping (flag=" + String(energy.readDataFlag) + 
                  ", meterConfigured=" + String(dBot.meterAddInSpiff) + ")");
        }
        
        DEBUG("[Task] EnergyReadTask: Completed.");
        Serial.println("******************************************************************************");
    }
}

// =========================================================================
// === SD PROCESSING TASK (Timer-driven, conditional) ===
// =========================================================================
void sdProcessingTask(void *pvParameters) {
    DEBUG("[Task] SDProcessTask initialized and waiting...");
    
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        Serial.println("==============================================================================");
        DEBUG("[Task] SDProcessTask: Woke up.");
        Serial.println("==============================================================================");
        bool cardOK = (SD.cardType() != CARD_NONE);
        bool wifiOK = dBot.wifiStatus;
        bool busyOK = !card.isBusy();

        DEBUG("  - SD Card: " + String(cardOK ? "OK" : "FAIL"));
        DEBUG("  - WiFi:    " + String(wifiOK ? "OK" : "FAIL"));
        DEBUG("  - Busy:    " + String(card.isBusy() ? "YES" : "NO"));

        if (cardOK && wifiOK && busyOK) {
            DEBUG("[Task] SDProcessTask: Processing queues...");
            card.processLogQueues();
        } else {
            DEBUG("[Task] SDProcessTask: Skipping (conditions not met).");
        }
        Serial.println("==============================================================================");
        DEBUG("[Task] SDProcessTask: Finished.");
        Serial.println("==============================================================================");
    }
}

// =========================================================================
// === TIMER CALLBACKS (Lightweight notification senders) ===
// =========================================================================
void timerCallback1(TimerHandle_t xTimer) {
    if (dataTaskHandle != NULL) {
        DEBUG("[Timer] EnergyTimer: Notifying EnergyReadTask.");
        xTaskNotifyGive(dataTaskHandle);
    }
}

// void timerCallback2(TimerHandle_t xTimer) {
//     if (sdCardTaskHandle != NULL) {
//         DEBUG("[Timer] SDTimer: Notifying SDProcessTask.");
//         xTaskNotifyGive(sdCardTaskHandle);
//     }
// }

// =========================================================================
// === MAIN APPLICATION TASK (Core 1) ===
// =========================================================================
void mainAppTask(void * parameter) {
    DEBUG("[Task] MainAppTask started on Core 1.");
    
    for(;;)
    {
        // Handle core Devsbot logic
        dBot.Loop();

        // Handle QC web server
        qc.loop();

        // Yield to other tasks
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// =========================================================================
// === SETUP FUNCTION ===
// =========================================================================
void setup()
{
    Serial.begin(115200);
    delay(200);
    
    // --- Initialize Debugging First ---
    DebugConfig::init();
    Serial.printf("\n========================================\n");
    Serial.printf("        ESP32 Gateway Booting Up        \n");
    Serial.printf("========================================\n");
    Serial.printf("Runtime Debug: SERIAL=%s, WEB=%s, BOTH=%s\n",
                  DebugConfig::DEBUG_SERIAL ? "ON" : "OFF",
                  DebugConfig::DEBUG_WEB ? "ON" : "OFF",
                  DebugConfig::DEBUG_BOTH ? "ON" : "OFF");

    // --- Initialize Core Systems ---
    rtcManager.init();
    dBot.begin();
    energy.serialInit();

    // --- Configure Madura Steel (if enabled) ---
    if (dBot.maduraSteelOn)
    {
        DEBUG("[Setup] Configuring Madura Steel mode...");
        for (uint8_t i = 0; i < sizeof(ledPins) / sizeof(ledPins[0]); i++)
        {
            pinMode(ledPins[i], OUTPUT);
            pinMode(buttonPins[i], INPUT);
            digitalWrite(ledPins[i], LOW);
        }
        readLedStatusEEPROM();

        if (firstGoVal == 1)
        {
            DEBUG("[Setup] First Go: Setting default LED states.");
            digitalWrite(ledPins[1], 1);
            digitalWrite(ledPins[3], 1);
            buttonPinsStatus[1] = 0;
            buttonPinsStatus[3] = 0;
        }

        DEBUG("[Setup] Creating Button Read Task on Core 0...");
        xTaskCreatePinnedToCore(
            buttonReadTask,
            "ButtonReadTask",
            1024 * 6,  // Reduced from 8KB to 6KB (sufficient for GPIO)
            NULL,
            1,
            &buttonTaskHandle,
            0
        );
    }

    // =====================================================
    // ENERGY TASK - ALWAYS RUNS (Independent of SD)
    // =====================================================
    DEBUG("[Setup] Creating Energy Read Task on Core 0...");
    BaseType_t energyTaskResult = xTaskCreatePinnedToCore(
        readEnergyData,
        "EnergyReadTask",
        1024 * 6,
        NULL,
        1,
        &dataTaskHandle,
        0
    );
    
    if (energyTaskResult != pdPASS) {
        DEBUG("[ERROR] Failed to create EnergyReadTask!");
    } else {
        DEBUG("  - ✓ EnergyReadTask created");
        
        myTimer1 = xTimerCreate(
            "EnergyTimer",
            pdMS_TO_TICKS(1000 * dBot.completeJsonReading),
            pdTRUE,
            NULL,
            timerCallback1
        );
        
        if (myTimer1 && xTimerStart(myTimer1, 0) == pdPASS) {
            DEBUG("  - ✓ EnergyTimer started (" + String(dBot.completeJsonReading) + "s)");
        } else {
            DEBUG("[ERROR] Failed to start EnergyTimer!");
        }
    }

    // =====================================================
    // SD TASK - CONDITIONAL (Only if enabled)
    // =====================================================
    if (dBot.storeDataToSd)
    {
        DEBUG("[Setup] SD card logging ENABLED.");
        
        bool sdInitSuccess = false;
        const int maxRetries = 3;
        
        for (int attempt = 1; attempt <= maxRetries && !sdInitSuccess; attempt++) {
            DEBUG("[Setup] SD Init Attempt " + String(attempt) + "/" + String(maxRetries));
            
            if (attempt > 1) {
                int delayMs = 500 * (1 << (attempt - 2));
                DEBUG("[Setup] Retry delay: " + String(delayMs) + "ms");
                delay(delayMs);
            }
            
            if (card.sdInit()) {
                sdInitSuccess = true;
                DEBUG("[Setup] ✓ SD Card OK (attempt " + String(attempt) + ")");
                break;
            } else {
                DEBUG("[Setup] ✗ SD Init failed (attempt " + String(attempt) + ")");
            }
        }
        
        if (sdInitSuccess) {
            DEBUG("[Setup] Creating SD Process Task on Core 0...");
            BaseType_t sdTaskResult = xTaskCreatePinnedToCore(
                sdProcessingTask,
                "SDProcessTask",
                1024 * 8,
                NULL,
                1,
                &sdCardTaskHandle,
                0
            );
            
            if (sdTaskResult != pdPASS) {
                DEBUG("[ERROR] Failed to create SDProcessTask!");
            } else {
                DEBUG("  - ✓ SDProcessTask created");
                
                // myTimer2 = xTimerCreate(
                //     "SDTimer",
                //     pdMS_TO_TICKS(1000 * 180),
                //     pdTRUE,
                //     NULL,
                //     timerCallback2
                // );
                
                // if (myTimer2 && xTimerStart(myTimer2, 0) == pdPASS) {
                //     DEBUG("  - ✓ SDTimer started (180s)");
                // } else {
                //     DEBUG("[ERROR] Failed to start SDTimer!");
                // }
                DEBUG("  - SDProcessTask will be triggered by Heartbeat every ~3 mins.");
            }
        } else {
            DEBUG("[ERROR] ════════════════════════════════════════");
            DEBUG("[ERROR] SD Card FAILED after " + String(maxRetries) + " attempts!");
            DEBUG("[ERROR] Energy monitoring continues WITHOUT SD.");
            DEBUG("[ERROR] ════════════════════════════════════════");
        }
    } else {
        DEBUG("[Setup] SD logging DISABLED by config.");
    }

    // --- Setup QC Web Server ---
    DEBUG("[Setup] Initializing QC Web Server...");
    int* dioPins; String* dioModes; int dioCnt;
    uint8_t* slaveIds; int slaveCnt;
    uint16_t* regs; int regCnt;
    uint32_t baud; uint8_t dbits, par, sb;

    dBot.getDIOConfiguration(dioPins, dioModes, dioCnt);
    dBot.getRS485Configuration(slaveIds, slaveCnt, regs, regCnt, baud, dbits, par, sb);

    qc.setDIOConfiguration(dioPins, dioModes, dioCnt);
    qc.setRS485Configuration(slaveIds, slaveCnt, regs, regCnt, baud, dbits, par, sb, 17, 16, 4, 2);
    qc.setDevsbotReference(&dBot);
    qc.begin();
    QCWebServer::activeInstance = &qc;
    server.begin();

    String qcServerAddress = (WiFi.isConnected()) ? WiFi.localIP().toString() : "192.168.4.1";
    DEBUG("[Setup] QC Server ready: http://" + qcServerAddress);

    // --- Create Main Application Task ---
    DEBUG("[Setup] Creating Main Application Task on Core 1...");
    xTaskCreatePinnedToCore(
        mainAppTask,
        "MainAppTask",
        1024 * 16,
        NULL,
        2,
        &mainAppTaskHandle,
        1
    );

    DEBUG("\n========================================");
    DEBUG("         Setup Complete - Running         ");
    DEBUG("========================================");
}

// =========================================================================
// === LOOP (Empty - all logic in tasks) ===
// =========================================================================
void loop() {
    // Intentionally empty - all work done in FreeRTOS tasks
    vTaskDelay(portMAX_DELAY);
}