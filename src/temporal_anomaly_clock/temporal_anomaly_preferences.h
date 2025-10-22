#ifndef TEMPORAL_ANOMALY_PREFERENCES_H
#define TEMPORAL_ANOMALY_PREFERENCES_H

#include "temporal_anomaly_types.h"
#include <base_preferences.h> 

struct TemporalAnomalyConfig : public BaseConfig {
  bool showStartupAnimation;
  char owm_api_key[MAX_PREF_STRING_LEN];
  char owm_city[MAX_PREF_STRING_LEN];
  char owm_state_code[MAX_PREF_STRING_LEN];
  char owm_country_code[MAX_PREF_STRING_LEN];
  char tempUnit[MAX_PREF_STRING_LEN];

  uint8_t anomalyLevel; 
};

class TemporalAnomalyPreferences : public BasePreferences {
public:
    TemporalAnomalyPreferences() : BasePreferences(config) {}
    void getPreferences() override;
    void putPreferences() override;
    void dumpPreferences() override;
    TemporalAnomalyConfig config;
};

#endif // TEMPORAL_ANOMALY_PREFERENCES_H
