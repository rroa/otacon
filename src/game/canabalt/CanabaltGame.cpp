#include "canabalt/CanabaltGame.hpp"
#include "canabalt/Player.hpp"
#include "render/IRenderer.hpp"
#include "audio/IAudio.hpp"
#include "asset/Image.hpp"
#include <algorithm>
#include <cstdio>
#include <string>

using namespace otacon;

namespace canabalt {

static const char* kModeLabels[] = { "1 BOX", "2 GRAVITY", "3 RUN+JUMP", "4 INFINITE" };

void CanabaltGame::init(GameContext& ctx) {
    logicalW_ = ctx.logicalW; logicalH_ = ctx.logicalH;
    renderer_ = ctx.renderer;
    resources_ = ctx.resources; assetDir_ = ctx.assetDir; audio_ = ctx.audio;
    scene_.camera.setViewport(ctx.logicalW, ctx.logicalH);
    // The player sprite is a scene node that owns its own sheet (F6 sprite path).
    playerSprite_.load(resources_, assetDir_);
    // Game-flow chrome: the PAUSED graphic and a software cursor.
    auto loadTex = [&](const char* f, int& w, int& h) -> TextureHandle {
        if (!resources_) return 0;
        const std::string path = std::string(assetDir_) + "/images/raw/" + f;
        const TextureHandle t = resources_->texture(path);
        resources_->size(path, w, h);
        return t;
    };
    pausedTex_ = loadTex("paused.png", pausedW_, pausedH_);
    cursorTex_ = loadTex("cursor.png", cursorW_, cursorH_);
    setMode(startMode_);
}

void CanabaltGame::shutdown() {
    scene_.clearNodes();
    playerSprite_.destroy(renderer_);
    if (renderer_) {
        // Cache-owned; released once by the engine after shutdown.
    }
    pausedTex_ = cursorTex_ = 0;
    mode_.reset();
}

void CanabaltGame::applyQuake(bool retrigger) {
    scene_.camera.setShakeConfig(quakeEnabled_, R(quakeIntensity_), R(2.5f));
    if (!quakeEnabled_) scene_.camera.stopShake();
    else if (retrigger) scene_.camera.triggerShake();
}

void CanabaltGame::setMode(int index) {
    modeIndex_ = (index % modeCount() + modeCount()) % modeCount();
    grabbed_ = nullptr;
    applyQuake(false);                          // config in place before the mode triggers it
    scene_.clearNodes();                        // drop the old mode's nodes before it dies
    ModeServices services{rng_, renderer_, resources_, assetDir_, audio_, &particlesEnabled_, &reveal_};
    mode_.reset(makeMode(modeIndex_, services));
    mode_->enter(scene_);                        // builds entities + configures nodes

    // Assemble the scene tree: the mode's background nodes, then the player
    // sprite, then the mode's foreground nodes — so the player sits between the
    // buildings and the particles.
    std::vector<Node*> backdrop, bg, fg;
    mode_->collectBackdrop(backdrop);
    for (Node* n : backdrop) scene_.addBackdropNode(n);    // behind the parallax entities
    mode_->collectNodes(bg, fg);
    for (Node* n : bg) scene_.addNode(n);
    playerSprite_.setTarget(mode_->spriteTarget());
    scene_.addNode(&playerSprite_);
    for (Node* n : fg) scene_.addNode(n);
}

void CanabaltGame::handleInput(const InputFrame& in) {
    in_ = &in;
    if (in.selectSlot >= 0 && in.selectSlot < modeCount()) setMode(in.selectSlot);
    if (in.isPressed(Action::NextMode)) setMode(modeIndex_ + 1);
    if (in.isPressed(Action::PrevMode)) setMode(modeIndex_ - 1);

    // F1: flip the RNG between deterministic and entropy-seeded, then rebuild
    // the level so the class can see the difference immediately.
    if (in.isPressed(Action::Aux1)) {
        rng_.setMode(rng_.mode() == GameRandom::Mode::Deterministic
                         ? GameRandom::Mode::Nondeterministic
                         : GameRandom::Mode::Deterministic);
        setMode(modeIndex_);
    }
    // F2/F3/F4: quake on-off / weaker / stronger (previewed live).
    if (in.isPressed(Action::Aux2)) { quakeEnabled_ = !quakeEnabled_; applyQuake(true); }
    if (in.isPressed(Action::Aux3)) { quakeIntensity_ = quakeIntensity_ > 0.002f ? quakeIntensity_ - 0.002f : 0.f; applyQuake(true); }
    if (in.isPressed(Action::Aux4)) { quakeIntensity_ += 0.002f; applyQuake(true); }
    // F6: cycle the sprite-reveal level up by one (geometry -> ... -> full).
    if (in.isPressed(Action::Aux5)) reveal_.level = (reveal_.level + 1) % (SpriteReveal::kMax + 1);
    // F8: jump between all-geometry and all-sprites.
    if (in.isPressed(Action::Aux8)) reveal_.level = (reveal_.level == 0) ? SpriteReveal::kMax : 0;
    // F7: global particles on/off (independent of the mode).
    if (in.isPressed(Action::Aux7)) particlesEnabled_ = !particlesEnabled_;
    // M: show/hide the software mouse cursor (independent of the clean-play toggle).
    if (in.isPressed(Action::ToggleCursor)) cursorVisible_ = !cursorVisible_;
    // F9: mute/unmute all audio (master volume 0 silences SFX and music alike).
    if (in.isPressed(Action::ToggleSound) && audio_) {
        soundOn_ = !soundOn_;
        audio_->setMasterVolume(soundOn_ ? 1.f : 0.f);
    }
}

void CanabaltGame::update(Real dt) {
    if (mode_ && in_) mode_->update(dt, *in_, scene_);
    // Re-point the sprite node before the Scene ticks it: a death-restart builds
    // a fresh player, so spriteTarget() can change mid-frame.
    Player* tgt = mode_ ? mode_->spriteTarget() : nullptr;
    playerSprite_.setTarget(tgt);
    playerSprite_.visible = tgt && reveal_.player() && playerSprite_.hasTexture();
    // In sprite mode the Scene skips the player's box (the sprite stands in for
    // it) but the collider overlay still works off the box.
    if (tgt) tgt->renderable = !playerSprite_.visible;

    scene_.update(dt);            // tick the scene nodes (emitters, player sprite)
    updateDrag();
}

// ---- debug grab/drag (right mouse) ----------------------------------------
bool CanabaltGame::inScene(const Entity* e) const {
    for (const Entity* x : scene_.entities()) if (x == e) return true;
    return false;
}
Entity* CanabaltGame::pick(float lx, float ly) const {
    const auto& es = scene_.entities();
    for (auto it = es.rbegin(); it != es.rend(); ++it) {
        Entity* e = *it;
        if (e->fixed || !e->exists) continue;
        Vec2f s = scene_.camera.screen(*e);
        if (lx >= s.x && lx <= s.x + toFloat(e->size.x) &&
            ly >= s.y && ly <= s.y + toFloat(e->size.y))
            return e;
    }
    return nullptr;
}
void CanabaltGame::updateDrag() {
    if (!in_) return;
    if (grabbed_ && !inScene(grabbed_)) grabbed_ = nullptr;
    float lx = in_->mouseNx * float(logicalW_);
    float ly = in_->mouseNy * float(logicalH_);
    if (in_->dragPressed) grabbed_ = pick(lx, ly);
    if (in_->dragReleased || !in_->dragHeld) grabbed_ = nullptr;
    if (grabbed_ && in_->dragHeld) {
        Vec2 w = scene_.camera.worldFromScreen({lx, ly}, grabbed_->scrollFactor);
        grabbed_->pos = {w.x - grabbed_->size.x * R(0.5f), w.y - grabbed_->size.y * R(0.5f)};
        grabbed_->velocity = {R(0), R(0)};
        grabbed_->onFloor = false;
    }
}

Entity* CanabaltGame::entityAt(float lx, float ly) const {
    const auto& es = scene_.entities();
    for (auto it = es.rbegin(); it != es.rend(); ++it) {     // topmost first
        Entity* e = *it;
        if (!e->exists) continue;
        Vec2f s = scene_.camera.screen(*e);
        if (lx >= s.x && lx <= s.x + toFloat(e->size.x) &&
            ly >= s.y && ly <= s.y + toFloat(e->size.y))
            return e;
    }
    return nullptr;
}

void CanabaltGame::drawInspector(IRenderer& r) const {
    if (!in_) return;
    float lx = in_->mouseNx * float(logicalW_), ly = in_->mouseNy * float(logicalH_);
    Entity* e = entityAt(lx, ly);
    if (!e) return;
    float px = float(logicalW_) - 118, py = 22;
    r.fillRect(px - 2, py - 2, 118, 40, Color{0, 0, 0, 0.6f});
    r.drawRectOutline(px - 2, py - 2, 118, 40, Color{0.4f, 0.9f, 1.f, 0.8f}, 1);
    char b[64];
    Color c{0.8f, 0.95f, 1.f, 1};
    std::snprintf(b, sizeof b, "POS %d,%d", int(toFloat(e->pos.x)), int(toFloat(e->pos.y)));
    r.drawText(b, px, py, 1.f, c);
    std::snprintf(b, sizeof b, "SIZE %dx%d", int(toFloat(e->size.x)), int(toFloat(e->size.y)));
    r.drawText(b, px, py + 7, 1.f, c);
    std::snprintf(b, sizeof b, "VEL %d,%d", int(toFloat(e->velocity.x)), int(toFloat(e->velocity.y)));
    r.drawText(b, px, py + 14, 1.f, c);
    std::snprintf(b, sizeof b, "%s %s %s", e->solid ? "SOLID" : "-", e->fixed ? "FIXED" : "MOVE",
                  e->onFloor ? "ONFLOOR" : "AIR");
    r.drawText(b, px, py + 21, 1.f, c);
    // Outline the inspected entity.
    Vec2f s = scene_.camera.screen(*e);
    r.drawRectOutline(s.x, s.y, toFloat(e->size.x), toFloat(e->size.y), Color{0.4f, 0.9f, 1.f, 1}, 1);
}

void CanabaltGame::render(IRenderer& r, const DebugRuntime& dbg) {
    scene_.render(r, dbg);                                 // entities + all scene nodes
    // Game pause (P): freeze + centred PAUSED graphic. Shown even in clean play.
    if (dbg.paused() && pausedTex_)
        r.drawImage(pausedTex_, float(logicalW_ - pausedW_) * 0.5f,
                    float(logicalH_ - pausedH_) * 0.5f, float(pausedW_), float(pausedH_));
    if (dbg.uiHidden()) return;                            // clean play: nothing but the game
    if (dbg.enabled(DebugView::Perf)) drawInspector(r);   // hover inspector with the perf overlay
    if (grabbed_) {
        Vec2f s = scene_.camera.screen(*grabbed_);
        r.drawRectOutline(s.x - 2, s.y - 2, toFloat(grabbed_->size.x) + 4,
                          toFloat(grabbed_->size.y) + 4, Color{1, 1, 1, 1}, 1.f);
    }
    drawLegend(r);
    // Software mouse cursor, drawn last so it sits on top of the chrome.
    if (cursorVisible_ && cursorTex_ && in_)
        r.drawImage(cursorTex_, in_->mouseNx * float(logicalW_), in_->mouseNy * float(logicalH_),
                    float(cursorW_), float(cursorH_));
}

void CanabaltGame::drawLegend(IRenderer& r) const {
    // Row 1: the modes (current highlighted).
    float x = 3, y = float(logicalH_) - 16;
    for (int i = 0; i < modeCount(); ++i) {
        bool cur = (i == modeIndex_);
        Color c = cur ? Color{1, 1, 0.3f, 1} : Color{0.55f, 0.55f, 0.62f, 0.9f};
        if (cur) r.fillRect(x - 2, y - 2, r.textWidth(kModeLabels[i], 1.f) + 3, 9, Color{0, 0, 0, 0.45f});
        x += r.drawText(kModeLabels[i], x, y, 1.f, c) + 7;
    }
    r.drawText("RMB-DRAG", x + 4, y, 1.f, Color{0.5f, 0.8f, 1.f, 0.9f});

    // Row 2: the teaching toggles + their current state.
    char aux[200];
    std::snprintf(aux, sizeof aux, "F1 RNG:%s  F2 QUAKE:%s  F6/F8 SPRITES:%s  F7 PARTICLES:%s  F9 SOUND:%s  F10 MUSIC",
                  rng_.modeName(), quakeEnabled_ ? "ON" : "OFF",
                  reveal_.name(), particlesEnabled_ ? "ON" : "OFF", soundOn_ ? "ON" : "OFF");
    r.drawText(aux, 3, float(logicalH_) - 8, 1.f, Color{0.6f, 0.85f, 0.7f, 0.9f});
}

const char* CanabaltGame::statusLine() const {
    if (mode_) mode_->status(status_, sizeof status_);
    return status_;
}

} // namespace canabalt
