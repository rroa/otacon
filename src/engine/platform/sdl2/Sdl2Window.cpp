/*
===========================================================================

OTACON ENGINE
platform/sdl2/Sdl2Window.cpp - SDL2 window backend

The same IWindow, over SDL2 instead of GLFW, mapping SDL's key codes onto the
same logical Actions.

Having two of these is what keeps the window seam honest - an interface with
exactly one implementation is only a guess at an abstraction.

===========================================================================
*/
#include "platform/Window.hpp"
#define SDL_MAIN_HANDLED                 // we own main(); don't let SDL hijack it
#include <SDL.h>
#if defined(OTACON_BACKEND_VULKAN)
#  include <SDL_vulkan.h>
#  include <vector>
#endif
#include <cstdio>
#include <cstring>

namespace otacon {

class Sdl2Window final : public IWindow {
public:
    explicit Sdl2Window(const WindowConfig& cfg) : api_(cfg.api) {
        SDL_SetMainReady();
        if (SDL_Init(SDL_INIT_VIDEO) != 0) {
            std::fprintf(stderr, "[sdl2] init failed: %s\n", SDL_GetError());
            return;
        }

        Uint32 flags = SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI;
        switch (cfg.api) {
            case GraphicsApi::OpenGLLegacy:
                flags |= SDL_WINDOW_OPENGL;
                SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
                SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
                SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
                break;
            case GraphicsApi::OpenGLModern:
                flags |= SDL_WINDOW_OPENGL;
                SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
                SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
                SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
                SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
                break;
            case GraphicsApi::Vulkan:
                flags |= SDL_WINDOW_VULKAN;
                break;
        }
        if (cfg.api != GraphicsApi::Vulkan) SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

        win_ = SDL_CreateWindow(cfg.title.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                cfg.width, cfg.height, flags);
        if (!win_) { std::fprintf(stderr, "[sdl2] window creation failed: %s\n", SDL_GetError()); return; }

        if (cfg.api != GraphicsApi::Vulkan) {
            ctx_ = SDL_GL_CreateContext(win_);
            if (!ctx_) { std::fprintf(stderr, "[sdl2] GL context failed: %s\n", SDL_GetError()); return; }
            SDL_GL_MakeCurrent(win_, ctx_);
            SDL_GL_SetSwapInterval(cfg.vsync ? 1 : 0);
        }
        // We draw our own software cursor; hide the OS arrow (matches GLFW backend).
        SDL_ShowCursor(SDL_DISABLE);
        start_ = SDL_GetPerformanceCounter();
    }

    ~Sdl2Window() override {
        if (ctx_) SDL_GL_DeleteContext(ctx_);
        if (win_) SDL_DestroyWindow(win_);
        SDL_Quit();
    }

    bool valid() const { return win_ && (api_ == GraphicsApi::Vulkan || ctx_); }

    void pollEvents() override {
        std::memset(frame_.pressed, 0, sizeof(frame_.pressed));   // edges last one frame
        frame_.dragPressed = frame_.dragReleased = false;
        frame_.selectSlot = -1;

        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            switch (e.type) {
                case SDL_QUIT: closed_ = true; break;
                case SDL_WINDOWEVENT:
                    if (e.window.event == SDL_WINDOWEVENT_CLOSE) closed_ = true;
                    break;
                case SDL_KEYDOWN:
                    if (e.key.repeat == 0) onKey(e.key.keysym.sym);
                    break;
                case SDL_MOUSEBUTTONDOWN:
                    if (e.button.button == SDL_BUTTON_LEFT)  edge(Action::Jump);
                    if (e.button.button == SDL_BUTTON_RIGHT) frame_.dragPressed = true;
                    break;
                case SDL_MOUSEBUTTONUP:
                    if (e.button.button == SDL_BUTTON_RIGHT) frame_.dragReleased = true;
                    break;
                default: break;
            }
        }

        // Level-triggered "held": jump (space/up/W/left-mouse) and the drag button.
        const Uint8* ks = SDL_GetKeyboardState(nullptr);
        int mx = 0, my = 0;
        Uint32 mb = SDL_GetMouseState(&mx, &my);
        frame_.held[int(Action::Jump)] = ks[SDL_SCANCODE_SPACE] || ks[SDL_SCANCODE_UP] ||
                                         ks[SDL_SCANCODE_W] || (mb & SDL_BUTTON(SDL_BUTTON_LEFT));
        frame_.dragHeld = (mb & SDL_BUTTON(SDL_BUTTON_RIGHT)) != 0;
        int ww = 1, wh = 1; SDL_GetWindowSize(win_, &ww, &wh);
        frame_.mouseNx = ww > 0 ? float(mx) / float(ww) : 0.f;
        frame_.mouseNy = wh > 0 ? float(my) / float(wh) : 0.f;
    }

    bool shouldClose() const override { return closed_ || !win_; }
    void requestClose() override { closed_ = true; }
    const InputFrame& input() const override { return frame_; }

