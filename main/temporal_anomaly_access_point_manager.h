// temporal_anomaly_access_point_manager.h — adds app-specific rows to the portal.
//
// Adds: startup-animation toggle, OpenWeatherMap API key + city.

#pragma once

#include "ESP32NTPClock.h"
#include "temporal_anomaly_preferences.h"

class TemporalAnomalyAccessPointManager : public BaseAccessPointManager {
public:
    explicit TemporalAnomalyAccessPointManager(TemporalAnomalyPreferences& prefs)
        : BaseAccessPointManager(prefs) {}

protected:
    void initializeFormFields() override;
};
