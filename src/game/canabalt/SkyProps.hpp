// SkyProps.hpp — the animated "easter-egg" props that live in the parallax sky.
//
// The original PlayState seeds the distant skyline with a few moving extras: a
// jet that periodically streaks across, and giant walking mechs that idle, march
// and fire. They are pure background colour — they never touch gameplay — so they
// live here as self-contained scene nodes, each driving its own animation clock
// and its own little random stream (kept separate from the level generator's RNG
// so they never disturb which buildings get generated).
#pragma once
#include "scene/Node.hpp"
#include "scene/Emitter.hpp"
#include "render/IRenderer.hpp"
#include <cstdint>

namespace otacon { class Camera; }

namespace canabalt {

// A jet that parks off-screen, then every ~10-30s warps to the far right and
// flies left at high speed, rattling the camera as it passes (Jet.m).
class JetNode final : public otacon::Node {
public:
    void load(otacon::IRenderer* r, const char* assetDir);
    void destroy(otacon::IRenderer* r);
    void useCamera(otacon::Camera* c) { quakeCam_ = c; }   // the jet shakes the screen
    bool consumeFired() { bool f = fired_; fired_ = false; return f; }   // for the flyby SFX

    void update(otacon::Real dt, const otacon::Camera& cam) override;
    void render(otacon::IRenderer& r, const otacon::Camera& cam) const override;

private:
    float rand01() { rng_ = rng_ * 1664525u + 1013904223u; return float(rng_ >> 8) / float(1u << 24); }

    otacon::TextureHandle tex_ = 0;
    int   w_ = 0, h_ = 0;
    float x_ = -500.f, y_ = 0.f;
    float vx_ = -1200.f;
    float timer_ = 0.f, limit_ = 14.f;
    bool  fired_ = false;                  // warped in this... consumed for the SFX
    otacon::Camera* quakeCam_ = nullptr;
    std::uint32_t rng_ = 0x1357AceFu;
};

// A giant walking mech (Walker.m). It idles, occasionally marches a little, and
// occasionally fires its cannon — venting a steady plume of smoke from the muzzle.
// When it scrolls off the left it warps back ahead, re-randomising its facing.
class WalkerNode final : public otacon::Node {
public:
    void load(otacon::IRenderer* r, const char* assetDir);
    void destroy(otacon::IRenderer* r);
    void place(float startX, int seed);   // initial world x + per-walker random seed
    void setSmokeTexture(otacon::TextureHandle t) { smoke_.texture = t; }  // reveal toggle

    void update(otacon::Real dt, const otacon::Camera& cam) override;
    void render(otacon::IRenderer& r, const otacon::Camera& cam) const override;

private:
    enum class Anim { Idle, Walk, Fire };
    float rand01() { rng_ = rng_ * 1664525u + 1013904223u; return float(rng_ >> 8) / float(1u << 24); }
    void  play(Anim a);
    void  fireCannon();
    int   frame() const;

    otacon::TextureHandle tex_ = 0;
    int   w_ = 120, h_ = 80;
    float x_ = -500.f, y_ = 44.f;
    float vx_ = 0.f;
    int   facing_ = 0;                 // 0 = facing right, 1 = facing left
    Anim  anim_ = Anim::Idle;
    float frameTime_ = 0.f;            // animation clock for the current clip
    bool  firing_ = false;
    float walkTimer_ = 0.f, idleTimer_ = 0.f;
    otacon::Emitter smoke_;            // cannon / exhaust plume
    std::uint32_t rng_ = 0x2468BdF0u;
};

// A tall steel girder that sweeps across the foreground (BG.m with random=YES).
// It scrolls much faster than the camera (a near-foreground parallax), and each
// time it leaves the left edge it leaps far ahead and picks a new speed, so a
// beam streaks past now and then rather than on a fixed beat.
class GirderNode final : public otacon::Node {
public:
    void load(otacon::IRenderer* r, const char* assetDir);
    void destroy(otacon::IRenderer* r);

    void update(otacon::Real dt, const otacon::Camera& cam) override;
    void render(otacon::IRenderer& r, const otacon::Camera& cam) const override;

private:
    float rand01() { rng_ = rng_ * 1664525u + 1013904223u; return float(rng_ >> 8) / float(1u << 24); }

    otacon::TextureHandle tex_ = 0;
    int   w_ = 0, h_ = 0;
    float x_ = 3000.f;
    float sfx_ = 3.f;                  // horizontal parallax factor (re-rolled on wrap)
    std::uint32_t rng_ = 0x0F1E2D3Cu;
};

} // namespace canabalt
