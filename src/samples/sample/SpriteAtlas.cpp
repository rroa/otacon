// SpriteAtlas.cpp — sample 20: one texture, many sprites, one bind.
//
// The Stress Test sample measures the cost of a texture change: every bind
// forces a new draw call, so the "many textures" mode collapses at a fraction of
// the count the single-texture mode reaches. This is the other half of that
// finding -- what you do about it.
//
// render/Atlas.hpp is the answer, and it is barely an abstraction: named
// rectangles inside one sheet, converted to UVs on lookup. The value is entirely
// in what it makes possible, which is drawing a whole scene without ever
// changing the bound texture.
//
// Both sides of the screen draw the SAME sprites in the SAME arrangement. The
// left binds a separate texture per sprite; the right draws them all from one
// packed sheet. The draw-call counts underneath are the entire point, and they
// are read from the renderer rather than asserted.
#include "Sample.hpp"
#include "common/Art.hpp"
#include "core/math/Random.hpp"
#include "render/Atlas.hpp"
#include "render/IRenderer.hpp"
#include "platform/Window.hpp"
#include "core/debug/Debug.hpp"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

namespace samples {
namespace {

using otacon::Color;

constexpr int kFrames = 6;          // the hero strip's frame count
constexpr int kSprites = 96;        // drawn per side

class SpriteAtlas final : public Sample {
public:
    void init(SampleContext& ctx) override {
        r_ = ctx.renderer;
        W_ = float(ctx.logicalW); H_ = float(ctx.logicalH);

        int fw = 0, fh = 0, fc = 0;
        otacon::Image strip = art::heroSheet(fw, fh, fc);
        frameW_ = fw; frameH_ = fh;

        // --- the "many textures" side: one upload per frame -----------------
        // Cutting the strip into separate textures is the naive arrangement
        // this sample exists to compare against.
        for (int f = 0; f < kFrames && f < fc; ++f) {
            otacon::Image one;
            one.width = fw; one.height = fh;
            one.rgba.assign(std::size_t(fw) * fh * 4, 0);
            for (int y = 0; y < fh; ++y)
                std::memcpy(&one.rgba[std::size_t(y) * fw * 4],
                            &strip.rgba[(std::size_t(y) * strip.width + f * fw) * 4],
                            std::size_t(fw) * 4);
            loose_[f] = r_->createTexture(one);
        }

        // --- the atlas side: one upload, named regions ----------------------
        sheet_ = r_->createTexture(strip);
        atlas_.init(sheet_, strip.width, strip.height);
        atlas_.addGrid("hero", 0, 0, fw, fh, fc, 1);

        layout();
    }
    void shutdown() override {
        if (!r_) return;
        for (int f = 0; f < kFrames; ++f)
            if (loose_[f]) { r_->destroyTexture(loose_[f]); loose_[f] = 0; }
        if (sheet_) { r_->destroyTexture(sheet_); sheet_ = 0; }
    }

    void enter() override { t_ = 0; showRegions_ = false; animate_ = true; }

    void handleInput(const otacon::InputFrame& in) override {
        if (in.isPressed(otacon::Action::Jump)) showRegions_ = !showRegions_;
        if (in.isPressed(otacon::Action::Aux6)) animate_ = !animate_;
        if (in.isPressed(otacon::Action::Aux4)) layout();
    }

    void update(otacon::Real dt) override { if (animate_) t_ += otacon::toFloat(dt); }

    void render(otacon::IRenderer& r, const otacon::DebugRuntime&) override {
        const float top = layout::kTop + 16;
        const float half = W_ * 0.5f;
        r.drawLine(half, top - 8, half, H_ - 96, Color{1, 1, 1, 0.10f}, 1.f);

        r.drawText("SEPARATE TEXTURES", 10, top - 12, 1.f, Color{1.f, 0.55f, 0.45f, 1.f});
        r.drawText("ONE ATLAS", half + 10, top - 12, 1.f, Color{0.45f, 0.90f, 0.60f, 1.f});

        const int before = r.drawCalls();   // previous frame's total; see below
        (void)before;

        for (int i = 0; i < kSprites; ++i) {
            const Sprite& s = sprites_[std::size_t(i)];
            const int f = animate_ ? int(t_ * 9.f + float(i)) % kFrames : s.frame;
            const float sz = frameW_ * s.scale;
            const float sh = frameH_ * s.scale;

            // Left: bind a different texture for each sprite.
            if (loose_[f])
                r.drawImage(loose_[f], s.x, top + s.y, sz, sh);

            // Right: the same picture, every sprite from one sheet.
            char name[16];
            std::snprintf(name, sizeof name, "hero%d", f);
            atlas_.drawScaled(r, name, half + s.x, top + s.y, sz, sh);
        }

        if (showRegions_) drawSheet(r, half + 10, H_ - 92);
        drawPanel(r);
    }

