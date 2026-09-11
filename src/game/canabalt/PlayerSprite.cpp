#include "canabalt/PlayerSprite.hpp"
#include "canabalt/Player.hpp"
#include "scene/Camera.hpp"
#include "asset/Image.hpp"
#include <algorithm>
#include <string>

using namespace otacon;

namespace canabalt {

void PlayerSpriteNode::load(IRenderer* r, const char* assetDir) {
    if (tex_ || !r) return;
    Image img = loadPng((std::string(assetDir) + "/images/player2.png").c_str());
    if (img.valid()) { texW_ = img.width; texH_ = img.height; tex_ = r->createTexture(img); }
}

void PlayerSpriteNode::destroy(IRenderer* r) {
    if (tex_ && r) r->destroyTexture(tex_);
    tex_ = 0;
}

void PlayerSpriteNode::setTarget(Player* p) {
    if (p == target_) return;
    target_ = p;
    animTime_ = R(0);
    airTime_ = R(0);
}

void PlayerSpriteNode::update(Real dt, const Camera&) {
    if (!target_) return;
    animTime_ += dt;
    airTime_ = target_->onFloor ? R(0) : airTime_ + dt;
    // A hard landing (or an obstacle) sets stumble -> play the roll once.
    if (target_->stumble && toFloat(stumbleTime_) < 0.f) {
        stumbleTime_ = R(0);
        target_->stumble = false;            // consume the one-shot
    }
    if (toFloat(stumbleTime_) >= 0.f) {
        stumbleTime_ += dt;
        if (toFloat(stumbleTime_) > 0.55f) stumbleTime_ = R(-1);   // 11 frames @ ~20fps
    }
}

// Pick the animation frame the way Player.m does (run speed-scaled, then jump
// rising, then fall).
int PlayerSpriteNode::frame() const {
    const Player& p = *target_;
    float vx = toFloat(p.velocity.x);
    if (p.onFloor) {
        if (toFloat(stumbleTime_) >= 0.f)                           // tumbling roll 27-37
            return 27 + std::min(10, int(toFloat(stumbleTime_) * 20.f));
        float fps = vx < 150 ? 15.f : vx < 300 ? 28.f : vx < 550 ? 40.f : 30.f;
        return int(toFloat(animTime_) * fps) % 16;                  // run cycle 0-15
    }
    if (toFloat(p.velocity.y) < -140.f)
        return 16 + std::min(3, int(toFloat(airTime_) * 12.f));     // jump 16-19
    return 20 + int(toFloat(airTime_) * 14.f) % 7;                  // fall 20-26
}

void PlayerSpriteNode::render(IRenderer& r, const Camera& cam) const {
    if (!target_ || !tex_) return;
    constexpr int kFrame = 30;                 // 30x30 frames in player2.png
    Vec2f s = cam.screen(*target_);
    int f = frame();
    int cols = texW_ / kFrame;
    float fu = float(kFrame) / float(texW_), fv = float(kFrame) / float(texH_);
    float u0 = float(f % cols) * fu, v0 = float(f / cols) * fv;
    // The 30x30 graphic sits at the hitbox minus the (6,12) offset (Player.m).
    r.drawImage(tex_, s.x - 6, s.y - 12, float(kFrame), float(kFrame),
                u0, v0, u0 + fu, v0 + fv);
}

} // namespace canabalt
