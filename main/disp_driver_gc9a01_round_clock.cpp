#include "disp_driver_gc9a01_round_clock.h"

#include "esp_log.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_gc9a01.h"
#include "esp_lvgl_port.h"
#include "esp_timer.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <sys/time.h>

static const char* TAG = "disp_gc9a01";

// ---- Pin mapping, confirmed working on this board (see
// gc9a01_round_display_test's bring-up notes) ----
#define LCD_SPI_HOST    SPI2_HOST
#define PIN_LCD_SCLK    GPIO_NUM_6
#define PIN_LCD_MOSI    GPIO_NUM_7
#define PIN_LCD_MISO    GPIO_NUM_NC   // display is write-only, no MISO
#define PIN_LCD_CS      GPIO_NUM_10
#define PIN_LCD_DC      GPIO_NUM_2
#define PIN_LCD_RST     GPIO_NUM_NC   // no dedicated reset line on this board
#define PIN_LCD_BL      GPIO_NUM_3

#define LCD_H_RES       240
#define LCD_V_RES       240
#define LCD_BITS_PER_PIXEL 16
#define LCD_SPI_CLOCK_HZ   (20 * 1000 * 1000)

#define CLOCK_CENTER_X   (LCD_H_RES / 2)
#define CLOCK_CENTER_Y   (LCD_V_RES / 2)
#define HOUR_HAND_LEN    55
#define MIN_HAND_LEN     85
#define SEC_HAND_LEN     100

#define TIME_DIGIT_W 20
#define TIME_COLON_W 8
#define TIME_ROW_Y   60

DispDriverGc9a01RoundClock::DispDriverGc9a01RoundClock() {
    std::memset(_statusBuf, 0, sizeof(_statusBuf));
}

// --- Panel + LVGL bring-up ---------------------------------------------------

void DispDriverGc9a01RoundClock::begin() {
    if (_began) return;

    ESP_LOGI(TAG, "configuring backlight on GPIO%d", PIN_LCD_BL);
    gpio_config_t bl_cfg = {};
    bl_cfg.pin_bit_mask = 1ULL << PIN_LCD_BL;
    bl_cfg.mode = GPIO_MODE_OUTPUT;
    ESP_ERROR_CHECK(gpio_config(&bl_cfg));
    gpio_set_level(PIN_LCD_BL, 1);

    ESP_LOGI(TAG, "initializing SPI bus on SCLK=%d MOSI=%d", PIN_LCD_SCLK, PIN_LCD_MOSI);
    spi_bus_config_t buscfg = {};
    buscfg.sclk_io_num = PIN_LCD_SCLK;
    buscfg.mosi_io_num = PIN_LCD_MOSI;
    buscfg.miso_io_num = PIN_LCD_MISO;
    buscfg.quadwp_io_num = -1;
    buscfg.quadhd_io_num = -1;
    buscfg.max_transfer_sz = LCD_H_RES * 40 * sizeof(uint16_t);
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO));

    ESP_LOGI(TAG, "attaching panel IO, CS=%d DC=%d", PIN_LCD_CS, PIN_LCD_DC);
    esp_lcd_panel_io_spi_config_t io_config = GC9A01_PANEL_IO_SPI_CONFIG(
        PIN_LCD_CS, PIN_LCD_DC, nullptr, nullptr);
    io_config.pclk_hz = LCD_SPI_CLOCK_HZ;
    io_config.lcd_cmd_bits = 8;
    io_config.lcd_param_bits = 8;
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_SPI_HOST,
                                              &io_config, &_ioHandle));

    ESP_LOGI(TAG, "creating GC9A01 panel, RST=%d", PIN_LCD_RST);
    esp_lcd_panel_dev_config_t panel_config = {};
    panel_config.reset_gpio_num = PIN_LCD_RST;
    panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR; // confirmed by test; RGB swaps red/blue
    panel_config.bits_per_pixel = LCD_BITS_PER_PIXEL;
    ESP_ERROR_CHECK(esp_lcd_new_panel_gc9a01(_ioHandle, &panel_config, &_panelHandle));

    ESP_ERROR_CHECK(esp_lcd_panel_reset(_panelHandle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(_panelHandle));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(_panelHandle, true));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(_panelHandle, true));

    ESP_LOGI(TAG, "starting LVGL port");
    const lvgl_port_cfg_t lvgl_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    ESP_ERROR_CHECK(lvgl_port_init(&lvgl_cfg));

    lvgl_port_display_cfg_t disp_cfg = {};
    disp_cfg.io_handle = _ioHandle;
    disp_cfg.panel_handle = _panelHandle;
    disp_cfg.buffer_size = LCD_H_RES * 40;
    disp_cfg.double_buffer = true;
    disp_cfg.hres = LCD_H_RES;
    disp_cfg.vres = LCD_V_RES;
    disp_cfg.monochrome = false;
    disp_cfg.color_format = LV_COLOR_FORMAT_RGB565;
    // This physical panel needs a horizontal mirror to render right-reading —
    // must be set here (not via a separate esp_lcd_panel_mirror() call before
    // lvgl_port_add_disp()) because lvgl_port_add_disp() re-applies orientation
    // from this struct internally, silently overwriting anything set earlier.
    disp_cfg.rotation.mirror_x = true;
    disp_cfg.flags.buff_dma = true;
    disp_cfg.flags.swap_bytes = true;
    lv_disp_t* disp = lvgl_port_add_disp(&disp_cfg);
    (void)disp;

    ESP_LOGI(TAG, "building UI");
    lvgl_port_lock(0);
    buildUi();
    lvgl_port_unlock();

    _tickTimer = lv_timer_create(tickTimerCb, 30, this);

    _began = true;
}

