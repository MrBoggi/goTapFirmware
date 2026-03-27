/**
 * @file wifi_manager.h
 * @brief WiFi connection management for goTapFirmware.
 *
 * Provides blocking connect (setup) and non-blocking reconnect (loop).
 */

#pragma once

/**
 * @brief Connect to WiFi using credentials from config.h.
 *        Blocks until connected or max retries exceeded (triggers ESP restart).
 *        Call once in setup().
 */
void wifiConnect();

/**
 * @brief Check WiFi status and reconnect if disconnected.
 *        Non-blocking – call every loop iteration.
 */
void wifiReconnectIfNeeded();

/**
 * @brief Sync internal clock with NTP.
 */
void syncTime();
