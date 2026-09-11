/*
===========================================================================

OTACON ENGINE
platform/Window.hpp - window, input and context seam

The rest of the engine only ever sees IWindow + InputFrame, never GLFW or
SDL. A backend is chosen at build time (WINDOW_BACKEND in build.cfg) and
instantiated by createWindow(). Graphics backends ask the window for the
bits they need: a GL context (makeContextCurrent/glProcAddress/swapBuffers)
or Vulkan surface plumbing (vulkanInstanceExtensions/createVulkanSurface).

===========================================================================
*/
#pragma once
#include <cstdint>
#include <string>

// Forward-declare Vulkan handles so this header needs no Vulkan include.
struct VkInstance_T;  using VkInstance = VkInstance_T*;
struct VkSurfaceKHR_T;using VkSurfaceKHR = VkSurfaceKHR_T*;

namespace otacon {

enum class GraphicsApi { OpenGLLegacy, OpenGLModern, Vulkan };

// Logical input actions — backends map physical keys/buttons onto these so the
// game never mentions a key code. `held` is level, `pressed` is a rising edge.

/*
=============================================================================

                                LOGICAL INPUT

=============================================================================
*/
enum class Action : std::uint8_t {
    Jump,            // touch / space / mouse — the one gameplay input
    NextMode, PrevMode,
    CyclePreset, TogglePause, Step,
    ToggleColliders, ToggleParallax, ToggleWireframe, ToggleGrid,
    Reset, Quit,
    ToggleUi,        // `  — hide/show ALL dev UI (clean play)
    ToggleCursor,    // M  — show/hide the in-game software cursor
    ToggleSound,     // F9 — mute/unmute all audio
    CycleMusic,      // F10 — cycle the background music track
    ToggleTimestep,  // F5 — engine: switch variable/fixed timestep
    TogglePerf,      // H  — engine: perf/memory overlay
    TimeFaster, TimeSlower,  // = / - : simulation time scale
    Screenshot,      // F12 — save a PNG of the framebuffer
    // Generic F-key slots a game can bind to whatever it likes (the engine stays
    // game-agnostic) — e.g. mode switches, debug toggles, effect triggers.
    Aux1, Aux2, Aux3, Aux4, Aux5, Aux6, Aux7, Aux8,
    Count
};

struct InputFrame {
    bool held[int(Action::Count)]    = {};
    bool pressed[int(Action::Count)] = {};   // edge-triggered this frame
    bool isHeld(Action a)    const { return held[int(a)]; }
    bool isPressed(Action a) const { return pressed[int(a)]; }

    // Pointer (for debug grab/drag). Position is normalized [0,1] over the
    // window content so the game can scale it to logical coordinates.
    float mouseNx = 0, mouseNy = 0;
    bool  dragHeld = false, dragPressed = false, dragReleased = false;  // right mouse button

    // Direct mode selection: number keys 1..N set this to 0..N-1 (-1 = none).
    int   selectSlot = -1;
};

struct WindowConfig {
    std::string title = "Otacon";   // engine default; each game sets its own via cfg.title
    int  width  = 960;     // 480x320 logical, 2x default for visibility
    int  height = 640;
    int  logicalWidth = 480;
    int  logicalHeight = 320;
    GraphicsApi api = GraphicsApi::OpenGLModern;
    bool vsync = true;
};


/*
=============================================================================

                                  THE SEAM

=============================================================================
*/
class IWindow {
public:
    virtual ~IWindow() = default;

    virtual void  pollEvents() = 0;
    virtual bool  shouldClose() const = 0;
    virtual void  requestClose() = 0;
    virtual const InputFrame& input() const = 0;

    virtual void  framebufferSize(int& w, int& h) const = 0;
    virtual double time() const = 0;

    // OpenGL plumbing (no-ops on a Vulkan window).
    virtual void  makeContextCurrent() {}
    virtual void  swapBuffers() {}
    virtual void* glProcAddress(const char* name) { (void)name; return nullptr; }

    // Vulkan plumbing (no-ops on a GL window).
    virtual void  vulkanInstanceExtensions(const char**& exts, uint32_t& count) { exts = nullptr; count = 0; }
    virtual bool  createVulkanSurface(VkInstance, VkSurfaceKHR&) { return false; }
};

// Implemented by the compiled-in backend (platform/glfw or platform/sdl2),
// dispatched through PlatformFactory.cpp.
IWindow* createWindow(const WindowConfig& cfg);
const char* windowBackendName();

} // namespace otacon
