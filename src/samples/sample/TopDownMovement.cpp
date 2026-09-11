// TopDownMovement.cpp — sample 11: the diagonal bug, and camera lag.
//
// Two things go wrong in almost every first top-down controller, and both are
// visible here side by side.
//
// The first is the diagonal. If you add a full-speed step on X and a full-speed
// step on Y, the diagonal is sqrt(2) = 1.41x faster than the cardinals, so the
// quickest route anywhere is a zig-zag. The fix is to normalise the input vector
// before scaling it by speed — one line, and the speed readout proves it.
//
// The second is the camera. Pinning it exactly to the player is correct and
// feels terrible: there is no sense of motion and every twitch shakes the
// screen. A lerp toward the target, plus a lead in the direction of travel,
// costs two lines and is most of what "good camera" means in 2D.
#include "Sample.hpp"
#include "common/Art.hpp"
#include "common/Rng.hpp"
#include "scene/Camera.hpp"
#include "scene/Entity.hpp"
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
using otacon::R;

constexpr float kWorldW = 1600.f, kWorldH = 1100.f;
constexpr int   kTrail = 260;

class TopDownMovement final : public Sample {
public:
    void init(SampleContext& ctx) override {
        r_ = ctx.renderer;
        W_ = float(ctx.logicalW); H_ = float(ctx.logicalH);
        cam_.setViewport(ctx.logicalW, int(H_ - layout::kTop));
        int fw = 0, fh = 0, fc = 0;
        hero_ = r_->createTexture(art::heroSheet(fw, fh, fc));
        frameW_ = fw; frameH_ = fh; frameCount_ = fc;

        Rng rng(0x70D0);
        props_.clear();
        for (int i = 0; i < 90; ++i)
            props_.push_back({rng.range(0, kWorldW), rng.range(0, kWorldH),
                              rng.range(10.f, 34.f), rng.rangeI(0, 3)});
    }
    void shutdown() override { if (hero_ && r_) { r_->destroyTexture(hero_); hero_ = 0; } }

    void enter() override {
        px_ = kWorldW * 0.5f; py_ = kWorldH * 0.5f; vx_ = vy_ = 0;
        normalize_ = true; smoothCam_ = true; lead_ = true; instant_ = false;
        camX_ = px_; camY_ = py_; trailLen_ = 0; trailHead_ = 0; anim_ = 0;
    }

    void handleInput(const otacon::InputFrame& in) override {
        if (in.isPressed(otacon::Action::Jump)) normalize_ = !normalize_;
        if (in.isPressed(otacon::Action::Aux6)) instant_ = !instant_;       // E
        if (in.isPressed(otacon::Action::Aux4)) smoothCam_ = !smoothCam_;   // F4
        if (in.isPressed(otacon::Action::Aux5)) lead_ = !lead_;             // F6
        // The pointer is the stick: direction from the player to the cursor,
        // with a dead zone in the middle so a resting cursor means "stop".
        const float sx = in.mouseNx * W_, sy = in.mouseNy * H_;
        const float cx = W_ * 0.5f, cy = layout::kTop + (H_ - layout::kTop) * 0.5f;
        inX_ = sx - cx; inY_ = sy - cy;
        const float len = std::sqrt(inX_ * inX_ + inY_ * inY_);
        if (len < kDeadZone) { inX_ = inY_ = 0.f; }
        else {
            // Clamp to a unit stick: past the ring, it is full deflection.
            const float k = std::min(1.f, (len - kDeadZone) / (kStickR - kDeadZone));
            inX_ = inX_ / len * k; inY_ = inY_ / len * k;
        }
    }

