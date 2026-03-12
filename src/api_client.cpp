/**
 * @file api_client.cpp
 * @brief goTapList REST API client implementation.
 */

#include "api_client.h"
#include "config.h"


#include <Arduino.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFi.h>

// ------------------------------------------------------------------
// Helpers
// ------------------------------------------------------------------

static bool httpGet(const char* url, String& responseBody) {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[API] WiFi not connected – skipping GET");
        return false;
    }

    HTTPClient http;
    http.setTimeout(HTTP_TIMEOUT_MS);
    http.begin(url);

    int code = http.GET();
    if (code == HTTP_CODE_NO_CONTENT) {
        // 204 – no beer on tap
        http.end();
        return false;
    }
    if (code != HTTP_CODE_OK) {
        Serial.printf("[API] GET %s returned %d\n", url, code);
        http.end();
        return false;
    }

    responseBody = http.getString();
    http.end();
    return true;
}

// ------------------------------------------------------------------
// Public API
// ------------------------------------------------------------------

bool fetchTapDisplay(TapData& out) {
    // Build URL
    char url[256];
    snprintf(url, sizeof(url), "%s/taps/%s/display", API_BASE_URL, TAP_ID);

    String body;
    if (!httpGet(url, body)) {
        // 204 or error → show empty tap
        out.isEmpty = true;
        return false;
    }

    // Parse JSON
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, body);
    if (err) {
        Serial.printf("[API] JSON parse error: %s\n", err.c_str());
        return false;
    }

    // Populate struct – use strlcpy to stay within buffer bounds
    strlcpy(out.beerName,       doc["beer_name"]  | "",   sizeof(out.beerName));
    strlcpy(out.logoUrl,        doc["logo_url"]   | "",   sizeof(out.logoUrl));
    strlcpy(out.kegId,          doc["keg_id"]     | KEG_ID, sizeof(out.kegId));
    out.abv              = doc["abv"]              | 0.0f;
    out.remainingLiters  = doc["remaining_liters"] | 0.0f;
    out.isEmpty          = false;

    Serial.printf("[API] Tap data: %s (%.1f%% ABV, %.1f L left)\n",
                  out.beerName, out.abv, out.remainingLiters);
    return true;
}

bool postPour(const char* kegId, float liters) {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[API] WiFi not connected – skipping POST");
        return false;
    }

    char url[256];
    snprintf(url, sizeof(url), "%s/kegs/%s/pour", API_BASE_URL, kegId);

    // Build JSON body
    char body[64];
    snprintf(body, sizeof(body), "{\"liters\":%.2f,\"source\":\"esp32\"}", liters);

    HTTPClient http;
    http.setTimeout(HTTP_TIMEOUT_MS);
    http.begin(url);
    http.addHeader("Content-Type", "application/json");

    int code = http.POST(body);
    http.end();

    if (code == HTTP_CODE_OK || code == HTTP_CODE_CREATED) {
        Serial.printf("[API] Pour registered: %.2f L on keg %s\n", liters, kegId);
        return true;
    }

    Serial.printf("[API] POST pour failed: %d\n", code);
    return false;
}
