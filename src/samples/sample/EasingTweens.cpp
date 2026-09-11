// EasingTweens.cpp — sample 16: the shape of a motion, and what drives it.
//
// Two engine pieces that are easy to confuse. core/math/Ease.hpp SHAPES a 0..1;
// core/Tween.hpp DRIVES one over time. Everything on the left is the first,
// everything on the right is the second.
//
// The panel at the bottom is the part worth staying for. `a += (b - a) * 0.1f`
// is the most common smoothing in games and it is frame-rate dependent: it
// converges faster at 120fps than at 30, so a camera tuned on one machine feels
// wrong on another. ease::damp is the exponential form that does not have that
// problem, and running both at two different step rates side by side makes the
// difference impossible to argue with.
#include "Sample.hpp"
#include "core/Timer.hpp"
#include "core/Tween.hpp"
#include "core/math/Ease.hpp"
#include "render/IRenderer.hpp"
#include "platform/Window.hpp"
#include "core/debug/Debug.hpp"
#include <cmath>
#include <cstdio>

namespace samples {
namespace {

using otacon::Color;
namespace ease = otacon::ease;

struct Curve { const char* name; otacon::EaseFn fn; };

const Curve kCurves[] = {
    {"linear",       [](float t) { return t; }},
    {"smoothstep",   ease::smoothstep},
    {"smootherstep", ease::smootherstep},
    {"quadIn",       ease::quadIn},
    {"quadOut",      ease::quadOut},
    {"cubicInOut",   ease::cubicInOut},
    {"sineOut",      ease::sineOut},
    {"expoOut",      ease::expoOut},
    {"backOut",      ease::backOut},
    {"elasticOut",   ease::elasticOut},
    {"bounceOut",    ease::bounceOut},
};
constexpr int kCurveCount = int(sizeof kCurves / sizeof kCurves[0]);
constexpr int kRunners = 4;

class EasingTweens final : public Sample {
public:
    void init(SampleContext& ctx) override { W_ = float(ctx.logicalW); H_ = float(ctx.logicalH); }

    void enter() override {
        curve_ = 1; t_ = 0;
        tweens_.cancelAll();
        timers_.cancelAll();
        for (int i = 0; i < kRunners; ++i) runner_[i] = 0.f;
        // A repeater, so the difference between the two is visible at a glance:
        // this one fires forever on an interval and moves nothing.
        pulse_ = 0;
        timers_.every(0.5f, [this] { ++pulse_; });
        naive_ = damped_ = 0.f;
        target_ = 1.f;
        slowNaive_ = slowDamped_ = 0.f;
        accum_ = 0.f;
        fire();
    }

    void handleInput(const otacon::InputFrame& in) override {
        if (in.isPressed(otacon::Action::Jump)) fire();
        if (in.selectSlot >= 1 && in.selectSlot <= 9 && in.selectSlot <= kCurveCount)
            { curve_ = in.selectSlot - 1; fire(); }
        if (in.isPressed(otacon::Action::Aux4)) curve_ = (curve_ + 1) % kCurveCount, fire();
        if (in.isPressed(otacon::Action::Aux6)) { target_ = target_ > 0.5f ? 0.f : 1.f; }
    }

    void update(otacon::Real dt) override {
        const float d = otacon::toFloat(dt);
        t_ += d;
        timers_.update(dt);      // deferred work
        tweens_.update(dt);      // moving values

        // --- the frame-rate independence demonstration ----------------------
        // Both smoothers chase the same target. The top pair steps at the real
        // frame rate; the bottom pair steps at a deliberately coarse 15fps, and
        // only one of the two stays in agreement with its fast twin.
        naive_  += (target_ - naive_) * kNaiveFactor;
        damped_  = ease::damp(damped_, target_, kDampRate, d);

        accum_ += d;
        const float slowStep = 1.f / 15.f;
        while (accum_ >= slowStep) {
            accum_ -= slowStep;
            slowNaive_  += (target_ - slowNaive_) * kNaiveFactor;
            slowDamped_  = ease::damp(slowDamped_, target_, kDampRate, slowStep);
        }
    }

    void render(otacon::IRenderer& r, const otacon::DebugRuntime&) override {
        const float top = layout::kTop + 4;
        drawCurveGallery(r, 8, top, 250, 150);
        drawRunners(r, 272, top);
        drawFrameRatePanel(r, 8, H_ - 112);
    }

