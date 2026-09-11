// CollisionScene.hpp — Build 7: collision, and random skins.
//
// The world from Build 6 becomes deadly: the bird's box is tested against the
// pipes and the ground, and on contact the run "crashes" — the world freezes,
// input is ignored, and the bird tumbles to the ground. (Scoring and the proper
// game-over/restart flow are Builds 8–9; here we just prove collision works.)
//
// This build also rolls the cosmetic variants Flappy randomizes per run, like it
// already does for day/night: the bird colour (yellow/red/blue) and the pipe
// colour (green/red). It owns the extra colour textures and just repoints the
// base classes' active handles — no draw code is duplicated.
#pragma once
#include "flappy/scene/WorldScene.hpp"
#include <random>

namespace otacon { struct GameContext; struct InputFrame; }

namespace flappy {

// Not 'final': Build 8 (ScoreScene) extends this, adding scoring + restart.
class CollisionScene : public WorldScene {
public:
    ~CollisionScene() override;
    void init(otacon::GameContext& ctx) override;
    void enter() override;
    void handleInput(const otacon::InputFrame& in) override;
    void update(otacon::Real dt) override;
    const char* name() const override { return "7 - collision"; }
    const char* status() const override;

protected:
    void drawColliders(otacon::IRenderer& r) const override;   // bird box + pipe rects + ground
    bool crashed_ = false;        // shared with Build 8's game-over handling

private:
    bool collides() const;        // bird box vs pipes + ground

    otacon::IRenderer*    renderer_ = nullptr;
    otacon::TextureHandle redBird_[3]  = {0, 0, 0};
    otacon::TextureHandle blueBird_[3] = {0, 0, 0};
    otacon::TextureHandle redPipe_     = 0;
    std::mt19937 skinRng_{0x5C1Du};    // not reseeded per run → skins vary across resets
    mutable char status_[48]{};
};

} // namespace flappy
