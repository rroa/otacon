#include "flappy/scene/PipesScene.hpp"
#include "render/IRenderer.hpp"
#include <cstdio>

namespace flappy {

void PipesScene::spawn(float x) {
    // Keep the whole gap above the ground line (its bottom edge clears kGroundY).
    std::uniform_real_distribution<float> gap(
        cfg::kPipeMargin, cfg::kGroundY - cfg::kPipeGap - cfg::kPipeMargin);
    pipes_.push_back({x, gap(rng_)});
}

void PipesScene::enter() {
    GravityScene::enter();               // reset bird + gravity (TextureScene adds no reset)
    pipes_.clear();
    rng_.seed(0xF1A99u);
    spawn(cfg::kLogicalW * 0.9f);        // one already partly on-screen so motion reads at once
}

void PipesScene::update(otacon::Real dt) {
    GravityScene::update(dt);            // bird physics, unchanged

    const float dx = cfg::kScrollSpeed * otacon::toFloat(dt);
    for (auto& p : pipes_) p.x -= dx;

    // Drop pairs that have fully exited on the left.
    while (!pipes_.empty() && pipes_.front().x + cfg::kPipeWidth < 0.f)
        pipes_.erase(pipes_.begin());

    // Keep the field fed from the right at a fixed spacing.
    if (pipes_.empty() || pipes_.back().x <= cfg::kLogicalW - cfg::kPipeSpacing)
        spawn(pipes_.empty() ? float(cfg::kLogicalW) : pipes_.back().x + cfg::kPipeSpacing);
}

void PipesScene::render(otacon::IRenderer& r) const {
    drawBackground(r);   // clear color, or a real image in Build 6
    drawPipes(r);        // green rects, or textured in Build 4
    drawForeground(r);   // nothing, or the scrolling ground in Build 6
    drawBird(r);         // single frame, or animated + tilted in Build 5
    if (showColliders) drawColliders(r);   // 'C' overlay (Build 7+)
}

void PipesScene::drawPipes(otacon::IRenderer& r) const {
    for (const auto& p : pipes_) {
        r.fillRect(p.x, 0.f, cfg::kPipeWidth, p.gapY, cfg::kPipe);                  // top pipe
        const float by = p.gapY + cfg::kPipeGap;
        r.fillRect(p.x, by, cfg::kPipeWidth, cfg::kLogicalH - by, cfg::kPipe);      // bottom pipe
    }
}

const char* PipesScene::status() const {
    std::snprintf(status_, sizeof status_, "SCROLL %.0f  PIPES %zu", cfg::kScrollSpeed, pipes_.size());
    return status_;
}

} // namespace flappy
