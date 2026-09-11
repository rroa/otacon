// Pendulum.cpp — sample 08: the integrator is a design decision.
//
// The engine's Entity integrator moves AABBs; a pendulum has an angular
// constraint instead, so this sample brings its own maths. That makes it the
// right place to show something the rest of the engine takes for granted: *how*
// you step a differential equation changes the answer, and the error has a
// character you can see.
//
// Three pendulums run side by side from identical initial conditions, one per
// integrator. The plot underneath is total energy, which is conserved by the
// real system — so any drift away from the dashed line is the integrator lying
// to you. Explicit Euler gains energy and spirals out. Semi-implicit Euler (what
// almost every game engine, including this one, actually uses) wobbles around
// the truth but stays bounded. RK4 tracks it closely for far longer.
//
// The double-pendulum mode is the other half of the lesson: even a perfect
// integrator cannot save you from a system where the answer depends on the
// initial conditions more precisely than you can represent them.
#include "Sample.hpp"
#include "render/IRenderer.hpp"
#include "platform/Window.hpp"
#include "core/debug/Debug.hpp"
#include <cmath>
#include <cstdio>

namespace samples {
namespace {

using otacon::Color;

constexpr float kG = 9.81f;
constexpr int   kIntegrators = 3;
constexpr int   kPlot = 240;

const char* kNames[kIntegrators] = {"explicit Euler", "semi-implicit Euler", "RK4"};
constexpr Color kColors[kIntegrators] = {
    {1.00f, 0.45f, 0.38f, 1.f}, {0.45f, 0.85f, 0.55f, 1.f}, {0.45f, 0.72f, 1.00f, 1.f},
};

// A simple pendulum: theta'' = -(g/L) sin(theta).
struct Simple {
    float theta = 0, omega = 0, length = 1.f;

    float accel(float th) const { return -(kG / length) * std::sin(th); }
    // Energy per unit mass, measured from the lowest point.
    float energy() const {
        const float v = omega * length;
        return 0.5f * v * v + kG * length * (1.f - std::cos(theta));
    }

    void stepExplicitEuler(float dt) {
        // Both updates read the OLD state. This is the version that gains energy.
        const float a = accel(theta);
        theta += omega * dt;
        omega += a * dt;
    }
    void stepSemiImplicit(float dt) {
        // Velocity first, then position using the NEW velocity. One line's
        // difference from the above, and it is the difference between a stable
        // game and a divergent one.
        omega += accel(theta) * dt;
        theta += omega * dt;
    }
    void stepRK4(float dt) {
        auto deriv = [&](float th, float om, float& dth, float& dom) {
            dth = om; dom = accel(th);
        };
        float k1t, k1o, k2t, k2o, k3t, k3o, k4t, k4o;
        deriv(theta, omega, k1t, k1o);
        deriv(theta + 0.5f * dt * k1t, omega + 0.5f * dt * k1o, k2t, k2o);
        deriv(theta + 0.5f * dt * k2t, omega + 0.5f * dt * k2o, k3t, k3o);
        deriv(theta + dt * k3t, omega + dt * k3o, k4t, k4o);
        theta += dt / 6.f * (k1t + 2 * k2t + 2 * k3t + k4t);
        omega += dt / 6.f * (k1o + 2 * k2o + 2 * k3o + k4o);
    }
};

// A double pendulum, stepped with RK4. The equations are the standard
// Lagrangian result for two point masses on massless rods.
struct Double {
    float t1 = 2.2f, t2 = 2.6f, w1 = 0, w2 = 0;
    float l1 = 0.9f, l2 = 0.9f, m1 = 1.f, m2 = 1.f;

