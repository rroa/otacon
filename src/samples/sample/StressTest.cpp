// StressTest.cpp — sample 15: find the wall, and learn which wall it is.
//
// "How many sprites can it draw" is the wrong question, because the answer
// depends entirely on which resource you exhaust first. This sample ramps a
// chosen primitive until the frame time climbs, and reports the three numbers
// that tell you *why* it climbed:
//
//   draw calls   how many times geometry was handed to the backend
//   vertices     how much geometry that was
//   ms/frame     what it cost
//
// The instructive part is comparing modes. Solid quads and textured sprites push
// the same six vertices each, but every texture change forces a new draw call —
// so the single-texture mode stays flat while the many-textures mode collapses
// at a fraction of the count. That gap is the entire argument for atlasing, and
// it is the kind of thing you should measure on your own machine rather than
// take on trust.
#include "Sample.hpp"
#include "common/Art.hpp"
#include "common/Rng.hpp"
#include "render/IRenderer.hpp"
#include "platform/Window.hpp"
#include "core/debug/Debug.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

namespace samples {
namespace {

using otacon::Color;

enum Mode { kSolid = 0, kSpritesOne, kSpritesMany, kLines, kText, kModeCount };
const char* kModeName[kModeCount] = {
    "solid quads", "sprites (1 texture)", "sprites (8 textures)", "lines", "text glyphs"
};
const char* kModeNote[kModeCount] = {
    "fillRect: 6 verts, and its own draw call",
    "one bind for every sprite on screen",
    "8 textures, cycled — watch the draw calls",
    "2 triangles per line segment too",
    "each glyph is its own little quad batch",
};

constexpr int kMaxItems = 20000;
constexpr int kPlot = 200;

class StressTest final : public Sample {
public:
    void init(SampleContext& ctx) override {
        r_ = ctx.renderer;
        W_ = float(ctx.logicalW); H_ = float(ctx.logicalH);

        int fw = 0, fh = 0, fc = 0;
        otacon::Image sheet = art::heroSheet(fw, fh, fc);
        frameW_ = fw; frameCount_ = fc;
        sheetTex_ = r_->createTexture(sheet);
        // Eight tinted copies, purely so the "many textures" mode has something
        // to force a bind change with.
        for (int i = 0; i < kTexVariants; ++i) {
            otacon::Image v = sheet;
            const float k = 0.55f + 0.45f * (float(i) / kTexVariants);
            for (std::size_t p = 0; p < v.rgba.size(); p += 4) {
                v.rgba[p]     = std::uint8_t(v.rgba[p] * k);
                v.rgba[p + 1] = std::uint8_t(v.rgba[p + 1] * (1.f - k * 0.3f));
                v.rgba[p + 2] = std::uint8_t(v.rgba[p + 2] * (0.5f + k * 0.5f));
            }
            variants_[i] = r_->createTexture(v);
        }
        items_.resize(kMaxItems);
        seedItems();
    }
    void shutdown() override {
        if (!r_) return;
        if (sheetTex_) { r_->destroyTexture(sheetTex_); sheetTex_ = 0; }
        for (int i = 0; i < kTexVariants; ++i)
            if (variants_[i]) { r_->destroyTexture(variants_[i]); variants_[i] = 0; }
    }

    void enter() override {
        mode_ = kSolid; count_ = 500; autoRamp_ = false; t_ = 0;
        head_ = 0; worst_ = 0;
        for (int i = 0; i < kPlot; ++i) plot_[i] = 0;
        seedItems();
    }

    void handleInput(const otacon::InputFrame& in) override {
        if (in.isPressed(otacon::Action::Jump)) autoRamp_ = !autoRamp_;
        if (in.selectSlot >= 1 && in.selectSlot <= kModeCount) { mode_ = in.selectSlot - 1; reset(); }
        if (in.isPressed(otacon::Action::Aux6)) { mode_ = (mode_ + 1) % kModeCount; reset(); }   // E
        if (in.isPressed(otacon::Action::Aux4)) count_ = std::max(50, count_ / 2);
        if (in.isPressed(otacon::Action::Aux5)) count_ = std::min(kMaxItems, count_ * 2);
        if (in.isPressed(otacon::Action::Aux7)) reset();
    }

