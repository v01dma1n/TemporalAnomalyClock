#include "temporal_anomaly_animation.h"
#include "temporal_anomaly_app.h" 
#include "debug.h"
#include <math.h>         
#include <algorithm>      
#include <stdlib.h>
#include <Preferences.h>

// Simple linear interpolation helper
float TemporalAnomalyAnimation::lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

// Easing function: Smooth start and smooth stop (Cubic ease-in-out)
float TemporalAnomalyAnimation::easeInOut(float t) {
    return t * t * (3.0f - 2.0f * t);
}

TemporalAnomalyAnimation::TemporalAnomalyAnimation(const char* timeFormat, bool dotsWithPreviousChar, bool isLiveUpdate)
    : _timeFormat(timeFormat), 
      _dotsWithPreviousChar(dotsWithPreviousChar),
      _isLiveUpdate(isLiveUpdate),
      _rng(esp_random()) 
{
    time_t now = time(nullptr);
    localtime_r(&now, &_anomalousTime);
    _anomalousEpoch = now;
}

void TemporalAnomalyAnimation::setup(IDisplayDriver* display) {
    IAnimation::setup(display);
    
    triggerNewAnomalyTarget();
    _lastUpdate_ms = millis();
}

bool TemporalAnomalyAnimation::isDone() {
    return false;
}

int TemporalAnomalyAnimation::getNonLinearitySetting() {
    Preferences preferences;
    preferences.begin("config", true); 
    int level = preferences.getUInt(ANOMALY_LEVEL_KEY, 0); 
    preferences.end();
    return std::min(level, MAX_ANOMALY_LEVEL);
}

void TemporalAnomalyAnimation::triggerNewAnomalyTarget() {
    int anomalyLevel = getNonLinearitySetting();
    
    if (anomalyLevel == 0) {
        _targetAnomalyDeviation = 0.0f;
        _transitionDuration_ms = 0;
        _holdDuration_ms = 0;
        _currentAnomalyDeviation = 0.0f;
        return;
    }

    _transitionStartTime_ms = millis();
    
    float anomalyRatio = (float)anomalyLevel / MAX_ANOMALY_LEVEL;
    int maxTargetMag = (int)(MAX_ANOMALY_SECONDS * anomalyRatio);

    _targetAnomalyDeviation = (float)_rng.nextRange(-maxTargetMag, maxTargetMag);
    
    int minT = 3;
    int maxT = 5;
    if (anomalyLevel > 7) { minT = 5; maxT = 10; }
    _transitionDuration_ms = random(minT, maxT + 1) * 1000; 
    
    int baseHoldSec = random(10, 20) - (int)(anomalyLevel * 0.5f);
    _holdDuration_ms = (baseHoldSec < 5 ? 5 : baseHoldSec) * 1000;

    // --- DEBUG LOG: New Target Set ---
    LOGDBG("ANOMALY: NEW TARGET | Target: %.1fs (Level %d, Trans %ds, Hold %ds)", 
            _targetAnomalyDeviation, 
            anomalyLevel, 
            (int)(_transitionDuration_ms / 1000), 
            (int)(_holdDuration_ms / 1000));
}

void TemporalAnomalyAnimation::updateAnomalousTime() {
    unsigned long now_ms = millis();
    time_t realEpoch = time(nullptr);
    int anomalyLevel = getNonLinearitySetting();

    // Check if the real second has changed (for periodic logging)
    static time_t last_real_log_epoch = 0;
    
    if (anomalyLevel == 0) {
        _currentAnomalyDeviation = 0.0f;
    } else {
        unsigned long timeElapsed_ms = now_ms - _transitionStartTime_ms;
        float startDeviation = _currentAnomalyDeviation;

        if (timeElapsed_ms < _transitionDuration_ms) {
            float progress = (float)timeElapsed_ms / _transitionDuration_ms;
            float easedProgress = easeInOut(progress);
            
            _currentAnomalyDeviation = lerp(startDeviation, _targetAnomalyDeviation, easedProgress);
        } else if (timeElapsed_ms < (_transitionDuration_ms + _holdDuration_ms)) {
            _currentAnomalyDeviation = _targetAnomalyDeviation; 
        } else {
            float previousTarget = _targetAnomalyDeviation;
            triggerNewAnomalyTarget(); 
            
            _currentAnomalyDeviation = previousTarget; 
            
            timeElapsed_ms = now_ms - _transitionStartTime_ms;
            float progress = (float)timeElapsed_ms / _transitionDuration_ms;
            float easedProgress = easeInOut(progress);
            _currentAnomalyDeviation = lerp(previousTarget, _targetAnomalyDeviation, easedProgress);
        }
    }

    _anomalousEpoch = realEpoch + (time_t)roundf(_currentAnomalyDeviation);
    localtime_r(&_anomalousEpoch, &_anomalousTime); 
    
    // --- DEBUG LOG: Current State (Once per real second) ---
    if (realEpoch != last_real_log_epoch) {
        last_real_log_epoch = realEpoch;
        LOGDBG("ANOMALY: LIVE | Real: %lu | Anomaly: %lu | Deviation: %.1fs", 
               (long unsigned int)realEpoch, 
               (long unsigned int)_anomalousEpoch, 
               _currentAnomalyDeviation);
    }
}

void TemporalAnomalyAnimation::update() {
    unsigned long now_ms = millis();
    
    if (now_ms - _lastUpdate_ms < (1000 / ANIMATION_FPS)) {
        return;
    }
    _lastUpdate_ms = now_ms;
    
    updateAnomalousTime();

    char txt[16];
    formatTime(txt, sizeof(txt));

    _display->print(txt, _dotsWithPreviousChar); 

    if (_anomalousTime.tm_sec == 0) {
        TemporalAnomalyClockApp::getInstance().triggerMatrixAnimation();
    }
}

void TemporalAnomalyAnimation::formatTime(char *txt, unsigned txt_size) const {
    strftime(txt, txt_size, _timeFormat, &_anomalousTime);
}