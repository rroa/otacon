// Player.hpp — Canabalt's runner. Game code: derives from the engine Entity and
// reproduces Player.m's physics exactly (auto-run with speed-dependent
// acceleration, variable-height jump, wall-death, fall-death, stumble).
#pragma once
#include "scene/Entity.hpp"

namespace canabalt {

using otacon::Entity;
using otacon::Real;
using otacon::R;

class Player final : public Entity {
public:
    Player();
    void reset(Real startX, Real startY);

    // Set each frame from input before update().
    bool touching = false;   // jump button / touch held
    bool pause    = false;

    void update(Real dt) override;
    void hitBottom(Entity& c, Real v) override;
    void hitLeft(Entity& c, Real v) override;

    bool dead_ = false;
    bool stumble = false;
    bool crashed() const { return acceleration.x <= R(0); }   // hit a wall
    Real jumpLimit() const { return jumpLimit_; }             // used by the level generator
    const char* epitaph = "fall";

private:
    void updateRunSpeed();   // speed-dependent forward acceleration ramp
    void updateJump(Real dt);// variable-height jump (hold to go higher)

    Real jump_ = R(0);       // jump hold timer (-1 = not jumping)
    Real jumpLimit_ = R(0);  // max hold time for the current speed
    Real my_ = R(0);         // time spent at terminal fall speed (=> stumble)
};

} // namespace canabalt
