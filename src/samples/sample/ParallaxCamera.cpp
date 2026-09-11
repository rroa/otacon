// ParallaxCamera.cpp — sample 04: one camera, many scroll rates.
//
// Parallax is not a rendering feature; it is a single multiply. The engine's
// Camera turns a world position into a screen position with
//   screen = world - scroll * scrollFactor
// so a layer with factor 0.2 slides at a fifth of the camera's speed and reads
// as distant, and a factor of 0 pins a layer to the screen (that is all a HUD
// is). Nothing else in the pipeline knows about depth.
//
// The sample runs five layers at once and lets you scrub each factor, plus a
// split view that draws the same scene with every factor forced to 1 so you can
// see exactly what the multiply is buying.
#include "Sample.hpp"
#include "core/math/Random.hpp"
#include "scene/Camera.hpp"
#include "render/IRenderer.hpp"
#include "platform/Window.hpp"
#include "core/debug/Debug.hpp"
#include <cmath>
#include <cstdio>
#include <vector>

namespace samples {
namespace {

using otacon::Color;

struct Layer {
    const char* name;
    float factor;        // scrollFactor.x
    Color color;
    float baseY, height; // world-space band
    int   count;         // props in the band
    float propW, propMinH, propMaxH;
};

constexpr int kLayerCount = 5;

class ParallaxCamera final : public Sample {
public:
    void init(SampleContext& ctx) override {
        W_ = float(ctx.logicalW); H_ = float(ctx.logicalH);
        cam_.setViewport(ctx.logicalW, ctx.logicalH);
        layout();
    }

    void enter() override {
        for (int i = 0; i < kLayerCount; ++i) factor_[i] = kLayers[i].factor;
        scroll_ = 0; selected_ = 0; autoScroll_ = true; compare_ = false;
    }

    void handleInput(const otacon::InputFrame& in) override {
        if (in.isPressed(otacon::Action::Jump)) autoScroll_ = !autoScroll_;
        if (in.isPressed(otacon::Action::Aux6)) compare_ = !compare_;             // E
        if (in.selectSlot >= 1 && in.selectSlot <= kLayerCount) selected_ = in.selectSlot - 1;
        if (in.isPressed(otacon::Action::Aux4)) factor_[selected_] = clamp01(factor_[selected_] - 0.05f);
        if (in.isPressed(otacon::Action::Aux5)) factor_[selected_] = clamp01(factor_[selected_] + 0.05f);
        if (in.dragHeld) { scroll_ += (lastNx_ - in.mouseNx) * W_ * 1.5f; autoScroll_ = false; }
        lastNx_ = in.mouseNx;
    }

    void update(otacon::Real dt) override {
        if (autoScroll_) scroll_ += 90.f * otacon::toFloat(dt);
        if (scroll_ < 0) scroll_ += kWorldW;
        if (scroll_ > kWorldW) scroll_ -= kWorldW;
        // Camera::scroll is an offset ADDED to world positions (that is how a
        // follow camera stores it), so scrolling right is a negative scroll.
        cam_.scroll = {otacon::R(-scroll_), otacon::R(0)};
    }

    void render(otacon::IRenderer& r, const otacon::DebugRuntime& dbg) override {
        const float top = layout::kTop;
        const float sceneH = compare_ ? (H_ - top - 74) * 0.5f : (H_ - top - 74);

        drawScene(r, top, sceneH, false);
        if (compare_) {
            const float y2 = top + sceneH + 4;
            r.drawLine(0, y2 - 2, W_, y2 - 2, Color{0.3f, 0.36f, 0.46f, 1.f}, 1.f);
            drawScene(r, y2, sceneH, true);
            r.drawText("ALL FACTORS = 1  (no parallax)", 6, y2 + 3, 1.f, Color{1.f, 0.6f, 0.4f, 1.f});
        }

        if (dbg.enabled(otacon::DebugView::ParallaxBands)) drawBands(r, top, sceneH);
        drawPanel(r);
    }

    const char* status() const override {
        std::snprintf(buf_, sizeof buf_, "scroll %.0f  layer %d '%s' x%.2f%s",
                      scroll_, selected_ + 1, kLayers[selected_].name, factor_[selected_],
                      compare_ ? "  SPLIT" : "");
        return buf_;
    }
    const char* keys() const override {
        return "SPACE  auto-scroll on/off\nE      split view vs no parallax\n1-5    select a layer\nF4/F6  that layer's factor -/+\nright-drag  scrub the camera";
    }

private:
    static constexpr float kWorldW = 2400.f;

    static constexpr Layer kLayers[kLayerCount] = {
        {"sky band",   0.05f, {0.18f, 0.22f, 0.38f, 1.f}, 30.f,  70.f,  9,  90.f, 30.f, 66.f},
        {"far hills",  0.20f, {0.22f, 0.30f, 0.46f, 1.f}, 78.f,  54.f, 14,  60.f, 22.f, 52.f},
        {"mid towers", 0.45f, {0.26f, 0.36f, 0.54f, 1.f}, 96.f,  58.f, 18,  34.f, 20.f, 56.f},
        {"near roofs", 0.80f, {0.32f, 0.44f, 0.62f, 1.f}, 128.f, 44.f, 24,  26.f, 14.f, 42.f},
        {"foreground", 1.30f, {0.42f, 0.56f, 0.76f, 1.f}, 150.f, 30.f, 30,  18.f, 10.f, 28.f},
    };

