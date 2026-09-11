#include "dino/DinoGame.hpp"
#include "core/debug/Debug.hpp"
#include "render/IRenderer.hpp"
#include "asset/Image.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

using namespace otacon;

namespace dino {

static const char* kModeLabels[] = { "1 GEOMETRY", "2 MECHANICS", "3 GAMEPLAY", "4 PIXEL ART" };

// --- The real Chrome "offline" sprite atlas (1x = 1233x68). Coordinates from the
//     reference's offline_sprite_definitions.ts; sub-rects are sheet pixels. ----
constexpr float kSheetW = 1233.f, kSheetH = 68.f;
namespace atlas {
// T-Rex: 44x47 frames (ducking 59 wide), block at x=848; frame x = 848 + offset.
constexpr float trexY = 2.f, trexW = 44.f, trexH = 47.f;
constexpr float trexStand = 848.f;   // jumping / standing-still (offset 0)
constexpr float trexRun1  = 936.f;   // running (offset 88)
constexpr float trexRun2  = 980.f;   // running (offset 132)
constexpr float trexCrash = 1068.f;  // crashed  (offset 220)
constexpr float cactusSmallX = 228.f, cactusSmallY = 2.f, cactusSmallW = 17.f, cactusSmallH = 35.f;
constexpr float cactusLargeX = 332.f, cactusLargeY = 2.f, cactusLargeW = 25.f, cactusLargeH = 50.f;
constexpr float birdX = 134.f, birdY = 2.f, birdW = 46.f, birdH = 40.f;   // frame2 at birdX+46
constexpr float cloudX = 86.f, cloudY = 2.f, cloudW = 46.f, cloudH = 14.f;
constexpr float horizonY = 54.f, horizonW = 600.f, horizonH = 14.f;       // flat @2, bumpy @602
constexpr float digitX = 655.f, digitY = 2.f, digitW = 10.f, digitH = 13.f;  // 0-9 then H,I
constexpr float gameOverX = 655.f, gameOverY = 16.f, gameOverW = 191.f, gameOverH = 12.f;
} // namespace atlas

constexpr float kRefScale = 1.f;
constexpr float kRefGameY = 0.f;

static const ObstacleSpec kSmallCactus{
    "SMALL", 17.f * kRefScale, 35.f * kRefScale,
    kRefGameY + 105.f * kRefScale, 0.f, 4.f, 120.f, 3, false
};
static const ObstacleSpec kLargeCactus{
    "LARGE", 25.f * kRefScale, 50.f * kRefScale,
    kRefGameY + 90.f * kRefScale, 0.f, 7.f, 120.f, 3, false
};
static const ObstacleSpec kBird{
    "BIRD", 46.f * kRefScale, 40.f * kRefScale,
    kRefGameY + 75.f * kRefScale, 8.5f, 999.f, 150.f, 1, true
};

const char* SpriteReveal::name() const {
    switch (level) {
        case 0: return "GEOMETRY";
        case 1: return "PLAYER";
        case 2: return "WORLD";
        default: return "PIXEL";
    }
}

void DinoGame::init(GameContext& ctx) {
    renderer_ = ctx.renderer;
    res_      = ctx.resources;
    assetDir_ = ctx.assetDir;
    logicalW_ = ctx.logicalW;
    logicalH_ = ctx.logicalH;

    // Chrome's dino remembers your best across sessions; ours only remembered it
    // until you quit. SaveData is what closes that gap.
    const char* home = std::getenv("HOME");
    save_.load(home ? (std::string(home) + "/.otacon_dino").c_str() : "dino_save.dat");
    highScore_ = save_.getInt("hi", 0);
    if (renderer_) {
        // Through the engine's cache, which also owns the teardown ordering.
        if (res_) sheet_ = res_->texture(std::string(assetDir_) + "/images/sprite.png");
    }
    groundCollider_.fixed = true;
    groundCollider_.solid = true;
    groundCollider_.color = Color{0.15f, 0.16f, 0.17f, 0.25f};
    groundCollider_.pos = {R(kGameX), R(kGroundY)};
    groundCollider_.size = {R(kSourceW * kScale), R(4)};
    setMode(3);   // boot into the full pixel-art game; 1/[ ] reveal the construction
}

void DinoGame::setMode(int index) {
    constexpr int count = 4;
    int wrapped = (index % count + count) % count;
    mode_ = DinoMode(wrapped);
    reveal_.level = wantsPixelArt() ? SpriteReveal::kMax : reveal_.level;
    resetRun();
}

int DinoGame::modeIndex() const {
    return int(mode_);
}

bool DinoGame::hasMechanics() const {
    return mode_ != DinoMode::Geometry;
}

bool DinoGame::hasObstacles() const {
    return mode_ == DinoMode::Gameplay || mode_ == DinoMode::PixelArt;
}

bool DinoGame::wantsPixelArt() const {
    return mode_ == DinoMode::PixelArt || reveal_.level > 0;
}

Color DinoGame::clearColor() const {
    return night_ ? Color::rgb(0x101318) : Color::rgb(0xf7f7f7);
}

void DinoGame::resetRun() {
    // Deterministic walks a seed sequence so each run differs but replays;
    // otherwise the engine generator draws from OS entropy.
    if (deterministic_) { rng_.setSource(otacon::Random::Source::Deterministic);
                          rng_.seed(0xD1A0u + std::uint32_t(seed_++)); }
    else rng_.setSource(otacon::Random::Source::Entropy);

    currentSpeed_ = hasMechanics() ? 6.f : 0.f;
    distance_ = score_ = horizonOffset_ = animTimer_ = restartTimer_ = 0.f;
    spawnCooldown_ = 85.f;
    jumping_ = reachedMinHeight_ = speedDrop_ = crashed_ = night_ = false;
    runFrame_ = 0;
    obstacles_.clear();
    clouds_.clear();
    clouds_.push_back(Cloud{kGameX + 430.f, kGameY + 36.f, 160.f});
    clouds_.push_back(Cloud{kGameX + 120.f, kGameY + 22.f, 240.f});

    player_.pos = {R(kTrexX), R(kGroundY - kTrexH)};
    player_.size = {R(kTrexW), R(kTrexH)};
    player_.velocity = {};
    player_.fixed = false;
    player_.solid = true;
    player_.renderable = false;
    player_.color = Color{0.18f, 0.19f, 0.2f, 1};
    minJumpY_ = kGroundY - kTrexH - 30.f * kScale;
}

void DinoGame::handleInput(const InputFrame& in) {
    in_ = &in;
    if (in.selectSlot >= 0 && in.selectSlot < 4) setMode(in.selectSlot);
    if (in.isPressed(Action::NextMode)) setMode(modeIndex() + 1);
    if (in.isPressed(Action::PrevMode)) setMode(modeIndex() - 1);
    if (in.isPressed(Action::Reset)) resetRun();
    if (in.isPressed(Action::Aux1)) {
        deterministic_ = !deterministic_;
        resetRun();
    }
    if (in.isPressed(Action::Aux2)) showCollisionParts_ = !showCollisionParts_;
    if (in.isPressed(Action::Aux5)) reveal_.level = (reveal_.level + 1) % (SpriteReveal::kMax + 1);
    if (in.isPressed(Action::Aux8)) reveal_.level = reveal_.level == 0 ? SpriteReveal::kMax : 0;
    if (in.isPressed(Action::ToggleCursor)) cursorVisible_ = !cursorVisible_;

    if (in.isPressed(Action::Jump)) {
        if (crashed_) resetRun();
        else startJump();
    }
    if (in.isPressed(Action::Aux6) && jumping_) {
        speedDrop_ = true;
        jumpVelocity_ = 1.f;
    }
}

void DinoGame::startJump() {
    if (!hasMechanics() || jumping_ || crashed_) return;
    jumping_ = true;
    reachedMinHeight_ = false;
    speedDrop_ = false;
    jumpVelocity_ = kInitialJumpVelocity - currentSpeed_ / 10.f;
}

void DinoGame::endJump() {
    if (reachedMinHeight_ && jumpVelocity_ < -5.f) jumpVelocity_ = -5.f;
}

void DinoGame::update(Real dt) {
    float dtMs = toFloat(dt) * 1000.f;
    if (crashed_) {
        restartTimer_ += dtMs;
        updateEntitiesForDebug();
        return;
    }

    animTimer_ += dtMs;
    if (animTimer_ >= (jumping_ ? 1000.f / 60.f : 1000.f / 12.f)) {
        runFrame_ = 1 - runFrame_;
        animTimer_ = 0.f;
    }

    updatePlayer(dtMs);
    updateWorld(dtMs);
    updateEntitiesForDebug();
}

void DinoGame::updatePlayer(float dtMs) {
    if (!hasMechanics() || !jumping_) return;

    float frames = dtMs / (1000.f / 60.f);
    float y = toFloat(player_.pos.y);
    y += jumpVelocity_ * (speedDrop_ ? kSpeedDropCoefficient : 1.f) * frames;
    jumpVelocity_ += kGravity * frames;

    if (y < minJumpY_ || speedDrop_) reachedMinHeight_ = true;
    if (y < kGameY + 30.f || speedDrop_) endJump();
    if (y > kGroundY - kTrexH) {
        y = kGroundY - kTrexH;
        jumping_ = false;
        speedDrop_ = false;
        jumpVelocity_ = 0.f;
    }
    player_.pos.y = R(y);
}

void DinoGame::updateWorld(float dtMs) {
    if (!hasMechanics()) return;

    currentSpeed_ = std::min(13.f, currentSpeed_ + 0.001f * dtMs / (1000.f / 60.f));
    float dx = currentSpeed_ * 60.f / 1000.f * dtMs * kScale;
    horizonOffset_ = std::fmod(horizonOffset_ + dx, 24.f);
    distance_ += dx;
    score_ = distance_ / 18.f;
    if (int(score_) > highScore_) {
        highScore_ = int(score_);
        // raise() is the engine's "a high score only ever goes up", and writing
        // on each new best means a crash cannot lose it.
        if (save_.raise("hi", highScore_)) save_.save();
    }
    night_ = false;   // day-only: the dark sprites can't be inverted without a post-pass

    for (Cloud& cloud : clouds_) {
        cloud.x -= dx * 0.08f;
        if (cloud.x < kGameX - 100.f) {
            cloud.x = kGameX + kSourceW * kScale + randRange(60.f, 220.f);
            cloud.y = kGameY + randRange(14.f, 48.f);
        }
    }

    if (!hasObstacles()) return;
    for (Obstacle& obstacle : obstacles_) {
        float extra = obstacle.spec && obstacle.spec->bird ? 0.8f * kScale : 0.f;
        obstacle.box.pos.x -= R(dx + extra);
        obstacle.flap += dtMs;
        if (!obstacle.passed && toFloat(obstacle.box.right()) < kTrexX) obstacle.passed = true;
        if (playerHits(obstacle)) {
            crashed_ = true;
            currentSpeed_ = 0.f;
        }
    }
    obstacles_.erase(std::remove_if(obstacles_.begin(), obstacles_.end(), [](const Obstacle& o) {
        return toFloat(o.box.right()) < -40.f;
    }), obstacles_.end());

    spawnCooldown_ -= dx;
    maybeSpawnObstacle();
}

void DinoGame::maybeSpawnObstacle() {
    if (!obstacles_.empty() || spawnCooldown_ > 0.f) return;
    const ObstacleSpec* choices[] = { &kSmallCactus, &kLargeCactus, &kBird };
    const ObstacleSpec* spec = choices[randInt(0, currentSpeed_ >= kBird.minSpeed ? 2 : 1)];
    spawnObstacle(*spec);
}

void DinoGame::spawnObstacle(const ObstacleSpec& spec) {
    Obstacle obstacle;
    obstacle.spec = &spec;
    obstacle.group = spec.bird ? 1 : randInt(1, currentSpeed_ >= spec.multipleSpeed ? spec.maxGroup : 1);
    float w = spec.w * float(obstacle.group);
    obstacle.box.pos = {R(kGameX + kSourceW * kScale + spec.w), R(spec.y)};
    obstacle.box.size = {R(w), R(spec.h)};
    obstacle.box.fixed = true;
    obstacle.box.solid = true;
    obstacle.box.renderable = false;
    obstacle.box.color = Color{0.2f, 0.42f, 0.25f, 1};
    obstacle.box.refreshHulls();
    obstacles_.push_back(obstacle);

    float minGap = w * (currentSpeed_ / kScale) + spec.minGap * kScale;
    spawnCooldown_ = randRange(minGap, minGap * 1.5f);
}

void DinoGame::updateEntitiesForDebug() {
    player_.refreshHulls();
    groundCollider_.refreshHulls();
    for (Obstacle& obstacle : obstacles_) obstacle.box.refreshHulls();
}

bool DinoGame::playerHits(const Obstacle& obstacle) const {
    float px = toFloat(player_.pos.x) + 6.f;
    float py = toFloat(player_.pos.y) + 2.f;
    float pw = toFloat(player_.size.x) - 16.f;
    float ph = toFloat(player_.size.y) - 8.f;
    float ox = toFloat(obstacle.box.pos.x);
    float oy = toFloat(obstacle.box.pos.y);
    float ow = toFloat(obstacle.box.size.x);
    float oh = toFloat(obstacle.box.size.y);
    if (obstacle.spec && obstacle.spec->bird) {
        ox += 4.f; oy += 16.f; ow -= 8.f; oh = 16.f;
    } else {
        ox += 2.f; ow -= 4.f;
    }
    return px < ox + ow && px + pw > ox && py < oy + oh && py + ph > oy;
}

float DinoGame::randRange(float lo, float hi) {
    return rng_.range(lo, hi);
}

int DinoGame::randInt(int lo, int hi) {
    return rng_.rangeI(lo, hi);
}

void DinoGame::render(IRenderer& r, const DebugRuntime& dbg) {
    drawWorld(r);

    // Feed Otacon's existing collider/parallax overlays with the active bodies.
    if (dbg.enabled(DebugView::Colliders) || dbg.enabled(DebugView::ParallaxBands)) {
        r.drawRectOutline(toFloat(groundCollider_.pos.x), toFloat(groundCollider_.pos.y),
                          toFloat(groundCollider_.size.x), toFloat(groundCollider_.size.y),
                          Color{0.2f, 1.f, 0.3f, 1}, 1);
        r.drawRectOutline(toFloat(player_.pos.x), toFloat(player_.pos.y),
                          toFloat(player_.size.x), toFloat(player_.size.y),
                          Color{0.2f, 1.f, 0.3f, 1}, 1);
        for (const Obstacle& obstacle : obstacles_) {
            r.drawRectOutline(toFloat(obstacle.box.pos.x), toFloat(obstacle.box.pos.y),
                              toFloat(obstacle.box.size.x), toFloat(obstacle.box.size.y),
                              Color{0.2f, 1.f, 0.3f, 1}, 1);
        }
    }
    if (showCollisionParts_) drawColliderParts(r);
    drawHud(r, dbg);
}

void DinoGame::shutdown() {
    sheet_ = 0;   // the engine's Resources cache owns it
    sheet_ = 0;
}

void DinoGame::blit(IRenderer& r, float dx, float dy, float sx, float sy, float sw, float sh) const {
    r.drawImage(sheet_, dx, dy, sw, sh,
                sx / kSheetW, sy / kSheetH, (sx + sw) / kSheetW, (sy + sh) / kSheetH);
}

// The metres-run odometer, top-right, from the sprite digits: "HI <best> <cur>".
void DinoGame::drawScore(IRenderer& r) const {
    using namespace atlas;
    constexpr float adv = 11.f;       // per-digit advance (DEST_WIDTH from Chrome)
    auto num = [&](int v, float rightX, int units) {   // draw `units` digits ending at rightX
        for (int i = 0; i < units; ++i) {
            int d = v % 10; v /= 10;
            blit(r, rightX - (i + 1) * adv, 6.f, digitX + d * digitW, digitY, digitW, digitH);
        }
    };
    float x = kViewW - 6.f;
    num(int(score_), x, 5);                              // current score, far right
    if (highScore_ > 0) {
        float hiRight = x - 5 * adv - 12.f;              // a gap, then the best
        num(highScore_, hiRight, 5);
        float label = hiRight - 5 * adv - 8.f;           // then the "HI" label
        blit(r, label - adv,     6.f, digitX + 11 * digitW, digitY, digitW, digitH);  // I
        blit(r, label - 2 * adv, 6.f, digitX + 10 * digitW, digitY, digitW, digitH);  // H
    }
}

void DinoGame::drawWorld(IRenderer& r) const {
    const bool art = wantsPixelArt() && sheet_;   // real sprites available + wanted
    for (const Cloud& cloud : clouds_) drawCloud(r, cloud, art && reveal_.world());
    drawGround(r, art && reveal_.world());
    for (const Obstacle& obstacle : obstacles_) drawObstacle(r, obstacle, art && reveal_.world());
    if (reveal_.player() || mode_ == DinoMode::PixelArt) drawPlayerPixel(r);
    else drawPlayerGeometry(r);

    if (art) drawScore(r);
    else {
        char buf[48];
        if (highScore_ > 0) std::snprintf(buf, sizeof buf, "HI %05d  %05d", highScore_, int(score_));
        else                std::snprintf(buf, sizeof buf, "%05d", int(score_));
        r.drawText(buf, kViewW - r.textWidth(buf, 1.f) - 6.f, 6.f, 1.f, Color{0.32f, 0.32f, 0.32f, 1});
    }

    if (crashed_) {
        if (art) blit(r, (kViewW - atlas::gameOverW) * 0.5f, 50.f,
                      atlas::gameOverX, atlas::gameOverY, atlas::gameOverW, atlas::gameOverH);
        else { float w = r.textWidth("GAME OVER", 2.f);
               r.drawText("GAME OVER", (kViewW - w) * 0.5f, 50.f, 2.f, Color{0.2f, 0.2f, 0.2f, 1}); }
        const char* hint = "PRESS SPACE";
        r.drawText(hint, (kViewW - r.textWidth(hint, 1.f)) * 0.5f, 74.f, 1.f, Color{0.5f, 0.5f, 0.5f, 1});
    }
}

void DinoGame::drawGround(IRenderer& r, bool pixel) const {
    if (pixel && sheet_) {
        using namespace atlas;
        const float gy = kGroundY - 2.f;                     // align the sprite line with kGroundY
        const float off = std::fmod(horizonOffset_, horizonW);
        // Two 600-wide segments tile the view, scrolling; flat (sheet x=2) then
        // bumpy (sheet x=2+600), so the horizon alternates like Chrome's.
        blit(r, kGameX - off,             gy, 2.f,             horizonY, horizonW, horizonH);
        blit(r, kGameX - off + horizonW,  gy, 2.f + horizonW,  horizonY, horizonW, horizonH);
        return;
    }
    r.drawLine(kGameX, kGroundY, kViewW - 10.f, kGroundY, Color{0.33f, 0.33f, 0.33f, 1}, 2.f);
}

void DinoGame::drawCloud(IRenderer& r, const Cloud& cloud, bool pixel) const {
    if (pixel && sheet_) {
        blit(r, cloud.x, cloud.y, atlas::cloudX, atlas::cloudY, atlas::cloudW, atlas::cloudH);
        return;
    }
    r.drawRectOutline(cloud.x, cloud.y, 46.f, 14.f, Color{0.78f, 0.78f, 0.78f, 1}, 1.f);
}

void DinoGame::drawPlayerGeometry(IRenderer& r) const {
    float x = toFloat(player_.pos.x), y = toFloat(player_.pos.y), s = kScale;
    Color aabb = crashed_ ? Color{0.9f, 0.18f, 0.18f, 1} : Color{0.16f, 0.18f, 0.2f, 1};
    Color part{0.12f, 0.48f, 0.74f, 0.55f};
    r.drawRectOutline(x, y, toFloat(player_.size.x), toFloat(player_.size.y), aabb, 1);
    // Chromium's running collision boxes, shown as a readable construction.
    r.fillRect(x + 22.f * s, y + 0.f * s, 17.f * s, 16.f * s, part);
    r.fillRect(x + 1.f * s, y + 18.f * s, 30.f * s, 9.f * s, part);
    r.fillRect(x + 10.f * s, y + 35.f * s, 14.f * s, 8.f * s, part);
    r.fillRect(x + 1.f * s, y + 24.f * s, 29.f * s, 5.f * s, part);
    r.fillRect(x + 5.f * s, y + 30.f * s, 21.f * s, 4.f * s, part);
    r.fillRect(x + 9.f * s, y + 34.f * s, 15.f * s, 4.f * s, part);
}

void DinoGame::drawPlayerPixel(IRenderer& r) const {
    float x = toFloat(player_.pos.x), y = toFloat(player_.pos.y), s = kScale;
    if (sheet_) {
        using namespace atlas;
        float fx = crashed_ ? trexCrash
                 : jumping_  ? trexStand                         // airborne: legs together
                 : (runFrame_ == 0 ? trexRun1 : trexRun2);       // alternating stride
        blit(r, x, y, fx, trexY, trexW, trexH);
        return;
    }
    // Fallback (no sprite sheet): the rectangle construction.
    Color c{0.18f, 0.19f, 0.2f, 1};
    r.fillRect(x + 18.f * s, y + 0.f * s, 19.f * s, 14.f * s, c);
    r.fillRect(x + 26.f * s, y + 4.f * s, 12.f * s, 5.f * s, c);
    r.fillRect(x + 26.f * s, y + 9.f * s, 7.f * s, 3.f * s, c);
    r.fillRect(x + 5.f * s, y + 14.f * s, 26.f * s, 18.f * s, c);
    r.fillRect(x + 0.f * s, y + 19.f * s, 9.f * s, 5.f * s, c);
    r.fillRect(x + 14.f * s, y + 31.f * s, 8.f * s, 13.f * s, c);
    r.fillRect(x + 4.f * s, y + 31.f * s, 6.f * s, 12.f * s, c);
    if (!crashed_) r.fillRect(x + 29.f * s, y + 3.f * s, 2.f * s, 2.f * s, clearColor());
    if (!jumping_ && !crashed_) {
        if (runFrame_ == 0) r.fillRect(x + 21.f * s, y + 38.f * s, 8.f * s, 5.f * s, c);
        else r.fillRect(x + 2.f * s, y + 37.f * s, 8.f * s, 5.f * s, c);
    }
}

void DinoGame::drawObstacle(IRenderer& r, const Obstacle& obstacle, bool pixel) const {
    float x = toFloat(obstacle.box.pos.x), y = toFloat(obstacle.box.pos.y);
    float w = toFloat(obstacle.box.size.x), h = toFloat(obstacle.box.size.y);
    if (!obstacle.spec) return;
    if (obstacle.spec->bird) drawBird(r, x, y, w, h, int(obstacle.flap / (1000.f / 6.f)) & 1, pixel);
    else drawCactus(r, x, y, obstacle.spec->w, h, obstacle.group, obstacle.spec == &kLargeCactus, pixel);
}

void DinoGame::drawCactus(IRenderer& r, float x, float y, float w, float h, int group, bool large, bool pixel) const {
    if (pixel && sheet_) {
        using namespace atlas;
        // A group of N cacti is one (N*width)-wide sub-rect from the cactus block.
        const float sx = large ? cactusLargeX : cactusSmallX;
        const float sy = large ? cactusLargeY : cactusSmallY;
        const float sw = (large ? cactusLargeW : cactusSmallW) * float(group);
        const float sh = large ? cactusLargeH : cactusSmallH;
        blit(r, x, y, sx, sy, sw, sh);
        return;
    }
    Color c{0.18f, 0.19f, 0.2f, 1};
    for (int i = 0; i < group; ++i) r.fillRect(x + float(i) * w + w * 0.30f, y, w * 0.40f, h, c);
}

void DinoGame::drawBird(IRenderer& r, float x, float y, float w, float h, int frame, bool pixel) const {
    if (pixel && sheet_) {
        blit(r, x, y, atlas::birdX + float(frame) * atlas::birdW, atlas::birdY, atlas::birdW, atlas::birdH);
        return;
    }
    r.fillRect(x, y + h * 0.35f, w, h * 0.3f, Color{0.18f, 0.19f, 0.2f, 1});
}

void DinoGame::drawColliderParts(IRenderer& r) const {
    Color c{0.2f, 0.7f, 1.f, 1};
    float x = toFloat(player_.pos.x), y = toFloat(player_.pos.y), s = kScale;
    r.drawRectOutline(x + 22.f * s, y + 0.f * s, 17.f * s, 16.f * s, c, 1);
    r.drawRectOutline(x + 1.f * s, y + 18.f * s, 30.f * s, 9.f * s, c, 1);
    r.drawRectOutline(x + 10.f * s, y + 35.f * s, 14.f * s, 8.f * s, c, 1);
}

void DinoGame::drawHud(IRenderer& r, const DebugRuntime& dbg) const {
    if (dbg.uiHidden()) return;          // the score itself is drawn in drawWorld
    float x = 3.f, y = float(logicalH_) - 16.f;
    for (int i = 0; i < 4; ++i) {
        bool cur = i == modeIndex();
        Color c = cur ? Color{1, 1, 0.3f, 1} : Color{0.55f, 0.55f, 0.62f, 0.9f};
        if (cur) r.fillRect(x - 2, y - 2, r.textWidth(kModeLabels[i], 1.f) + 3, 9, Color{0, 0, 0, 0.35f});
        x += r.drawText(kModeLabels[i], x, y, 1.f, c) + 7;
    }
    char aux[180];
    std::snprintf(aux, sizeof aux, "F1 RNG:%s  F2 HITBOX-PARTS:%s  F6/F8 ART:%s  E SPEED-DROP",
                  deterministic_ ? "DETERMINISTIC" : "RANDOM",
                  showCollisionParts_ ? "ON" : "OFF", reveal_.name());
    r.drawText(aux, 3, float(logicalH_) - 8, 1.f, Color{0.42f, 0.65f, 0.55f, 0.95f});

    if (cursorVisible_ && in_) {
        float mx = in_->mouseNx * float(logicalW_), my = in_->mouseNy * float(logicalH_);
        r.drawLine(mx - 4, my, mx + 4, my, Color{0.1f, 0.6f, 1.f, 1}, 1);
        r.drawLine(mx, my - 4, mx, my + 4, Color{0.1f, 0.6f, 1.f, 1}, 1);
    }
}

const char* DinoGame::statusLine() const {
    std::snprintf(status_, sizeof status_, "DINO  MODE-%s  SPEED-%.1f  SCORE-%05d  %s",
                  kModeLabels[modeIndex()] + 2, currentSpeed_, int(score_),
                  crashed_ ? "CRASHED" : jumping_ ? "JUMP" : "RUN");
    return status_;
}

} // namespace dino
