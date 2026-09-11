/*
===========================================================================

OTACON ENGINE
scene/Verlet.hpp - position-based dynamics: ropes, chains and cloth

The AABB solver in Collision.cpp handles bodies. This handles the things bodies
hang from: anything made of points held at distances from each other.

Verlet integration never stores velocity. It stores where a point *was*, and
velocity is implied by the gap:

    next = current + (current - previous) + acceleration * dt^2

That is what makes constraints cheap. To enforce one you simply move the points;
the motion takes care of itself, with no impulse to compute and no velocity to
fix up afterwards.

The catch is that satisfying one constraint breaks its neighbour, so you do not
solve - you *relax*, sweeping the list repeatedly and getting closer each pass.
`iterations` is therefore the stiffness knob, and the only one: at 1 a rope is
elastic, at 20 it is a chain. Nothing else changes.

===========================================================================
*/
#pragma once
#include "core/math/Vector.hpp"
#include <cmath>
#include <cstddef>
#include <vector>

namespace otacon {

/*
==================
VerletPoint

`prev` is the previous position, not a velocity. Pinning a point is just
refusing to move it, which is why an anchor needs no special case anywhere else.
==================
*/
struct VerletPoint {
    float x = 0, y = 0;
    float px = 0, py = 0;
    bool  pinned = false;

    void place(float nx, float ny) { x = px = nx; y = py = ny; }
    // Move without implying velocity - for dragging, where the grab should not
    // fling the point when released.
    void teleport(float nx, float ny) { x = px = nx; y = py = ny; }
    void nudge(float dx, float dy) { x += dx; y += dy; }
    float vx() const { return x - px; }
    float vy() const { return y - py; }
};

/*
==================
DistanceConstraint

Two points and the distance they want to be apart. `stiffness` in 0..1 scales
how much of the error one pass corrects, which is a second, softer knob than the
iteration count.
==================
*/
struct DistanceConstraint {
    int   a = 0, b = 0;
    float rest = 0.f;
    float stiffness = 1.f;
};

/*
==================
VerletBody

A rope, a bridge and a cloth are the same two arrays; only the constraint list
differs. The helpers below build the usual shapes, but nothing stops you from
wiring the links yourself.
==================
*/
class VerletBody {
public:
    std::vector<VerletPoint>        points;
    std::vector<DistanceConstraint> links;

    float gravity    = 1400.f;
    float damping    = 0.995f;   // velocity retained per step; 1 is frictionless
    int   iterations = 8;        // relaxation passes: the stiffness knob

    void clear() { points.clear(); links.clear(); }

    int add(float x, float y, bool pinned = false) {
        VerletPoint p; p.place(x, y); p.pinned = pinned;
        points.push_back(p);
        return int(points.size()) - 1;
    }
    // Rest length defaults to however far apart they currently are, which is
    // almost always what you want when building from placed points.
    void link(int a, int b, float rest = -1.f, float stiffness = 1.f) {
        if (a < 0 || b < 0 || a >= int(points.size()) || b >= int(points.size())) return;
        if (rest < 0.f) {
            const float dx = points[b].x - points[a].x, dy = points[b].y - points[a].y;
            rest = std::sqrt(dx * dx + dy * dy);
        }
        links.push_back({a, b, rest, stiffness});
    }

    /*
    ==================
    step

    One frame: integrate, then relax. The relaxation runs inside the frame rather
    than across frames because a half-relaxed body looks like a stretched one.
    ==================
    */
    void step(float dt) {
        integrate(dt);
        for (int i = 0; i < iterations; ++i) relax();
    }

    void integrate(float dt) {
        for (VerletPoint& p : points) {
            if (p.pinned) continue;
            const float vx = (p.x - p.px) * damping;
            const float vy = (p.y - p.py) * damping;
            p.px = p.x; p.py = p.y;
            p.x += vx;
            p.y += vy + gravity * dt * dt;
        }
    }