    const char* status() const override {
        std::snprintf(buf_, sizeof buf_, "%s  tweens %zu  timers %zu  pulses %d",
                      kCurves[curve_].name, tweens_.count(), timers_.count(), pulse_);
        return buf_;
    }
    const char* keys() const override {
        return "SPACE  fire the runners again\n1-9    pick a curve   F4 cycles\nE      flip the smoothing target";
    }

private:
    static constexpr float kNaiveFactor = 0.12f;   // the per-frame lerp everyone writes
    static constexpr float kDampRate    = 7.7f;    // tuned to match it at 60fps

    /*
     * Staggered starts, so four copies of one curve read as a wave and the shape
     * of the easing shows in the spacing rather than only the speed.
     *
     * The delay is a Timer, not a zero-length tween with a no-op curve. Both
     * would work and only one says what it means: core/Timer.hpp is for
     * deferring work, core/Tween.hpp is for moving a value.
     */
    void fire() {
        timers_.cancelAll();
        for (int i = 0; i < kRunners; ++i) {
            tweens_.cancelTarget(&runner_[i]);
            runner_[i] = 0.f;
        }
        tweens_.to(&runner_[0], 1.f, 1.2f, kCurves[curve_].fn);
        for (int i = 1; i < kRunners; ++i)
            timers_.after(float(i) * 0.12f,
                          [this, i] { tweens_.to(&runner_[i], 1.f, 1.2f, kCurves[curve_].fn); });
    }

    // The curve itself: t across, eased value up. The dashed 0..1 box is the
    // unit square, so an overshooting curve (back, elastic) visibly leaves it.
    void drawCurveGallery(otacon::IRenderer& r, float x, float y, float w, float h) const {
        r.drawText("EASE  -  shapes a 0..1", x, y, 1.f, Color{0.55f, 0.80f, 1.f, 1.f});
        const float gx = x, gy = y + 12, gw = w, gh = h;
        r.fillRect(gx, gy, gw, gh, Color{0.06f, 0.07f, 0.10f, 1.f});
        r.drawRectOutline(gx, gy, gw, gh, Color{0.22f, 0.28f, 0.38f, 1.f}, 1.f);
        // Mid-line and the unit-square top, both dashed.
        for (float dx = gx; dx < gx + gw; dx += 6) {
            r.fillRect(dx, gy + gh * 0.5f, 3, 1, Color{1, 1, 1, 0.12f});
            r.fillRect(dx, gy + gh * 0.22f, 3, 1, Color{1, 1, 1, 0.20f});
            r.fillRect(dx, gy + gh * 0.78f, 3, 1, Color{1, 1, 1, 0.20f});
        }

        // Every curve faintly, the selected one bright — a gallery and a
        // comparison at the same time.
        for (int c = 0; c < kCurveCount; ++c) {
            const bool sel = (c == curve_);
            const Color col = sel ? Color{1.f, 0.85f, 0.35f, 1.f} : Color{0.35f, 0.45f, 0.60f, 0.45f};
            const int steps = 64;
            for (int i = 1; i <= steps; ++i) {
                const float t0 = float(i - 1) / steps, t1 = float(i) / steps;
                const float v0 = kCurves[c].fn(t0), v1 = kCurves[c].fn(t1);
                r.drawLine(gx + t0 * gw, mapY(gy, gh, v0),
                           gx + t1 * gw, mapY(gy, gh, v1), col, sel ? 2.f : 1.f);
            }
        }
        r.drawText(kCurves[curve_].name, gx + 4, gy + gh - 9, 1.f, Color{1.f, 0.85f, 0.35f, 1.f});
        r.drawText("faint: every curve   bright: selected", gx, gy + gh + 4, 1.f,
                   Color{0.42f, 0.48f, 0.60f, 1.f});
    }
    static float mapY(float gy, float gh, float v) {
        // 0 sits at 78% down and 1 at 22%, leaving room for overshoot either way.
        return gy + gh * 0.78f - v * gh * 0.56f;
    }

