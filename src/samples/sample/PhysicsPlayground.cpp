// PhysicsPlayground.cpp — sample 07: what the engine's collision actually does.
//
// Otacon's solver is flixel's, and flixel's is deliberately narrow: axis-aligned
// boxes, separated one axis at a time (X, then Y), with no restitution and no
// rotation. That is not a shortcoming to apologise for — it is the reason a
// platformer built on it feels tight and predictable, because a box that lands
// stops dead instead of chattering.
//
// This playground makes the narrowness legible. Boxes stack, the swept hulls the
// solver actually tests are drawn, and you can step one frame at a time to watch
// the X pass and the Y pass resolve separately.
#include "Sample.hpp"
#include "common/Rng.hpp"
#include "scene/Entity.hpp"
#include "scene/Collision.hpp"
#include "render/IRenderer.hpp"
#include "platform/Window.hpp"
#include "core/debug/Debug.hpp"
#include <cstdio>
#include <memory>
#include <vector>

namespace samples {
namespace {

using otacon::Color;
using otacon::Entity;
using otacon::R;

class PhysicsPlayground final : public Sample {
public:
    void init(SampleContext& ctx) override {
        W_ = float(ctx.logicalW); H_ = float(ctx.logicalH);
    }

    void enter() override {
        boxes_.clear(); statics_.clear(); all_.clear();
        gravity_ = 900.f; dragCoef_ = 0.f; maxFall_ = 480.f;
        rng_.reseed(0xB0CE5);
        buildStatics();
        for (int i = 0; i < 14; ++i) spawn(rng_.range(80.f, W_ - 120.f), rng_.range(60.f, 160.f));
        grabbed_ = nullptr;
    }

    void handleInput(const otacon::InputFrame& in) override {
        mx_ = in.mouseNx * W_; my_ = in.mouseNy * H_;
        if (in.isPressed(otacon::Action::Jump)) spawn(mx_, my_);
        if (in.isPressed(otacon::Action::Aux6)) { boxes_.clear(); rebuildAll(); }        // E clear
        if (in.isPressed(otacon::Action::Aux4)) showHulls_ = !showHulls_;                // F4
        if (in.selectSlot == 1) gravity_ = 0.f;
        if (in.selectSlot == 2) gravity_ = 900.f;
        if (in.selectSlot == 3) gravity_ = 2200.f;
        if (in.selectSlot == 4) dragCoef_ = 0.f;
        if (in.selectSlot == 5) dragCoef_ = 600.f;
        if (in.selectSlot == 6) maxFall_ = 200.f;
        if (in.selectSlot == 7) maxFall_ = 480.f;
        if (in.selectSlot == 8) maxFall_ = 4000.f;
        applyTuning();

        // Right-drag picks up the box under the cursor — the fastest way to
        // provoke the solver into a case you want to look at.
        if (in.dragPressed) grabbed_ = pick(mx_, my_);
        if (!in.dragHeld) grabbed_ = nullptr;
    }

    void update(otacon::Real dt) override {
        if (grabbed_) {
            // Move the grabbed box directly and zero its velocity, so it pushes
            // rather than launches.
            grabbed_->pos = {R(mx_ - otacon::toFloat(grabbed_->size.x) * 0.5f),
                             R(my_ - otacon::toFloat(grabbed_->size.y) * 0.5f)};
            grabbed_->velocity = {R(0), R(0)};
        }
        for (auto& s : statics_) s->update(dt);        // refreshes the static hulls
        for (auto& b : boxes_) {
            b->onFloor = false;
            b->update(dt);
        }
        // Every box against everything else. O(n^2), which is honest at this
        // count and is exactly the thing a broad phase would replace.
        for (auto& b : boxes_) {
            if (b.get() == grabbed_) continue;
            otacon::collideWithGroup(*b, all_);
        }
        // Anything that escapes the pit is recycled rather than lost.
        for (auto& b : boxes_) {
            if (otacon::toFloat(b->pos.y) > H_ + 80.f) {
                b->pos = {R(rng_.range(80.f, W_ - 120.f)), R(40.f)};
                b->velocity = {R(0), R(0)};
            }
        }
    }

    void render(otacon::IRenderer& r, const otacon::DebugRuntime& dbg) override {
        for (auto& s : statics_) drawBox(r, *s, Color{0.22f, 0.26f, 0.34f, 1.f});
        for (auto& b : boxes_) {
            Color c = b->onFloor ? Color{0.45f, 0.80f, 0.55f, 1.f} : Color{0.42f, 0.62f, 0.92f, 1.f};
            if (b.get() == grabbed_) c = Color{1.f, 0.82f, 0.35f, 1.f};
            drawBox(r, *b, c);
        }

        // The swept hulls: the rect the solver tests is the union of where a box
        // was and where it wants to be, which is why fast boxes do not tunnel.
        if (showHulls_ || dbg.enabled(otacon::DebugView::Colliders)) {
            for (auto& b : boxes_) {
                drawRect(r, b->colHullX, Color{1.f, 0.45f, 0.35f, 0.55f});
                drawRect(r, b->colHullY, Color{0.45f, 0.85f, 1.f, 0.55f});
            }
        }

        drawPanel(r);
    }

