// Time.hpp — in-house timing.
//
// Clock wraps the OS monotonic clock and hands out seconds. TimeManager turns
// that wall-clock stream into the per-frame `elapsed` the simulation consumes.
//
// Two stepping policies (a teaching contrast):
//   * Variable  — elapsed = now-last, clamped to maxElapsed. This is exactly
//                 what the original flixel game does (maxElapsed = 1/20 s) so it
//                 is the default and keeps the feel identical.
//   * Fixed     — accumulate real time and emit N steps of a fixed dt. Makes
//                 the simulation deterministic regardless of frame rate, at the
//                 cost of a tiny bit of temporal aliasing; offered for the
//                 lesson on fixed-vs-variable timesteps.
#pragma once
#include <cstdint>

namespace otacon {

class Clock {
public:
    Clock();
    double now() const;        // seconds since construction (monotonic)
private:
    std::uint64_t start_;
};

class TimeManager {
public:
    enum class Policy { Variable, Fixed };

    explicit TimeManager(Policy policy = Policy::Variable,
                         double targetFps = 60.0,
                         double maxElapsed = 1.0 / 20.0);

    // Call once per rendered frame. Returns how many simulation steps to run;
    // read stepDelta() for the dt of each. (Variable policy returns 0 or 1.)
    int  beginFrame();
    double stepDelta() const { return stepDelta_; }

    // Switch policy at runtime (F5) to feel variable vs. fixed timestep.
    void   setPolicy(Policy p) { policy_ = p; accumulator_ = 0.0; }
    void   togglePolicy() { setPolicy(policy_ == Policy::Variable ? Policy::Fixed : Policy::Variable); }
    Policy policy() const { return policy_; }
    const char* policyName() const { return policy_ == Policy::Fixed ? "FIXED" : "VARIABLE"; }

    double fps() const { return fps_; }
    double totalTime() const { return total_; }
    std::uint64_t frameCount() const { return frames_; }

    // Simulation time scale (slow-mo / fast-forward). Scales the real elapsed
    // time fed into the stepping, so fixed-timestep determinism is preserved.
    void   nudgeTimeScale(int dir);   // dir +1 faster, -1 slower (preset steps)
    double timeScale() const { return timeScale_; }

    // Ring buffer of the last N real frame times (seconds) for an on-screen graph.
    static constexpr int kHistory = 120;
    const float* frameHistory() const { return history_; }
    int frameHistoryCount() const { return kHistory; }
    int frameHistoryHead() const { return histHead_; }   // index of the newest sample+1

private:
    Clock  clock_;
    Policy policy_;
    double fixedDt_;
    double maxElapsed_;
    double last_;
    double accumulator_ = 0.0;
    double stepDelta_ = 0.0;
    double total_ = 0.0;
    // fps smoothing
    double fpsTimer_ = 0.0;
    int    fpsFrames_ = 0;
    double fps_ = 0.0;
    std::uint64_t frames_ = 0;
    double timeScale_ = 1.0;
    float  history_[kHistory] = {0};
    int    histHead_ = 0;
};

} // namespace otacon
