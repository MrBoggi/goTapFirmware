/**
 * @file api_client.h
 * @brief goTapList REST API client for goTapFirmware.
 */

#pragma once

#include <stdint.h>

// -------------------------------------------------------------------
// Data structures
// -------------------------------------------------------------------

/** Maximum length for string fields from the API */
static constexpr uint8_t API_STR_LEN = 128;

#define MAX_TAPS_IN_LIST 10

struct TapInfo {
    int id;                   ///< Tap ID
    char name[API_STR_LEN];   ///< Name of the tap
};

struct TapList {
    TapInfo taps[MAX_TAPS_IN_LIST];
    uint8_t count;
};

/**
 * @brief Data received from GET /api/taps/{tapID}/display
 */
struct TapData {
    char  beerName[API_STR_LEN]; ///< Name of the beer on tap
    char  logoUrl[API_STR_LEN];  ///< Absolute URL to JPEG logo image
    char  kegId[API_STR_LEN];    ///< Keg ID used for pour registration
    float abv;                   ///< Alcohol by volume (%)
    float remainingLiters;       ///< Remaining volume in keg (litres)
    bool  isEmpty;               ///< True when tap has no beer (204 response)
};

/**
 * @brief OTA Update Info from GET /api/ota/check
 */
struct OtaUpdateInfo {
    bool updateAvailable;
    char version[16];
    char url[256];
};

// -------------------------------------------------------------------
// API functions
// -------------------------------------------------------------------

/**
 * @brief Fetch display data for current tap.
 * @param[out] out  Populated TapData struct on success.
 * @return true on success, false on HTTP/parse error.
 */
bool fetchTapDisplay(TapData& out);

/**
 * @brief Register a pour via POST /api/kegs/{kegID}/pour.
 * @param kegId   Keg ID string.
 * @param liters  Volume poured in litres (e.g. 0.25, 0.33, 0.44, 0.50).
 * @return true on success (HTTP 200/201), false otherwise.
 */
bool postPour(const char* kegId, float liters);

/**
 * @brief Fetch list of available taps.
 * @param[out] out  Populated TapList struct on success.
 * @return true on success, false on HTTP/parse error.
 */
bool fetchTapsList(TapList& out);

/**
 * @brief Check for OTA update.
 * @param currentVersion  The current firmware version (e.g. 1.0.0).
 * @param targetBranch    The branch to look for (main, dev, etc).
 * @param[out] out        Populated OtaUpdateInfo struct.
 * @return true on success (even if no update available), false on error.
 */
bool fetchOtaUpdateInfo(const char* currentVersion, const char* targetBranch, OtaUpdateInfo& out);

/**
 * @brief Update target branch for current tap.
 * @param tapId     Tap ID string.
 * @param branch    New branch name ("main" or "development").
 * @return true on success (HTTP 204), false on error.
 */
bool updateTapBranch(const char* tapId, const char* branch);
