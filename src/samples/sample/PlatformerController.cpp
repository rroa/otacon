// PlatformerController.cpp — sample 10: game feel, made visible.
//
// The physics in a platformer is trivial. What separates one that feels good
// from one that feels broken is a handful of small forgivenesses, and every one
// of them is a lie told to the player on purpose:
//
//   coyote time    you can still jump for a moment after walking off an edge
//   jump buffer    a jump pressed just before landing is remembered
//   variable jump  releasing early cuts the rise short
//   apex hang      gravity eases near the top of the arc
//   fast fall      falling uses stronger gravity than rising
//
// Each is a timer or a multiplier, and each can be switched off here. The strip
// along the bottom is a rolling timeline of the controller's state, so when a
// jump fails you can see *which* forgiveness was missing rather than guessing.
#include "Sample.hpp"
#include "scene/Entity.hpp"
#include "scene/Collision.hpp"
#include "render/IRenderer.hpp"
#include "platform/Window.hpp"
#include "core/debug/Debug.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <memory>
#include <vector>

namespace samples {
namespace {

using otacon::Color;
using otacon::Entity;
using otacon::R;

struct Tuning {
    const char* name;
    float runSpeed, accel, friction;
    float jumpVel, gravityUp, gravityDown, maxFall;
    float coyote, buffer, apexWindow, apexScale;
};

const Tuning kTunings[] = {
    {"tight",  180.f, 1600.f, 2000.f, -330.f, 1250.f, 1750.f, 520.f, 0.10f, 0.12f, 55.f, 0.55f},
    {"floaty", 150.f,  700.f,  600.f, -300.f,  700.f,  850.f, 320.f, 0.16f, 0.18f, 90.f, 0.45f},
    {"heavy",  200.f, 2400.f, 3000.f, -380.f, 1900.f, 2600.f, 800.f, 0.05f, 0.06f, 30.f, 0.80f},
};
constexpr int kTuningCount = int(sizeof kTunings / sizeof kTunings[0]);

constexpr int kTimeline = 210;

class PlatformerController final : public Sample {
public:
    void init(SampleContext& ctx) override { W_ = float(ctx.logicalW); H_ = float(ctx.logicalH); }

    void enter() override {
        tuning_ = 0;
        coyoteOn_ = bufferOn_ = variableOn_ = apexOn_ = fastFallOn_ = true;
        buildLevel();
        resetPlayer();
        head_ = 0;
        for (int i = 0; i < kTimeline; ++i) tl_[i] = 0;
    }

    void handleInput(const otacon::InputFrame& in) override {
        const bool held = in.isHeld(otacon::Action::Jump);
        jumpPressed_ = in.isPressed(otacon::Action::Jump);
        jumpReleased_ = jumpHeld_ && !held;
        jumpHeld_ = held;
        // A/D would be ideal, but the engine's action set is game-agnostic: the
        // pointer's side of the player drives the run instead, which keeps the
        // sample to one input seam.
        steer_ = (in.mouseNx * W_) - otacon::toFloat(player_.pos.x);
        if (in.selectSlot >= 1 && in.selectSlot <= kTuningCount) tuning_ = in.selectSlot - 1;
        if (in.isPressed(otacon::Action::Aux4)) coyoteOn_   = !coyoteOn_;
        if (in.isPressed(otacon::Action::Aux5)) bufferOn_   = !bufferOn_;
        if (in.isPressed(otacon::Action::Aux7)) variableOn_ = !variableOn_;
        if (in.isPressed(otacon::Action::Aux8)) apexOn_     = !apexOn_;
        if (in.isPressed(otacon::Action::Aux6)) fastFallOn_ = !fastFallOn_;
    }

