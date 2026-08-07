#include "temporal_anomaly_app.h"
#include "version.h"

#include <cstdio>
#include <ctime>

TemporalAnomalyClockApp& TemporalAnomalyClockApp::getInstance() {
    static TemporalAnomalyClockApp instance;
    return instance;
}

TemporalAnomalyClockApp::~TemporalAnomalyClockApp() = default;

TemporalAnomalyClockApp::TemporalAnomalyClockApp()
    : _appPrefs(),
      _apManagerConcrete(_appPrefs),
      _weatherManager(*this) {
    _displayManager = std::make_unique<DisplayManager>(_display);
    _prefs     = &_appPrefs;
    _apManager = &_apManagerConcrete;
}

void TemporalAnomalyClockApp::setupHardware() {
    _displayManager->begin();
    // Preferences are already loaded by this point in the BaseNtpClockApp
    // lifecycle (setup() calls _prefs->setup()/getPreferences() before
    // setupHardware()). A portal save reboots the device, so a one-time
    // read here is enough — no live-reload needed.
    _display.setAnomalyParams(_appPrefs.config.anomalyPeriodSec,
                               _appPrefs.config.anomalyAmplitudeTimes100 / 100.0);
    _display.setAnomalyLevel(_appPrefs.config.anomalyLevel);
}

void TemporalAnomalyClockApp::setup() {
    BaseNtpClockApp::setup();

    // No character-grid scene playlist — the round LCD's own watch face
    // (not the engine's SceneManager) drives the RUNNING_NORMAL display.
    // See loop(): the display driver's showClockFace() is toggled directly
    // off the FSM state instead.
    if (_sceneManager) _sceneManager->setup(nullptr, 0);

    LOGINF("%s ready", APP_NAME);
}

void TemporalAnomalyClockApp::loop() {
    BaseNtpClockApp::loop();

    // WeatherManager gates on isOkToRunScenes() and its own 15-minute
    // interval internally.
    _weatherManager.update();
    WeatherData w = _weatherManager.getWeatherData();
    _display.setWeatherData(w.tempF, w.humidity, w.valid);

    bool running = _fsmManager && _fsmManager->isInState("RUNNING_NORMAL");
    if (running != _wasRunning) {
        _display.showClockFace(running);
        _wasRunning = running;
    }
}

bool TemporalAnomalyClockApp::isOkToRunScenes() const {
    return _fsmManager && _fsmManager->isInState("RUNNING_NORMAL");
}

void TemporalAnomalyClockApp::formatTime(char* txt, unsigned txt_size,
                                          const char* format, time_t now) {
    struct tm ti;
    localtime_r(&now, &ti);
    strftime(txt, txt_size, format, &ti);
}

void TemporalAnomalyClockApp::activateAccessPoint() {
    _apManagerConcrete.setup(APP_HOST_NAME);

    char waiting[48];
    snprintf(waiting, sizeof(waiting), "JOIN WIFI: %s", APP_HOST_NAME);
    _displayManager->setAnimation(std::make_unique<StaticTextAnimation>(waiting));

    _apManagerConcrete.runBlockingLoop();
}
