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
