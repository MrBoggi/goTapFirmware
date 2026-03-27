/**
 * @file api_client.cpp
 * @brief goTapList REST API client implementation.
 */

#include "api_client.h"
#include "config.h"
#include "settings.h"

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
    snprintf(url, sizeof(url), "%s/taps/%s/display", API_BASE_URL, settingsGetTapId());

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
    strlcpy(out.beerName,       doc["beerName"]   | "",   sizeof(out.beerName));
    
    // Logo fallback: if "beerLogo" is missing but "beerId" exists, construct the URL (like goTOVGUI)
    const char* logo = doc["beerLogo"] | "";
    if (logo[0] == '\0' && !doc["beerId"].isNull()) {
        snprintf(out.logoUrl, sizeof(out.logoUrl), "%s/beers/%d/logo", API_BASE_URL, doc["beerId"].as<int>());
    } else {
        strlcpy(out.logoUrl, logo, sizeof(out.logoUrl));
    }

    if (out.logoUrl[0] != '\0') {
        Serial.printf("[API] Logo URL: %s\n", out.logoUrl);
    }

    strlcpy(out.kegId,          doc["kegId"].as<String>().c_str(), sizeof(out.kegId));
    out.abv              = doc["beerAbv"]          | 0.0f;
    out.remainingLiters  = doc["volumeLeft"]       | 0.0f;
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
    char body[128];
    snprintf(body, sizeof(body), "{\"liters\":%.2f,\"source\":\"esp32-%s\"}", liters, settingsGetTapId());

    HTTPClient http;
    http.setTimeout(HTTP_TIMEOUT_MS);
    http.begin(url);
    http.addHeader("Content-Type", "application/json");

    int code = http.POST(body);
    http.end();

    if (code == HTTP_CODE_OK || code == HTTP_CODE_CREATED || code == HTTP_CODE_NO_CONTENT || code == 204) {
        Serial.printf("[API] Pour registered: %.2f L on keg %s\n", liters, kegId);
        return true;
    }

    Serial.printf("[API] POST pour failed: %d\n", code);
    return false;
}

bool fetchTapsList(TapList& out) {
    out.count = 0;

    char url[256];
    snprintf(url, sizeof(url), "%s/taps", API_BASE_URL);

    String body;
    if (!httpGet(url, body)) {
        return false;
    }

    // Taps is an array of objects
    // Example: [{"tapId":2,"tapName":"1",...}, ...]
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, body);
    if (err) {
        Serial.printf("[API] fetchTapsList JSON parse error: %s\n", err.c_str());
        return false;
    }

    JsonArray array = doc.as<JsonArray>();
    for (JsonVariant v : array) {
        if (out.count >= MAX_TAPS_IN_LIST) break;
        out.taps[out.count].id = v["tapId"] | 0;
        strlcpy(out.taps[out.count].name, v["tapName"] | "Ukjent", sizeof(out.taps[out.count].name));
        out.count++;
    }

    Serial.printf("[API] Fetched %d taps\n", out.count);
    return true;
}
