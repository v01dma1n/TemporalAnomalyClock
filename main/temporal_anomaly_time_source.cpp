#include "temporal_anomaly_time_source.h"

#include "esp_timer.h"
#include "esp_random.h"

#include <cmath>
#include <ctime>
#include <sys/time.h>

void TemporalAnomalyTimeSource::setWobbleParams(double periodSec, double amplitude) {
    _periodSec = (periodSec > 0.0) ? periodSec : 1.0;
    _amplitude = amplitude;
}

void TemporalAnomalyTimeSource::setChaosLevel(int level) {
    if (level < 0) level = 0;
    if (level > 11) level = 11;
    _chaosLevel = level;
}

// Uniform random double in [0,1), using the hardware RNG (no seeding needed).
static double randUnit() {
    return (double)(esp_random() % 1000001u) / 1000000.0;
}

// Uniform random double in [-1,1).
static double randSigned() {
    return randUnit() * 2.0 - 1.0;
}

// Chaos anomaly mode (levels 1-11), layered *additively* on top of the
// sinusoidal wobble computed in getDisplayTime() — see setChaosLevel().
// Unlike the sinusoidal system, this does not guarantee any particular
// long-run accuracy; it's a damped random walk instead:
//   - occasional random "kicks" to a velocity (_chaosRateSec), probability
//     and magnitude both scaling with level, so low levels stutter rarely
//     and high levels kick often and hard;
//   - velocity mean-reversion pulls the rate back toward 0 over time, but
//     more weakly at higher levels, so excursions last longer approaching
//     the high end ("more extreme and unpredictable... frequent and longer
//     periods of speed change or reversal");
//   - a gentle "spring" pulls the accumulated position (_chaosOffsetSec)
//     back toward 0 too, so the clock still stays roughly near real time
//     over long periods even without an exact-accuracy guarantee.
// Level 11 is a hard special case handled separately in getDisplayTime():
// the displayed second becomes fully random each real second, ignoring
// this walk entirely.
void TemporalAnomalyTimeSource::tickChaos(double dt_sec) {
    if (_chaosLevel <= 0 || _chaosLevel >= 11) {
        _chaosOffsetSec = 0.0;
        _chaosRateSec = 0.0;
        return;
    }
    double level = (double)_chaosLevel;

    double kickProb = level / 10.0 * 0.15; // ~1.5%/tick at level 1 .. 15%/tick at level 10
    if (randUnit() < kickProb) {
        _chaosRateSec += randSigned() * level * 0.35;
    }

    double reversion = 2.5 / level; // weaker pull-back at higher levels
    _chaosRateSec *= (1.0 - reversion * dt_sec);
    _chaosRateSec += -0.15 * _chaosOffsetSec * dt_sec; // spring back toward real time

    // Hard cap on velocity: without one, a run of same-direction kicks can
    // accumulate faster than the weak high-level reversion pulls it back
    // (this used to show up as the second hand "teleporting" and the
    // renderer visibly lagging under sustained bad luck).
    double maxRate = level * 4.0;
    if (_chaosRateSec > maxRate) _chaosRateSec = maxRate;
    if (_chaosRateSec < -maxRate) _chaosRateSec = -maxRate;

    _chaosOffsetSec += _chaosRateSec * dt_sec;
}

// "Temporal anomaly" wobble: the displayed time runs faster and slower
// than reality, but stays exactly correct on average. Modeled as a
// sinusoidal *rate* around 1.0x: rate(t) = 1 + A*sin(2*pi*t/P). Integrating
// that rate gives the displayed-time offset from real time:
//   offset(t) = C * (1 - cos(2*pi*t/P)),  C = A*P/(2*pi)
// offset(t) is periodic with period P, so offset(t+60) - offset(t) is
// exactly zero whenever P evenly divides 60 — meaning displayed time gains
// exactly as much as it loses over *any* 60-second window, not just ones
// aligned to a particular phase. This holds regardless of amplitude, so
// with A > 1 the rate swings negative for part of each cycle (min is 1-A)
// and the hands/digits visibly run backwards for a moment before catching
// back up — still exactly correct on average.
DisplayTime TemporalAnomalyTimeSource::getDisplayTime() {
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    double real_sec = (double)tv.tv_sec + tv.tv_usec / 1000000.0;

    double c = _amplitude * _periodSec / (2.0 * M_PI);
    double phase = 2.0 * M_PI * real_sec / _periodSec;
    double wobble = c * (1.0 - cos(phase));

    // Chaos walk uses real elapsed time since the previous call (via the
    // monotonic esp_timer, immune to NTP adjustments of the wall clock)
    // rather than an assumed fixed tick period: if the caller's render
    // pipeline falls behind, integrating with a hardcoded dt would make
    // the chaos physics silently drift from its intended speed. Clamped
    // so a long stall (e.g. the AP-mode portal blocking the app loop)
    // can't inject one giant kick when ticking resumes.
    int64_t now_us = esp_timer_get_time();
    double dt_sec = (_lastTickUs > 0) ? (double)(now_us - _lastTickUs) / 1e6 : 0.03;
    if (dt_sec < 0.0) dt_sec = 0.03;
    if (dt_sec > 0.2) dt_sec = 0.2;
    _lastTickUs = now_us;
    tickChaos(dt_sec);

    double total_sec = real_sec + wobble + _chaosOffsetSec;

    // Instantaneous rate deviation from 1.0x: d(wobble)/d(real_sec) works
    // out to A*sin(phase) (the derivative of c*(1-cos(phase)) w.r.t. real
    // time, using c = A*P/(2*pi) so the P/(2*pi) factors cancel), plus
    // the chaos walk's current velocity, which by construction already
    // *is* its instantaneous rate contribution (_chaosOffsetSec
    // integrates _chaosRateSec each tick). See lastRateDeviation().
    _lastRateDeviation = _amplitude * sin(phase) + _chaosRateSec;

    // total_sec is still a UTC epoch value (both perturbations only add a
    // small offset in seconds, timezone-agnostic). Break it down via
    // localtime_r() rather than raw modulo arithmetic so the result
    // respects the configured/detected timezone (see sntp_client.cpp's
    // setenv("TZ", ...)/tzset()) instead of always being UTC.
    double frac = total_sec - floor(total_sec);
    time_t epoch = (time_t)total_sec;
    struct tm tm_local;
    localtime_r(&epoch, &tm_local);

    DisplayTime dt;
    dt.hour = tm_local.tm_hour;
    dt.minute = tm_local.tm_min;
    dt.second = tm_local.tm_sec + frac;
    dt.wday = tm_local.tm_wday;
    dt.mday = tm_local.tm_mday;

    // Level 11: "completely random numbers" — the displayed second is
    // fully random, re-rolled once per real elapsed second (not every
    // ~30ms tick, or it'd read as flicker/noise rather than a
    // broken-but-legible clock). Hour/minute/date keep tracking real
    // time; only "the second progression" goes chaotic, per the original
    // request. No fractional sweep either — the hand jumps discretely,
    // matching the digits.
    if (_chaosLevel >= 11) {
        uint32_t real_whole_sec = (uint32_t)real_sec;
        if (real_whole_sec != _level11ShownRealSec) {
            _level11ShownRealSec = real_whole_sec;
            _level11RandomSec = esp_random() % 60;
        }
        dt.second = (double)_level11RandomSec;
    }

    return dt;
}
