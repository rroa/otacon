// PathfindingBoids.cpp — sample 12: two ways to decide where to go.
//
// Pathfinding and flocking are the two halves of movement AI, and they are
// opposites worth seeing together.
//
// A* is global and deliberate: it searches the whole graph before anything
// moves, and its behaviour is entirely decided by the heuristic. Set the
// heuristic to zero and it degenerates into Dijkstra — correct, but it explores
// everywhere; the sample draws the open and closed sets so that cost is not
// abstract. Over-weight the heuristic and it rushes to a cheap answer that may
// not be the shortest. That trade is the whole algorithm.
//
// Boids is local and emergent: no agent knows the flock exists. Three rules over
// a small neighbourhood — separation, alignment, cohesion — and the flock is a
// side effect. Zero a weight and watch which part of "flocking" it was.
#include "Sample.hpp"
#include "core/math/Random.hpp"
#include "scene/TileMap.hpp"
#include "scene/PathFinder.hpp"
#include "scene/Steering.hpp"
#include "render/IRenderer.hpp"
#include "platform/Window.hpp"
#include "core/debug/Debug.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

namespace samples {
namespace {

using otacon::Color;

// ---------------------------------------------------------------------------
// The A* search and the flock both live in the engine now (scene/PathFinder.hpp,
// scene/Steering.hpp). What stays here is the presentation: the grid this sample
// searches, and the drawing that makes the open and closed sets visible.
constexpr int kGridW = 54, kGridH = 30;

using otacon::PathFinder;
using otacon::Boid;

class PathfindingBoids final : public Sample {
public:
    void init(SampleContext& ctx) override { W_ = float(ctx.logicalW); H_ = float(ctx.logicalH); }

    void enter() override {
        mode_ = 0; heuristic_ = 1; seed_ = 3;
        sepW_ = 1.6f; aliW_ = 1.0f; cohW_ = 0.9f; showNeighbours_ = false;
        buildGrid();
        solve();
        buildFlock();
    }

    void handleInput(const otacon::InputFrame& in) override {
        mx_ = in.mouseNx * W_; my_ = in.mouseNy * H_;
        if (in.isPressed(otacon::Action::Jump)) mode_ = 1 - mode_;

        if (mode_ == 0) {
            if (in.isPressed(otacon::Action::Aux6)) { heuristic_ = (heuristic_ + 1) % 3; solve(); }
            if (in.isPressed(otacon::Action::Aux4)) { ++seed_; buildGrid(); solve(); }
            // Drag moves the goal, so you can watch the search change shape.
            if (in.dragHeld) {
                const int gx = int((mx_ - originX()) / cell()), gy = int((my_ - originY()) / cell());
                if (map_.inBounds(gx, gy) && !map_.solid(gx, gy)) { goalX_ = gx; goalY_ = gy; solve(); }
            }
        } else {
            if (in.selectSlot == 1) sepW_ = 0.f;
            if (in.selectSlot == 2) sepW_ = 1.6f;
            if (in.selectSlot == 3) aliW_ = 0.f;
            if (in.selectSlot == 4) aliW_ = 1.0f;
            if (in.selectSlot == 5) cohW_ = 0.f;
            if (in.selectSlot == 6) cohW_ = 0.9f;
            if (in.isPressed(otacon::Action::Aux4)) showNeighbours_ = !showNeighbours_;
            if (in.isPressed(otacon::Action::Aux6)) buildFlock();
        }
    }

    void update(otacon::Real dt) override {
        if (mode_ != 1) return;
        const float d = otacon::toFloat(dt);
        if (d <= 0.f || d > 0.1f) return;
        stepFlock(d);
    }

    void render(otacon::IRenderer& r, const otacon::DebugRuntime&) override {
        if (mode_ == 0) renderAStar(r); else renderBoids(r);
    }

    const char* status() const override {
        if (mode_ == 0)
            std::snprintf(buf_, sizeof buf_, "A*  %s  visited %d  path %d",
                          PathFinder::heuristicName(kHeuristics[heuristic_]),
                          finder_.visited(), finder_.pathLength());
        else
            std::snprintf(buf_, sizeof buf_, "boids  %d agents  sep %.1f  ali %.1f  coh %.1f",
                          int(flock_.boids.size()), sepW_, aliW_, cohW_);
        return buf_;
    }
    const char* keys() const override {
        return "SPACE  A* / boids\nA*:    E heuristic   F4 new maze   right-drag move the goal\nboids: 1-6 weights   F4 neighbour links   E reshuffle";
    }

private:
    // ---- layout ----
    float cell() const { return std::min((W_ - 176.f) / kGridW, (H_ - layout::kTop - 20.f) / kGridH); }
    float originX() const { return 8.f; }
    float originY() const { return layout::kTop + 6.f; }

