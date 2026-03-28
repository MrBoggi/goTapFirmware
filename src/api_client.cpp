/**
 * @file api_client.cpp
 * @brief goTapList REST API client implementation.
 */

#include "api_client.h"
#include "gotap_config.h"
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

    Serial.printf("[API] GET %s\n", url); // Log the URL being requested

    int code = http.GET();
    if (code == HTTP_CODE_NO_CONTENT) {
        // 204 – no beer on tap
        Serial.printf("[API] GET %s returned %d (No Content)\n", url, code);
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

    // Populate struct – use camelCase to match backend (Issue #6 / development branch sync)
    strlcpy(out.beerName,       doc["beerName"]   | "",   sizeof(out.beerName));
    out.abv              = doc["beerAbv"]         | 0.0f;
    out.remainingLiters  = doc["volumeLeft"]      | 0.0f;
    
    // Construct keg ID as string (it's an int in the JSON)
    if (!doc["kegId"].isNull()) {
        snprintf(out.kegId, sizeof(out.kegId), "%d", doc["kegId"].as<int>());
    } else {
        out.kegId[0] = '\0';
    }

    // Construct hardware-optimized logo URL: /api/beers/{id}/logo/display
    // The optimized endpoint returns 300x300 JPEG for memory-efficient tjpgd decoding.
    int beerId = doc["beerId"] | 0;
    if (beerId > 0) {
        snprintf(out.logoUrl, sizeof(out.logoUrl), "%s/beers/%d/logo/display", API_BASE_URL, beerId);
    } else {
        out.logoUrl[0] = '\0';
    }

    out.isEmpty = false;
    Serial.printf("[API] Tap data: %s (%.1f%% ABV, %.1f L left)\n", out.beerName, out.abv, out.remainingLiters);
    Serial.printf("[API] Final Logo URL: %s\n", out.logoUrl);
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

bool fetchOtaUpdateInfo(const char* currentVersion, const char* targetBranch, OtaUpdateInfo& out) {
    out.updateAvailable = false;
    out.version[0] = '\0';
    out.url[0] = '\0';

    char vStr[20];
    if (currentVersion[0] != 'v') {
        snprintf(vStr, sizeof(vStr), "v%s", currentVersion);
    } else {
        strlcpy(vStr, currentVersion, sizeof(vStr));
    }

    char url[512];
    snprintf(url, sizeof(url), "%s/ota/check?tap_id=%s&version=%s&branch=%s", 
             API_BASE_URL, settingsGetTapId(), vStr, targetBranch);

    String body;
    if (!httpGet(url, body)) {
        return false;
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, body);
    if (err) {
        Serial.printf("[API] OTA check JSON parse error: %s\n", err.c_str());
        return false;
    }

    out.updateAvailable = doc["updateAvailable"] | false;
    if (out.updateAvailable) {
        strlcpy(out.version, doc["version"] | "", sizeof(out.version));
        strlcpy(out.url,     doc["url"]     | "", sizeof(out.url));
        
        // Ensure URL is absolute (prepend base URL if it's relative)
        if (out.url[0] == '/') {
            char fullUrl[512];
            snprintf(fullUrl, sizeof(fullUrl), "%s%s", API_BASE_URL, out.url);
            strlcpy(out.url, fullUrl, sizeof(out.url));
        }
    }

    return true;
}

bool updateTapBranch(const char* tapId, const char* branch) {
    if (!tapId || !branch) return false;

    char url[256];
    // API_BASE_URL already includes /api, so we use /taps/{id}/branch directly
    snprintf(url, sizeof(url), "%s/taps/%s/branch", API_BASE_URL, tapId);

    // Build JSON body manually (no need for ArduinoJson here)
    char jsonBody[64];
    snprintf(jsonBody, sizeof(jsonBody), "{\"targetBranch\":\"%s\"}", branch);

    HTTPClient http;
    http.begin(url);
    http.addHeader("Content-Type", "application/json");

    int httpCode = http.PUT(jsonBody);
    bool success = (httpCode == 204 || httpCode == 200);

    if (success) {
        Serial.printf("[API] PUT %s - Branch updated to %s\n", url, branch);
    } else {
        Serial.printf("[API] PUT %s - Failed: %d\n", url, httpCode);
    }

    http.end();
    return success;
}
