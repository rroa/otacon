// main.cpp — entry point. Wires the Canabalt game onto the Otacon engine and
// picks the window's graphics context to match the compiled-in backend.
#include "App.hpp"
#include "canabalt/CanabaltGame.hpp"
#include <cstdio>
#include <cstring>
#include <cstdlib>

#ifndef OTACON_ASSET_DIR
#define OTACON_ASSET_DIR "assets"
#endif

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IOLBF, 0);   // line-buffered logs

    // --capture <path> [--frames N]: render N deterministic frames, save a PNG, quit.
    const char* capturePath = nullptr;
    int captureFrames = 60;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--capture") == 0 && i + 1 < argc) capturePath = argv[++i];
        else if (std::strcmp(argv[i], "--frames") == 0 && i + 1 < argc) captureFrames = std::atoi(argv[++i]);
    }

    otacon::WindowConfig cfg;
    cfg.title  = "Canabalt - Otacon engine";
    cfg.width  = 960;     // 480x320 logical, presented at 2x
    cfg.height = 640;
#if defined(OTACON_BACKEND_GL_LEGACY)
    cfg.api = otacon::GraphicsApi::OpenGLLegacy;
#elif defined(OTACON_BACKEND_VULKAN)
    cfg.api = otacon::GraphicsApi::Vulkan;
#else
    cfg.api = otacon::GraphicsApi::OpenGLModern;
#endif

    canabalt::CanabaltGame game;
    otacon::App app;
    if (!app.init(cfg, &game, OTACON_ASSET_DIR)) {
        std::fprintf(stderr, "Failed to initialize Otacon app.\n");
        return 1;
    }
    if (capturePath) app.setCapture(capturePath, captureFrames);
    std::printf("Controls: SPACE/click=jump  1-4 or ]/[=mode  F6=reveal sprites (cycle)  F8=all sprites on/off\n"
                "  F7=particles on/off  E=burst sparks at mouse (mode 1)  right-mouse-drag=grab box  `=hide dev UI\n"
                "  F1=RNG  F2=quake  F3/F4=quake intensity  F5=timestep  H=perf+mem (hover=inspect)\n"
                "  +/-=time scale  F12=screenshot  V=view  C=colliders  X=parallax  Z=wireframe  G=grid\n"
                "  P=pause  O=step  R=reset  ESC=quit\n");
    app.run();
    app.shutdown();
    return 0;
}
