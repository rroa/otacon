// Modes.cpp — the incremental demo modes (see Mode.hpp).
//
//   0 Static Box   : one box on the ground. Pure geometry + camera.
//   1 Gravity      : a box falls and lands on a platform (integrator+collision).
//   2 Run & Jump   : the real Player auto-runs on flat ground; tap to jump;
//                    camera follows. Full feel, no hazards.
//   3 Infinite Run : the ported Sequence.m generator + parallax. The core loop.
#include "canabalt/Mode.hpp"
#include "canabalt/Player.hpp"
#include "canabalt/Sequence.hpp"
#include "canabalt/Building.hpp"
#include "canabalt/Crane.hpp"
#include "canabalt/Billboard.hpp"
#include "canabalt/Decoration.hpp"
#include "canabalt/Leg.hpp"
#include "canabalt/SkyProps.hpp"
#include "canabalt/Hud.hpp"
#include "canabalt/Sounds.hpp"
#include "scene/Collision.hpp"
#include "scene/Emitter.hpp"
#include "render/IRenderer.hpp"
#include "asset/Image.hpp"
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

using namespace otacon;

namespace canabalt {
namespace {

// Logical resolution -------------------------------------------------------
constexpr int kViewW = 480, kViewH = 320;

// Palette (hex like the original) ------------------------------------------
constexpr unsigned kColGround      = 0x35353d;
constexpr unsigned kColPlayerBox   = 0xf0f0f0;
constexpr unsigned kColFallingBox  = 0xf0c020;

// Camera framing (PlayState.m) ---------------------------------------------
constexpr float kFocusAheadX   = 240.f;   // FlxG.width * 0.5 — focus half a screen ahead
constexpr float kFocusDownY    = 57.6f;   // FlxG.height * 0.18
constexpr float kFocusAirNudge = 20.f;    // extra drop while airborne
constexpr float kFollowLerp    = 15.f;
constexpr float kFollowMaxX    = 1e9f;    // effectively unbounded to the right
constexpr float kFollowMaxY    = 480.f;

// Shared box dims / physics -------------------------------------------------
constexpr float kBoxW = 16.f, kBoxH = 18.f;
constexpr float kGravity = 1200.f, kMaxFall = 360.f;
constexpr float kDeathRestartDelay = 4.0f;   // seconds dead before auto-restart
                                             // (jump/R retries immediately)

// Demo scene coordinates ----------------------------------------------------
constexpr float kDropX = 232.f, kDropY = 40.f;   // Gravity mode: where the box spawns
constexpr float kRunGroundY = 200.f;             // Run/Jump mode: flat roof height

Entity* makeBox(std::vector<std::unique_ptr<Entity>>& store,
                Real x, Real y, Real w, Real h, unsigned color, bool fixed) {
    auto e = std::make_unique<Entity>();
    e->pos = {x, y}; e->size = {w, h}; e->color = Color::rgb(color); e->fixed = fixed;
    // NOTE (flixel semantics): `fixed` means "collision won't push me"; `moves`
    // means "run updateMotion, which refreshes my collision hulls". Static
    // collidable geometry must keep moves=true (it integrates zero velocity)
    // or its hulls stay empty and nothing can land on it. They are independent.
    e->solid = true; e->moves = true;
    Entity* p = e.get();
    store.push_back(std::move(e));
    return p;
}

// Aim the camera the way PlayState.m does: at a 1x1 focus point half a screen
// AHEAD of the player (so the player rides the left side with the road visible
// ahead), nudged down a little while airborne. Crucially there is NO velocity
// lead — in the original the focus is a plain FlxObject, not a FlxSprite, so
// flixel skips the lead. Following the player *with* a 1.5x lead throws him
// off-screen at speed.
void aimCameraFocus(Entity& focus, const Player& p) {
    focus.size = {R(1), R(1)};
    focus.pos.x = p.pos.x + R(kFocusAheadX);
    focus.pos.y = p.pos.y + R(kFocusDownY) + (p.onFloor ? R(0) : R(kFocusAirNudge));
}

// Common follow-camera setup for the running modes.
void setupFollowCamera(Scene& scene, Entity& focus, const Player& player) {
    aimCameraFocus(focus, player);
    scene.camera.follow(&focus, R(kFollowLerp));
    scene.camera.followBounds(R(0), R(0), R(kFollowMaxX), R(kFollowMaxY));
    scene.camera.triggerShake();                 // startup quake (if enabled)
}

} // namespace

// ---------------------------------------------------------------------------
// Mode 0 — static box
// ---------------------------------------------------------------------------
class StaticBoxMode final : public Mode {
public:
    using Mode::Mode;
    const char* name() const override { return "STATIC BOX"; }
    void enter(Scene& scene) override {
        ents_.clear();
        scene.camera.setViewport(kViewW, kViewH);
        scene.camera.scroll = {R(0), R(0)};
        makeBox(ents_, R(0), R(240), R(kViewW), R(80), kColGround, true);     // ground
        makeBox(ents_, R((kViewW - kBoxW) / 2), R(208), R(kBoxW), R(kBoxH), kColPlayerBox, true);
        rebuild(scene);
    }
    void update(Real, const InputFrame&, Scene&) override {}
    void status(char* b, std::size_t n) const override {
        std::snprintf(b, n, "MODE 0/%d  STATIC BOX   ]-NEXT  V-VIEW  C-COLLIDERS", modeCount());
    }
private:
    void rebuild(Scene& w) { w.clear(); for (auto& e : ents_) w.add(e.get()); }
    std::vector<std::unique_ptr<Entity>> ents_;
};

// ---------------------------------------------------------------------------
// Mode 1 — gravity + collision
// ---------------------------------------------------------------------------
class GravityMode final : public Mode {
public:
    using Mode::Mode;
    const char* name() const override { return "GRAVITY"; }
    void enter(Scene& scene) override {
        ents_.clear();
        scene.camera.setViewport(kViewW, kViewH);
        scene.camera.scroll = {R(0), R(0)};
        ground_ = makeBox(ents_, R(80), R(250), R(320), R(70), kColGround, true);
        groundGroup_ = {ground_};
        box_ = makeBox(ents_, R(kDropX), R(kDropY), R(kBoxW), R(kBoxH), kColFallingBox, false);
        drop();
        // Interactive particle demo: press E to burst sparks at the cursor.
        sparks_.particleSize = {4, 4};
        sparks_.color = Color::rgb(0xf0c020);
        sparks_.minSpeed = {-160, -200}; sparks_.maxSpeed = {160, -40};
        sparks_.gravity = 500; sparks_.minRotation = -540; sparks_.maxRotation = 540;
        sparks_.init(160); sparks_.seed(0x533Au);
        rebuild(scene);
    }
    void update(Real dt, const InputFrame& in, Scene& scene) override {
        if (in.isPressed(Action::Jump) || in.isPressed(Action::Reset)) drop();
        sparks_.visible = particlesOn();           // Scene ticks + draws it
        // E (held or pressed) bursts particles at the mouse position.
        if (particlesOn() && (in.isHeld(Action::Aux6) || in.isPressed(Action::Aux6))) {
            sparks_.position = {in.mouseNx * float(kViewW), in.mouseNy * float(kViewH)};
            sparks_.area = {0, 0};
            sparks_.start(true, 18);
        }
        ground_->update(dt);           // refresh the platform's collision hulls
        box_->update(dt);
        collideWithGroup(*box_, groundGroup_);
        rebuild(scene);
    }
    void collectNodes(std::vector<Node*>&, std::vector<Node*>& fg) override { fg.push_back(&sparks_); }
    void status(char* b, std::size_t n) const override {
        std::snprintf(b, n, "MODE 1/%d  GRAVITY   SPACE-DROP   E-SPARKS(at mouse)   particles=%d",
                      modeCount(), sparks_.liveCount());
    }
private:
    void drop() {                      // (re)spawn the falling box at the top
        box_->pos = {R(kDropX), R(kDropY)};
        box_->velocity = {R(0), R(0)};
        box_->acceleration = {R(0), R(kGravity)};     // gravity only, no auto-run
        box_->maxVelocity = {R(10000), R(kMaxFall)};
        box_->onFloor = false;
    }
    void rebuild(Scene& w) { w.clear(); for (auto& e : ents_) w.add(e.get()); }
    std::vector<std::unique_ptr<Entity>> ents_;
    Entity* ground_ = nullptr;
    Entity* box_ = nullptr;
    std::vector<Entity*> groundGroup_;
    Emitter sparks_;
};

// ---------------------------------------------------------------------------
// Mode 2 — run & jump on flat ground
// ---------------------------------------------------------------------------
class RunJumpMode final : public Mode {
public:
    using Mode::Mode;
    const char* name() const override { return "RUN & JUMP"; }
    void enter(Scene& scene) override {
        ents_.clear();
        scene.camera.setViewport(kViewW, kViewH);
        // One very long flat roof to run along.
        ground_ = makeBox(ents_, R(-200), R(kRunGroundY), R(200000), R(380), kColGround, true);
        groundGroup_ = {ground_};
        player_ = std::make_unique<Player>();
        player_->reset(R(40), R(kRunGroundY - kBoxH - 4));
        setupFollowCamera(scene, focus_, *player_);
        rebuild(scene);
    }
    void update(Real dt, const InputFrame& in, Scene& scene) override {
        player_->touching = in.isHeld(Action::Jump);
        if (in.isPressed(Action::Reset)) player_->reset(R(40), R(kRunGroundY - kBoxH - 4));
        ground_->update(dt);           // refresh the ground's collision hulls
        player_->update(dt);
        collideWithGroup(*player_, groundGroup_);
        aimCameraFocus(focus_, *player_);
        scene.camera.doFollow(dt);
        rebuild(scene);
    }
    void status(char* b, std::size_t n) const override {
        std::snprintf(b, n, "MODE 2/%d  RUN & JUMP   HOLD-SPACE-JUMP  v=%d  %dM", modeCount(),
                      int(toFloat(player_->velocity.x)), int(toFloat(player_->pos.x) / 10));
    }
    Player* spriteTarget() override { return player_.get(); }
private:
    void rebuild(Scene& w) { w.clear(); for (auto& e : ents_) w.add(e.get()); w.add(player_.get()); }
    std::vector<std::unique_ptr<Entity>> ents_;
    std::unique_ptr<Player> player_;
    Entity  focus_;                 // camera follow target (not drawn)
    Entity* ground_ = nullptr;
    std::vector<Entity*> groundGroup_;
};

// Draws the live city by dispatching each sequence piece to the right visual:
// building facade, crane, billboard, etc. — the same per-type switch the
// original generator uses. The mode toggles its visibility with the building
// reveal level (the plain geometry boxes show when it is hidden).
class SequenceVisualsNode final : public Node {
public:
    SequenceField* field = nullptr;
    Building*   building = nullptr;
    Crane*      crane = nullptr;
    Billboard*  billboard = nullptr;
    Decoration* decoration = nullptr;
    Leg*        leg = nullptr;
    TextureHandle bombTex = 0;

