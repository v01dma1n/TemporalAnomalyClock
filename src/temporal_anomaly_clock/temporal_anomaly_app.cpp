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
    // 1. Setup preferences and log level
    _appPrefs.setup();
    g_appLogLevel = _appPrefs.config.logLevel;

    // 2. Call the BASE setup engine.
    // This correctly initializes hardware, WiFi, Time, FSM, SceneManager,
    // and starts the unwanted StartupAnimation.
    BaseNtpClockApp::setup();

    // 3. IMMEDIATELY stop the animation started by the base class.
    getClock().setAnimation(nullptr);

    // 4. Setup application-specific managers
    _weatherManager = std::make_unique<TemporalAnomalyWeatherDataManager>(*this);

    // 5. Ensure the SceneManager is neutered (has no playlist)
    if (_sceneManager) {
        _sceneManager->setup(nullptr, 0);
    }

    // 6. Setup the one and only animation object for this clock
    s_anomaly_clock.setup(&getDisplay());
    
    LOGINF("--- TEMPORAL ANOMALY APP SETUP COMPLETE ---");
    // The FSM was already started by BaseNtpClockApp::setup()
}

// --- Main Loop (Corrected) ---
void TemporalAnomalyClockApp::loop() {
// 1. Run the FSM
    if (_fsmManager) _fsmManager->update();

    // 2. Run the weather manager
    if (_weatherManager) _weatherManager->update();
    
    // 3. Run the display logic BEFORE the display manager
    if (isOkToRunScenes()) {
        // If no transient animation (like Matrix) is running...
        if (!getClock().isAnimationRunning()) {
            // ...update the continuous anomalous time display.
            // This writes the anomalous time to the buffer.
            s_anomaly_clock.update();
        }
    }

    // 4. Run the DisplayManager LAST
    // This will:
    // a) Run the startup/Matrix animation if one is active (overwriting the buffer).
    // b) If no animation is active, it will just proceed.
    // c) In ALL cases, it will grab the FINAL display buffer
    //    (which was just updated by an anim OR s_anomaly_clock)
    //    and send it to the displayTask queue.
    if (_displayManager) _displayManager->update();
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