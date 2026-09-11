#include "flappy/scene/TextureScene.hpp"
#include "IGame.hpp"            // GameContext
#include "render/IRenderer.hpp"
#include "asset/Image.hpp"
#include <string>

namespace flappy {

TextureScene::~TextureScene() {
    if (tex_ && renderer_) renderer_->destroyTexture(tex_);
}

void TextureScene::init(otacon::GameContext& ctx) {
    renderer_ = ctx.renderer;
    // A single static frame is enough here; flap animation arrives in Build 5.
    const std::string path = std::string(ctx.assetDir) + "/sprites/yellowbird-midflap.png";
    otacon::Image img = otacon::loadPng(path.c_str());
    if (img.valid() && renderer_) tex_ = renderer_->createTexture(img);
}

void TextureScene::render(otacon::IRenderer& r) const { drawBird(r); }

void TextureScene::drawBird(otacon::IRenderer& r) const {
    if (!tex_) { GravityScene::render(r); return; }     // fall back to the square if missing
    // Center the 34x24 sprite on the bird's logical box (kBirdSize square).
    const float cx = bird_.x + cfg::kBirdSize * 0.5f;
    const float cy = bird_.y + cfg::kBirdSize * 0.5f;
    r.drawImage(tex_, cx - cfg::kBirdSpriteW * 0.5f, cy - cfg::kBirdSpriteH * 0.5f,
                cfg::kBirdSpriteW, cfg::kBirdSpriteH);
}

} // namespace flappy