    void deriv(float a1, float a2, float v1, float v2,
               float& da1, float& da2, float& dv1, float& dv2) const {
        const float d = a1 - a2, sd = std::sin(d), cd = std::cos(d);
        const float den = 2 * m1 + m2 - m2 * std::cos(2 * a1 - 2 * a2);
        da1 = v1; da2 = v2;
        dv1 = (-kG * (2 * m1 + m2) * std::sin(a1)
               - m2 * kG * std::sin(a1 - 2 * a2)
               - 2 * sd * m2 * (v2 * v2 * l2 + v1 * v1 * l1 * cd)) / (l1 * den);
        dv2 = (2 * sd * (v1 * v1 * l1 * (m1 + m2)
               + kG * (m1 + m2) * std::cos(a1)
               + v2 * v2 * l2 * m2 * cd)) / (l2 * den);
    }
    void step(float dt) {
        float a1[4], a2[4], v1[4], v2[4];
        deriv(t1, t2, w1, w2, a1[0], a2[0], v1[0], v2[0]);
        deriv(t1 + .5f * dt * a1[0], t2 + .5f * dt * a2[0], w1 + .5f * dt * v1[0], w2 + .5f * dt * v2[0],
              a1[1], a2[1], v1[1], v2[1]);
        deriv(t1 + .5f * dt * a1[1], t2 + .5f * dt * a2[1], w1 + .5f * dt * v1[1], w2 + .5f * dt * v2[1],
              a1[2], a2[2], v1[2], v2[2]);
        deriv(t1 + dt * a1[2], t2 + dt * a2[2], w1 + dt * v1[2], w2 + dt * v2[2],
              a1[3], a2[3], v1[3], v2[3]);
        t1 += dt / 6 * (a1[0] + 2 * a1[1] + 2 * a1[2] + a1[3]);
        t2 += dt / 6 * (a2[0] + 2 * a2[1] + 2 * a2[2] + a2[3]);
        w1 += dt / 6 * (v1[0] + 2 * v1[1] + 2 * v1[2] + v1[3]);
        w2 += dt / 6 * (v2[0] + 2 * v2[1] + 2 * v2[2] + v2[3]);
    }
};

class Pendulum final : public Sample {
public:
    void init(SampleContext& ctx) override { W_ = float(ctx.logicalW); H_ = float(ctx.logicalH); }

    void enter() override {
        doubleMode_ = false; bigStep_ = false; head_ = 0; t_ = 0;
        for (int i = 0; i < kIntegrators; ++i) {
            p_[i] = Simple{};
            p_[i].theta = kStartTheta;
            p_[i].omega = 0.f;
            p_[i].length = 1.f;
            for (int k = 0; k < kPlot; ++k) plot_[i][k] = p_[i].energy();
        }
        baseline_ = p_[0].energy();
        // Two double pendulums a hair apart, to show divergence.
        dbl_[0] = Double{};
        dbl_[1] = Double{};
        dbl_[1].t1 += 0.001f;
        trailLen_ = 0;
    }

    void handleInput(const otacon::InputFrame& in) override {
        if (in.isPressed(otacon::Action::Jump)) doubleMode_ = !doubleMode_;
        if (in.isPressed(otacon::Action::Aux6)) bigStep_ = !bigStep_;       // E
        if (in.isPressed(otacon::Action::Aux4)) enter();                    // F4 restart
    }

    void update(otacon::Real dt) override {
        // A deliberately coarse step is where the integrators separate fastest;
        // the honest default is the frame's own dt.
        const float step = bigStep_ ? 1.f / 20.f : otacon::toFloat(dt);
        if (step <= 0.f || step > 0.2f) return;
        t_ += step;

        if (doubleMode_) {
            for (int i = 0; i < 2; ++i) dbl_[i].step(step);
            if (trailLen_ < kTrail) ++trailLen_;
            trailHead_ = (trailHead_ + 1) % kTrail;
            for (int i = 0; i < 2; ++i) {
                const float x = dbl_[i].l1 * std::sin(dbl_[i].t1) + dbl_[i].l2 * std::sin(dbl_[i].t2);
                const float y = dbl_[i].l1 * std::cos(dbl_[i].t1) + dbl_[i].l2 * std::cos(dbl_[i].t2);
                trail_[i][trailHead_][0] = x;
                trail_[i][trailHead_][1] = y;
            }
            return;
        }

        p_[0].stepExplicitEuler(step);
        p_[1].stepSemiImplicit(step);
        p_[2].stepRK4(step);
        for (int i = 0; i < kIntegrators; ++i) plot_[i][head_] = p_[i].energy();
        head_ = (head_ + 1) % kPlot;
    }

