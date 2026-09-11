// ChainRope.cpp — sample 09: Verlet points and the relaxation loop.
//
// A rope is the cheapest interesting physics you can write, because Verlet
// integration makes the hard part disappear. You never store velocity; you store
// where a point *was*, and velocity is implied by the gap:
//
//     next = current + (current - previous) + acceleration * dt^2
//
// Enforcing a constraint is then just moving points, and the motion takes care
// of itself — no impulse bookkeeping, no velocity fixups.
//
// The catch is that satisfying one distance constraint breaks its neighbour, so
// you do not solve; you *relax*, sweeping the list repeatedly and getting closer
// each pass. That iteration count is the knob this sample is really about: at 1
// the rope is elastic, at 20 it is a chain. Nothing else changes.
#include "Sample.hpp"
#include "scene/Verlet.hpp"
#include "core/math/Random.hpp"
#include "render/IRenderer.hpp"
#include "platform/Window.hpp"
#include "core/debug/Debug.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

namespace samples {
namespace {

using otacon::Color;

// Point, Link and the solver used to live here. They are otacon::VerletPoint,
// otacon::DistanceConstraint and otacon::VerletBody now -- a constraint solver is
// a physics subsystem, not something a sample should own.
using otacon::VerletBody;
using otacon::VerletPoint;
using otacon::DistanceConstraint;

enum Shape { kRope = 0, kBridge, kCloth, kShapeCount };
const char* kShapeName[kShapeCount] = {"rope", "bridge", "cloth"};

class ChainRope final : public Sample {
public:
    void init(SampleContext& ctx) override { W_ = float(ctx.logicalW); H_ = float(ctx.logicalH); }

    void enter() override {
        shape_ = kRope; iterations_ = 8; gravity_ = 1400.f; damping_ = 0.995f;
        showLinks_ = true; grabbed_ = -1; wind_ = false; t_ = 0;
        build();
    }

    void handleInput(const otacon::InputFrame& in) override {
        mx_ = in.mouseNx * W_; my_ = in.mouseNy * H_;
        if (in.isPressed(otacon::Action::Jump)) { shape_ = (shape_ + 1) % kShapeCount; build(); }
        if (in.isPressed(otacon::Action::Aux6)) wind_ = !wind_;              // E
        if (in.isPressed(otacon::Action::Aux4)) showLinks_ = !showLinks_;    // F4
        if (in.selectSlot >= 1 && in.selectSlot <= 9) iterations_ = kIterPreset[in.selectSlot - 1];

        if (in.dragPressed) grabbed_ = body_.nearest(mx_, my_);
        if (!in.dragHeld) grabbed_ = -1;
    }

    void update(otacon::Real dt) override {
        const float step = otacon::toFloat(dt);
        if (step <= 0.f || step > 0.1f) return;
        t_ += step;

        if (grabbed_ >= 0 && grabbed_ < int(body_.points.size())) {
            // teleport() moves without implying velocity, so releasing a
            // dragged point does not fling it.
            body_.points[grabbed_].teleport(mx_, my_);
        }

        body_.gravity = gravity_;
        body_.damping = damping_;
        body_.iterations = iterations_;
        body_.integrate(step);
        if (wind_) {
            const float gust = std::sin(t_ * 1.7f) * 0.6f + std::sin(t_ * 4.3f) * 0.25f;
            for (VerletPoint& p : body_.points) if (!p.pinned) p.x += gust * 26.f * step;
        }
        // The relaxation sweep. More passes = stiffer, and this loop is the
        // entire difference between a bungee and a chain. Interleaving the floor
        // with the passes is why a pile settles instead of sinking.
        for (int i = 0; i < iterations_; ++i) {
            body_.relax();
            body_.collideFloor(H_ - 26.f);
        }
    }

