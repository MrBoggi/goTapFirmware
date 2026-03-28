/**
 * @file ota_manager.h
 * @brief Handles Over-The-Air (OTA) updates for goTapFirmware.
 */

#pragma once

#include <Arduino.h>

/**
 * @brief Check for and perform an update if available.
 * @param updateUrl The direct URL to the .bin file.
 * @return True if update sequence started, false otherwise.
 */
bool otaPerformUpdate(const char* updateUrl);

/**
 * @brief Returns true if an update is currently in progress.
 */
bool otaIsUpdating();
