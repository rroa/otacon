#include "Gallery.hpp"
#include "platform/Window.hpp"
#include "render/IRenderer.hpp"
#include "core/debug/Debug.hpp"
#include <cstdio>
#include <cstring>

using otacon::Action;
using otacon::Color;

namespace samples {

namespace {
constexpr float kBarH = 13.f;                       // title bar height
constexpr Color kInk  {0.92f, 0.94f, 1.00f, 1.f};
constexpr Color kDim  {0.58f, 0.62f, 0.74f, 1.f};
constexpr Color kAccent{0.45f, 0.82f, 1.00f, 1.f};
constexpr Color kPanel{0.05f, 0.06f, 0.09f, 0.94f};
}

void Gallery::init(otacon::GameContext& ctx) {
    ctx_.renderer = ctx.renderer;
    ctx_.audio    = ctx.audio;
    ctx_.logicalW = ctx.logicalW;
    ctx_.logicalH = ctx.logicalH;
    made_.resize(std::size_t(sampleCount()));
    int n = sampleCount();
    switchTo(startIndex_ < 0 ? 0 : (startIndex_ >= n ? n - 1 : startIndex_));
}

// Samples are built the first time they are opened, then kept. Fifteen samples
// each allocating textures up front would be wasteful when you only ever look
// at one at a time — and the stress test in particular wants its memory back.
Sample* Gallery::active() {
    auto& slot = made_[std::size_t(index_)];
    if (!slot) {
        slot.reset(sampleInfo(index_).make());
        slot->init(ctx_);
        slot->enter();
    }
    return slot.get();
}

void Gallery::switchTo(int index) {
    const int n = sampleCount();
    index_ = ((index % n) + n) % n;
    pickerCursor_ = index_;
    active()->enter();
}

void Gallery::handleInput(const otacon::InputFrame& in) {
    // The picker swallows navigation while it is open.
    if (pickerOpen_) {
        if (in.isPressed(Action::NextMode)) pickerCursor_ = (pickerCursor_ + 1) % sampleCount();
        if (in.isPressed(Action::PrevMode)) pickerCursor_ = (pickerCursor_ + sampleCount() - 1) % sampleCount();
        if (in.selectSlot >= 0 && in.selectSlot < sampleCount()) pickerCursor_ = in.selectSlot;
        if (in.isPressed(Action::Jump)) { switchTo(pickerCursor_); pickerOpen_ = false; }
        if (in.isPressed(Action::Aux1)) pickerOpen_ = false;
        return;
    }

    if (in.isPressed(Action::Aux1)) { pickerOpen_ = true; pickerCursor_ = index_; return; }
    if (in.isPressed(Action::Aux2)) { helpOpen_ = !helpOpen_; return; }
    if (in.isPressed(Action::NextMode)) { switchTo(index_ + 1); return; }
    if (in.isPressed(Action::PrevMode)) { switchTo(index_ - 1); return; }
    if (in.isPressed(Action::Reset)) { active()->enter(); return; }
    // Everything else — including the number keys — belongs to the sample. The
    // picker (F1) is the way to jump around, which keeps 1-0 free for the
    // samples that need a row of presets.

    active()->handleInput(in);
}

void Gallery::update(otacon::Real dt) {
    if (pickerOpen_) return;              // the picker pauses the sample behind it
    active()->update(dt);
}

void Gallery::render(otacon::IRenderer& r, const otacon::DebugRuntime& dbg) {
    active()->render(r, dbg);
    if (!dbg.uiHidden()) {
        drawChrome(r);
        if (helpOpen_)   drawHelp(r);
        if (pickerOpen_) drawPicker(r);
    }
}

void Gallery::shutdown() {
    for (auto& s : made_) if (s) s->shutdown();
    made_.clear();
}

otacon::Color Gallery::clearColor() const {
    const auto& slot = made_[std::size_t(index_)];
    return slot ? slot->background() : Color::rgb(0x14141c);
}

const char* Gallery::statusLine() const {
    const auto& slot = made_[std::size_t(index_)];
    const char* extra = slot ? slot->status() : "";
    if (extra[0])
        std::snprintf(status_, sizeof status_, "%02d/%02d %s | %s",
                      index_ + 1, sampleCount(), sampleInfo(index_).name, extra);
    else
        std::snprintf(status_, sizeof status_, "%02d/%02d %s",
                      index_ + 1, sampleCount(), sampleInfo(index_).name);
    return status_;
}

// ---------------------------------------------------------------------------
// A permanent, minimal title bar: which sample you are on and what it shows.
// Drawn by the gallery rather than each sample, so all fifteen look like one set.
void Gallery::drawChrome(otacon::IRenderer& r) const {
    const float W = float(ctx_.logicalW);
    r.fillRect(0, 0, W, kBarH, Color{0.03f, 0.04f, 0.06f, 0.82f});
    r.drawLine(0, kBarH, W, kBarH, Color{0.20f, 0.30f, 0.42f, 0.9f}, 1.f);

    char left[128];
    std::snprintf(left, sizeof left, "%02d/%02d  %s", index_ + 1, sampleCount(), sampleInfo(index_).name);
    r.drawText(left, 4, 4, 1.f, kInk);

    const char* hint = "F1 list  F2 help  [ ]";
    r.drawText(hint, W - r.textWidth(hint, 1.f) - 4, 4, 1.f, kDim);

    // The blurb sits just under the bar: one line on what the sample teaches.
    r.drawText(sampleInfo(index_).blurb, 4, kBarH + 4, 1.f, Color{0.62f, 0.70f, 0.85f, 0.85f});
}

void Gallery::drawPicker(otacon::IRenderer& r) const {
    const float W = float(ctx_.logicalW), H = float(ctx_.logicalH);
    r.fillRect(0, 0, W, H, Color{0, 0, 0, 0.72f});

    const float panelW = 300, rowH = 11;
    const float panelH = sampleCount() * rowH + 30;
    const float px = (W - panelW) * 0.5f, py = (H - panelH) * 0.5f;
    r.fillRect(px, py, panelW, panelH, kPanel);
    r.drawRectOutline(px, py, panelW, panelH, Color{0.30f, 0.45f, 0.62f, 1.f}, 1.f);
    r.drawText("ENGINE SAMPLES", px + 8, py + 7, 1.f, kAccent);
    r.drawText("[ ] move   SPACE open   F1 close", px + 8, py + 16, 1.f, kDim);

    for (int i = 0; i < sampleCount(); ++i) {
        const float ry = py + 28 + i * rowH;
        const bool sel = (i == pickerCursor_);
        if (sel) r.fillRect(px + 4, ry - 2, panelW - 8, rowH, Color{0.16f, 0.30f, 0.45f, 1.f});
        char row[128];
        std::snprintf(row, sizeof row, "%2d  %s", i + 1, sampleInfo(i).name);
        r.drawText(row, px + 8, ry, 1.f, sel ? kInk : (i == index_ ? kAccent : kDim));
    }
}

void Gallery::drawHelp(otacon::IRenderer& r) const {
    const float W = float(ctx_.logicalW), H = float(ctx_.logicalH);
    const float panelW = 320, panelH = 118;
    const float px = (W - panelW) * 0.5f, py = (H - panelH) * 0.5f;
    r.fillRect(px, py, panelW, panelH, kPanel);
    r.drawRectOutline(px, py, panelW, panelH, Color{0.30f, 0.45f, 0.62f, 1.f}, 1.f);

    float y = py + 7;
    r.drawText(sampleInfo(index_).name, px + 8, y, 1.f, kAccent); y += 11;
    r.drawText(sampleInfo(index_).blurb, px + 8, y, 1.f, kDim); y += 13;

    const auto& slot = made_[std::size_t(index_)];
    const char* sampleKeys = slot ? slot->keys() : "";
    if (sampleKeys[0]) {
        r.drawText("THIS SAMPLE", px + 8, y, 1.f, kInk); y += 9;
        // The sample's key list is one string with '\n' between lines.
        char buf[512];
        std::snprintf(buf, sizeof buf, "%s", sampleKeys);
        char* line = buf;
        while (line && *line) {
            char* nl = std::strchr(line, '\n');
            if (nl) *nl = '\0';
            r.drawText(line, px + 12, y, 1.f, kDim); y += 8;
            line = nl ? nl + 1 : nullptr;
        }
        y += 4;
    }
    r.drawText("GALLERY", px + 8, y, 1.f, kInk); y += 9;
    r.drawText("F1 list   F2 help   [ ] prev/next sample", px + 12, y, 1.f, kDim); y += 8;
    r.drawText("R reset   ` clean view   H perf   F12 shot", px + 12, y, 1.f, kDim);
}

} // namespace samples
