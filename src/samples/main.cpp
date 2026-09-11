// main.cpp — entry point for the Otacon engine samples.
//
// Wires the Gallery onto the engine and picks the window's graphics context to
// match the compiled-in backend, exactly as each game's main() does. Every
// sample runs on every backend, so this file has no backend-specific logic
// beyond choosing the context type.
#include "App.hpp"
#include "Gallery.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IOLBF, 0);

    const char* capturePath = nullptr;
    const char* recordDir = nullptr;
    int captureFrames = 60;
    int startSample = 0;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--capture") == 0 && i + 1 < argc) capturePath = argv[++i];
        else if (std::strcmp(argv[i], "--frames") == 0 && i + 1 < argc) captureFrames = std::atoi(argv[++i]);
        else if (std::strcmp(argv[i], "--record") == 0 && i + 1 < argc) recordDir = argv[++i];
        // --sample N opens sample N (1-based) directly: what the pixel diff and
        // any screenshot tooling needs in order to target one screen.
        else if (std::strcmp(argv[i], "--sample") == 0 && i + 1 < argc) startSample = std::atoi(argv[++i]) - 1;
    }

    otacon::WindowConfig cfg;
    cfg.title         = "Otacon Engine Samples";
    cfg.logicalWidth  = 640;
    cfg.logicalHeight = 400;
    cfg.width         = 1280;
    cfg.height        = 800;
#if defined(OTACON_BACKEND_GL_LEGACY)
    cfg.api = otacon::GraphicsApi::OpenGLLegacy;
#elif defined(OTACON_BACKEND_VULKAN)
    cfg.api = otacon::GraphicsApi::Vulkan;
#else
    cfg.api = otacon::GraphicsApi::OpenGLModern;
#endif

    samples::Gallery gallery;
    gallery.selectAtStartup(startSample);

    otacon::App app;
    if (!app.init(cfg, &gallery, "")) {
        std::fprintf(stderr, "Failed to initialize Otacon app.\n");
        return 1;
    }
    if (recordDir)        app.setRecord(recordDir, captureFrames);
    else if (capturePath) app.setCapture(capturePath, captureFrames);

    std::printf("Otacon engine samples — %d screens\n"
                "  F1 = sample list   [ / ] = prev/next   1-0 = jump   F3 = +10\n"
                "  F2 = help (per-sample keys)   R = reset   ` = clean view\n"
                "  H = perf+memory   +/- = time scale   F12 = screenshot   ESC = quit\n",
                samples::sampleCount());
    app.run();
    app.shutdown();
    return 0;
}
