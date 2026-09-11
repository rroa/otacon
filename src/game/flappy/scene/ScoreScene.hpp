// ScoreScene.hpp — Build 8: core mechanics, playable.
//
// Build 7 made the world deadly; this makes it a game. You flap through the
// pipes, each cleared pair scores a point, and a crash ends the run with a
// game-over panel you can restart from (Space / R). The score replaces the
// distance meter at the top. The polished HUD — proper number sprites, the
// menu, sound — is Build 9.
#pragma once
#include "flappy/scene/CollisionScene.hpp"

namespace otacon { struct InputFrame; }

namespace flappy {

// Not 'final': Build 9 (HudScene) extends this, reusing the scoring + crash flow.
class ScoreScene : public CollisionScene {
public:
    void enter() override;
    void handleInput(const otacon::InputFrame& in) override;
    void update(otacon::Real dt) override;
    void render(otacon::IRenderer& r) const override;
    const char* name() const override { return "8 - scoring (playable)"; }
    const char* status() const override;

protected:
    int score_ = 0;     // pipes cleared this run (Build 9 reads it for the sprite HUD + 'point' sound)

private:
    void drawScore(otacon::IRenderer& r) const;
    void drawGameOver(otacon::IRenderer& r) const;

    mutable char status_[40]{};
};

} // namespace flappy
