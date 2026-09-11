/*
===========================================================================

OTACON ENGINE
IGame.hpp - the game contract

The engine (App) owns the window, renderer, timing and debug runtime, and
drives an IGame through init/input/update/render. Each game is one IGame;
another is simply another implementation. Games never touch GLFW/GL/Vulkan — they
see only this context and the engine's scene/render/math APIs.

===========================================================================
*/
#pragma once
#include "core/math/Scalar.hpp"
#include "render/RenderTypes.hpp"

namespace otacon {

class IRenderer;
class IWindow;
class IAudio;
class DebugRuntime;
class Resources;
struct InputFrame;

struct GameContext {
    IRenderer*    renderer = nullptr;
    IWindow*      window   = nullptr;
    IAudio*       audio    = nullptr;
    DebugRuntime* debug    = nullptr;
    // Load-once cache for textures and sounds, owned by the App. Ambient rather
    // than per-game because the correct thing here is a single owner: two
    // systems loading the same file should get the same handle, and everything
    // must be released while the renderer is still alive. A game that manages
    // its own textures may ignore this, but then it owns that ordering problem.
    Resources*    resources = nullptr;
    int           logicalW = 480;
    int           logicalH = 320;
    const char*   assetDir = "";
};

class IGame {
public:
    virtual ~IGame() = default;

    virtual void init(GameContext& ctx) = 0;
    virtual void handleInput(const InputFrame& in) = 0;   // jump, mode switching, reset…
    virtual void update(Real dt) = 0;                     // one fixed sim step
    virtual void render(IRenderer& r, const DebugRuntime& dbg) = 0;
    // Called before the renderer is torn down, so games can free GPU resources.
    virtual void shutdown() {}

    virtual Color       clearColor() const { return Color::rgb(0xb0b0bf); }
    virtual const char* title() const = 0;
    virtual const char* statusLine() const { return ""; }   // shown in the HUD
};

} // namespace otacon
