// HelloSprite.cpp — sample 01: the smallest complete thing you can draw.
//
// Four ways to put something on screen, side by side, all going through the
// same seam: fillRect, drawRectOutline, drawImage and drawImageRotated. Every
// one of them is tessellated into triangles by IRenderer before a backend ever
// sees it, which is why the four panels look the same under GL modern, GL
// legacy and Vulkan.
#include "Sample.hpp"
#include "common/Art.hpp"
#include "render/IRenderer.hpp"
#include "platform/Window.hpp"
#include "core/debug/Debug.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace samples {
namespace {

using otacon::Color;

class HelloSprite final : public Sample {
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

    void enter() override { t_ = 0; zoom_ = 6; tint_ = 0; spin_ = false; }

    void handleInput(const otacon::InputFrame& in) override {
        if (in.isPressed(otacon::Action::Jump)) tint_ = (tint_ + 1) % kTints;
        if (in.isPressed(otacon::Action::Aux6)) spin_ = !spin_;          // E
        if (in.selectSlot >= 1 && in.selectSlot <= 9) zoom_ = in.selectSlot;
        mouseX_ = in.mouseNx * W_; mouseY_ = in.mouseNy * H_;
    }

    void update(otacon::Real dt) override { t_ += otacon::toFloat(dt); }

    void render(otacon::IRenderer& r, const otacon::DebugRuntime&) override {
        const float top = layout::kTop + 6;
        const float cellW = W_ / 4.f, cellH = H_ - top - 34;

        struct Panel { const char* title; const char* call; };
        const Panel panels[4] = {
            {"SOLID RECT",  "fillRect(x,y,w,h,c)"},
            {"OUTLINE",     "drawRectOutline(...)"},
            {"SPRITE",      "drawImage(tex,...)"},
            {"ROTATED",     "drawImageRotated(...)"},
        };
        for (int i = 0; i < 4; ++i) {
            const float cx = cellW * i + cellW * 0.5f;
            const float cy = top + cellH * 0.5f;
            if (i > 0) r.drawLine(cellW * i, top, cellW * i, top + cellH, Color{1, 1, 1, 0.10f}, 1.f);
            r.drawText(panels[i].title, cellW * i + 6, top + 2, 1.f, Color{0.55f, 0.80f, 1.f, 1.f});

            const float w = frameW_ * float(zoom_), h = frameH_ * float(zoom_);
            const Color tint = kTintTable[tint_];
            switch (i) {
                case 0:
                    r.fillRect(cx - w * 0.5f, cy - h * 0.5f, w, h, tint);
                    break;
                case 1:
                    r.drawRectOutline(cx - w * 0.5f, cy - h * 0.5f, w, h, tint, 2.f);
                    break;
                case 2:
                    drawFrame(r, 4, cx - w * 0.5f, cy - h * 0.5f, w, h, tint);
                    break;
                default: {
                    const float angle = spin_ ? t_ * 90.f : 0.f;
                    drawFrameRotated(r, 4, cx, cy, w, h, angle, tint);
                    break;
                }
            }
            r.drawText(panels[i].call, cellW * i + 6, top + cellH - 8, 1.f, Color{0.5f, 0.55f, 0.66f, 1.f});
        }

        // A sprite that follows the pointer, to make "logical coordinates"
        // concrete. Clamped out of the caption band so it never sits on the text.
        const float fw = frameW_ * 3.f, fh = frameH_ * 3.f;
        const float cy = std::min(mouseY_, top + cellH - 16.f);
        drawFrame(r, 4, mouseX_ - fw * 0.5f, cy - fh * 0.5f, fw, fh, Color{1, 1, 1, 0.85f});

        char info[128];
        std::snprintf(info, sizeof info, "logical %gx%g   cursor %.0f,%.0f   zoom x%d",
                      W_, H_, mouseX_, mouseY_, zoom_);
        r.drawText(info, 6, H_ - 20, 1.f, Color{0.55f, 0.62f, 0.75f, 1.f});
        r.drawText("every panel becomes triangles in IRenderer before any backend sees it",
                   6, H_ - 10, 1.f, Color{0.42f, 0.48f, 0.60f, 1.f});
    }

    const char* status() const override {
        std::snprintf(buf_, sizeof buf_, "zoom x%d  tint %s  spin %s",
                      zoom_, kTintName[tint_], spin_ ? "ON" : "off");
        return buf_;
    }
    const char* keys() const override {
        return "SPACE  cycle tint\n1-9    zoom\nE      spin the rotated panel\nmouse  moves the loose sprite";
    }

private:
    // One frame of the strip: the UV window is frame f of frameCount_ across.
    void drawFrame(otacon::IRenderer& r, int f, float x, float y, float w, float h, Color tint) const {
        const float u0 = float(f) / frameCount_, u1 = float(f + 1) / frameCount_;
        r.drawImage(tex_, x, y, w, h, u0, 0.f, u1, 1.f, tint);
    }
    void drawFrameRotated(otacon::IRenderer& r, int f, float cx, float cy, float w, float h,
                          float angle, Color tint) const {
        const float u0 = float(f) / frameCount_, u1 = float(f + 1) / frameCount_;
        r.drawImageRotated(tex_, cx, cy, w, h, angle, u0, 0.f, u1, 1.f, tint);
    }

    static constexpr int kTints = 4;
    static constexpr Color kTintTable[kTints] = {
        {1.00f, 1.00f, 1.00f, 1.f}, {1.00f, 0.55f, 0.45f, 1.f},
        {0.45f, 1.00f, 0.65f, 1.f}, {0.55f, 0.70f, 1.00f, 1.f},
    };
    static constexpr const char* kTintName[kTints] = {"white", "warm", "green", "cool"};

    otacon::IRenderer* r_ = nullptr;
    otacon::TextureHandle tex_ = 0;
    float W_ = 640, H_ = 400, t_ = 0;
    float mouseX_ = 320, mouseY_ = 200;
    int frameW_ = 12, frameH_ = 16, frameCount_ = 6;
    int zoom_ = 6, tint_ = 0;
    bool spin_ = false;
    mutable char buf_[96]{};
};

} // namespace

Sample* makeHelloSprite() { return new HelloSprite(); }

} // namespace samples
