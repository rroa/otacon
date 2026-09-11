// Sequence.hpp — faithful port of the original Canabalt level generator
// (reference/src/Sequence.m).
//
// The original keeps exactly TWO "sequences" that leapfrog: each owns one wide
// building (its main collision block). When a sequence scrolls off the left edge
// it `reset`s, generating the next building just past the *other* sequence. The
// gap / width / drop of each new building are driven by the player's current
// speed and jump limit, which is what makes the difficulty scale as you go
// faster. We reproduce that algorithm verbatim, including the special types'
// *collision-block shapes* — crane, billboard, leg, hallway ceiling — because
// they change how the level plays. The generator itself stays purely logical:
// it emits `Piece`s (rect + archetype + variants) and the visual dispatcher in
// Modes.cpp turns each into a facade, crane, billboard or leg. That split is why
// the F6/F8 sprite reveal can swap the whole city between solid boxes and art
// without the generator knowing.
#pragma once
#include "scene/Entity.hpp"
#include "scene/Camera.hpp"
#include "canabalt/GameRandom.hpp"
#include <cstdint>
#include <vector>

namespace canabalt {

class Player;

// Building/obstacle archetypes (same enum order as Sequence.m).
enum SeqType { ROOF, HALLWAY, COLLAPSE, BOMB, CRANE, BILLBOARD, LEG };

class SequenceField {
public:
    // `rng` is the game's shared, switchable RNG (F1 toggles its mode); the
    // generator reseeds it on init so a deterministic run is reproducible.
    void init(Player* player, GameRandom& rng);
    void update(otacon::Real dt, const otacon::Camera& cam);

    // All solid blocks the player should collide with this frame.
    std::vector<otacon::Entity*>& collisionBlocks() { return collide_; }
    // All blocks to draw (same set; kept separate for clarity).
    void appendRenderBlocks(std::vector<otacon::Entity*>& out);

    // One renderable piece of the live city: the sequence's logical rect (world
    // units) + its archetype + the variants chosen for it. The visual dispatcher
    // turns each piece into a building facade, crane, billboard, leg, etc. The
    // rect is the *logical* building rect — not the thin collision ledge a crane
    // or billboard leaves behind — so the renderers get the real extents.
    struct Piece {
        int   type;
        float x, y, w, h;             // logical sequence rect
        int   wallType, windowType;
        bool  escape;                 // fire escape on the right edge
        float aux;                    // billboard sign height OR hallway opening height
        std::uint32_t seed;           // per-building deterministic decor seed
        bool  brokeL, brokeR;         // hallway: which end windows are smashed
        float legY;                   // LEG: current world-y of the dropping leg
        float bombY;                  // BOMB: current world-y of the dropping bomb
        bool  launch;                 // the opening tunnel (gets no scattered doors)
    };
    void appendPieces(std::vector<Piece>& out);

    // A small roof obstacle for rendering (knocked ones tumble with `angle`).
    // `knocked` ones no longer collide — the debug overlay uses that to skip them.
    struct ObView { float x, y, angle; int frame; bool alt; bool knocked; };
    void appendObstacles(std::vector<ObView>& out);

    // A pigeon for rendering: sits (idle) until it flies up off the roof.
    struct DoveView { float x, y, anim; bool facing, flying; };
    void appendDoves(std::vector<DoveView>& out);

    // world-x positions where the player smashed a hallway window this frame
    // (so the game can burst glass shards there). Refilled each update().
    const std::vector<float>& windowSmashes() const { return smashes_; }
    // world-x positions where a giant leg stomped this frame (burst + quake).
    const std::vector<float>& legStomps() const { return stomps_; }
    // world-x positions where a building started collapsing / a bomb landed.
    const std::vector<float>& collapses() const { return collapses_; }
    const std::vector<float>& bombLandings() const { return bombs_; }
    // Events for the matching one-shot SFX (all refilled each update()).
    const std::vector<float>& bombDrops() const { return bombDrops_; }    // bomb begins falling
    const std::vector<float>& legDrops() const { return legDrops_; }      // leg begins dropping
    const std::vector<float>& obstacleHits() const { return obHits_; }    // ran into a roof obstacle
    const std::vector<float>& doveFlushes() const { return flushes_; }    // a pigeon took off
    bool onCrane() const;   // is the player currently over a crane beam? (metal footsteps)

