// BroadPhase.cpp — sample 18: why O(n^2) stops being acceptable.
//
// collideWithGroup tests every pair. Thirty bodies is 435 tests and nobody
// notices; three hundred is 44,850 and the frame is gone. The growth is the
// whole problem, and it is invisible until you count.
//
// So this counts. The same bodies are tested both ways every frame -- the naive
// pair loop, and scene/SpatialGrid.hpp's uniform grid -- and the two numbers are
// plotted against each other as the population climbs. Nothing about the answer
// changes; only the number of tests taken to reach it.
//
// The cell size matters more than people expect, so it is a knob here. Too small
// and one body spans many cells; too large and everything lands in one bucket,
// which is the O(n^2) you started with. The occupancy readout is how you tell.
#include "Sample.hpp"
#include "core/math/Random.hpp"
#include "scene/Entity.hpp"
#include "scene/SpatialGrid.hpp"
#include "render/IRenderer.hpp"
#include "platform/Window.hpp"
#include "core/debug/Debug.hpp"
#include <cstdio>
#include <vector>

namespace samples {
namespace {

using otacon::Color;
using otacon::Entity;
using otacon::R;

constexpr int kMaxBodies = 900;
constexpr int kPlot = 180;

class BroadPhase final : public Sample {
public:
    void init(SampleContext& ctx) override { W_ = float(ctx.logicalW); H_ = float(ctx.logicalH); }

    void enter() override {
        count_ = 120; cell_ = 48.f; showGrid_ = true; autoRamp_ = false;
        grid_.setCellSize(cell_);
        seed();
        head_ = 0;
        for (int i = 0; i < kPlot; ++i) { naivePlot_[i] = 0; gridPlot_[i] = 0; }
    }

    void handleInput(const otacon::InputFrame& in) override {
        if (in.isPressed(otacon::Action::Jump)) autoRamp_ = !autoRamp_;
        if (in.isPressed(otacon::Action::Aux6)) showGrid_ = !showGrid_;
        if (in.isPressed(otacon::Action::Aux4)) setCount(count_ / 2);
        if (in.isPressed(otacon::Action::Aux5)) setCount(count_ * 2);
        if (in.selectSlot >= 1 && in.selectSlot <= 6) {
            const float sizes[6] = {16.f, 24.f, 32.f, 48.f, 96.f, 200.f};
            cell_ = sizes[in.selectSlot - 1];
            grid_.setCellSize(cell_);
        }
    }

    void update(otacon::Real dt) override {
        const float d = otacon::toFloat(dt);
        if (d <= 0.f || d > 0.1f) return;
        if (autoRamp_) { ramp_ += d; if (ramp_ > 0.35f) { ramp_ = 0; setCount(int(count_ * 1.12f) + 4); } }

        const float top = layout::kTop + 4, bottom = H_ - 118;
        for (int i = 0; i < count_; ++i) {
            Entity& e = bodies_[std::size_t(i)];
            float x = otacon::toFloat(e.pos.x) + vel_[i][0] * d;
            float y = otacon::toFloat(e.pos.y) + vel_[i][1] * d;
            if (x < 4)        { x = 4; vel_[i][0] = -vel_[i][0]; }
            if (x > W_ - 12)  { x = W_ - 12; vel_[i][0] = -vel_[i][0]; }
            if (y < top)      { y = top; vel_[i][1] = -vel_[i][1]; }
            if (y > bottom)   { y = bottom; vel_[i][1] = -vel_[i][1]; }
            e.pos = {R(x), R(y)};
        }

        // --- the measurement -------------------------------------------------
        // Both methods answer the same question; only the cost differs.
        naiveTests_ = 0;
        naiveHits_ = 0;
        for (int i = 0; i < count_; ++i)
            for (int j = i + 1; j < count_; ++j) {
                ++naiveTests_;
                if (overlaps(bodies_[std::size_t(i)], bodies_[std::size_t(j)])) ++naiveHits_;
            }

        live_.clear();
        for (int i = 0; i < count_; ++i) live_.push_back(&bodies_[std::size_t(i)]);
        grid_.rebuild(live_);
        gridTests_ = 0;
        gridHits_ = 0;
        std::vector<Entity*> near;
        for (int i = 0; i < count_; ++i) {
            grid_.query(bodies_[std::size_t(i)], near);
            gridTests_ += int(near.size());
            for (Entity* o : near) if (overlaps(bodies_[std::size_t(i)], *o)) ++gridHits_;
        }
        gridTests_ /= 2;    // each pair is visited from both sides
        gridHits_  /= 2;

        naivePlot_[head_] = naiveTests_;
        gridPlot_[head_] = gridTests_;
        head_ = (head_ + 1) % kPlot;
    }