    void render(otacon::IRenderer& r, const otacon::DebugRuntime&) override {
        if (doubleMode_) { renderDouble(r); return; }

        const float top = layout::kTop + 8;
        const float cellW = W_ / 3.f;
        const float pivotY = top + 24, scale = 92.f;

        for (int i = 0; i < kIntegrators; ++i) {
            const float cx = cellW * i + cellW * 0.5f;
            if (i) r.drawLine(cellW * i, top, cellW * i, top + 210, Color{1, 1, 1, 0.08f}, 1.f);
            r.drawText(kNames[i], cellW * i + 8, top, 1.f, kColors[i]);

            // The arc the bob should stay on, if energy were conserved.
            for (int k = -28; k <= 28; ++k) {
                const float a = kStartTheta * (k / 28.f);
                r.fillRect(cx + std::sin(a) * scale, pivotY + std::cos(a) * scale, 1, 1,
                           Color{1, 1, 1, 0.10f});
            }
            const float bx = cx + std::sin(p_[i].theta) * scale;
            const float by = pivotY + std::cos(p_[i].theta) * scale;
            r.drawLine(cx, pivotY, bx, by, Color{0.55f, 0.60f, 0.72f, 1.f}, 1.f);
            r.fillRect(cx - 2, pivotY - 2, 4, 4, Color{0.75f, 0.80f, 0.92f, 1.f});
            r.fillRect(bx - 5, by - 5, 10, 10, kColors[i]);

            char info[64];
            const float e = p_[i].energy();
            std::snprintf(info, sizeof info, "E %.3f  (%+.1f%%)", e,
                          baseline_ > 0 ? (e / baseline_ - 1.f) * 100.f : 0.f);
            r.drawText(info, cellW * i + 8, top + 192, 1.f, kColors[i]);
        }

        // --- the energy plot -------------------------------------------------
        const float gx = 30, gy = H_ - 112, gw = W_ - 60, gh = 78;
        r.fillRect(gx, gy, gw, gh, Color{0.06f, 0.07f, 0.10f, 1.f});
        r.drawRectOutline(gx, gy, gw, gh, Color{0.22f, 0.28f, 0.38f, 1.f}, 1.f);
        r.drawText("TOTAL ENERGY  (flat = correct)", gx, gy - 10, 1.f, Color{0.55f, 0.80f, 1.f, 1.f});

        // The conserved value, as a dashed reference line.
        const float mid = gy + gh * 0.5f;
        for (float x = gx; x < gx + gw; x += 6) r.fillRect(x, mid, 3, 1, Color{1, 1, 1, 0.35f});

        const float span = baseline_ > 0 ? baseline_ * 1.4f : 1.f;
        for (int i = 0; i < kIntegrators; ++i) {
            for (int k = 1; k < kPlot; ++k) {
                const float e0 = plot_[i][(head_ + k - 1) % kPlot];
                const float e1 = plot_[i][(head_ + k) % kPlot];
                const float x0 = gx + (float(k - 1) / kPlot) * gw;
                const float x1 = gx + (float(k) / kPlot) * gw;
                auto mapY = [&](float e) {
                    float y = mid - (e - baseline_) / span * gh;
                    return y < gy ? gy : (y > gy + gh ? gy + gh : y);
                };
                r.drawLine(x0, mapY(e0), x1, mapY(e1), kColors[i], 1.f);
            }
        }
        char foot[128];
        std::snprintf(foot, sizeof foot, "step %s   t %.1fs",
                      bigStep_ ? "1/20 s (coarse — watch Euler go)" : "frame dt", t_);
        r.drawText(foot, gx, gy + gh + 5, 1.f, Color{0.55f, 0.62f, 0.75f, 1.f});
        r.drawText("SPACE for the double pendulum", gx, gy + gh + 15, 1.f, Color{0.42f, 0.48f, 0.60f, 1.f});
    }