    void render(IRenderer& r, const Camera& cam) const override {
        if (!field || !building) return;
        std::vector<SequenceField::Piece> pieces;
        field->appendPieces(pieces);
        for (const auto& p : pieces) {
            // Each building picks its own roof cap from the seed (6 styles), so the
            // rooftops vary down the skyline instead of all sharing roof1. Hallways
            // likewise pick one of the 2 floor caps.
            const int roofType  = int((p.seed >> 4) % 6u);
            const int floorType = int((p.seed >> 10) % 2u);
            switch (p.type) {
                case CRANE:
                    if (crane && crane->loaded()) { crane->draw(r, cam, p.x, p.y, p.w, p.h, p.seed); continue; }
                    break;   // fall back to a plain facade if the art is missing
                case BILLBOARD:
                    if (billboard && billboard->loaded()) { billboard->draw(r, cam, p.x, p.y, p.w, p.h, p.aux, p.seed); continue; }
                    break;
                case HALLWAY: {
                    // The building front below the floor (with a floor cap, not a
                    // roof — it's a hallway), then the tunnel over it.
                    building->draw(r, cam, p.x, p.y, p.w, p.h, p.wallType, p.windowType, true, false, false, roofType, floorType);
                    building->drawHall(r, cam, p.x, p.y, p.w, p.aux, p.wallType, p.windowType,
                                       p.seed, !p.launch);
                    // Glass panes still covering the unsmashed ends of the opening.
                    Vec2f s0 = cam.screenPoint({R(p.x), R(p.y)}, {1, 1});
                    const float T = 16.f, hh = p.aux;
                    Color glass{0.70f, 0.82f, 0.92f, 0.30f};
                    if (!p.brokeL) r.fillRect(s0.x, s0.y - hh, T, hh, glass);
                    if (!p.brokeR) r.fillRect(s0.x + p.w - T, s0.y - hh, T, hh, glass);
                    continue;
                }
                case LEG:
                    building->draw(r, cam, p.x, p.y, p.w, p.h, p.wallType, p.windowType, false, false, p.escape, roofType);
                    if (decoration) decoration->draw(r, cam, p.x, p.y, p.w, p.seed);
                    if (leg && leg->loaded()) leg->draw(r, cam, p.x + p.w * 0.5f - 64.f, p.legY);
                    continue;
                case BOMB:
                    building->draw(r, cam, p.x, p.y, p.w, p.h, p.wallType, p.windowType, false, false, p.escape, roofType);
                    if (decoration) decoration->draw(r, cam, p.x, p.y, p.w, p.seed);
                    if (bombTex) {
                        Vec2f bs = cam.screenPoint({R(p.x + p.w * 0.5f - 20.f), R(p.bombY)}, {1, 1});
                        r.drawImage(bombTex, bs.x, bs.y, 40, 80);
                    }
                    continue;
                default: break;
            }
            building->draw(r, cam, p.x, p.y, p.w, p.h, p.wallType, p.windowType, false, false, p.escape, roofType);
            // Flat roofs (ROOF/COLLAPSE/BOMB) get scattered rooftop props.
            if (decoration) decoration->draw(r, cam, p.x, p.y, p.w, p.seed);
        }
    }
};

// Draws the small roof obstacles (in front of the player; knocked ones tumble).
class ObstaclesNode final : public Node {
public:
    SequenceField* field = nullptr;
    TextureHandle  tex = 0, tex2 = 0;       // obstacles.png (4), obstacles2.png (2)
    void render(IRenderer& r, const Camera& cam) const override {
        if (!field) return;
        std::vector<SequenceField::ObView> obs;
        field->appendObstacles(obs);
        for (const auto& o : obs) {
            TextureHandle t = o.alt ? tex2 : tex;
            if (!t) continue;
            const int frames = o.alt ? 2 : 4;
            const float fw = 1.f / float(frames);
            const float u0 = float(o.frame % frames) * fw;
            Vec2f s = cam.screenPoint({R(o.x), R(o.y)}, {1, 1});
            if (o.angle != 0.f)
                r.drawImageRotated(t, s.x + 9, s.y + 9, 18, 18, o.angle, u0, 0, u0 + fw, 1);
            else
                r.drawImage(t, s.x, s.y, 18, 18, u0, 0, u0 + fw, 1);
        }
    }
    // Obstacles collide via a hand-rolled 18x18 overlap test (not a scene Entity),
    // so they outline themselves for the C view. Knocked-away ones no longer hit.
    void renderColliders(IRenderer& r, const Camera& cam) const override {
        if (!field) return;
        std::vector<SequenceField::ObView> obs;
        field->appendObstacles(obs);
        const Color green{0.2f, 1.f, 0.3f, 1.f};
        for (const auto& o : obs) {
            if (o.knocked) continue;
            Vec2f s = cam.screenPoint({R(o.x), R(o.y)}, {1, 1});
            r.drawRectOutline(s.x, s.y, 18, 18, green, 1.f);
        }
    }
};

// Draws the pigeons: sitting (idle frame) until they flush, then flapping away.
class DovesNode final : public Node {
public:
    SequenceField* field = nullptr;
    TextureHandle  tex = 0;          // dove.png (4 frames of 10x10)
    void render(IRenderer& r, const Camera& cam) const override {
        if (!field || !tex) return;
        std::vector<SequenceField::DoveView> ds;
        field->appendDoves(ds);
        for (const auto& d : ds) {
            int frame = d.flying ? int(d.anim * 15.f) % 3 : 3;   // idle = frame 3
            float u0 = float(frame) * 0.25f, u1 = u0 + 0.25f;
            if (d.facing) { float t = u0; u0 = u1; u1 = t; }      // flip horizontally
            Vec2f s = cam.screenPoint({R(d.x), R(d.y)}, {1, 1});
            r.drawImage(tex, s.x, s.y, 10, 10, u0, 0, u1, 1);
        }
    }
};

// ---------------------------------------------------------------------------
// Mode 3 — infinite rooftops + parallax
// ---------------------------------------------------------------------------
class InfiniteMode final : public Mode {
public:
    using Mode::Mode;
    const char* name() const override { return "INFINITE RUN"; }

