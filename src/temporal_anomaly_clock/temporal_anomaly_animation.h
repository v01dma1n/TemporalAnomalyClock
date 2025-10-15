#ifndef TEMPORAL_ANOMALY_ANIMATION_H
#define TEMPORAL_ANOMALY_ANIMATION_H

#include "i_animation.h"
#include "fast_random.h" 
#include <time.h>
#include <Preferences.h>

#define ANOMALY_LEVEL_KEY "anomaly_level"
#define MAX_ANOMALY_LEVEL 11            
#define MAX_ANOMALY_SECONDS 30          
#define ANIMATION_FPS 20                

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

    tm _anomalousTime; 
    time_t _anomalousEpoch = 0;
    
    float _currentAnomalyDeviation = 0.0f; 
    float _targetAnomalyDeviation = 0.0f; 
    
    unsigned long _transitionStartTime_ms = 0; 
    unsigned long _transitionDuration_ms = 0; 
    unsigned long _holdDuration_ms = 0; 
    unsigned long _lastUpdate_ms = 0;
    
    FastRandom _rng;

    void updateAnomalousTime();
    float easeInOut(float t);
    float lerp(float a, float b, float t);
};

#endif // TEMPORAL_ANOMALY_ANIMATION_H