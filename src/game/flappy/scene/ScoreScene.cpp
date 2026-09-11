#include "flappy/scene/ScoreScene.hpp"
#include "platform/Window.hpp"  // InputFrame, Action
#include "render/IRenderer.hpp"
#include <cstdio>

namespace flappy {

void ScoreScene::enter() {
    CollisionScene::enter();
    score_ = 0;
}

void ScoreScene::handleInput(const otacon::InputFrame& in) {
    if (crashed_) {
        if (in.isPressed(otacon::Action::Jump)) enter();   // tap to play again
        return;
    }
    CollisionScene::handleInput(in);                        // flap / gravity tuning while alive
}

void ScoreScene::update(otacon::Real dt) {
    CollisionScene::update(dt);     // world + collision (sets crashed_ on a hit)
    if (crashed_) return;
    // One point per pair, the moment it fully clears the bird's fixed column.
    for (auto& p : pipes_)
        if (!p.passed && p.x + cfg::kPipeWidth < cfg::kBirdX) { p.passed = true; ++score_; }
}

void ScoreScene::render(otacon::IRenderer& r) const {
    // FlapScene::render is the layered world+bird (+collider overlay); calling it
    // directly skips Build 6's distance meter, which the score now replaces.
    FlapScene::render(r);
    drawScore(r);
    if (crashed_) drawGameOver(r);
}

void ScoreScene::drawScore(otacon::IRenderer& r) const {
    char buf[8];
    std::snprintf(buf, sizeof buf, "%d", score_);
    const float w = r.textWidth(buf, 4.f);
    const float x = (cfg::kLogicalW - w) * 0.5f, y = 50.f;
    r.drawText(buf, x + 1.f, y + 1.f, 4.f, otacon::Color{0, 0, 0, 0.4f});   // shadow
    r.drawText(buf, x, y, 4.f, otacon::Color{1, 1, 1, 1});                  // ink
}

void ScoreScene::drawGameOver(otacon::IRenderer& r) const {
    auto text = [&](const char* t, float y, float s) {
        const float w = r.textWidth(t, s), x = (cfg::kLogicalW - w) * 0.5f;
        r.drawText(t, x + 1.f, y + 1.f, s, otacon::Color{0, 0, 0, 0.5f});
        r.drawText(t, x, y, s, otacon::Color{1, 1, 1, 1});
    };
    char s[24];
    std::snprintf(s, sizeof s, "SCORE %d", score_);
    text("GAME OVER", 195.f, 3.f);
    text(s, 235.f, 2.f);
    text("SPACE TO RETRY", 265.f, 1.f);
}

const char* ScoreScene::status() const {
    std::snprintf(status_, sizeof status_, crashed_ ? "GAME OVER  SCORE %d" : "SCORE %d", score_);
    return status_;
}

} // namespace flappy