    void framebufferSize(int& w, int& h) const override {
        w = h = 0;
        if (!win_) return;
#if defined(OTACON_BACKEND_VULKAN)
        if (api_ == GraphicsApi::Vulkan) { SDL_Vulkan_GetDrawableSize(win_, &w, &h); return; }
#endif
        SDL_GL_GetDrawableSize(win_, &w, &h);
    }
    double time() const override {
        return double(SDL_GetPerformanceCounter() - start_) / double(SDL_GetPerformanceFrequency());
    }

    void  makeContextCurrent() override { if (win_ && ctx_) SDL_GL_MakeCurrent(win_, ctx_); }
    void  swapBuffers() override { if (win_ && ctx_) SDL_GL_SwapWindow(win_); }
    void* glProcAddress(const char* name) override { return SDL_GL_GetProcAddress(name); }

    void vulkanInstanceExtensions(const char**& exts, uint32_t& count) override {
#if defined(OTACON_BACKEND_VULKAN)
        unsigned int n = 0;
        SDL_Vulkan_GetInstanceExtensions(win_, &n, nullptr);
        vkExts_.resize(n);
        SDL_Vulkan_GetInstanceExtensions(win_, &n, vkExts_.data());
        exts = vkExts_.data(); count = n;
#else
        exts = nullptr; count = 0;
#endif
    }
    bool createVulkanSurface(VkInstance inst, VkSurfaceKHR& surface) override {
#if defined(OTACON_BACKEND_VULKAN)
        return SDL_Vulkan_CreateSurface(win_, (VkInstance)inst, (VkSurfaceKHR*)&surface) == SDL_TRUE;
#else
        (void)inst; (void)surface; return false;
#endif
    }

private:
    void edge(Action a) { frame_.pressed[int(a)] = true; }

    // Mirror of the GLFW key map (keycodes -> actions).
    void onKey(SDL_Keycode k) {
        switch (k) {
            case SDLK_SPACE: case SDLK_UP: case SDLK_w: edge(Action::Jump); break;
            case SDLK_RIGHTBRACKET: case SDLK_PERIOD:   edge(Action::NextMode); break;
            case SDLK_LEFTBRACKET:  case SDLK_COMMA:    edge(Action::PrevMode); break;
            case SDLK_TAB: case SDLK_v:                 edge(Action::CyclePreset); break;
            case SDLK_p: edge(Action::TogglePause); break;
            case SDLK_o: edge(Action::Step); break;
            case SDLK_c: edge(Action::ToggleColliders); break;
            case SDLK_x: edge(Action::ToggleParallax); break;
            case SDLK_z: edge(Action::ToggleWireframe); break;
            case SDLK_g: edge(Action::ToggleGrid); break;
            case SDLK_r: edge(Action::Reset); break;
            case SDLK_ESCAPE: edge(Action::Quit); break;
            case SDLK_1: case SDLK_KP_1: frame_.selectSlot = 0; break;
            case SDLK_2: case SDLK_KP_2: frame_.selectSlot = 1; break;
            case SDLK_3: case SDLK_KP_3: frame_.selectSlot = 2; break;
            case SDLK_4: case SDLK_KP_4: frame_.selectSlot = 3; break;
            case SDLK_F1: edge(Action::Aux1); break;
            case SDLK_F2: edge(Action::Aux2); break;
            case SDLK_F3: edge(Action::Aux3); break;
            case SDLK_F4: edge(Action::Aux4); break;
            case SDLK_F5: edge(Action::ToggleTimestep); break;
            case SDLK_F6: edge(Action::Aux5); break;
            case SDLK_F7: edge(Action::Aux7); break;   // game: particles on/off
            case SDLK_F8: edge(Action::Aux8); break;   // game: all sprites on/off
            case SDLK_F9: edge(Action::ToggleSound); break;   // game: mute/unmute
            case SDLK_F10: edge(Action::CycleMusic); break;   // game: next music track
            case SDLK_e: edge(Action::Aux6); break;    // game: emit particles
            case SDLK_BACKQUOTE: edge(Action::ToggleUi); break;
            case SDLK_m: edge(Action::ToggleCursor); break;
            case SDLK_h: edge(Action::TogglePerf); break;
            case SDLK_EQUALS: case SDLK_KP_PLUS:  edge(Action::TimeFaster); break;
            case SDLK_MINUS:  case SDLK_KP_MINUS: edge(Action::TimeSlower); break;
            case SDLK_F12: edge(Action::Screenshot); break;
            default: break;
        }
    }

    SDL_Window*   win_ = nullptr;
    SDL_GLContext ctx_ = nullptr;
    GraphicsApi   api_;
    InputFrame    frame_;
    bool          closed_ = false;
    Uint64        start_ = 0;
#if defined(OTACON_BACKEND_VULKAN)
    std::vector<const char*> vkExts_;
#endif
};

/*
==================
createWindowSdl2

The factory PlatformFactory.cpp resolves to when SDL2 is compiled in.
==================
*/
IWindow* createWindowSdl2(const WindowConfig& cfg) {
    auto* w = new Sdl2Window(cfg);
    if (!w->valid()) { delete w; return nullptr; }
    return w;
}

} // namespace otacon
