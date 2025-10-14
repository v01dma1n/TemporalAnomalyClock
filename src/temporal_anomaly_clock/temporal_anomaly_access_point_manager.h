#ifndef TEMPORAL_ANOMALY_ACCESS_POINT_MANAGER_H
#define TEMPORAL_ANOMALY_ACCESS_POINT_MANAGER_H

#include <base_access_point_manager.h>
#include "temporal_anomaly_preferences.h"

class TemporalAnomalyAccessPointManager : public BaseAccessPointManager {
public:
    TemporalAnomalyAccessPointManager(TemporalAnomalyPreferences& prefs) : BaseAccessPointManager(prefs) {}

protected:
    void initializeFormFields() override;
};

#endif // TEMPORAL_ANOMALY_ACCESS_POINT_MANAGER_H
