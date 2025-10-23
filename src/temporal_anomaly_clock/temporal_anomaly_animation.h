#ifndef TEMPORAL_ANOMALY_ANIMATION_H
#define TEMPORAL_ANOMALY_ANIMATION_H

#include "i_animation.h"
#include "fast_random.h" 
#include <time.h>
#include <Preferences.h>

#define ANIMATION_FPS 20               
#define DISPLAY_BUFFER_SIZE 16 // For char txt[16]

#define MAX_ANOMALY_LEVEL 10 // Levels 1-10 control deviation magnitude
#define MAX_RANDOM_LEVEL 11  // Level 11 triggers full random time
#define MAX_ANOMALY_SECONDS 60 // Max deviation at level 10

// Anomaly transition/hold timing
#define MIN_TRANSITION_SEC 3
#define MAX_TRANSITION_SEC 5
#define HIGH_ANOMALY_THRESHOLD 7 // Level above which transitions are slower
#define HIGH_ANOMALY_MIN_TRANS_SEC 5
#define HIGH_ANOMALY_MAX_TRANS_SEC 10
#define MIN_HOLD_SEC 1
#define MAX_HOLD_SEC 5 // Will give 1-5 second hold
#define MIN_TRANSITION_MS 1000

// Tolerances
#define TARGET_DEVIATION_ZERO_TOLERANCE 0.1f
#define CURRENT_DEVIATION_ZERO_TOLERANCE 0.01f

// A known "good" time to validate NTP sync (Jan 1, 2024)
#define MIN_VALID_EPOCH 1704067200UL
#define EPOCH_CLAMP_BUFFER 10

class TemporalAnomalyAnimation : public IAnimation {
public:
    TemporalAnomalyAnimation(const char* timeFormat, bool dotsWithPreviousChar, bool isLiveUpdate);
    ~TemporalAnomalyAnimation() override = default;

    void setup(IDisplayDriver* display) override;
    void update() override;
    bool isDone() override; // Must return false for continuous display

    void formatTime(char *txt, unsigned txt_size) const;
    void triggerNewAnomalyTarget();

    time_t getAnomalousEpoch() const { return _anomalousEpoch; }
    int getNonLinearitySetting();

private:
    const char* _timeFormat;
    bool _dotsWithPreviousChar;
    bool _isLiveUpdate;
    FastRandom _rng;
    float _currentAnomalyDeviation = 0.0f; 

    tm _anomalousTime; 
    time_t _anomalousEpoch = 0;
    
    float _targetAnomalyDeviation = 0.0f; 
    float _transitionStartDeviation = 0.0f; // Stores deviation at the start of a transition

    unsigned long _transitionStartTime_ms = 0; 
    unsigned long _transitionDuration_ms = 0; 
    unsigned long _holdDuration_ms = 0; 
    unsigned long _lastUpdate_ms = 0;
    
    void applySimpleAnomaly(time_t realEpoch);

    void updateAnomalousTime();

    float easeInOut(float t);
    float lerp(float a, float b, float t);
};

#endif // TEMPORAL_ANOMALY_ANIMATION_H