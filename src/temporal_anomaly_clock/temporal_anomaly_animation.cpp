#include "temporal_anomaly_animation.h"
#include "temporal_anomaly_app.h"
#include "debug.h"
#include <math.h>
#include <algorithm>
#include <stdlib.h>
#include <Preferences.h>

// --- Helper Functions Moved Inside Class ---

// Linear interpolation (now a static member)
float TemporalAnomalyAnimation::lerp(float a, float b, float t) {
    return a + t * (b - a);
}

// Simple ease-in / ease-out curve (now a static member)
float TemporalAnomalyAnimation::easeInOut(float t) {
    return t < 0.5 ? 2.0f * t * t : -1.0f + (4.0f - 2.0f * t) * t;
}

// --- Class Implementation ---

TemporalAnomalyAnimation::TemporalAnomalyAnimation(const char* timeFormat, bool dotsWithPreviousChar, bool isLiveUpdate)
    : // Initializer list order MUST match header declaration order
      _timeFormat(timeFormat),
      _dotsWithPreviousChar(dotsWithPreviousChar),
      _isLiveUpdate(isLiveUpdate),
      _rng(esp_random()), // Seed RNG
      _currentAnomalyDeviation(0.0f) // Initialized AFTER _rng
{
    time_t now = time(nullptr);
    localtime_r(&now, &_anomalousTime);
    _anomalousEpoch = now;
}

void TemporalAnomalyAnimation::setup(IDisplayDriver* display) {
    IAnimation::setup(display);
    LOGDBG("AnomalyAnim::setup() - Initializing...");
    triggerNewAnomalyTarget();
    _lastUpdate_ms = millis();
}

bool TemporalAnomalyAnimation::isDone() {
    return false;
}

int TemporalAnomalyAnimation::getNonLinearitySetting() {
    int level = TemporalAnomalyClockApp::getInstance().getPrefs().config.anomalyLevel;
    // LOGMSG(APP_LOG_DEBUG, "AnomalyAnim::getNonLinearitySetting() - Read level from config: %d", level);
    return std::min(level, MAX_RANDOM_LEVEL);
}


void TemporalAnomalyAnimation::triggerNewAnomalyTarget() {
    int anomalyLevel = getNonLinearitySetting();
    LOGMSG(APP_LOG_DEBUG,"[Trigger] Start. Level: %d", anomalyLevel);

    _transitionStartDeviation = _currentAnomalyDeviation;

    // --- (Reset logic for Level 0/11 remains the same) ---
    if (anomalyLevel == 0 || anomalyLevel == MAX_RANDOM_LEVEL) {
        // ...
        return;
    }

    // --- (Calculation of _transitionStartTime_ms, anomalyRatio, maxTargetMag remains the same) ---
    _transitionStartTime_ms = millis();
    float anomalyRatio = (float)anomalyLevel / MAX_ANOMALY_LEVEL;
    int maxTargetMag = std::max(1, (int)(MAX_ANOMALY_SECONDS * anomalyRatio));
    LOGMSG(APP_LOG_DEBUG,"[Trigger] Ratio: %.2f, MaxMag: %d", anomalyRatio, maxTargetMag);

    // --- (Random target calculation remains the same) ---
    float sign = (random(0, 2) == 0) ? 1.0f : -1.0f;
    uint32_t magnitude = _rng.nextRange(0, (uint32_t)maxTargetMag + 1);
    LOGMSG(APP_LOG_DEBUG,"[Trigger] Sign: %.1f, Magnitude: %lu", sign, magnitude);
    _targetAnomalyDeviation = sign * (float)magnitude;
    LOGMSG(APP_LOG_DEBUG,"[Trigger] Initial _targetAnomalyDeviation: %.1f", _targetAnomalyDeviation);

    // --- (Near-zero check remains the same) ---
    if (fabsf(_targetAnomalyDeviation) < TARGET_DEVIATION_ZERO_TOLERANCE && anomalyLevel > 0) {
        // ...
    }

    // --- (Transition duration calculation remains the same) ---
    int minT = MIN_TRANSITION_SEC;
    int maxT = MAX_TRANSITION_SEC;
    if (anomalyLevel > HIGH_ANOMALY_THRESHOLD) {
        minT = HIGH_ANOMALY_MIN_TRANS_SEC; 
        maxT = HIGH_ANOMALY_MAX_TRANS_SEC; 
    }
    _transitionDuration_ms = random(minT, maxT + 1) * 1000;

    // Random between 1-5 seconds.
    // We can ignore the level adjustment for now, or add a smaller one.
    // Let's just do a simple 1-5 second random hold.
    int baseHoldSec = random(MIN_HOLD_SEC, MAX_HOLD_SEC); // random(min, max) -> min to max-1, so use 6 for 1-5
    _holdDuration_ms = baseHoldSec * 1000;

    if (anomalyLevel > 0 && anomalyLevel != MAX_RANDOM_LEVEL && _transitionDuration_ms < MIN_TRANSITION_MS) {
        _transitionDuration_ms = MIN_TRANSITION_MS; // Force at least 1 second transition
        LOGDBG("[Trigger] Forcing minimum transition duration to 1s");
    }

    // Log the FINAL target value
    LOGMSG(APP_LOG_DEBUG,"ANOMALY: NEW TARGET | Target: %.1fs (Level %d, Trans %ds, Hold %ds)",
            _targetAnomalyDeviation, anomalyLevel,
            (int)(_transitionDuration_ms / 1000), (int)(_holdDuration_ms / 1000));
}

