#include "settings.h"
#include "config.h"
#include <Preferences.h>
#include <Arduino.h>

static Preferences preferences;
static char g_activeTapId[32] = {0};

void settingsInit() {
    preferences.begin("gotap", false);
    
    String savedId = preferences.getString("tapId", TAP_ID);
    strlcpy(g_activeTapId, savedId.c_str(), sizeof(g_activeTapId));
    
    Serial.printf("[Settings] Active Tap ID: %s (Default in config: %s)\n", g_activeTapId, TAP_ID);
}

const char* settingsGetTapId() {
    return g_activeTapId;
}

void settingsSetTapId(const char* tapId) {
    if (!tapId || strlen(tapId) == 0) return;
    
    strlcpy(g_activeTapId, tapId, sizeof(g_activeTapId));
    preferences.putString("tapId", tapId);
    Serial.printf("[Settings] Saved new Tap ID to NVS: %s\n", tapId);
}
