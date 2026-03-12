/**
 * @file ui.cpp
 * @brief LVGL v9 UI implementation for goTapFirmware.
 *
 * Screen 1 – Idle  : logo + beer name / ABV / volume (or empty-tap icon).
 * Screen 2 – Pour  : modal overlay with 4 size buttons + cancel.
 *
 * LVGL is NOT thread-safe. All UI calls must happen from the Arduino main loop.
 */

#include "ui.h"
#include "api_client.h"

#include <Arduino.h>
#include <lvgl.h>
#include <HTTPClient.h>
#include <WiFi.h>

// ------------------------------------------------------------------
// Internal state
// ------------------------------------------------------------------
static lv_obj_t* g_screenIdle  = nullptr;
static lv_obj_t* g_screenPour  = nullptr;
static TapData   g_currentData = {};

// Idle-screen widgets
static lv_obj_t* g_imgLogo     = nullptr;
static lv_obj_t* g_labelName   = nullptr;
static lv_obj_t* g_labelAbv    = nullptr;
static lv_obj_t* g_labelVolume = nullptr;
static lv_obj_t* g_spinnerLoad = nullptr;
static lv_obj_t* g_labelEmpty  = nullptr;

// LVGL v9 image descriptor + PSRAM buffer for downloaded logo
static lv_image_dsc_t g_logoDsc       = {};
static uint8_t*       g_logoBuffer    = nullptr;
static size_t         g_logoBufferSz  = 0;

// ------------------------------------------------------------------
// Colour palette (LVGL v9: lv_color_make)
// ------------------------------------------------------------------
#define COL_BG      lv_color_make(0x12, 0x12, 0x1E)
#define COL_ACCENT  lv_color_make(0xF5, 0xA6, 0x23)
#define COL_WHITE   lv_color_make(0xFF, 0xFF, 0xFF)
#define COL_GREEN   lv_color_make(0x2E, 0xCC, 0x71)
#define COL_BTN_BG  lv_color_make(0x1E, 0x1E, 0x32)
#define COL_CANCEL  lv_color_make(0x80, 0x20, 0x20)
#define COL_GREY    lv_color_make(0x80, 0x80, 0x80)

// ------------------------------------------------------------------
// Forward declarations
// ------------------------------------------------------------------
static void buildIdleScreen();
static void buildPourScreen();
static void logoTapCb(lv_event_t* e);
static void pourBtnCb(lv_event_t* e);
static void cancelBtnCb(lv_event_t* e);
static void confirmTimerCb(lv_timer_t* t);
static bool downloadLogo(const char* url);

// ------------------------------------------------------------------
// Public
// ------------------------------------------------------------------

void uiInit() {
    g_screenIdle = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(g_screenIdle, COL_BG, 0);

    g_screenPour = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(g_screenPour, COL_BG, 0);

    buildIdleScreen();
    buildPourScreen();

    lv_screen_load(g_screenIdle);
}

