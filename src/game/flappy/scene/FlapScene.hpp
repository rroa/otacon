// FlapScene.hpp — Build 5: the bird comes alive.
//
// Everything from Build 4 (textured pipes scrolling, gravity, flap), plus the two
// things that give Flappy its feel: the wing animates (up/mid/down frames cycle)
// and the whole bird tilts by its vertical velocity — nose up the instant you
// flap, rotating toward a steep nose-dive as it falls. Still no collision; this
// build is purely about how the player looks and moves.
#pragma once
#include "scene/Animator.hpp"
#include "flappy/scene/PipeTextureScene.hpp"

namespace otacon { struct GameContext; }

namespace flappy {

// Not 'final': Build 6 (WorldScene) extends this, adding background + ground.
class FlapScene : public PipeTextureScene {
public:
    ~FlapScene() override;
    void init(otacon::GameContext& ctx) override;
    void enter() override;
    void update(otacon::Real dt) override;
    const char* name() const override { return "5 - flap + tilt"; }

protected:
    void drawBird(otacon::IRenderer& r) const override;   // animated + rotated

    // The active 3-frame wing set drawn by drawBird. Defaults to the owned yellow
    // set; Build 7 repoints it to a randomly chosen colour (no ownership change).
    const otacon::TextureHandle* birdFrames_ = nullptr;
    otacon::TextureHandle        yellowBird_[3] = {0, 0, 0};   // up, mid, down — owned here
    otacon::Animator             anim_;                        // wing cycle clock (Build 9 advances it in Ready)

private:
    otacon::IRenderer* renderer_ = nullptr;
};

} // namespace flappy