    const char* status() const override {
        std::snprintf(buf_, sizeof buf_, "%zu boxes  g %.0f  drag %.0f  maxfall %.0f%s",
                      boxes_.size(), gravity_, dragCoef_, maxFall_, showHulls_ ? "  HULLS" : "");
        return buf_;
    }
    const char* keys() const override {
        return "SPACE  drop a box at the cursor\nE      clear the boxes\nF4     show the swept hulls\n1-3    gravity   4-5 drag   6-8 terminal speed\nright-drag  grab a box\nP / O  pause and single-step";
    }

private:
    void buildStatics() {
        const float top = layout::kTop;
        add(statics_, 0, H_ - 26, W_, 26);                       // floor
        add(statics_, 0, top, 10, H_ - top);                     // left wall
        add(statics_, W_ - 10, top, 10, H_ - top);               // right wall
        add(statics_, 90, H_ - 96, 150, 12);                     // ledges
        add(statics_, 300, H_ - 150, 170, 12);
        add(statics_, 200, H_ - 220, 120, 12);
        for (auto& s : statics_) { s->fixed = true; s->solid = true; s->moves = true; }
        rebuildAll();
    }
    void add(std::vector<std::unique_ptr<Entity>>& into, float x, float y, float w, float h) {
        auto e = std::make_unique<Entity>();
        e->pos = {R(x), R(y)};
        e->size = {R(w), R(h)};
        e->refreshHulls();
        into.push_back(std::move(e));
    }
    void spawn(float x, float y) {
        if (boxes_.size() >= kMaxBoxes) return;
        const float s = rng_.range(12.f, 26.f);
        add(boxes_, x - s * 0.5f, y - s * 0.5f, s, s);
        Entity& b = *boxes_.back();
        b.solid = true; b.fixed = false; b.moves = true;
        b.velocity = {R(rng_.range(-40.f, 40.f)), R(0)};
        rebuildAll();
        applyTuning();
    }
    // The collision group is every solid in the scene; rebuilt when the set changes.
    void rebuildAll() {
        all_.clear();
        for (auto& s : statics_) all_.push_back(s.get());
        for (auto& b : boxes_)   all_.push_back(b.get());
    }
    void applyTuning() {
        for (auto& b : boxes_) {
            b->acceleration = {R(0), R(gravity_)};
            b->drag = {R(dragCoef_), R(0)};
            b->maxVelocity = {R(2000), R(maxFall_)};
        }
    }
    Entity* pick(float x, float y) {
        for (auto it = boxes_.rbegin(); it != boxes_.rend(); ++it) {
            Entity& b = **it;
            const float bx = otacon::toFloat(b.pos.x), by = otacon::toFloat(b.pos.y);
            const float bw = otacon::toFloat(b.size.x), bh = otacon::toFloat(b.size.y);
            if (x >= bx && x <= bx + bw && y >= by && y <= by + bh) return &b;
        }
        return nullptr;
    }

    static void drawBox(otacon::IRenderer& r, const Entity& e, Color c) {
        r.fillRect(otacon::toFloat(e.pos.x), otacon::toFloat(e.pos.y),
                   otacon::toFloat(e.size.x), otacon::toFloat(e.size.y), c);
    }
    static void drawRect(otacon::IRenderer& r, const otacon::Rect& rc, Color c) {
        r.drawRectOutline(otacon::toFloat(rc.x), otacon::toFloat(rc.y),
                          otacon::toFloat(rc.w), otacon::toFloat(rc.h), c, 1.f);
    }

    void drawPanel(otacon::IRenderer& r) const {
        float y = layout::kTop + 4;
        const float px = W_ - 150;
        r.drawText("SOLVER", px, y, 1.f, Color{0.55f, 0.80f, 1.f, 1.f}); y += 11;
        r.drawText("AABB only", px, y, 1.f, Color{0.62f, 0.68f, 0.80f, 1.f}); y += 8;
        r.drawText("X pass, then Y pass", px, y, 1.f, Color{0.62f, 0.68f, 0.80f, 1.f}); y += 8;
        r.drawText("no restitution", px, y, 1.f, Color{0.62f, 0.68f, 0.80f, 1.f}); y += 8;
        r.drawText("no rotation", px, y, 1.f, Color{0.62f, 0.68f, 0.80f, 1.f}); y += 14;
        char t[64];
        std::snprintf(t, sizeof t, "gravity   %.0f", gravity_);  r.drawText(t, px, y, 1.f, Color{0.55f, 0.62f, 0.75f, 1.f}); y += 9;
        std::snprintf(t, sizeof t, "drag      %.0f", dragCoef_); r.drawText(t, px, y, 1.f, Color{0.55f, 0.62f, 0.75f, 1.f}); y += 9;
        std::snprintf(t, sizeof t, "max fall  %.0f", maxFall_);  r.drawText(t, px, y, 1.f, Color{0.55f, 0.62f, 0.75f, 1.f}); y += 14;
        r.drawText("red  = swept hull X", px, y, 1.f, Color{1.f, 0.45f, 0.35f, 1.f}); y += 9;
        r.drawText("blue = swept hull Y", px, y, 1.f, Color{0.45f, 0.85f, 1.f, 1.f}); y += 9;
        r.drawText("green = onFloor", px, y, 1.f, Color{0.45f, 0.80f, 0.55f, 1.f});

        r.drawText("boxes land dead because separation has no bounce — that is the feel",
                   8, H_ - 10, 1.f, Color{0.42f, 0.48f, 0.60f, 1.f});
    }

    static constexpr std::size_t kMaxBoxes = 90;

    std::vector<std::unique_ptr<Entity>> boxes_, statics_;
    std::vector<Entity*> all_;
    Entity* grabbed_ = nullptr;
    Rng rng_{0xB0CE5};
    float W_ = 640, H_ = 400, mx_ = 320, my_ = 200;
    float gravity_ = 900, dragCoef_ = 0, maxFall_ = 480;
    bool showHulls_ = false;
    mutable char buf_[128]{};
};

} // namespace

Sample* makePhysicsPlayground() { return new PhysicsPlayground(); }

} // namespace samples
