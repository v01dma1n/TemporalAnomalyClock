// temporal_anomaly_app.h — the TemporalAnomalyClock application.
//
// Mirrors MoodWhisperer's WhispererApp in structure: one class that
// inherits BaseNtpClockApp, owns a display driver and a DisplayManager,
// and supplies the IBaseClock/IWeatherClock implementations the engine
// wants.
//
// Display: was a MAX6921-driven VFD tube, now a round GC9A01 LCD running
// an analog watch face — see disp_driver_gc9a01_round_clock.h. The
// engine's boot/status text (WiFi connect, NTP sync, AP mode) renders
// through the same IDisplayDriver contract as any other app, bridged to
// an LVGL label; the watch face takes over once the FSM reaches
// RUNNING_NORMAL (see loop()).
//
// No external RTC on this board — NTP-only timekeeping. hasRtcTime()
// keeps IBaseClock's default (false); if NTP hasn't synced yet, the FSM
// falls back to AP_MODE the same way it would for any RTC-less app.
//
// Dropped vs the Arduino original: the per-minute Matrix animation
// (VFD-specific, no equivalent on a pixel LCD). The original's "anomaly
// level" preference was dropped too (it was dead code there) but the name
// is back as a real, from-scratch feature — see
// temporal_anomaly_time_source.h and temporal_anomaly_preferences.h.
//
// _timeSource owns the "what time to display" logic (sinusoidal wobble +
// chaos levels) and is wired into _display via setTimeProvider() in
// setupHardware() — the display driver itself has no idea any of that
// exists, it just renders whatever time it's handed each tick.

#pragma once

#include "ESP32NTPClock.h"
#include "disp_driver_gc9a01_round_clock.h"
#include "temporal_anomaly_preferences.h"
#include "temporal_anomaly_access_point_manager.h"
#include "temporal_anomaly_time_source.h"

#include <memory>

class TemporalAnomalyClockApp : public BaseNtpClockApp, public virtual IWeatherClock {
public:
    static TemporalAnomalyClockApp& getInstance();

    ~TemporalAnomalyClockApp() override;

    void setup() override;
    void loop() override;
    void setupHardware() override;

    TemporalAnomalyPreferences& getPrefs() { return _appPrefs; }

    // --- IBaseClock ---------------------------------------------------------
    const char* getAppName()  const override { return APP_HOST_NAME; }
    const char* getSsid()     const override { return _appPrefs.config.ssid; }
    const char* getPassword() const override { return _appPrefs.config.password; }
    const char* getTimezone() const override { return _appPrefs.config.time_zone; }

    IDisplayDriver& getDisplay() override { return _display; }
    DisplayManager& getClock()   override { return *_displayManager; }

    bool isOkToRunScenes() const override;

    // --- IWeatherClock --------------------------------------------------------
    const char* getOwmApiKey() const override { return _appPrefs.config.owmApiKey; }
    const char* getOwmCity()   const override { return _appPrefs.config.owmCity; }

    void activateAccessPoint() override;
    void formatTime(char* txt, unsigned txt_size,
                    const char* format, time_t now) override;

private:
    TemporalAnomalyClockApp();

    static constexpr const char* APP_HOST_NAME = "temporal-anomaly";

    DispDriverGc9a01RoundClock        _display;
    std::unique_ptr<DisplayManager>   _displayManager;
    TemporalAnomalyTimeSource         _timeSource;

    TemporalAnomalyPreferences         _appPrefs;
    TemporalAnomalyAccessPointManager  _apManagerConcrete;
    WeatherManager                     _weatherManager;

    // Tracks the FSM's RUNNING_NORMAL transition so showClockFace() is
    // only called on actual state changes, not every loop() tick.
    bool _wasRunning = false;
};
