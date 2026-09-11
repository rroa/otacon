// SpriteAnimation.cpp — sample 02: frames, timing and UV windows.
//
// Animation is two ideas that people usually meet tangled together: *which*
// frame to show (a clock problem) and *where that frame lives in the texture*
// (a UV problem). This sample puts both on screen at once — the strip is drawn
// with the live frame boxed, so you can watch the UV window slide while the
// timer advances — and lets you drive the clock by hand to prove they are
// separate.
#include "Sample.hpp"
#include "common/Art.hpp"
#include "render/IRenderer.hpp"
#include "platform/Window.hpp"
#include "core/debug/Debug.hpp"
#include <cstdio>

namespace samples {
namespace {

using otacon::Color;

// A frame-based clip: a list of frame indices plus how long each is held.
struct Clip {
    const char* name;
    const int*  frames;
    int         count;
    float       fps;
    bool        loop;
};

const int kRunFrames[]  = {0, 1, 2, 3};
const int kIdleFrames[] = {4};
const int kJumpFrames[] = {5};
const int kPingPong[]   = {0, 1, 2, 3, 2, 1};

const Clip kClips[] = {
    {"run",       kRunFrames,  4, 12.f, true},
    {"run slow",  kRunFrames,  4,  5.f, true},
    {"ping-pong", kPingPong,   6, 12.f, true},
    {"idle",      kIdleFrames, 1,  1.f, true},
    {"jump",      kJumpFrames, 1,  1.f, true},
};
constexpr int kClipCount = int(sizeof kClips / sizeof kClips[0]);

class SpriteAnimation final : public Sample {
public:
    void init(SampleContext& ctx) override {
        r_ = ctx.renderer;
        W_ = float(ctx.logicalW); H_ = float(ctx.logicalH);
        int fw = 0, fh = 0, fc = 0;
        otacon::Image sheet = art::heroSheet(fw, fh, fc);
        frameW_ = fw; frameH_ = fh; frameCount_ = fc;
        tex_ = r_->createTexture(sheet);
    }
    void shutdown() override { if (tex_ && r_) { r_->destroyTexture(tex_); tex_ = 0; } }

    void enter() override { clip_ = 0; cursor_ = 0; timer_ = 0; manual_ = false; }

    void handleInput(const otacon::InputFrame& in) override {
        if (in.selectSlot >= 1 && in.selectSlot <= kClipCount) clip_ = in.selectSlot - 1;
        if (in.isPressed(otacon::Action::Aux6)) manual_ = !manual_;         // E
        if (manual_ && in.isPressed(otacon::Action::Jump)) step(1);         // hand-crank
        if (!manual_ && in.isPressed(otacon::Action::Jump)) paused_ = !paused_;
    }

    void update(otacon::Real dt) override {
        if (manual_ || paused_) return;
        const Clip& c = kClips[clip_];
        timer_ += otacon::toFloat(dt);
        const float hold = 1.f / (c.fps > 0 ? c.fps : 1.f);
        while (timer_ >= hold) { timer_ -= hold; step(1); }
    }