    const char* status() const override {
        if (doubleMode_) {
            const float sep = std::fabs(dbl_[0].t1 - dbl_[1].t1) + std::fabs(dbl_[0].t2 - dbl_[1].t2);
            std::snprintf(buf_, sizeof buf_, "double pendulum  t %.1fs  separation %.5f rad", t_, sep);
        } else {
            std::snprintf(buf_, sizeof buf_, "euler %+.1f%%  semi %+.1f%%  rk4 %+.1f%%  step %s",
                          pct(0), pct(1), pct(2), bigStep_ ? "coarse" : "dt");
        }
        return buf_;
    }
    const char* keys() const override {
        return "SPACE  simple (3 integrators) / double pendulum\nE      coarse fixed step vs frame dt\nF4     restart from the same state";
    }

private:
    static constexpr float kStartTheta = 2.3f;
    static constexpr int kTrail = 700;

    float pct(int i) const { return baseline_ > 0 ? (p_[i].energy() / baseline_ - 1.f) * 100.f : 0.f; }

    void renderDouble(otacon::IRenderer& r) {
        const float cx = W_ * 0.5f, cy = layout::kTop + 96, scale = 88.f;
        const Color cols[2] = {{0.45f, 0.72f, 1.00f, 1.f}, {1.00f, 0.55f, 0.40f, 1.f}};

        // Trails first, so the arms draw over them.
        for (int i = 0; i < 2; ++i) {
            for (int k = 1; k < trailLen_; ++k) {
                const int a = (trailHead_ - k + kTrail) % kTrail;
                const float fade = 1.f - float(k) / kTrail;
                Color c = cols[i]; c.a = fade * 0.5f;
                r.fillRect(cx + trail_[i][a][0] * scale, cy + trail_[i][a][1] * scale, 1, 1, c);
            }
        }
        for (int i = 0; i < 2; ++i) {
            const Double& d = dbl_[i];
            const float x1 = cx + d.l1 * std::sin(d.t1) * scale;
            const float y1 = cy + d.l1 * std::cos(d.t1) * scale;
            const float x2 = x1 + d.l2 * std::sin(d.t2) * scale;
            const float y2 = y1 + d.l2 * std::cos(d.t2) * scale;
            r.drawLine(cx, cy, x1, y1, cols[i], 1.f);
            r.drawLine(x1, y1, x2, y2, cols[i], 1.f);
            r.fillRect(x1 - 3, y1 - 3, 6, 6, cols[i]);
            r.fillRect(x2 - 4, y2 - 4, 8, 8, cols[i]);
        }
        r.fillRect(cx - 3, cy - 3, 6, 6, Color{0.8f, 0.85f, 0.95f, 1.f});

        float y = H_ - 62;
        r.drawText("TWO PENDULUMS, STARTED 0.001 RAD APART", 10, y, 1.f, Color{0.55f, 0.80f, 1.f, 1.f}); y += 11;
        r.drawText("Same integrator, same step, same code. They separate anyway:", 10, y, 1.f,
                   Color{0.62f, 0.68f, 0.80f, 1.f}); y += 9;
        r.drawText("the system amplifies any difference, including the last bit of a float.", 10, y, 1.f,
                   Color{0.62f, 0.68f, 0.80f, 1.f}); y += 9;
        r.drawText("This is why a deterministic replay needs identical arithmetic, not just identical code.",
                   10, y, 1.f, Color{0.42f, 0.48f, 0.60f, 1.f});
    }

    Simple p_[kIntegrators];
    Double dbl_[2];
    float  plot_[kIntegrators][kPlot]{};
    float  trail_[2][kTrail][2]{};
    int    head_ = 0, trailHead_ = 0, trailLen_ = 0;
    float  W_ = 640, H_ = 400, t_ = 0, baseline_ = 1.f;
    bool   doubleMode_ = false, bigStep_ = false;
    mutable char buf_[160]{};
};

} // namespace

Sample* makePendulum() { return new Pendulum(); }

} // namespace samples
