#include "settings.h"
#include "config.h"
#include <Preferences.h>
#include <Arduino.h>

#ifndef OTA_DEFAULT_BRANCH
#define OTA_DEFAULT_BRANCH "main"
#endif

static Preferences preferences;
static char g_activeTapId[32] = {0};
static char g_targetBranch[32] = {0};

void settingsInit() {
    preferences.begin("gotap", false);
    
    String savedId = preferences.getString("tapId", TAP_ID);
    strlcpy(g_activeTapId, savedId.c_str(), sizeof(g_activeTapId));

    String savedBranch = preferences.getString("branch", OTA_DEFAULT_BRANCH);
    strlcpy(g_targetBranch, savedBranch.c_str(), sizeof(g_targetBranch));
    
    Serial.printf("[Settings] Active Tap ID: %s (Default in config: %s)\n", g_activeTapId, TAP_ID);
}

const char* settingsGetTapId() {
    return g_activeTapId;
}

const char* settingsGetTargetBranch() {
    return g_targetBranch;
}
void settingsSetTapId(const char* tapId) {
    if (!tapId || strlen(tapId) == 0) return;
    
    strlcpy(g_activeTapId, tapId, sizeof(g_activeTapId));
    preferences.putString("tapId", tapId);
    Serial.printf("[Settings] Saved new Tap ID to NVS: %s\n", tapId);
}

void settingsSetTargetBranch(const char* branch) {
    if (!branch || strlen(branch) == 0) return;
    
    strlcpy(g_targetBranch, branch, sizeof(g_targetBranch));
    preferences.putString("branch", branch);
    Serial.printf("[Settings] Saved new Target Branch to NVS: %s\n", branch);
}
