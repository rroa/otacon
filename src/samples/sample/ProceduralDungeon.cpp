// ProceduralDungeon.cpp — sample 14: generation you can single-step.
//
// A generator that runs in one frame is impossible to debug and impossible to
// teach, because every interesting decision has already happened by the time you
// see the result. So this one runs as a state machine you can advance a step at
// a time, and each stage leaves its working visible:
//
//   1 scatter    drop candidate rooms at random and reject overlaps
//   2 connect    build a spanning tree so every room is reachable
//   3 loops      add back a few discarded edges, because a pure tree is a
//                corridor maze with no choices in it
//   4 carve      cut the corridors as L-bends between room centres
//   5 walls      thicken the boundary into wall tiles and place the doors
//
// The seed is on screen and fixed per run, so a layout you like can be found
// again — the single most useful property a generator can have.
#include "Sample.hpp"
#include "common/Art.hpp"
#include "core/math/Random.hpp"
#include "scene/TileMap.hpp"
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

constexpr int kW = 72, kH = 42;

// Tile codes. These are the dungeon's own vocabulary, mapped to tileset indices
// only at draw time — keeping "what a cell means" separate from "what it looks
// like" is why the same map can be drawn as art or as a debug view.
enum Cell : std::uint8_t { kVoid = 0, kFloor = 1, kCorridor = 2, kWall = 3, kDoor = 4 };

struct Room {
    int x, y, w, h;
    int cx() const { return x + w / 2; }
    int cy() const { return y + h / 2; }
    bool overlaps(const Room& o, int pad) const {
        return !(x - pad >= o.x + o.w || o.x - pad >= x + w ||
                 y - pad >= o.y + o.h || o.y - pad >= y + h);
    }
};

struct Edge { int a, b; float len; };

const char* kStageName[6] = {"empty", "1 scatter rooms", "2 spanning tree",
                             "3 add loops", "4 carve corridors", "5 walls & doors"};

class ProceduralDungeon final : public Sample {
public:
    void init(SampleContext& ctx) override {
        r_ = ctx.renderer;
        W_ = float(ctx.logicalW); H_ = float(ctx.logicalH);
        int ts = 0, tc = 0;
        tileset_ = r_->createTexture(art::tileset(ts, tc));
        tileCount_ = tc;
    }
    void shutdown() override { if (tileset_ && r_) { r_->destroyTexture(tileset_); tileset_ = 0; } }

    void enter() override { seed_ = 0x0D06; stage_ = 0; art_ = false; regenerate(); runTo(5); }

    void handleInput(const otacon::InputFrame& in) override {
        if (in.isPressed(otacon::Action::Jump)) { if (stage_ < 5) runTo(stage_ + 1); else { regenerate(); runTo(5); } }
        if (in.isPressed(otacon::Action::Aux4)) { seed_ += 7919u; regenerate(); runTo(5); }   // F4 new seed
        if (in.isPressed(otacon::Action::Aux6)) { regenerate(); }                             // E back to stage 0
        if (in.isPressed(otacon::Action::Aux5)) art_ = !art_;                                 // F6 art/debug
        if (in.selectSlot >= 1 && in.selectSlot <= 5) { regenerate(); runTo(in.selectSlot); }
    }

