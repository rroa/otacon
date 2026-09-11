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

void DebugRuntime::cyclePreset() {
    preset_ = (preset_ + 1) % kPresetCount;
    flags_ = kPresets[preset_].flags;
}
const char* DebugRuntime::presetName() const { return kPresets[preset_].name; }

} // namespace otacon
