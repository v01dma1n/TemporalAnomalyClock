#ifndef TEMPORAL_ANOMALY_ANIMATION_H
#define TEMPORAL_ANOMALY_ANIMATION_H

#include "i_animation.h"
#include "fast_random.h" 
#include <time.h>
#include <Preferences.h>

#define ANOMALY_LEVEL_KEY "anomaly_level"
#define ANIMATION_FPS 20               
#define MAX_ANOMALY_LEVEL 10 // Levels 1-10 control deviation magnitude
#define MAX_RANDOM_LEVEL 11  // Level 11 triggers full random time
#define MAX_ANOMALY_SECONDS 60 // Max deviation at level 10
#define MAX_ANOMALY_SPEED 5.0f // Max deviation change per second (e.g., 5.0 seconds/second)


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