    void render(otacon::IRenderer& r, const otacon::DebugRuntime&) override {
        const float c = cell(), ox = originX(), oy = originY();

        if (art_ && stage_ >= 4) {
            // Art view: the same array, read through the tileset.
            for (int y = 0; y < kH; ++y)
                for (int x = 0; x < kW; ++x) {
                    const std::uint8_t t = map_.at(x, y);
                    if (t == kVoid) continue;
                    const int tile = (t == kWall) ? 6 : (t == kDoor ? 5 : (t == kCorridor ? 2 : 1));
                    const float uw = 1.f / float(tileCount_), u0 = float(tile - 1) * uw;
                    r.drawImage(tileset_, ox + x * c, oy + y * c, c + 0.5f, c + 0.5f, u0, 0, u0 + uw, 1);
                }
        } else {
            for (int y = 0; y < kH; ++y)
                for (int x = 0; x < kW; ++x) {
                    const std::uint8_t t = map_.at(x, y);
                    if (t == kVoid) continue;
                    r.fillRect(ox + x * c, oy + y * c, c - 0.4f, c - 0.4f, kCellColor[t]);
                }
        }

        // Rooms as outlines while they are still just candidates.
        if (stage_ >= 1) {
            for (std::size_t i = 0; i < rooms_.size(); ++i) {
                const Room& rm = rooms_[i];
                r.drawRectOutline(ox + rm.x * c, oy + rm.y * c, rm.w * c, rm.h * c,
                                  Color{0.45f, 0.72f, 1.f, stage_ >= 4 ? 0.22f : 0.75f}, 1.f);
                if (stage_ < 4) {
                    char n[8]; std::snprintf(n, sizeof n, "%d", int(i));
                    r.drawText(n, ox + rm.cx() * c - 2, oy + rm.cy() * c - 2, 1.f,
                               Color{0.75f, 0.85f, 1.f, 0.9f});
                }
            }
        }
        // The graph: tree edges in yellow, the loops added back in green.
        if (stage_ >= 2) {
            // Once the corridors exist, the graph is scaffolding: keep it visible
            // for the connection it explains, but stop it competing with the map.
            const float a = stage_ >= 4 ? 0.34f : 0.9f;
            for (const Edge& e : tree_) drawEdge(r, e, Color{1.f, 0.85f, 0.3f, a}, ox, oy, c);
            if (stage_ >= 3)
                for (const Edge& e : loops_) drawEdge(r, e, Color{0.45f, 0.95f, 0.6f, a}, ox, oy, c);
        }

        drawPanel(r);
    }

    const char* status() const override {
        std::snprintf(buf_, sizeof buf_, "seed 0x%X  %s  %d rooms  %d edges%s",
                      seed_, kStageName[stage_], int(rooms_.size()),
                      int(tree_.size() + loops_.size()), art_ ? "  ART" : "");
        return buf_;
    }
    const char* keys() const override {
        return "SPACE  next stage (or regenerate at the end)\n1-5    jump straight to a stage\nF4     new seed\nE      back to stage 0\nF6     art tiles / debug colours";
    }

private:
    float cell() const { return std::min((W_ - 160.f) / kW, (H_ - layout::kTop - 14.f) / kH); }
    float originX() const { return 8.f; }
    float originY() const { return layout::kTop + 6.f; }

    void regenerate() {
        map_.resize(kW, kH, kVoid);
        rooms_.clear(); tree_.clear(); loops_.clear();
        rng_.seed(seed_);
        stage_ = 0;
    }
    void runTo(int stage) {
        if (stage < stage_) { const std::uint32_t s = seed_; regenerate(); seed_ = s; rng_.seed(seed_); }
        while (stage_ < stage) advance();
    }

    void advance() {
        switch (stage_) {
            case 0: scatter();  break;
            case 1: spanTree(); break;
            case 2: addLoops(); break;
            case 3: carve();    break;
            case 4: walls();    break;
            default: return;
        }
        ++stage_;
    }

    // 1. Throw rooms at the map and keep the ones that fit. Rejection sampling
    //    is crude, but it is predictable and it never wedges.
    void scatter() {
        for (int attempt = 0; attempt < 400 && int(rooms_.size()) < kMaxRooms; ++attempt) {
            Room rm;
            rm.w = rng_.rangeI(5, 13);
            rm.h = rng_.rangeI(4, 9);
            rm.x = rng_.rangeI(2, kW - rm.w - 2);
            rm.y = rng_.rangeI(2, kH - rm.h - 2);
            bool clash = false;
            for (const Room& o : rooms_) if (rm.overlaps(o, 2)) { clash = true; break; }
            if (clash) continue;
            rooms_.push_back(rm);
        }
        for (const Room& rm : rooms_)
            for (int y = rm.y; y < rm.y + rm.h; ++y)
                for (int x = rm.x; x < rm.x + rm.w; ++x) map_.set(x, y, kFloor);
    }

