#include "flappy/scene/PipeTextureScene.hpp"
#include "IGame.hpp"            // GameContext
#include "render/IRenderer.hpp"
#include "asset/Image.hpp"
#include <string>

namespace flappy {

PipeTextureScene::~PipeTextureScene() {
    if (greenPipe_ && renderer_) renderer_->destroyTexture(greenPipe_);
}

void PipeTextureScene::init(otacon::GameContext& ctx) {
    TextureScene::init(ctx);    // load the bird sprite (base behavior)
    renderer_ = ctx.renderer;
    const std::string path = std::string(ctx.assetDir) + "/sprites/pipe-green.png";
    otacon::Image img = otacon::loadPng(path.c_str());
    if (img.valid() && renderer_) greenPipe_ = renderer_->createTexture(img);
    pipeTex_ = greenPipe_;      // active texture defaults to green
}

void PipeTextureScene::drawPipe(otacon::IRenderer& r, float x, float mouthY,
                                bool mouthUp, float height) const {
    const float W = cfg::kPipeWidth, capH = cfg::kPipeCapH;
    const float capV = capH / cfg::kPipeSpriteH;       // UV row where the lip ends
    if (mouthUp) {
        // Bottom pipe, sprite upright: lip at the mouth, shaft stretched below.
        r.drawImage(pipeTex_, x, mouthY,        W, capH,          0.f, 0.f,  1.f, capV);
        r.drawImage(pipeTex_, x, mouthY + capH, W, height - capH, 0.f, capV, 1.f, 1.f);
    } else {
        // Top pipe, sprite flipped vertically (swap V): lip points down at the
        // mouth, shaft stretched above up to the screen edge.
        r.drawImage(pipeTex_, x, mouthY - capH,   W, capH,          0.f, capV, 1.f, 0.f);
        r.drawImage(pipeTex_, x, mouthY - height, W, height - capH, 0.f, 1.f,  1.f, capV);
    }
}

void PipeTextureScene::drawPipes(otacon::IRenderer& r) const {
    if (!pipeTex_) { PipesScene::drawPipes(r); return; }   // fall back to green rects
    for (const auto& p : pipes_) {
        drawPipe(r, p.x, p.gapY, /*mouthUp=*/false, p.gapY);                       // top
        const float by = p.gapY + cfg::kPipeGap;
        drawPipe(r, p.x, by,     /*mouthUp=*/true,  cfg::kLogicalH - by);          // bottom
    }
}

} // namespace flappy
