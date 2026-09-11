// TilemapWorld.cpp — sample 03: a world made of indices, not sprites.
//
// The whole idea of a tilemap is indirection: the world is a small array of
// numbers, and one shared texture says what each number looks like. That buys
// you memory (a 128x64 world is 8 KB), and it buys you culling — you draw the
// window you can see, not the world you have.
//
// The sample makes both visible: scroll a map far larger than the screen, watch
// the drawn-tile count stay flat as you go, and toggle to a view that prints the
// raw indices over the art so the array underneath stops being abstract.
#include "Sample.hpp"
#include "common/Art.hpp"
#include "common/Rng.hpp"
#include "common/TileMap.hpp"
#include "render/IRenderer.hpp"
#include "platform/Window.hpp"
#include "core/debug/Debug.hpp"
#include <cmath>
#include <cstdio>

namespace samples {
namespace {

using otacon::Color;

constexpr int kMapW = 128, kMapH = 64;

class TilemapWorld final : public Sample {
public:
    void init(SampleContext& ctx) override {
        r_ = ctx.renderer;
        W_ = float(ctx.logicalW); H_ = float(ctx.logicalH);
        int tileSize = 0, tileCount = 0;
        tileset_ = r_->createTexture(art::tileset(tileSize, tileCount));
        tileCount_ = tileCount;
        build();
    }
    void shutdown() override { if (tileset_ && r_) { r_->destroyTexture(tileset_); tileset_ = 0; } }

    void enter() override {
        scrollX_ = 0; zoom_ = 2; showIndices_ = false; autoScroll_ = true;
        centreOnGround();
    }
    // The generator puts the surface around row 26. Opening the sample looking
    // at empty sky above it would be a poor first impression, so frame it.
    void centreOnGround() {
        const float dest = 16.f * float(zoom_);
        const float viewH = H_ - layout::kTop - 30.f;
        scrollY_ = 26.f * dest - viewH * 0.42f;
        if (scrollY_ < 0) scrollY_ = 0;
    }

    void handleInput(const otacon::InputFrame& in) override {
        if (in.isPressed(otacon::Action::Jump)) autoScroll_ = !autoScroll_;
        if (in.isPressed(otacon::Action::Aux6)) showIndices_ = !showIndices_;     // E
        if (in.isPressed(otacon::Action::Aux4)) { build(); }                      // F4 regenerate
        if (in.selectSlot >= 1 && in.selectSlot <= 4 && in.selectSlot != zoom_) {
            zoom_ = in.selectSlot;
            centreOnGround();
        }
        // Drag with the right mouse button to pan by hand.
        if (in.dragHeld) {
            if (dragging_) {
                scrollX_ -= (in.mouseNx - lastNx_) * W_;
                scrollY_ -= (in.mouseNy - lastNy_) * H_;
                autoScroll_ = false;
            }
            dragging_ = true;
        } else {
            dragging_ = false;
        }
        lastNx_ = in.mouseNx; lastNy_ = in.mouseNy;
    }

    void update(otacon::Real dt) override {
        if (autoScroll_) scrollX_ += 70.f * otacon::toFloat(dt);
        clampScroll();
    }

    void render(otacon::IRenderer& r, const otacon::DebugRuntime&) override {
        const float top = layout::kTop;
        const float viewH = H_ - top - 30;
        const float dest = 16.f * float(zoom_);

        drawn_ = map_.draw(r, tileset_, tileCount_, scrollX_, scrollY_, dest, 0, top, W_, viewH);

        if (showIndices_) drawIndices(r, dest, top, viewH);

        // --- the minimap: where the drawn window sits in the whole world ----
        const float mmW = 128.f, mmH = mmW * float(kMapH) / float(kMapW);
        const float mmX = W_ - mmW - 8, mmY = top + 4;
        r.fillRect(mmX - 1, mmY - 1, mmW + 2, mmH + 2, Color{0, 0, 0, 0.55f});
        // One rect per 2x2 block keeps the minimap to a few hundred draws.
        const float cw = mmW / (kMapW / 2.f), ch = mmH / (kMapH / 2.f);
        for (int y = 0; y < kMapH; y += 2) {
            for (int x = 0; x < kMapW; x += 2) {
                const std::uint8_t t = map_.at(x, y);
                if (!t) continue;
                r.fillRect(mmX + (x / 2) * cw, mmY + (y / 2) * ch, cw, ch, kMiniColor[t % 7]);
            }
        }
        const float vw = (W_ / dest) * (mmW / kMapW), vh = (viewH / dest) * (mmH / kMapH);
        r.drawRectOutline(mmX + (scrollX_ / dest) * (mmW / kMapW),
                          mmY + (scrollY_ / dest) * (mmH / kMapH), vw, vh,
                          Color{1.f, 0.85f, 0.3f, 1.f}, 1.f);
        r.drawText("WORLD", mmX, mmY + mmH + 3, 1.f, Color{0.55f, 0.62f, 0.75f, 1.f});

        // --- the cost readout ------------------------------------------------
        r.fillRect(0, H_ - 24, W_, 24, Color{0.03f, 0.04f, 0.06f, 0.88f});
        char info[160];
        std::snprintf(info, sizeof info,
                      "world %dx%d = %d tiles (%d KB)   tiles drawn this frame: %d",
                      kMapW, kMapH, kMapW * kMapH, (kMapW * kMapH) / 1024, drawn_);
        r.drawText(info, 6, H_ - 20, 1.f, Color{0.55f, 0.62f, 0.75f, 1.f});
        r.drawText("the drawn count barely moves as you scroll — that is the culling",
                   6, H_ - 10, 1.f, Color{0.42f, 0.48f, 0.60f, 1.f});
    }

