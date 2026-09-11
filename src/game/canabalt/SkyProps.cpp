#include "canabalt/SkyProps.hpp"
#include "asset/Resources.hpp"
#include "scene/Camera.hpp"
#include "asset/Image.hpp"
#include <algorithm>
#include <string>

using namespace otacon;

namespace canabalt {

namespace { constexpr float kViewW = 480.f; }   // logical screen width (FlxG.width)

// === Jet =====================================================================

void JetNode::load(Resources* res, const char* assetDir) {
    if (tex_ || !res) return;
    const std::string path = std::string(assetDir) + "/images/raw/jet.png";
    tex_ = res->texture(path);
    res->size(path, w_, h_);   // drawn at native size
}

void JetNode::destroy(IRenderer* r) {
    // Nothing to free: these textures are owned by the engine's Resources
    // cache, which releases them once, after the game shuts down and while
    // the renderer is still alive. Freeing them here too would double-free.
    (void)r;
}

void JetNode::update(Real dt, const Camera&) {
    if (!visible) return;
    float fdt = toFloat(dt);
    timer_ += fdt;
    if (timer_ > limit_) {
        // Warp to the far right and dive in at a fresh altitude, then rattle the
        // screen as the shockwave hits.
        x_ = 960.f;
        y_ = -20.f + rand01() * 120.f;
        if (quakeCam_) quakeCam_->triggerShake();
        fired_ = true;                       // the game plays the flyby SFX
        timer_ = 0.f;
        limit_ = 10.f + rand01() * 20.f;
    }
    if (x_ < -float(w_)) return;     // parked off-screen until the next pass
    x_ += vx_ * fdt;
}

void JetNode::render(IRenderer& r, const Camera& cam) const {
    if (!tex_) return;
    if (x_ < -float(w_)) return;
    Vec2f s = cam.screenPoint({R(x_), R(y_)}, {0.f, 0.3f});
    r.drawImage(tex_, s.x, s.y, float(w_), float(h_));
}

// === Walker ==================================================================

void WalkerNode::load(Resources* res, const char* assetDir) {
    if (tex_ || !res) return;
    const std::string path = std::string(assetDir) + "/images/raw/walker.png";
    tex_ = res->texture(path);
    // A small, slow exhaust/cannon plume that drifts upward (no gravity).
    smoke_.particleSize = {16, 16};
    smoke_.color = Color{0.72f, 0.72f, 0.76f, 0.22f};
    smoke_.minSpeed = {-3, -20}; smoke_.maxSpeed = {3, -10};
    smoke_.gravity = 0; smoke_.minRotation = -30; smoke_.maxRotation = 30;
    smoke_.delay = 0.3f; smoke_.scrollFactor = {0.1f, 0.15f};
    smoke_.spriteSize = {32, 32}; smoke_.frameCols = 4;   // smoke.png layout when textured
    smoke_.init(20); smoke_.seed(0x9A71u);
}

void WalkerNode::destroy(IRenderer* r) {
    // Nothing to free: these textures are owned by the engine's Resources
    // cache, which releases them once, after the game shuts down and while
    // the renderer is still alive. Freeing them here too would double-free.
    (void)r;
}

void WalkerNode::place(float startX, int seed) {
    x_ = startX;
    y_ = 40.f + (float((seed * 2654435761u) >> 8 & 0xFFu) / 255.f) * 10.f;
    // Mix the caller's seed into this walker's own stream, so two walkers
    // placed with different seeds diverge rather than marching in step.
    rng_.seed(rng_.currentSeed() ^ (std::uint32_t(seed) * 0x9E3779B9u + 1u));
}

void WalkerNode::play(Anim a) { anim_ = a; frameTime_ = 0.f; }

void WalkerNode::fireCannon() {
    float muzzleX = x_ + (facing_ == 0 ? float(w_ - 22) : 10.f);
    smoke_.position = {muzzleX, y_ + float(h_)};
    smoke_.start(false);     // continuous puffing from the muzzle
}

int WalkerNode::frame() const {
    int step = int(frameTime_ * 8.f);              // 8 fps animation
    switch (anim_) {
        case Anim::Walk: return step % 6;          // frames 0-5
        case Anim::Fire: return 6 + std::min(5, step);  // frames 6-11, once
        default:         return 0;                 // idle
    }
}

void WalkerNode::update(Real dt, const Camera& cam) {
    if (!visible) return;
    float fdt = toFloat(dt);
    frameTime_ += fdt;

    // Idle -> occasionally march or fire; while firing, hold until the clip ends.
    if (walkTimer_ > 0.f) {
        walkTimer_ -= fdt;
        if (walkTimer_ <= 0.f) { play(Anim::Fire); firing_ = true; vx_ = 0.f; fireCannon(); }
    } else if (firing_) {
        if (int(frameTime_ * 8.f) >= 6) {          // fire clip finished
            firing_ = false; idleTimer_ = 1.f + rand01() * 2.f; play(Anim::Idle);
        }
    } else if (idleTimer_ > 0.f) {
        idleTimer_ -= fdt;
        if (idleTimer_ <= 0.f) {
            if (rand01() < 0.5f) {
                walkTimer_ = 2.f + rand01() * 4.f; play(Anim::Walk);
                vx_ = facing_ == 0 ? 40.f : -40.f;
            } else {
                play(Anim::Fire); firing_ = true; fireCannon();
            }
        }
    }

    x_ += vx_ * fdt;

    // Wrapped off the left? Warp back ahead of the camera, re-rolling the facing.
    Vec2f s = cam.screenPoint({R(x_), R(y_)}, {0.1f, 0.15f});
    if (s.x + float(w_) * 2.f < 0.f) {
        walkTimer_ = rand01() * 2.f;
        facing_ = rand01() > 0.5f ? 0 : 1;
        x_ += kViewW + float(w_) * 2.f + rand01() * kViewW;
    }

    // Keep the plume hanging at the muzzle, then advance it.
    float muzzleX = x_ + (facing_ == 0 ? float(w_ - 22) : 10.f);
    smoke_.position = {muzzleX, y_ + float(h_)};
    smoke_.update(dt, cam);
}

void WalkerNode::render(IRenderer& r, const Camera& cam) const {
    if (!tex_) return;
    Vec2f s = cam.screenPoint({R(x_), R(y_)}, {0.1f, 0.15f});
    int f = frame();
    float fu = float(w_) / 1440.f;                 // 12 frames of 120px in the strip
    float u0 = float(f) * fu, u1 = u0 + fu;
    if (facing_ == 1) std::swap(u0, u1);           // face left -> mirror horizontally
    r.drawImage(tex_, s.x, s.y, float(w_), float(h_), u0, 0.f, u1, 1.f);
    smoke_.render(r, cam);
}

// === Girder ==================================================================

void GirderNode::load(Resources* res, const char* assetDir) {
    if (tex_ || !res) return;
    const std::string path = std::string(assetDir) + "/images/raw/girder-tall.png";
    tex_ = res->texture(path);
    res->size(path, w_, h_);   // drawn at native size
}

void GirderNode::destroy(IRenderer* r) {
    // Nothing to free: these textures are owned by the engine's Resources
    // cache, which releases them once, after the game shuts down and while
    // the renderer is still alive. Freeing them here too would double-free.
    (void)r;
}

void GirderNode::update(Real, const Camera& cam) {
    if (!visible) return;
    Vec2f s = cam.screenPoint({R(x_), R(0)}, {sfx_, 0.f});
    if (s.x + float(w_) < 0.f) {       // swept past the left edge -> leap far ahead
        x_ += kViewW * 10.f + rand01() * kViewW * 10.f;
        sfx_ = 2.f + rand01() * 3.f;
    }
}

void GirderNode::render(IRenderer& r, const Camera& cam) const {
    if (!tex_) return;
    Vec2f s = cam.screenPoint({R(x_), R(0)}, {sfx_, 0.f});
    r.drawImage(tex_, s.x, s.y, float(w_), float(h_));
}

} // namespace canabalt