void uiShowIdle(const TapData& data) {
    memcpy(&g_currentData, &data, sizeof(TapData));

    if (data.isEmpty) {
        lv_obj_add_flag(g_imgLogo,     LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(g_labelName,   LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(g_labelAbv,    LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(g_labelVolume, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(g_labelEmpty, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(g_labelEmpty, LV_OBJ_FLAG_HIDDEN);

        // Show spinner while fetching logo
        lv_obj_remove_flag(g_spinnerLoad, LV_OBJ_FLAG_HIDDEN);
        lv_refr_now(nullptr);

        if (downloadLogo(data.logoUrl)) {
            lv_image_set_src(g_imgLogo, &g_logoDsc);
            lv_obj_remove_flag(g_imgLogo, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(g_imgLogo, LV_OBJ_FLAG_HIDDEN);
        }

        lv_obj_add_flag(g_spinnerLoad, LV_OBJ_FLAG_HIDDEN);

        // Text labels
        lv_label_set_text(g_labelName, data.beerName);

        char buf[32];
        snprintf(buf, sizeof(buf), "%.1f%% ABV", data.abv);
        lv_label_set_text(g_labelAbv, buf);

        snprintf(buf, sizeof(buf), "%.1f L igjen", data.remainingLiters);
        lv_label_set_text(g_labelVolume, buf);

        lv_obj_remove_flag(g_labelName,   LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(g_labelAbv,    LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(g_labelVolume, LV_OBJ_FLAG_HIDDEN);
    }

    lv_screen_load_anim(g_screenIdle, LV_SCR_LOAD_ANIM_FADE_IN, 200, 0, false);
}

void uiShowPourSelector(const TapData& data) {
    memcpy(&g_currentData, &data, sizeof(TapData));
    lv_screen_load_anim(g_screenPour, LV_SCR_LOAD_ANIM_MOVE_TOP, 200, 0, false);
}

void uiShowConfirmation() {
    lv_obj_t* overlay = lv_obj_create(g_screenIdle);
    lv_obj_set_size(overlay, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(overlay, COL_GREEN, 0);
    lv_obj_set_style_bg_opa(overlay, LV_OPA_80, 0);
    lv_obj_set_style_border_width(overlay, 0, 0);
    lv_obj_center(overlay);

    lv_obj_t* lbl = lv_label_create(overlay);
    lv_label_set_text(lbl, LV_SYMBOL_OK "  Registrert!");
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(lbl, COL_WHITE, 0);
    lv_obj_center(lbl);

    lv_timer_create(confirmTimerCb, 2000, overlay);
    lv_screen_load_anim(g_screenIdle, LV_SCR_LOAD_ANIM_FADE_IN, 200, 0, false);
}

// ------------------------------------------------------------------
// Private: Download logo to PSRAM
// ------------------------------------------------------------------
static bool downloadLogo(const char* url) {
    if (!url || url[0] == '\0' || WiFi.status() != WL_CONNECTED) return false;

    HTTPClient http;
    http.setTimeout(5000);
    http.begin(url);
    int code = http.GET();
    if (code != HTTP_CODE_OK) { http.end(); return false; }

    int len = http.getSize();
    if (len <= 0 || len > 200 * 1024) { http.end(); return false; }

    if (!g_logoBuffer || g_logoBufferSz < (size_t)len) {
        free(g_logoBuffer);
        g_logoBuffer   = (uint8_t*)heap_caps_malloc(len, MALLOC_CAP_SPIRAM);
        g_logoBufferSz = g_logoBuffer ? (size_t)len : 0;
    }
    if (!g_logoBuffer) { Serial.println("[UI] PSRAM alloc failed"); http.end(); return false; }

    WiFiClient* stream = http.getStreamPtr();
    size_t rx = 0;
    while (http.connected() && rx < (size_t)len) {
        if (stream->available()) rx += stream->readBytes(g_logoBuffer + rx, len - rx);
    }
    http.end();
    if (rx != (size_t)len) return false;

    // LVGL v9 image descriptor (raw JPEG, let tjpgd decode on draw)
    g_logoDsc = {};
    g_logoDsc.header.magic    = LV_IMAGE_HEADER_MAGIC;
    g_logoDsc.header.cf       = LV_COLOR_FORMAT_RAW;
    g_logoDsc.header.w        = 300; // display size (LVGL will scale)
    g_logoDsc.header.h        = 300;
    g_logoDsc.data_size       = (uint32_t)rx;
    g_logoDsc.data            = g_logoBuffer;

    Serial.printf("[UI] Logo: %u bytes\n", rx);
    return true;
}

// ------------------------------------------------------------------
// Private: Build screens
// ------------------------------------------------------------------
static void buildIdleScreen() {
    g_spinnerLoad = lv_spinner_create(g_screenIdle);
    lv_obj_set_size(g_spinnerLoad, 80, 80);
    lv_obj_center(g_spinnerLoad);
    lv_obj_add_flag(g_spinnerLoad, LV_OBJ_FLAG_HIDDEN);

    g_imgLogo = lv_image_create(g_screenIdle);
    lv_obj_set_size(g_imgLogo, 300, 300);
    lv_obj_align(g_imgLogo, LV_ALIGN_CENTER, 0, -50);
    lv_obj_add_flag(g_imgLogo, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(g_imgLogo, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(g_imgLogo, logoTapCb, LV_EVENT_CLICKED, nullptr);

    g_labelName = lv_label_create(g_screenIdle);
    lv_obj_set_style_text_font(g_labelName, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(g_labelName, COL_WHITE, 0);
    lv_label_set_long_mode(g_labelName, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_width(g_labelName, 700);
    lv_obj_align(g_labelName, LV_ALIGN_BOTTOM_MID, 0, -100);
    lv_obj_add_flag(g_labelName, LV_OBJ_FLAG_HIDDEN);

    g_labelAbv = lv_label_create(g_screenIdle);
    lv_obj_set_style_text_font(g_labelAbv, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(g_labelAbv, COL_ACCENT, 0);
    lv_obj_align(g_labelAbv, LV_ALIGN_BOTTOM_MID, -100, -65);
    lv_obj_add_flag(g_labelAbv, LV_OBJ_FLAG_HIDDEN);

    g_labelVolume = lv_label_create(g_screenIdle);
    lv_obj_set_style_text_font(g_labelVolume, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(g_labelVolume, COL_WHITE, 0);
    lv_obj_align(g_labelVolume, LV_ALIGN_BOTTOM_MID, 100, -65);
    lv_obj_add_flag(g_labelVolume, LV_OBJ_FLAG_HIDDEN);

    g_labelEmpty = lv_label_create(g_screenIdle);
    lv_label_set_text(g_labelEmpty, LV_SYMBOL_WARNING "\n\nTom kran");
    lv_obj_set_style_text_font(g_labelEmpty, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(g_labelEmpty, COL_ACCENT, 0);
    lv_obj_set_style_text_align(g_labelEmpty, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(g_labelEmpty);
    lv_obj_add_flag(g_labelEmpty, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t* hint = lv_label_create(g_screenIdle);
    lv_label_set_text(hint, "Trykk på logo for å registrere tapping");
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(hint, COL_GREY, 0);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -20);
}

static void buildPourScreen() {
    lv_obj_t* title = lv_label_create(g_screenPour);
    lv_label_set_text(title, "Velg størrelse");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(title, COL_WHITE, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 30);

    static const float  pourSizes[]  = {0.25f, 0.33f, 0.44f, 0.50f};
    static const char*  pourLabels[] = {"0,25 L", "0,33 L", "0,44 L", "0,50 L"};

    const int btnW = 300, btnH = 130, gapX = 20, gapY = 20;
    for (int i = 0; i < 4; i++) {
        int col  = i % 2;
        int row  = i / 2;
        int xPos = col == 0 ? -(btnW / 2 + gapX / 2) : (btnW / 2 + gapX / 2);
        int yPos = row == 0 ? -(btnH / 2 + gapY / 2) + 20 : (btnH / 2 + gapY / 2) + 20;

        lv_obj_t* btn = lv_button_create(g_screenPour); // LVGL v9: lv_button_create
        lv_obj_set_size(btn, btnW, btnH);
        lv_obj_align(btn, LV_ALIGN_CENTER, xPos, yPos);
        lv_obj_set_style_bg_color(btn, COL_BTN_BG, 0);
        lv_obj_set_style_bg_color(btn, COL_ACCENT, LV_STATE_PRESSED);
        lv_obj_set_style_radius(btn, 16, 0);
        lv_obj_add_event_cb(btn, pourBtnCb, LV_EVENT_CLICKED, (void*)&pourSizes[i]);

        lv_obj_t* lbl = lv_label_create(btn);
        lv_label_set_text(lbl, pourLabels[i]);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_28, 0);
        lv_obj_set_style_text_color(lbl, COL_WHITE, 0);
        lv_obj_center(lbl);
    }

    lv_obj_t* cancelBtn = lv_button_create(g_screenPour);
    lv_obj_set_size(cancelBtn, 80, 80);
    lv_obj_align(cancelBtn, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_set_style_bg_color(cancelBtn, COL_CANCEL, 0);
    lv_obj_set_style_radius(cancelBtn, LV_RADIUS_CIRCLE, 0);
    lv_obj_add_event_cb(cancelBtn, cancelBtnCb, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* cancelLbl = lv_label_create(cancelBtn);
    lv_label_set_text(cancelLbl, LV_SYMBOL_CLOSE);
    lv_obj_set_style_text_font(cancelLbl, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(cancelLbl, COL_WHITE, 0);
    lv_obj_center(cancelLbl);
}

// ------------------------------------------------------------------
// Callbacks
// ------------------------------------------------------------------
static void logoTapCb(lv_event_t* e) {
    (void)e;
    uiShowPourSelector(g_currentData);
}

static void pourBtnCb(lv_event_t* e) {
    const float* liters = (const float*)lv_event_get_user_data(e);
    if (!liters) return;
    Serial.printf("[UI] Pour: %.2f L\n", *liters);
    if (postPour(g_currentData.kegId, *liters)) {
        uiShowConfirmation();
    } else {
        lv_screen_load_anim(g_screenIdle, LV_SCR_LOAD_ANIM_FADE_IN, 200, 0, false);
    }
}

static void cancelBtnCb(lv_event_t* e) {
    (void)e;
    lv_screen_load_anim(g_screenIdle, LV_SCR_LOAD_ANIM_MOVE_BOTTOM, 200, 0, false);
}

static void confirmTimerCb(lv_timer_t* t) {
    lv_obj_t* overlay = (lv_obj_t*)lv_timer_get_user_data(t);
    if (overlay) lv_obj_delete(overlay); // LVGL v9: lv_obj_delete (not lv_obj_del)
    lv_timer_delete(t);                  // LVGL v9: lv_timer_delete (not lv_timer_del)
}
