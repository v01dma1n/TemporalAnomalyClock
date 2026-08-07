#include "temporal_anomaly_access_point_manager.h"

void TemporalAnomalyAccessPointManager::initializeFormFields() {
    BaseAccessPointManager::initializeFormFields();
    auto& cfg = static_cast<TemporalAnomalyPreferences&>(_prefs).config;

    _formFields.push_back(FormField{
        "show_startup", "Show Startup Animation", false, VALIDATION_NONE,
        PREF_BOOL, { .bool_pref = &cfg.showStartupAnimation },
        nullptr, 0,
    });

    _formFields.push_back(FormField{
        "owm_key", "OpenWeatherMap API Key", true, VALIDATION_NONE,
        PREF_STRING, { .str_pref = cfg.owmApiKey }, nullptr, 0,
    });

    _formFields.push_back(FormField{
        "owm_city", "OWM City (e.g. Warsaw,PL)", false, VALIDATION_NONE,
        PREF_STRING, { .str_pref = cfg.owmCity }, nullptr, 0,
    });

    _formFields.push_back(FormField{
        "anom_period", "Anomaly wobble period, sec (should divide 60 evenly, e.g. 5/6/10/12/15/20/30/60)",
        false, VALIDATION_INTEGER, PREF_INT,
        { .int_pref = &cfg.anomalyPeriodSec }, nullptr, 0,
    });

    _formFields.push_back(FormField{
        "anom_amplit", "Anomaly wobble amplitude x100 (100=steady, 140=can run briefly backwards)",
        false, VALIDATION_INTEGER, PREF_INT,
        { .int_pref = &cfg.anomalyAmplitudeTimes100 }, nullptr, 0,
    });

    _formFields.push_back(FormField{
        "anom_level",
        "Anomaly chaos level 0-11 (0=off, 1-3 subtle, 4-7 noticeable, 8-10 extreme, 11 fully random)",
        false, VALIDATION_INTEGER, PREF_INT,
        { .int_pref = &cfg.anomalyLevel }, nullptr, 0,
    });
}
