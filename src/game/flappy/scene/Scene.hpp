// Scene.hpp — one progressive "build" of the game, as a formal scene.
//
// The whole point of this game is to grow it one concept at a time — kinematics,
// gravity, textures, pipes, collision, scoring — and *keep each step alive* so
// you can page back to it. Rather than branch on a mode enum inside one god
// object, each step is its own Scene subclass owning only its state. The Game
// holds a list of them and forwards the frame to whichever is active.
#pragma once
#include "core/math/Scalar.hpp"
#include "render/RenderTypes.hpp"
#include "flappy/Config.hpp"

namespace otacon { class IRenderer; struct InputFrame; struct GameContext; }

namespace flappy {

class Scene {
public:
    virtual ~Scene() = default;

    virtual void init(otacon::GameContext&) {}                 // one-time setup (load textures, …)
    virtual void enter() {}                                     // (re)initialise on switch-in / reset
    virtual void handleInput(const otacon::InputFrame&) {}
    virtual void update(otacon::Real /*dt*/) {}
    virtual void render(otacon::IRenderer&) const = 0;

    // The frame is cleared to this before render() runs; defaults to the sky.
    virtual otacon::Color background() const { return cfg::kSky; }
    virtual const char* name() const = 0;
    // Optional live tunables/state for this build, shown in the HUD status line.
    virtual const char* status() const { return ""; }

    // Collision-box overlay, toggled by the engine's 'C' key. The Game sets
    // `showColliders` each frame from the debug runtime; builds that have hit
    // boxes override drawColliders() to outline them.
    bool showColliders = false;
    virtual void drawColliders(otacon::IRenderer&) const {}

    // Self-playing demo mode (for recording a GIF); only HudScene implements it.
    virtual void setDemo(bool) {}
};

} // namespace flappy
