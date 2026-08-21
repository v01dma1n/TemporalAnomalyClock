#include "temporal_anomaly_app.h"
#include "clock_dial_bg.h"
#include "photo_face.h"
#include "version.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

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
    // clock_dial_bg.h for where this comes from and what it replaces.
    _display.setDialBackground(&clock_dial_bg);
    // Same before-begin() timing requirement as setBrandText() above; see
    // photo_face.h for where the pixel data comes from. Round-robins
    // between these on each visit to photo mode — see setPhotos().
    static const lv_image_dsc_t* const kPhotos[] = {
        &photo_face_scrivener, &photo_face_warden, &photo_face_archivist, &photo_face_test_signal
    };
    _display.setPhotos(kPhotos, 4);

    _displayManager->begin();

    // Startup splash (FSM_STARTUP_ANIM, see startupAnimMs() override) --
    // was never actually queued despite showStartupAnimation existing as
    // a portal toggle, so nothing rendered during that window. A static
    // centered 3-line banner rather than ScrollingTextAnimation --
    // scrolling is built for single-line segment-style text sliding
    // through a fixed-width window, and reading a "\n"-joined string
    // through that same window looked broken (wrapped oddly, not
    // centered) rather than like three settled lines. showStartupAnimation
    // now just gates whether the splash shows at all. yearBuf was already
    // computed above for setBrandText() and is still in scope.
    if (_appPrefs.config.showStartupAnimation) {
        char splash[48];
        snprintf(splash, sizeof(splash), "%s\n%s\n%s", APP_NAME, APP_AUTHOR, yearBuf);
        _displayManager->setAnimation(std::make_unique<StaticTextAnimation>(splash));
    }

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
    snprintf(waiting, sizeof(waiting), "JOIN WIFI:\n%s", APP_HOST_NAME);
    _displayManager->setAnimation(std::make_unique<StaticTextAnimation>(waiting));

    // BaseAccessPointManager::runBlockingLoop() deliberately has no
    // display dependency (see ESP32WiFi2's CLAUDE.md) -- it's the
    // caller's job to push banner updates while it's captive, so we run
    // our own loop here instead of calling it, switching the banner once
    // a client actually associates to tell them where to point a
    // browser. The DNS captive-portal hijack pops this automatically on
    // most phones, but not reliably enough to skip an explicit
    // instruction -- 192.168.4.1 is the fixed default soft-AP address
    // (see base_access_point_manager.cpp's startSoftAp()).
    bool shownConnected = false;
    while (true) {
        if (!shownConnected && _apManagerConcrete.isClientConnected()) {
            shownConnected = true;
            _displayManager->setAnimation(std::make_unique<StaticTextAnimation>(
                "OPEN IN BROWSER:\nhttp://192.168.4.1"));
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