    // 2. Prim's algorithm over room centres: the cheapest guarantee that every
    //    room is reachable from every other.
    void spanTree() {
        const int n = int(rooms_.size());
        if (n < 2) return;
        std::vector<bool> in(n, false);
        in[0] = true;
        for (int added = 1; added < n; ++added) {
            Edge best{-1, -1, 1e30f};
            for (int a = 0; a < n; ++a) {
                if (!in[a]) continue;
                for (int b = 0; b < n; ++b) {
                    if (in[b]) continue;
                    const float dx = float(rooms_[a].cx() - rooms_[b].cx());
                    const float dy = float(rooms_[a].cy() - rooms_[b].cy());
                    const float d = dx * dx + dy * dy;
                    if (d < best.len) best = {a, b, d};
                }
            }
            if (best.a < 0) break;
            in[best.b] = true;
            tree_.push_back(best);
        }
    }

    // 3. A spanning tree has exactly one route between any two rooms, which
    //    plays as a corridor maze. Adding a few short extra edges back gives the
    //    player choices and the level loops.
    void addLoops() {
        const int n = int(rooms_.size());
        std::vector<Edge> candidates;
        for (int a = 0; a < n; ++a)
            for (int b = a + 1; b < n; ++b) {
                bool inTree = false;
                for (const Edge& e : tree_)
                    if ((e.a == a && e.b == b) || (e.a == b && e.b == a)) { inTree = true; break; }
                if (inTree) continue;
                const float dx = float(rooms_[a].cx() - rooms_[b].cx());
                const float dy = float(rooms_[a].cy() - rooms_[b].cy());
                candidates.push_back({a, b, dx * dx + dy * dy});
            }
        std::sort(candidates.begin(), candidates.end(),
                  [](const Edge& p, const Edge& q) { return p.len < q.len; });
        const int want = std::max(1, n / 5);
        for (int i = 0; i < want && i < int(candidates.size()); ++i) loops_.push_back(candidates[i]);
    }

    // 4. L-bends between centres. Which leg goes first is a coin flip, which is
    //    all it takes to stop every corridor looking the same.
    void carve() {
        auto corridor = [&](const Edge& e) {
            const Room& A = rooms_[e.a];
            const Room& B = rooms_[e.b];
            const bool hFirst = rng_.chance(0.5f);
            const int x0 = A.cx(), y0 = A.cy(), x1 = B.cx(), y1 = B.cy();
            if (hFirst) { hall(x0, x1, y0, true); hall(y0, y1, x1, false); }
            else        { hall(y0, y1, x0, false); hall(x0, x1, y1, true); }
        };
        for (const Edge& e : tree_)  corridor(e);
        for (const Edge& e : loops_) corridor(e);
    }
    void hall(int from, int to, int fixed, bool horizontal) {
        const int step = from <= to ? 1 : -1;
        for (int v = from; v != to + step; v += step) {
            const int x = horizontal ? v : fixed;
            const int y = horizontal ? fixed : v;
            if (map_.at(x, y) == kVoid) map_.set(x, y, kCorridor);
            // Two tiles wide vertically so corridors are walkable, not hairlines.
            if (horizontal && map_.at(x, y + 1) == kVoid) map_.set(x, y + 1, kCorridor);
        }
    }

    // 5. Any void touching a floor becomes wall; a corridor meeting a room
    //    becomes a door. Both are purely local rules over the finished array.
    void walls() {
        otacon::TileMap out = map_;
        for (int y = 0; y < kH; ++y)
            for (int x = 0; x < kW; ++x) {
                if (map_.at(x, y) != kVoid) continue;
                bool touches = false;
                for (int dy = -1; dy <= 1 && !touches; ++dy)
                    for (int dx = -1; dx <= 1; ++dx) {
                        const std::uint8_t t = map_.at(x + dx, y + dy);
                        if (t == kFloor || t == kCorridor) { touches = true; break; }
                    }
                if (touches) out.set(x, y, kWall);
            }
        for (int y = 1; y < kH - 1; ++y)
            for (int x = 1; x < kW - 1; ++x) {
                if (map_.at(x, y) != kCorridor) continue;
                const bool nearRoom = map_.at(x - 1, y) == kFloor || map_.at(x + 1, y) == kFloor ||
                                      map_.at(x, y - 1) == kFloor || map_.at(x, y + 1) == kFloor;
                if (nearRoom) out.set(x, y, kDoor);
            }
        map_ = out;
    }

