// disp_driver_gc9a01_round_clock.h — GC9A01 round LCD driver for this app.
//
// Two responsibilities layered on one LVGL screen:
//
//   1. IDisplayDriver — satisfies BaseNtpClockApp/ClockFsmManager/
//      SceneManager's character-grid contract so the engine's boot/WiFi/
//      NTP/AP-mode status messages keep working unmodified. There's no
//      real segment glass behind this: setChar()/setBuffer() etc. just
//      maintain a plain text buffer, and writeDisplay() pushes it to an
//      LVGL status label. Segment-mask methods are accepted but no-op —
//      nothing in this app drives segment-style scene animations (the
//      SceneManager playlist is empty; see temporal_anomaly_app.cpp).
//
//   2. The analog + digital watch face — ported verbatim from
//      ~/projects/0_incubator/gc9a01_round_display_test (pin map, BGR
//      color order, tight-bounding-box + roundf() hand rendering, the
//      six-slot digit readout). Lives on its own LVGL screen (not nested
//      in a container on the boot screen) so its elements are direct
//      children of a screen-level object, exactly like the incubator
//      project's proven-correct structure; showClockFace() switches
//      between the two screens via lv_scr_load() instead of a
//      show/hide flag on a wrapping container.
//
// Hardware confirmed working on the JCZN "ESP32-2424S012"-style board:
// SCLK=6, MOSI=7, CS=10, DC=2, no dedicated RST (software reset), BL=3.

#pragma once

#include "i_display_driver.h"

#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "lvgl.h"

#include <cstdint>
#include <vector>

class DispDriverGc9a01RoundClock : public IDisplayDriver {
public:
    DispDriverGc9a01RoundClock();

    // --- IDisplayDriver ----------------------------------------------------
    void begin() override;
    int  getDisplaySize() override { return kStatusCells; }
    void setBrightness(uint8_t level) override;
    void clear() override;
    void setChar(int position, char character, bool dot = false) override;
    void setSegments(int position, uint16_t mask) override;
    void setDot(int position, bool on) override;
    unsigned long mapAsciiToSegment(char ascii_char, bool dot) override;
    void setBuffer(const std::vector<unsigned long>& newBuffer) override;
    void writeDisplay() override;
    bool needsContinuousUpdate() const override { return false; }
    void getFrameData(unsigned long* buffer) override;

    // Swap between the boot/status text line and the watch face. Both are
    // already built at begin()-time; this just toggles visibility.
    void showClockFace(bool show);

    // Configures the watch face's time-wobble effect (see anomalyDisplayTime()
    // in the .cpp for the math). periodSec must be > 0; clamped to 1 if not.
    // amplitude has no hard constraint — 1.0 is steady real time, >1.0 lets
    // the hands run backwards for part of each cycle.
    void setAnomalyParams(double periodSec, double amplitude);

    // Feeds the latest weather reading in for the temperature/humidity face
    // rotation. Safe to call every app loop() tick; only used when valid.
    void setWeatherData(float tempF, int humidity, bool valid);

private:
    static constexpr int kStatusCells = 32;

    void buildUi();
    void tickWatchFace();
    static void tickTimerCb(lv_timer_t* timer);
    double anomalyDisplayTime(double real_sec) const;
    void updateFaceModeVisibility();

    char _statusBuf[kStatusCells + 1] = {};
    bool _began = false;
    bool _showingFace = false;

    lv_obj_t* _bootScreen = nullptr;
    lv_obj_t* _statusLabel = nullptr;
    lv_obj_t* _faceScreen = nullptr;

    // Watch face objects/state (see gc9a01_round_display_test for the
    // rendering approach these mirror). Each hand is drawn as kHandSegs
    // stacked line segments of decreasing width and a color ramp (via
    // lv_color_mix()) for a tapered "sword hand" look with a smooth-ish
    // gradient — a plain lv_line can't taper or gradient a single stroke.
    // 6 segments (rather than the original 3) to make the color/width
    // steps small enough that the banding isn't obviously visible.
    static constexpr int kHandSegs = 6;
    lv_obj_t* _hourSegs[kHandSegs] = {};
    lv_obj_t* _minSegs[kHandSegs] = {};
    lv_obj_t* _secSegs[kHandSegs] = {};
    lv_point_precise_t _hourSegPts[kHandSegs][2] = {};
    lv_point_precise_t _minSegPts[kHandSegs][2] = {};
    lv_point_precise_t _secSegPts[kHandSegs][2] = {};
    lv_obj_t* _digitSlots[6] = {}; // H tens, H ones, M tens, M ones, S tens, S ones
    lv_obj_t* _colonSlots[2] = {};
    uint32_t _digitsShownSec = UINT32_MAX;

    // Info-row rotation: alternates the digit readout between time,
    // temperature, and humidity every kFaceModeDurationUs. Hands keep
    // ticking continuously regardless of mode — see tickWatchFace().
    enum class FaceMode { TIME, TEMPERATURE, HUMIDITY };
    static constexpr int64_t kFaceModeDurationUs = 5 * 1000000; // 5s per mode
    FaceMode _faceMode = FaceMode::TIME;
    int64_t _faceModeSinceUs = 0;
    lv_obj_t* _infoLabel = nullptr;
    float _tempF = 0.0f;
    int _humidity = 0;
    bool _weatherValid = false;

    // Watch-face time-wobble params; see setAnomalyParams(). Defaults match
    // the values this app used before they became configurable.
    double _anomalyPeriodSec = 10.0;
    double _anomalyAmplitude = 1.4;

    esp_lcd_panel_io_handle_t _ioHandle = nullptr;
    esp_lcd_panel_handle_t _panelHandle = nullptr;
    lv_timer_t* _tickTimer = nullptr;
};
