#include "App.hpp"
#include "flappy/Game.hpp"
#include "flappy/Config.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>

#ifndef OTACON_ASSET_DIR
#define OTACON_ASSET_DIR "assets"
#endif

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IOLBF, 0);

    const char* capturePath = nullptr;
    const char* recordDir = nullptr;
    int captureFrames = 60;
    bool demo = false;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--capture") == 0 && i + 1 < argc) capturePath = argv[++i];
        else if (std::strcmp(argv[i], "--frames") == 0 && i + 1 < argc) captureFrames = std::atoi(argv[++i]);
        else if (std::strcmp(argv[i], "--record") == 0 && i + 1 < argc) recordDir = argv[++i];
        else if (std::strcmp(argv[i], "--demo") == 0) demo = true;
    }

    otacon::WindowConfig cfg;
    cfg.title         = "Flappy Bird - Otacon engine";
    cfg.logicalWidth  = flappy::cfg::kLogicalW;
    cfg.logicalHeight = flappy::cfg::kLogicalH;
    cfg.width         = flappy::cfg::kLogicalW * flappy::cfg::kWindowScale;
    cfg.height        = flappy::cfg::kLogicalH * flappy::cfg::kWindowScale;
#if defined(OTACON_BACKEND_GL_LEGACY)
    cfg.api = otacon::GraphicsApi::OpenGLLegacy;
#elif defined(OTACON_BACKEND_VULKAN)
    cfg.api = otacon::GraphicsApi::Vulkan;
#else
    cfg.api = otacon::GraphicsApi::OpenGLModern;
#endif

    flappy::Game game;
    if (demo) game.enableDemo();                 // self-play the newest build
    otacon::App app;
    if (!app.init(cfg, &game, OTACON_ASSET_DIR)) {
        std::fprintf(stderr, "Failed to initialize Otacon app.\n");
        return 1;
    }
    if (recordDir)       app.setRecord(recordDir, captureFrames);
    else if (capturePath) app.setCapture(capturePath, captureFrames);
    std::printf("Controls: SPACE/UP/W=flap (or hold to rise in build 1)  |  F1/F2=gravity -/+\n"
                "  ]/[=next/prev build  |  C=collision boxes  |  F9=mute  |  R=reset  |  ESC=quit\n");
    app.run();
    app.shutdown();
    return 0;
}