    void enter(Scene& scene) override {
        scene.camera.setViewport(kViewW, kViewH);
        player_ = std::make_unique<Player>();
        field_.init(player_.get(), gameRng_);     // ported Sequence.m generator (reseeds rng)
        buildParallax();
        player_->reset(R(0), R(80 - 14));         // Player.m spawn; first roof at y=80
        setupFollowCamera(scene, focus_, *player_);
        configureEmitters();
        visuals_.field = &field_;
        visuals_.building = &buildings_;
        visuals_.crane = &crane_;
        visuals_.billboard = &billboard_;
        visuals_.decoration = &deco_;
        visuals_.leg = &leg_;
        visuals_.bombTex = bombTex_;
        obstaclesNode_.field = &field_;
        obstaclesNode_.tex = obTex_;
        obstaclesNode_.tex2 = obTex2_;
        dovesNode_.field = &field_;
        dovesNode_.tex = doveTex_;
        // Sky easter eggs: a jet that streaks past (and shakes the screen) and two
        // marching walkers, plus a girder that sweeps the foreground.
        jet_.useCamera(&scene.camera);
        walkerA_.place(-300.f, 1);
        walkerB_.place(220.f, 2);
        // First time into the mode we open on the title; death-restarts skip it.
        titleActive_ = !seenTitle_;
        title_.visible = titleActive_;
        hud_.visible = !titleActive_; hud_.setDistance(0);
        gameOver_.hide();
        deadTimer_ = R(0); wasDead_ = false;
        footTimer_ = 0.f; prevOnFloor_ = false; prevStumble_ = false;
        if (titleActive_) sounds_.playTitleMusic(); else sounds_.playMusic();
        rebuild(scene);
    }