    // ---- A* ----
    void buildGrid() {
        map_.resize(kGridW, kGridH, 0);
        map_.firstSolid = 1;
        otacon::Random rng(0xA5741 + seed_ * 977u);
        // Scattered rectangular blocks: enough structure to make the heuristic
        // matter, without becoming a maze with only one answer.
        for (int i = 0; i < 34; ++i) {
            const int w = rng.rangeI(2, 9), h = rng.rangeI(2, 7);
            const int x = rng.rangeI(1, kGridW - w - 1), y = rng.rangeI(1, kGridH - h - 1);
            for (int yy = y; yy < y + h; ++yy)
                for (int xx = x; xx < x + w; ++xx) map_.set(xx, yy, 1);
        }
        startX_ = 1; startY_ = 1; goalX_ = kGridW - 2; goalY_ = kGridH - 2;
        for (int dy = 0; dy < 3; ++dy) for (int dx = 0; dx < 3; ++dx) {
            map_.set(startX_ + dx, startY_ + dy, 0);
            map_.set(goalX_ - dx, goalY_ - dy, 0);
        }
    }

    void solve() {
        finder_.heuristic = kHeuristics[heuristic_];
        finder_.search(map_, startX_, startY_, goalX_, goalY_);
    }

    void renderAStar(otacon::IRenderer& r) const {
        const float c = cell(), ox = originX(), oy = originY();
        for (int y = 0; y < kGridH; ++y) {
            for (int x = 0; x < kGridW; ++x) {
                Color col;
                if (map_.solid(x, y))                col = Color{0.20f, 0.23f, 0.30f, 1.f};
                else if (finder_.wasExplored(x, y))  col = Color{0.16f, 0.30f, 0.40f, 1.f};  // closed
                else if (finder_.isFrontier(x, y))   col = Color{0.22f, 0.46f, 0.42f, 1.f};  // open
                else                                 col = Color{0.08f, 0.09f, 0.12f, 1.f};
                r.fillRect(ox + x * c, oy + y * c, c - 0.7f, c - 0.7f, col);
            }
        }
        const std::vector<int>& path = finder_.path();
        for (std::size_t i = 1; i < path.size(); ++i) {
            const int a = path[i - 1], b = path[i];
            r.drawLine(ox + (a % kGridW + 0.5f) * c, oy + (a / kGridW + 0.5f) * c,
                       ox + (b % kGridW + 0.5f) * c, oy + (b / kGridW + 0.5f) * c,
                       Color{1.f, 0.85f, 0.3f, 1.f}, 2.f);
        }
        r.fillRect(ox + startX_ * c, oy + startY_ * c, c, c, Color{0.45f, 0.90f, 0.55f, 1.f});
        r.fillRect(ox + goalX_ * c, oy + goalY_ * c, c, c, Color{1.f, 0.45f, 0.40f, 1.f});

        const float px = W_ - 162;
        float y = originY();
        r.drawText("A*", px, y, 1.f, Color{0.55f, 0.80f, 1.f, 1.f}); y += 11;
        r.drawText("f = g + h", px, y, 1.f, Color{0.62f, 0.68f, 0.80f, 1.f}); y += 9;
        r.drawText("g: cost so far", px, y, 1.f, Color{0.50f, 0.56f, 0.68f, 1.f}); y += 8;
        r.drawText("h: guess to goal", px, y, 1.f, Color{0.50f, 0.56f, 0.68f, 1.f}); y += 14;
        r.drawText("HEURISTIC (E)", px, y, 1.f, Color{0.55f, 0.80f, 1.f, 1.f}); y += 10;
        r.drawText(PathFinder::heuristicName(kHeuristics[heuristic_]), px, y, 1.f, Color{1.f, 0.85f, 0.35f, 1.f}); y += 14;
        char t[64];
        std::snprintf(t, sizeof t, "visited  %d", finder_.visited()); r.drawText(t, px, y, 1.f, Color{0.55f, 0.62f, 0.75f, 1.f}); y += 9;
        std::snprintf(t, sizeof t, "path     %d", finder_.pathLength()); r.drawText(t, px, y, 1.f, Color{0.55f, 0.62f, 0.75f, 1.f}); y += 14;
        r.drawText("teal  frontier", px, y, 1.f, Color{0.30f, 0.62f, 0.58f, 1.f}); y += 8;
        r.drawText("blue  explored", px, y, 1.f, Color{0.28f, 0.50f, 0.66f, 1.f}); y += 14;
        r.drawText("h=0 explores every-", px, y, 1.f, Color{0.42f, 0.48f, 0.60f, 1.f}); y += 8;
        r.drawText("where; over-weighted", px, y, 1.f, Color{0.42f, 0.48f, 0.60f, 1.f}); y += 8;
        r.drawText("h is fast but not", px, y, 1.f, Color{0.42f, 0.48f, 0.60f, 1.f}); y += 8;
        r.drawText("always shortest.", px, y, 1.f, Color{0.42f, 0.48f, 0.60f, 1.f});
    }

