#include "flappy/scene/GravityScene.hpp"
#include "platform/Window.hpp"   // InputFrame, Action
#include "render/IRenderer.hpp"
#include <algorithm>
#include <cstdio>

namespace flappy {

void GravityScene::enter() {
    bird_.x  = cfg::kBirdX;
    bird_.y  = cfg::kBirdStartY;
    bird_.vy = 0;
    gravity_ = cfg::kGravity;
}

void GravityScene::handleInput(const otacon::InputFrame& in) {
    // A flap is an edge, not a level: each press resets vertical velocity to a
    // fixed upward kick. Holding does nothing extra — you must tap to climb.
    if (in.isPressed(otacon::Action::Jump)) bird_.vy = cfg::kFlapImpulse;

    // Tune the gravity force live to feel the parameter (F1 weaker, F2 stronger).
    if (in.isPressed(otacon::Action::Aux1)) gravity_ -= cfg::kGravityStep;
    if (in.isPressed(otacon::Action::Aux2)) gravity_ += cfg::kGravityStep;
    gravity_ = std::clamp(gravity_, cfg::kGravityMin, cfg::kGravityMax);
}

void GravityScene::update(otacon::Real dt) {
    // Semi-implicit Euler: accelerate, then move by the new velocity. No bounds
    // checks — falling (or flying) off-screen is the lesson here.
    bird_.vy += gravity_ * otacon::toFloat(dt);
    bird_.integrate(dt);
}

void GravityScene::render(otacon::IRenderer& r) const {
    r.fillRect(bird_.x, bird_.y, cfg::kBirdSize, cfg::kBirdSize, cfg::kBird);
}

const char* GravityScene::status() const {
    std::snprintf(status_, sizeof status_, "GRAVITY %.0f (F1/F2)  SPACE=flap", gravity_);
    return status_;
}

} // namespace flappy