    void update(Real dt, const InputFrame& in, Scene& scene) override {
        player_->touching = in.isHeld(Action::Jump);
        if (in.isPressed(Action::CycleMusic)) sounds_.nextTrack();   // F10: next track
        // Title screen: hold the world frozen until the first jump kicks off the run.
        if (titleActive_) {
            if (in.isPressed(Action::Jump)) {
                titleActive_ = false; seenTitle_ = true;
                title_.visible = false; hud_.visible = true;
                sounds_.crumble();                   // the city's opening rumble
                sounds_.playMusic();                 // swap title theme -> run theme
            }
            return;
        }
        // Once dead: clicking EXIT (top-right) returns to the title; a jump (left
        // click counts) or R anywhere else starts a fresh run immediately.
        if (player_->dead_) {
            const float mx = in.mouseNx * float(kViewW), my = in.mouseNy * float(kViewH);
            gameOver_.setMouse(mx, my);
            if (in.isPressed(Action::Jump) && gameOver_.exitHit(mx, my)) {
                seenTitle_ = false; restart(scene); return;   // back to the title
            }
            if (in.isPressed(Action::Reset) || in.isPressed(Action::Jump)) { restart(scene); return; }
        } else if (in.isPressed(Action::Reset)) { restart(scene); return; }

        player_->update(dt);
        field_.update(dt, scene.camera);          // generate/recycle buildings, refresh hulls
        collideWithGroup(*player_, field_.collisionBlocks());

        // Footsteps + a jump sound the instant the player leaves the roof heading
        // up. Cadence is Player.m's: ft = max(0.15, (1 - vx/maxVx)*0.35); a crane
        // beam underfoot rings out the metal (footc) instead of the roof (foot).
        const float fdt = toFloat(dt);
        if (player_->onFloor && !player_->dead_) {
            footTimer_ += fdt;
            const float vx = toFloat(player_->velocity.x), maxVx = toFloat(player_->maxVelocity.x);
            float ft = (1.f - vx / maxVx) * 0.35f;
            if (ft < 0.15f) ft = 0.15f;
            if (footTimer_ >= ft) {
                footTimer_ = 0.f;
                if (field_.onCrane()) sounds_.footConcrete(); else sounds_.footstep();
            }
        } else {
            footTimer_ = 0.f;
        }
        if (prevOnFloor_ && !player_->onFloor && toFloat(player_->velocity.y) < -50.f)
            sounds_.jump();
        prevOnFloor_ = player_->onFloor;
        // Tumble on a hard landing (rising edge — `stumble` can linger across
        // frames if a new one lands mid-roll). Obstacle clips get their own clink
        // below, so don't double them up with a tumble here.
        if (player_->stumble && !prevStumble_ && field_.obstacleHits().empty()) sounds_.tumble();
        prevStumble_ = player_->stumble;

        aimCameraFocus(focus_, *player_);
        scene.camera.doFollow(dt);
        recycleParallax(scene);

        // Emitters are scene nodes now: the Scene ticks and draws them. Here we
        // just steer them — set positions and fire bursts — gated on F7.
        const bool pOn = particlesOn();
        smoke_.visible = gibs_.visible = glass_.visible = pOn;
        if (pOn) {
            // Ambient city smoke drifts up across the top of the view. The
            // emitter spawn band is kept pinned to the screen top, but each puff
            // gets the smoke's tiny parallax factor, so it hangs in the distant
            // skyline haze instead of rushing past in the play plane.
            smoke_.position = {-toFloat(scene.camera.scroll.x) * 0.1f,
                               -toFloat(scene.camera.scroll.y) * 0.05f + 40.f};
            smoke_.area = {float(kViewW), 28.f};

            // Burst of debris the moment the player dies.
            if (player_->dead_ && !wasDead_) {
                gibs_.position = {toFloat(player_->pos.x), toFloat(player_->pos.y)};
                gibs_.area = {toFloat(player_->size.x), toFloat(player_->size.y)};
                gibs_.start(true, 40);
            }

            // Smash glass shards out of each hallway window the player breaks.
            for (float sx : field_.windowSmashes()) {
                float vx = toFloat(player_->velocity.x);
                glass_.minSpeed = {vx * 0.4f, -200.f};
                glass_.maxSpeed = {vx * 1.1f, 120.f};
                glass_.position = {sx, toFloat(player_->pos.y)};
                glass_.area = {4, 16};
                glass_.start(true, 16);
            }
        }
        // Giant-leg stomps, building collapses, and bomb landings all shake the
        // camera (and kick up debris when particles are on).
        auto shakeAndBurst = [&](const std::vector<float>& xs, float halfW, int n) {
            for (float sx : xs) {
                scene.camera.triggerShake();
                if (pOn) {
                    gibs_.position = {sx - halfW, toFloat(player_->pos.y)};
                    gibs_.area = {halfW * 2.f, 8.f};
                    gibs_.start(true, n);
                }
            }
        };
        shakeAndBurst(field_.legStomps(), 60.f, 30);
        shakeAndBurst(field_.collapses(), 40.f, 24);
        shakeAndBurst(field_.bombLandings(), 16.f, 20);

        // The matching one-shot SFX (independent of the particle toggle). A window
        // smash is the crack (window) plus the shards' tinkle (glass).
        for (std::size_t i = field_.windowSmashes().size(); i--; ) { sounds_.windowSmash(); sounds_.glass(); }
        for (std::size_t i = field_.legStomps().size();     i--; ) sounds_.legStomp();
        for (std::size_t i = field_.collapses().size();     i--; ) sounds_.crumble();
        for (std::size_t i = field_.bombLandings().size();  i--; ) sounds_.bombExplode();
        for (std::size_t i = field_.bombDrops().size();     i--; ) sounds_.bombPre();
        for (std::size_t i = field_.legDrops().size();      i--; ) sounds_.legLaunch();
        for (std::size_t i = field_.obstacleHits().size();  i--; ) sounds_.obstacle();
        for (std::size_t i = field_.doveFlushes().size();   i--; ) sounds_.flap();
        if (jet_.consumeFired()) sounds_.flyby();

        // Game-flow UI: the live metres counter, then the death overlay the moment
        // the player dies (which also hides the counter).
        const int metres = int(toFloat(player_->pos.x) / 10);
        hud_.setDistance(metres);
        if (player_->dead_ && !wasDead_) {
            hud_.visible = false;
            if (player_->crashed()) sounds_.wall();   // smacked a wall (a fall is silent)
            const bool record = metres > bestDistance_;
            if (record) { bestDistance_ = metres; saveHighScore(); }   // persist a new best
            gameOver_.show(metres, player_->epitaph, record);
        }
        wasDead_ = player_->dead_;

        if (player_->dead_) {
            deadTimer_ += dt;
            if (deadTimer_ > R(kDeathRestartDelay)) restart(scene);
        }
        applyReveal();
        rebuild(scene);
    }

