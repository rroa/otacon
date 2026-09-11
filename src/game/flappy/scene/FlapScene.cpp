#include "flappy/scene/FlapScene.hpp"
#include "IGame.hpp"            // GameContext
#include "render/IRenderer.hpp"
#include "asset/Image.hpp"
#include <algorithm>
#include <string>

namespace flappy {

FlapScene::~FlapScene() {
    // The engine's cache owns these; nothing to free here.
}

void FlapScene::init(otacon::GameContext& ctx) {
    PipeTextureScene::init(ctx);    // textured pipes + the base single-frame bird
    renderer_ = ctx.renderer;
    const char* names[3] = {"yellowbird-upflap", "yellowbird-midflap", "yellowbird-downflap"};
    for (int i = 0; i < 3; ++i) {
        // midflap is also wanted by TextureScene; the cache hands back the same
        // handle rather than decoding and uploading it a second time.
        if (res_) yellowBird_[i] = res_->texture(std::string(ctx.assetDir) + "/sprites/" + names[i] + ".png");
    }
    birdFrames_ = yellowBird_;      // active set defaults to yellow
}

void FlapScene::enter() {
    PipeTextureScene::enter();
    animTime_ = 0.f;
}

void FlapScene::update(otacon::Real dt) {
    PipeTextureScene::update(dt);
    animTime_ += otacon::toFloat(dt);
}

void FlapScene::drawBird(otacon::IRenderer& r) const {
    if (!birdFrames_ || !birdFrames_[0]) { PipeTextureScene::drawBird(r); return; }   // static fallback

    // Wing: ping-pong up→mid→down→mid so the flap reads in both directions.
    static constexpr int seq[4] = {0, 1, 2, 1};
    const int step = int(animTime_ / cfg::kAnimStep);
    const otacon::TextureHandle tex = birdFrames_[seq[step % 4]];

    // Tilt: nose up while rising/just-flapped, rotating toward a dive as it falls.
    float angle = cfg::kTiltUp + std::max(0.f, bird_.vy) * cfg::kTiltRate;
    angle = std::clamp(angle, cfg::kTiltUp, cfg::kTiltDown);

    const float cx = bird_.x + cfg::kBirdSize * 0.5f;
    const float cy = bird_.y + cfg::kBirdSize * 0.5f;
    r.drawImageRotated(tex, cx, cy, cfg::kBirdSpriteW, cfg::kBirdSpriteH, angle);
}

} // namespace flappy
