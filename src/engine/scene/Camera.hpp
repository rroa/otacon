// Camera.hpp — 2D scrolling camera (faithful flixel follow + parallax).
#pragma once
#include "scene/Entity.hpp"

namespace otacon {

class Camera {
public:
    Vec2 scroll{};            // world scroll offset (added to entity positions)

    void setViewport(int w, int h) { width_ = w; height_ = h; stopShake(); }

    void follow(Entity* target, Real lerp);
    void followAdjust(Real leadX, Real leadY) { lead_ = {leadX, leadY}; }
    void followBounds(Real minX, Real minY, Real maxX, Real maxY);
    void setPaused(bool p) { paused_ = p; }
    void doFollow(Real dt);

    // ---- Screen quake (faithful FlxQuake) ----
    // Adds a decaying random per-frame offset to the whole screen. `intensity`
    // is a fraction of the viewport (flixel's startup value is ~0.0065), `axis`
    // restricts the shake (the original shakes vertically only: {0,1}).
    void setShakeConfig(bool enabled, Real intensity, Real duration) {
        shakeEnabled_ = enabled; shakeIntensity_ = intensity; shakeDuration_ = duration;
    }
    void setShakeAxis(Vec2f axis) { shakeAxis_ = axis; }
    void triggerShake() { if (shakeEnabled_) shakeTimer_ = shakeDuration_; }
    void stopShake() { shakeTimer_ = R(0); shakeOffset_ = {0, 0}; }
    bool shaking() const { return shakeTimer_ > R(0); }

    // flixel getScreenXY: floor(world + eps) + floor(scroll * scrollFactor).
    Vec2f screen(const Entity& e) const;
    Vec2f screenPoint(Vec2 world, Vec2f scrollFactor) const;
    // Inverse of screenPoint, for cursor picking / dragging.
    Vec2 worldFromScreen(Vec2f screenPt, Vec2f scrollFactor) const;

    int width()  const { return width_; }
    int height() const { return height_; }

private:
    Entity* target_ = nullptr;
    Real    lerp_ = R(1);
    Vec2    lead_{};
    Vec2    target_pt_{};
    Vec2    min_{}, max_{};
    bool    hasMin_ = false, hasMax_ = false, paused_ = false;
    int     width_ = 480, height_ = 320;

    // quake state
    bool          shakeEnabled_ = false;
    Real          shakeIntensity_ = R(0.0065f), shakeDuration_ = R(2.5f), shakeTimer_ = R(0);
    Vec2f         shakeAxis_{0, 1};
    Vec2f         shakeOffset_{0, 0};
    std::uint32_t shakeRng_ = 0x9E3779B9u;   // tiny LCG for the offset (cosmetic)
    float         shakeRand();               // [-1,1)
};

} // namespace otacon
