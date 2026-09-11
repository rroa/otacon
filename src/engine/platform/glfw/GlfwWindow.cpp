/*
===========================================================================

OTACON ENGINE
platform/glfw/GlfwWindow.cpp - GLFW window backend

Window, context and input, behind IWindow. This file and the physical key
table in it are the only places in the engine that name a key code: every
layer above sees logical Actions, which is what lets a game be rebound or
re-hosted without touching game code.

===========================================================================
*/
#include "platform/Window.hpp"
#if defined(OTACON_BACKEND_VULKAN)
#  define GLFW_INCLUDE_VULKAN            // makes glfw declare glfwCreateWindowSurface
#endif
#include <GLFW/glfw3.h>
#include <cstdio>
#include <cstring>

namespace otacon {

class GlfwWindow final : public IWindow {
public:
    explicit GlfwWindow(const WindowConfig& cfg) : api_(cfg.api) {
        if (!glfwInit()) { std::fprintf(stderr, "[glfw] init failed\n"); return; }
        glfwSetErrorCallback([](int e, const char* d){ std::fprintf(stderr, "[glfw] error %d: %s\n", e, d); });

        switch (cfg.api) {
            case GraphicsApi::OpenGLLegacy:
                // Fixed-function needs a compatibility (non-core) 2.1 context.
                glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
                glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
                glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
                break;
            case GraphicsApi::OpenGLModern:
                // Core 3.3; macOS additionally requires forward-compat + core profile.
                glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
                glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
                glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
                glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
                glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
                break;
            case GraphicsApi::Vulkan:
                glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
                break;
        }
        win_ = glfwCreateWindow(cfg.width, cfg.height, cfg.title.c_str(), nullptr, nullptr);
        if (!win_) { std::fprintf(stderr, "[glfw] window creation failed\n"); return; }
        glfwSetWindowUserPointer(win_, this);
        glfwSetKeyCallback(win_, &GlfwWindow::keyCb);
        glfwSetMouseButtonCallback(win_, &GlfwWindow::mouseCb);
        // Hide the OS arrow: the game draws its own software cursor (which the
        // clean-play toggle `\`` then hides too). Position still tracks normally.
        glfwSetInputMode(win_, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
        if (cfg.api != GraphicsApi::Vulkan) {
            glfwMakeContextCurrent(win_);
            glfwSwapInterval(cfg.vsync ? 1 : 0);
        }
    }
    ~GlfwWindow() override {
        if (win_) glfwDestroyWindow(win_);
        glfwTerminate();
    }
    bool valid() const { return win_ != nullptr; }

    void pollEvents() override {
        std::memset(frame_.pressed, 0, sizeof(frame_.pressed));   // edges last one frame
        // Carry over edges captured in callbacks since the previous poll.
        std::memcpy(frame_.pressed, pending_, sizeof(pending_));
        std::memset(pending_, 0, sizeof(pending_));
        frame_.dragPressed = pendingDragPress_;  pendingDragPress_ = false;
        frame_.dragReleased = pendingDragRelease_; pendingDragRelease_ = false;
        frame_.selectSlot = pendingSelect_; pendingSelect_ = -1;
        glfwPollEvents();
        // Level-triggered "held" for the jump action (touch analog).
        bool jump = key(GLFW_KEY_SPACE) || key(GLFW_KEY_UP) || key(GLFW_KEY_W) ||
                    glfwGetMouseButton(win_, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
        frame_.held[int(Action::Jump)] = jump;
        // Pointer state for debug grab/drag (right mouse button).
        frame_.dragHeld = glfwGetMouseButton(win_, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
        double cx = 0, cy = 0; int ww = 1, wh = 1;
        glfwGetCursorPos(win_, &cx, &cy);
        glfwGetWindowSize(win_, &ww, &wh);
        frame_.mouseNx = ww > 0 ? float(cx) / float(ww) : 0.f;
        frame_.mouseNy = wh > 0 ? float(cy) / float(wh) : 0.f;
    }
    bool shouldClose() const override { return win_ ? glfwWindowShouldClose(win_) : true; }
    void requestClose() override { if (win_) glfwSetWindowShouldClose(win_, GLFW_TRUE); }
    const InputFrame& input() const override { return frame_; }

    void framebufferSize(int& w, int& h) const override {
        w = h = 0; if (win_) glfwGetFramebufferSize(win_, &w, &h);
    }
    double time() const override { return glfwGetTime(); }

    void  makeContextCurrent() override { if (win_ && api_ != GraphicsApi::Vulkan) glfwMakeContextCurrent(win_); }
    void  swapBuffers() override { if (win_ && api_ != GraphicsApi::Vulkan) glfwSwapBuffers(win_); }
    void* glProcAddress(const char* name) override { return (void*)glfwGetProcAddress(name); }

    void vulkanInstanceExtensions(const char**& exts, uint32_t& count) override {
        exts = glfwGetRequiredInstanceExtensions(&count);
    }
    bool createVulkanSurface(VkInstance inst, VkSurfaceKHR& surface) override {
#if defined(OTACON_BACKEND_VULKAN)
        return glfwCreateWindowSurface((VkInstance)inst, win_,
                                       nullptr, (VkSurfaceKHR*)&surface) == 0;
#else
        (void)inst; (void)surface; return false;
#endif
    }

private:
    bool key(int k) const { return glfwGetKey(win_, k) == GLFW_PRESS; }

    void edge(Action a) { pending_[int(a)] = true; }

    static void keyCb(GLFWwindow* w, int key, int, int action, int) {
        if (action != GLFW_PRESS) return;
        auto* self = static_cast<GlfwWindow*>(glfwGetWindowUserPointer(w));
        switch (key) {
            case GLFW_KEY_SPACE: case GLFW_KEY_UP: case GLFW_KEY_W: self->edge(Action::Jump); break;
            case GLFW_KEY_RIGHT_BRACKET: case GLFW_KEY_PERIOD:      self->edge(Action::NextMode); break;
            case GLFW_KEY_LEFT_BRACKET:  case GLFW_KEY_COMMA:       self->edge(Action::PrevMode); break;
            case GLFW_KEY_TAB: case GLFW_KEY_V:                     self->edge(Action::CyclePreset); break;
            case GLFW_KEY_P:                                        self->edge(Action::TogglePause); break;
            case GLFW_KEY_O:                                        self->edge(Action::Step); break;
            case GLFW_KEY_C:                                        self->edge(Action::ToggleColliders); break;
            case GLFW_KEY_X:                                        self->edge(Action::ToggleParallax); break;
            case GLFW_KEY_Z:                                        self->edge(Action::ToggleWireframe); break;
            case GLFW_KEY_G:                                        self->edge(Action::ToggleGrid); break;
            case GLFW_KEY_R:                                        self->edge(Action::Reset); break;
            case GLFW_KEY_ESCAPE:                                   self->edge(Action::Quit); break;
            // Direct mode selection by number key (1..4 and keypad 1..4).
            case GLFW_KEY_1: case GLFW_KEY_KP_1: self->pendingSelect_ = 0; break;
            case GLFW_KEY_2: case GLFW_KEY_KP_2: self->pendingSelect_ = 1; break;
            case GLFW_KEY_3: case GLFW_KEY_KP_3: self->pendingSelect_ = 2; break;
            case GLFW_KEY_4: case GLFW_KEY_KP_4: self->pendingSelect_ = 3; break;
            // F-keys: aux/engine toggles.
            case GLFW_KEY_F1: self->edge(Action::Aux1); break;
            case GLFW_KEY_F2: self->edge(Action::Aux2); break;
            case GLFW_KEY_F3: self->edge(Action::Aux3); break;
            case GLFW_KEY_F4: self->edge(Action::Aux4); break;
            case GLFW_KEY_F5: self->edge(Action::ToggleTimestep); break;
            case GLFW_KEY_F6: self->edge(Action::Aux5); break;
            case GLFW_KEY_F7: self->edge(Action::Aux7); break;   // game: particles on/off
            case GLFW_KEY_F8: self->edge(Action::Aux8); break;   // game: all sprites on/off
            case GLFW_KEY_F9:  self->edge(Action::ToggleSound); break;  // game: mute/unmute
            case GLFW_KEY_F10: self->edge(Action::CycleMusic); break;   // game: next music track
            case GLFW_KEY_E:  self->edge(Action::Aux6); break;   // game: emit particles
            // dev/debug tools
            case GLFW_KEY_GRAVE_ACCENT:                   self->edge(Action::ToggleUi); break;
            case GLFW_KEY_M:                              self->edge(Action::ToggleCursor); break;
            case GLFW_KEY_H:                              self->edge(Action::TogglePerf); break;
            case GLFW_KEY_EQUAL: case GLFW_KEY_KP_ADD:    self->edge(Action::TimeFaster); break;
            case GLFW_KEY_MINUS: case GLFW_KEY_KP_SUBTRACT: self->edge(Action::TimeSlower); break;
            case GLFW_KEY_F12:                            self->edge(Action::Screenshot); break;
            default: break;
        }
    }
    static void mouseCb(GLFWwindow* w, int button, int action, int) {
        auto* self = static_cast<GlfwWindow*>(glfwGetWindowUserPointer(w));
        if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
            self->edge(Action::Jump);
        if (button == GLFW_MOUSE_BUTTON_RIGHT) {
            if (action == GLFW_PRESS)   self->pendingDragPress_ = true;
            if (action == GLFW_RELEASE) self->pendingDragRelease_ = true;
        }
    }

    GLFWwindow*  win_ = nullptr;
    GraphicsApi  api_;
    InputFrame   frame_;
    bool         pending_[int(Action::Count)] = {};
    bool         pendingDragPress_ = false, pendingDragRelease_ = false;
    int          pendingSelect_ = -1;
};

/*
==================
createWindowGlfw

The factory PlatformFactory.cpp resolves to when GLFW is compiled in.
==================
*/
IWindow* createWindowGlfw(const WindowConfig& cfg) {
    auto* w = new GlfwWindow(cfg);
    if (!w->valid()) { delete w; return nullptr; }
    return w;
}

} // namespace otacon
