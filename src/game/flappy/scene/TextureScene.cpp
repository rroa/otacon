#include "flappy/scene/TextureScene.hpp"
#include "IGame.hpp"            // GameContext
#include "render/IRenderer.hpp"
#include "asset/Image.hpp"
#include <string>

namespace flappy {

// Textures come from the engine's cache now, so nothing here owns one and the
// destructor has nothing to free. That is the point: this scene and FlapScene
// both want yellowbird-midflap, and loading it twice meant two decodes and two
// GPU textures for one file.
TextureScene::~TextureScene() = default;

void TextureScene::init(otacon::GameContext& ctx) {
    renderer_ = ctx.renderer;
    res_ = ctx.resources;
    // A single static frame is enough here; flap animation arrives in Build 5.
    if (res_) tex_ = res_->texture(std::string(ctx.assetDir) + "/sprites/yellowbird-midflap.png");
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