    void drawRunners(otacon::IRenderer& r, float x, float y) const {
        r.drawText("TWEEN  -  drives one over time", x, y, 1.f, Color{0.55f, 0.80f, 1.f, 1.f});
        const float trackW = W_ - x - 16;
        for (int i = 0; i < kRunners; ++i) {
            const float ty = y + 18 + i * 26;
            r.fillRect(x, ty + 7, trackW, 2, Color{0.14f, 0.17f, 0.24f, 1.f});
            const float px = x + runner_[i] * (trackW - 16);
            r.fillRect(px, ty, 16, 16, Color{0.45f, 0.80f + 0.05f * i, 1.f, 1.f});
        }
        float ty = y + 18 + kRunners * 26 + 2;
        r.drawText("one curve, four starts staggered by Timers::after", x, ty, 1.f,
                   Color{0.42f, 0.48f, 0.60f, 1.f}); ty += 12;
        // Timers::every, ticking beside the tweens so the split is obvious.
        r.drawText("TIMER", x, ty, 1.f, Color{0.55f, 0.80f, 1.f, 1.f});
        char t[64]; std::snprintf(t, sizeof t, "every(0.5s) has fired %d times", pulse_);
        r.drawText(t, x + 34, ty, 1.f, Color{0.62f, 0.68f, 0.80f, 1.f}); ty += 9;
        for (int i = 0; i < 8; ++i)
            r.fillRect(x + 34 + i * 9, ty, 6, 4,
                       (pulse_ % 8) == i ? Color{1.f, 0.85f, 0.35f, 1.f}
                                         : Color{0.18f, 0.21f, 0.28f, 1.f});
        ty += 11;
        r.drawText("a timer defers work; a tween moves a value", x, ty, 1.f,
                   Color{0.42f, 0.48f, 0.60f, 1.f}); ty += 8;
        r.drawText("SPACE to fire again", x, ty, 1.f, Color{0.42f, 0.48f, 0.60f, 1.f});
    }

    void drawFrameRatePanel(otacon::IRenderer& r, float x, float y) const {
        r.fillRect(0, y - 6, W_, H_ - y + 6, Color{0.03f, 0.04f, 0.06f, 0.92f});
        r.drawText("WHY damp() EXISTS", x, y, 1.f, Color{0.55f, 0.80f, 1.f, 1.f});
        r.drawText("both chase the same target; the lower pair steps at 15fps", x, y + 10, 1.f,
                   Color{0.55f, 0.62f, 0.75f, 1.f});

        const float bx = x + 6, bw = W_ - 200;
        auto bar = [&](float ty, const char* label, float v, Color c) {
            r.drawText(label, x, ty - 1, 1.f, c);
            r.fillRect(bx + 96, ty, bw, 5, Color{0.12f, 0.14f, 0.20f, 1.f});
            r.fillRect(bx + 96, ty, bw * (v < 0 ? 0 : (v > 1 ? 1 : v)), 5, c);
        };
        bar(y + 24, "lerp 60fps",  naive_,      Color{1.00f, 0.55f, 0.40f, 1.f});
        bar(y + 33, "lerp 15fps",  slowNaive_,  Color{1.00f, 0.35f, 0.30f, 1.f});
        bar(y + 46, "damp 60fps",  damped_,     Color{0.45f, 0.90f, 0.60f, 1.f});
        bar(y + 55, "damp 15fps",  slowDamped_, Color{0.30f, 0.75f, 0.50f, 1.f});

        char t[128];
        const float lerpGap = std::fabs(naive_ - slowNaive_);
        const float dampGap = std::fabs(damped_ - slowDamped_);
        std::snprintf(t, sizeof t, "disagreement:  lerp %.3f    damp %.3f", lerpGap, dampGap);
        r.drawText(t, x, y + 68, 1.f, dampGap < lerpGap ? Color{0.45f, 0.90f, 0.60f, 1.f}
                                                        : Color{0.55f, 0.62f, 0.75f, 1.f});
        r.drawText("the lerp pair separates with frame rate; the damp pair does not",
                   x, y + 78, 1.f, Color{0.42f, 0.48f, 0.60f, 1.f});
    }

    otacon::Timers timers_;
    otacon::Tweens tweens_;
    float W_ = 640, H_ = 400, t_ = 0;
    float runner_[kRunners]{};
    float naive_ = 0, damped_ = 0, slowNaive_ = 0, slowDamped_ = 0;
    float target_ = 1.f, accum_ = 0.f;
    int   curve_ = 1, pulse_ = 0;
    mutable char buf_[96]{};
};

} // namespace

Sample* makeEasingTweens() { return new EasingTweens(); }

} // namespace samples
