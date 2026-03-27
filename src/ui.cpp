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
#include "settings.h"

#include <Arduino.h>
#include <lvgl.h>
#include <HTTPClient.h>
#include <WiFi.h>

// ------------------------------------------------------------------
// Internal state
// ------------------------------------------------------------------
static lv_obj_t* g_screenIdle  = nullptr;
static lv_obj_t* g_screenPour  = nullptr;
static lv_obj_t* g_screenConfig= nullptr;
static TapData   g_currentData = {};
static TapList   g_tapList     = {};

static lv_obj_t* g_tapListContainer = nullptr;

extern uint32_t g_lastRefreshMs;

// Idle-screen widgets
static lv_obj_t* g_logoContainer = nullptr;
static lv_obj_t* g_imgLogo       = nullptr;
static lv_obj_t* g_labelName     = nullptr;
static lv_obj_t* g_labelAbv    = nullptr;
static lv_obj_t* g_labelVolume = nullptr;
static lv_obj_t* g_spinnerLoad = nullptr;
static lv_obj_t* g_labelEmpty  = nullptr;

// LVGL v9 image descriptor + PSRAM buffer for downloaded logo
static lv_image_dsc_t g_logoDsc       = {};
static uint8_t*       g_logoBuffer    = nullptr;
static size_t         g_logoBufferSz  = 0;

// ------------------------------------------------------------------
// Colour palette (Tæsse ØlVerksted)
// ------------------------------------------------------------------
#define COL_BG      lv_color_make(0x2B, 0x2D, 0x30)
#define COL_ACCENT  lv_color_make(0xF3, 0x9C, 0x12)
#define COL_WHITE   lv_color_make(0xFF, 0xFF, 0xFF)
#define COL_GREEN   lv_color_make(0x2E, 0xCC, 0x71)
#define COL_BTN_BG  lv_color_make(0x3B, 0x3D, 0x40)
#define COL_CANCEL  lv_color_make(0x80, 0x20, 0x20)
#define COL_GREY    lv_color_make(0xA0, 0xA0, 0xA0)

// ------------------------------------------------------------------
// Forward declarations
// ------------------------------------------------------------------
static void buildIdleScreen();
static void buildPourScreen();
static void buildConfigScreen();
static void logoTapCb(lv_event_t* e);
static void pourBtnCb(lv_event_t* e);
static void cancelBtnCb(lv_event_t* e);
static void settingsBtnCb(lv_event_t* e);
static void tapSelectedCb(lv_event_t* e);
static void cancelConfigBtnCb(lv_event_t* e);
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

    g_screenConfig = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(g_screenConfig, COL_BG, 0);

    buildIdleScreen();
    buildPourScreen();
    buildConfigScreen();

    lv_screen_load(g_screenIdle);
}

