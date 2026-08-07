#include "temporal_anomaly_preferences.h"

#include <cstring>

static constexpr const char* KEY_SHOW_STARTUP     = "show_startup";
static constexpr const char* KEY_OWM_KEY          = "owm_key";
static constexpr const char* KEY_OWM_CITY         = "owm_city";
static constexpr const char* KEY_ANOMALY_PERIOD   = "anom_period";
static constexpr const char* KEY_ANOMALY_AMPLIT   = "anom_amplit";
static constexpr const char* KEY_ANOMALY_LEVEL    = "anom_level";

TemporalAnomalyPreferences::TemporalAnomalyPreferences()
    : BasePreferences(config) {
    std::memset(&config, 0, sizeof(config));
    config.showStartupAnimation = true;
    config.anomalyPeriodSec = 10;
    config.anomalyAmplitudeTimes100 = 140;
    config.anomalyLevel = 0;
}

void TemporalAnomalyPreferences::getPreferences() {
    BasePreferences::getPreferences();

    if (!openNvs(false)) return;

    config.showStartupAnimation = readBool(KEY_SHOW_STARTUP, true);
    readString(KEY_OWM_KEY,  config.owmApiKey, sizeof(config.owmApiKey));
    readString(KEY_OWM_CITY, config.owmCity,   sizeof(config.owmCity));
    config.anomalyPeriodSec = readInt(KEY_ANOMALY_PERIOD, 10);
    config.anomalyAmplitudeTimes100 = readInt(KEY_ANOMALY_AMPLIT, 140);
    config.anomalyLevel = readInt(KEY_ANOMALY_LEVEL, 0);

    closeNvs();
}

void TemporalAnomalyPreferences::putPreferences() {
    BasePreferences::putPreferences();

    if (!openNvs(true)) return;

    writeBool  (KEY_SHOW_STARTUP, config.showStartupAnimation);
    writeString(KEY_OWM_KEY,      config.owmApiKey);
    writeString(KEY_OWM_CITY,     config.owmCity);
    writeInt   (KEY_ANOMALY_PERIOD, config.anomalyPeriodSec);
    writeInt   (KEY_ANOMALY_AMPLIT, config.anomalyAmplitudeTimes100);
    writeInt   (KEY_ANOMALY_LEVEL,  config.anomalyLevel);

    closeNvs();
}

void TemporalAnomalyPreferences::dumpPreferences() {
    BasePreferences::dumpPreferences();
    LOGDBG("Pref=%s: %s", KEY_SHOW_STARTUP,
           config.showStartupAnimation ? "yes" : "no");
    LOGDBG("Pref=%s: %s", KEY_OWM_KEY,  config.owmApiKey[0] ? "(set)" : "(empty)");
    LOGDBG("Pref=%s: %s", KEY_OWM_CITY, config.owmCity);
    LOGDBG("Pref=%s: %d sec", KEY_ANOMALY_PERIOD, (int)config.anomalyPeriodSec);
    LOGDBG("Pref=%s: %d (%.2fx)", KEY_ANOMALY_AMPLIT,
           (int)config.anomalyAmplitudeTimes100,
           config.anomalyAmplitudeTimes100 / 100.0f);
    LOGDBG("Pref=%s: %d", KEY_ANOMALY_LEVEL, (int)config.anomalyLevel);
}
