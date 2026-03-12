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
#include "wifi_manager.h"
#include "api_client.h"
#include "ui.h"

// ------------------------------------------------------------------
// Timing
// ------------------------------------------------------------------
static uint32_t g_lastRefreshMs = 0;

// ------------------------------------------------------------------
// setup
// ------------------------------------------------------------------
void setup() {
    Serial.begin(115200);
    delay(500); // let monitor connect
    Serial.println("[Main] goTapFirmware starting...");

    // 1. Display + LVGL (must come before WiFi to show boot message early)
    smartdisplay_init();          // init ST7262 display + GT911 touch
    lv_init();                    // LVGL core init

    Serial.println("[Main] Display initialised");

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
}