    void drawEdge(otacon::IRenderer& r, const Edge& e, Color c, float ox, float oy, float cs) const {
        if (e.a < 0 || e.b < 0 || e.a >= int(rooms_.size()) || e.b >= int(rooms_.size())) return;
        r.drawLine(ox + (rooms_[e.a].cx() + 0.5f) * cs, oy + (rooms_[e.a].cy() + 0.5f) * cs,
                   ox + (rooms_[e.b].cx() + 0.5f) * cs, oy + (rooms_[e.b].cy() + 0.5f) * cs, c, 1.f);
    }

    void drawPanel(otacon::IRenderer& r) const {
        const float px = W_ - 146;
        float y = originY();
        r.drawText("STAGES", px, y, 1.f, Color{0.55f, 0.80f, 1.f, 1.f}); y += 11;
        for (int i = 1; i <= 5; ++i) {
            r.drawText(kStageName[i], px, y, 1.f,
                       i == stage_ ? Color{1.f, 0.85f, 0.35f, 1.f}
                                   : (i < stage_ ? Color{0.45f, 0.85f, 0.55f, 1.f}
                                                 : Color{0.40f, 0.45f, 0.55f, 1.f}));
            y += 9;
        }
        y += 8;
        char t[64];
        std::snprintf(t, sizeof t, "seed  0x%X", seed_);
        r.drawText(t, px, y, 1.f, Color{0.55f, 0.62f, 0.75f, 1.f}); y += 9;
        std::snprintf(t, sizeof t, "rooms %d", int(rooms_.size()));
        r.drawText(t, px, y, 1.f, Color{0.55f, 0.62f, 0.75f, 1.f}); y += 9;
        std::snprintf(t, sizeof t, "tree  %d", int(tree_.size()));
        r.drawText(t, px, y, 1.f, Color{1.f, 0.85f, 0.3f, 1.f}); y += 9;
        std::snprintf(t, sizeof t, "loops %d", int(loops_.size()));
        r.drawText(t, px, y, 1.f, Color{0.45f, 0.95f, 0.6f, 1.f}); y += 14;
        r.drawText("a pure tree is a maze", px, y, 1.f, Color{0.42f, 0.48f, 0.60f, 1.f}); y += 8;
        r.drawText("with one route; the", px, y, 1.f, Color{0.42f, 0.48f, 0.60f, 1.f}); y += 8;
        r.drawText("loops are what make", px, y, 1.f, Color{0.42f, 0.48f, 0.60f, 1.f}); y += 8;
        r.drawText("it feel like a place.", px, y, 1.f, Color{0.42f, 0.48f, 0.60f, 1.f});
    }

    static constexpr int kMaxRooms = 16;
    static constexpr Color kCellColor[5] = {
        {0, 0, 0, 0},
        {0.24f, 0.30f, 0.40f, 1.f},   // floor
        {0.18f, 0.24f, 0.30f, 1.f},   // corridor
        {0.12f, 0.14f, 0.18f, 1.f},   // wall
        {0.70f, 0.55f, 0.25f, 1.f},   // door
    };

    otacon::IRenderer* r_ = nullptr;
    otacon::TextureHandle tileset_ = 0;
    otacon::TileMap map_;
    std::vector<Room> rooms_;
    std::vector<Edge> tree_, loops_;
    otacon::Random rng_{0x0D06};
    std::uint32_t seed_ = 0x0D06;
    float W_ = 640, H_ = 400;
    int   stage_ = 0, tileCount_ = 6;
    bool  art_ = false;
    mutable char buf_[160]{};
};

} // namespace

Sample* makeProceduralDungeon() { return new ProceduralDungeon(); }

} // namespace samples
