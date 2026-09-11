// TileMap.hpp — a grid of tile indices, shared by the samples that need one.
//
// Three samples want the same thing from different angles: Tilemap World draws
// one, Procedural Dungeon generates one, Pathfinding searches one. Rather than
// three private copies, they share this — which also makes the point that a
// tilemap is just an array plus an agreement about what the numbers mean.
//
// Tile 0 is always empty. `firstSolid` is the index at which tiles start
// blocking movement, so "which tiles are walls" stays data, not code.
#pragma once
#include "render/IRenderer.hpp"
#include <cstdint>
#include <vector>

namespace samples {

class TileMap {
public:
    void resize(int w, int h, std::uint8_t fill = 0) {
        w_ = w; h_ = h;
        cells_.assign(std::size_t(w) * h, fill);
    }
    int width()  const { return w_; }
    int height() const { return h_; }
    bool inBounds(int x, int y) const { return x >= 0 && y >= 0 && x < w_ && y < h_; }

    std::uint8_t at(int x, int y) const {
        return inBounds(x, y) ? cells_[std::size_t(y) * w_ + x] : 0;
    }
    void set(int x, int y, std::uint8_t t) {
        if (inBounds(x, y)) cells_[std::size_t(y) * w_ + x] = t;
    }
    // Out of bounds counts as solid: it keeps every caller's edge case honest
    // without a bounds test at each site.
    bool solid(int x, int y) const {
        if (!inBounds(x, y)) return true;
        return cells_[std::size_t(y) * w_ + x] >= firstSolid;
    }

    std::uint8_t firstSolid = 1;

    // Draw the visible window of the map. `scroll` is in pixels, `dest` is the
    // on-screen size of one tile, and only the rows/columns that intersect the
    // clip rect are touched — culling to the view is what keeps a large map
    // cheap, and is the entire reason a tilemap is drawn this way rather than
    // as one sprite per cell.
    // Returns the number of tiles actually submitted, which is the number worth
    // watching: IRenderer::drawCalls() reports the *previous* frame's total, so
    // it cannot be differenced around a call to measure one.
    int draw(otacon::IRenderer& r, otacon::TextureHandle tileset, int tileCount,
             float scrollX, float scrollY, float dest,
             float clipX, float clipY, float clipW, float clipH) const {
        if (!tileset || dest <= 0) return 0;
        int drawn = 0;
        const int x0 = int((scrollX) / dest) - 1, x1 = int((scrollX + clipW) / dest) + 1;
        const int y0 = int((scrollY) / dest) - 1, y1 = int((scrollY + clipH) / dest) + 1;
        const float uw = 1.f / float(tileCount);
        for (int ty = y0; ty <= y1; ++ty) {
            for (int tx = x0; tx <= x1; ++tx) {
                const std::uint8_t t = at(tx, ty);
                if (!t) continue;
                const float dx = clipX + tx * dest - scrollX;
                const float dy = clipY + ty * dest - scrollY;
                if (dx > clipX + clipW || dy > clipY + clipH || dx + dest < clipX || dy + dest < clipY) continue;
                const float u0 = float(t - 1) * uw;
                r.drawImage(tileset, dx, dy, dest, dest, u0, 0.f, u0 + uw, 1.f);
                ++drawn;
            }
        }
        return drawn;
    }

private:
    std::vector<std::uint8_t> cells_;
    int w_ = 0, h_ = 0;
};

} // namespace samples
