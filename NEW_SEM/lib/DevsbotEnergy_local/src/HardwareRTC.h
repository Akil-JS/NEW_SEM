#ifndef HARDWARE_RTC_H
#define HARDWARE_RTC_H

#include <Arduino.h>
#include <RTClib.h> // From Adafruit
#include "time.h"      // Required for NTP functionality

/**
 * @class HardwareRTC
 * @brief Manages the DS3231 Real-Time Clock hardware.
 *
 * This class provides a centralized, singleton-like interface for initializing 
 * the RTC and retrieving accurate, formatted timestamps. This prevents the need
 * for multiple RTC objects and initialization calls across the project.
 */
class HardwareRTC {
public:
    /**
     * @brief Constructor for the RTC manager.
     */
    HardwareRTC();

    /**
     * @brief Initializes the RTC module. Connects to the hardware and sets the time if needed.
     * @return True if initialization is successful, false otherwise.
     */
    bool init();

    /**
     * @brief Gets the current time as a formatted String.
     * @return A String in "YYYY-MM-DD HH:MM:SS" format. Returns a default epoch
     * timestamp if the RTC is not initialized.
     */
    String getTimestamp();

    /**
     * @brief Gets the current time as a DateTime object for more complex time calculations.
     * @return An RTClib DateTime object.
     */
    DateTime now();

    // --- THIS IS THE FIX ---
    // A public "getter" function to safely check the initialization status.
    bool isReady() const;

    // --- NEW PUBLIC METHOD ---
    void performNTPSync(); 
    // -------------------------

private:
    RTC_DS3231 rtc;      // The underlying RTC hardware object.
    bool isInitialized;  // Flag to prevent multiple initializations.

    // --- NTP Configuration for India Standard Time (UTC+5:30) ---
    const char* ntpServer = "pool.ntp.org";
    const long gmtOffset_sec = 19800; // 5.5 hours * 3600 seconds/hour
    const int daylightOffset_sec = 0;   // India does not observe daylight saving

    /**
     * @brief Private helper function to connect to the NTP server and get the current time.
     * @param timeinfo A struct to be filled with the network time.
     * @return True on success, false on failure (e.g., timeout).
     */
    bool syncWithNTP(struct tm &timeinfo);
};

// Declare a single global instance of the RTC manager to be used across the project.
extern HardwareRTC rtcManager;

#endif // HARDWARE_RTC_H
