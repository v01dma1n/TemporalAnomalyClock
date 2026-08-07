// temporal_anomaly_time_source.h — the "temporal anomaly" itself: what
// makes this clock's displayed time diverge from real time. Deliberately
// kept separate from disp_driver_gc9a01_round_clock.{h,cpp} so the display
// driver stays a generic, reusable "round LCD watch face" with no
// knowledge of wobble/chaos — it just renders whatever
// IDisplayTimeProvider::getDisplayTime() hands it (see
// i_display_time_provider.h).
//
// Combines two independent, additive perturbations on top of real time:
//
//   1. A periodic sinusoidal "wobble" — exactly correct on average over
//      any window equal to (a multiple of) its period, when the period
//      evenly divides 60. See setWobbleParams() and getDisplayTime().
//   2. A "chaos" damped random walk, levels 0-11, layered additively on
//      top — unlike the wobble, this makes NO accuracy guarantee. See
//      setChaosLevel() and tickChaos().
//
// Real system time (NTP sync, logs, preferences, time()) is never
// touched — this class only affects what getDisplayTime() returns.

#pragma once

#include "i_display_time_provider.h"

#include <cstdint>

class TemporalAnomalyTimeSource : public IDisplayTimeProvider {
public:
    // periodSec must be > 0 (clamped to 1 if not). amplitude 1.0 = steady
    // real time; >1.0 lets the displayed time run backwards for part of
    // each cycle.
    void setWobbleParams(double periodSec, double amplitude);

    // 0 = off, 1-3 subtle speed variations/occasional skips, 4-7
    // noticeable accel/decel/short reversals, 8-10 extreme/unpredictable,
    // 11 = the displayed second becomes fully random (re-rolled once per
    // real elapsed second, no fractional sweep). Clamped to [0,11].
    void setChaosLevel(int level);

    // --- IDisplayTimeProvider ------------------------------------------
    DisplayTime getDisplayTime() override;

    // How fast displayed time is *currently* running relative to real
    // time, as a deviation from 1.0x (0.0 = normal speed, positive =
    // running fast, negative = running slow) -- the instantaneous rate,
    // combining both the sinusoidal wobble's rate contribution
    // (A*sin(phase)) and the chaos walk's current velocity
    // (_chaosRateSec). Updated on each getDisplayTime() call; meant for
    // driving a UI indicator (see DispDriverGc9a01RoundClock::
    // setRateGauge()), not for anything precision-sensitive.
    double lastRateDeviation() const { return _lastRateDeviation; }

private:
    void tickChaos(double dt_sec);

    double _periodSec = 10.0;
    double _amplitude = 1.4;

    int _chaosLevel = 0;               // 0-11
    double _chaosOffsetSec = 0.0;      // accumulated offset, added to total_sec
    double _chaosRateSec = 0.0;        // current chaos "velocity" (can go negative)
    int64_t _lastTickUs = 0;           // real wall-clock time of the previous call, for tickChaos()'s dt
    double _lastRateDeviation = 0.0;   // see lastRateDeviation()

    uint32_t _level11ShownRealSec = UINT32_MAX; // gates level-11's once-per-real-second reroll
    uint32_t _level11RandomSec = 0;
};
