/**
 * @file main.cpp
 * @brief Entry point for goTapFirmware – ESP32-S3 tap tower display.
 *
 * Hardware: JC8048W550  (800×480 IPS, ST7262 RGB, GT911 touch, 8 MB PSRAM)
 * Display lib: esp32_smartdisplay (handles ST7262 + GT911 initialisation)
 *
 * Flow:
 *   setup() → WiFi → display + LVGL → UI init → first API fetch
 *   loop()  → lv_timer_handler() → 60-s polling → WiFi reconnect
 */

#include <Arduino.h>
#include <esp32_smartdisplay.h>  // Handles ST7262 + GT911 for JC8048W550
#include <lvgl.h>

#include "gotap_config.h"   // MUST be included before any module that uses its defines
#include "settings.h"
#include "wifi_manager.h"
#include "api_client.h"
#include "ota_manager.h"
#include "ui.h"
#include <time.h>

// ------------------------------------------------------------------
// Timing & State
// ------------------------------------------------------------------
uint32_t g_lastRefreshMs = 0;
static bool g_isScreenSleeping = false;
static int  g_lastOtaCheckHour = -1;

// ------------------------------------------------------------------
// Helpers
// ------------------------------------------------------------------
void checkAndApplyUpdates() {
    Serial.println("[Main] Checking for OTA updates...");
    
    OtaUpdateInfo info;
    const char* currentVer = GOTAP_VERSION;
    const char* branch = settingsGetTargetBranch();

    if (fetchOtaUpdateInfo(currentVer, branch, info)) {
        if (info.updateAvailable) {
            Serial.printf("[Main] Update available: %s. Downloading from %s...\n", info.version, info.url);
            // This call is blocking and will restart the ESP on success
            otaPerformUpdate(info.url);
        } else {
            Serial.println("[Main] No update available");
        }
    } else {
        Serial.println("[Main] OTA info fetch failed");
    }
}

// ------------------------------------------------------------------
// setup
// ------------------------------------------------------------------
void setup() {
    Serial.begin(115200);
    delay(500); // let monitor connect
    Serial.println("[Main] goTapFirmware starting...");

    // 0. Init Settings (NVS)
    settingsInit();

    // 1. Display + LVGL (must come before WiFi to show boot message early)
    smartdisplay_init();          // init ST7262 display + GT911 touch
    lv_init();                    // LVGL core init

    // Set rotation to portrait (270 degrees relative to landscape)
    lv_display_t* disp = lv_display_get_default();
    if (disp) {
        lv_display_set_rotation(disp, LV_DISPLAY_ROTATION_270);
    }

    Serial.println("[Main] Display initialised (Portrait)");

    // 2. WiFi (blocking – restarts if it cannot connect)
    wifiConnect();

    // 3. UI
    uiInit();

    // 4. Initial data fetch
    TapData data;
    fetchTapDisplay(data);
    uiShowIdle(data);

    // 5. Initial OTA check
    checkAndApplyUpdates();

    g_lastRefreshMs = millis();
    Serial.println("[Main] Setup complete");
}

// ------------------------------------------------------------------
// loop
// ------------------------------------------------------------------
void loop() {
    // Update LVGL tick
    static uint32_t lastTick = 0;
    uint32_t currentTick = millis();
    lv_tick_inc(currentTick - lastTick);
    lastTick = currentTick;

    // LVGL timer handler – must be called frequently (every few ms)
    lv_timer_handler();

    // 60-second API polling (millis-based, non-blocking)
    uint32_t now = millis();
    if (now - g_lastRefreshMs >= DISPLAY_REFRESH_INTERVAL_MS) {
        g_lastRefreshMs = now;
        TapData data;
        if (fetchTapDisplay(data)) {
            uiShowIdle(data);
        }
    }

    // Scheduled OTA check (Every day at 03:00 with 0-15 min random delay)
    static uint32_t otaTargetMs = 0;
    static bool otaPending = false;

    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
        if (timeinfo.tm_hour == 3 && g_lastOtaCheckHour != 3) {
            g_lastOtaCheckHour = 3;
            // Set a random delay between 0 and 15 minutes (900,000 ms)
            uint32_t delayMs = random(0, 900001); 
            otaTargetMs = millis() + delayMs;
            otaPending = true;
            Serial.printf("[Main] 03:00 detected. Scheduling OTA check in %d seconds...\n", delayMs / 1000);
        } else if (timeinfo.tm_hour != 3) {
            g_lastOtaCheckHour = timeinfo.tm_hour;
        }
    }

    if (otaPending && millis() >= otaTargetMs) {
        otaPending = false;
        checkAndApplyUpdates();
    }

    // WiFi reconnect guard
    wifiReconnectIfNeeded();

    // Screen Auto-Sleep Logic (Check every loop)
    uint32_t inactiveMs = lv_display_get_inactive_time(lv_display_get_default());
    if (!g_isScreenSleeping && inactiveMs >= DISPLAY_SLEEP_TIMEOUT_MS) {
        g_isScreenSleeping = true;
        smartdisplay_lcd_set_backlight(0.0f); // Turn off backlight completely
        Serial.println("[Main] Screen sleeping due to 30 min inactivity");
    } else if (g_isScreenSleeping && inactiveMs < DISPLAY_SLEEP_TIMEOUT_MS) {
        g_isScreenSleeping = false;
        smartdisplay_lcd_set_backlight(1.0f); // Wake up full brightness
        Serial.println("[Main] Screen touched, waking up!");
    }
}