    const char* status() const override {
        std::snprintf(buf_, sizeof buf_, "scroll %.0f,%.0f  zoom x%d  drawn %d%s",
                      scrollX_, scrollY_, zoom_, drawn_, showIndices_ ? "  INDICES" : "");
        return buf_;
    }
    const char* keys() const override {
        return "SPACE  auto-scroll on/off\nE      overlay the raw tile indices\nF4     regenerate the world\n1-4    zoom\nright-drag  pan by hand";
    }

private:
    // A small layered generator: ground height from summed noise, then water in
    // the dips, sand at the shoreline, stone underground, and brick ruins.
    void build() {
        map_.resize(kMapW, kMapH, 0);
        map_.firstSolid = 1;
        Rng rng(seed_++);
        float h[kMapW];
        for (int x = 0; x < kMapW; ++x) {
            const float t = float(x);
            h[x] = 26.f
                 + 7.f * std::sin(t * 0.055f)
                 + 3.5f * std::sin(t * 0.17f + 1.3f)
                 + 1.5f * std::sin(t * 0.41f + 2.7f);
        }
        for (int x = 0; x < kMapW; ++x) {
            const int top = int(h[x]);
            for (int y = 0; y < kMapH; ++y) {
                if (y < top) {
                    map_.set(x, y, y >= kWaterLine ? 4 : 0);   // water fills low air
                } else if (y == top) {
                    map_.set(x, y, top >= kWaterLine - 1 ? 5 : 1);   // shoreline sand vs grass
                } else if (y < top + 4) {
                    map_.set(x, y, 2);                       // dirt
                } else {
                    map_.set(x, y, 3);                       // stone
                }
            }
            // Occasional brick ruin standing on the surface.
            if (rng.chance(0.06f) && top < 31) {
                const int hgt = rng.rangeI(2, 5);
                for (int k = 1; k <= hgt; ++k) map_.set(x, top - k, 6);
            }
        }
    }

    void clampScroll() {
        const float dest = 16.f * float(zoom_);
        const float maxX = kMapW * dest - W_, maxY = kMapH * dest - (H_ - layout::kTop - 30);
        if (scrollX_ > maxX) scrollX_ = autoScroll_ ? 0.f : maxX;
        if (scrollX_ < 0) scrollX_ = 0;
        if (scrollY_ > maxY) scrollY_ = maxY;
        if (scrollY_ < 0) scrollY_ = 0;
    }

    // The array, drawn over the art. Only legible when zoomed in, which is the
    // honest constraint — so it only prints when a tile is big enough to hold text.
    void drawIndices(otacon::IRenderer& r, float dest, float top, float viewH) const {
        if (dest < 24) {
            r.drawText("zoom in (2-4) to see the indices", 8, top + 6, 1.f, Color{1.f, 0.7f, 0.35f, 1.f});
            return;
        }
        const int x0 = int(scrollX_ / dest), x1 = int((scrollX_ + W_) / dest);
        const int y0 = int(scrollY_ / dest), y1 = int((scrollY_ + viewH) / dest);
        for (int ty = y0; ty <= y1; ++ty) {
            for (int tx = x0; tx <= x1; ++tx) {
                if (!map_.inBounds(tx, ty)) continue;
                const float dx = tx * dest - scrollX_, dy = top + ty * dest - scrollY_;
                if (dy < top) continue;
                r.fillRect(dx, dy, dest, dest, Color{0, 0, 0, 0.45f});
                r.drawRectOutline(dx, dy, dest, dest, Color{1, 1, 1, 0.12f}, 1.f);
                char n[4]; std::snprintf(n, sizeof n, "%u", map_.at(tx, ty));
                r.drawText(n, dx + dest * 0.5f - 2, dy + dest * 0.5f - 2, 1.f,
                           Color{1.f, 0.85f, 0.35f, 1.f});
            }
        }
    }

    // Rows at or below this fill with water where there is no ground, which is
    // what turns the generator's dips into lakes rather than dry chasms.
    static constexpr int kWaterLine = 31;

    static constexpr Color kMiniColor[7] = {
        {0, 0, 0, 0},
        {0.33f, 0.62f, 0.28f, 1.f}, {0.50f, 0.36f, 0.22f, 1.f}, {0.46f, 0.47f, 0.52f, 1.f},
        {0.16f, 0.38f, 0.68f, 1.f}, {0.84f, 0.76f, 0.52f, 1.f}, {0.55f, 0.24f, 0.22f, 1.f},
    };

    otacon::IRenderer* r_ = nullptr;
    otacon::TextureHandle tileset_ = 0;
    TileMap map_;
    float W_ = 640, H_ = 400;
    float scrollX_ = 0, scrollY_ = 200;
    float lastNx_ = 0, lastNy_ = 0;
    int   tileCount_ = 6, zoom_ = 2, drawn_ = 0;
    std::uint32_t seed_ = 7;
    bool  showIndices_ = false, autoScroll_ = true, dragging_ = false;
    mutable char buf_[128]{};
};

} // namespace

Sample* makeTilemapWorld() { return new TilemapWorld(); }

} // namespace samples