void TemporalAnomalyAnimation::updateAnomalousTime() {
    unsigned long now_ms = millis();
    time_t realEpoch = time(nullptr);
    int anomalyLevel = getNonLinearitySetting();
    bool isTimeValid = (realEpoch >= MIN_VALID_EPOCH); // Check validity once

    static bool wasTimeValidLast = false; // Track validity across calls
    static time_t last_real_log_epoch = 0;


    // --- Handle Invalid Time ---
    if (!isTimeValid) {
        // Log only when validity changes
        if (wasTimeValidLast) {
             LOGDBG("AnomalyAnim::updateAnomalousTime() - NTP time lost/invalid! RealEpoch: %lu", (long unsigned int)realEpoch);
        } else {
            // Log less frequently if waiting initially
            if (realEpoch != last_real_log_epoch) {
                LOGDBG("AnomalyAnim::updateAnomalousTime() - Waiting for valid NTP time. RealEpoch: %lu", (long unsigned int)realEpoch);
            }
        }

        // Set anomalous time to whatever (invalid) real time is
        _anomalousEpoch = realEpoch;
        localtime_r(&_anomalousEpoch, &_anomalousTime);

        // Reset deviations ONLY when time first becomes invalid
        if (wasTimeValidLast) {
             LOGDBG("AnomalyAnim::updateAnomalousTime() - Resetting deviation due to time loss.");
             _currentAnomalyDeviation = 0.0f;
             _targetAnomalyDeviation = 0.0f;
             _transitionStartDeviation = 0.0f;
        }
        // DO NOT return - allow rest of logic to run, using invalid realEpoch

    // --- Time IS Valid ---
    } else {
        // Log only when validity changes
        if (!wasTimeValidLast) {
            LOGDBG("AnomalyAnim::updateAnomalousTime() - NTP time is now valid. RealEpoch: %lu", (long unsigned int)realEpoch);
            // Optionally trigger a new target immediately when time becomes valid?
            // triggerNewAnomalyTarget(); // Consider if needed
        }

        // --- Anomaly Logic (Level 11, 0, 1-10) ---
        if (anomalyLevel == MAX_RANDOM_LEVEL) {
            _anomalousTime.tm_hour = random(0, 24);
            _anomalousTime.tm_min  = random(0, 60);
            _anomalousTime.tm_sec  = random(0, 60);
            _currentAnomalyDeviation = NAN;
            _targetAnomalyDeviation = NAN;

        } else if (anomalyLevel == 0) {
            if (fabsf(_currentAnomalyDeviation) > CURRENT_DEVIATION_ZERO_TOLERANCE) {
                 LOGDBG("AnomalyAnim::updateAnomalousTime() - Anomaly level 0 detected, resetting deviation.");
            }
            _currentAnomalyDeviation = 0.0f;
            _targetAnomalyDeviation = 0.0f;
            _transitionStartDeviation = 0.0f;

        } else { // Levels 1-10
            unsigned long timeElapsed_ms = now_ms - _transitionStartTime_ms;

            // In Transition
            if (_transitionDuration_ms > 0 && timeElapsed_ms < _transitionDuration_ms) {
                float progress = (float)timeElapsed_ms / _transitionDuration_ms;
                float easedProgress = TemporalAnomalyAnimation::easeInOut(std::min(progress, 1.0f));
                _currentAnomalyDeviation = TemporalAnomalyAnimation::lerp(_transitionStartDeviation, _targetAnomalyDeviation, easedProgress);

            // Holding Target
            } else if (timeElapsed_ms < (_transitionDuration_ms + _holdDuration_ms)) {
                 // Snap or smoothly approach target (using previous clamping logic if needed)
                 _currentAnomalyDeviation = _targetAnomalyDeviation; // Simple snap for now

            // Hold Finished: Trigger New Target
            } else {
                 triggerNewAnomalyTarget(); // This sets _transitionStartDeviation
            }
        } // End anomaly level logic
    } // End time valid check

    // --- Calculate final epoch (only if not Level 11 and deviation is valid) ---
    // Use the potentially invalid realEpoch if necessary
    if (anomalyLevel != MAX_RANDOM_LEVEL && !isnan(_currentAnomalyDeviation)) {
         float clampedDeviation = std::max(-((float)realEpoch - EPOCH_CLAMP_BUFFER), 
                                           -std::min((float)(0xFFFFFFFF - realEpoch - EPOCH_CLAMP_BUFFER), 
                                           _currentAnomalyDeviation));
         _anomalousEpoch = realEpoch + (time_t)roundf(clampedDeviation);
         localtime_r(&_anomalousEpoch, &_anomalousTime); // Use calculated epoch
    } else if (anomalyLevel != MAX_RANDOM_LEVEL) {
        // If deviation is NaN but not level 11 (error state?), use real time
         _anomalousEpoch = realEpoch;
         localtime_r(&_anomalousEpoch, &_anomalousTime);
    }
    // For level 11, _anomalousTime struct was set directly
    // If time was invalid, _anomalousTime was set earlier

    // --- Logging ---
    if (realEpoch != last_real_log_epoch) { // Log once per second
        last_real_log_epoch = realEpoch;
        if (!isTimeValid) {
            LOGDBG("ANOMALY: INVALID TIME | Displaying H:%02d M:%02d S:%02d | Level: %d",
                    _anomalousTime.tm_hour, _anomalousTime.tm_min, _anomalousTime.tm_sec, anomalyLevel);
        } else if (anomalyLevel == MAX_RANDOM_LEVEL) {
             LOGDBG("ANOMALY: LIVE (RANDOM) | Real: %lu | Anomaly H:%02d M:%02d S:%02d | Level: %d",
               (long unsigned int)realEpoch,
               _anomalousTime.tm_hour, _anomalousTime.tm_min, _anomalousTime.tm_sec,
               anomalyLevel);
        } else {
            LOGDBG("ANOMALY: LIVE | Real: %lu | Anomaly: %lu | Deviation: %.1fs | Target: %.1fs | StartDev: %.1fs | Level: %d",
               (long unsigned int)realEpoch, (long unsigned int)_anomalousEpoch,
               _currentAnomalyDeviation, _targetAnomalyDeviation, _transitionStartDeviation, anomalyLevel);
        }
    }

    // Update validity tracker for next call
    wasTimeValidLast = isTimeValid;
}

void TemporalAnomalyAnimation::update() {
    unsigned long now_ms = millis();
    if (now_ms - _lastUpdate_ms < (1000 / ANIMATION_FPS)) {
        return;
    }
    _lastUpdate_ms = now_ms;

    updateAnomalousTime();

    char txt[DISPLAY_BUFFER_SIZE];
    formatTime(txt, sizeof(txt));

    LOGDBG("AnomalyAnim::update() - Buffer Write: '%s'", txt);

    _display->print(txt, _dotsWithPreviousChar);
}

void TemporalAnomalyAnimation::formatTime(char *txt, unsigned txt_size) const {
    strftime(txt, txt_size, _timeFormat, &_anomalousTime);
}

