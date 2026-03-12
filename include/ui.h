/**
 * @file ui.h
 * @brief LVGL UI layer for goTapFirmware.
 *
 * Two screens:
 *  - Idle  : beer logo + name / ABV / volume (or empty-tap icon)
 *  - Pour  : modal overlay with 4 size buttons + cancel (X)
 */

#pragma once

#include "api_client.h"

/**
 * @brief Initialise LVGL screens (call after smartdisplay_init + lv_init).
 */
void uiInit();

/**
 * @brief Show the idle screen with tap and beer data.
 * @param data  Current tap data; if data.isEmpty the empty-tap icon is shown.
 */
void uiShowIdle(const TapData& data);

/**
 * @brief Show pour size selector modal on top of idle screen.
 * @param data  Used to obtain kegId for the pour POST call.
 */
void uiShowPourSelector(const TapData& data);

/**
 * @brief Show brief confirmation ("Registrert!") then return to idle.
 *        Non-blocking – uses an LVGL timer callback internally.
 */
void uiShowConfirmation();
