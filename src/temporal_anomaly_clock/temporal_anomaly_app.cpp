#include "temporal_anomaly_app.h"
#include "display_manager.h"
#include "debug.h"
#include "version.h"             
#include <Wire.h> 

// --- Data getters for the scene playlist ---u
float TemporalAnomalyClockApp_getTimeData() { return 0; }
float TemporalAnomalyClockApp_getTempData() { return TemporalAnomalyClockApp::getInstance().getTempData(); }
float TemporalAnomalyClockApp_getHumidityData() { return TemporalAnomalyClockApp::getInstance().getHumidityData(); }

// --- The application's specific scene playlist ---
// Now only contains the Time scene, which is live updated by TemporalAnomalyClock::tick()
// Matrix animation added for the per-minute scene reset.
static const DisplayScene scenePlaylist[] = {
    // The "live update" scene is handled by the custom tick function.
    // The scene manager must only have one scene to prevent it from switching away.
    // Duration is irrelevant as the scene is manually refreshed by the anomaly tick.
    { "Time",        " %H.%M.%S",    SLOT_MACHINE,  false, true,  99999999, 200, 50, TemporalAnomalyClockApp_getTimeData },
    // Add a short matrix animation to use for the minute-change transition
    { "Matrix",      " %H.%M.%S",    MATRIX,        false, false, 4000, 350, 50, TemporalAnomalyClockApp_getTimeData } // Placeholder for Matrix transition
};
static const int numScenes = sizeof(scenePlaylist) / sizeof(DisplayScene);

// --- Constructor ---
TemporalAnomalyClockApp::TemporalAnomalyClockApp() :
    // Initialize the VFD driver with display size and SPI pins
    _display(DISP_LEN, VSPI_SCLK, VSPI_MISO, VSPI_MOSI, VSPI_SS, VSPI_BLANK),
    _appPrefs(),
    _apManager(_appPrefs)
{
    _displayManager = std::make_unique<DisplayManager>(_display);

    _prefs = &_appPrefs;
    BaseNtpClockApp::_apManager = &_apManager;
    _rtcActive = false;
}
 
// --- Destructor ---
TemporalAnomalyClockApp::~TemporalAnomalyClockApp() = default;

void TemporalAnomalyClockApp::setupHardware() {
    i2c_bus_clear();
    Wire.begin();
    _displayManager->begin();
    // This clock uses an RTC, so we initialize it.
    _rtcActive = _rtc.begin();
    if (_rtcActive && (!_rtc.isrunning() || _rtc.now() < DateTime(F(__DATE__), F(__TIME__)))) {
        _rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }
}

// --- Main Setup and Loop ---
void TemporalAnomalyClockApp::setup() {

    // Call the generic setup engine from the library
    BaseNtpClockApp::setup();
    
    // Set the global log level from the loaded preferences
    g_appLogLevel = _appPrefs.config.logLevel;

    // Initialize application-specific managers
    _weatherManager = std::make_unique<TemporalAnomalyWeatherDataManager>(*this);
    if (_sceneManager) {
        _sceneManager->setup(scenePlaylist, numScenes);
    }
    LOGINF("--- TEMPORAL ANOMALY APP SETUP COMPLETE ---");
}

void TemporalAnomalyClockApp::loop() {
    BaseNtpClockApp::loop();
    if (_weatherManager) _weatherManager->update();
}

DisplayManager& TemporalAnomalyClockApp::getClock() {
    return *_displayManager;
}

// --- IBaseClock & IWeatherClock Interface Implementations ---

void TemporalAnomalyClockApp::activateAccessPoint() {
    _apManager.setup(getAppName());
    String waitingMsgStr = "SETUP MODE -- WIFI ";
    waitingMsgStr += getAppName();
    _apManager.runBlockingLoop(*_displayManager, waitingMsgStr.c_str(), "CONNECTED - SETUP AT 192.168.4.1");
}

float TemporalAnomalyClockApp::getTempData() { 
    return _currentWeatherData.isValid ? _currentWeatherData.temperatureF : UNSET_VALUE;
}

float TemporalAnomalyClockApp::getHumidityData() { 
    return _currentWeatherData.isValid ? _currentWeatherData.humidity : UNSET_VALUE;
}

bool TemporalAnomalyClockApp::isOkToRunScenes() const {
    return _fsmManager && _fsmManager->isInState("RUNNING_NORMAL");
}

void TemporalAnomalyClockApp::formatTime(char *txt, unsigned txt_size, const char *format, time_t now) {
    struct tm timeinfo = *localtime(&now);
    strftime(txt, txt_size, format, &timeinfo);
}

void TemporalAnomalyClockApp::syncRtcFromNtp() {
    if (!_rtcActive) return;
    time_t now_utc = time(nullptr);
    _rtc.adjust(DateTime(now_utc));
    LOGINF("RTC synchronized with NTP time.");
}

const char* TemporalAnomalyClockApp::getAppName() const {
    return AP_HOST_NAME;
}
