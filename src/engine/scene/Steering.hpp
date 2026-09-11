/*
===========================================================================

OTACON ENGINE
scene/Steering.hpp - flocking and steering behaviours

The counterpart to PathFinder. A* is global and deliberate: it searches the whole
graph before anything moves. Steering is local and emergent: each agent looks
only at its immediate neighbours, and the group behaviour is a side effect that
no agent knows about.

Reynolds' three rules are the whole of flocking:

    separation   steer away from neighbours that are too close
    alignment    match the average heading of neighbours
    cohesion     steer toward the average position of neighbours

Zero any one weight and you can see exactly which part of "flocking" it was:
without separation they clump into a point, without alignment there is no shared
heading, without cohesion the flock disperses and never re-forms.

The neighbour search is the honest O(n^2) pair loop. For flocks in the hundreds
that is fine; past that, hand it a SpatialGrid, which is what that class is for.

===========================================================================
*/
#pragma once
#include "core/math/Vector.hpp"
#include <cmath>
#include <cstddef>
#include <vector>

namespace otacon {

struct Boid {
    float x = 0, y = 0, vx = 0, vy = 0;
    float speed() const { return std::sqrt(vx * vx + vy * vy); }
    Vec2f heading() const {
        const float s = speed();
        return s > 1e-4f ? Vec2f{vx / s, vy / s} : Vec2f{1.f, 0.f};
    }
};

struct FlockParams {
    float neighbourRadius = 34.f;
    float separateRadius  = 15.f;
    float separationWeight = 1.6f;
    float alignmentWeight  = 1.0f;
    float cohesionWeight   = 0.9f;
    // Separation is weighted by 1/d^2, which is a large number at close range;
    // this scales it back into the same magnitude as the other two.
    float separationScale  = 900.f;
    float cohesionScale    = 1.1f;
    float maxSpeed = 128.f;
    float minSpeed = 52.f;
};

class Flock {
public:
    std::vector<Boid> boids;
    FlockParams       params;

    void clear() { boids.clear(); }
    void add(float x, float y, float vx, float vy) { boids.push_back({x, y, vx, vy}); }

    /*
    ==================
    step

    One frame. Forces are accumulated from the previous positions and written
    into a copy, so every agent sees the same instant - updating in place would
    make a boid's behaviour depend on its index in the array.
    ==================
    */
    void step(float dt, Vec2f* attractor = nullptr, float attractWeight = 0.12f) {
        if (boids.empty()) return;
        const FlockParams& P = params;
        const float R2 = P.neighbourRadius * P.neighbourRadius;
        const float S2 = P.separateRadius * P.separateRadius;

        scratch_ = boids;
        for (std::size_t i = 0; i < boids.size(); ++i) {
            const Boid& b = boids[i];
            float sx = 0, sy = 0, ax = 0, ay = 0, cx = 0, cy = 0;
            int nNear = 0, nSep = 0;

            for (std::size_t j = 0; j < boids.size(); ++j) {
                if (i == j) continue;
                const float dx = boids[j].x - b.x, dy = boids[j].y - b.y;
                const float d2 = dx * dx + dy * dy;
                if (d2 > R2) continue;
                ++nNear;
                ax += boids[j].vx; ay += boids[j].vy;
                cx += boids[j].x;  cy += boids[j].y;
                if (d2 < S2 && d2 > 1e-4f) { sx -= dx / d2; sy -= dy / d2; ++nSep; }
            }

            float fx = 0, fy = 0;
            if (nSep) { fx += sx * P.separationScale * P.separationWeight;
                        fy += sy * P.separationScale * P.separationWeight; }
            if (nNear) {
                fx += (ax / float(nNear) - b.vx) * P.alignmentWeight;
                fy += (ay / float(nNear) - b.vy) * P.alignmentWeight;
                fx += (cx / float(nNear) - b.x) * P.cohesionScale * P.cohesionWeight;
                fy += (cy / float(nNear) - b.y) * P.cohesionScale * P.cohesionWeight;
            }
            if (attractor) {
                fx += (attractor->x - b.x) * attractWeight;
                fy += (attractor->y - b.y) * attractWeight;
            }

            Boid& n = scratch_[i];
            n.vx += fx * dt; n.vy += fy * dt;
            clampSpeed(n);
            n.x += n.vx * dt; n.y += n.vy * dt;
        }
        boids.swap(scratch_);
    }

    // Wrap every agent into a rect, so a flock never leaves the play area.
    void wrap(float minX, float minY, float maxX, float maxY) {
        for (Boid& b : boids) {
            if (b.x < minX) b.x = maxX;
            if (b.x > maxX) b.x = minX;
            if (b.y < minY) b.y = maxY;
            if (b.y > maxY) b.y = minY;
        }
    }
    int countNeighbours(std::size_t i) const {
        if (i >= boids.size()) return 0;
        const float R2 = params.neighbourRadius * params.neighbourRadius;
        int n = 0;
        for (std::size_t j = 0; j < boids.size(); ++j) {
            if (i == j) continue;
            const float dx = boids[j].x - boids[i].x, dy = boids[j].y - boids[i].y;
            if (dx * dx + dy * dy <= R2) ++n;
        }
        return n;
    }

private:
    void clampSpeed(Boid& b) const {
        const float s = b.speed();
        if (s > params.maxSpeed) { b.vx = b.vx / s * params.maxSpeed; b.vy = b.vy / s * params.maxSpeed; }
        // A minimum keeps a becalmed agent from stalling into a dot, which reads
        // as a bug even though it is a valid solution to the rules.
        else if (s < params.minSpeed && s > 1e-3f) { b.vx = b.vx / s * params.minSpeed; b.vy = b.vy / s * params.minSpeed; }
    }
    std::vector<Boid> scratch_;
};

/*
=============================================================================

                            SINGLE-AGENT STEERING

=============================================================================
*/

// The classic primitives, returning a desired-velocity delta to add to a body.
namespace steer {

inline Vec2f seek(Vec2f pos, Vec2f vel, Vec2f target, float maxSpeed) {
    float dx = target.x - pos.x, dy = target.y - pos.y;
    const float d = std::sqrt(dx * dx + dy * dy);
    if (d < 1e-4f) return {0.f, 0.f};
    return {dx / d * maxSpeed - vel.x, dy / d * maxSpeed - vel.y};
}
inline Vec2f flee(Vec2f pos, Vec2f vel, Vec2f threat, float maxSpeed) {
    const Vec2f s = seek(pos, vel, threat, maxSpeed);
    return {-s.x, -s.y};
}
/*
==================
arrive

seek, but easing down inside `slowRadius` instead of overshooting and orbiting
the target the way a plain seek does.
==================
*/
inline Vec2f arrive(Vec2f pos, Vec2f vel, Vec2f target, float maxSpeed, float slowRadius) {
    float dx = target.x - pos.x, dy = target.y - pos.y;
    const float d = std::sqrt(dx * dx + dy * dy);
    if (d < 1e-4f) return {-vel.x, -vel.y};
    const float want = (d < slowRadius && slowRadius > 1e-4f) ? maxSpeed * (d / slowRadius) : maxSpeed;
    return {dx / d * want - vel.x, dy / d * want - vel.y};
}

} // namespace steer
} // namespace otacon
