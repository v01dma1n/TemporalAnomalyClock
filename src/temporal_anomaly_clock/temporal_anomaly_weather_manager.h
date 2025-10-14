#ifndef TEMPORAL_ANOMALY_TYPES_WEATHER_MANAGER_H
#define TEMPORAL_ANOMALY_TYPES_WEATHER_MANAGER_H

class IWeatherClock;

class TemporalAnomalyWeatherDataManager {
public:
    TemporalAnomalyWeatherDataManager(IWeatherClock& app);
    void update();
private:
    IWeatherClock& _app;
    unsigned long _lastWeatherFetchTime;
    const unsigned long _weatherFetchInterval = 900000;
};

#endif // TEMPORAL_ANOMALY_TYPES_WEATHER_MANAGER_H
