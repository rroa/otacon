#include "flappy/scene/WorldScene.hpp"
#include "IGame.hpp"            // GameContext
#include "render/IRenderer.hpp"
#include "asset/Image.hpp"
#include <cmath>
#include <cstdio>
#include <string>

namespace flappy {

WorldScene::~WorldScene() {
    if (renderer_)
        for (auto t : {dayTex_, nightTex_, baseTex_})
            if (t) renderer_->destroyTexture(t);
}

void WorldScene::init(otacon::GameContext& ctx) {
    FlapScene::init(ctx);       // bird frames + textured pipes
    renderer_ = ctx.renderer;
    auto load = [&](const char* file) -> otacon::TextureHandle {
        otacon::Image img = otacon::loadPng((std::string(ctx.assetDir) + "/sprites/" + file).c_str());
        return (img.valid() && renderer_) ? renderer_->createTexture(img) : 0;
    };
    dayTex_   = load("background-day.png");
    nightTex_ = load("background-night.png");
    baseTex_  = load("base.png");
}

void WorldScene::enter() {
    FlapScene::enter();
    distance_ = 0.f;
    night_ = bgRng_.chance(0.5f);   // day or night, at random
}

void WorldScene::update(otacon::Real dt) {
    FlapScene::update(dt);
    distance_ += cfg::kScrollSpeed * otacon::toFloat(dt);             // start measuring distance
}

void WorldScene::drawBackground(otacon::IRenderer& r) const {
    const otacon::TextureHandle bg = night_ ? nightTex_ : dayTex_;
    if (bg) r.drawImage(bg, 0.f, 0.f, cfg::kLogicalW, cfg::kLogicalH);
}

void WorldScene::drawForeground(otacon::IRenderer& r) const {
    if (!baseTex_) return;
    // The ground scrolls with the world; tile copies to cover the canvas width.
    const float off = std::fmod(distance_, cfg::kBaseW);
    for (float x = -off; x < cfg::kLogicalW; x += cfg::kBaseW)
        r.drawImage(baseTex_, x, cfg::kGroundY, cfg::kBaseW, cfg::kBaseH);
}

void WorldScene::drawDistance(otacon::IRenderer& r) const {
    char buf[16];
    std::snprintf(buf, sizeof buf, "%d", int(distance_ / 10.f));
    const float w = r.textWidth(buf, 3.f);
    const float x = (cfg::kLogicalW - w) * 0.5f, y = 40.f;
    r.drawText(buf, x + 1.f, y + 1.f, 3.f, otacon::Color{0, 0, 0, 0.35f});   // shadow
    r.drawText(buf, x, y, 3.f, otacon::Color{1, 1, 1, 1});                    // ink
}

void WorldScene::render(otacon::IRenderer& r) const {
    FlapScene::render(r);   // layered: background → pipes → ground → bird
    drawDistance(r);        // measurement readout, on top
}

const char* WorldScene::status() const {
    std::snprintf(status_, sizeof status_, "%s  DIST %d", night_ ? "NIGHT" : "DAY", int(distance_ / 10.f));
    return status_;
}

} // namespace flappy
