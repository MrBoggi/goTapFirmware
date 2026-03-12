/**
 * @file wifi_manager.cpp
 * @brief WiFi connection management.
 */

#include "wifi_manager.h"
#include "config.h"


#include <Arduino.h>
#include <WiFi.h>

static constexpr uint8_t  WIFI_MAX_RETRIES    = 30;   // 30 × 500 ms = 15 s
static constexpr uint16_t WIFI_RETRY_DELAY_MS = 500;

void wifiConnect() {
    Serial.printf("[WiFi] Connecting to SSID: %s\n", GOTAP_WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(GOTAP_WIFI_SSID, GOTAP_WIFI_PASSWORD);

    uint8_t retries = 0;
    while (WiFi.status() != WL_CONNECTED) {
        delay(WIFI_RETRY_DELAY_MS); // acceptable in setup() only
        Serial.print('.');
        if (++retries >= WIFI_MAX_RETRIES) {
            Serial.println("\n[WiFi] Failed to connect. Restarting...");
            ESP.restart();
        }
    }

    Serial.printf("\n[WiFi] Connected! IP: %s\n", WiFi.localIP().toString().c_str());
}

void wifiReconnectIfNeeded() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[WiFi] Connection lost – reconnecting...");
        WiFi.reconnect();
    }
}
