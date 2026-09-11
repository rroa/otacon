// Bird.hpp — the player's kinematic state.
//
// Deliberately tiny and pure: just position + vertical velocity and a single
// integration step. Each build drives `vy` differently (constant in Build 1,
// gravity + flap impulses later) but reuses this same state, so the model the
// player controls is identical across every scene.
#pragma once
#include "core/math/Scalar.hpp"

namespace flappy {

struct Bird {
    float x  = 0;   // left edge, logical px (pinned during play)
    float y  = 0;   // top edge, logical px
    float vy = 0;   // vertical velocity, px/second (+down)

    // Semi-implicit Euler: advance position by the current velocity.
    void integrate(otacon::Real dt) { y += vy * otacon::toFloat(dt); }
};

} // namespace flappy