    // Switch buildings + background between geometry and sprites per the F6/F8
    // reveal level. Buildings tile the wall texture via REPEAT UVs (one draw each).
    void applyReveal() {
        for (Entity* e : wrapSprites_) e->renderable = spriteBackground();
        // The sky/foreground easter eggs share the background reveal level.
        for (Entity* e : eggSprites_) e->renderable = spriteBackground();
        jet_.visible = walkerA_.visible = walkerB_.visible = spriteBackground();
        girder_.visible = spriteBackground();
        // In building-sprite mode the FacadeNode draws the buildings, so the
        // plain collision boxes are hidden (the Scene stops drawing them).
        const bool bsp = spriteBuildings() && buildings_.loaded();
        visuals_.visible = bsp;
        obstaclesNode_.visible = bsp;
        dovesNode_.visible = bsp;
        std::vector<Entity*> blocks;
        field_.appendRenderBlocks(blocks);
        for (Entity* b : blocks) b->renderable = !bsp;
        // Top reveal level swaps particle quads for their sprite frames.
        const bool sp = spriteParticles();
        smoke_.texture = sp ? smokeTex_ : 0;
        gibs_.texture  = sp ? gibTex_  : 0;
        glass_.texture = sp ? glassTex_ : 0;
        walkerA_.setSmokeTexture(sp ? smokeTex_ : 0);   // the mechs' muzzle plume too
        walkerB_.setSmokeTexture(sp ? smokeTex_ : 0);
    }