    void update(otacon::Real dt) override {
        const float d = otacon::toFloat(dt);
        if (d <= 0.f || d > 0.1f) return;
        const Tuning& T = kTunings[tuning_];

        // --- timers ----------------------------------------------------------
        if (wasOnFloor_) coyoteTimer_ = coyoteOn_ ? T.coyote : 0.f;
        else coyoteTimer_ = std::max(0.f, coyoteTimer_ - d);
        if (jumpPressed_) bufferTimer_ = bufferOn_ ? T.buffer : 0.0001f;
        else bufferTimer_ = std::max(0.f, bufferTimer_ - d);

        // --- horizontal ------------------------------------------------------
        const float want = std::fabs(steer_) < 6.f ? 0.f : (steer_ > 0 ? T.runSpeed : -T.runSpeed);
        float vx = otacon::toFloat(player_.velocity.x);
        if (want != 0.f) {
            vx += (want > vx ? 1.f : -1.f) * T.accel * d;
            if ((want > 0 && vx > want) || (want < 0 && vx < want)) vx = want;
        } else {
            const float drop = T.friction * d;
            vx = vx > 0 ? std::max(0.f, vx - drop) : std::min(0.f, vx + drop);
        }

        // --- jump ------------------------------------------------------------
        float vy = otacon::toFloat(player_.velocity.y);
        bool jumped = false;
        if (bufferTimer_ > 0.f && coyoteTimer_ > 0.f) {
            vy = T.jumpVel;
            bufferTimer_ = 0.f; coyoteTimer_ = 0.f;
            jumped = true;
            jumpFlash_ = 0.18f;
        }
        // Variable height: releasing early trades the rest of the rise away.
        if (variableOn_ && jumpReleased_ && vy < 0.f) vy *= 0.42f;

        // --- gravity, with the apex and fast-fall shaping --------------------
        float g = (vy < 0.f) ? T.gravityUp : (fastFallOn_ ? T.gravityDown : T.gravityUp);
        const bool nearApex = apexOn_ && std::fabs(vy) < T.apexWindow;
        if (nearApex) g *= T.apexScale;
        vy = std::min(vy + g * d, T.maxFall);

        player_.velocity = {R(vx), R(vy)};
        player_.acceleration = {R(0), R(0)};        // this controller integrates by hand
        player_.onFloor = false;
        for (auto& s : level_) s->update(dt);
        player_.update(dt);
        otacon::collideWithGroup(player_, levelPtrs_);

        wasOnFloor_ = player_.onFloor;
        if (otacon::toFloat(player_.pos.y) > H_ + 60.f) resetPlayer();
        jumpFlash_ = std::max(0.f, jumpFlash_ - d);

        // --- timeline --------------------------------------------------------
        std::uint8_t bits = 0;
        if (player_.onFloor)      bits |= 1;
        if (coyoteTimer_ > 0.f && !player_.onFloor) bits |= 2;
        if (bufferTimer_ > 0.f)   bits |= 4;
        if (jumped)               bits |= 8;
        if (nearApex)             bits |= 16;
        tl_[head_] = bits;
        head_ = (head_ + 1) % kTimeline;
    }

    void render(otacon::IRenderer& r, const otacon::DebugRuntime&) override {
        for (auto& s : level_)
            r.fillRect(otacon::toFloat(s->pos.x), otacon::toFloat(s->pos.y),
                       otacon::toFloat(s->size.x), otacon::toFloat(s->size.y),
                       Color{0.22f, 0.26f, 0.34f, 1.f});

        // The player, squashed a little on the frame it leaves the ground.
        const float pw = otacon::toFloat(player_.size.x), ph = otacon::toFloat(player_.size.y);
        const float px = otacon::toFloat(player_.pos.x), py = otacon::toFloat(player_.pos.y);
        const float squash = jumpFlash_ > 0 ? jumpFlash_ / 0.18f : 0.f;
        r.fillRect(px - squash * 2, py + squash * 5, pw + squash * 4, ph - squash * 5,
                   player_.onFloor ? Color{0.45f, 0.85f, 0.55f, 1.f} : Color{0.45f, 0.72f, 1.f, 1.f});

        // Where the pointer is steering to.
        r.drawLine(px + pw * 0.5f, py + ph * 0.5f, px + pw * 0.5f + steer_, py + ph * 0.5f,
                   Color{1.f, 0.85f, 0.35f, 0.35f}, 1.f);

        drawToggles(r);
        drawTimeline(r);
    }

    const char* status() const override {
        std::snprintf(buf_, sizeof buf_, "%s  vy %.0f  coyote %.2f  buffer %.2f  %s",
                      kTunings[tuning_].name, otacon::toFloat(player_.velocity.y),
                      coyoteTimer_, bufferTimer_, player_.onFloor ? "grounded" : "airborne");
        return buf_;
    }
    const char* keys() const override {
        return "SPACE  jump (hold for height)\nmouse  steers left/right\n1-3    tight / floaty / heavy\nF4 coyote  F6 buffer  F7 variable\nF8 apex hang   E fast fall";
    }

private:
    void buildLevel() {
        level_.clear(); levelPtrs_.clear();
        auto add = [&](float x, float y, float w, float h) {
            auto e = std::make_unique<Entity>();
            e->pos = {R(x), R(y)}; e->size = {R(w), R(h)};
            e->fixed = true; e->solid = true; e->moves = true;
            e->refreshHulls();
            level_.push_back(std::move(e));
        };
        const float floorY = H_ - 130;
        add(0, floorY, 210, 20);
        add(250, floorY, 120, 20);            // a gap that needs coyote time
        add(410, floorY - 34, 110, 18);
        add(150, floorY - 74, 90, 14);
        add(330, floorY - 104, 96, 14);
        add(520, floorY - 86, 100, 14);
        add(0, H_ - 62, W_, 10);              // catch floor, above the timeline panel
        for (auto& s : level_) levelPtrs_.push_back(s.get());
    }
    void resetPlayer() {
        player_ = Entity{};
        player_.pos = {R(60), R(H_ - 164)};
        player_.size = {R(14), R(20)};
        player_.solid = true; player_.fixed = false; player_.moves = true;
        player_.maxVelocity = {R(4000), R(4000)};
        coyoteTimer_ = bufferTimer_ = 0.f; wasOnFloor_ = false;
    }

