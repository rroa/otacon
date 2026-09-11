/*
===========================================================================

OTACON ENGINE
core/debug/Debug.hpp - runtime debug views

Holds a set of independently toggleable "views" (overlays/inspectors) that
can be flipped on and off while the game is running. The Game queries these
flags each frame to decide what extra geometry/text to draw. Keeping this as
plain state (not baked into the renderer) means every graphics backend gets
the same debug visualization for free.

===========================================================================
*/
#pragma once
#include <cstdint>

namespace otacon {

enum class DebugView : std::uint32_t {
    Colliders     = 1u << 0,   // draw collision AABBs
    CameraFocus   = 1u << 1,   // draw the camera follow target + lead
    ParallaxBands = 1u << 2,   // tint/outline each parallax layer
    Grid          = 1u << 3,   // world grid in screen space
    Wireframe     = 1u << 4,   // backend wireframe mode
    Hud           = 1u << 5,   // perf + state HUD text
    Velocities    = 1u << 6,   // draw velocity vectors
    Perf          = 1u << 7,   // perf/memory overlay (frame graph + counters)
};

class DebugRuntime {
public:
    // Master switch: when hidden, ALL dev/debug UI is suppressed (clean play).
    bool uiHidden() const { return uiHidden_; }
    void toggleUi()       { uiHidden_ = !uiHidden_; }
    void setUiHidden(bool h) { uiHidden_ = h; }

    // A view counts as enabled only if it's set AND the UI isn't hidden.
    bool enabled(DebugView v) const { return !uiHidden_ && (flags_ & std::uint32_t(v)) != 0; }
    void toggle(DebugView v)        { flags_ ^= std::uint32_t(v); }
    void set(DebugView v, bool on)  { on ? (flags_ |= std::uint32_t(v)) : (flags_ &= ~std::uint32_t(v)); }
    std::uint32_t raw() const       { return flags_; }

    // Single-stepping: when paused, requestStep() lets exactly one sim step run.
    bool paused() const   { return paused_; }
    void togglePause()    { paused_ = !paused_; }
    void requestStep()    { stepRequested_ = true; }
    bool consumeStep()    { bool s = stepRequested_; stepRequested_ = false; return s; }

    // Cycle through curated overlay presets (the "switch views" hotkey).
    void cyclePreset();
    int  preset() const { return preset_; }
    const char* presetName() const;

private:
    std::uint32_t flags_ = std::uint32_t(DebugView::Hud);
    bool paused_ = false;
    bool stepRequested_ = false;
    bool uiHidden_ = false;
    int  preset_ = 0;
};

} // namespace otacon
