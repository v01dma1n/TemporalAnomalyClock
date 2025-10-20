#include "temporal_anomaly_app.h"
#include "display_manager.h"
#include "debug.h"
#include "version.h"             
#include <Wire.h> 
#include "anim_matrix.h" 
#include "temporal_anomaly_animation.h" // Still required for logic definition
#include <algorithm> 
#include <memory> 

// Global object to manage the permanent anomaly state
static TemporalAnomalyAnimation s_anomaly_clock(" %H.%M.%S", false, true);

// --- Data getters for the scene playlist ---
float TemporalAnomalyClockApp_getTimeData() { return 0; }
float TemporalAnomalyClockApp_getTempData() { return TemporalAnomalyClockApp::getInstance().getTempData(); }
float TemporalAnomalyClockApp_getHumidityData() { return TemporalAnomalyClockApp::getInstance().getHumidityData(); }

// Scene playlist is now only used for the Matrix parameters.
static const DisplayScene scenePlaylist[] = {
    { "Matrix", "TEMPORAL ANOMALY", MATRIX, false, false, 4000, 100, 50, TemporalAnomalyClockApp_getTimeData } 
};
static const int numScenes = sizeof(scenePlaylist) / sizeof(DisplayScene);

// --- Constructor ---
TemporalAnomalyClockApp::TemporalAnomalyClockApp() :
    // --- FIX 1: Use the 1-argument (buffer-only) constructor ---
    _display(DISP_LEN),
    _appPrefs(),
    _apManager(_appPrefs)
{
    _displayManager = std::make_unique<DisplayManager>(_display);
    _prefs = &_appPrefs;
    BaseNtpClockApp::_apManager = &_apManager;
    _rtcActive = false;
}
 
TemporalAnomalyClockApp::~TemporalAnomalyClockApp() = default;

void TemporalAnomalyClockApp::setupHardware() {
    i2c_bus_clear();
    Wire.begin();
    _displayManager->begin();
    _rtcActive = _rtc.begin();
    if (_rtcActive && (!_rtc.isrunning() || _rtc.now() < DateTime(F(__DATE__), F(__TIME__)))) {
        _rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }
}

// --- Main Setup ---
void TemporalAnomalyClockApp::setup() {
    _appPrefs.setup();
    g_appLogLevel = _appPrefs.config.logLevel;

    BaseNtpClockApp::setup();
    
    _weatherManager = std::make_unique<TemporalAnomalyWeatherDataManager>(*this);
    if (_sceneManager) {
        _sceneManager->setup(scenePlaylist, numScenes);
    }
    
    s_anomaly_clock.setup(&getDisplay());
    LOGINF("--- TEMPORAL ANOMALY APP SETUP COMPLETE ---");
}

// --- Main Loop (Corrected) ---
void TemporalAnomalyClockApp::loop() {
    // --- FIX 2: Call the base class loop ---
    // This runs _fsmManager->update() AND _displayManager->update().
    // This is CRITICAL for the startup animation to run and finish.
    BaseNtpClockApp::loop(); 

    // 2. Run app-specific managers
    if (_weatherManager) _weatherManager->update();
    
    // 3. Run the special anomaly logic *only when* in the normal state
    if (isOkToRunScenes()) {
        // If no transient animation is running (like the Matrix),
        // update the continuous anomaly time.
        if (!getClock().isAnimationRunning()) {
            s_anomaly_clock.update();
        }
    }
}

// --- Public Helper for Per-Minute Animation ---
void TemporalAnomalyClockApp::triggerMatrixAnimation() {
    if (!getClock().isAnimationRunning()) {
        char textBuffer[16];
        s_anomaly_clock.formatTime(textBuffer, sizeof(textBuffer));
        const DisplayScene& matrixScene = scenePlaylist[0];
        
        auto anim = std::make_unique<MatrixAnimation>(
            textBuffer, 
            matrixScene.anim_param_1, 
            matrixScene.anim_param_2, 
            matrixScene.dots_with_previous
        );
        getClock().setAnimation(std::move(anim));
    }
}

DisplayManager& TemporalAnomalyClockApp::getClock() {
    return *_displayManager;
}

// --- (All other IBaseClock/IWeatherClock interface methods) ---
// --- (They remain unchanged from the original file) ---

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

void TemporalAnomalyClockApp::activateAccessPoint() {
    _apManager.setup(getAppName());
    String waitingMsgStr = "SETUP MODE -- WIFI ";
    waitingMsgStr += getAppName();
    _apManager.runBlockingLoop(*_displayManager, waitingMsgStr.c_str(), "CONNECTED - SETUP AT 192.168.4.1");
}