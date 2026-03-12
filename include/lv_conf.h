/**
 * @file lv_conf.h
 * LVGL v9 configuration for JC8048W550 (800x480, 16-bit colour, PSRAM-backed).
 * esp32_smartdisplay provides lv_conf_internal.h defaults; this file overrides
 * only what we need.
 */

#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

/*====================
   Colour settings
 *====================*/
#define LV_COLOR_DEPTH 16

/*====================
   Memory
 *====================*/
#define LV_MEM_SIZE (64 * 1024U)   /* 64 KB for LVGL objects (internal DRAM) */

/*====================
   Font support (Montserrat built-in)
 *====================*/
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_28 1
#define LV_FONT_DEFAULT &lv_font_montserrat_14

/*====================
   Image decoders
 *====================*/
/* LVGL v9: built-in JPEG decoding via tjpgd */
#define LV_USE_TJPGD    1
#define LV_USE_PNG      0
#define LV_USE_BMP      0
#define LV_USE_GIF      0
#define LV_USE_QRCODE   0

/*====================
   Widgets
 *====================*/
#define LV_USE_BTN     1
#define LV_USE_LABEL   1
#define LV_USE_IMG     1
#define LV_USE_SPINNER 1
#define LV_USE_MSGBOX  0
#define LV_USE_ARC     1   /* required by lv_spinner */
#define LV_USE_BAR     0
#define LV_USE_SLIDER  0
#define LV_USE_SWITCH  0
#define LV_USE_TABLE   0
#define LV_USE_CHART   0
#define LV_USE_METER   0
#define LV_USE_TEXTAREA  1   /* required by lv_spinbox */
#define LV_USE_KEYBOARD  0

/*====================
   Logging
 *====================*/
#define LV_USE_LOG     1
#define LV_LOG_LEVEL   LV_LOG_LEVEL_WARN
#define LV_LOG_PRINTF  1

/*====================
   Misc
 *====================*/
#define LV_USE_PERF_MONITOR 0
#define LV_USE_MEM_MONITOR  0

/* Disable ARM Helium assembly – Xtensa/ESP32-S3 is not ARM */
#define LV_DRAW_SW_ASM LV_DRAW_SW_ASM_NONE

#endif /* LV_CONF_H */
