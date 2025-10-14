#ifndef TEMPORAL_ANOMALY__APP_H
#define TEMPORAL_ANOMALY__APP_H

#include "display_manager.h"

#include "temporal_anomaly_types.h"
#include "temporal_anomaly_access_point_manager.h"
#include "temporal_anomaly_weather_manager.h"
#include "temporal_anomaly_preferences.h"

#include "RTClib.h"
#include <ESP32NTPClock.h>
#include <ESP32NTPClock_MAX6921.h> // Use the VFD driver
#include <base_ntp_clock_app.h>
#include <i_weather_clock.h>

#include <memory>

class DisplayManager;

#define AP_HOST_NAME "temporal-anomaly"

#define DISP_LEN    10

#define VSPI_MISO   19
#define VSPI_MOSI   23
#define VSPI_SCLK   18
#define VSPI_SS      5
#define VSPI_BLANK   0

class TemporalAnomalyClockApp : public BaseNtpClockApp, public IWeatherClock {
public:
    static TemporalAnomalyClockApp& getInstance() {
        static TemporalAnomalyClockApp instance;
        return instance;
    }
    
    ~TemporalAnomalyClockApp(); 

    void setup() override;
    void loop() override;

    void setupHardware() override;

    TemporalAnomalyPreferences& getPrefs() { return _appPrefs; }
    float getTempData();
    float getHumidityData();
    
    // --- Implementation of the IBaseClock & IWeatherClock interfaces ---
    const char* getAppName() const override;
    const char* getSsid() const override { return _appPrefs.config.ssid; }
    const char* getPassword() const override { return _appPrefs.config.password; }
    const char* getTimezone() const override { return _appPrefs.config.time_zone; }
    const char* getTempUnit() const override { return _appPrefs.config.tempUnit; }
    const char* getOwmApiKey() const override { return _appPrefs.config.owm_api_key; }
    const char* getOwmCity() const override { return _appPrefs.config.owm_city; }
    const char* getOwmStateCode() const override { return _appPrefs.config.owm_state_code; }
    const char* getOwmCountryCode() const override { return _appPrefs.config.owm_country_code; }
    void setWeatherData(const OpenWeatherData& data) override { _currentWeatherData = data; }
    bool isOkToRunScenes() const override;
    void syncRtcFromNtp() override;
    void activateAccessPoint() override;
    void formatTime(char *txt, unsigned int txt_size, const char *format, time_t now) override;
    IDisplayDriver& getDisplay() override { return _display; }
    DisplayManager& getClock() override;
    RTC_DS1307& getRtc() override { return _rtc; }
    bool isRtcActive() const override { return _rtcActive; }

private:
    TemporalAnomalyClockApp();

    // VFD-specific hardware components
    DispDriverMAX6921 _display;
    std::unique_ptr<DisplayManager> _displayManager;
    RTC_DS1307 _rtc;
    bool _rtcActive;

    // Application-specific preferences & managers
    TemporalAnomalyPreferences _appPrefs;
    TemporalAnomalyAccessPointManager _apManager;
    std::unique_ptr<TemporalAnomalyWeatherDataManager> _weatherManager; 
    OpenWeatherData _currentWeatherData;
};

#endif // TEMPORAL_ANOMALY__APP_H
