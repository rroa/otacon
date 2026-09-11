/*
===========================================================================

OTACON ENGINE
scene/SpatialGrid.hpp - uniform-grid broad phase

collideWithGroup tests every pair, which is O(n^2). That is honest at a few dozen
bodies and quietly ruinous at a few hundred: 30 bodies is 435 tests, 300 is
44,850.

A uniform grid fixes it the cheap way. Bucket each body by the cells its AABB
covers, then only test bodies that share a cell. It is not the most sophisticated
broad phase - a BVH handles wildly varying sizes better - but it is O(1) to
insert, needs no tree to rebuild, and for a 2D game whose objects are mostly one
of a few sizes it is hard to beat.

Pick `cellSize` at roughly the size of a typical body. Too small and one body
spans many cells; too large and every body lands in the same one, which is the
O(n^2) you started with.

===========================================================================
*/
#pragma once
#include "scene/Entity.hpp"
#include <cstddef>
#include <unordered_map>
#include <vector>

namespace otacon {

class SpatialGrid {
public:
    explicit SpatialGrid(float cellSize = 64.f) : cell_(cellSize > 1.f ? cellSize : 1.f) {}

    void setCellSize(float s) { cell_ = s > 1.f ? s : 1.f; }
    float cellSize() const { return cell_; }

    void clear() { buckets_.clear(); }

    /*
    ==================
    insert

    A body goes into every cell its AABB touches, so a large one is found from
    any of them. Rebuilding from scratch each frame is usually cheaper than
    tracking movement between cells, and it cannot go stale.
    ==================
    */
    void insert(Entity* e) {
        if (!e || !e->exists) return;
        int x0, y0, x1, y1;
        bounds(*e, x0, y0, x1, y1);
        for (int y = y0; y <= y1; ++y)
            for (int x = x0; x <= x1; ++x)
                buckets_[key(x, y)].push_back(e);
    }
    void rebuild(const std::vector<Entity*>& all) {
        clear();
        for (Entity* e : all) insert(e);
    }

    /*
    ==================
    query

    Everything sharing a cell with `e`, deduplicated. `out` is cleared first, and
    `e` itself is never returned.
    ==================
    */
    void query(const Entity& e, std::vector<Entity*>& out) const {
        out.clear();
        int x0, y0, x1, y1;
        bounds(e, x0, y0, x1, y1);
        for (int y = y0; y <= y1; ++y) {
            for (int x = x0; x <= x1; ++x) {
                auto it = buckets_.find(key(x, y));
                if (it == buckets_.end()) continue;
                for (Entity* other : it->second) {
                    if (other == &e) continue;
                    // A body spanning several shared cells would otherwise be
                    // returned once per cell.
                    bool seen = false;
                    for (Entity* got : out) if (got == other) { seen = true; break; }
                    if (!seen) out.push_back(other);
                }
            }
        }
    }

    std::size_t bucketCount() const { return buckets_.size(); }
    // The worst bucket's occupancy: if this approaches the body count, the cell
    // size is wrong and the grid is buying nothing.
    std::size_t largestBucket() const {
        std::size_t n = 0;
        for (const auto& kv : buckets_) if (kv.second.size() > n) n = kv.second.size();
        return n;
    }

private:
    static std::uint64_t key(int x, int y) {
        return (std::uint64_t(std::uint32_t(x)) << 32) | std::uint32_t(y);
    }
    void bounds(const Entity& e, int& x0, int& y0, int& x1, int& y1) const {
        const float ex = toFloat(e.pos.x), ey = toFloat(e.pos.y);
        const float ew = toFloat(e.size.x), eh = toFloat(e.size.y);
        x0 = int(std::floor(ex / cell_));        y0 = int(std::floor(ey / cell_));
        x1 = int(std::floor((ex + ew) / cell_)); y1 = int(std::floor((ey + eh) / cell_));
    }

    float cell_ = 64.f;
    std::unordered_map<std::uint64_t, std::vector<Entity*>> buckets_;
};

} // namespace otacon