    void render(otacon::IRenderer& r, const otacon::DebugRuntime&) override {
        if (showGrid_) {
            for (float x = 0; x < W_; x += cell_)
                r.drawLine(x, layout::kTop, x, H_ - 118, Color{1, 1, 1, 0.05f}, 1.f);
            for (float y = layout::kTop; y < H_ - 118; y += cell_)
                r.drawLine(0, y, W_, y, Color{1, 1, 1, 0.05f}, 1.f);
        }
        for (int i = 0; i < count_; ++i) {
            const Entity& e = bodies_[std::size_t(i)];
            r.fillRect(otacon::toFloat(e.pos.x), otacon::toFloat(e.pos.y), 8, 8,
                       Color{0.45f, 0.72f, 1.f, 0.85f});
        }
        drawPanel(r);
    }

    const char* status() const override {
        std::snprintf(buf_, sizeof buf_, "n=%d  naive %d tests  grid %d tests  (%.1fx fewer)  cell %.0f",
                      count_, naiveTests_, gridTests_,
                      gridTests_ > 0 ? float(naiveTests_) / float(gridTests_) : 0.f, cell_);
        return buf_;
    }
    const char* keys() const override {
        return "SPACE  auto-ramp the population\nF4/F6  halve / double the count\n1-6    cell size\nE      show the grid";
    }

private:
    static bool overlaps(const Entity& a, const Entity& b) {
        return !(a.right() < b.pos.x || a.pos.x > b.right() ||
                 a.bottom() < b.pos.y || a.pos.y > b.bottom());
    }
    void setCount(int n) { count_ = n < 8 ? 8 : (n > kMaxBodies ? kMaxBodies : n); }

    void seed() {
        otacon::Random rng(0xB40Du);
        bodies_.assign(kMaxBodies, Entity{});
        for (int i = 0; i < kMaxBodies; ++i) {
            bodies_[std::size_t(i)].pos = {R(rng.range(8.f, W_ - 16.f)),
                                           R(rng.range(layout::kTop + 4, H_ - 122))};
            bodies_[std::size_t(i)].size = {R(8), R(8)};
            vel_[i][0] = rng.range(-70.f, 70.f);
            vel_[i][1] = rng.range(-70.f, 70.f);
        }
    }

