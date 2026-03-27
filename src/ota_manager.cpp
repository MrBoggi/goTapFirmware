/**
 * @file ota_manager.cpp
 * @brief OTA update logic using HTTPUpdate.
 */

#include "ota_manager.h"
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>

static bool g_isUpdating = false;

bool otaPerformUpdate(const char* updateUrl) {
    if (!updateUrl || updateUrl[0] == '\0') return false;
    if (WiFi.status() != WL_CONNECTED) return false;

    Serial.printf("[OTA] Starting update from: %s\n", updateUrl);
    g_isUpdating = true;

    // Optional: Set a callback to show progress on UI
    // httpUpdate.onProgress(progressCb);

    // For ESP32, httpUpdate.update() handles everything: 
    // downloading, writing to partition, and restarting.
    // We use a non-secure client for now as assumed in the plan.
    WiFiClient client;
    t_httpUpdate_return ret = httpUpdate.update(client, updateUrl);

    switch (ret) {
        case HTTP_UPDATE_FAILED:
            Serial.printf("[OTA] Update failed (%d): %s\n", 
                          httpUpdate.getLastError(), httpUpdate.getLastErrorString().c_str());
            g_isUpdating = false;
            return false;

        case HTTP_UPDATE_NO_UPDATES:
            Serial.println("[OTA] No updates available");
            g_isUpdating = false;
            return false;

        case HTTP_UPDATE_OK:
            Serial.println("[OTA] Update successful! Rebooting...");
            // Device will usually reboot automatically here.
            return true;
    }

    g_isUpdating = false;
    return false;
}

bool otaIsUpdating() {
    return g_isUpdating;
}
