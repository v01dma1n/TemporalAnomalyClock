#include "temporal_anomaly_weather_manager.h"
#include "i_weather_clock.h"
#include "debug.h"

TemporalAnomalyWeatherDataManager::TemporalAnomalyWeatherDataManager(IWeatherClock& app) : 
    _app(app), _lastWeatherFetchTime(0) {}

void TemporalAnomalyWeatherDataManager::update() {
    if (!_app.isOkToRunScenes()) {
        return;
    }

    bool timeToFetch = (millis() - _lastWeatherFetchTime >= _weatherFetchInterval);
    if (_lastWeatherFetchTime == 0) {
        timeToFetch = true;
    }

    if (timeToFetch) {
        _lastWeatherFetchTime = millis();
        const char* apiKey = _app.getOwmApiKey();
        if (apiKey == nullptr || strlen(apiKey) == 0) {
            LOGINF("OpenWeatherMap API key is not configured. Skipping fetch.");
            return;
        }

        LOGINF("Fetching new weather data...");
        OWMConfig owm_config;
        owm_config.city = _app.getOwmCity();
        owm_config.state_code = _app.getOwmStateCode();
        owm_config.country_code = _app.getOwmCountryCode();
        owm_config.api_key = apiKey;
        owm_config.temp_unit = _app.getTempUnit();
        
        OpenWeatherData data = getOpenWeatherData(owm_config);
        _app.setWeatherData(data);
    }
}