    void update(otacon::Real dt) override {
        const float d = otacon::toFloat(dt);
        t_ += d;
        frameMs_ = d * 1000.f;
        plot_[head_] = frameMs_;
        head_ = (head_ + 1) % kPlot;
        worst_ = std::max(worst_, frameMs_);

        // Auto-ramp climbs while the frame is comfortably under budget and stops
        // once it is not, so it settles on this machine's actual ceiling.
        if (autoRamp_) {
            rampTimer_ += d;
            if (rampTimer_ > 0.25f) {
                rampTimer_ = 0.f;
                const float avg = average();
                if (avg < 15.f && count_ < kMaxItems) count_ = int(count_ * 1.15f) + 8;
                else if (avg > 22.f && count_ > 60) { count_ = int(count_ * 0.92f); autoRamp_ = false; }
            }
        }

        for (int i = 0; i < count_ && i < kMaxItems; ++i) {
            Item& it = items_[i];
            it.x += it.vx * d; it.y += it.vy * d;
            if (it.x < 0) { it.x = 0; it.vx = -it.vx; }
            if (it.x > W_) { it.x = W_; it.vx = -it.vx; }
            if (it.y < layout::kTop) { it.y = layout::kTop; it.vy = -it.vy; }
            if (it.y > H_ - 78) { it.y = H_ - 78; it.vy = -it.vy; }
            it.angle += it.spin * d;
        }
    }

    void render(otacon::IRenderer& r, const otacon::DebugRuntime&) override {
        const int n = std::min(count_, kMaxItems);
        const float uw = 1.f / float(frameCount_);

        for (int i = 0; i < n; ++i) {
            const Item& it = items_[i];
            switch (mode_) {
                case kSolid:
                    r.fillRect(it.x, it.y, it.size, it.size, it.color);
                    break;
                case kSpritesOne: {
                    const float u0 = float(it.frame) * uw;
                    r.drawImage(sheetTex_, it.x, it.y, it.size, it.size * 1.33f, u0, 0, u0 + uw, 1);
                    break;
                }
                case kSpritesMany: {
                    const float u0 = float(it.frame) * uw;
                    r.drawImage(variants_[i % kTexVariants], it.x, it.y, it.size, it.size * 1.33f,
                                u0, 0, u0 + uw, 1);
                    break;
                }
                case kLines:
                    r.drawLine(it.x, it.y, it.x + std::cos(it.angle) * 14.f,
                               it.y + std::sin(it.angle) * 14.f, it.color, 1.f);
                    break;
                default:
                    r.drawText("otacon", it.x, it.y, 1.f, it.color);
                    break;
            }
        }
        drawPanel(r, r.drawCalls(), r.vertexCount());
    }

    const char* status() const override {
        std::snprintf(buf_, sizeof buf_, "%s  n=%d  %.1f ms  %d draws%s",
                      kModeName[mode_], count_, average(), lastDraws_, autoRamp_ ? "  RAMPING" : "");
        return buf_;
    }
    const char* keys() const override {
        return "SPACE  auto-ramp to this machine's ceiling\n1-5    primitive type   E cycles\nF4/F6  halve / double the count\nF7     reset the measurements";
    }

private:
    struct Item { float x, y, vx, vy, size, angle, spin; int frame; Color color; };
    static constexpr int kTexVariants = 8;

    void reset() {
        worst_ = 0; rampTimer_ = 0;
        for (int i = 0; i < kPlot; ++i) plot_[i] = 0;
    }
    void seedItems() {
        Rng rng(0x57E55);
        for (int i = 0; i < kMaxItems; ++i) {
            Item& it = items_[i];
            it.x = rng.range(0, W_); it.y = rng.range(layout::kTop, H_ - 78);
            it.vx = rng.range(-60, 60); it.vy = rng.range(-60, 60);
            it.size = rng.range(4, 12);
            it.angle = rng.range(0, 6.28f); it.spin = rng.range(-3, 3);
            it.frame = rng.rangeI(0, 6);
            it.color = Color{rng.range(0.3f, 1.f), rng.range(0.4f, 1.f), rng.range(0.5f, 1.f), 0.9f};
        }
    }
    float average() const {
        float s = 0; for (int i = 0; i < kPlot; ++i) s += plot_[i];
        return s / kPlot;
    }

