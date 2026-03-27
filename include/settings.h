/**
 * @file settings.h
 * @brief Wrapper for NVS (Preferences) lagring av innstillinger som f.eks tapId.
 */

#pragma once

/**
 * @brief Initialize settings. Laster lagret tapId eller setter til default (config.h TAP_ID).
 */
void settingsInit();

/**
 * @brief Henter gjeldende tapId (enten lagret eller fallback).
 */
const char* settingsGetTapId();

/**
 * @brief Henter gjeldende OTA-branch (main, dev, etc).
 */
const char* settingsGetTargetBranch();

/**
 * @brief Lagre ny tapId i NVS.
 */
void settingsSetTapId(const char* tapId);

/**
 * @brief Lagre ny OTA-branch i NVS.
 */
void settingsSetTargetBranch(const char* branch);
