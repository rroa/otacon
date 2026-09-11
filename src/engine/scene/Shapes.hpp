/*
===========================================================================

OTACON ENGINE
scene/Shapes.hpp - circles, capsules, and the overlap tests between them

The solver in Collision.cpp is AABB-only, deliberately: axis-aligned separation
is what makes a platformer feel tight. But "does it feel tight" is a question
about the *response*, and plenty of things only need the *query* -- a blast
radius, a pickup trigger, an aggro range, a sword arc, whether two units are
touching. Forcing those through a box gives corners where there should not be
any.

So these are query shapes. Nothing here moves a body or resolves a penetration;
each function answers a geometric question and hands back enough to act on it
(overlap yes/no, or the penetration vector when you want to push).

Everything reduces to one primitive. A circle is a point with a radius, a
capsule is a *segment* with a radius, and a box is a box -- so the whole file is
really "closest point on X to Y", compared against a radius. Reading it that way
makes the capsule cases stop looking like special cases.

===========================================================================
*/
#pragma once
#include "core/math/Rect.hpp"
#include "core/math/Vector.hpp"
#include <cmath>

namespace otacon {

struct Circle {
    Vec2f centre{0, 0};
    float radius = 0.f;
};

/*
==================
Capsule

A segment with a radius: the set of points within `radius` of the line from a to
b. A zero-length capsule is a circle, which is why the capsule routines below
need no separate degenerate case.
==================
*/
struct Capsule {
    Vec2f a{0, 0}, b{0, 0};
    float radius = 0.f;
};

/*
==================
Manifold

The result of an overlap that wants a response. `normal` points from the first
shape toward the second, and `depth` is how far they interpenetrate along it, so
moving the first by -normal*depth separates them exactly.
==================
*/
struct Manifold {
    bool  hit = false;
    Vec2f normal{0, 0};
    float depth = 0.f;
    Vec2f contact{0, 0};

