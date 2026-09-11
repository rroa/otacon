/*
===========================================================================

OTACON ENGINE
scene/Raycast.hpp - ray queries against boxes and tilemaps

Collision.cpp answers "these two bodies overlap, push them apart". This answers
the other question a game asks constantly: "what is the first thing along this
line, and where exactly did I hit it?"

That question is behind more than it looks like: line of sight for an enemy, a
hitscan weapon, a ground probe under a character, mouse picking against world
geometry, and the "is the jump I am about to make survivable" test an AI needs.

Two implementations, because boxes and grids want different algorithms:

  * against an AABB, the slab method. Treat the box as an intersection of two
    slabs, clip the ray's t-interval against each, and if anything survives you
    have both the entry point and the face that was hit.

  * against a TileMap, a DDA walk. Stepping cell to cell along the ray visits
    every cell it touches, in order, without testing any it does not - which is
    what makes a ray across a 128x64 map cost the length of the ray rather than
    the size of the map.

===========================================================================
*/
#pragma once
#include "core/math/Rect.hpp"
#include "core/math/Vector.hpp"
#include "scene/Entity.hpp"
#include "scene/TileMap.hpp"
#include <cmath>
#include <cstdint>
#include <vector>

namespace otacon {

/*
==================
RayHit

`t` is the fraction along the ray, so the caller can compare hits from different
queries without re-deriving distances. `normal` is the face that was struck,
which is what a slide or a bounce needs and what a bare distance cannot give you.
==================
*/
struct RayHit {
    bool    hit = false;
    float   t = 0.f;              // 0..1 along the ray
    Vec2f   point{0, 0};
    Vec2f   normal{0, 0};
    Entity* entity = nullptr;     // set by the entity queries
    int     tileX = 0, tileY = 0; // set by the tilemap query
    std::uint8_t tile = 0;