    void drawPanel(otacon::IRenderer& r) const {
        const float y0 = H_ - 112;
        r.fillRect(0, y0 - 6, W_, H_ - y0 + 6, Color{0.03f, 0.04f, 0.06f, 0.92f});

        // Pair tests per frame, both methods, log-ish scaled so the gap is
        // visible even when it is two orders of magnitude.
        const float gx = 8, gy = y0, gw = W_ * 0.55f, gh = 60;
        r.fillRect(gx, gy, gw, gh, Color{0.06f, 0.07f, 0.10f, 1.f});
        r.drawRectOutline(gx, gy, gw, gh, Color{0.22f, 0.28f, 0.38f, 1.f}, 1.f);
        r.drawText("PAIR TESTS PER FRAME", gx, gy - 10, 1.f, Color{0.55f, 0.80f, 1.f, 1.f});
        // Auto-scale to the largest value on screen. A fixed ceiling pinned the
        // whole plot to the floor at low populations, which hid the very thing
        // it is here to show.
        int peak = 1;
        for (int i = 0; i < kPlot; ++i) if (naivePlot_[i] > peak) peak = naivePlot_[i];
        auto mapY = [&](int v) {
            const float f = float(v) / float(peak);
            return gy + gh - (f > 1.f ? 1.f : f) * gh;
        };
        char pk[48]; std::snprintf(pk, sizeof pk, "peak %d", peak);
        r.drawText(pk, gx + gw - 46, gy + 2, 1.f, Color{0.40f, 0.46f, 0.58f, 1.f});
        for (int i = 1; i < kPlot; ++i) {
            const float x0 = gx + float(i - 1) / kPlot * gw, x1 = gx + float(i) / kPlot * gw;
            r.drawLine(x0, mapY(naivePlot_[(head_ + i - 1) % kPlot]),
                       x1, mapY(naivePlot_[(head_ + i) % kPlot]), Color{1.f, 0.45f, 0.35f, 0.95f}, 1.f);
            r.drawLine(x0, mapY(gridPlot_[(head_ + i - 1) % kPlot]),
                       x1, mapY(gridPlot_[(head_ + i) % kPlot]), Color{0.45f, 0.90f, 0.60f, 0.95f}, 1.f);
        }

        const float px = gx + gw + 14;
        float y = y0;
        char t[96];
        r.drawText("O(n^2)", px, y, 1.f, Color{1.f, 0.45f, 0.35f, 1.f});
        std::snprintf(t, sizeof t, "%d tests", naiveTests_);
        r.drawText(t, px + 70, y, 1.f, Color{1.f, 0.45f, 0.35f, 1.f}); y += 10;
        r.drawText("SpatialGrid", px, y, 1.f, Color{0.45f, 0.90f, 0.60f, 1.f});
        std::snprintf(t, sizeof t, "%d tests", gridTests_);
        r.drawText(t, px + 70, y, 1.f, Color{0.45f, 0.90f, 0.60f, 1.f}); y += 12;

        std::snprintf(t, sizeof t, "bodies      %d", count_);
        r.drawText(t, px, y, 1.f, Color{0.80f, 0.86f, 0.95f, 1.f}); y += 9;
        std::snprintf(t, sizeof t, "cell size   %.0f", cell_);
        r.drawText(t, px, y, 1.f, Color{0.55f, 0.62f, 0.75f, 1.f}); y += 9;
        std::snprintf(t, sizeof t, "buckets     %zu", grid_.bucketCount());
        r.drawText(t, px, y, 1.f, Color{0.55f, 0.62f, 0.75f, 1.f}); y += 9;
        // The number that says whether the cell size is right.
        std::snprintf(t, sizeof t, "worst bucket %zu", grid_.largestBucket());
        r.drawText(t, px, y, 1.f, grid_.largestBucket() > std::size_t(count_) / 4
                                      ? Color{1.f, 0.55f, 0.45f, 1.f} : Color{0.55f, 0.62f, 0.75f, 1.f});
        y += 12;

        // The proof that the cheaper method is not a different answer.
        std::snprintf(t, sizeof t, "overlaps found:  naive %d   grid %d", naiveHits_, gridHits_);
        r.drawText(t, gx, gy + gh + 5, 1.f, naiveHits_ == gridHits_
                                                ? Color{0.45f, 0.90f, 0.60f, 1.f}
                                                : Color{1.f, 0.40f, 0.40f, 1.f});
        r.drawText("same answer, fewer tests - that is the whole trade",
                   gx, gy + gh + 15, 1.f, Color{0.42f, 0.48f, 0.60f, 1.f});
    }

    otacon::SpatialGrid grid_{48.f};
    std::vector<Entity>  bodies_;
    std::vector<Entity*> live_;
    float vel_[kMaxBodies][2]{};
    int   naivePlot_[kPlot]{}, gridPlot_[kPlot]{};
    float W_ = 640, H_ = 400, cell_ = 48.f, ramp_ = 0.f;
    int   count_ = 120, head_ = 0;
    int   naiveTests_ = 0, gridTests_ = 0, naiveHits_ = 0, gridHits_ = 0;
    bool  showGrid_ = true, autoRamp_ = false;
    mutable char buf_[160]{};
};

} // namespace

Sample* makeBroadPhase() { return new BroadPhase(); }

} // namespace samples