    void render(otacon::IRenderer& r, const otacon::DebugRuntime&) override {
        r.fillRect(0, H_ - 26, W_, 26, Color{0.14f, 0.17f, 0.23f, 1.f});

        if (showLinks_) {
            for (const DistanceConstraint& l : body_.links) {
                const VerletPoint& a = body_.points[l.a];
                const VerletPoint& b = body_.points[l.b];
                const float dx = b.x - a.x, dy = b.y - a.y;
                const float d = std::sqrt(dx * dx + dy * dy);
                // Colour by strain: blue is slack, red is stretched past rest.
                const float s = l.rest > 0.0001f ? (d - l.rest) / l.rest : 0.f;
                const float k = std::min(1.f, std::fabs(s) * 6.f);
                const Color c{0.35f + k * 0.65f, 0.70f - k * 0.45f, 0.95f - k * 0.70f, 1.f};
                r.drawLine(a.x, a.y, b.x, b.y, c, 1.f);
            }
        }
        for (std::size_t i = 0; i < body_.points.size(); ++i) {
            const VerletPoint& p = body_.points[i];
            if (p.pinned) r.fillRect(p.x - 3, p.y - 3, 6, 6, Color{1.f, 0.82f, 0.35f, 1.f});
            else if (int(i) == grabbed_) r.fillRect(p.x - 3, p.y - 3, 6, 6, Color{1.f, 1.f, 1.f, 1.f});
            else if (shape_ != kCloth) r.fillRect(p.x - 1.5f, p.y - 1.5f, 3, 3, Color{0.70f, 0.78f, 0.92f, 1.f});
        }

        drawPanel(r);
    }

    const char* status() const override {
        std::snprintf(buf_, sizeof buf_, "%s  %d points  %d links  %d iterations  strain %.1f%%",
                      kShapeName[shape_], int(body_.points.size()), int(body_.links.size()),
                      iterations_, body_.strain() * 100.f);
        return buf_;
    }
    const char* keys() const override {
        return "SPACE  rope / bridge / cloth\nE      wind\nF4     show the links\n1-9    relaxation iterations (1 = elastic, 20 = rigid)\nright-drag  grab a point";
    }

private:
    static constexpr int kIterPreset[9] = {1, 2, 3, 5, 8, 12, 16, 24, 40};

    void build() {
        const float top = layout::kTop + 30;
        switch (shape_) {
            case kRope:   body_.makeRope(W_ * 0.5f - 40, top, 46, 8.f); break;
            case kBridge: body_.makeBridge(70.f, W_ - 70.f, top + 40, 26); break;
            default:      body_.makeCloth(W_ * 0.5f - 7.5f * 12.f, top, 16, 12, 12.f); break;
        }
        body_.gravity = gravity_;
        body_.damping = damping_;
        body_.iterations = iterations_;
    }

    void drawPanel(otacon::IRenderer& r) const {
        const float px = 8;
        float y = layout::kTop + 4;
        r.drawText("VERLET", px, y, 1.f, Color{0.55f, 0.80f, 1.f, 1.f}); y += 10;
        r.drawText("next = cur + (cur-prev) + a*dt^2", px, y, 1.f, Color{0.62f, 0.68f, 0.80f, 1.f}); y += 8;
        r.drawText("no velocity is ever stored", px, y, 1.f, Color{0.42f, 0.48f, 0.60f, 1.f});

        // The iteration knob, as a bar: the one number that decides the feel.
        const float bx = W_ - 176, by = layout::kTop + 4;
        r.drawText("RELAXATION PASSES", bx, by, 1.f, Color{0.55f, 0.80f, 1.f, 1.f});
        r.fillRect(bx, by + 12, 160, 7, Color{0.12f, 0.14f, 0.20f, 1.f});
        r.fillRect(bx, by + 12, 160.f * (float(iterations_) / 40.f), 7, Color{0.45f, 0.82f, 1.f, 1.f});
        char t[64];
        std::snprintf(t, sizeof t, "%d  (1-9 presets)", iterations_);
        r.drawText(t, bx, by + 22, 1.f, Color{0.55f, 0.62f, 0.75f, 1.f});
        std::snprintf(t, sizeof t, "worst strain %.1f%%", body_.strain() * 100.f);
        r.drawText(t, bx, by + 32, 1.f, Color{0.55f, 0.62f, 0.75f, 1.f});
        r.drawText(iterations_ <= 2 ? "elastic — constraints barely hold"
                                    : (iterations_ >= 16 ? "rigid — behaves like a chain"
                                                         : "springy — the usual compromise"),
                   bx, by + 42, 1.f, Color{1.f, 0.85f, 0.35f, 1.f});

        r.drawText("link colour is strain: blue slack, red stretched",
                   8, H_ - 10, 1.f, Color{0.42f, 0.48f, 0.60f, 1.f});
    }

    VerletBody body_;
    float W_ = 640, H_ = 400, mx_ = 0, my_ = 0, t_ = 0;
    float gravity_ = 1400.f, damping_ = 0.995f;
    int   shape_ = kRope, iterations_ = 8, grabbed_ = -1;
    bool  showLinks_ = true, wind_ = false;
    mutable char buf_[160]{};
};

} // namespace

Sample* makeChainRope() { return new ChainRope(); }

} // namespace samples