    struct Prop { float x, w, h; };

    // Props are generated once with a fixed seed, so the scene is identical on
    // every run and every backend.
    void layout() {
        otacon::Random rng(0x9A11A);
        for (int i = 0; i < kLayerCount; ++i) {
            const Layer& L = kLayers[i];
            props_[i].clear();
            for (int k = 0; k < L.count; ++k) {
                Prop p;
                p.x = (kWorldW / L.count) * k + rng.range(-14.f, 14.f);
                p.w = L.propW * rng.range(0.7f, 1.25f);
                p.h = rng.range(L.propMinH, L.propMaxH);
                props_[i].push_back(p);
            }
        }
    }

    void drawScene(otacon::IRenderer& r, float top, float h, bool forceOne) const {
        const float scale = h / 190.f;      // the bands are authored for ~190 world units
        for (int i = 0; i < kLayerCount; ++i) {
            const Layer& L = kLayers[i];
            const float f = forceOne ? 1.f : factor_[i];   // fed to the camera as scrollFactor
            const bool sel = (i == selected_) && !forceOne;
            for (const Prop& p : props_[i]) {
                // The projection is the engine's, not ours. Camera::screenPoint
                // is world - scroll * scrollFactor, which is the whole of
                // parallax; a sample that re-derived it here would be
                // demonstrating its own arithmetic instead of the engine's.
                float sx = cam_.screenPoint({otacon::R(p.x), otacon::R(0)}, {f, 0.f}).x;
                // Wrap so a band tiles forever without needing more props.
                sx = std::fmod(std::fmod(sx, kWorldW) + kWorldW, kWorldW);
                if (sx > W_ + p.w) sx -= kWorldW;
                const float by = top + (L.baseY + L.height) * scale;
                const float ph = p.h * scale;
                Color c = L.color;
                if (sel) { c.r += 0.18f; c.g += 0.14f; c.b += 0.06f; }
                r.fillRect(sx, by - ph, p.w * scale, ph, c);
            }
        }
        // A ground strip so the near layers have somewhere to stand.
        r.fillRect(0, top + 180.f * scale, W_, h - 180.f * scale + 2, Color{0.10f, 0.13f, 0.20f, 1.f});
    }

    void drawBands(otacon::IRenderer& r, float top, float h) const {
        const float scale = h / 190.f;
        for (int i = 0; i < kLayerCount; ++i) {
            const Layer& L = kLayers[i];
            r.drawRectOutline(0, top + L.baseY * scale, W_, L.height * scale,
                              Color{1.f, 0.85f, 0.3f, 0.35f}, 1.f);
        }
    }

    void drawPanel(otacon::IRenderer& r) const {
        float y = H_ - 68;
        r.drawText("LAYER          FACTOR     SPEED", 6, y, 1.f, Color{0.55f, 0.80f, 1.f, 1.f});
        y += 10;
        for (int i = 0; i < kLayerCount; ++i) {
            const bool sel = (i == selected_);
            if (sel) r.fillRect(4, y - 2, 300, 10, Color{0.16f, 0.28f, 0.42f, 1.f});
            char row[96];
            std::snprintf(row, sizeof row, "%d %-12s  x%.2f", i + 1, kLayers[i].name, factor_[i]);
            r.drawText(row, 6, y, 1.f, sel ? Color{1, 1, 1, 1} : Color{0.55f, 0.62f, 0.75f, 1.f});
            // A bar whose length is the layer's apparent speed.
            const float bw = 120.f * (factor_[i] / 1.4f);
            r.fillRect(190, y, bw < 1 ? 1 : bw, 5, sel ? Color{1.f, 0.85f, 0.3f, 1.f}
                                                       : Color{0.40f, 0.58f, 0.80f, 1.f});
            y += 10;
        }
        r.drawText("screen = world - scroll * scrollFactor", 330, H_ - 58, 1.f, Color{1.f, 0.85f, 0.3f, 1.f});
        r.drawText("factor 0 pins to the screen (a HUD)", 330, H_ - 47, 1.f, Color{0.55f, 0.62f, 0.75f, 1.f});
        r.drawText("factor 1 is locked to the world", 330, H_ - 37, 1.f, Color{0.55f, 0.62f, 0.75f, 1.f});
        r.drawText("factor > 1 rushes past: it is in front", 330, H_ - 27, 1.f, Color{0.55f, 0.62f, 0.75f, 1.f});
        r.drawText("X toggles the engine's parallax band overlay", 330, H_ - 14, 1.f, Color{0.42f, 0.48f, 0.60f, 1.f});
    }

    static float clamp01(float v) { return v < 0.f ? 0.f : (v > 2.f ? 2.f : v); }

    otacon::Camera cam_;
    std::vector<Prop> props_[kLayerCount];
    float factor_[kLayerCount] = {0.05f, 0.20f, 0.45f, 0.80f, 1.30f};
    float W_ = 640, H_ = 400, scroll_ = 0, lastNx_ = 0;
    int   selected_ = 0;
    bool  autoScroll_ = true, compare_ = false;
    mutable char buf_[128]{};
};

} // namespace

Sample* makeParallaxCamera() { return new ParallaxCamera(); }

} // namespace samples