    // Buildings draw behind the player; particles spray in front.
    // The walkers are the farthest props (distant mechs) — they belong behind the
    // skyline and midground, so they render in the backdrop pass, not with the
    // other sky nodes.
    void collectBackdrop(std::vector<Node*>& backdrop) override {
        backdrop.push_back(&walkerA_);
        backdrop.push_back(&walkerB_);
    }

    void collectNodes(std::vector<Node*>& bg, std::vector<Node*>& fg) override {
        // The jet flies in front of the midground (behind the buildings); the
        // girder sweeps in front of everything.
        bg.push_back(&jet_);
        bg.push_back(&visuals_);
        bg.push_back(&smoke_);
        fg.push_back(&obstaclesNode_);
        fg.push_back(&dovesNode_);
        fg.push_back(&glass_);
        fg.push_back(&gibs_);
        fg.push_back(&girder_);
        fg.push_back(&hud_);        // UI sits on top of everything
        fg.push_back(&gameOver_);
        fg.push_back(&title_);      // the start screen covers all
    }

    void status(char* b, std::size_t n) const override {
        std::snprintf(b, n, "MODE 3/%d  INFINITE   %dM  v=%d  %s", modeCount(),
                      int(toFloat(player_->pos.x) / 10), int(toFloat(player_->velocity.x)),
                      player_->dead_ ? (player_->crashed() ? "CRASHED!" : "FELL!") : "RUN!");
    }
    Player* spriteTarget() override { return player_.get(); }

private:
    // High score persisted to a small text file beside the assets, so your best
    // run survives between launches (the in-session best lives in bestDistance_).
    std::string scorePath() const { return std::string(assetDir_) + "/highscore.dat"; }
    void loadHighScore() {
        if (std::FILE* f = std::fopen(scorePath().c_str(), "r")) {
            int v = 0;
            if (std::fscanf(f, "%d", &v) == 1 && v > bestDistance_) bestDistance_ = v;
            std::fclose(f);
        }
    }
    void saveHighScore() const {
        if (std::FILE* f = std::fopen(scorePath().c_str(), "w")) {
            std::fprintf(f, "%d\n", bestDistance_);
            std::fclose(f);
        }
    }

