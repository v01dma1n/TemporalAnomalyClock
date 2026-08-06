// temporal_anomaly_preferences.h — application-specific preferences.
//
// Ported from the Arduino original's TemporalAnomalyConfig. Dropped fields
// that no longer apply in this generation:
//   * owm_state_code / owm_country_code — IWeatherClock's current interface
//     is just getOwmCity(), a single composed string (e.g. "Warsaw,PL");
//     see i_weather_clock.h.
//   * tempUnit — the shared WeatherManager/WeatherData (weather_client.h)
//     only reports Fahrenheit now, there's no unit selection to persist.
//   * anomalyLevel — was stored but never actually read anywhere in the
//     Arduino app's loop()/update() logic (hardcoded to 5 at load time,
//     the real read was commented out). Dropped as dead config rather than
//     ported as a no-op.
//
// anomalyPeriodSec/anomalyAmplitudeTimes100 are new (2026-08-06): they
// drive the watch face's actual time-wobble effect (see
// DispDriverGc9a01RoundClock::setAnomalyParams() and
// disp_driver_gc9a01_round_clock.cpp's anomalyDisplayTime()) — the first
// real implementation of the "anomaly" the clock is named for.

#pragma once

#include "ESP32NTPClock.h"

struct TemporalAnomalyConfig : public BaseConfig {
    bool showStartupAnimation;
    char owmApiKey[MAX_PREF_STRING_LEN];
    char owmCity[MAX_PREF_STRING_LEN];   // e.g. "Warsaw,PL"

    // Watch-face time wobble. periodSec must evenly divide 60 for the
    // display to stay exactly correct on average over any 60s window —
    // not enforced by the portal form, just document it in the label.
    int32_t anomalyPeriodSec;
    int32_t anomalyAmplitudeTimes100; // amplitude * 100; >100 allows the hands to run backwards
};

class TemporalAnomalyPreferences : public BasePreferences {
public:
    TemporalAnomalyPreferences();

    void getPreferences() override;
    void putPreferences() override;
    void dumpPreferences() override;

    TemporalAnomalyConfig config;
};
