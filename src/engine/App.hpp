// App.hpp — the Otacon engine runtime. Owns the platform window, renderer,
// time manager and debug runtime; runs the main loop and drives an IGame.
#pragma once
#include "platform/Window.hpp"
#include "core/time/Time.hpp"
#include "core/debug/Debug.hpp"

namespace otacon {

class IRenderer;
class IAudio;
class IGame;

class App {
public:
    bool init(const WindowConfig& cfg, IGame* game, const char* assetDir);
    void run();
    void shutdown();

    // Headless-ish capture: run `frames` frames (no HUD, for determinism) then
    // save a screenshot and quit. Used by tools/backend-diff.sh.
    void setCapture(const char* path, int frames);

    // Record EVERY frame to `dir`/NNNN.png for `frames` frames, then quit (no
    // HUD). For assembling a GIF/video of a run from the resulting PNG sequence.
    void setRecord(const char* dir, int frames);

    DebugRuntime& debug() { return debug_; }

private:
    void handleGlobalInput(const InputFrame& in);
    void drawHud();
    void drawPerf();

    IWindow*     window_   = nullptr;
    IRenderer*   renderer_ = nullptr;
    IAudio*      audio_    = nullptr;
    IGame*       game_     = nullptr;
    TimeManager  time_{TimeManager::Policy::Variable};
    DebugRuntime debug_;
    int          logicalW_ = 480, logicalH_ = 320;
    bool         running_  = false;
    bool         wantShot_ = false;
    int          shotCount_ = 0;
    const char*  capturePath_ = nullptr;
    int          captureFrames_ = 0, frameNo_ = 0;
    const char*  recordDir_ = nullptr;
    int          recordFrames_ = 0;
};

} // namespace otacon