    // Load the world art once (raw PNGs via the in-house decoder).
    void loadTextures() {
        if (bgTex_) return;
        auto load = [&](const char* f, bool repeat = false) -> TextureHandle {
            return resources_ ? resources_->texture(std::string(assetDir_) + "/images/raw/" + f, repeat) : 0;
        };
        bgTex_   = load("background-trimmed.png");   // 480x48
        mg1Tex_  = load("midground1-trimmed.png");   // 480x97
        mg2Tex_  = load("midground2-trimmed.png");
        shipTex_ = load("mothership-filled.png");    // 240x40 distant easter egg
        towerTex_= load("dark_tower-filled.png");    // 128x128 distant easter egg
        jet_.load(resources_, assetDir_);             // streaking jet
        walkerA_.load(resources_, assetDir_);         // marching mechs
        walkerB_.load(resources_, assetDir_);
        girder_.load(resources_, assetDir_);          // foreground girder sweep
        hud_.load(resources_, assetDir_);             // distance odometer
        gameOver_.load(resources_, assetDir_);        // death overlay
        title_.load(resources_, assetDir_);           // start screen
        sounds_.load(audio_, assetDir_);             // SFX bank (in-house CAF decode)
        loadHighScore();                             // best distance from a prior session
        buildings_.load(resources_, assetDir_);       // wall/window/roof tiles
        crane_.load(resources_, assetDir_);           // crane pieces
        billboard_.load(resources_, assetDir_);       // billboard pieces
        deco_.load(resources_, assetDir_);            // rooftop props
        leg_.load(resources_, assetDir_);             // giant leg
        // Particle sheets (shown when the reveal reaches the particle level).
        smokeTex_ = load("smoke.png");               // 4 x 32px puffs
        gibTex_   = load("demo_gibs.png");           // 6 x 20px chunks
        doveTex_  = load("dove.png");                // 4 x 10px birds
        glassTex_ = load("glass.png");               // 48 x 8px shards
        bombTex_  = load("bomb.png");                // 40 x 80 bomb
        obTex_    = load("obstacles.png");           // 4 x 18px hurdles
        obTex2_   = load("obstacles2.png");          // 2 x 18px hurdles
    }

    Entity* addBG(TextureHandle tex, float x, float y, float w, float h, Vec2f sf) {
        auto e = std::make_unique<Entity>();
        e->pos = {R(x), R(y)}; e->size = {R(w), R(h)};
        e->scrollFactor = sf; e->texture = tex; e->solid = false; e->moves = false;
        Entity* p = e.get(); parallaxStore_.push_back(std::move(e));
        return p;
    }
    void addBand(float y, float h, unsigned color, Vec2f sf) {
        auto e = std::make_unique<Entity>();
        e->pos = {R(0), R(y)}; e->size = {R(kViewW), R(h)};
        e->scrollFactor = sf; e->color = Color::rgb(color); e->solid = false; e->moves = false;
        parallaxStore_.push_back(std::move(e));
    }

    // Faithful PlayState layout: two wrapping background strips + a solid fill
    // band, then two midground strips + a solid band (back -> front).
    void buildParallax() {
        loadTextures();
        parallaxStore_.clear();
        wrapSprites_.clear();
        eggSprites_.clear();
        // Distant easter eggs sit furthest back (tiny x scroll factor) so they
        // barely creep as the city races by. They share the skyline's *vertical*
        // parallax (0.25) and sit at its level, so the mothership hovers over the
        // horizon and the dark tower rises out of it — instead of floating pinned
        // to the screen top. Added first => drawn behind the sky strips, so each
        // emerges from the silhouette.
        eggSprites_.push_back(addBG(shipTex_,  900,  20, 240,  40, {0.015f, 0.25f}));
        eggSprites_.push_back(addBG(towerTex_, 1700, -8, 128, 128, {0.015f, 0.25f}));
        wrapSprites_.push_back(addBG(bgTex_, 0,   66, 480, 48, {0.15f, 0.25f}));
        wrapSprites_.push_back(addBG(bgTex_, 480, 66, 480, 48, {0.15f, 0.25f}));
        addBand(114, 88, 0x868696, {0.f, 0.25f});
        wrapSprites_.push_back(addBG(mg1Tex_, 0,   112, 480, 97, {0.4f, 0.5f}));
        wrapSprites_.push_back(addBG(mg2Tex_, 480, 112, 480, 97, {0.4f, 0.5f}));
        addBand(209, 223, 0x646A7D, {0.f, 0.5f});
    }
    // Background strips wrap by 2*width once off the left (flixel BG.update).
    void recycleParallax(Scene& scene) {
        for (Entity* e : wrapSprites_) {
            Vec2f s = scene.camera.screen(*e);
            if (s.x + toFloat(e->size.x) < 0.f) e->pos.x += e->size.x * R(2);
        }
    }

