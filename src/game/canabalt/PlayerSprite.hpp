// PlayerSprite.hpp — a scene node that draws the player as an animated sprite.
//
// It owns the player sprite sheet and the run/jump/fall animation clock, and
// draws whichever frame matches the player's motion. The game adds it to the
// Scene between the buildings and the foreground particles, points it at the
// active mode's player, and toggles its visibility with the F6 reveal.
#pragma once
#include "asset/Resources.hpp"
#include "scene/Node.hpp"
#include "render/IRenderer.hpp"

namespace canabalt {

class Player;

class PlayerSpriteNode final : public otacon::Node {
public:
    void load(otacon::Resources* res, const char* assetDir);   // player2.png
    void destroy(otacon::IRenderer* r);
    bool hasTexture() const { return tex_ != 0; }

    // Point at the mode's player. Resets the animation clock only when the
    // target actually changes (so a restart re-points without restarting mid-air
    // every frame).
    void setTarget(Player* p);

    void update(otacon::Real dt, const otacon::Camera& cam) override;
    void render(otacon::IRenderer& r, const otacon::Camera& cam) const override;

private:
    int frame() const;                  // run 0-15, jump 16-19, fall 20-26, stumble 27-37

    Player* target_ = nullptr;
    otacon::TextureHandle tex_ = 0;
    int texW_ = 0, texH_ = 0;
    otacon::Real animTime_ = otacon::R(0);      // run-cycle clock
    otacon::Real airTime_  = otacon::R(0);      // time since leaving the ground
    otacon::Real stumbleTime_ = otacon::R(-1);  // hard-landing roll clock (<0 = not rolling)
};

} // namespace canabalt