    // ---- boids ----
    void buildFlock() {
        otacon::Random rng(0xB01D5);
        flock_.clear();
        for (int i = 0; i < 190; ++i) {
            const otacon::Vec2f d = rng.onUnitCircle();
            flock_.add(rng.range(60.f, W_ - 220.f),
                       rng.range(layout::kTop + 40.f, H_ - 40.f),
                       d.x * 70.f, d.y * 70.f);
        }
    }

    void stepFlock(float dt) {
        flock_.params.separationWeight = sepW_;
        flock_.params.alignmentWeight  = aliW_;
        flock_.params.cohesionWeight   = cohW_;
        otacon::Vec2f cursor{mx_, my_};
        flock_.step(dt, &cursor);
        flock_.wrap(8.f, layout::kTop + 4.f, W_ - 170.f, H_ - 8.f);
    }

    void renderBoids(otacon::IRenderer& r) const {
        if (showNeighbours_) {
            const float R2 = flock_.params.neighbourRadius * flock_.params.neighbourRadius;
            for (std::size_t i = 0; i < flock_.boids.size(); i += 3) {
                for (std::size_t j = i + 1; j < flock_.boids.size(); j += 3) {
                    const float dx = flock_.boids[j].x - flock_.boids[i].x, dy = flock_.boids[j].y - flock_.boids[i].y;
                    if (dx * dx + dy * dy > R2) continue;
                    r.drawLine(flock_.boids[i].x, flock_.boids[i].y, flock_.boids[j].x, flock_.boids[j].y,
                               Color{0.35f, 0.65f, 0.90f, 0.13f}, 1.f);
                }
            }
        }
        for (const Boid& b : flock_.boids) {
            const float sp = std::sqrt(b.vx * b.vx + b.vy * b.vy);
            const float ux = sp > 0.001f ? b.vx / sp : 1.f, uy = sp > 0.001f ? b.vy / sp : 0.f;
            // A dart: a line from tail to nose, so heading is readable.
            r.drawLine(b.x - ux * 4.f, b.y - uy * 4.f, b.x + ux * 4.f, b.y + uy * 4.f,
                       Color{0.55f, 0.80f, 1.f, 0.95f}, 1.f);
        }
        r.drawRectOutline(mx_ - 5, my_ - 5, 10, 10, Color{1.f, 0.85f, 0.35f, 0.7f}, 1.f);

        const float px = W_ - 162;
        float y = originY();
        r.drawText("BOIDS", px, y, 1.f, Color{0.55f, 0.80f, 1.f, 1.f}); y += 11;
        r.drawText("three local rules", px, y, 1.f, Color{0.62f, 0.68f, 0.80f, 1.f}); y += 8;
        r.drawText("no agent sees the flock", px, y, 1.f, Color{0.42f, 0.48f, 0.60f, 1.f}); y += 14;
        y = weight(r, px, y, "1/2 separation", sepW_);
        y = weight(r, px, y, "3/4 alignment",  aliW_);
        y = weight(r, px, y, "5/6 cohesion",   cohW_);
        y += 8;
        r.drawText(sepW_ == 0.f ? "no separation: they clump"
                 : aliW_ == 0.f ? "no alignment: no shared heading"
                 : cohW_ == 0.f ? "no cohesion: the flock disperses"
                                : "all three: a flock",
                   px, y, 1.f, Color{1.f, 0.85f, 0.35f, 1.f});
    }

    float weight(otacon::IRenderer& r, float px, float y, const char* label, float v) const {
        r.drawText(label, px, y, 1.f, Color{0.62f, 0.68f, 0.80f, 1.f}); y += 9;
        r.fillRect(px, y, 140, 5, Color{0.12f, 0.14f, 0.20f, 1.f});
        r.fillRect(px, y, 140.f * (v / 2.f), 5, v > 0 ? Color{0.45f, 0.82f, 1.f, 1.f}
                                                      : Color{1.f, 0.45f, 0.35f, 1.f});
        return y + 11;
    }


    otacon::TileMap map_;
    PathFinder      finder_;
    otacon::Flock   flock_;
    float W_ = 640, H_ = 400, mx_ = 320, my_ = 200;
    float sepW_ = 1.6f, aliW_ = 1.0f, cohW_ = 0.9f;
    int   mode_ = 0, heuristic_ = 1;
    static constexpr PathFinder::Heuristic kHeuristics[3] = {
        PathFinder::Heuristic::None, PathFinder::Heuristic::Manhattan, PathFinder::Heuristic::Greedy,
    };
    int   startX_ = 1, startY_ = 1, goalX_ = 1, goalY_ = 1;
    std::uint32_t seed_ = 3;
    bool  showNeighbours_ = false;
    mutable char buf_[160]{};
};

} // namespace

Sample* makePathfindingBoids() { return new PathfindingBoids(); }

} // namespace samples