    void drawToggles(otacon::IRenderer& r) const {
        const float px = W_ - 150;
        float y = layout::kTop + 4;
        r.drawText("FORGIVENESS", px, y, 1.f, Color{0.55f, 0.80f, 1.f, 1.f}); y += 11;
        const struct { const char* k; const char* n; bool on; } rows[] = {
            {"F4", "coyote time",  coyoteOn_},
            {"F6", "jump buffer",  bufferOn_},
            {"F7", "variable jump",variableOn_},
            {"F8", "apex hang",    apexOn_},
            {"E ", "fast fall",    fastFallOn_},
        };
        for (const auto& row : rows) {
            char t[64];
            std::snprintf(t, sizeof t, "%s %-14s %s", row.k, row.n, row.on ? "ON" : "off");
            r.drawText(t, px, y, 1.f, row.on ? Color{0.45f, 0.90f, 0.60f, 1.f}
                                             : Color{0.55f, 0.55f, 0.60f, 1.f});
            y += 9;
        }
        y += 6;
        r.drawText("1-3 TUNING", px, y, 1.f, Color{0.55f, 0.80f, 1.f, 1.f}); y += 10;
        for (int i = 0; i < kTuningCount; ++i) {
            char t[48]; std::snprintf(t, sizeof t, "%d %s", i + 1, kTunings[i].name);
            r.drawText(t, px, y, 1.f, i == tuning_ ? Color{1.f, 0.85f, 0.35f, 1.f}
                                                   : Color{0.55f, 0.62f, 0.75f, 1.f});
            y += 9;
        }
        r.drawText("turn them all off and try", px, y + 6, 1.f, Color{0.42f, 0.48f, 0.60f, 1.f});
        r.drawText("the gap at speed", px, y + 14, 1.f, Color{0.42f, 0.48f, 0.60f, 1.f});
    }

    void drawTimeline(otacon::IRenderer& r) const {
        // Lanes need room for a 5px glyph plus a gap, or the labels collide into
        // an unreadable stack — which is exactly what 3px spacing did.
        const float laneH = 8.f, rowH = 5.f;
        const float panelH = laneH * 5 + 16;
        const float y0 = H_ - panelH;
        const float x0 = 8, w = W_ - 190;
        r.fillRect(0, y0 - 2, W_, panelH + 2, Color{0.03f, 0.04f, 0.06f, 0.90f});
        r.drawText("STATE TIMELINE", x0, y0 + 2, 1.f, Color{0.55f, 0.80f, 1.f, 1.f});
        const struct { std::uint8_t bit; const char* n; Color c; } lanes[] = {
            {1,  "ground", {0.45f, 0.85f, 0.55f, 1.f}},
            {2,  "coyote", {1.00f, 0.80f, 0.30f, 1.f}},
            {4,  "buffer", {0.55f, 0.70f, 1.00f, 1.f}},
            {16, "apex",   {0.85f, 0.55f, 1.00f, 1.f}},
            {8,  "JUMP",   {1.00f, 0.40f, 0.40f, 1.f}},
        };
        float ly = y0 + 12;
        for (const auto& lane : lanes) {
            r.drawText(lane.n, x0, ly, 1.f, lane.c);
            // The lane's track, so an empty lane still reads as a lane.
            r.fillRect(x0 + 34, ly, w, rowH, Color{0.10f, 0.12f, 0.16f, 1.f});
            for (int i = 0; i < kTimeline; ++i) {
                const std::uint8_t b = tl_[(head_ + i) % kTimeline];
                if (b & lane.bit)
                    r.fillRect(x0 + 34 + (float(i) / kTimeline) * w, ly, w / kTimeline + 0.6f, rowH, lane.c);
            }
            ly += laneH;
        }
    }

    Entity player_;
    std::vector<std::unique_ptr<Entity>> level_;
    std::vector<Entity*> levelPtrs_;
    float W_ = 640, H_ = 400;
    float steer_ = 0, coyoteTimer_ = 0, bufferTimer_ = 0, jumpFlash_ = 0;
    bool  jumpHeld_ = false, jumpPressed_ = false, jumpReleased_ = false, wasOnFloor_ = false;
    bool  coyoteOn_ = true, bufferOn_ = true, variableOn_ = true, apexOn_ = true, fastFallOn_ = true;
    int   tuning_ = 0, head_ = 0;
    std::uint8_t tl_[kTimeline]{};
    mutable char buf_[160]{};
};

} // namespace

Sample* makePlatformerController() { return new PlatformerController(); }

} // namespace samples
