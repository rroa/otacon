// Entity.hpp — a physics-enabled scene body (Otacon engine, generic).
//
// This is the engine's reusable equivalent of flixel's FlxObject: an AABB with
// velocity/acceleration/drag/maxVelocity integrated by a flixel-style midpoint
// scheme, plus the swept-hull bookkeeping the collision
// solver needs. It is game-agnostic — any game's movers (a runner's player, a
// flapping bird) derive from it. Rendering-wise an Entity is a solid colored
// rectangle (the "geometry-first" representation), or a textured sprite when
// `texture` is set — the Scene picks per entity, which is what lets a game fade
// from boxes to art without touching the simulation.
#pragma once
#include "core/math/Vector.hpp"
#include "core/math/Rect.hpp"
#include "render/RenderTypes.hpp"

namespace otacon {

// flixel's per-axis velocity integrator (FlxU computeVelocity).
Real computeVelocity(Real velocity, Real acceleration, Real drag, Real max, Real dt);

class Entity {
public:
    virtual ~Entity() = default;

    // ---- transform ----
    Vec2  pos{};                       // top-left in world units
    Vec2  size{};                      // width, height
    Vec2f offset{0, 0};                // sprite draw offset vs. hitbox
    Vec2f scrollFactor{1, 1};          // parallax factor (1 = locked to world)

    // ---- physics ----
    Vec2 velocity{};
    Vec2 acceleration{};
    Vec2 drag{};
    Vec2 maxVelocity{R(10000), R(10000)};
    Real angle = R(0);             // degrees (for spinning particles)
    Real angularVelocity = R(0);   // degrees / second

    // ---- flags ----
    bool exists = true, active = true, visible = true;
    bool solid = true, fixed = false, moves = true, dead = false, onFloor = false;
    bool collideLeft = true, collideRight = true, collideTop = true, collideBottom = true;

    // ---- collision scratch (swept hulls) ----
    Rect colHullX, colHullY;
    Vec2 colVector{};

    // ---- render ----
    Color color{1, 1, 1, 1};
    bool  renderable = true;
    // Optional texture: when non-zero the Scene draws this entity as a textured
    // sprite (sub-rect uv0..uv1, tinted by color) instead of a solid rect.
    TextureHandle texture = 0;
    Vec2f uv0{0, 0}, uv1{1, 1};

    Real right()  const { return pos.x + size.x; }
    Real bottom() const { return pos.y + size.y; }

    void refreshHulls();
    void updateMotion(Real dt);
    virtual void update(Real dt) { updateMotion(dt); }

    // Collision response callbacks (overridable, mirror flixel hitX).
    virtual void preCollide(Entity&) {}
    virtual void hitLeft(Entity&, Real v)   { if (!fixed) velocity.x = v; }
    virtual void hitRight(Entity& c, Real v){ hitLeft(c, v); }
    virtual void hitTop(Entity&, Real v)    { if (!fixed) velocity.y = v; }
    virtual void hitBottom(Entity&, Real v) { onFloor = true; if (!fixed) velocity.y = v; }
};

} // namespace otacon