    explicit operator bool() const { return hit; }
};

namespace shape {

inline float lengthSq(Vec2f v) { return v.x * v.x + v.y * v.y; }
inline float length(Vec2f v) { return std::sqrt(lengthSq(v)); }
inline Vec2f normalize(Vec2f v, Vec2f fallback = {1.f, 0.f}) {
    const float l = length(v);
    return l > 1e-6f ? Vec2f{v.x / l, v.y / l} : fallback;
}

/*
==================
closestPointOnSegment

The workhorse. Project p onto the infinite line through a-b, then clamp the
parameter to [0,1] so it stays on the segment -- the clamp is the only thing
separating a segment from a line, and forgetting it is the classic capsule bug.
==================
*/
inline Vec2f closestPointOnSegment(Vec2f p, Vec2f a, Vec2f b) {
    const Vec2f ab{b.x - a.x, b.y - a.y};
    const float denom = lengthSq(ab);
    if (denom < 1e-12f) return a;                      // degenerate: a == b
    float t = ((p.x - a.x) * ab.x + (p.y - a.y) * ab.y) / denom;
    t = t < 0.f ? 0.f : (t > 1.f ? 1.f : t);
    return {a.x + ab.x * t, a.y + ab.y * t};
}

inline Vec2f closestPointInRect(Vec2f p, const Rect& r) {
    const float x0 = toFloat(r.x), y0 = toFloat(r.y);
    const float x1 = x0 + toFloat(r.w), y1 = y0 + toFloat(r.h);
    return {p.x < x0 ? x0 : (p.x > x1 ? x1 : p.x),
            p.y < y0 ? y0 : (p.y > y1 ? y1 : p.y)};
}

// The shortest distance between two segments, and the points realising it.
// Used by capsule-vs-capsule, which is exactly this test against a radius sum.
inline float segmentDistance(Vec2f p1, Vec2f q1, Vec2f p2, Vec2f q2,
                             Vec2f& c1, Vec2f& c2) {
    // Iterating the two clamped projections converges immediately for the
    // non-parallel case and is stable for the parallel one, which the closed
    // form is not.
    c1 = p1; c2 = closestPointOnSegment(c1, p2, q2);
    for (int i = 0; i < 3; ++i) {
        c1 = closestPointOnSegment(c2, p1, q1);
        c2 = closestPointOnSegment(c1, p2, q2);
    }
    return length({c2.x - c1.x, c2.y - c1.y});
}

/*
=============================================================================

                              OVERLAP QUERIES

=============================================================================
*/

inline bool overlaps(const Circle& a, const Circle& b) {
    const float r = a.radius + b.radius;
    return lengthSq({b.centre.x - a.centre.x, b.centre.y - a.centre.y}) <= r * r;
}

// Clamp the centre into the box; if the clamped point is within the radius they
// touch. This is also correct when the centre is inside the box, where the
// clamp is a no-op and the distance is zero.
inline bool overlaps(const Circle& c, const Rect& r) {
    const Vec2f p = closestPointInRect(c.centre, r);
    return lengthSq({p.x - c.centre.x, p.y - c.centre.y}) <= c.radius * c.radius;
}
inline bool overlaps(const Rect& r, const Circle& c) { return overlaps(c, r); }

inline bool overlaps(const Capsule& cap, const Circle& c) {
    const Vec2f p = closestPointOnSegment(c.centre, cap.a, cap.b);
    const float r = cap.radius + c.radius;
    return lengthSq({p.x - c.centre.x, p.y - c.centre.y}) <= r * r;
}
inline bool overlaps(const Circle& c, const Capsule& cap) { return overlaps(cap, c); }

inline bool overlaps(const Capsule& x, const Capsule& y) {
    Vec2f c1, c2;
    const float d = segmentDistance(x.a, x.b, y.a, y.b, c1, c2);
    return d <= x.radius + y.radius;
}

// Sample the segment against the box. Exact for the axis-aligned cases a 2D
// game actually uses, and close enough elsewhere that a trigger volume behaves;
// a capsule needing exact box contact wants a real SAT, which this file does
// not pretend to be.
inline bool overlaps(const Capsule& cap, const Rect& r) {
    const int kSteps = 8;
    for (int i = 0; i <= kSteps; ++i) {
        const float t = float(i) / float(kSteps);
        const Vec2f p{cap.a.x + (cap.b.x - cap.a.x) * t, cap.a.y + (cap.b.y - cap.a.y) * t};
        if (overlaps(Circle{p, cap.radius}, r)) return true;
    }
    return false;
}
inline bool overlaps(const Rect& r, const Capsule& cap) { return overlaps(cap, r); }

/*
=============================================================================

                            PENETRATION MANIFOLDS

=============================================================================
*/

/*
==================
resolve (circle vs circle)

Concentric circles have no meaningful normal, so one is chosen rather than
returning a zero vector that would silently do nothing -- a stack of exactly
coincident bodies must still come apart.
==================
*/
inline Manifold resolve(const Circle& a, const Circle& b) {
    Manifold m;
    const Vec2f d{b.centre.x - a.centre.x, b.centre.y - a.centre.y};
    const float r = a.radius + b.radius;
    const float dist2 = lengthSq(d);
    if (dist2 > r * r) return m;
    const float dist = std::sqrt(dist2);
    m.hit = true;
    m.normal = dist > 1e-6f ? Vec2f{d.x / dist, d.y / dist} : Vec2f{1.f, 0.f};
    m.depth = r - dist;
    m.contact = {a.centre.x + m.normal.x * a.radius, a.centre.y + m.normal.y * a.radius};
    return m;
}

inline Manifold resolve(const Circle& c, const Rect& r) {
    Manifold m;
    const Vec2f p = closestPointInRect(c.centre, r);
    const Vec2f d{p.x - c.centre.x, p.y - c.centre.y};
    const float dist2 = lengthSq(d);
    if (dist2 > c.radius * c.radius) return m;

    m.hit = true;
    m.contact = p;
    if (dist2 > 1e-12f) {
        const float dist = std::sqrt(dist2);
        m.normal = {d.x / dist, d.y / dist};
        m.depth = c.radius - dist;
    } else {
        // The centre is inside the box: push out through the nearest face,
        // which the clamped point cannot tell us because it is the centre.
        const float x0 = toFloat(r.x), y0 = toFloat(r.y);
        const float x1 = x0 + toFloat(r.w), y1 = y0 + toFloat(r.h);
        const float left = c.centre.x - x0, right = x1 - c.centre.x;
        const float up = c.centre.y - y0, down = y1 - c.centre.y;
        float best = left; m.normal = {-1.f, 0.f};
        if (right < best) { best = right; m.normal = {1.f, 0.f}; }
        if (up < best)    { best = up;    m.normal = {0.f, -1.f}; }
        if (down < best)  { best = down;  m.normal = {0.f, 1.f}; }
        m.depth = best + c.radius;
    }
    return m;
}

inline Manifold resolve(const Capsule& cap, const Circle& c) {
    const Vec2f p = closestPointOnSegment(c.centre, cap.a, cap.b);
    return resolve(Circle{p, cap.radius}, c);
}

} // namespace shape
} // namespace otacon