void uiShowIdle(const TapData& data) {
    memcpy(&g_currentData, &data, sizeof(TapData));

    if (data.isEmpty) {
        lv_obj_add_flag(g_logoContainer, LV_OBJ_FLAG_HIDDEN);
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
            lv_image_cache_drop(&g_logoDsc);
            lv_image_set_src(g_imgLogo, &g_logoDsc);
            lv_obj_remove_flag(g_logoContainer, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(g_logoContainer, LV_OBJ_FLAG_HIDDEN);
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

    if (lv_screen_active() != g_screenIdle) {
        lv_screen_load_anim(g_screenIdle, LV_SCR_LOAD_ANIM_FADE_IN, 200, 0, false);
    }
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
// Custom Stream for PSRAM Logo Buffer
// ------------------------------------------------------------------
class LogoStream : public Stream {
private:
    uint8_t* _buf;
    size_t _cap;
    size_t _len;

public:
    LogoStream(uint8_t* buf, size_t cap) : _buf(buf), _cap(cap), _len(0) {}

    size_t write(uint8_t data) override {
        if (_len < _cap) {
            _buf[_len++] = data;
            return 1;
        }
        return 0; // Buffer full
    }

    size_t write(const uint8_t *buffer, size_t size) override {
        size_t space = _cap - _len;
        size_t toWrite = (size < space) ? size : space;
        if (toWrite > 0) {
            memcpy(_buf + _len, buffer, toWrite);
            _len += toWrite;
        }
        return toWrite;
    }

    int available() override { return 0; }
    int read() override { return -1; }
    int peek() override { return -1; }
};

// ------------------------------------------------------------------
// Private: Download logo to PSRAM
// ------------------------------------------------------------------

// Extractor helper to bypass LVGL v9 memory size bug
static bool getJpegSize(const uint8_t* data, size_t len, int32_t* w, int32_t* h) {
    if (len < 4 || data[0] != 0xFF || data[1] != 0xD8) return false;
    size_t i = 2;
    while (i < len - 8) {
        if (data[i] == 0xFF) {
            uint8_t marker = data[i + 1];
            if (marker == 0xFF) { i++; continue; } // Padding bytes
            if (marker == 0xC0 || marker == 0xC1 || marker == 0xC2) { // SOF markers contain dimensions
                *h = (data[i + 5] << 8) | data[i + 6];
                *w = (data[i + 7] << 8) | data[i + 8];
                return true;
            }
            if (marker == 0xDA || marker == 0xD9) { // SOS (Start of Scan) or EOI
                break;
            }
            size_t block_len = (data[i + 2] << 8) | data[i + 3];
            i += 2 + block_len;
        } else {
            i++;
        }
    }
    return false;
}

static bool getPngSize(const uint8_t* data, size_t len, int32_t* w, int32_t* h) {
    if (len < 24) return false;
    // PNG signature
    if (data[0] == 0x89 && data[1] == 0x50 && data[2] == 0x4E && data[3] == 0x47) {
        // IHDR chunk: width is at offset 16, height at 20
        *w = (data[16] << 24) | (data[17] << 16) | (data[18] << 8) | data[19];
        *h = (data[20] << 24) | (data[21] << 16) | (data[22] << 8) | data[23];
        return true;
    }
    return false;
}

static bool downloadLogo(const char* url) {
    if (!url || url[0] == '\0' || WiFi.status() != WL_CONNECTED) return false;

    HTTPClient http;
    http.setTimeout(5000);
    http.begin(url);
    int code = http.GET();
    if (code != HTTP_CODE_OK) {
        Serial.printf("[UI] Download failed: GET %s returned %d\n", url, code);
        http.end();
        return false;
    }

    int len = http.getSize();
    // Add 32 bytes padding to safely inject the dummy JFIF block if needed
    size_t allocSz = ((len > 0) ? (size_t)len : 2500 * 1024) + 32; 
    
    if (len <= 0) Serial.println("[UI] Logo size unknown (chunked?) - using 2.5MB buffer");

    if (allocSz > 3000 * 1024) { 
        Serial.printf("[UI] Logo too large: %d bytes\n", len);
        http.end(); 
        return false; 
    }

    if (!g_logoBuffer || g_logoBufferSz < allocSz) {
        free(g_logoBuffer);
        g_logoBuffer   = (uint8_t*)heap_caps_malloc(allocSz, MALLOC_CAP_SPIRAM);
        g_logoBufferSz = g_logoBuffer ? allocSz : 0;
    }
    if (!g_logoBuffer) { Serial.println("[UI] PSRAM alloc failed"); http.end(); return false; }

    // Use custom Stream wrapper to properly handle chunked decoding (writeToStream handles the decoding chunk headers)
    LogoStream ls(g_logoBuffer, g_logoBufferSz);
    int written = http.writeToStream(&ls);
    http.end();
    
    if (written <= 0) {
        Serial.printf("[UI] Download failed or empty: %d\n", written);
        return false;
    }
    
    if (len > 0 && written != len) {
        Serial.printf("[UI] Download mismatch: expected %d, got %d\n", len, written);
        return false;
    }

    size_t rx = (size_t)written;

    // Log the first few bytes to verify format (JPEG: FF D8 FF...)
    if (rx > 4) {
        Serial.printf("[UI] Header bytes: %02X %02X %02X %02X\n", 
                      g_logoBuffer[0], g_logoBuffer[1], g_logoBuffer[2], g_logoBuffer[3]);
    }

    // LVGL v9 BUG WORKAROUND:
    // LVGL's internal `is_jpg()` strictly requires an APP0 "JFIF" marker block
    // to identify a JPEG, but many APIs return valid stripped JPEGs (e.g. FF D8 FF DB...).
    // If we detect a stripped JPEG, we inject a dummy JFIF APP0 block.
    if (rx > 4 && g_logoBuffer[0] == 0xFF && g_logoBuffer[1] == 0xD8 && g_logoBuffer[2] != 0xE0) {
        const uint8_t jfif_app0[18] = {
            0xFF, 0xE0, 0x00, 0x10, 
            0x4A, 0x46, 0x49, 0x46, 0x00, 
            0x01, 0x01, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00
        };
        
        // Ensure we have enough capacity
        if (rx + 18 <= g_logoBufferSz) {
            memmove(&g_logoBuffer[2 + 18], &g_logoBuffer[2], rx - 2);
            memcpy(&g_logoBuffer[2], jfif_app0, 18);
            rx += 18;
            Serial.println("[UI] Applied dummy JFIF APP0 header to bypass LVGL v9 bug");
        } else {
            Serial.println("[UI] Buffer too small to inject JFIF header!");
        }
    }

    // LVGL v9 image descriptor (raw JPEG/PNG)
    g_logoDsc = {};
    g_logoDsc.header.magic    = LV_IMAGE_HEADER_MAGIC;
    g_logoDsc.header.cf       = LV_COLOR_FORMAT_RAW;
    g_logoDsc.header.w        = 0; 
    g_logoDsc.header.h        = 0;
    g_logoDsc.data_size       = (uint32_t)rx;
    g_logoDsc.data            = g_logoBuffer;

    // LVGL BUG WORKAROUND: Raw memory sources require explicit Width and Height!
    int32_t jw = 0, jh = 0;
    if (getJpegSize(g_logoBuffer, rx, &jw, &jh)) {
        g_logoDsc.header.cf = LV_COLOR_FORMAT_RAW;
        g_logoDsc.header.w = jw;
        g_logoDsc.header.h = jh;
        Serial.printf("[UI] Parsed physical JPEG dimensions: %lux%lu\n", jw, jh);
    } else if (getPngSize(g_logoBuffer, rx, &jw, &jh)) {
        g_logoDsc.header.cf = LV_COLOR_FORMAT_RAW_ALPHA;
        g_logoDsc.header.w = jw;
        g_logoDsc.header.h = jh;
        Serial.printf("[UI] Parsed physical PNG dimensions: %lux%lu\n", jw, jh);
    } else {
        Serial.printf("[UI] Failed to parse image dimensions. LVGL decoding might abort.\n");
    }

    Serial.printf("[UI] MEMFS Driver for 'M': %s\n", lv_fs_is_ready('M') ? "READY" : "NOT READY");
    Serial.printf("[UI] Logo: %u bytes received (Buffer: %p, DSC Size: %u)\n", (uint32_t)rx, g_logoBuffer, g_logoDsc.data_size);
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

    g_logoContainer = lv_obj_create(g_screenIdle);
    lv_obj_set_size(g_logoContainer, 300, 300);
    lv_obj_set_style_radius(g_logoContainer, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_clip_corner(g_logoContainer, true, 0);
    lv_obj_set_style_border_width(g_logoContainer, 0, 0);
    lv_obj_clear_flag(g_logoContainer, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(g_logoContainer, LV_ALIGN_CENTER, 0, -120);
    lv_obj_add_flag(g_logoContainer, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(g_logoContainer, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(g_logoContainer, logoTapCb, LV_EVENT_CLICKED, nullptr);

    g_imgLogo = lv_image_create(g_logoContainer);
    lv_obj_center(g_imgLogo);

    g_labelName = lv_label_create(g_screenIdle);
    lv_obj_set_style_text_font(g_labelName, &lv_font_montserrat_36, 0);
    lv_obj_set_style_text_color(g_labelName, COL_WHITE, 0);
    lv_obj_set_style_text_align(g_labelName, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(g_labelName, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(g_labelName, 440);
    lv_obj_align(g_labelName, LV_ALIGN_BOTTOM_MID, 0, -220);
    lv_obj_add_flag(g_labelName, LV_OBJ_FLAG_HIDDEN);

    g_labelAbv = lv_label_create(g_screenIdle);
    lv_obj_set_style_text_font(g_labelAbv, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(g_labelAbv, COL_ACCENT, 0);
    lv_obj_align(g_labelAbv, LV_ALIGN_BOTTOM_MID, -110, -160);
    lv_obj_add_flag(g_labelAbv, LV_OBJ_FLAG_HIDDEN);

    g_labelVolume = lv_label_create(g_screenIdle);
    lv_obj_set_style_text_font(g_labelVolume, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(g_labelVolume, COL_WHITE, 0);
    lv_obj_align(g_labelVolume, LV_ALIGN_BOTTOM_MID, 110, -160);
    lv_obj_add_flag(g_labelVolume, LV_OBJ_FLAG_HIDDEN);

    g_labelEmpty = lv_label_create(g_screenIdle);
    lv_label_set_text(g_labelEmpty, LV_SYMBOL_WARNING "\n\nTom kran");
    lv_obj_set_style_text_font(g_labelEmpty, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(g_labelEmpty, COL_ACCENT, 0);
    lv_obj_set_style_text_align(g_labelEmpty, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(g_labelEmpty);
    lv_obj_add_flag(g_labelEmpty, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t* hint = lv_label_create(g_screenIdle);
    lv_label_set_text(hint, "Trykk paa logo for aa registrere tapping");
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(hint, COL_GREY, 0);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -20);

    lv_obj_t* settingsBtn = lv_button_create(g_screenIdle);
    lv_obj_set_size(settingsBtn, 60, 60);
    lv_obj_align(settingsBtn, LV_ALIGN_TOP_RIGHT, -10, 10);
    lv_obj_set_style_bg_color(settingsBtn, COL_BTN_BG, 0);
    lv_obj_set_style_radius(settingsBtn, 8, 0);
    lv_obj_add_event_cb(settingsBtn, settingsBtnCb, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* settingsLbl = lv_label_create(settingsBtn);
    lv_label_set_text(settingsLbl, LV_SYMBOL_SETTINGS);
    lv_obj_set_style_text_font(settingsLbl, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(settingsLbl, COL_WHITE, 0);
    lv_obj_center(settingsLbl);
}

static void buildPourScreen() {
    lv_obj_t* title = lv_label_create(g_screenPour);
    lv_label_set_text(title, "Velg stoerrelse");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_36, 0);
    lv_obj_set_style_text_color(title, COL_WHITE, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 30);

    static const float  pourSizes[]  = {0.25f, 0.33f, 0.44f, 0.50f};
    static const char*  pourLabels[] = {"0,25 L", "0,33 L", "0,44 L", "0,50 L"};

    const int btnW = 400, btnH = 100, gapY = 20;
    for (int i = 0; i < 4; i++) {
        int yPos = -120 + i * (btnH + gapY);

        lv_obj_t* btn = lv_button_create(g_screenPour);
        lv_obj_set_size(btn, btnW, btnH);
        lv_obj_align(btn, LV_ALIGN_CENTER, 0, yPos);
        lv_obj_set_style_bg_color(btn, COL_ACCENT, 0); // Orange by default
        lv_obj_set_style_bg_color(btn, COL_BTN_BG, LV_STATE_PRESSED); // Dark grey on press
        lv_obj_set_style_radius(btn, 16, 0);
        lv_obj_add_event_cb(btn, pourBtnCb, LV_EVENT_CLICKED, (void*)&pourSizes[i]);

        lv_obj_t* lbl = lv_label_create(btn);
        lv_label_set_text(lbl, pourLabels[i]);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_32, 0);
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
    lv_obj_set_style_text_font(cancelLbl, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(cancelLbl, COL_WHITE, 0);
    lv_obj_center(cancelLbl);
}

static void buildConfigScreen() {
    lv_obj_t* title = lv_label_create(g_screenConfig);
    lv_label_set_text(title, "Velg Kran");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_36, 0);
    lv_obj_set_style_text_color(title, COL_WHITE, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 30);

    g_tapListContainer = lv_obj_create(g_screenConfig);
    lv_obj_set_size(g_tapListContainer, 440, 300);
    lv_obj_align(g_tapListContainer, LV_ALIGN_CENTER, 0, -10);
    lv_obj_set_style_bg_opa(g_tapListContainer, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(g_tapListContainer, 0, 0);
    
    // Sett opp flex layout for knappene
    lv_obj_set_flex_flow(g_tapListContainer, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_row(g_tapListContainer, 15, 0);
    lv_obj_set_style_pad_column(g_tapListContainer, 15, 0);
    lv_obj_set_flex_align(g_tapListContainer, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    // Bare avbryt-knapp trengs nå
    int btnW = 180, btnH = 80;
    lv_obj_t* cancelBtn = lv_button_create(g_screenConfig);
    lv_obj_set_size(cancelBtn, btnW, btnH);
    lv_obj_align(cancelBtn, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_set_style_bg_color(cancelBtn, COL_CANCEL, 0);
    lv_obj_set_style_radius(cancelBtn, 12, 0);
    lv_obj_add_event_cb(cancelBtn, cancelConfigBtnCb, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* cancelLbl = lv_label_create(cancelBtn);
    lv_label_set_text(cancelLbl, "Avbryt");
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
        g_lastRefreshMs = 0; // Force immediate API fetch in main loop
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

static void tapSelectedCb(lv_event_t* e) {
    int tapId = *(int*)lv_event_get_user_data(e);
    char newId[16];
    snprintf(newId, sizeof(newId), "%d", tapId);
    settingsSetTapId(newId);
    Serial.printf("[UI] Saved new tap ID %s, restarting...\n", newId);
    delay(100);
    ESP.restart();
}

static void settingsBtnCb(lv_event_t* e) {
    (void)e;
    
    // Tøm container for gamle knapper
    lv_obj_clean(g_tapListContainer);

    if (fetchTapsList(g_tapList)) {
        if (g_tapList.count == 0) {
            lv_obj_t* lbl = lv_label_create(g_tapListContainer);
            lv_label_set_text(lbl, "Ingen kraner funnet");
            lv_obj_set_style_text_color(lbl, COL_WHITE, 0);
        } else {
            const char* currentIdStr = settingsGetTapId();
            int currentId = atoi(currentIdStr);

            for (int i = 0; i < g_tapList.count; i++) {
                lv_obj_t* btn = lv_button_create(g_tapListContainer);
                lv_obj_set_size(btn, 170, 80);
                
                // Gjør den aktive kranen grønn, resten oransje
                if (g_tapList.taps[i].id == currentId) {
                    lv_obj_set_style_bg_color(btn, COL_GREEN, 0);
                } else {
                    lv_obj_set_style_bg_color(btn, COL_ACCENT, 0); // Oransje
                    lv_obj_set_style_bg_color(btn, COL_BTN_BG, LV_STATE_PRESSED);
                }
                lv_obj_set_style_radius(btn, 12, 0);
                lv_obj_add_event_cb(btn, tapSelectedCb, LV_EVENT_CLICKED, &g_tapList.taps[i].id);

                lv_obj_t* lbl = lv_label_create(btn);
                char buf[64];
                snprintf(buf, sizeof(buf), "Kran %s", g_tapList.taps[i].name);
                lv_label_set_text(lbl, buf);
                lv_obj_set_style_text_font(lbl, &lv_font_montserrat_28, 0);
                lv_obj_set_style_text_color(lbl, COL_WHITE, 0);
                
                // Forhindre at lange navn sprenger knappen:
                lv_label_set_long_mode(lbl, LV_LABEL_LONG_SCROLL_CIRCULAR);
                lv_obj_set_width(lbl, 150);
                
                lv_obj_center(lbl);
            }
        }
    } else {
        lv_obj_t* lbl = lv_label_create(g_tapListContainer);
        lv_label_set_text(lbl, "Feil ved oppslag");
        lv_obj_set_style_text_color(lbl, COL_WHITE, 0);
    }

    lv_screen_load_anim(g_screenConfig, LV_SCR_LOAD_ANIM_MOVE_LEFT, 200, 0, false);
}

static void cancelConfigBtnCb(lv_event_t* e) {
    (void)e;
    lv_screen_load_anim(g_screenIdle, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 200, 0, false);
}
