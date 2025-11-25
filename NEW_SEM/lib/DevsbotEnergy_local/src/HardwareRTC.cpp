#include "HardwareRTC.h"
#include "DebugMacro.h"

// Define the global instance that will be accessible from other files.
HardwareRTC rtcManager;

HardwareRTC::HardwareRTC() : isInitialized(false) {}

// bool HardwareRTC::init() {
//     // Prevent re-initialization if already done.
//     if (isInitialized) {
//         return true;
//     }

//     DEBUG("Initializing Hardware RTC (DS3231)...");
//     if (!rtc.begin()) {
//         DEBUG("FATAL: Couldn't find RTC module! Timestamps will be incorrect.");
//         isInitialized = false;
//         return false;
//     }

//     // This is a standard safety check. If the RTC has lost power (e.g., battery died),
//     // it sets the time to the date and time this code was compiled. This ensures you
//     // always have a reasonably accurate starting point.
//     if (rtc.lostPower()) {
//         DEBUG("RTC lost power, setting time to compile time.");
//         rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
//     }

//     isInitialized = true;
//     DEBUG("Hardware RTC initialized successfully.");
//     DEBUG("Current RTC Time: " + getTimestamp());
//     return true;
// }

/**
 * @brief Initializes the RTC, now with NTP auto-sync.
 */
// bool HardwareRTC::init() {
//     if (isInitialized) {
//         return true;
//     }

//     DEBUG("Initializing Hardware RTC (DS3231)...");
//     if (!rtc.begin()) {
//         DEBUG("FATAL: Couldn't find RTC module! Timestamps will be incorrect.");
//         isInitialized = false;
//         return false;
//     }
    
//     isInitialized = true;

//     if (WiFi.status() == WL_CONNECTED) {
//         struct tm timeinfo;
//         if (syncWithNTP(timeinfo)) {
//             DateTime ntpTime(timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
//                              timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);

//             DateTime rtcTime = rtc.now();

//             long delta;
//             uint32_t ntp_ts = ntpTime.unixtime();
//             uint32_t rtc_ts = rtcTime.unixtime();
//             if (ntp_ts > rtc_ts) {
//                 delta = ntp_ts - rtc_ts;
//             } else {
//                 delta = rtc_ts - ntp_ts;
//             }

//             if (delta > 5) {
//                 DEBUG("RTC time is out of sync by " + String(delta) + " seconds. Updating hardware clock...");
//                 rtc.adjust(ntpTime);
//             } else {
//                 DEBUG("RTC time is already in sync with network time.");
//             }
//         } else {
//              DEBUG("Falling back to existing RTC time due to NTP failure.");
//         }
//     } else {
//         DEBUG("No WiFi connection. Using existing RTC time without network verification.");
//     }

//     if (rtc.lostPower()) {
//         DEBUG("RTC lost power, setting time to compile time as a fallback.");
//         rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
//     }

//     DEBUG("Hardware RTC initialization complete.");
//     return true;
// }


bool HardwareRTC::init() {
    if (isInitialized) {
        return true;
    }

    DEBUG("Initializing Hardware RTC (DS3231)...");
    if (!rtc.begin()) {
        DEBUG("FATAL: Couldn't find RTC module! Timestamps will be incorrect.");
        isInitialized = false;
        return false;
    }
    
    if (rtc.lostPower()) {
        DEBUG("RTC lost power, setting time to compile time as a fallback.");
        rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }

    isInitialized = true;
    DEBUG("Hardware RTC initialization complete.");
    
    // Try an immediate sync just in case we are already connected (rare but possible on quick reboots)
    performNTPSync();
    
    return true;
}

// --- NEW METHOD IMPLEMENTATION ---
void HardwareRTC::performNTPSync() {
    if (!isInitialized) return;

    if (WiFi.status() == WL_CONNECTED) {
        DEBUG("[RTC] WiFi connected. Checking network time...");
        struct tm timeinfo;
        if (syncWithNTP(timeinfo)) {
            DateTime ntpTime(timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                             timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);

            DateTime rtcTime = rtc.now();
            long delta = ntpTime.unixtime() - rtcTime.unixtime();
            
            // Use absolute value for delta
            if (abs(delta) > 5) {
                DEBUG("[RTC] Time out of sync by " + String(delta) + "s. Updating DS3231...");
                rtc.adjust(ntpTime);
            } else {
                DEBUG("[RTC] Time is already in sync.");
            }
        }
    } else {
         // Optional: only log this if you want verbose debug
         // DEBUG("[RTC] Cannot sync NTP: No WiFi connection.");
    }
}

/**
 * @brief Private helper to sync time from the NTP server.
 */

bool HardwareRTC::syncWithNTP(struct tm &timeinfo) {
    DEBUG("Attempting to sync time with NTP server...");
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

    if (!getLocalTime(&timeinfo, 10000)) { // 10-second timeout
        DEBUG("--> NTP Sync FAILED. Could not obtain time from server.");
        return false;
    }
    
    DEBUG("--> NTP Sync SUCCESS. Current local time: " + String(asctime(&timeinfo)));
    return true;
}

// --- THIS IS THE FIX ---
// Implementation of the new public getter function was missing.
bool HardwareRTC::isReady() const {
    return isInitialized;
}


// String HardwareRTC::getTimestamp() {
//     if (!isInitialized) {
//         return "1970-01-01 00:00:00"; // Return default epoch on error.
//     }

//     DateTime now = rtc.now();
//     char timestampBuffer[20];
    
//     // Format the date into the exact "YYYY-MM-DD HH:MM:SS" format required by the server.
//     sprintf(timestampBuffer, "%04d-%02d-%02d %02d:%02d:%02d", 
//             now.year(), 
//             now.month(), 
//             now.day(), 
//             now.hour(), 
//             now.minute(), 
//             now.second());
            
//     return String(timestampBuffer);
// }

String HardwareRTC::getTimestamp() {
    // if (!isInitialized) {
    //     return "1970-01-01 00:00:00"; // Return default epoch on error
    // }

    DateTime localTime = rtc.now();
    uint32_t unixTime = localTime.unixtime();
    unixTime -= gmtOffset_sec; 
    DateTime utcTime(unixTime);

    char timestampBuffer[21]; // Increased size for the 'T' and 'Z'

    // --- THIS IS THE FIX ---
    // Reverted the format to use a space separator and no 'Z',
    // exactly as your server expects.
    sprintf(timestampBuffer, "%04d-%02d-%02d %02d:%02d:%02d", 
            utcTime.year(), 
            utcTime.month(), 
            utcTime.day(), 
            utcTime.hour(), 
            utcTime.minute(), 
            utcTime.second());
    // --- END OF FIX ---
            
    return String(timestampBuffer);
}
DateTime HardwareRTC::now() {
    if (!isInitialized) {
        // Return a zero-time object if not initialized.
        // FIX: Explicitly cast 0 to uint32_t to resolve the ambiguous constructor call.
        return DateTime((uint32_t)0); 
    }
    return rtc.now();
}

