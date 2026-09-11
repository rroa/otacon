/*
===========================================================================

OTACON ENGINE
scene/Iso.hpp - isometric grid: the coordinate conversions everything else needs

Every bug in an isometric game is ultimately a coordinate bug, so this file is
deliberately small and deliberately exact. Three spaces, and it is worth naming
them before any code touches them:

  TILE    integer grid indices. (0,0), (1,0), (0,1)... what the map array is
          indexed by, and what pathfinding and fog of war work in.

  WORLD   continuous tile coordinates. (3.5, 2.25) is a unit standing between
          tiles. Units move in this space; only rendering leaves it.

  SCREEN  pixels, before the camera's scroll is applied. This is the space
          IRenderer draws in.

The projection is the classic 2:1 diamond:

        screenX = (wx - wy) * tileW/2
        screenY = (wx + wy) * tileH/2  -  elevation * heightStep

which is a rotation and a squash, and the squash is why tileH is usually half of
tileW. Read it as: moving +1 in x walks you down-right, +1 in y walks you
down-left, and both move you down the screen -- which is what makes (wx + wy)
the depth axis.

              (0,0)
              /   \             +x goes down-right
         (1,0)     (0,1)        +y goes down-left
              \   /             both go DOWN the screen
              (1,1)

The inverse is the same maths solved the other way, and it is exact: toWorld and
toScreen round-trip. That is asserted in the tests rather than hoped for,
because a picking bug and a rendering bug look identical on screen and a failing
round-trip tells you instantly which half is wrong.

Elevation lifts a tile straight up the screen and does NOT change its depth --
a raised tile is still at the same place on the ground plane. Getting that
backwards makes a hill sort in front of the things standing on it.

===========================================================================
*/
#pragma once
#include "core/math/Vector.hpp"
#include <cmath>

namespace otacon {

/*
==================
IsoGrid

The shape of one diamond, and the only thing that knows it. Everything else --
tilemap, picking, sorting, the minimap -- asks this.

`tileW` is the full width of a tile's diamond and `tileH` its full height; 2:1
(64x32) is the usual choice but nothing here requires it. `heightStep` is how
far one elevation level lifts a tile, conventionally tileH/2.
==================
*/
struct IsoGrid {
    float tileW = 64.f;
    float tileH = 32.f;
    float heightStep = 16.f;

    float halfW() const { return tileW * 0.5f; }
    float halfH() const { return tileH * 0.5f; }

    /*
    ==================
    toScreen

    World (continuous tile coords) -> screen pixels, at the CENTRE of the tile's
    diamond. Centre rather than a corner because that is what you place a sprite
    or a unit against; a corner would make every caller re-derive the offset.
    ==================
    */
    Vec2f toScreen(float wx, float wy, float elevation = 0.f) const {
        return {(wx - wy) * halfW(),
                (wx + wy) * halfH() - elevation * heightStep};
    }
    Vec2f toScreen(Vec2f world, float elevation = 0.f) const {
        return toScreen(world.x, world.y, elevation);
    }

    /*
    ==================
    toWorld

    The exact inverse. `elevation` must be the one the point was projected with,
    which is why picking against a raised tile has to know its height first --
    there is no way to recover it from a screen position alone, and pretending
    otherwise is where "my clicks are off by one on hills" comes from.
    ==================
    */
    Vec2f toWorld(Vec2f screen, float elevation = 0.f) const {
        const float sy = screen.y + elevation * heightStep;
        const float a = screen.x / halfW();      // wx - wy
        const float b = sy / halfH();            // wx + wy
        return {(a + b) * 0.5f, (b - a) * 0.5f};
    }

    /*
    ==================
    toTile

    World -> integer tile index, flooring. Floor and not truncate: truncation
    rounds toward zero, so every tile in the negative quadrant would be off by
    one and the map would appear to shift as you crossed the origin.
    ==================
    */
    static void toTile(Vec2f world, int& tx, int& ty) {
        tx = int(std::floor(world.x));
        ty = int(std::floor(world.y));
    }
    void screenToTile(Vec2f screen, int& tx, int& ty, float elevation = 0.f) const {
        toTile(toWorld(screen, elevation), tx, ty);
    }

