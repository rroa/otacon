/*
===========================================================================

OTACON ENGINE
App.cpp - the engine runtime

Owns the window, renderer, audio device, clock and debug runtime, and drives
an IGame through init / input / update / render.

This is the only file that knows the order those things happen in. A game
sees a dt and a renderer; it never polls an event queue, never swaps a
buffer, and never decides how many simulation steps a frame is worth.

===========================================================================
*/
#include "App.hpp"
#include "IGame.hpp"
#include "render/IRenderer.hpp"
#include "audio/IAudio.hpp"
#include "core/memory/Memory.hpp"
#include <algorithm>
#include <cstdio>
#include <cstring>

namespace otacon {

/*
==================
App::init

Bring up the window, then the renderer, then audio, then the game. The order
matters: the renderer needs a live context, and the game's init() is where
it uploads textures, so it has to come last.
==================
*/
bool App::init(const WindowConfig& cfg, IGame* game, const char* assetDir) {
    logicalW_ = cfg.logicalWidth > 0 ? cfg.logicalWidth : 480;
    logicalH_ = cfg.logicalHeight > 0 ? cfg.logicalHeight : 320;
    game_ = game;

    window_ = createWindow(cfg);
    if (!window_) { std::fprintf(stderr, "[app] window creation failed (%s)\n", windowBackendName()); return false; }

    renderer_ = createRenderer();
    if (!renderer_->init(window_, logicalW_, logicalH_)) {
        std::fprintf(stderr, "[app] renderer init failed (%s)\n", graphicsBackendName());
        return false;
    }
    std::printf("[app] Otacon engine | window=%s | graphics=%s | scalar=%s\n",
                windowBackendName(), graphicsBackendName(), kScalarName);

    audio_ = createAudio();                       // CoreAudio on macOS, silent stub else

    GameContext ctx;
    ctx.renderer = renderer_; ctx.window = window_; ctx.audio = audio_; ctx.debug = &debug_;
    ctx.logicalW = logicalW_; ctx.logicalH = logicalH_; ctx.assetDir = assetDir;
    game_->init(ctx);
    running_ = true;
    return true;
}

/*
======================
App::handleGlobalInput

The keys the engine owns, handled before the game sees the frame. Every one
is a debug or presentation control, so no game can shadow them and no game
has to implement them.
======================
*/
void App::handleGlobalInput(const InputFrame& in) {
    if (in.isPressed(Action::Quit))            window_->requestClose();
    if (in.isPressed(Action::ToggleUi))        debug_.toggleUi();
    if (in.isPressed(Action::CyclePreset))     debug_.cyclePreset();
    if (in.isPressed(Action::TogglePause))     debug_.togglePause();
    if (in.isPressed(Action::Step))            debug_.requestStep();
    if (in.isPressed(Action::ToggleColliders)) debug_.toggle(DebugView::Colliders);
    if (in.isPressed(Action::ToggleParallax))  debug_.toggle(DebugView::ParallaxBands);
    if (in.isPressed(Action::ToggleWireframe)) debug_.toggle(DebugView::Wireframe);
    if (in.isPressed(Action::ToggleGrid))      debug_.toggle(DebugView::Grid);
    if (in.isPressed(Action::ToggleTimestep))  time_.togglePolicy();
    if (in.isPressed(Action::TogglePerf))      debug_.toggle(DebugView::Perf);
    if (in.isPressed(Action::TimeFaster))      time_.nudgeTimeScale(+1);
    if (in.isPressed(Action::TimeSlower))      time_.nudgeTimeScale(-1);
    if (in.isPressed(Action::Screenshot))      wantShot_ = true;
}

/*
==================
App::drawPerf

The perf overlay: a frame-time graph with the 60 and 30 fps budgets marked,
the renderer's draw statistics, and the memory manager's live totals.
==================
*/
void App::drawPerf() {
    const float x = 3, y = 22, w = 116, h = 28;
    renderer_->fillRect(x - 1, y - 1, w + 2, h + 2, Color{0, 0, 0, 0.55f});
    // 60fps and 30fps reference lines (16.7ms / 33.3ms over a 50ms scale).
    const float scaleMs = 50.f;
    auto yFor = [&](float ms) { return y + h - (ms / scaleMs) * h; };
    renderer_->drawLine(x, yFor(16.7f), x + w, yFor(16.7f), Color{0.3f, 0.8f, 0.3f, 0.5f}, 1);
    renderer_->drawLine(x, yFor(33.3f), x + w, yFor(33.3f), Color{0.8f, 0.7f, 0.2f, 0.5f}, 1);
    const float* hist = time_.frameHistory();
    int n = time_.frameHistoryCount(), head = time_.frameHistoryHead();
    for (int i = 1; i < n; ++i) {
        float a = hist[(head + i - 1) % n] * 1000.f, b = hist[(head + i) % n] * 1000.f;
        float xa = x + (float(i - 1) / n) * w, xb = x + (float(i) / n) * w;
        renderer_->drawLine(xa, std::max(y, yFor(a)), xb, std::max(y, yFor(b)), Color{0.4f, 0.9f, 1.f, 0.9f}, 1);
    }
    mem::Stats ms = mem::stats();
    char buf[160];
    std::snprintf(buf, sizeof buf, "FPS %d  %.1fX  DRAWS %d  VERTS %zu",
                  int(time_.fps() + 0.5), time_.timeScale(), renderer_->drawCalls(), renderer_->vertexCount());
    renderer_->drawText(buf, x, y + h + 2, 1.f, Color{0.7f, 0.95f, 1.f, 1});
    std::snprintf(buf, sizeof buf, "MEM %zuKB / %zu BLK / PEAK %zuKB",
                  ms.liveBytes / 1024, ms.liveBlocks, ms.peakBytes / 1024);
    renderer_->drawText(buf, x, y + h + 9, 1.f, Color{0.7f, 0.95f, 1.f, 1});
}

/*
==================
App::drawHud

The state line - backend, scalar type, frame rate, timestep policy, view
preset - plus whatever the game wants to say about itself.
==================
*/
void App::drawHud() {
    if (!debug_.enabled(DebugView::Hud)) return;
    char line[256];
    std::snprintf(line, sizeof line, "OTACON  %s  SCALAR-%s  FPS-%d  STEP-%s  VIEW-%s",
                  graphicsBackendName(), kScalarName, int(time_.fps() + 0.5),
                  time_.policyName(), debug_.presetName());
    renderer_->drawText(line, 3, 3, 1.f, Color{1, 1, 1, 0.9f});
    if (game_->statusLine()[0])
        renderer_->drawText(game_->statusLine(), 3, 11, 1.f, Color{1, 1, 0.6f, 0.9f});
    if (debug_.paused())
        renderer_->drawText("PAUSED  O-STEP", 3, float(logicalH_) - 8, 1.f, Color{1, 0.5f, 0.5f, 1});
}

/*
==================
App::setCapture

Render a fixed number of deterministic frames, save a PNG and quit. The dev
UI is hidden so the capture is the scene alone, which is what makes the
cross-backend comparison meaningful.
==================
*/
void App::setCapture(const char* path, int frames) {
    capturePath_ = path;
    captureFrames_ = frames > 0 ? frames : 60;
    debug_.setUiHidden(true);   // clean, deterministic frame (no HUD/legend/overlays)
}

/*
==================
App::setRecord

Save every frame to a numbered PNG, for assembling a video of a run.
==================
*/
void App::setRecord(const char* dir, int frames) {
    recordDir_ = dir;
    recordFrames_ = frames > 0 ? frames : 120;
    debug_.setUiHidden(true);   // clean frames for the GIF
}

/*
=============================================================================

                                  MAIN LOOP

=============================================================================
*/

/*
==================
App::run

The main loop. Poll, let the engine take its keys, let the game take the rest,
step the simulation however many times the clock says, then render.

While recording, the loop ignores a window-close and steps exactly one fixed
frame per capture: the frame count is the authoritative stop, and a recording
that drifted with real time would not be reproducible.
==================
*/
void App::run() {
    // While recording, ignore a spurious window-close (an offscreen window can be
    // closed by the OS mid-run); the frame count is the authoritative stop.
    while (running_ && (recordDir_ != nullptr || !window_->shouldClose())) {
        ++frameNo_;
        int steps = time_.beginFrame();
        window_->pollEvents();
        const InputFrame& in = window_->input();
        handleGlobalInput(in);
        game_->handleInput(in);

        Real dt = R(float(time_.stepDelta()));
        if (recordDir_) {
            game_->update(R(1.f / 60.f));   // exactly one fixed step per recorded frame (smooth, deterministic)
        } else if (debug_.paused()) {
            if (debug_.consumeStep()) game_->update(dt);
        } else {
            for (int i = 0; i < steps; ++i) game_->update(dt);
        }

        // Decide on a screenshot for THIS frame *before* rendering, so the
        // backend can capture at a valid point (endFrame). `path` must outlive
        // endFrame, so keep it in this scope.
        char path[128];
        bool recordNow  = recordDir_ != nullptr;
        bool captureNow = !recordNow && capturePath_ && frameNo_ >= captureFrames_;
        if (recordNow) {
            std::snprintf(path, sizeof path, "%s/%04d.png", recordDir_, frameNo_);
            renderer_->captureNextFrame(path);
        } else if (captureNow) {
            renderer_->captureNextFrame(capturePath_);
        } else if (wantShot_) {
            std::snprintf(path, sizeof path, "screenshot_%03d.png", shotCount_++);
            renderer_->captureNextFrame(path);
        }
        wantShot_ = false;

        renderer_->setWireframe(debug_.enabled(DebugView::Wireframe));
        renderer_->beginFrame(game_->clearColor());
        game_->render(*renderer_, debug_);
        drawHud();
        if (debug_.enabled(DebugView::Perf)) drawPerf();
        renderer_->endFrame();                    // screenshot is written here, if requested
        if (captureNow) running_ = false;         // capture done -> quit
        if (recordNow && frameNo_ >= recordFrames_) running_ = false;   // recording done -> quit
        window_->swapBuffers();
    }
}

/*
==================
App::shutdown

Tear down in the reverse of init. The game frees its GPU resources first,
while the renderer it allocated them from is still alive, and the memory
report comes last so it can see everything that was released.
==================
*/
void App::shutdown() {
    if (game_) game_->shutdown();          // free game GPU resources first
    if (audio_)    { delete audio_; audio_ = nullptr; }   // stops the audio thread
    if (renderer_) { renderer_->shutdown(); delete renderer_; renderer_ = nullptr; }
    if (window_)   { delete window_; window_ = nullptr; }
    mem::report("shutdown");
}

} // namespace otacon