    otacon::Real firstRoofY() const { return roofY0_; }

private:
    struct Seq {
        otacon::Entity block;      // main collision/visual block
        otacon::Entity ceiling;    // hallway ceiling (only when ceilingActive)
        bool  ceilingActive = false;
        float x = 0, y = 0, width = 0, height = 0;   // the sequence rect (world units)
        float drawY = 0, drawH = 0;                  // render rect (pre-collapse-shrink)
        int   type = ROOF;
        int   wallType = 0, windowType = 0;          // facade variants
        bool  hasEscape = false;                     // fire escape on the right edge
        float billH = 0;                             // billboard sign height (BILLBOARD)
        float hallPixels = 0;                        // hallway opening height (HALLWAY)
        bool  launch = false;                        // curIndex 0: the opening tunnel
        std::uint32_t decorSeed = 1;                 // per-building deterministic decor seed
        bool  passed = false;
        float winL = 1e9f, winR = 1e9f;              // hallway window x positions
        bool  brokeL = false, brokeR = false;        // already smashed?
        float legY = -480, legLandY = 0;             // LEG: drop animation
        bool  legLanded = false;
        bool  collapsing = false;                    // COLLAPSE / stomped: sinking
        float sinkVY = 0;                            // current sink speed
        bool  sinkBlock = false;                     // also sink the collision block
        float bombY = -80, bombLandY = 0;            // BOMB: drop animation
        bool  bombLanded = false;

        // Small roof obstacles you stumble over (they slow you + get kicked off).
        struct Ob {
            float x = 0, y = 0;          // sprite top-left (18x18)
            int   frame = 0;
            bool  alt = false;           // obstacles2 sheet
            bool  active = false, knocked = false;
            float vx = 0, vy = 0, angle = 0, angVel = 0;
        };
        static constexpr int kMaxOb = 4;
        Ob   obs[kMaxOb];
        int  obCount = 0;

        // Pigeons that sit on the roof and flush as the player approaches.
        struct Dv {
            float x = 0, y = 0, trigger = 0;
            bool  facing = false, active = false, flying = false;
            float vx = 0, vy = 0, ax = 0, ay = 0, anim = 0;
            float r1 = 0, r2 = 0, r3 = 0;     // rolled-once fly randomness
        };
        static constexpr int kMaxDove = 10;
        Dv   doves[kMaxDove];
        int  doveCount = 0;
    };

    void reset(Seq& self, const Seq& other);
    static void setBlock(otacon::Entity& b, float x, float y, float w, float h);

    Seq seqA_, seqB_;
    Player*     player_ = nullptr;
    GameRandom* rng_ = nullptr;        // shared with the rest of the game
    std::vector<otacon::Entity*> collide_;
    std::vector<float> smashes_;       // window-smash x positions this frame
    std::vector<float> stomps_;        // leg-stomp x positions this frame
    std::vector<float> collapses_;     // building-collapse-start x positions
    std::vector<float> bombs_;         // bomb-landing x positions
    std::vector<float> bombDrops_;     // bomb-drop-start x positions
    std::vector<float> legDrops_;      // leg-drop-start x positions
    std::vector<float> obHits_;        // obstacle-hit x positions
    std::vector<float> flushes_;       // dove-flush x positions

    // Generator state (statics in the original).
    int curIndex_ = 0, nextIndex_ = 3, nextType_ = 1;
    int lastType_ = 0, thisType_ = 0;
    float roofY0_ = 80.f;          // first building's roof Y (player spawn ref)
};

} // namespace canabalt
