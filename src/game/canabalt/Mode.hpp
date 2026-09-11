// Mode.hpp — a single demo "mode" in the incremental build. Cycling through
// modes shows the progression: a static box -> gravity+collision -> a running,
// jumping player -> the infinite rooftop runner -> a textured sprite showcase.
//
// Modes are handed ModeServices: the shared GameRandom (so the level honours the
// F1 RNG toggle), the renderer (to create textures), and the asset directory.
#pragma once
#include "scene/Scene.hpp"
#include "scene/Node.hpp"
#include "platform/Window.hpp"
#include "canabalt/GameRandom.hpp"
#include <cstddef>
#include <vector>

namespace otacon { class IRenderer; class IAudio; }

namespace canabalt {

class Player;

// Progressive geometry->sprite reveal. F6 cycles the level up by one; F8 jumps
// between all-geometry (0) and all-sprite (kMax). Each level reveals one more
// category of art, so you can watch the world build up from boxes to sprites.
struct SpriteReveal {
    int level = 0;     // 0 geometry, 1 +player, 2 +buildings, 3 +background, 4 +particles
    static constexpr int kMax = 4;
    bool player()     const { return level >= 1; }
    bool buildings()  const { return level >= 2; }
    bool background() const { return level >= 3; }
    bool particles()  const { return level >= 4; }
    const char* name() const {
        switch (level) { case 0: return "GEOMETRY"; case 1: return "PLAYER";
                         case 2: return "BUILDINGS"; case 3: return "BACKGROUND";
                         default: return "FULL-SPRITES"; }
    }
};

struct ModeServices {
    GameRandom&         rng;
    otacon::IRenderer*  renderer;
    const char*         assetDir;
    otacon::IAudio*     audio = nullptr;             // sound output (null => silent)
    const bool*         particlesEnabled = nullptr;   // game-wide toggle (null => always on)
    const SpriteReveal* sprites = nullptr;            // geometry/sprite reveal level
};

class Mode {
public:
    explicit Mode(const ModeServices& s)
        : gameRng_(s.rng), renderer_(s.renderer), assetDir_(s.assetDir), audio_(s.audio),
          particlesEnabled_(s.particlesEnabled), sprites_(s.sprites) {}
    virtual ~Mode() = default;

    virtual const char* name() const = 0;
    virtual void enter(otacon::Scene& scene) = 0;
    virtual void update(otacon::Real dt, const otacon::InputFrame& in, otacon::Scene& scene) = 0;
    virtual void status(char* buf, std::size_t n) const = 0;
    // Hand the Scene the mode's scene nodes (emitters, facade drawers), split
    // into those that draw behind the player and those in front. The game adds
    // the player sprite between the two lists. Default: no nodes.
    virtual void collectNodes(std::vector<otacon::Node*>& /*background*/,
                              std::vector<otacon::Node*>& /*foreground*/) {}
    // Nodes that draw behind the entity layer (the farthest parallax). Default none.
    virtual void collectBackdrop(std::vector<otacon::Node*>& /*backdrop*/) {}
    // The mode's player, if any — the game draws it as a box or sprite per the
    // global representation toggle (F6). Modes without a player return nullptr.
    virtual Player* spriteTarget() { return nullptr; }

    // Game-wide particle toggle: emitters should check this before updating/drawing.
    bool particlesOn() const { return !particlesEnabled_ || *particlesEnabled_; }
    // Sprite-reveal queries (default geometry if no state was provided).
    bool spriteBuildings()  const { return sprites_ && sprites_->buildings(); }
    bool spriteBackground() const { return sprites_ && sprites_->background(); }
    bool spriteParticles()  const { return sprites_ && sprites_->particles(); }

protected:
    GameRandom&         gameRng_;
    otacon::IRenderer*  renderer_;
    const char*         assetDir_;
    otacon::IAudio*     audio_;
    const bool*         particlesEnabled_;
    const SpriteReveal* sprites_;
};

// Factory: modes 0..count()-1.
int   modeCount();
Mode* makeMode(int index, const ModeServices& services);

} // namespace canabalt
