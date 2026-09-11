#include "flappy/scene/CollisionScene.hpp"
#include "IGame.hpp"            // GameContext
#include "platform/Window.hpp"  // InputFrame
#include "render/IRenderer.hpp"
#include "asset/Image.hpp"
#include <cstdio>
#include <string>

namespace flappy {

namespace {
bool aabb(float ax, float ay, float aw, float ah, float bx, float by, float bw, float bh) {
    return ax < bx + bw && ax + aw > bx && ay < by + bh && ay + ah > by;
}
}

CollisionScene::~CollisionScene() {
    if (renderer_) {
        // Cache-owned; nothing to free.
    }
}

void CollisionScene::init(otacon::GameContext& ctx) {
    WorldScene::init(ctx);     // yellow bird, green pipe, backgrounds, base
    renderer_ = ctx.renderer;
    auto loadFrames = [&](otacon::TextureHandle (&dst)[3], const char* stem) {
        const char* suffix[3] = {"upflap", "midflap", "downflap"};
        for (int i = 0; i < 3; ++i)
            if (res_) dst[i] = res_->texture(std::string(ctx.assetDir) + "/sprites/"
                                             + stem + "-" + suffix[i] + ".png");
    };
    loadFrames(redBird_,  "redbird");
    loadFrames(blueBird_, "bluebird");
    if (res_) redPipe_ = res_->texture(std::string(ctx.assetDir) + "/sprites/pipe-red.png");
}

void CollisionScene::enter() {
    WorldScene::enter();       // bird/pipes reset, distance 0, day/night roll
    crashed_ = false;
    // Roll cosmetic skins for this run and repoint the base classes' active handles.
    const int bc = skinRng_.rangeI(0, 3);
    birdFrames_ = (bc == 1 && redBird_[0])  ? redBird_
                : (bc == 2 && blueBird_[0]) ? blueBird_
                                            : yellowBird_;
    const int pc = skinRng_.rangeI(0, 2);
    pipeTex_ = (pc == 1 && redPipe_) ? redPipe_ : greenPipe_;
}

void CollisionScene::handleInput(const otacon::InputFrame& in) {
    if (!crashed_) WorldScene::handleInput(in);    // a crash locks out the flap
}

void CollisionScene::update(otacon::Real dt) {
    if (crashed_) {
        // World is frozen; the dead bird just falls and comes to rest on the ground.
        GravityScene::update(dt);
        const float floor = cfg::kGroundY - cfg::kBirdSize;
        if (bird_.y >= floor) { bird_.y = floor; bird_.vy = 0.f; }
        return;
    }
    WorldScene::update(dt);
    if (collides()) crashed_ = true;
}

bool CollisionScene::collides() const {
    const float bx = bird_.x, by = bird_.y, bs = cfg::kBirdSize;
    if (by + bs >= cfg::kGroundY) return true;                  // ground
    for (const auto& p : pipes_) {
        if (aabb(bx, by, bs, bs, p.x, 0.f, cfg::kPipeWidth, p.gapY)) return true;             // top pipe
        const float bottom = p.gapY + cfg::kPipeGap;
        if (aabb(bx, by, bs, bs, p.x, bottom, cfg::kPipeWidth, cfg::kGroundY - bottom)) return true;
    }
    return false;
}

void CollisionScene::drawColliders(otacon::IRenderer& r) const {
    const otacon::Color birdBox{1.f, 0.1f, 0.1f, 0.95f};   // red — the bird hit box
    const otacon::Color pipeBox{0.1f, 1.f, 1.f, 0.85f};    // cyan — the pipe rects
    const otacon::Color floor  {1.f, 0.1f, 1.f, 0.9f};     // magenta — the ground line
    r.drawRectOutline(bird_.x, bird_.y, cfg::kBirdSize, cfg::kBirdSize, birdBox, 1.f);
    for (const auto& p : pipes_) {
        r.drawRectOutline(p.x, 0.f, cfg::kPipeWidth, p.gapY, pipeBox, 1.f);
        const float bottom = p.gapY + cfg::kPipeGap;
        r.drawRectOutline(p.x, bottom, cfg::kPipeWidth, cfg::kGroundY - bottom, pipeBox, 1.f);
    }
    r.drawLine(0.f, cfg::kGroundY, cfg::kLogicalW, cfg::kGroundY, floor, 1.f);
}

const char* CollisionScene::status() const {
    std::snprintf(status_, sizeof status_, "%s", crashed_ ? "CRASHED (R to reset)" : "ALIVE");
    return status_;
}

} // namespace flappy
