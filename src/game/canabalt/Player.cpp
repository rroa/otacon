#include "canabalt/Player.hpp"
#include <algorithm>

namespace canabalt {
namespace {

// All values are straight from the original Player.m — named here so the
// physics is readable and tweakable in one place.

// Hitbox + spawn -------------------------------------------------------------
constexpr float kHitW   = 16.f;      // collision box (the 30x30 sprite has a 6,12 offset)
constexpr float kHitH   = 18.f;
constexpr float kSpawnY = 80.f - 14.f;

// Run motion -----------------------------------------------------------------
constexpr float kStartSpeed   = 125.f;   // initial velocity.x
constexpr float kGravity      = 1200.f;  // acceleration.y
constexpr float kDragX        = 640.f;   // only applies when accel.x == 0 (after a wall)
constexpr float kMaxSpeedX    = 1000.f;
constexpr float kMaxFallSpeed = 360.f;
constexpr float kWallFallSpeed = 1000.f; // maxVelocity.y once you've smacked a wall

// Speed-dependent acceleration: the slower you are, the harder you accelerate,
// so you ramp up fast then taper. Each band is "below this speed -> this accel".
struct SpeedBand { float below, accel; };
constexpr SpeedBand kAccelRamp[] = {
    {100.f, 60.f}, {250.f, 36.f}, {400.f, 24.f}, {600.f, 12.f},
};
constexpr float kAccelTop = 4.f;         // at or above the last band

// Variable-height jump -------------------------------------------------------
constexpr float kJumpLimitDivisor = 2.5f;   // jumpLimit = vx / (maxVx * 2.5)
constexpr float kJumpLimitCap     = 0.35f;  // ...capped at 0.35 s of hold
constexpr float kShortJumpTime    = 0.08f;  // a tap shorter than this is a weak hop
constexpr float kShortJumpScale   = 0.65f;

// Death / stumble ------------------------------------------------------------
constexpr float kDeathY         = 484.f;    // fall off the bottom -> dead
constexpr float kStumbleFallTime = 0.16f;   // time at terminal fall speed -> hard landing

} // namespace

Player::Player() {
    size = {R(kHitW), R(kHitH)};
    offset = {0, 0};                      // geometry milestone draws the hitbox itself
    drag = {R(kDragX), R(0)};
    acceleration = {R(1), R(kGravity)};   // x is overwritten by the ramp each frame
    maxVelocity = {R(kMaxSpeedX), R(kMaxFallSpeed)};
    velocity = {R(kStartSpeed), R(0)};
    color = otacon::Color::rgb(0xf0f0f0);
    reset(R(0), R(kSpawnY));
}

void Player::reset(Real startX, Real startY) {
    pos = {startX, startY};
    velocity = {R(kStartSpeed), R(0)};
    acceleration = {R(1), R(kGravity)};
    maxVelocity = {R(kMaxSpeedX), R(kMaxFallSpeed)};
    jump_ = R(0); jumpLimit_ = R(0); my_ = R(0);
    dead_ = dead = false; stumble = false; onFloor = false;
    epitaph = "fall";
}

void Player::update(Real dt) {
    if (pos.y > R(kDeathY)) { dead_ = dead = true; return; }   // fell off the bottom

    // Wall death: once acceleration.x is zeroed we just keep falling.
    if (acceleration.x <= R(0)) { updateMotion(dt); return; }

    updateRunSpeed();        // forward acceleration for this frame
    updateJump(dt);          // upward thrust if the button is held
    updateMotion(dt);        // integrate velocity -> position (engine)

    if (velocity.y == maxVelocity.y) my_ += dt;   // accruing terminal-fall time
}

// The slower you are, the harder you accelerate, so you ramp up fast then taper.
void Player::updateRunSpeed() {
    if (velocity.x < R(0)) { velocity.x = R(0); return; }   // never run backwards
    acceleration.x = R(kAccelTop);
    for (const SpeedBand& band : kAccelRamp)
        if (velocity.x < R(band.below)) { acceleration.x = R(band.accel); break; }
}

// Variable-height jump: while the button is held (and we're within the speed-
// scaled time window) keep applying upward velocity; a short tap launches softer
// than a full hold. `jump_` is the hold timer; -1 means "not jumping".
void Player::updateJump(Real dt) {
    jumpLimit_ = std::min(velocity.x / (maxVelocity.x * R(kJumpLimitDivisor)), R(kJumpLimitCap));

    if (jump_ >= R(0) && touching && !pause) {
        jump_ += dt;                                   // accumulate hold time
        if (jump_ > jumpLimit_) jump_ = R(-1);         // past the window: end the jump
    } else {
        jump_ = R(-1);                                 // released / blocked: no thrust
    }

    if (jump_ > R(0))
        velocity.y = (jump_ < R(kShortJumpTime)) ? -maxVelocity.y * R(kShortJumpScale)
                                                 : -maxVelocity.y;
}

void Player::hitBottom(Entity& c, Real v) {
    if (my_ > R(kStumbleFallTime)) stumble = true;  // hard landing
    if (!touching) jump_ = R(0);                    // re-arm jump on a grounded frame
    my_ = R(0);
    Entity::hitBottom(c, v);
}

void Player::hitLeft(Entity& c, Real v) {
    // Smacked into a wall: stop dead, fall, record the cause of death.
    acceleration.x = R(0);
    velocity.x = R(0);
    maxVelocity.y = R(kWallFallSpeed);
    epitaph = "hit";
    Entity::hitLeft(c, v);
}

} // namespace canabalt
