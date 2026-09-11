#include "flappy/scene/GeometryScene.hpp"
#include "platform/Window.hpp"   // InputFrame, Action
#include "render/IRenderer.hpp"

namespace flappy {

void GeometryScene::enter() {
    bird_.x  = cfg::kBirdX;
    bird_.y  = cfg::kBirdStartY;
    bird_.vy = 0;
    rising_  = false;
}

void GeometryScene::handleInput(const otacon::InputFrame& in) {
    // The engine maps Space / Up / W / click onto the single Jump action. We read
    // it as a level (held), not an edge: holding rises, releasing sinks.
    rising_ = in.isHeld(otacon::Action::Jump);
}

void GeometryScene::update(otacon::Real dt) {
    bird_.vy = rising_ ? -cfg::kManualSpeed : cfg::kManualSpeed;   // up is -Y
    bird_.integrate(dt);

    // This build keeps the square on-screen; falling off the bottom is what the
    // gravity build introduces, so here we just stop at the edges.
    const float maxY = cfg::kLogicalH - cfg::kBirdSize;
    if (bird_.y < 0.f)  { bird_.y = 0.f;  bird_.vy = 0.f; }
    if (bird_.y > maxY) { bird_.y = maxY; bird_.vy = 0.f; }
}

void GeometryScene::render(otacon::IRenderer& r) const {
    // Background is the cleared sky (Scene::background); we only draw the bird.
    r.fillRect(bird_.x, bird_.y, cfg::kBirdSize, cfg::kBirdSize, cfg::kBird);
}

} // namespace flappy