    /*
    ==================
    depth

    The painter's-order key: draw ascending and things nearer the camera land on
    top. The depth axis is (wx + wy), because both axes move down the screen.

    Elevation is deliberately NOT part of the key. A raised tile occupies the
    same ground position; lifting it in the sort would make a hill draw in front
    of the units standing on it. `bias` is the tie-breaker for things sharing a
    tile -- a unit over floor decoration -- and is what you reach for instead.
    ==================
    */
    static float depth(float wx, float wy, float bias = 0.f) {
        return wx + wy + bias;
    }
    static float depth(Vec2f world, float bias = 0.f) { return depth(world.x, world.y, bias); }

    /*
    ==================
    footprintDepth

    A multi-tile building sorts by its FAR corner, not its origin. A 3x3 keep
    anchored at (10,10) occupies out to (12,12), and sorting it at its origin
    would let a unit standing at (11,11) -- visually in front of the near wall --
    draw behind the whole building.
    ==================
    */
    static float footprintDepth(float originX, float originY, int tilesX, int tilesY) {
        return depth(originX + float(tilesX) - 1.f, originY + float(tilesY) - 1.f);
    }

    /*
    ==================
    diamondCorners

    The four corners of a tile, screen space, clockwise from the top. For the
    selection outline, the debug grid, and any hit test that needs the real
    diamond rather than its bounding box.
    ==================
    */
    void diamondCorners(float wx, float wy, float elevation, Vec2f out[4]) const {
        const Vec2f c = toScreen(wx + 0.5f, wy + 0.5f, elevation);   // tile centre
        out[0] = {c.x,             c.y - halfH()};   // top
        out[1] = {c.x + halfW(),   c.y};             // right
        out[2] = {c.x,             c.y + halfH()};   // bottom
        out[3] = {c.x - halfW(),   c.y};             // left
    }

    /*
    ==================
    containsPoint

    Is a screen point inside this tile's diamond? The rhombus test is the sum of
    normalised distances from the centre: |dx|/halfW + |dy|/halfH <= 1. Cheaper
    and more honest than four edge cross-products, and it is the test picking
    should use -- a bounding box would claim the corners, which belong to the
    four neighbouring tiles.
    ==================
    */
    bool containsPoint(float wx, float wy, float elevation, Vec2f screen) const {
        const Vec2f c = toScreen(wx + 0.5f, wy + 0.5f, elevation);
        const float dx = std::fabs(screen.x - c.x) / halfW();
        const float dy = std::fabs(screen.y - c.y) / halfH();
        return dx + dy <= 1.f;
    }

    /*
    ==================
    visibleTileRange

    Which tiles can a screen rect possibly touch? The four corners of the rect
    map to four points in world space, and their bounding box is the answer --
    inflated by one, because a tile's sprite is usually taller than its diamond
    and a building anchored just off-screen still draws into it.

    This is what keeps a large map affordable: a 256x256 world is 65,536 tiles
    and a screen holds a few hundred.
    ==================
    */
    void visibleTileRange(Vec2f topLeft, Vec2f bottomRight, float maxElevation,
                          int& x0, int& y0, int& x1, int& y1, int pad = 2) const {
        const Vec2f c[4] = {
            toWorld({topLeft.x,     topLeft.y},     0.f),
            toWorld({bottomRight.x, topLeft.y},     0.f),
            toWorld({topLeft.x,     bottomRight.y}, maxElevation),
            toWorld({bottomRight.x, bottomRight.y}, maxElevation),
        };
        float minX = c[0].x, maxX = c[0].x, minY = c[0].y, maxY = c[0].y;
        for (int i = 1; i < 4; ++i) {
            minX = c[i].x < minX ? c[i].x : minX;
            maxX = c[i].x > maxX ? c[i].x : maxX;
            minY = c[i].y < minY ? c[i].y : minY;
            maxY = c[i].y > maxY ? c[i].y : maxY;
        }
        x0 = int(std::floor(minX)) - pad;
        y0 = int(std::floor(minY)) - pad;
        x1 = int(std::ceil(maxX)) + pad;
        y1 = int(std::ceil(maxY)) + pad;
    }
};

} // namespace otacon
