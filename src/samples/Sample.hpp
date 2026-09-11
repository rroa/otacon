// Sample.hpp — the contract every engine sample implements.
//
// The samples are the engine's executable documentation: fifteen small,
// self-contained programs that each demonstrate one thing Otacon can do, all
// hosted by a single `samples` executable you page through at runtime (the same
// shape as Canabalt's modes and Flappy's builds).
//
// A sample is deliberately NOT an IGame. The gallery is the IGame; a sample is
// one screen inside it, so adding a sample means writing one class and adding
// one line to the table in Registry.cpp — no CMake target, no main(), no window.
//
// Samples own no assets on disk: every texture they use is generated in code
// (common/Art.hpp). That keeps the set runnable from a fresh checkout and means
// a sample never depends on a *game's* art, which would break the engine/game
// boundary the whole project is built around.
#pragma once
#include "core/math/Scalar.hpp"
#include "render/RenderTypes.hpp"

namespace otacon {
class IRenderer;
class IAudio;
class DebugRuntime;
struct InputFrame;
}

namespace samples {

// The top of the screen is a shared header band: the engine's App draws its two
// HUD lines into it (y=3 and y=11) and the gallery adds a third (y=19), so a
// sample's content must start below all of them. One constant rather than
// fifteen guesses -- and the reason it is 34 and not 26 is that the gallery used
// to draw its own title over the engine's first line.
namespace layout { inline constexpr float kTop = 34.f; }

struct SampleContext {
    otacon::IRenderer* renderer = nullptr;
    otacon::IAudio*    audio    = nullptr;
    int logicalW = 640, logicalH = 400;
};

class Sample {
public:
    virtual ~Sample() = default;

    // One-time setup: build textures, compile effects. Called once, the first
    // time the sample is opened (samples are constructed lazily, then kept).
    virtual void init(SampleContext& ctx) { (void)ctx; }
    // (Re)start: called on every switch-in and on R. Put resettable state here,
    // not in init(), so R is always a clean restart.
    virtual void enter() {}
    virtual void handleInput(const otacon::InputFrame& in) { (void)in; }
    virtual void update(otacon::Real dt) { (void)dt; }
    virtual void render(otacon::IRenderer& r, const otacon::DebugRuntime& dbg) = 0;
    // Free GPU resources. Called before the renderer is torn down.
    virtual void shutdown() {}

    virtual otacon::Color background() const { return otacon::Color::rgb(0x14141c); }
    // Live state for the status bar (frame counts, tunables, mode).
    virtual const char* status() const { return ""; }
    // Sample-specific keys, shown in the on-screen help ('?').
    virtual const char* keys() const { return ""; }
};

// ---------------------------------------------------------------------------
// The registry. One row per sample; the gallery reads names and blurbs from
// here without constructing anything, so the picker can list all fifteen while
// only the open sample actually exists.
struct SampleInfo {
    const char* name;      // picker label
    const char* blurb;     // one line: what this sample is actually showing
    Sample* (*make)();     // factory
};

int               sampleCount();
const SampleInfo& sampleInfo(int index);

} // namespace samples