    void drawPanel(otacon::IRenderer& r, int draws, std::size_t verts) const {
        lastDraws_ = draws;
        const float y0 = H_ - 74;
        // The background has to start above the column headers, which sit at
        // y0-10; otherwise they float over the scene and become unreadable.
        r.fillRect(0, y0 - 16, W_, H_ - y0 + 16, Color{0.03f, 0.04f, 0.06f, 0.92f});

        // The frame-time plot, with the 60 and 30 fps budgets marked.
        const float gx = 8, gy = y0, gw = W_ * 0.42f, gh = 44;
        r.fillRect(gx, gy, gw, gh, Color{0.06f, 0.07f, 0.10f, 1.f});
        auto mapY = [&](float ms) {
            const float v = gy + gh - (ms / 50.f) * gh;
            return v < gy ? gy : (v > gy + gh ? gy + gh : v);
        };
        r.drawLine(gx, mapY(16.7f), gx + gw, mapY(16.7f), Color{0.3f, 0.8f, 0.3f, 0.5f}, 1.f);
        r.drawLine(gx, mapY(33.3f), gx + gw, mapY(33.3f), Color{0.8f, 0.7f, 0.2f, 0.5f}, 1.f);
        for (int i = 1; i < kPlot; ++i) {
            const float a = plot_[(head_ + i - 1) % kPlot], b = plot_[(head_ + i) % kPlot];
            r.drawLine(gx + (float(i - 1) / kPlot) * gw, mapY(a),
                       gx + (float(i) / kPlot) * gw, mapY(b), Color{0.4f, 0.9f, 1.f, 0.95f}, 1.f);
        }
        r.drawRectOutline(gx, gy, gw, gh, Color{0.22f, 0.28f, 0.38f, 1.f}, 1.f);
        r.drawText("FRAME TIME (50ms full scale)", gx, gy - 10, 1.f, Color{0.55f, 0.80f, 1.f, 1.f});

        // The three numbers that explain the plot.
        const float cx = gx + gw + 16;
        float y = y0 - 10;
        char t[96];
        r.drawText("COST", cx, y, 1.f, Color{0.55f, 0.80f, 1.f, 1.f}); y += 11;
        std::snprintf(t, sizeof t, "items      %d", count_);
        r.drawText(t, cx, y, 1.f, Color{0.80f, 0.86f, 0.95f, 1.f}); y += 9;
        std::snprintf(t, sizeof t, "draw calls %d", draws);
        r.drawText(t, cx, y, 1.f, draws > 400 ? Color{1.f, 0.55f, 0.45f, 1.f}
                                              : Color{0.55f, 0.62f, 0.75f, 1.f}); y += 9;
        std::snprintf(t, sizeof t, "vertices   %zu", verts);
        r.drawText(t, cx, y, 1.f, Color{0.55f, 0.62f, 0.75f, 1.f}); y += 9;
        std::snprintf(t, sizeof t, "avg %.1f ms   peak %.1f ms", average(), worst_);
        r.drawText(t, cx, y, 1.f, Color{0.55f, 0.62f, 0.75f, 1.f});

        // Mode picker along the bottom.
        const float mx = cx + 190;
        y = y0 - 10;
        r.drawText("1-5 PRIMITIVE", mx, y, 1.f, Color{0.55f, 0.80f, 1.f, 1.f}); y += 11;
        for (int i = 0; i < kModeCount; ++i) {
            char row[48]; std::snprintf(row, sizeof row, "%d %s", i + 1, kModeName[i]);
            r.drawText(row, mx, y, 1.f, i == mode_ ? Color{1.f, 0.85f, 0.35f, 1.f}
                                                   : Color{0.48f, 0.54f, 0.66f, 1.f});
            y += 9;
        }
        r.drawText(kModeNote[mode_], gx, gy + gh + 6, 1.f, Color{0.42f, 0.48f, 0.60f, 1.f});
        // Worth saying plainly, because the numbers above make it unmissable:
        // IRenderer submits each primitive on its own, so draw calls track item
        // count almost exactly. Batching is the obvious next optimisation, and
        // this sample is how you would measure whether it paid.
        r.drawText("this renderer submits one draw per primitive — draws track items 1:1",
                   gx, gy + gh + 15, 1.f, Color{0.42f, 0.48f, 0.60f, 1.f});
    }

    otacon::IRenderer* r_ = nullptr;
    otacon::TextureHandle sheetTex_ = 0, variants_[kTexVariants]{};
    std::vector<Item> items_;
    float plot_[kPlot]{};
    float W_ = 640, H_ = 400, t_ = 0, frameMs_ = 0, worst_ = 0, rampTimer_ = 0;
    int   mode_ = kSolid, count_ = 500, head_ = 0;
    int   frameW_ = 12, frameCount_ = 6;
    bool  autoRamp_ = false;
    mutable int lastDraws_ = 0;
    mutable char buf_[160]{};
};

} // namespace

Sample* makeStressTest() { return new StressTest(); }

} // namespace samples
