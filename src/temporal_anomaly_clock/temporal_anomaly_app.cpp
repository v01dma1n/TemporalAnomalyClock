#include "temporal_anomaly_app.h"
#include "display_manager.h"
#include "debug.h"
#include "version.h"             
#include <Wire.h> 
#include "anim_matrix.h" 
#include "temporal_anomaly_animation.h" 
#include <algorithm> 

// --- Data getters for the scene playlist ---
float TemporalAnomalyClockApp_getTimeData() { return 0; }
float TemporalAnomalyClockApp_getTempData() { return TemporalAnomalyClockApp::getInstance().getTempData(); }
float TemporalAnomalyClockApp_getHumidityData() { return TemporalAnomalyClockApp::getInstance().getHumidityData(); }

// Scene playlist: only Matrix is necessary for the per-minute transition.
static const DisplayScene scenePlaylist[] = {
    { "Matrix", "TEMPORAL ANOMALY", MATRIX, false, false, 4000, 100, 50, TemporalAnomalyClockApp_getTimeData } 
};
static const int numScenes = sizeof(scenePlaylist) / sizeof(DisplayScene);

// --- Constructor ---
TemporalAnomalyClockApp::TemporalAnomalyClockApp() :
    _display(DISP_LEN, VSPI_SCLK, VSPI_MISO, VSPI_MOSI, VSPI_SS, VSPI_BLANK),
    _appPrefs(),
    _apManager(_appPrefs)
{
    _displayManager = std::make_unique<DisplayManager>(_display);
    _prefs = &_appPrefs;
    BaseNtpClockApp::_apManager = &_apManager;
    _rtcActive = false;
    
    // Allocate the continuous animation on the heap
    TemporalAnomalyAnimation* newAnim = new TemporalAnomalyAnimation(" %H.%M.%S", false, true);
    
    // --- FIX: Manually trigger setup/initialization here ---
    // Since the FSM/SceneManager clears the animation, we must initialize the state
    // before the object is moved.
    newAnim->setup(&getDisplay());
    
    // Store the raw pointer for direct access later
    _continuousClockAnimation = newAnim; 
    
    // Give ownership of the allocated object to the DisplayManager's unique_ptr
    getClock().setAnimation(std::unique_ptr<TemporalAnomalyAnimation>(newAnim));
}
 

void TemporalAnomalyClockApp::setupHardware() {
    i2c_bus_clear();
    Wire.begin();
    _displayManager->begin();
    _rtcActive = _rtc.begin();
    if (_rtcActive && (!_rtc.isrunning() || _rtc.now() < DateTime(F(__DATE__), F(__TIME__)))) {
        _rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }
}

// --- Main Setup and Loop ---
void TemporalAnomalyClockApp::setup() {

    BaseNtpClockApp::setup();
    g_appLogLevel = _appPrefs.config.logLevel;
    
    _weatherManager = std::make_unique<TemporalAnomalyWeatherDataManager>(*this);

    if (_sceneManager) {
        _sceneManager->setup(scenePlaylist, numScenes);
    }
    
    // The continuous animation is already set in the constructor.
    
    LOGINF("--- TEMPORAL ANOMALY APP SETUP COMPLETE ---");
}

void TemporalAnomalyClockApp::loop() {
    BaseNtpClockApp::loop();
    if (_weatherManager) _weatherManager->update();
    
    // Ensure the clock animation is running once FSM is in the RUNNING_NORMAL state
    if (isOkToRunScenes() && !getClock().isAnimationRunning()) {
        resetClockAnimation();
    }
}

void TemporalAnomalyClockApp::resetClockAnimation() {
    // This is called when a transient animation (like Matrix) is done or FSM enters RUNNING_NORMAL.
    if (!getClock().isAnimationRunning()) {
        
        // Create a new continuous clock animation object
        TemporalAnomalyAnimation* newAnim = new TemporalAnomalyAnimation(" %H.%M.%S", false, true);
        
        // --- FIX: Manually trigger setup/initialization here as well ---
        newAnim->setup(&getDisplay());

        // Store the pointer for later access (required due to -fno-rtti)
        _continuousClockAnimation = newAnim; 
        
        // Set the new animation, transferring ownership to the DisplayManager
        getClock().setAnimation(std::unique_ptr<TemporalAnomalyAnimation>(newAnim));
    }
}


// --- Public Helper for Per-Minute Animation ---
void TemporalAnomalyClockApp::triggerMatrixAnimation() {
    
    // Check if the current animation is running AND if the pointer is set
    if (getClock().isAnimationRunning() && _continuousClockAnimation) {
        
        IAnimation* currentAnim = getClock().getCurrentAnimation();
        
        char textBuffer[16];
        
        // Use static_cast and pointer comparison to safely access the time data
        if (currentAnim == _continuousClockAnimation) {
            
            TemporalAnomalyAnimation* anomalyAnim = static_cast<TemporalAnomalyAnimation*>(currentAnim);
            
            anomalyAnim->formatTime(textBuffer, sizeof(textBuffer));

            const DisplayScene& matrixScene = scenePlaylist[0]; 
            
            // Create and set the Matrix animation (this will interrupt the clock animation)
            auto anim = std::make_unique<MatrixAnimation>(
                textBuffer, 
                matrixScene.anim_param_1, 
                matrixScene.anim_param_2, 
                matrixScene.dots_with_previous
            );
            getClock().setAnimation(std::move(anim));
        }
    }
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