    void render(otacon::IRenderer& r, const otacon::DebugRuntime&) override {
        const Clip& c = kClips[clip_];
        const int frame = c.frames[cursor_ % c.count];
        const float top = layout::kTop + 10;

        // --- the live sprite, big -------------------------------------------
        const float scale = 9.f;
        const float sw = frameW_ * scale, sh = frameH_ * scale;
        const float sx = W_ * 0.30f - sw * 0.5f, sy = top + 40;
        // A ground line so the run cycle has something to push against.
        r.drawLine(sx - 40, sy + sh, sx + sw + 40, sy + sh, Color{0.3f, 0.36f, 0.46f, 1.f}, 1.f);
        drawFrame(r, frame, sx, sy, sw, sh, Color{1, 1, 1, 1});

        // --- the sheet, with the live frame boxed ---------------------------
        const float stripScale = 4.f;
        const float stripW = frameW_ * frameCount_ * stripScale, stripH = frameH_ * stripScale;
        const float stx = W_ * 0.58f, sty = top + 56;
        r.drawText("THE SHEET  (one texture, six frames)", stx, sty - 22, 1.f, Color{0.55f, 0.80f, 1.f, 1.f});
        r.fillRect(stx - 2, sty - 2, stripW + 4, stripH + 4, Color{0.08f, 0.09f, 0.13f, 1.f});
        r.drawImage(tex_, stx, sty, stripW, stripH);
        for (int i = 0; i <= frameCount_; ++i) {
            const float gx = stx + i * frameW_ * stripScale;
            r.drawLine(gx, sty, gx, sty + stripH, Color{1, 1, 1, 0.16f}, 1.f);
        }
        const float boxX = stx + frame * frameW_ * stripScale;
        r.drawRectOutline(boxX, sty, frameW_ * stripScale, stripH, Color{1.f, 0.85f, 0.3f, 1.f}, 2.f);

        // The UV window for the live frame — the actual numbers being sampled.
        char uv[128];
        std::snprintf(uv, sizeof uv, "frame %d -> u0 %.3f  u1 %.3f", frame,
                      float(frame) / frameCount_, float(frame + 1) / frameCount_);
        r.drawText(uv, stx, sty + stripH + 8, 1.f, Color{1.f, 0.85f, 0.3f, 1.f});

        // --- the clip's frame list, with the cursor -------------------------
        float ly = sty + stripH + 26;
        r.drawText("CLIP", stx, ly, 1.f, Color{0.55f, 0.80f, 1.f, 1.f}); ly += 10;
        for (int i = 0; i < c.count; ++i) {
            const bool on = (i == cursor_ % c.count);
            char cell[8];
            std::snprintf(cell, sizeof cell, "%d", c.frames[i]);
            const float cx = stx + i * 14.f;
            if (on) r.fillRect(cx - 3, ly - 2, 12, 10, Color{0.20f, 0.42f, 0.62f, 1.f});
            r.drawText(cell, cx, ly, 1.f, on ? Color{1, 1, 1, 1} : Color{0.5f, 0.56f, 0.68f, 1.f});
        }

        // --- the clock ------------------------------------------------------
        const float barX = W_ * 0.30f - 90, barY = sy + sh + 26, barW = 180;
        r.drawText("HOLD PER FRAME", barX, barY - 12, 1.f, Color{0.55f, 0.80f, 1.f, 1.f});
        r.fillRect(barX, barY, barW, 6, Color{0.12f, 0.14f, 0.20f, 1.f});
        const float hold = 1.f / (c.fps > 0 ? c.fps : 1.f);
        const float fill = manual_ ? 0.f : (hold > 0 ? timer_ / hold : 0.f);
        r.fillRect(barX, barY, barW * (fill < 0 ? 0 : (fill > 1 ? 1 : fill)), 6,
                   Color{0.35f, 0.85f, 0.55f, 1.f});
        char clock[96];
        std::snprintf(clock, sizeof clock, manual_ ? "manual: SPACE advances one frame"
                                                   : "%.0f fps  =  %.0f ms per frame", c.fps, hold * 1000.f);
        r.drawText(clock, barX, barY + 11, 1.f, Color{0.55f, 0.62f, 0.75f, 1.f});

        // --- clip menu ------------------------------------------------------
        float my = top;
        r.drawText("1-5 CLIP", 8, my, 1.f, Color{0.55f, 0.80f, 1.f, 1.f}); my += 10;
        for (int i = 0; i < kClipCount; ++i) {
            char row[48];
            std::snprintf(row, sizeof row, "%d %s", i + 1, kClips[i].name);
            r.drawText(row, 10, my, 1.f, i == clip_ ? Color{1, 1, 1, 1} : Color{0.45f, 0.51f, 0.62f, 1.f});
            my += 9;
        }
    }

    const char* status() const override {
        const Clip& c = kClips[clip_];
        std::snprintf(buf_, sizeof buf_, "%s  frame %d/%d  %s",
                      c.name, cursor_ % c.count + 1, c.count,
                      manual_ ? "MANUAL" : (paused_ ? "PAUSED" : "playing"));
        return buf_;
    }
    const char* keys() const override {
        return "1-5    pick a clip\nSPACE  pause (or advance, in manual)\nE      manual / automatic clock";
    }

private:
    void step(int n) { cursor_ = (cursor_ + n) % kClips[clip_].count; }
    void drawFrame(otacon::IRenderer& r, int f, float x, float y, float w, float h, Color tint) const {
        const float u0 = float(f) / frameCount_, u1 = float(f + 1) / frameCount_;
        r.drawImage(tex_, x, y, w, h, u0, 0.f, u1, 1.f, tint);
    }

    otacon::IRenderer* r_ = nullptr;
    otacon::TextureHandle tex_ = 0;
    float W_ = 640, H_ = 400, timer_ = 0;
    int frameW_ = 12, frameH_ = 16, frameCount_ = 6;
    int clip_ = 0, cursor_ = 0;
    bool manual_ = false, paused_ = false;
    mutable char buf_[96]{};
};

} // namespace

Sample* makeSpriteAnimation() { return new SpriteAnimation(); }

} // namespace samples
