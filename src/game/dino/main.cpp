#include "App.hpp"
#include "dino/DinoGame.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>

#ifndef OTACON_ASSET_DIR
#define OTACON_ASSET_DIR "assets"
#endif

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IOLBF, 0);

    const char* capturePath = nullptr;
    int captureFrames = 60;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--capture") == 0 && i + 1 < argc) capturePath = argv[++i];
        else if (std::strcmp(argv[i], "--frames") == 0 && i + 1 < argc) captureFrames = std::atoi(argv[++i]);
    }

    otacon::WindowConfig cfg;
    cfg.title = "Dino - Otacon engine";
    cfg.logicalWidth = 600;
    cfg.logicalHeight = 150;
    cfg.width = 1200;
    cfg.height = 300;
#if defined(OTACON_BACKEND_GL_LEGACY)
    cfg.api = otacon::GraphicsApi::OpenGLLegacy;
#elif defined(OTACON_BACKEND_VULKAN)
    cfg.api = otacon::GraphicsApi::Vulkan;
#else
    cfg.api = otacon::GraphicsApi::OpenGLModern;
#endif

    dino::DinoGame game;
    otacon::App app;
    if (!app.init(cfg, &game, OTACON_ASSET_DIR)) {
        std::fprintf(stderr, "Failed to initialize Otacon app.\n");
        return 1;
    }
    if (capturePath) app.setCapture(capturePath, captureFrames);
    std::printf("Controls: SPACE/click=jump  E=speed drop  1-4 or ]/[=mode  F6=cycle art reveal  F8=geometry/art\n"
                "  F1=deterministic/random obstacles  F2=collision-part boxes  C=entity AABB  G=grid  Z=wireframe\n"
                "  H=perf  F5=timestep  +/-=time scale  F12=screenshot  P=pause  O=step  R=reset  ESC=quit\n");
    app.run();
    app.shutdown();
    return 0;
}
