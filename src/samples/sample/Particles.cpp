// Particles.cpp — sample 06: the engine's Emitter, with its knobs exposed.
//
// otacon::Emitter is a pool of ordinary Entities. That is the whole trick: a
// particle falls, drags and spins through exactly the same integrator as a
// player, so there is no second physics path to keep in sync. This sample wires
// every emitter parameter to a key so you can feel what each one does, and
// shows the pool's occupancy — because "how many particles are live" is the
// number that actually decides whether an effect is affordable.
#include "Sample.hpp"
#include "common/Art.hpp"
#include "scene/Emitter.hpp"
#include "scene/Camera.hpp"
#include "render/IRenderer.hpp"
#include "platform/Window.hpp"
#include "core/debug/Debug.hpp"
#include <cstdio>

namespace samples {
namespace {

using otacon::Color;

class Particles final : public Sample {
public:
    void init(SampleContext& ctx) override {
        r_ = ctx.renderer;
        W_ = float(ctx.logicalW); H_ = float(ctx.logicalH);
        cam_.setViewport(ctx.logicalW, ctx.logicalH);
        dotTex_ = r_->createTexture(art::dot(16));
        configure();
        em_.init(kPool);
    }
    void shutdown() override { if (dotTex_ && r_) { r_->destroyTexture(dotTex_); dotTex_ = 0; } }

    void enter() override {
        gravity_ = 320.f; spread_ = 160.f; spin_ = 220.f; drag_ = 0.f;
        textured_ = true; continuous_ = false;
        em_.init(kPool);
        configure();
        em_.seed(0xC0FFEE);
        burst();
    }

    void handleInput(const otacon::InputFrame& in) override {
        mouse_ = {in.mouseNx * W_, in.mouseNy * H_};
        if (in.isPressed(otacon::Action::Jump)) burst();
        if (in.isPressed(otacon::Action::Aux6)) {                    // E
            continuous_ = !continuous_;
            configure();
            if (continuous_) em_.start(false); else em_.stop();
        }
        if (in.isPressed(otacon::Action::Aux4)) { textured_ = !textured_; configure(); }   // F4
        // 1-8 tune the four parameters, low/high per pair.
        switch (in.selectSlot) {
            case 1: gravity_ = 0.f;    break;
            case 2: gravity_ = 900.f;  break;
            case 3: spread_  = 40.f;   break;
            case 4: spread_  = 320.f;  break;
            case 5: spin_    = 0.f;    break;
            case 6: spin_    = 720.f;  break;
            case 7: drag_    = 0.f;    break;
            case 8: drag_    = 260.f;  break;
            default: break;
        }
        if (in.selectSlot >= 1 && in.selectSlot <= 8) configure();
    }

    void update(otacon::Real dt) override {
        if (continuous_) em_.position = {mouse_.x, mouse_.y};
        em_.update(dt, cam_);
    }

    void render(otacon::IRenderer& r, const otacon::DebugRuntime&) override {
        em_.render(r, cam_);

        // Emitter origin marker.
        r.drawRectOutline(mouse_.x - 4, mouse_.y - 4, 8, 8, Color{1.f, 0.8f, 0.3f, 0.8f}, 1.f);

        // --- parameter readout ----------------------------------------------
        float y = layout::kTop + 6;
        r.drawText("EMITTER", 8, y, 1.f, Color{0.55f, 0.80f, 1.f, 1.f}); y += 11;
        y = row(r, y, "1/2 gravity", gravity_, 900.f);
        y = row(r, y, "3/4 spread",  spread_,  320.f);
        y = row(r, y, "5/6 spin",    spin_,    720.f);
        y = row(r, y, "7/8 drag",    drag_,    260.f);

        // --- pool occupancy: the number that decides affordability ----------
        const int live = em_.liveCount();
        const float barW = 150.f;
        y += 6;
        r.drawText("POOL", 8, y, 1.f, Color{0.55f, 0.80f, 1.f, 1.f}); y += 10;
        r.fillRect(8, y, barW, 7, Color{0.12f, 0.14f, 0.20f, 1.f});
        r.fillRect(8, y, barW * (float(live) / kPool), 7, Color{0.35f, 0.85f, 0.55f, 1.f});
        r.drawRectOutline(8, y, barW, 7, Color{0.3f, 0.36f, 0.46f, 1.f}, 1.f);
        char pool[64];
        std::snprintf(pool, sizeof pool, "%d / %d live", live, kPool);
        r.drawText(pool, 8, y + 10, 1.f, Color{0.55f, 0.62f, 0.75f, 1.f});

        r.drawText(continuous_ ? "E: continuous (follows the cursor)"
                               : "E: burst mode — SPACE to explode",
                   8, H_ - 20, 1.f, Color{0.55f, 0.62f, 0.75f, 1.f});
        r.drawText("particles are Entities: same integrator, same drag, same spin",
                   8, H_ - 10, 1.f, Color{0.42f, 0.48f, 0.60f, 1.f});
    }

    const char* status() const override {
        std::snprintf(buf_, sizeof buf_, "%s  %s  live %d/%d",
                      continuous_ ? "continuous" : "burst",
                      textured_ ? "textured" : "solid quads", em_.liveCount(), kPool);
        return buf_;
    }
    const char* keys() const override {
        return "SPACE  burst at the cursor\nE      burst / continuous\nF4     textured / solid quads\n1-8    gravity, spread, spin, drag";
    }

private:
    static constexpr int kPool = 400;

    // Push the current knob values into the emitter. Called whenever one
    // changes, so the next particle emitted uses them.
    void configure() {
        em_.position = {mouse_.x, mouse_.y};
        em_.area = {0, 0};
        em_.minSpeed = {-spread_, -spread_};
        em_.maxSpeed = { spread_,  spread_ * 0.4f};
        em_.minRotation = -spin_; em_.maxRotation = spin_;
        em_.gravity = gravity_;
        em_.drag = {drag_, drag_};
        em_.delay = 0.01f;
        em_.particleSize = {3, 3};
        em_.color = Color{0.95f, 0.75f, 0.35f, 1.f};
        em_.texture = textured_ ? dotTex_ : 0;
        em_.spriteSize = {7, 7};
        em_.frameCols = 1; em_.frameRows = 1;
    }
    void burst() {
        em_.position = {mouse_.x, mouse_.y};
        em_.start(true, 120);
    }
    float row(otacon::IRenderer& r, float y, const char* label, float v, float max) const {
        r.drawText(label, 8, y, 1.f, Color{0.62f, 0.68f, 0.80f, 1.f});
        const float bx = 78, bw = 80;
        r.fillRect(bx, y, bw, 5, Color{0.12f, 0.14f, 0.20f, 1.f});
        r.fillRect(bx, y, bw * (max > 0 ? v / max : 0.f), 5, Color{0.45f, 0.72f, 1.f, 1.f});
        char num[24]; std::snprintf(num, sizeof num, "%.0f", v);
        r.drawText(num, bx + bw + 5, y, 1.f, Color{0.55f, 0.62f, 0.75f, 1.f});
        return y + 10;
    }

    otacon::IRenderer* r_ = nullptr;
    otacon::Emitter em_;
    otacon::Camera  cam_;
    otacon::TextureHandle dotTex_ = 0;
    otacon::Vec2f mouse_{320, 200};
    float W_ = 640, H_ = 400;
    float gravity_ = 320, spread_ = 160, spin_ = 220, drag_ = 0;
    bool textured_ = true, continuous_ = false;
    mutable char buf_[96]{};
};

} // namespace

Sample* makeParticles() { return new Particles(); }

} // namespace samples
