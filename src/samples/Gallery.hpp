// Gallery.hpp — the samples browser, as an Otacon IGame.
//
// One IGame hosts all fifteen samples. It owns the picker overlay, the paging
// keys, the per-sample help, and lazy construction; a Sample itself only has to
// draw its own screen. That is the same shape as Canabalt's Mode list and
// Flappy's Scene list, and it means the whole set costs one window, one
// renderer and one CMake target.
#pragma once
#include "IGame.hpp"
#include "Sample.hpp"
#include <memory>
#include <vector>

namespace samples {

class Gallery final : public otacon::IGame {
public:
    void init(otacon::GameContext& ctx) override;
    void handleInput(const otacon::InputFrame& in) override;
    void update(otacon::Real dt) override;
    void render(otacon::IRenderer& r, const otacon::DebugRuntime& dbg) override;
    void shutdown() override;

    otacon::Color clearColor() const override;
    const char*   title() const override { return "Otacon Engine Samples"; }
    const char*   statusLine() const override;

    // Open a sample by index at startup (--sample N), for screenshots and the
    // cross-backend pixel diff.
    void selectAtStartup(int index) { startIndex_ = index; }

private:
    Sample* active();                    // constructs on first use
    void    switchTo(int index);
    void    drawChrome(otacon::IRenderer& r) const;   // title bar + hints
    void    drawPicker(otacon::IRenderer& r) const;
    void    drawHelp(otacon::IRenderer& r) const;

    SampleContext ctx_{};
    // Parallel to the registry: a slot stays null until its sample is opened.
    std::vector<std::unique_ptr<Sample>> made_;
    int  index_ = 0;
    int  startIndex_ = 0;
    bool pickerOpen_ = false;
    int  pickerCursor_ = 0;
    bool helpOpen_ = false;
    mutable char status_[256]{};
};

} // namespace samples
