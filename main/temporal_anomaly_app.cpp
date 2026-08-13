#include "temporal_anomaly_app.h"
#include "photo_face.h"
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
    // setBrandText() must land before begin() -- buildUi() (called inside
    // begin()) bakes it into fixed labels created once at startup.
    char yearBuf[5];
    snprintf(yearBuf, sizeof(yearBuf), "%.4s", APP_DATE); // APP_DATE is "YYYY-MM-DD"
    _display.setBrandText(APP_AUTHOR, yearBuf);
    // Same before-begin() timing requirement as setBrandText() above; see
    // photo_face.h for where the pixel data comes from.
    _display.setPhoto(&photo_face_man);

    _displayManager->begin();
    // Preferences are already loaded by this point in the BaseNtpClockApp
    // lifecycle (setup() calls _prefs->setup()/getPreferences() before
    // setupHardware()). A portal save reboots the device, so a one-time
    // read here is enough — no live-reload needed.
    _timeSource.setWobbleParams(_appPrefs.config.anomalyPeriodSec,
                                 _appPrefs.config.anomalyAmplitudeTimes100 / 100.0);
    _timeSource.setChaosLevel(_appPrefs.config.anomalyLevel);
    _display.setTimeProvider(&_timeSource);
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

    // Rate gauge full-scale: a first-pass estimate, not derived from
    // anything precise. Default wobble amplitude (1.4) alone deflects to
    // ~28-56% of the gauge; any active chaos level (rate cap = level*4.0,
    // see TemporalAnomalyTimeSource::tickChaos()) pushes further toward
    // saturation as the level rises -- pegging at the extremes under
    // heavy chaos is an intentional, expected look, not a bug.
    static constexpr float kRateGaugeFullScale = 5.0f;
    _display.setRateGauge((float)_timeSource.lastRateDeviation(), kRateGaugeFullScale);

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
