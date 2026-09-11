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
struct InputFrame;

struct GameContext {
    IRenderer*    renderer = nullptr;
    IWindow*      window   = nullptr;
    IAudio*       audio    = nullptr;
    DebugRuntime* debug    = nullptr;
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
