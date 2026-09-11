#include "core/time/Time.hpp"
#include <chrono>

namespace otacon {

using ns = std::chrono::nanoseconds;
static std::uint64_t nowNs() {
    return std::uint64_t(std::chrono::duration_cast<ns>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}

Clock::Clock() : start_(nowNs()) {}
double Clock::now() const { return double(nowNs() - start_) * 1e-9; }

TimeManager::TimeManager(Policy policy, double targetFps, double maxElapsed)
    : policy_(policy), fixedDt_(1.0 / targetFps), maxElapsed_(maxElapsed) {
    last_ = clock_.now();
}

void TimeManager::nudgeTimeScale(int dir) {
    static const double presets[] = {0.1, 0.25, 0.5, 1.0, 2.0, 4.0};
    int idx = 3;   // 1.0x
    for (int i = 0; i < 6; ++i) if (presets[i] <= timeScale_ + 1e-6) idx = i;
    idx += (dir > 0 ? 1 : -1);
    if (idx < 0) idx = 0;
    if (idx > 5) idx = 5;
    timeScale_ = presets[idx];
}

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
