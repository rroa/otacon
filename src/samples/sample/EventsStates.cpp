// EventsStates.cpp — sample 19: decoupling, made visible.
//
// Two engine headers that only pay off when a project is large enough to hurt,
// which makes them hard to demonstrate honestly. So this shows the mechanism
// rather than a toy game: guards patrol, each driven by a scene/StateMachine,
// and every transition posts to a core/Events EventBus. Three unrelated
// listeners react, and the log on the right records who heard what.
//
// The point is what is NOT in the code. No guard holds a pointer to the alarm
// counter, the log, or the audio stub. They post a Spotted and carry on. Adding
// a fourth listener would touch none of the guards -- which is the entire
// argument, and the thing a pile of direct calls cannot give you.
//
// The state machine half is the same argument in miniature: a guard is Patrol,
// Alert or Search, and it cannot be two of them. Booleans admit `alert && search`;
// a state cannot.
#include "Sample.hpp"
#include "core/Events.hpp"
#include "core/math/Random.hpp"
#include "scene/StateMachine.hpp"
#include "render/IRenderer.hpp"
#include "platform/Window.hpp"
#include "core/debug/Debug.hpp"
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace samples {
namespace {

using otacon::Color;

// ---- the events. Plain structs; the bus keys on the type itself, so there is
// ---- no string to typo and no enum to keep in sync.
struct Spotted { int guard; float x, y; };
struct LostTrack { int guard; };
struct Patrolling { int guard; };

enum class Mind { Patrol, Alert, Search };

struct Guard {
    otacon::StateMachine<Mind> fsm;
    // A Signal, not a bus post. Turning at the end of a beat is this guard's own
    // business and the listener is known at the call site -- exactly the case
    // Signal is for, where routing it through a global bus would be indirection
    // for its own sake.
    otacon::Signal<int> onTurn;
    float x = 0, y = 0, homeX = 0, dir = 1.f, speed = 40.f;
    float flash = 0.f;
};

constexpr int kGuards = 4;
constexpr int kLogLines = 11;

class EventsStates final : public Sample {
public:
    void init(SampleContext& ctx) override { W_ = float(ctx.logicalW); H_ = float(ctx.logicalH); }

    void enter() override {
        log_.clear();
        alarms_ = 0; sounds_ = 0; turns_ = 0; lastTurn_ = 0; t_ = 0;
        bus_ = otacon::EventBus{};
        wireListeners();
        buildGuards();
        note("ready - move the cursor near a guard");
    }

    void handleInput(const otacon::InputFrame& in) override {
        mouse_ = {in.mouseNx * W_, in.mouseNy * H_};
        if (in.isPressed(otacon::Action::Jump)) { enter(); }
    }

    void update(otacon::Real dt) override {
        const float d = otacon::toFloat(dt);
        if (d <= 0.f || d > 0.1f) return;
        t_ += d;
        for (Guard& g : guards_) {
            g.fsm.update(dt);
            g.flash = g.flash > 0.f ? g.flash - d : 0.f;
        }
    }

    void render(otacon::IRenderer& r, const otacon::DebugRuntime&) override {
        const float top = layout::kTop + 6;
        const float floorY = H_ - 132;
        r.fillRect(0, floorY + 20, W_ * 0.62f, 3, Color{0.22f, 0.26f, 0.34f, 1.f});

        for (std::size_t i = 0; i < guards_.size(); ++i) {
            const Guard& g = guards_[i];
            const Color c = stateColor(g.fsm.state());
            // Sight range, so "why did it fire" is never a guess.
            drawRing(r, g.x, g.y, kSight, Color{c.r, c.g, c.b, 0.16f});
            if (g.flash > 0.f)
                drawRing(r, g.x, g.y, kSight * (1.f - g.flash / kFlash) + 6.f,
                         Color{1.f, 0.85f, 0.35f, g.flash / kFlash});
            r.fillRect(g.x - 5, g.y - 9, 10, 18, c);
            const char* label = g.fsm.is(Mind::Patrol) ? "PATROL"
                              : g.fsm.is(Mind::Alert)  ? "ALERT" : "SEARCH";
            r.drawText(label, g.x - 12, g.y - 20, 1.f, c);
            char t[24]; std::snprintf(t, sizeof t, "%.1fs", g.fsm.timeInState());
            r.drawText(t, g.x - 8, g.y + 12, 1.f, Color{0.45f, 0.51f, 0.62f, 1.f});
        }

        r.drawRectOutline(mouse_.x - 4, mouse_.y - 4, 8, 8, Color{1.f, 0.85f, 0.35f, 0.9f}, 1.f);
        drawStateKey(r, 8, top);
        drawLog(r, W_ * 0.64f, top);
        drawFooter(r);
    }

    const char* status() const override {
        std::snprintf(buf_, sizeof buf_, "alarms %d  sfx %d  bus listeners: Spotted %zu",
                      alarms_, sounds_, bus_.listenerCount<Spotted>());
        return buf_;
    }
    const char* keys() const override {
        return "mouse   move near a guard to be spotted\nSPACE   reset the scene and the log";
    }

private:
    static constexpr float kSight = 54.f;
    static constexpr float kFlash = 0.45f;

    static Color stateColor(Mind m) {
        switch (m) {
            case Mind::Patrol: return {0.45f, 0.82f, 1.00f, 1.f};
            case Mind::Alert:  return {1.00f, 0.45f, 0.38f, 1.f};
            default:           return {1.00f, 0.82f, 0.35f, 1.f};
        }
    }

    /*
     * Three listeners that know nothing about guards. Adding a fourth would
     * touch none of the code that posts.
     */
    void wireListeners() {
        bus_.subscribe<Spotted>([this](const Spotted& e) {
            ++alarms_;
            char t[64]; std::snprintf(t, sizeof t, "guard %d SPOTTED you", e.guard);
            note(t);
        });
        bus_.subscribe<Spotted>([this](const Spotted&) { ++sounds_; });   // an audio stub
        bus_.subscribe<LostTrack>([this](const LostTrack& e) {
            char t[64]; std::snprintf(t, sizeof t, "guard %d lost track", e.guard);
            note(t);
        });
        bus_.subscribe<Patrolling>([this](const Patrolling& e) {
            char t[64]; std::snprintf(t, sizeof t, "guard %d back on patrol", e.guard);
            note(t);
        });
    }

    void buildGuards() {
        guards_.assign(kGuards, Guard{});
        otacon::Random rng(0xE7E7u);
        const float floorY = H_ - 132;
        for (int i = 0; i < kGuards; ++i) {
            Guard& g = guards_[std::size_t(i)];
            g.homeX = 60.f + float(i) * (W_ * 0.62f - 90.f) / float(kGuards - 1);
            g.x = g.homeX; g.y = floorY;
            g.dir = rng.chance(0.5f) ? 1.f : -1.f;
            g.speed = rng.range(28.f, 52.f);
            wireStates(i);
            // Two listeners on one signal, to show it is not merely a callback.
            g.onTurn.connect([this](int) { ++turns_; });
            g.onTurn.connect([this](int who) { lastTurn_ = who; });
            g.fsm.start(Mind::Patrol);
        }
    }

    // Each state owns its own behaviour and its own exit condition. Nothing
    // outside the machine decides when a guard changes its mind.
    void wireStates(int i) {
        Guard& g = guards_[std::size_t(i)];
        g.fsm.add(Mind::Patrol,
            [this, i] { bus_.post(Patrolling{i}); },
            [this, i](otacon::Real dt, float) {
                Guard& me = guards_[std::size_t(i)];
                me.x += me.dir * me.speed * otacon::toFloat(dt);
                if (me.x > me.homeX + 40.f) { me.x = me.homeX + 40.f; me.dir = -1.f; me.onTurn.emit(i); }
                if (me.x < me.homeX - 40.f) { me.x = me.homeX - 40.f; me.dir = 1.f; me.onTurn.emit(i); }
                if (sees(me)) me.fsm.change(Mind::Alert);
            });
        g.fsm.add(Mind::Alert,
            [this, i] {
                Guard& me = guards_[std::size_t(i)];
                me.flash = kFlash;
                bus_.post(Spotted{i, me.x, me.y});
            },
            [this, i](otacon::Real, float timeIn) {
                Guard& me = guards_[std::size_t(i)];
                if (!sees(me) && timeIn > 0.6f) me.fsm.change(Mind::Search);
            });
        g.fsm.add(Mind::Search,
            [this, i] { bus_.post(LostTrack{i}); },
            [this, i](otacon::Real, float timeIn) {
                Guard& me = guards_[std::size_t(i)];
                if (sees(me)) me.fsm.change(Mind::Alert);
                else if (timeIn > 1.6f) me.fsm.change(Mind::Patrol);
            });
    }

    bool sees(const Guard& g) const {
        const float dx = mouse_.x - g.x, dy = mouse_.y - g.y;
        return dx * dx + dy * dy < kSight * kSight;
    }

    void note(const char* text) {
        log_.push_back(text);
        if (int(log_.size()) > kLogLines) log_.erase(log_.begin());
    }

    void drawStateKey(otacon::IRenderer& r, float x, float y) const {
        r.drawText("STATE MACHINE", x, y, 1.f, Color{0.55f, 0.80f, 1.f, 1.f}); y += 11;
        const struct { const char* n; Mind m; const char* why; } rows[] = {
            {"PATROL", Mind::Patrol, "walks its beat"},
            {"ALERT",  Mind::Alert,  "posts Spotted on entry"},
            {"SEARCH", Mind::Search, "gives up after 1.6s"},
        };
        for (const auto& row : rows) {
            r.fillRect(x, y, 6, 6, stateColor(row.m));
            r.drawText(row.n, x + 10, y, 1.f, stateColor(row.m));
            r.drawText(row.why, x + 52, y, 1.f, Color{0.45f, 0.51f, 0.62f, 1.f});
            y += 10;
        }
        y += 4;
        r.drawText("a guard cannot be two of these;", x, y, 1.f, Color{0.42f, 0.48f, 0.60f, 1.f}); y += 8;
        r.drawText("booleans would admit alert && search", x, y, 1.f, Color{0.42f, 0.48f, 0.60f, 1.f});
    }

    void drawLog(otacon::IRenderer& r, float x, float y) const {
        r.drawText("EVENT BUS", x, y, 1.f, Color{0.55f, 0.80f, 1.f, 1.f}); y += 11;
        r.drawText("guards post; listeners react.", x, y, 1.f, Color{0.62f, 0.68f, 0.80f, 1.f}); y += 8;
        r.drawText("neither knows the other exists.", x, y, 1.f, Color{0.62f, 0.68f, 0.80f, 1.f}); y += 13;
        for (std::size_t i = 0; i < log_.size(); ++i) {
            const float fade = 0.35f + 0.65f * (float(i + 1) / float(log_.size()));
            r.drawText(log_[i].c_str(), x, y, 1.f, Color{0.80f, 0.86f, 0.95f, fade});
            y += 9;
        }
    }

    void drawFooter(otacon::IRenderer& r) const {
        const float y = H_ - 96;
        r.fillRect(0, y - 6, W_, H_ - y + 6, Color{0.03f, 0.04f, 0.06f, 0.92f});
        float ly = y;
        r.drawText("EventBus  -  LISTENERS ON Spotted", 8, ly, 1.f, Color{0.55f, 0.80f, 1.f, 1.f}); ly += 11;
        char t[96];
        std::snprintf(t, sizeof t, "alarm counter   fired %d times", alarms_);
        r.drawText(t, 12, ly, 1.f, Color{0.62f, 0.68f, 0.80f, 1.f}); ly += 9;
        std::snprintf(t, sizeof t, "audio stub      fired %d times", sounds_);
        r.drawText(t, 12, ly, 1.f, Color{0.62f, 0.68f, 0.80f, 1.f}); ly += 9;
        std::snprintf(t, sizeof t, "log writer      %zu lines", log_.size());
        r.drawText(t, 12, ly, 1.f, Color{0.62f, 0.68f, 0.80f, 1.f}); ly += 13;
        r.drawText("no guard holds a pointer to any of these. adding a fourth listener",
                   8, ly, 1.f, Color{0.42f, 0.48f, 0.60f, 1.f}); ly += 8;
        r.drawText("would not touch a single line of guard code - that is the whole argument.",
                   8, ly, 1.f, Color{0.42f, 0.48f, 0.60f, 1.f});

        // The other half of the header comment, demonstrated rather than claimed.
        const float sx = W_ * 0.58f;
        float sy = y;
        r.drawText("Signal  -  ONE PUBLISHER, KNOWN LISTENERS", sx, sy, 1.f,
                   Color{0.55f, 0.80f, 1.f, 1.f}); sy += 11;
        char st[96];
        std::snprintf(st, sizeof st, "guard.onTurn    %d emits", turns_);
        r.drawText(st, sx + 4, sy, 1.f, Color{0.62f, 0.68f, 0.80f, 1.f}); sy += 9;
        std::snprintf(st, sizeof st, "listeners       2 per guard");
        r.drawText(st, sx + 4, sy, 1.f, Color{0.62f, 0.68f, 0.80f, 1.f}); sy += 9;
        std::snprintf(st, sizeof st, "last turn       guard %d", lastTurn_);
        r.drawText(st, sx + 4, sy, 1.f, Color{0.62f, 0.68f, 0.80f, 1.f}); sy += 13;
        r.drawText("turning is the guard's own business and the", sx, sy, 1.f,
                   Color{0.42f, 0.48f, 0.60f, 1.f}); sy += 8;
        r.drawText("listener is known here - a global bus would be", sx, sy, 1.f,
                   Color{0.42f, 0.48f, 0.60f, 1.f}); sy += 8;
        r.drawText("indirection for its own sake.", sx, sy, 1.f, Color{0.42f, 0.48f, 0.60f, 1.f});
    }

    static void drawRing(otacon::IRenderer& r, float cx, float cy, float rad, Color c) {
        const int n = 32;
        for (int i = 0; i < n; ++i) {
            const float a0 = float(i) / n * 6.28318f, a1 = float(i + 1) / n * 6.28318f;
            r.drawLine(cx + std::cos(a0) * rad, cy + std::sin(a0) * rad,
                       cx + std::cos(a1) * rad, cy + std::sin(a1) * rad, c, 1.f);
        }
    }

    otacon::EventBus bus_;
    std::vector<Guard> guards_;
    std::vector<std::string> log_;
    otacon::Vec2f mouse_{320, 200};
    float W_ = 640, H_ = 400, t_ = 0;
    int   alarms_ = 0, sounds_ = 0, turns_ = 0, lastTurn_ = 0;
    mutable char buf_[160]{};
};

} // namespace

Sample* makeEventsStates() { return new EventsStates(); }

} // namespace samples