    void update(otacon::Real dt) override {
        const float d = otacon::toFloat(dt);
        if (d <= 0.f || d > 0.1f) return;

        // The whole lesson, in four lines.
        float ax = inX_, ay = inY_;
        const float mag = std::sqrt(ax * ax + ay * ay);
        if (normalize_ && mag > 1.f) { ax /= mag; ay /= mag; }   // <- the fix
        const float wantX = ax * kSpeed, wantY = ay * kSpeed;

        if (instant_) { vx_ = wantX; vy_ = wantY; }
        else {
            const float k = 1.f - std::exp(-kAccel * d);          // frame-rate independent ease
            vx_ += (wantX - vx_) * k;
            vy_ += (wantY - vy_) * k;
        }
        px_ = std::min(kWorldW, std::max(0.f, px_ + vx_ * d));
        py_ = std::min(kWorldH, std::max(0.f, py_ + vy_ * d));

        // Camera: lerp toward the player, offset ahead of where they are going.
        float tx = px_, ty = py_;
        if (lead_) { tx += vx_ * kLead; ty += vy_ * kLead; }
        if (smoothCam_) {
            const float k = 1.f - std::exp(-kCamLerp * d);
            camX_ += (tx - camX_) * k;
            camY_ += (ty - camY_) * k;
        } else { camX_ = tx; camY_ = ty; }

        cam_.scroll = {R(camX_ - W_ * 0.5f), R(camY_ - (H_ - layout::kTop) * 0.5f)};

        if (trailLen_ < kTrail) ++trailLen_;
        trailHead_ = (trailHead_ + 1) % kTrail;
        trail_[trailHead_][0] = px_; trail_[trailHead_][1] = py_;

        speed_ = std::sqrt(vx_ * vx_ + vy_ * vy_);
        anim_ += speed_ * d * 0.06f;
    }

    void render(otacon::IRenderer& r, const otacon::DebugRuntime&) override {
        const float top = layout::kTop;
        const float ox = camX_ - W_ * 0.5f, oy = camY_ - (H_ - top) * 0.5f;
        auto sx = [&](float wx) { return wx - ox; };
        auto sy = [&](float wy) { return wy - oy + top; };

        // A world grid, so camera motion is legible.
        for (float gx = std::floor(ox / 80) * 80; gx < ox + W_; gx += 80)
            r.drawLine(sx(gx), top, sx(gx), H_, Color{1, 1, 1, 0.05f}, 1.f);
        for (float gy = std::floor(oy / 80) * 80; gy < oy + H_; gy += 80)
            r.drawLine(0, sy(gy), W_, sy(gy), Color{1, 1, 1, 0.05f}, 1.f);

        for (const Prop& p : props_) {
            const float x = sx(p.x), y = sy(p.y);
            if (x < -40 || x > W_ + 40 || y < top - 40 || y > H_ + 40) continue;
            r.fillRect(x, y, p.s, p.s, kPropColor[p.kind]);
        }

        // The path travelled — a zig-zag here is the diagonal bug, drawn.
        for (int i = 1; i < trailLen_; ++i) {
            const int a = (trailHead_ - i + kTrail) % kTrail;
            const int b = (trailHead_ - i + 1 + kTrail) % kTrail;
            const float fade = 1.f - float(i) / kTrail;
            r.drawLine(sx(trail_[a][0]), sy(trail_[a][1]), sx(trail_[b][0]), sy(trail_[b][1]),
                       Color{1.f, 0.85f, 0.35f, fade * 0.6f}, 1.f);
        }

        // The player.
        const int frame = speed_ > 8.f ? (int(anim_) % 4) : 4;
        const float pw = frameW_ * 2.4f, ph = frameH_ * 2.4f;
        const float u0 = float(frame) / frameCount_, u1 = float(frame + 1) / frameCount_;
        r.drawImage(hero_, sx(px_) - pw * 0.5f, sy(py_) - ph * 0.5f, pw, ph, u0, 0, u1, 1);

        // The camera's target, when it is not on the player.
        if (lead_ || smoothCam_)
            r.drawRectOutline(sx(camX_) - 3, sy(camY_) - 3, 6, 6, Color{0.45f, 0.85f, 1.f, 0.8f}, 1.f);

        drawStick(r);
        drawPanel(r);
    }

    const char* status() const override {
        std::snprintf(buf_, sizeof buf_, "speed %.0f / %.0f  %s  cam %s%s",
                      speed_, kSpeed, normalize_ ? "normalized" : "RAW (diagonals faster)",
                      smoothCam_ ? "lerp" : "locked", lead_ ? "+lead" : "");
        return buf_;
    }
    const char* keys() const override {
        return "mouse  steers (dead zone in the middle)\nSPACE  normalize the input vector\nE      instant velocity vs acceleration\nF4     camera lerp on/off\nF6     camera lead on/off";
    }

private:
    struct Prop { float x, y, s; int kind; };
    static constexpr float kSpeed = 210.f, kAccel = 9.f, kCamLerp = 4.5f, kLead = 0.34f;
    static constexpr float kDeadZone = 18.f, kStickR = 120.f;
    static constexpr Color kPropColor[4] = {
        {0.24f, 0.34f, 0.28f, 1.f}, {0.30f, 0.28f, 0.22f, 1.f},
        {0.22f, 0.26f, 0.34f, 1.f}, {0.28f, 0.22f, 0.26f, 1.f},
    };

