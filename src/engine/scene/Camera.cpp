#include "scene/Camera.hpp"

namespace otacon {

void Camera::follow(Entity* target, Real lerp) {
    target_ = target;
    lerp_ = lerp;
    paused_ = false;
    // Snap immediately (flixel sets scroll = target on follow()).
    target_pt_.x = R(float(width_ / 2)) - target->pos.x - R(float(toInt(target->size.x) / 2));
    target_pt_.y = R(float(height_ / 2)) - target->pos.y - R(float(toInt(target->size.y) / 2));
    scroll = target_pt_;
}

void Camera::followBounds(Real minX, Real minY, Real maxX, Real maxY) {
    min_ = {-minX, -minY};
    max_ = {-maxX + R(float(width_)), -maxY + R(float(height_))};
    if (max_.x > min_.x) max_.x = min_.x;
    if (max_.y > min_.y) max_.y = min_.y;
    hasMin_ = hasMax_ = true;
}

float Camera::shakeRand() {
    shakeRng_ = shakeRng_ * 1664525u + 1013904223u;
    return (float(shakeRng_ >> 8) / float(1u << 24)) * 2.0f - 1.0f;   // [-1, 1)
}

void Camera::doFollow(Real dt) {
    // Advance the quake even if the follow is paused/targetless.
    if (shakeTimer_ > R(0)) {
        shakeTimer_ -= dt;
        if (shakeTimer_ <= R(0)) {
            stopShake();
        } else {
            float amp = toFloat(shakeIntensity_);
            shakeOffset_.x = shakeRand() * amp * float(width_)  * shakeAxis_.x;
            shakeOffset_.y = shakeRand() * amp * float(height_) * shakeAxis_.y;
        }
    }
    if (paused_ || !target_) return;
    target_pt_.x = R(float(width_ / 2)) - target_->pos.x - R(float(toInt(target_->size.x) / 2));
    target_pt_.y = R(float(height_ / 2)) - target_->pos.y - R(float(toInt(target_->size.y) / 2));
    target_pt_.x -= target_->velocity.x * lead_.x;
    target_pt_.y -= target_->velocity.y * lead_.y;
    scroll.x += (target_pt_.x - scroll.x) * lerp_ * dt;
    scroll.y += (target_pt_.y - scroll.y) * lerp_ * dt;
    if (hasMin_) { if (scroll.x > min_.x) scroll.x = min_.x; if (scroll.y > min_.y) scroll.y = min_.y; }
    if (hasMax_) { if (scroll.x < max_.x) scroll.x = max_.x; if (scroll.y < max_.y) scroll.y = max_.y; }
}

Vec2f Camera::screenPoint(Vec2 world, Vec2f sf) const {
    float wx = toFloat(sfloor(world.x + roundingError()));
    float wy = toFloat(sfloor(world.y + roundingError()));
    float sx = std::floor(toFloat(scroll.x) * sf.x);
    float sy = std::floor(toFloat(scroll.y) * sf.y);
    // The quake offset shifts the whole screen uniformly (camera shake).
    return {wx + sx + shakeOffset_.x, wy + sy + shakeOffset_.y};
}

Vec2f Camera::screen(const Entity& e) const {
    return screenPoint(e.pos, e.scrollFactor);
}

Vec2 Camera::worldFromScreen(Vec2f s, Vec2f sf) const {
    float wx = s.x - std::floor(toFloat(scroll.x) * sf.x);
    float wy = s.y - std::floor(toFloat(scroll.y) * sf.y);
    return {R(wx), R(wy)};
}

} // namespace otacon
