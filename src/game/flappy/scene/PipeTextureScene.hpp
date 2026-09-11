// PipeTextureScene.hpp — Build 4: textured pipes.
//
// Same scrolling field as Build 3, but each pair is now the real pipe sprite
// instead of a green rectangle. The bottom pipe is the sprite upright (lip at the
// gap); the top pipe is the same sprite flipped vertically (lip pointing down).
// The lip is drawn at native height and only the shaft is stretched, so pipes of
// any height keep a crisp mouth. Still no collision.
#pragma once
#include "flappy/scene/PipesScene.hpp"

namespace otacon { struct GameContext; }

namespace flappy {

// Not 'final': Build 5 (FlapScene) extends this, animating/tilting the bird.
class PipeTextureScene : public PipesScene {
public:
    ~PipeTextureScene() override;
    void init(otacon::GameContext& ctx) override;
    const char* name() const override { return "4 - pipe texture"; }

protected:
    void drawPipes(otacon::IRenderer& r) const override;   // textured pipes (replaces green rects)

    // Active pipe texture (drawn by drawPipes); defaults to the owned green one.
    // Build 7 repoints it to a randomly chosen colour (no ownership change).
    otacon::TextureHandle pipeTex_   = 0;
    otacon::TextureHandle greenPipe_ = 0;        // owned here

private:
    // Draw one pipe whose gap-facing mouth is at `mouthY`. `mouthUp` = bottom
    // pipe (shaft runs down); otherwise top pipe (flipped, shaft runs up).
    void drawPipe(otacon::IRenderer& r, float x, float mouthY, bool mouthUp, float height) const;

    otacon::IRenderer* renderer_ = nullptr;
};

} // namespace flappy