    const char* status() const override {
        std::snprintf(buf_, sizeof buf_, "%d sprites per side  |  atlas regions %zu  |  draws last frame %d",
                      kSprites, atlas_.size(), lastDraws_);
        return buf_;
    }
    const char* keys() const override {
        return "SPACE  show the packed sheet and its regions\nE      freeze the animation\nF4     reshuffle the layout";
    }

private:
    struct Sprite { float x, y, scale; int frame; };

    void layout() {
        otacon::Random rng(0xA71A5u + shuffle_++);
        sprites_.clear();
        const float half = W_ * 0.5f;
        for (int i = 0; i < kSprites; ++i)
            sprites_.push_back({rng.range(8.f, half - 40.f),
                                rng.range(4.f, H_ - 150.f),
                                rng.range(1.2f, 2.6f),
                                rng.rangeI(0, kFrames)});
    }

    // The sheet itself, with each named region outlined. Seeing the names on the
    // art is what makes "an atlas is just an agreement about rectangles" land.
    void drawSheet(otacon::IRenderer& r, float x, float y) const {
        const float scale = 3.f;
        const float sw = frameW_ * float(atlas_.size()) * scale, sh = frameH_ * scale;
        r.fillRect(x - 2, y - 2, sw + 4, sh + 4, Color{0.08f, 0.09f, 0.13f, 1.f});
        r.drawImage(sheet_, x, y, sw, sh);
        for (std::size_t i = 0; i < atlas_.size(); ++i) {
            const otacon::AtlasRegion& reg = atlas_.at(i);
            r.drawRectOutline(x + reg.x * scale, y + reg.y * scale,
                              reg.w * scale, reg.h * scale, Color{1.f, 0.85f, 0.3f, 0.7f}, 1.f);
            r.drawText(reg.name.c_str(), x + reg.x * scale + 1, y + sh + 2, 1.f,
                       Color{1.f, 0.85f, 0.3f, 0.9f});
        }
    }

    void drawPanel(otacon::IRenderer& r) const {
        lastDraws_ = r.drawCalls();
        const float y0 = H_ - 84;
        r.fillRect(0, y0 - 6, W_, H_ - y0 + 6, Color{0.03f, 0.04f, 0.06f, 0.92f});
        float y = y0;
        r.drawText("WHY IT MATTERS", 8, y, 1.f, Color{0.55f, 0.80f, 1.f, 1.f}); y += 11;
        r.drawText("Every texture change forces a new draw call. Both halves above draw the same",
                   8, y, 1.f, Color{0.62f, 0.68f, 0.80f, 1.f}); y += 9;
        r.drawText("picture; the left binds a texture per sprite, the right binds one for all of them.",
                   8, y, 1.f, Color{0.62f, 0.68f, 0.80f, 1.f}); y += 13;

        char t[128];
        std::snprintf(t, sizeof t, "sprites per side   %d", kSprites);
        r.drawText(t, 8, y, 1.f, Color{0.80f, 0.86f, 0.95f, 1.f}); y += 9;
        std::snprintf(t, sizeof t, "atlas regions      %zu   (one texture)", atlas_.size());
        r.drawText(t, 8, y, 1.f, Color{0.45f, 0.90f, 0.60f, 1.f}); y += 9;
        std::snprintf(t, sizeof t, "separate textures  %d   (one bind each)", kFrames);
        r.drawText(t, 8, y, 1.f, Color{1.f, 0.55f, 0.45f, 1.f});

        const float px = W_ * 0.54f;
        float py = y0 + 22;
        r.drawText("this renderer submits one draw per primitive, so the win here is", px, py, 1.f,
                   Color{0.42f, 0.48f, 0.60f, 1.f}); py += 8;
        r.drawText("the bind count rather than the call count - add batching and the", px, py, 1.f,
                   Color{0.42f, 0.48f, 0.60f, 1.f}); py += 8;
        r.drawText("atlas side collapses to a single draw while the left cannot.", px, py, 1.f,
                   Color{0.42f, 0.48f, 0.60f, 1.f});
    }

    otacon::IRenderer* r_ = nullptr;
    otacon::Atlas atlas_;
    otacon::TextureHandle sheet_ = 0, loose_[kFrames]{};
    std::vector<Sprite> sprites_;
    float W_ = 640, H_ = 400, t_ = 0;
    int   frameW_ = 12, frameH_ = 16;
    std::uint32_t shuffle_ = 0;
    bool  showRegions_ = false, animate_ = true;
    mutable int lastDraws_ = 0;
    mutable char buf_[160]{};
};

} // namespace

Sample* makeSpriteAtlas() { return new SpriteAtlas(); }

} // namespace samples