    explicit operator bool() const { return hit; }
};

namespace ray {

/*
==================
vsRect

The slab method. For each axis, the ray enters the slab at tNear and leaves at
tFar; the box is hit when the latest entry precedes the earliest exit.

A ray exactly parallel to an axis makes 1/dir infinite, which is deliberate and
correct here: the infinities compare the right way and the branch falls out,
which is why this needs no special case for axis-aligned rays.
==================
*/
inline RayHit vsRect(Vec2f origin, Vec2f dir, const Rect& box) {
    RayHit h;
    const float bx = toFloat(box.x), by = toFloat(box.y);
    const float bw = toFloat(box.w), bh = toFloat(box.h);

    const float invX = 1.f / dir.x;      // may be +/-inf; that is fine
    const float invY = 1.f / dir.y;

    float t1 = (bx - origin.x) * invX;
    float t2 = (bx + bw - origin.x) * invX;
    float t3 = (by - origin.y) * invY;
    float t4 = (by + bh - origin.y) * invY;

    if (t1 > t2) { const float s = t1; t1 = t2; t2 = s; }
    if (t3 > t4) { const float s = t3; t3 = t4; t4 = s; }

    const float tNear = t1 > t3 ? t1 : t3;
    const float tFar  = t2 < t4 ? t2 : t4;

    // tFar < 0 means the box is entirely behind the origin; tNear > 1 means it
    // is beyond the ray's end.
    if (tFar < 0.f || tNear > tFar || tNear > 1.f) return h;

    h.hit = true;
    h.t = tNear < 0.f ? 0.f : tNear;     // starting inside the box counts as t=0
    h.point = {origin.x + dir.x * h.t, origin.y + dir.y * h.t};
    // Whichever slab was entered last is the face that was struck.
    if (t1 > t3) h.normal = {invX < 0.f ? 1.f : -1.f, 0.f};
    else         h.normal = {0.f, invY < 0.f ? 1.f : -1.f};
    return h;
}

inline RayHit vsEntity(Vec2f origin, Vec2f dir, Entity& e) {
    RayHit h = vsRect(origin, dir, Rect(e.pos.x, e.pos.y, e.size.x, e.size.y));
    if (h.hit) h.entity = &e;
    return h;
}

/*
==================
vsGroup

The nearest hit among many. Returns the closest rather than the first found,
because "first in the array" is not a meaningful answer to a spatial question.
==================
*/
inline RayHit vsGroup(Vec2f origin, Vec2f dir, const std::vector<Entity*>& group,
                      const Entity* ignore = nullptr) {
    RayHit best;
    best.t = 1e30f;
    for (Entity* e : group) {
        if (!e || e == ignore || !e->exists || !e->solid) continue;
        const RayHit h = vsEntity(origin, dir, *e);
        if (h.hit && h.t < best.t) best = h;
    }
    if (!best.hit) best = RayHit{};
    return best;
}

/*
==================
vsTileMap

A DDA walk, in the Amanatides & Woo form. Step to whichever axis boundary is
nearer, so every cell the ray touches is visited exactly once and in order.

`maxCells` bounds the walk. A ray that is nearly parallel to an axis can touch a
great many cells, and a runaway loop in a line-of-sight test is a hang rather
than a wrong answer.
==================
*/
inline RayHit vsTileMap(Vec2f origin, Vec2f dir, const TileMap& map, float tileSize,
                        int maxCells = 4096) {
    RayHit h;
    if (tileSize <= 0.f) return h;

    int cx = int(std::floor(origin.x / tileSize));
    int cy = int(std::floor(origin.y / tileSize));

    // Starting inside a wall is a hit at t=0, with no useful normal.
    if (map.solid(cx, cy)) {
        h.hit = true; h.t = 0.f; h.point = origin;
        h.tileX = cx; h.tileY = cy; h.tile = map.at(cx, cy);
        return h;
    }

    const int stepX = dir.x > 0.f ? 1 : (dir.x < 0.f ? -1 : 0);
    const int stepY = dir.y > 0.f ? 1 : (dir.y < 0.f ? -1 : 0);

    const float invX = dir.x != 0.f ? std::fabs(tileSize / dir.x) : 1e30f;
    const float invY = dir.y != 0.f ? std::fabs(tileSize / dir.y) : 1e30f;

    // Distance along the ray to the first boundary on each axis.
    float nextX = 1e30f, nextY = 1e30f;
    if (stepX > 0)      nextX = ((cx + 1) * tileSize - origin.x) / dir.x;
    else if (stepX < 0) nextX = (cx * tileSize - origin.x) / dir.x;
    if (stepY > 0)      nextY = ((cy + 1) * tileSize - origin.y) / dir.y;
    else if (stepY < 0) nextY = (cy * tileSize - origin.y) / dir.y;

    for (int i = 0; i < maxCells; ++i) {
        float t;
        Vec2f normal;
        if (nextX < nextY) { t = nextX; cx += stepX; nextX += invX; normal = {float(-stepX), 0.f}; }
        else               { t = nextY; cy += stepY; nextY += invY; normal = {0.f, float(-stepY)}; }

        if (t > 1.f) break;                       // past the end of the ray
        if (stepX == 0 && stepY == 0) break;      // a zero-length direction

        if (map.solid(cx, cy)) {
            h.hit = true; h.t = t;
            h.point = {origin.x + dir.x * t, origin.y + dir.y * t};
            h.normal = normal;
            h.tileX = cx; h.tileY = cy; h.tile = map.at(cx, cy);
            return h;
        }
    }
    return h;
}

/*
==================
lineOfSight

The question a ray is most often standing in for. Note it takes two points
rather than a direction: "can A see B" is not the same query as "what is along
this heading", and conflating them is how off-by-one bugs in AI get written.
==================
*/
inline bool lineOfSight(Vec2f from, Vec2f to, const TileMap& map, float tileSize) {
    return !vsTileMap(from, {to.x - from.x, to.y - from.y}, map, tileSize).hit;
}

} // namespace ray
} // namespace otacon
