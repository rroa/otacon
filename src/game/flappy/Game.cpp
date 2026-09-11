#include "flappy/Game.hpp"
#include "flappy/scene/GeometryScene.hpp"
#include "flappy/scene/GravityScene.hpp"
#include "flappy/scene/TextureScene.hpp"
#include "flappy/scene/PipesScene.hpp"
#include "flappy/scene/PipeTextureScene.hpp"
#include "flappy/scene/FlapScene.hpp"
#include "flappy/scene/WorldScene.hpp"
#include "flappy/scene/CollisionScene.hpp"
#include "flappy/scene/ScoreScene.hpp"
#include "flappy/scene/HudScene.hpp"
#include "platform/Window.hpp"   // InputFrame, Action
#include "render/IRenderer.hpp"
#include "audio/IAudio.hpp"      // setMasterVolume (mute)
#include "core/debug/Debug.hpp"  // DebugRuntime, DebugView
#include <cstdio>

namespace flappy {

Game::Game() = default;
Game::~Game() = default;

void Game::init(otacon::GameContext& ctx) {
    ctx_ = ctx;
    // The ordered list of progressive builds. Each milestone appends one Scene.
    builds_.push_back(std::make_unique<GeometryScene>());   // 1   — kinematics
    builds_.push_back(std::make_unique<GravityScene>());    // 1.b — gravity
    builds_.push_back(std::make_unique<TextureScene>());    // 2   — player texture
    builds_.push_back(std::make_unique<PipesScene>());      // 3   — moving pipes (geometry)
    builds_.push_back(std::make_unique<PipeTextureScene>()); // 4  — textured pipes
    builds_.push_back(std::make_unique<FlapScene>());       // 5   — flap animation + tilt
    builds_.push_back(std::make_unique<WorldScene>());      // 6   — background + ground + distance
    builds_.push_back(std::make_unique<CollisionScene>());  // 7   — collision + random skins
    builds_.push_back(std::make_unique<ScoreScene>());      // 8   — scoring, playable
    builds_.push_back(std::make_unique<HudScene>());        // 9   — HUD + menu + sound
    for (auto& b : builds_) b->init(ctx_);                  // one-time setup (textures, …)
    // Boot into the newest build so the latest work is what you see; page back
    // with '[' to revisit the earlier ones.
    switchTo(builds_.size() - 1);
    if (demo_) active().setDemo(true);   // self-play the newest build (for the GIF)
}

Scene& Game::active() const { return *builds_[current_]; }

void Game::switchTo(std::size_t index) {
    current_ = index % builds_.size();
    active().enter();
}

void Game::handleInput(const otacon::InputFrame& in) {
    if (in.isPressed(otacon::Action::ToggleSound)) {        // F9 — mute/unmute all audio
        muted_ = !muted_;
        if (ctx_.audio) ctx_.audio->setMasterVolume(muted_ ? 0.f : 1.f);
    }
    const std::size_t n = builds_.size();
    if (in.isPressed(otacon::Action::NextMode)) switchTo((current_ + 1) % n);
    else if (in.isPressed(otacon::Action::PrevMode)) switchTo((current_ + n - 1) % n);
    else if (in.isPressed(otacon::Action::Reset)) active().enter();
    active().handleInput(in);
}

void Game::update(otacon::Real dt) { active().update(dt); }

void Game::render(otacon::IRenderer& r, const otacon::DebugRuntime& dbg) {
    active().showColliders = dbg.enabled(otacon::DebugView::Colliders);   // 'C' toggle
    active().render(r);
    drawBuildIndicator(r);
}

void Game::shutdown() { builds_.clear(); }

void Game::drawBuildIndicator(otacon::IRenderer& r) const {
    // "<build n/total>  <name>   [ / ]" — the trailing brackets are the paging
    // keys (PrevMode/NextMode). Always drawn, so scene navigation is visible
    // without the dev HUD. A faint shadow keeps it legible over any sky/sprite.
    char line[96];
    std::snprintf(line, sizeof line, "%zu/%zu  %s   [ / ]",
                  current_ + 1, builds_.size(), active().name());
    r.drawText(line, 5.f, 5.f, 1.f, otacon::Color{0, 0, 0, 0.30f});          // shadow
    r.drawText(line, 4.f, 4.f, 1.f, otacon::Color{0.13f, 0.16f, 0.18f, 1.f}); // ink
}

otacon::Color Game::clearColor() const { return active().background(); }

const char* Game::statusLine() const {
    const char* extra = active().status();
    if (extra[0])
        std::snprintf(status_, sizeof status_, "BUILD %zu/%zu  %s  |  %s",
                      current_ + 1, builds_.size(), active().name(), extra);
    else
        std::snprintf(status_, sizeof status_, "BUILD %zu/%zu  %s",
                      current_ + 1, builds_.size(), active().name());
    return status_;
}

} // namespace flappy
