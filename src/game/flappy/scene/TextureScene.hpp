// TextureScene.hpp — Build 2: the player, textured.
//
// Behaviour is identical to the gravity build (it *is* a GravityScene): same
// fall, same flap, same live gravity tuning. The only change is that the bird is
// now drawn with the real sprite instead of a yellow square. The world around it
// is still empty — only the player is textured here.
#pragma once
#include "asset/Resources.hpp"
#include "flappy/scene/GravityScene.hpp"

namespace otacon { struct GameContext; }

namespace flappy {

// Not 'final': Build 3 (PipesScene) extends this, reusing the textured player.
class TextureScene : public GravityScene {
public:
    ~TextureScene() override;
    void init(otacon::GameContext& ctx) override;     // load the bird sprite
    void render(otacon::IRenderer& r) const override; // blit it instead of a rect
    const char* name() const override { return "2 - player texture"; }

protected:
    // How the player is drawn. Overridden by Build 5 to animate + tilt the bird;
    // here it's a single static frame centered on the bird's box.
    virtual void drawBird(otacon::IRenderer& r) const;

protected:
    otacon::IRenderer*    renderer_ = nullptr;
    // The engine's cache owns the textures; the scenes only hold handles, so
    // there is nothing to free and no teardown ordering to get wrong. Protected
    // because every later build derives from this one and loads through it.
    otacon::Resources*    res_ = nullptr;

private:
    otacon::TextureHandle tex_ = 0;
};

} // namespace flappy
