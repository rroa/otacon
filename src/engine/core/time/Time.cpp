/*
===========================================================================

OTACON ENGINE
core/time/Time.cpp - clock and frame stepping

Turns the OS monotonic clock into the per-frame dt the simulation consumes,
under one of two policies. Variable is what the original flixel game does and
stays the default so the feel is unchanged; fixed accumulates real time and
emits whole steps, which is what makes a run reproducible regardless of frame
rate. F5 switches between them at runtime so the difference can be felt.

===========================================================================
*/
#include "core/time/Time.hpp"
#include <chrono>

namespace otacon {

using ns = std::chrono::nanoseconds;

/*
==================
nowNs

The monotonic clock, in nanoseconds. Monotonic and not wall-clock: a clock
that can be stepped backwards by NTP would hand the simulation a negative
dt.
==================
*/
static std::uint64_t nowNs() {
    return std::uint64_t(std::chrono::duration_cast<ns>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}

/*
==================
Clock
==================
*/
Clock::Clock() : start_(nowNs()) {}

/*
==================
Clock::now
==================
*/
double Clock::now() const { return double(nowNs() - start_) * 1e-9; }

/*
==================
TimeManager
==================
*/
TimeManager::TimeManager(Policy policy, double targetFps, double maxElapsed)
    : policy_(policy), fixedDt_(1.0 / targetFps), maxElapsed_(maxElapsed) {
    last_ = clock_.now();
}

/*
===========================
TimeManager::nudgeTimeScale

Step through preset time scales. Scaling the real elapsed time fed into the
stepping, rather than the dt handed out, is what keeps fixed-timestep
determinism intact while in slow motion.
===========================
*/
void TimeManager::nudgeTimeScale(int dir) {
    static const double presets[] = {0.1, 0.25, 0.5, 1.0, 2.0, 4.0};
    int idx = 3;   // 1.0x
    for (int i = 0; i < 6; ++i) if (presets[i] <= timeScale_ + 1e-6) idx = i;
    idx += (dir > 0 ? 1 : -1);
    if (idx < 0) idx = 0;
    if (idx > 5) idx = 5;
    timeScale_ = presets[idx];
}

/*
=======================
TimeManager::beginFrame

One frame of the clock. Variable clamps the elapsed time and returns a single
step - the clamp is what stops a breakpoint or a dragged window from
teleporting everything through a wall on resume. Fixed accumulates and
returns however many whole steps have banked up.
=======================
*/
int TimeManager::beginFrame() {
    double t = clock_.now();
    double frameTime = t - last_;
    last_ = t;
    if (frameTime < 0) frameTime = 0;

    // Record real frame time for the perf graph + FPS (independent of time scale).
    history_[histHead_] = float(frameTime);
    histHead_ = (histHead_ + 1) % kHistory;
    total_ += frameTime;
    ++frames_;
    fpsTimer_ += frameTime;
    ++fpsFrames_;
    if (fpsTimer_ >= 0.25) {
        fps_ = fpsFrames_ / fpsTimer_;
        fpsTimer_ = 0.0; fpsFrames_ = 0;
    }

    // Scale the *input* real time so the fixed-step dt stays constant -> slow-mo
    // and fast-forward keep the simulation deterministic.
    double simTime = frameTime * timeScale_;

    if (policy_ == Policy::Variable) {
        // flixel model: clamp the spike, run a single step.
        stepDelta_ = simTime > maxElapsed_ ? maxElapsed_ : simTime;
        return 1;
    }
    // Fixed: bank scaled time, emit whole steps of the constant fixed dt.
    accumulator_ += simTime;
    if (accumulator_ > maxElapsed_ * 8) accumulator_ = maxElapsed_ * 8;
    stepDelta_ = fixedDt_;
    int steps = 0;
    while (accumulator_ >= fixedDt_) { accumulator_ -= fixedDt_; ++steps; }
    return steps;
}

} // namespace otacon
