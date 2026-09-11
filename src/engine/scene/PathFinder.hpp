/*
===========================================================================

OTACON ENGINE
scene/PathFinder.hpp - A* over a TileMap

A* is Dijkstra plus a guess. It expands whichever open node has the lowest

    f = g + h        g: cost paid to get here    h: estimate of what remains

and the heuristic is the entire algorithm. Set h to zero and this degenerates
into Dijkstra: still correct, but it explores in every direction. Keep h at or
below the true remaining cost - "admissible" - and the first path found is the
shortest. Weight h above that and the search rushes to an answer that may not be
the shortest, which is often the right trade for a game.

The search keeps its working state (the open and closed sets) so a debug view
can draw what was explored. That is not incidental: the cost of a heuristic is
invisible until you can see how much of the map it touched.

===========================================================================
*/
#pragma once
#include "scene/TileMap.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace otacon {

struct PathNode {
    float g = 1e30f, f = 1e30f;
    int   parent = -1;
    bool  open = false, closed = false;
};

class PathFinder {
public:
    enum class Heuristic {
        None,        // h = 0 -> Dijkstra: correct, explores everywhere
        Manhattan,   // admissible for 4-way movement
        Euclidean,   // admissible for 8-way
        Greedy,      // Manhattan over-weighted: fast, not always shortest
    };
    enum class Movement { Four, Eight };

    Heuristic heuristic = Heuristic::Manhattan;
    Movement  movement  = Movement::Four;
    // Diagonals cost sqrt(2); charging 1 would make a staircase look free.
    float     diagonalCost = 1.41421356f;

    static const char* heuristicName(Heuristic h) {
        switch (h) {
            case Heuristic::None:      return "zero (= Dijkstra)";
            case Heuristic::Manhattan: return "manhattan";
            case Heuristic::Euclidean: return "euclidean";
            default:                   return "manhattan x2.5 (greedy)";
        }
    }

    /*
    ==================
    search

    Returns true when the goal was reached; path() then holds it start-first.
    Returns false and an empty path when the goal is unreachable, which is a
    normal answer and not an error.
    ==================
    */
    bool search(const TileMap& map, int startX, int startY, int goalX, int goalY) {
        w_ = map.width(); h_ = map.height();
        nodes_.assign(std::size_t(w_) * std::size_t(h_), PathNode{});
        path_.clear();
        visited_ = 0;
        goalX_ = goalX; goalY_ = goalY;
        if (!map.inBounds(startX, startY) || !map.inBounds(goalX, goalY)) return false;
        if (map.solid(startX, startY) || map.solid(goalX, goalY)) return false;

        const std::size_t start = idx(startX, startY);
        nodes_[start].g = 0.f;
        nodes_[start].f = estimate(startX, startY);
        nodes_[start].open = true;
        open_.clear();
        open_.push_back(int(start));

        static const int dx8[8] = {1, -1, 0, 0,  1,  1, -1, -1};
        static const int dy8[8] = {0, 0, 1, -1,  1, -1,  1, -1};
        const int dirs = (movement == Movement::Eight) ? 8 : 4;

        while (!open_.empty()) {
            // Pop the lowest f. A binary heap is the usual structure; a linear
            // scan is kept here because it is readable and a tile grid is small.
            std::size_t best = 0;
            for (std::size_t i = 1; i < open_.size(); ++i)
                if (nodes_[open_[i]].f < nodes_[open_[best]].f) best = i;
            const int cur = open_[best];
            open_[best] = open_.back();
            open_.pop_back();

            PathNode& c = nodes_[std::size_t(cur)];
            if (c.closed) continue;
            c.open = false; c.closed = true;
            ++visited_;

            const int cx = cur % w_, cy = cur / w_;
            if (cx == goalX && cy == goalY) { rebuild(cur); return true; }

            for (int k = 0; k < dirs; ++k) {
                const int nx = cx + dx8[k], ny = cy + dy8[k];
                if (!map.inBounds(nx, ny) || map.solid(nx, ny)) continue;
                // Refuse to cut a corner between two walls: legal on a grid,
                // but it looks like clipping through the join.
                if (k >= 4 && (map.solid(cx + dx8[k], cy) || map.solid(cx, cy + dy8[k]))) continue;

                const std::size_t ni = idx(nx, ny);
                if (nodes_[ni].closed) continue;
                const float step = (k >= 4) ? diagonalCost : 1.f;
                const float ng = c.g + step;
                if (ng < nodes_[ni].g) {
                    nodes_[ni].g = ng;
                    nodes_[ni].f = ng + estimate(nx, ny);
                    nodes_[ni].parent = cur;
                    if (!nodes_[ni].open) { nodes_[ni].open = true; open_.push_back(int(ni)); }
                }
            }
        }
        return false;
    }

    // The path, start first. Cell indices; x = i % width, y = i / width.
    const std::vector<int>& path() const { return path_; }
    int   pathLength() const { return int(path_.size()); }
    int   visited() const { return visited_; }        // how much was explored
    int   width() const { return w_; }

    // Working state, so a debug view can show what the heuristic cost.
    bool wasExplored(int x, int y) const {
        return inRange(x, y) && nodes_[idx(x, y)].closed;
    }
    bool isFrontier(int x, int y) const {
        return inRange(x, y) && nodes_[idx(x, y)].open;
    }

private:
    std::size_t idx(int x, int y) const { return std::size_t(y) * std::size_t(w_) + std::size_t(x); }
    bool inRange(int x, int y) const { return x >= 0 && y >= 0 && x < w_ && y < h_ && !nodes_.empty(); }

    float estimate(int x, int y) const {
        const float dx = float(std::abs(x - goalX_)), dy = float(std::abs(y - goalY_));
        switch (heuristic) {
            case Heuristic::None:      return 0.f;
            case Heuristic::Manhattan: return dx + dy;
            case Heuristic::Euclidean: return std::sqrt(dx * dx + dy * dy);
            default:                   return (dx + dy) * 2.5f;
        }
    }
    void rebuild(int goal) {
        for (int cur = goal; cur >= 0; cur = nodes_[std::size_t(cur)].parent) path_.push_back(cur);
        std::reverse(path_.begin(), path_.end());
    }

    std::vector<PathNode> nodes_;
    std::vector<int>      open_, path_;
    int w_ = 0, h_ = 0, goalX_ = 0, goalY_ = 0, visited_ = 0;
};

} // namespace otacon
