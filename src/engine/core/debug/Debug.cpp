/*
===========================================================================

OTACON ENGINE
core/debug/Debug.cpp - debug view presets

The curated overlay combinations the V key cycles. Presets exist because the
individual toggles are a power set nobody wants to walk by hand - these are
the handful of combinations that are actually useful while playing.

===========================================================================
*/
#include "core/debug/Debug.hpp"

namespace otacon {

namespace {
struct Preset { const char* name; std::uint32_t flags; };
constexpr std::uint32_t HUD = std::uint32_t(DebugView::Hud);
const Preset kPresets[] = {
    { "Game",      HUD },
    { "Colliders", HUD | std::uint32_t(DebugView::Colliders) | std::uint32_t(DebugView::CameraFocus) },
    { "Parallax",  HUD | std::uint32_t(DebugView::ParallaxBands) },
    { "Physics",   HUD | std::uint32_t(DebugView::Colliders) | std::uint32_t(DebugView::Velocities) | std::uint32_t(DebugView::CameraFocus) },
    { "Wireframe", HUD | std::uint32_t(DebugView::Wireframe)  | std::uint32_t(DebugView::Grid) },
    { "Everything",HUD | std::uint32_t(DebugView::Colliders) | std::uint32_t(DebugView::CameraFocus) |
                   std::uint32_t(DebugView::ParallaxBands) | std::uint32_t(DebugView::Velocities) | std::uint32_t(DebugView::Grid) },
};
constexpr int kPresetCount = int(sizeof(kPresets) / sizeof(kPresets[0]));
}

/*
=========================
DebugRuntime::cyclePreset

Advance to the next preset and apply its flag set wholesale.
=========================
*/
void DebugRuntime::cyclePreset() {
    preset_ = (preset_ + 1) % kPresetCount;
    flags_ = kPresets[preset_].flags;
}

/*
========================
DebugRuntime::presetName

The current preset's name, for the HUD.
========================
*/
const char* DebugRuntime::presetName() const { return kPresets[preset_].name; }

} // namespace otacon
