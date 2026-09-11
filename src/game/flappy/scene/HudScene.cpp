#include "flappy/scene/HudScene.hpp"
#include "IGame.hpp"            // GameContext
#include "platform/Window.hpp"  // InputFrame, Action
#include "render/IRenderer.hpp"
#include "asset/Image.hpp"
#include "asset/Audio.hpp"      // loadCaf
#include <cmath>
#include <cstdio>
#include <cstdlib>             // getenv
#include <cstring>
#include <fstream>
#include <string>

namespace flappy {

HudScene::~HudScene() {
    if (renderer_) {
        // Cache-owned; nothing to free.
    }
}

void HudScene::init(otacon::GameContext& ctx) {
    ScoreScene::init(ctx);
    renderer_ = ctx.renderer;
    audio_    = ctx.audio;

    const std::string spr = std::string(ctx.assetDir) + "/sprites/";
    auto tex = [&](const std::string& f) -> otacon::TextureHandle {
        return res_ ? res_->texture(spr + f) : 0;
    };
    for (int d = 0; d < 10; ++d) digits_[d] = tex(std::to_string(d) + ".png");
    messageTex_  = tex("message.png");
    gameoverTex_ = tex("gameover.png");

    const std::string snd = std::string(ctx.assetDir) + "/sound/";
    auto sound = [&](const char* f) -> otacon::SoundId {
        return audio_ ? audio_->createSound(otacon::loadCaf((snd + f).c_str())) : 0;
    };
    wing_ = sound("wing.caf"); point_ = sound("point.caf");
    hit_  = sound("hit.caf");  die_   = sound("die.caf"); swoosh_ = sound("swoosh.caf");

    const char* home = std::getenv("HOME");
    savePath_ = home ? std::string(home) + "/.otacon_flappy_best" : "flappy_best.dat";
    loadBest();
}

// The engine owns persistence (asset/SaveData.hpp). What that buys over the
// fstream this replaced: the write is atomic, so a crash mid-save leaves the
// previous best intact instead of a truncated file, and a malformed value reads
// back as the default rather than whatever operator>> left in the variable.
void HudScene::loadBest() {
    save_.load(savePath_.c_str());
    best_ = save_.getInt("best", 0);
}

void HudScene::saveBest() const {
    save_.set("best", best_);
    save_.save();
}

void HudScene::enter() {
    ScoreScene::enter();        // resets score, crash flag, skins, spawns a pipe
    pipes_.clear();             // the ready screen has no pipes until you start
    diePlayed_   = false;
    demoGoTime_  = 0.f;
    wireStates();
    fsm_.start(Phase::Ready);
    // best_ deliberately survives across runs (session high score).
}

void HudScene::flap() {
    bird_.vy = cfg::kFlapImpulse;
    if (audio_) audio_->play(wing_, 0.5f);
}

void HudScene::handleInput(const otacon::InputFrame& in) {
    if (demo_) return;                                  // autopilot ignores real input
    const bool jump = in.isPressed(otacon::Action::Jump);
    switch (fsm_.state()) {
        case Phase::Ready:
            if (jump) { fsm_.change(Phase::Playing); flap(); }     // first tap begins the run
            break;
        case Phase::Playing:
            CollisionScene::handleInput(in);            // flap + live gravity tuning
            if (jump && audio_) audio_->play(wing_, 0.5f);
            break;
        case Phase::GameOver:
            if (jump) { if (audio_) audio_->play(swoosh_, 0.6f); enter(); }   // tap to retry
            break;
    }
}

// A tiny autopilot so a recorded run plays itself: start, aim for the next gap,
// retry after the game-over screen has shown for a beat.
void HudScene::demoControl(otacon::Real dt) {
    switch (fsm_.state()) {
        case Phase::Ready:
            if (fsm_.timeInState() > 0.35f) { fsm_.change(Phase::Playing); flap(); }
            break;
        case Phase::Playing: {
            const Pipe* next = nullptr;
            for (const auto& p : pipes_)
                if (p.x + cfg::kPipeWidth > bird_.x) { next = &p; break; }
            // Aim low in the gap: one flap lifts ~67px, so a flap from near the
            // gap's bottom apexes just under the top pipe. Flapping on every frame
            // the bird is below the target both climbs fast between gaps and keeps
            // the overshoot bounded to one flap's worth.
            const float target = next ? next->gapY + cfg::kPipeGap * 0.72f : cfg::kGroundY * 0.5f;
            const float birdCenter = bird_.y + cfg::kBirdSize * 0.5f;
            if (birdCenter > target) flap();
            break;
        }
        case Phase::GameOver:
            demoGoTime_ += otacon::toFloat(dt);
            if (demoGoTime_ > 1.2f) enter();
            break;
    }
}

/*
 * The run's states live in an otacon::StateMachine. What that buys here is where
 * the game-over work goes: saving a new best and playing the impact used to sit
 * in update() behind a `!wasCrashed && crashed_` edge-detect, which is an entry
 * action written by hand. Now it IS an entry action, and it cannot fire twice.
 */
void HudScene::wireStates() {
    if (fsm_.stateCount() > 0) return;                  // wired once per object
    fsm_.add(Phase::Ready, nullptr, [this](otacon::Real dt, float timeIn) {
        anim_.update(dt);                               // keep the wings flapping on the menu
        bird_.y  = cfg::kBirdStartY + std::sin(timeIn * cfg::kBobOmega) * cfg::kBobAmp;
        bird_.vy = 0.f;
    });
    fsm_.add(Phase::Playing);
    fsm_.add(Phase::GameOver, [this] {
        newBest_ = score_ > best_;
        if (newBest_) { best_ = score_; saveBest(); }   // persisted high score
        demoGoTime_ = 0.f;
        if (audio_) audio_->play(hit_, 0.8f);
    });
}

void HudScene::update(otacon::Real dt) {
    if (demo_) demoControl(dt);                          // may flap / start / retry

    fsm_.update(dt);                                     // Ready's bob; applies any pending change
    if (fsm_.is(Phase::Ready)) return;

    const int  before     = score_;
    const bool wasCrashed  = crashed_;
    ScoreScene::update(dt);                             // world + collision + scoring (or crashed fall)

    if (score_ > before && audio_) audio_->play(point_, 0.7f);
    if (!wasCrashed && crashed_) fsm_.change(Phase::GameOver);   // entry action does the rest
    if (crashed_ && !diePlayed_) {                      // the body hitting the ground
        const float floor = cfg::kGroundY - cfg::kBirdSize;
        if (bird_.y >= floor) { if (audio_) audio_->play(die_, 0.7f); diePlayed_ = true; }
    }
}

void HudScene::render(otacon::IRenderer& r) const {
    FlapScene::render(r);    // world + bird (+ collider overlay); skips ScoreScene's bitmap HUD
    switch (fsm_.state()) {
        case Phase::Ready:    drawMessage(r); break;
        case Phase::Playing:  drawNumber(r, score_, cfg::kLogicalW * 0.5f, 36.f, 1.f); break;
        case Phase::GameOver: drawGameOverPanel(r); break;
    }
}

void HudScene::drawNumber(otacon::IRenderer& r, int value, float centerX, float top, float scale) const {
    if (!digits_[0]) return;
    char buf[12];
    std::snprintf(buf, sizeof buf, "%d", value);
    const int   n  = int(std::strlen(buf));
    const float dw = cfg::kDigitW * scale, dh = cfg::kDigitH * scale;
    float x = centerX - n * dw * 0.5f;
    for (int i = 0; i < n; ++i) {
        const int d = buf[i] - '0';
        if (digits_[d]) r.drawImage(digits_[d], x, top, dw, dh);
        x += dw;
    }
}

void HudScene::drawNumberRight(otacon::IRenderer& r, int value, float rightX, float top, float scale) const {
    if (!digits_[0]) return;
    char buf[12];
    std::snprintf(buf, sizeof buf, "%d", value);
    const int   n  = int(std::strlen(buf));
    const float dw = cfg::kDigitW * scale, dh = cfg::kDigitH * scale;
    float x = rightX - n * dw;
    for (int i = 0; i < n; ++i) {
        const int d = buf[i] - '0';
        if (digits_[d]) r.drawImage(digits_[d], x, top, dw, dh);
        x += dw;
    }
}

void HudScene::drawMessage(otacon::IRenderer& r) const {
    if (messageTex_)
        r.drawImage(messageTex_, (cfg::kLogicalW - cfg::kMsgW) * 0.5f, 70.f, cfg::kMsgW, cfg::kMsgH);
}

int HudScene::medalTier() const {
    if (score_ >= 40) return 4;   // platinum
    if (score_ >= 30) return 3;   // gold
    if (score_ >= 20) return 2;   // silver
    if (score_ >= 10) return 1;   // bronze
    return 0;
}

void HudScene::drawDisc(otacon::IRenderer& r, float cx, float cy, float radius, otacon::Color c) {
    // Filled circle via horizontal scanline strips (no circle primitive exists).
    for (int dy = int(-radius); dy <= int(radius); ++dy) {
        const float hw = std::sqrt(radius * radius - float(dy) * float(dy));
        r.fillRect(cx - hw, cy + float(dy), hw * 2.f, 1.f, c);
    }
}

void HudScene::drawGameOverPanel(otacon::IRenderer& r) const {
    using otacon::Color;
    // "GAME OVER" sprite above the board.
    if (gameoverTex_)
        r.drawImage(gameoverTex_, (cfg::kLogicalW - cfg::kGameOverW) * 0.5f, 92.f, cfg::kGameOverW, cfg::kGameOverH);

    // The scoreboard panel itself (no panel sprite ships, so draw it): a tan card
    // with a darker rim and a light top edge, like the original.
    const float px = 32.f, py = 150.f, pw = 224.f, ph = 104.f;
    r.fillRect(px - 2.f, py - 2.f, pw + 4.f, ph + 4.f, Color(0.74f, 0.69f, 0.45f));   // rim
    r.fillRect(px, py, pw, ph, Color(0.87f, 0.84f, 0.62f));                            // face
    r.fillRect(px + 3.f, py + 3.f, pw - 6.f, 2.f, Color(0.96f, 0.94f, 0.78f));         // top highlight

    const Color label(0.56f, 0.51f, 0.34f);

    // MEDAL, left half. A real medal if earned, otherwise an empty socket.
    r.drawText("MEDAL", px + 22.f, py + 16.f, 1.f, label);
    const float mcx = px + 50.f, mcy = py + 62.f, mr = 26.f;
    const int tier = medalTier();
    if (tier) {
        Color face;
        switch (tier) {
            case 1:  face = Color(0.80f, 0.50f, 0.22f); break;   // bronze
            case 2:  face = Color(0.72f, 0.76f, 0.80f); break;   // silver
            case 3:  face = Color(0.96f, 0.80f, 0.20f); break;   // gold
            default: face = Color(0.80f, 0.95f, 0.98f); break;   // platinum
        }
        drawDisc(r, mcx, mcy, mr + 2.f, Color(0.42f, 0.38f, 0.26f));            // rim
        drawDisc(r, mcx, mcy, mr, face);
        drawDisc(r, mcx - 8.f, mcy - 8.f, 7.f, Color(1.f, 1.f, 1.f, 0.45f));    // highlight
    } else {
        drawDisc(r, mcx, mcy, mr, Color(0.78f, 0.74f, 0.52f));                  // empty socket
    }

    // SCORE / BEST, right half — the big number sprites, right-aligned.
    const float rightX = px + pw - 16.f;
    r.drawText("SCORE", rightX - r.textWidth("SCORE", 1.f), py + 14.f, 1.f, label);
    drawNumberRight(r, score_, rightX, py + 24.f, 0.62f);
    r.drawText("BEST", rightX - r.textWidth("BEST", 1.f), py + 58.f, 1.f, label);
    if (newBest_) {                                                            // "NEW" badge
        const float bx = px + 104.f, by = py + 57.f;
        r.fillRect(bx, by, 24.f, 11.f, Color(0.90f, 0.26f, 0.16f));
        r.drawText("NEW", bx + 3.f, by + 3.f, 1.f, Color(1.f, 1.f, 1.f, 1.f));
    }
    drawNumberRight(r, best_, rightX, py + 68.f, 0.62f);

    // OK / SHARE buttons. OK == retry (tap / Space / R); SHARE is decorative.
    auto button = [&](float x, float y, float w, float h, const char* t) {
        r.fillRect(x, y, w, h, Color(0.94f, 0.62f, 0.18f));
        r.fillRect(x, y + h - 3.f, w, 3.f, Color(0.74f, 0.42f, 0.12f));
        r.drawRectOutline(x, y, w, h, Color(0.30f, 0.20f, 0.10f), 2.f);
        const float tw = r.textWidth(t, 1.5f);
        r.drawText(t, x + (w - tw) * 0.5f, y + (h - 7.5f) * 0.5f, 1.5f, Color(1.f, 1.f, 1.f, 1.f));
    };
    button(48.f, 272.f, 80.f, 30.f, "OK");
    button(160.f, 272.f, 90.f, 30.f, "SHARE");
}

const char* HudScene::status() const {
    const char* ph = fsm_.is(Phase::Ready) ? "READY" : fsm_.is(Phase::Playing) ? "PLAYING" : "GAME OVER";
    std::snprintf(status_, sizeof status_, "%s  SCORE %d", ph, score_);
    return status_;
}

} // namespace flappy
