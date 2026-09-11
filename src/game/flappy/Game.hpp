// Game.hpp — the Flappy Bird IGame: a thin shell over a list of build scenes.
//
// It owns no gameplay state of its own. Its whole job is to hold the ordered
// list of builds, forward each frame (input / update / render / clear color) to
// the active one, and let you page between builds at runtime with the engine's
// NextMode / PrevMode actions. New milestones are added by pushing one more
// Scene in init() — nothing else here changes.
#pragma once
#include "IGame.hpp"
#include <cstddef>
#include <memory>
#include <vector>

namespace flappy {

class Scene;

class Game final : public otacon::IGame {
public:
    Game();
    ~Game() override;   // out-of-line so unique_ptr<Scene> sees the complete type in Game.cpp

    void init(otacon::GameContext& ctx) override;
    void handleInput(const otacon::InputFrame& in) override;
    void update(otacon::Real dt) override;
    void render(otacon::IRenderer& r, const otacon::DebugRuntime& dbg) override;
    void shutdown() override;

    otacon::Color       clearColor() const override;
    const char*         title() const override { return "Flappy Bird - Otacon engine"; }
    const char*         statusLine() const override;

    void enableDemo() { demo_ = true; }   // call before init() to self-play the newest build

private:
    otacon::GameContext ctx_{};
    std::vector<std::unique_ptr<Scene>> builds_;
    std::size_t         current_ = 0;
    bool                muted_ = false;
    bool                demo_  = false;
    mutable char        status_[160]{};

    Scene&  active() const;
    void    switchTo(std::size_t index);
    // A small always-on label (drawn in the game, not the dev HUD) so you can
    // see which build you're on and page between them even in clean play.
    void    drawBuildIndicator(otacon::IRenderer& r) const;
};

} // namespace flappy