    // The stick, drawn as a ring with the live vector inside — and the unit
    // circle, so an un-normalized diagonal visibly pokes outside it.
    void drawStick(otacon::IRenderer& r) const {
        const float cx = 62, cy = H_ - 62, rad = 40;
        for (int i = 0; i < 48; ++i) {
            const float a0 = float(i) / 48 * 6.28318f, a1 = float(i + 1) / 48 * 6.28318f;
            r.drawLine(cx + std::cos(a0) * rad, cy + std::sin(a0) * rad,
                       cx + std::cos(a1) * rad, cy + std::sin(a1) * rad,
                       Color{0.35f, 0.42f, 0.55f, 1.f}, 1.f);
        }
        // The square the raw input can reach: its corners are the 1.41x problem.
        r.drawRectOutline(cx - rad, cy - rad, rad * 2, rad * 2, Color{1.f, 0.45f, 0.35f, 0.35f}, 1.f);
        r.drawLine(cx, cy, cx + inX_ * rad, cy + inY_ * rad, Color{1.f, 0.85f, 0.35f, 1.f}, 1.f);
        r.fillRect(cx + inX_ * rad - 2, cy + inY_ * rad - 2, 4, 4, Color{1.f, 0.85f, 0.35f, 1.f});
        r.drawText("STICK", cx - 12, cy - rad - 10, 1.f, Color{0.55f, 0.80f, 1.f, 1.f});
    }

    void drawPanel(otacon::IRenderer& r) const {
        const float px = 130;
        float y = H_ - 96;
        // A speed bar that pins at max only when normalization is on.
        r.drawText("SPEED", px, y, 1.f, Color{0.55f, 0.80f, 1.f, 1.f}); y += 10;
        const float bw = 150.f;
        r.fillRect(px, y, bw, 7, Color{0.12f, 0.14f, 0.20f, 1.f});
        const float f = speed_ / (kSpeed * 1.42f);
        r.fillRect(px, y, bw * std::min(1.f, f), 7,
                   speed_ > kSpeed * 1.02f ? Color{1.f, 0.45f, 0.35f, 1.f} : Color{0.45f, 0.85f, 0.55f, 1.f});
        // The mark at 1.0x: the bar should never pass it.
        r.drawLine(px + bw / 1.42f, y - 2, px + bw / 1.42f, y + 9, Color{1, 1, 1, 0.6f}, 1.f);
        y += 11;
        char t[96];
        std::snprintf(t, sizeof t, "%.0f px/s  (%.2fx of cardinal)", speed_, speed_ / kSpeed);
        r.drawText(t, px, y, 1.f, Color{0.55f, 0.62f, 0.75f, 1.f}); y += 11;
        r.drawText(normalize_ ? "normalized: diagonal == cardinal"
                              : "RAW: hold a diagonal and watch it pass the mark",
                   px, y, 1.f, normalize_ ? Color{0.45f, 0.90f, 0.60f, 1.f}
                                          : Color{1.f, 0.55f, 0.45f, 1.f});
        y += 11;
        r.drawText(instant_ ? "velocity: instant (snappy, no weight)"
                            : "velocity: eased toward the target",
                   px, y, 1.f, Color{0.55f, 0.62f, 0.75f, 1.f});
    }

    otacon::IRenderer* r_ = nullptr;
    otacon::TextureHandle hero_ = 0;
    otacon::Camera cam_;
    std::vector<Prop> props_;
    float trail_[kTrail][2]{};
    int   trailHead_ = 0, trailLen_ = 0;
    float W_ = 640, H_ = 400;
    float px_ = 0, py_ = 0, vx_ = 0, vy_ = 0, inX_ = 0, inY_ = 0;
    float camX_ = 0, camY_ = 0, speed_ = 0, anim_ = 0;
    int   frameW_ = 12, frameH_ = 16, frameCount_ = 6;
    bool  normalize_ = true, smoothCam_ = true, lead_ = true, instant_ = false;
    mutable char buf_[160]{};
};

} // namespace

Sample* makeTopDownMovement() { return new TopDownMovement(); }

} // namespace samples