// --- UI construction ---------------------------------------------------------

static lv_obj_t* createHand(lv_obj_t* parent, int16_t width, lv_color_t color) {
    lv_obj_t* hand = lv_line_create(parent);
    lv_obj_set_style_line_width(hand, width, 0);
    lv_obj_set_style_line_color(hand, color, 0);
    lv_obj_set_style_line_rounded(hand, true, 0);
    return hand;
}

static lv_obj_t* createTimeSlot(lv_obj_t* parent, int16_t width, int16_t x_offset, const char* text) {
    lv_obj_t* lbl = lv_label_create(parent);
    lv_obj_set_width(lbl, width);
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
    lv_obj_align(lbl, LV_ALIGN_CENTER, x_offset, TIME_ROW_Y);
    if (text) lv_label_set_text(lbl, text);
    return lbl;
}

void DispDriverGc9a01RoundClock::buildUi() {
    // --- boot screen: status text line (shown until RUNNING_NORMAL) ---
    _bootScreen = lv_scr_act();
    lv_obj_set_style_bg_color(_bootScreen, lv_color_black(), 0);

    _statusLabel = lv_label_create(_bootScreen);
    lv_obj_set_width(_statusLabel, 200);
    lv_label_set_long_mode(_statusLabel, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(_statusLabel, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(_statusLabel, lv_color_white(), 0);
    lv_obj_center(_statusLabel);
    lv_label_set_text(_statusLabel, "");

    // --- watch face: its own screen, loaded via showClockFace() ---
    _faceScreen = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(_faceScreen, lv_color_black(), 0);

    // outer bezel ring
    lv_obj_t* ring = lv_obj_create(_faceScreen);
    lv_obj_remove_style_all(ring);
    lv_obj_set_size(ring, 232, 232);
    lv_obj_center(ring);
    lv_obj_set_style_radius(ring, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(ring, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(ring, 2, 0);
    lv_obj_set_style_border_color(ring, lv_color_white(), 0);

    // hour tick marks
    for (int i = 0; i < 12; i++) {
        float rad = i * 30.0f * ((float)M_PI / 180.0f);
        int16_t r_out = 112, r_in = 98;
        static lv_point_precise_t tick_pts[12][2];
        tick_pts[i][0].x = CLOCK_CENTER_X + (int16_t)(r_out * sinf(rad));
        tick_pts[i][0].y = CLOCK_CENTER_Y - (int16_t)(r_out * cosf(rad));
        tick_pts[i][1].x = CLOCK_CENTER_X + (int16_t)(r_in * sinf(rad));
        tick_pts[i][1].y = CLOCK_CENTER_Y - (int16_t)(r_in * cosf(rad));

        lv_obj_t* tick = lv_line_create(_faceScreen);
        lv_obj_set_pos(tick, 0, 0);
        lv_obj_set_size(tick, LCD_H_RES, LCD_V_RES);
        lv_line_set_points(tick, tick_pts[i], 2);
        lv_obj_set_style_line_width(tick, 3, 0);
        lv_obj_set_style_line_color(tick, lv_color_white(), 0);
    }

    _hourHand = createHand(_faceScreen, 6, lv_color_white());
    _minHand = createHand(_faceScreen, 4, lv_color_white());
    _secHand = createHand(_faceScreen, 3, lv_palette_main(LV_PALETTE_RED));

    // center hub, drawn last so it sits above the hands
    lv_obj_t* hub = lv_obj_create(_faceScreen);
    lv_obj_remove_style_all(hub);
    lv_obj_set_size(hub, 10, 10);
    lv_obj_center(hub);
    lv_obj_set_style_radius(hub, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(hub, lv_palette_main(LV_PALETTE_RED), 0);
    lv_obj_set_style_bg_opa(hub, LV_OPA_COVER, 0);

    // digital readout, below center: fixed-position slots (see header comment
    // on why each digit gets its own fixed-width slot rather than one label)
    _digitSlots[0] = createTimeSlot(_faceScreen, TIME_DIGIT_W, -58, nullptr);
    _digitSlots[1] = createTimeSlot(_faceScreen, TIME_DIGIT_W, -38, nullptr);
    _colonSlots[0] = createTimeSlot(_faceScreen, TIME_COLON_W, -24, ":");
    _digitSlots[2] = createTimeSlot(_faceScreen, TIME_DIGIT_W, -10, nullptr);
    _digitSlots[3] = createTimeSlot(_faceScreen, TIME_DIGIT_W, 10, nullptr);
    _colonSlots[1] = createTimeSlot(_faceScreen, TIME_COLON_W, 24, ":");
    _digitSlots[4] = createTimeSlot(_faceScreen, TIME_DIGIT_W, 38, nullptr);
    _digitSlots[5] = createTimeSlot(_faceScreen, TIME_DIGIT_W, 58, nullptr);

    // Info label: same row, shown instead of the digit slots when cycling
    // to temperature/humidity. Hidden until the rotation selects it.
    _infoLabel = lv_label_create(_faceScreen);
    lv_obj_set_style_text_font(_infoLabel, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(_infoLabel, lv_color_white(), 0);
    lv_obj_align(_infoLabel, LV_ALIGN_CENTER, 0, TIME_ROW_Y);
    lv_obj_add_flag(_infoLabel, LV_OBJ_FLAG_HIDDEN);
}

// --- Watch face ticking -------------------------------------------------------

static void setHand(lv_obj_t* hand, lv_point_precise_t* pts, float angle_deg, int16_t len) {
    float rad = angle_deg * ((float)M_PI / 180.0f);
    int16_t cx = CLOCK_CENTER_X, cy = CLOCK_CENTER_Y;
    int16_t tx = cx + (int16_t)roundf(len * sinf(rad));
    int16_t ty = cy - (int16_t)roundf(len * cosf(rad));

    int16_t min_x = (cx < tx) ? cx : tx;
    int16_t max_x = (cx > tx) ? cx : tx;
    int16_t min_y = (cy < ty) ? cy : ty;
    int16_t max_y = (cy > ty) ? cy : ty;
    const int16_t pad = 4; // headroom for line width + rounded caps

    int16_t ox = min_x - pad;
    int16_t oy = min_y - pad;
    int16_t ow = (max_x - min_x) + 2 * pad;
    int16_t oh = (max_y - min_y) + 2 * pad;

    pts[0].x = cx - ox;
    pts[0].y = cy - oy;
    pts[1].x = tx - ox;
    pts[1].y = ty - oy;

    lv_obj_set_pos(hand, ox, oy);
    lv_obj_set_size(hand, ow, oh);
    lv_line_set_points(hand, pts, 2);
}

static void setDigit(lv_obj_t* slot, uint32_t value) {
    char buf[2] = { (char)('0' + value), '\0' };
    lv_label_set_text(slot, buf);
}

// "Temporal anomaly" wobble: the watch face displays time running faster
// and slower than reality, but stays exactly correct on average. Modeled as
// a sinusoidal *rate* around 1.0x: rate(t) = 1 + A*sin(2*pi*t/P). Integrating
// that rate gives the displayed-time offset from real time:
//   offset(t) = C * (1 - cos(2*pi*t/P)),  C = A*P/(2*pi)
// offset(t) is periodic with period P, so offset(t+60) - offset(t) is
// exactly zero whenever P evenly divides 60 — meaning displayed time gains
// exactly as much as it loses over *any* 60-second window, not just ones
// aligned to a particular phase. This holds regardless of amplitude, so
// with A > 1 the rate swings negative for part of each cycle (min is 1-A)
// and the hands/digits visibly run backwards for a moment before catching
// back up — still exactly correct on average.
// This only perturbs what's drawn on the watch face — real system time
// (NTP, logs, preferences) is untouched; see tickWatchFace(). Period and
// amplitude are configurable via the portal — see setAnomalyParams().
double DispDriverGc9a01RoundClock::anomalyDisplayTime(double real_sec) const {
    double c = _anomalyAmplitude * _anomalyPeriodSec / (2.0 * M_PI);
    double phase = 2.0 * M_PI * real_sec / _anomalyPeriodSec;
    return real_sec + c * (1.0 - cos(phase));
}

void DispDriverGc9a01RoundClock::setAnomalyParams(double periodSec, double amplitude) {
    _anomalyPeriodSec = (periodSec > 0.0) ? periodSec : 1.0;
    _anomalyAmplitude = amplitude;
}

void DispDriverGc9a01RoundClock::tickWatchFace() {
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    double real_sec = (double)tv.tv_sec + tv.tv_usec / 1000000.0;
    double total_sec = anomalyDisplayTime(real_sec);

    // total_sec is still a UTC epoch value (the anomaly wobble only adds a
    // small offset in seconds, timezone-agnostic). Break it down via
    // localtime_r() rather than raw modulo arithmetic so the face respects
    // the configured/detected timezone (see sntp_client.cpp's setenv("TZ",
    // ...)/tzset()) instead of always showing UTC.
    double frac = total_sec - floor(total_sec);
    time_t epoch = (time_t)total_sec;
    struct tm tm_local;
    localtime_r(&epoch, &tm_local);

    double s = tm_local.tm_sec + frac;
    uint32_t m = (uint32_t)tm_local.tm_min;
    uint32_t h = (uint32_t)(tm_local.tm_hour % 12);
    uint32_t sec_whole = (uint32_t)tm_local.tm_sec;

    // Hands always run, regardless of what the info row below is showing —
    // they're the "at a glance" clock and shouldn't disappear while the
    // rotation is on temperature/humidity.
    setHand(_hourHand, _hourPts, h * 30.0f + m * 0.5f, HOUR_HAND_LEN);
    setHand(_minHand, _minPts, m * 6.0f + (float)s * 0.1f, MIN_HAND_LEN);
    setHand(_secHand, _secPts, (float)s * 6.0f, SEC_HAND_LEN);

    // Info row: cycles time -> temperature -> humidity -> time. Skips
    // temperature/humidity entirely (stays on TIME) until a weather
    // reading has actually succeeded at least once.
    int64_t now_us = esp_timer_get_time();
    if (now_us - _faceModeSinceUs > kFaceModeDurationUs) {
        _faceModeSinceUs = now_us;
        do {
            _faceMode = static_cast<FaceMode>((static_cast<int>(_faceMode) + 1) % 3);
        } while (_faceMode != FaceMode::TIME && !_weatherValid);
        updateFaceModeVisibility();
    }

    if (_faceMode == FaceMode::TIME && (uint32_t)epoch != _digitsShownSec) {
        _digitsShownSec = (uint32_t)epoch;
        uint32_t hh = h;
        if (hh == 0) hh = 12;
        setDigit(_digitSlots[0], hh / 10);
        setDigit(_digitSlots[1], hh % 10);
        setDigit(_digitSlots[2], m / 10);
        setDigit(_digitSlots[3], m % 10);
        setDigit(_digitSlots[4], sec_whole / 10);
        setDigit(_digitSlots[5], sec_whole % 10);
    }
}

void DispDriverGc9a01RoundClock::updateFaceModeVisibility() {
    bool showDigits = (_faceMode == FaceMode::TIME);
    for (lv_obj_t* slot : _digitSlots) {
        if (showDigits) lv_obj_clear_flag(slot, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(slot, LV_OBJ_FLAG_HIDDEN);
    }
    for (lv_obj_t* colon : _colonSlots) {
        if (showDigits) lv_obj_clear_flag(colon, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(colon, LV_OBJ_FLAG_HIDDEN);
    }

    if (showDigits) {
        lv_obj_add_flag(_infoLabel, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    lv_obj_clear_flag(_infoLabel, LV_OBJ_FLAG_HIDDEN);
    char buf[16];
    if (_faceMode == FaceMode::TEMPERATURE) {
        snprintf(buf, sizeof(buf), "%.0f F", _tempF);
    } else {
        snprintf(buf, sizeof(buf), "%d%% RH", _humidity);
    }
    lv_label_set_text(_infoLabel, buf);
}

void DispDriverGc9a01RoundClock::setWeatherData(float tempF, int humidity, bool valid) {
    _tempF = tempF;
    _humidity = humidity;
    _weatherValid = valid;
}

void DispDriverGc9a01RoundClock::tickTimerCb(lv_timer_t* timer) {
    auto* self = static_cast<DispDriverGc9a01RoundClock*>(lv_timer_get_user_data(timer));
    self->tickWatchFace();
}

void DispDriverGc9a01RoundClock::showClockFace(bool show) {
    if (!_began) return;
    lvgl_port_lock(0);
    lv_scr_load(show ? _faceScreen : _bootScreen);
    lvgl_port_unlock();
    _showingFace = show;
}

// --- IDisplayDriver: status-text bridge --------------------------------------
// No real segment glass behind this driver — setSegments()/setDot() are
// accepted but no-op, and mapAsciiToSegment() has nothing meaningful to
// encode. Only the plain-text path (setChar/setBuffer/writeDisplay) does
// anything, which is all the boot/status messages this app shows need.

void DispDriverGc9a01RoundClock::setBrightness(uint8_t /*level*/) {
    // Backlight is a plain GPIO on/off, not PWM-driven — nothing to scale.
}

void DispDriverGc9a01RoundClock::clear() {
    std::memset(_statusBuf, 0, sizeof(_statusBuf));
}

void DispDriverGc9a01RoundClock::setChar(int position, char character, bool /*dot*/) {
    if (position < 0 || position >= kStatusCells) return;
    _statusBuf[position] = character;
}

void DispDriverGc9a01RoundClock::setSegments(int /*position*/, uint16_t /*mask*/) {}

void DispDriverGc9a01RoundClock::setDot(int /*position*/, bool /*on*/) {}

unsigned long DispDriverGc9a01RoundClock::mapAsciiToSegment(char ascii_char, bool /*dot*/) {
    return (unsigned long)(unsigned char)ascii_char;
}

void DispDriverGc9a01RoundClock::setBuffer(const std::vector<unsigned long>& newBuffer) {
    std::memset(_statusBuf, 0, sizeof(_statusBuf));
    size_t n = newBuffer.size() < (size_t)kStatusCells ? newBuffer.size() : (size_t)kStatusCells;
    for (size_t i = 0; i < n; i++) {
        _statusBuf[i] = (char)newBuffer[i];
    }
}

void DispDriverGc9a01RoundClock::writeDisplay() {
    if (!_began) return;
    char text[kStatusCells + 1];
    std::memcpy(text, _statusBuf, kStatusCells);
    text[kStatusCells] = '\0';

    lvgl_port_lock(0);
    lv_label_set_text(_statusLabel, text);
    lvgl_port_unlock();
}

void DispDriverGc9a01RoundClock::getFrameData(unsigned long* buffer) {
    for (int i = 0; i < kStatusCells; i++) {
        buffer[i] = (unsigned long)(unsigned char)_statusBuf[i];
    }
}
