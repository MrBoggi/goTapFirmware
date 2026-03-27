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

#include "config.h"   // MUST be included before any module that uses its defines
#include "settings.h"
#include "wifi_manager.h"
#include "api_client.h"
#include "ui.h"

// ------------------------------------------------------------------
// Timing & State
// ------------------------------------------------------------------
uint32_t g_lastRefreshMs = 0;
static bool g_isScreenSleeping = false;

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