    void configureEmitters() {
        // Smoke: slow upward drift, no gravity, gentle spin (PlayState values).
        smoke_.particleSize = {18, 18};
        smoke_.color = Color{0.72f, 0.72f, 0.76f, 0.22f};
        smoke_.minSpeed = {-3, -20}; smoke_.maxSpeed = {3, -10};
        smoke_.gravity = 0; smoke_.minRotation = -30; smoke_.maxRotation = 30;
        smoke_.delay = 0.3f; smoke_.scrollFactor = {0.1f, 0.05f};   // distant haze (PlayState)
        smoke_.init(28); smoke_.seed(0x5305u); smoke_.start(false);
        // Gibs: fast debris burst with spin under gravity (Sequence collapse values).
        gibs_.particleSize = {5, 5};
        gibs_.color = Color::rgb(0x4d4d59);
        gibs_.minSpeed = {-200, -120}; gibs_.maxSpeed = {200, 0};
        gibs_.gravity = 400; gibs_.minRotation = -720; gibs_.maxRotation = 720;
        gibs_.scrollFactor = {1, 1};
        gibs_.init(50); gibs_.seed(0x91B5u);
        // Glass shards: small white bits that spray forward when a window breaks
        // (their speed is set from the player's speed at the moment of the smash).
        glass_.particleSize = {3, 3};
        glass_.color = Color{0.95f, 0.97f, 1.f, 1.f};
        glass_.gravity = 500; glass_.minRotation = -45; glass_.maxRotation = 45;
        glass_.scrollFactor = {1, 1};
        glass_.init(80); glass_.seed(0x61A5u);
        // Sprite-sheet layouts for the textured (revealed) particles. The texture
        // handle itself is switched on/off by the reveal level in applyReveal().
        smoke_.spriteSize = {32, 32}; smoke_.frameCols = 4;
        gibs_.spriteSize  = {20, 20}; gibs_.frameCols  = 6;
        glass_.spriteSize = {8, 8};   glass_.frameCols = 48;
    }

    void restart(Scene& scene) { enter(scene); }
    void rebuild(Scene& w) {
        w.clear();
        for (auto& e : parallaxStore_) w.add(e.get());
        std::vector<Entity*> blocks;
        field_.appendRenderBlocks(blocks);
        for (Entity* b : blocks) w.add(b);
        w.add(player_.get());
    }

    SequenceField field_;           // the ported level generator
    std::vector<std::unique_ptr<Entity>> parallaxStore_;
    std::vector<Entity*> wrapSprites_;   // background strips that wrap horizontally
    std::vector<Entity*> eggSprites_;    // distant non-wrapping easter eggs
    TextureHandle bgTex_ = 0, mg1Tex_ = 0, mg2Tex_ = 0;
    TextureHandle shipTex_ = 0, towerTex_ = 0;   // mothership + dark tower
    TextureHandle smokeTex_ = 0, gibTex_ = 0, doveTex_ = 0, glassTex_ = 0, bombTex_ = 0;
    JetNode jet_;                   // periodic jet flyby
    WalkerNode walkerA_, walkerB_;  // marching mechs
    GirderNode girder_;             // foreground girder sweep
    HudNode hud_;                   // metres-run odometer
    GameOverNode gameOver_;         // death overlay
    TitleNode title_;               // start screen
    bool titleActive_ = false;      // showing the title (sim frozen)
    bool seenTitle_ = false;        // first run started? (survives death-restarts)
    Sounds sounds_;                 // SFX bank (loaded once)
    float footTimer_ = 0.f;         // footstep cadence accumulator
    bool  prevOnFloor_ = false;     // for detecting jumps (leaving the ground up)
    bool  prevStumble_ = false;     // rising-edge detector for the tumble sound
    Building buildings_;                  // building-facade renderer
    Crane    crane_;                      // crane variation renderer
    Billboard billboard_;                 // billboard variation renderer
    Decoration deco_;                     // rooftop prop scatter
    Leg      leg_;                        // giant leg renderer
    TextureHandle obTex_ = 0, obTex2_ = 0; // roof obstacle sheets
    SequenceVisualsNode visuals_;         // per-type dispatch into the scene tree
    ObstaclesNode obstaclesNode_;         // roof obstacles (foreground)
    std::unique_ptr<Player> player_;
    Entity  focus_;                 // camera follow target (not drawn)
    Emitter smoke_, gibs_, glass_;  // smoke, crash debris, window glass
    DovesNode dovesNode_;           // pigeons that sit + flush (foreground)
    bool    wasDead_ = false;
    int     bestDistance_ = 0;       // session best (survives death-restarts)
    Real deadTimer_ = R(0);

public:
    ~InfiniteMode() override {
        // Every texture above is owned by the engine's Resources cache, which
        // releases each exactly once after the game shuts down. The destroy()
        // calls below are now no-ops kept for symmetry with load().
        if (renderer_) {
            buildings_.destroy(renderer_);
            crane_.destroy(renderer_);
            billboard_.destroy(renderer_);
            deco_.destroy(renderer_);
            leg_.destroy(renderer_);
            jet_.destroy(renderer_);
            walkerA_.destroy(renderer_);
            walkerB_.destroy(renderer_);
            girder_.destroy(renderer_);
            hud_.destroy(renderer_);
            gameOver_.destroy(renderer_);
            title_.destroy(renderer_);
        }
    }
};

// ---------------------------------------------------------------------------
int modeCount() { return 4; }
Mode* makeMode(int index, const ModeServices& s) {
    switch (index) {
        case 0: return new StaticBoxMode(s);
        case 1: return new GravityMode(s);
        case 2: return new RunJumpMode(s);
        default: return new InfiniteMode(s);
    }
}

} // namespace canabalt