    // One relaxation pass. The correction is split between the pair, or given
    // entirely to whichever one is free.
    void relax() {
        for (const DistanceConstraint& l : links) {
            VerletPoint& a = points[std::size_t(l.a)];
            VerletPoint& b = points[std::size_t(l.b)];
            const float dx = b.x - a.x, dy = b.y - a.y;
            const float d = std::sqrt(dx * dx + dy * dy);
            if (d < 1e-6f) continue;
            const float diff = (d - l.rest) / d * 0.5f * l.stiffness;
            const float ox = dx * diff, oy = dy * diff;
            if (!a.pinned && !b.pinned) { a.x += ox; a.y += oy; b.x -= ox; b.y -= oy; }
            else if (!a.pinned)         { a.x += ox * 2.f; a.y += oy * 2.f; }
            else if (!b.pinned)         { b.x -= ox * 2.f; b.y -= oy * 2.f; }
        }
    }

    // Clamp every point below a line, with a little friction on contact so a
    // rope piles rather than skating.
    void collideFloor(float floorY, float friction = 0.4f) {
        for (VerletPoint& p : points) {
            if (p.pinned || p.y <= floorY) continue;
            p.y = floorY;
            p.px = p.x + (p.x - p.px) * friction;
        }
    }
    void collideBounds(float minX, float maxX) {
        for (VerletPoint& p : points) {
            if (p.pinned) continue;
            if (p.x < minX) { p.x = minX; p.px = p.x; }
            if (p.x > maxX) { p.x = maxX; p.px = p.x; }
        }
    }
    void addForce(float fx, float fy, float dt) {
        for (VerletPoint& p : points) {
            if (!p.pinned) { p.x += fx * dt; p.y += fy * dt; }
        }
    }

    // The worst proportional stretch across all links - how far from satisfied
    // the body is, and the number to watch when tuning `iterations`.
    float strain() const {
        float worst = 0.f;
        for (const DistanceConstraint& l : links) {
            const float dx = points[std::size_t(l.b)].x - points[std::size_t(l.a)].x;
            const float dy = points[std::size_t(l.b)].y - points[std::size_t(l.a)].y;
            const float d = std::sqrt(dx * dx + dy * dy);
            if (l.rest > 1e-6f) {
                const float e = std::fabs(d - l.rest) / l.rest;
                if (e > worst) worst = e;
            }
        }
        return worst;
    }
    int nearest(float x, float y, float maxDist = 18.f) const {
        int best = -1; float bestD = maxDist * maxDist;
        for (std::size_t i = 0; i < points.size(); ++i) {
            const float dx = points[i].x - x, dy = points[i].y - y;
            const float d = dx * dx + dy * dy;
            if (d < bestD) { bestD = d; best = int(i); }
        }
        return best;
    }

    /*
    ==================
    makeRope / makeBridge / makeCloth

    The three shapes that cover most uses. Each is only a different link list
    over the same machinery.
    ==================
    */
    void makeRope(float x, float y, int count, float segment, bool pinFirst = true) {
        clear();
        for (int i = 0; i < count; ++i) add(x + i * segment * 0.25f, y + i * segment, i == 0 && pinFirst);
        for (int i = 0; i + 1 < count; ++i) link(i, i + 1, segment);
    }
    void makeBridge(float x0, float x1, float y, int count) {
        clear();
        const float seg = count > 1 ? (x1 - x0) / float(count - 1) : 0.f;
        for (int i = 0; i < count; ++i) add(x0 + i * seg, y, i == 0 || i == count - 1);
        // Slightly shorter than the span so it hangs rather than sitting taut.
        for (int i = 0; i + 1 < count; ++i) link(i, i + 1, seg * 0.92f);
    }
    void makeCloth(float x, float y, int cols, int rows, float segment, int pinEvery = 5) {
        clear();
        for (int j = 0; j < rows; ++j)
            for (int i = 0; i < cols; ++i)
                add(x + i * segment, y + j * segment,
                    j == 0 && (i % pinEvery == 0 || i == cols - 1));
        auto idx = [&](int i, int j) { return j * cols + i; };
        for (int j = 0; j < rows; ++j)
            for (int i = 0; i < cols; ++i) {
                if (i + 1 < cols) link(idx(i, j), idx(i + 1, j), segment);
                if (j + 1 < rows) link(idx(i, j), idx(i, j + 1), segment);
            }
    }
};

} // namespace otacon
