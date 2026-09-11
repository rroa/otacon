// IsoGridSample.cpp — sample 21: the isometric foundation, made checkable.
//
// Every bug in an isometric game is a coordinate bug, and they all look the
// same on screen: things draw in the wrong order, or your click lands one tile
// off. This sample exists so you can tell those apart.
//
// The readout shows the same point in all three spaces at once -- screen pixels,
// continuous world coordinates, integer tile -- so when picking disagrees with
// rendering you can see WHICH conversion is wrong instead of guessing. The
// hovered tile is outlined with IsoGrid::diamondCorners, the same maths the
// sprite is rasterised from, so a mismatch between the outline and the art is
// itself the bug report.
//
// Deliberately prototype art. Flat, unambiguous tiles are the point: a depth
// sort that is subtly wrong is invisible against a painted castle and obvious
// against these.
#include "Sample.hpp"
#include "common/Art.hpp"
#include "core/math/Noise.hpp"
#include "core/math/Random.hpp"
#include "scene/Iso.hpp"
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
using otacon::IsoGrid;
using otacon::Vec2f;

constexpr int kMapW = 24, kMapH = 24;
constexpr int kTileW = 32, kTileH = 16, kSideH = 8;
constexpr int kTerrainKinds = 4;

// Anything drawn goes through one list and one sort, which is the only way
// units, buildings and terrain can be guaranteed to interleave correctly.
struct Drawable {
    float depth;
    float sx, sy;              // screen position (pre-camera)
    otacon::TextureHandle tex;
    float w, h;
    Color tint;
};

class IsoGridSample final : public Sample {
public:
    void init(SampleContext& ctx) override {
        r_ = ctx.renderer;
        W_ = float(ctx.logicalW); H_ = float(ctx.logicalH);
        grid_ = IsoGrid{float(kTileW), float(kTileH), float(kSideH)};

        const std::uint32_t palette[kTerrainKinds] = {
            0x5F9E4Au,   // grass
            0x8A7A4Eu,   // dirt
            0x6E7480u,   // stone
            0xC8B98Cu,   // sand
        };
        for (int i = 0; i < kTerrainKinds; ++i)
            tile_[i] = r_->createTexture(art::isoTile(kTileW, kTileH, kSideH, palette[i]));
        unit_    = r_->createTexture(art::isoMarker(8, 18, 0xE2574Cu));
        building_ = r_->createTexture(art::isoTile(kTileW * 3, kTileH * 3, 26, 0x9A6B4Fu));
        buildTilesX_ = 3; buildTilesY_ = 3;

        buildMap();
    }
    void shutdown() override {
        if (!r_) return;
        for (int i = 0; i < kTerrainKinds; ++i)
            if (tile_[i]) { r_->destroyTexture(tile_[i]); tile_[i] = 0; }
        if (unit_)     { r_->destroyTexture(unit_); unit_ = 0; }
        if (building_) { r_->destroyTexture(building_); building_ = 0; }
    }

    void enter() override {
        // Frame the map rather than guessing an offset: put the centre tile in
        // the middle of the content area. An iso map's screen extent is
        // (w+h)*tileH/2 tall, which is much taller than it is wide, so a camera
        // that is merely "somewhere near the top left" loses half of it.
        camX_ = 0.f;
        const Vec2f mid = grid_.toScreen(kMapW * 0.5f, kMapH * 0.5f, 2.f);
        const float viewMidY = (layout::kTop + (H_ - 80.f)) * 0.5f;
        camY_ = 70.f - (viewMidY - mid.y);
        showGrid_ = true; showDepth_ = false; flat_ = false;
        hoverX_ = hoverY_ = 0;
        t_ = 0;
    }

    void handleInput(const otacon::InputFrame& in) override {
        mouse_ = {in.mouseNx * W_, in.mouseNy * H_};
        if (in.isPressed(otacon::Action::Jump)) showGrid_ = !showGrid_;
        if (in.isPressed(otacon::Action::Aux6)) showDepth_ = !showDepth_;      // E
        if (in.isPressed(otacon::Action::Aux4)) { flat_ = !flat_; }            // F4
        if (in.isPressed(otacon::Action::Aux5)) buildMap();                    // F6
        // Right-drag pans: an RTS camera, in the crudest possible form.
        if (in.dragHeld) {
            if (dragging_) { camX_ -= (in.mouseNx - lastNx_) * W_; camY_ -= (in.mouseNy - lastNy_) * H_; }
            dragging_ = true;
        } else dragging_ = false;
        lastNx_ = in.mouseNx; lastNy_ = in.mouseNy;
    }

    void update(otacon::Real dt) override {
        t_ += otacon::toFloat(dt);
        // Two units walking a fixed circuit, so the sorting against the building
        // is exercised every frame rather than only when you happen to look.
        for (int i = 0; i < kUnits; ++i) {
            const float phase = t_ * 0.35f + float(i) * 3.14159f;
            unitPos_[i] = {11.5f + std::cos(phase) * 6.5f, 11.5f + std::sin(phase) * 6.5f};
        }
        pick();
    }

    void render(otacon::IRenderer& r, const otacon::DebugRuntime&) override {
        draw_.clear();
        collectTerrain();
        collectObjects();

        // ONE sort for everything. Terrain, buildings and units are the same
        // kind of problem, and sorting them in separate passes is what makes a
        // unit appear inside a wall.
        std::sort(draw_.begin(), draw_.end(),
                  [](const Drawable& a, const Drawable& b) { return a.depth < b.depth; });
        for (const Drawable& d : draw_)
            r.drawImage(d.tex, d.sx, d.sy, d.w, d.h, 0, 0, 1, 1, d.tint);

        if (showGrid_) drawHoverDiamond(r);
        drawReadout(r);
    }

    const char* status() const override {
        std::snprintf(buf_, sizeof buf_, "tile %d,%d  elev %d  %s  %d drawn of %d",
                      hoverX_, hoverY_, elevationAt(hoverX_, hoverY_),
                      flat_ ? "FLAT" : "elevated", int(draw_.size()), kMapW * kMapH);
        return buf_;
    }
    const char* keys() const override {
        return "mouse       hover picks a tile\nright-drag  pan the camera\nSPACE       tile outline on/off\nE           depth keys\nF4          flatten the terrain\nF6          regenerate";
    }

private:
    static constexpr int kUnits = 2;
    static constexpr int kBuildX = 10, kBuildY = 10;

    float originX() const { return W_ * 0.5f - camX_; }
    float originY() const { return 70.f - camY_; }

    int elevationAt(int x, int y) const {
        if (flat_ || x < 0 || y < 0 || x >= kMapW || y >= kMapH) return 0;
        return height_[std::size_t(y) * kMapW + x];
    }
    int kindAt(int x, int y) const {
        if (x < 0 || y < 0 || x >= kMapW || y >= kMapH) return 0;
        return kind_[std::size_t(y) * kMapW + x];
    }

    void buildMap() {
        height_.assign(std::size_t(kMapW) * kMapH, 0);
        kind_.assign(std::size_t(kMapW) * kMapH, 0);
        for (int y = 0; y < kMapH; ++y)
            for (int x = 0; x < kMapW; ++x) {
                const float n = otacon::noise::fbm(float(x) * 0.14f, float(y) * 0.14f, 4, 2.f, 0.5f, seed_);
                const int h = int(n * 4.2f);
                height_[std::size_t(y) * kMapW + x] = h;
                kind_[std::size_t(y) * kMapW + x] = h >= 3 ? 2 : (h >= 2 ? 1 : (n < 0.28f ? 3 : 0));
            }
        // The building's footprint is levelled, because a multi-tile structure
        // on sloped ground is a separate problem from sorting one.
        for (int y = kBuildY; y < kBuildY + buildTilesY_; ++y)
            for (int x = kBuildX; x < kBuildX + buildTilesX_; ++x)
                if (x < kMapW && y < kMapH) height_[std::size_t(y) * kMapW + x] = 2;
        ++seed_;
    }

    // Only the tiles the view can touch. A 24x24 map does not need this, but the
    // call is here because a 256x256 one does and the code should not change.
    void collectTerrain() {
        int x0, y0, x1, y1;
        grid_.visibleTileRange({-originX(), -originY()},
                               {W_ - originX(), H_ - originY()}, 6.f * kSideH, x0, y0, x1, y1);
        x0 = std::max(0, x0); y0 = std::max(0, y0);
        x1 = std::min(kMapW - 1, x1); y1 = std::min(kMapH - 1, y1);

        for (int y = y0; y <= y1; ++y)
            for (int x = x0; x <= x1; ++x) {
                const int e = elevationAt(x, y);
                const Vec2f s = grid_.toScreen(float(x), float(y), float(e));
                Drawable d;
                d.depth = IsoGrid::depth(float(x), float(y));
                // The sprite's top-left: the diamond is centred on the tile's
                // top corner, and the side faces hang below it.
                d.sx = originX() + s.x - kTileW * 0.5f;
                d.sy = originY() + s.y;
                d.tex = tile_[kindAt(x, y)];
                d.w = float(kTileW); d.h = float(kTileH + kSideH);
                d.tint = (x == hoverX_ && y == hoverY_) ? Color{1.35f, 1.35f, 1.1f, 1.f}
                                                        : Color{1, 1, 1, 1};
                draw_.push_back(d);
            }
    }

    void collectObjects() {
        // The building. Anchored at its origin tile but sorted by its FAR
        // corner, which is what lets a unit at (11,11) draw in front of it.
        {
            const int e = elevationAt(kBuildX, kBuildY);
            const Vec2f s = grid_.toScreen(float(kBuildX), float(kBuildY), float(e));
            Drawable d;
            d.depth = IsoGrid::footprintDepth(float(kBuildX), float(kBuildY), buildTilesX_, buildTilesY_);
            d.sx = originX() + s.x - kTileW * 1.5f;
            d.sy = originY() + s.y - 26.f;
            d.tex = building_;
            d.w = float(kTileW * 3); d.h = float(kTileH * 3 + 26);
            d.tint = Color{1, 1, 1, 1};
            draw_.push_back(d);
        }
        for (int i = 0; i < kUnits; ++i) {
            const Vec2f w = unitPos_[i];
            const int e = elevationAt(int(w.x), int(w.y));
            const Vec2f s = grid_.toScreen(w.x, w.y, float(e));
            Drawable d;
            // A unit sorts at its own continuous position, not its tile -- half
            // a tile of error is exactly enough to pop it through a wall.
            d.depth = IsoGrid::depth(w.x, w.y, 0.02f);   // bias: above the floor
            d.sx = originX() + s.x - 4.f;
            d.sy = originY() + s.y - 14.f;
            d.tex = unit_;
            d.w = 8.f; d.h = 18.f;
            d.tint = Color{1, 1, 1, 1};
            draw_.push_back(d);
        }
    }

    /*
     * Picking. Elevation makes this genuinely harder than the inverse alone: a
     * screen point does not know which height it was projected at, so the only
     * honest method is to test candidate tiles from the highest down and take
     * the first whose diamond actually contains the point. Testing flat-first
     * is what makes clicks land behind hills.
     */
    void pick() {
        const Vec2f local{mouse_.x - originX(), mouse_.y - originY()};
        hoverX_ = hoverY_ = -1;
        int bestDepth = -1;
        for (int e = 6; e >= 0; --e) {
            const Vec2f w = grid_.toWorld(local, float(e));
            int tx = 0, ty = 0;
            IsoGrid::toTile(w, tx, ty);
            if (tx < 0 || ty < 0 || tx >= kMapW || ty >= kMapH) continue;
            if (elevationAt(tx, ty) != e) continue;                // not this layer
            if (!grid_.containsPoint(float(tx), float(ty), float(e), local)) continue;
            const int d = tx + ty;
            if (d > bestDepth) { bestDepth = d; hoverX_ = tx; hoverY_ = ty; }
        }
    }

    void drawHoverDiamond(otacon::IRenderer& r) const {
        if (hoverX_ < 0) return;
        Vec2f c[4];
        grid_.diamondCorners(float(hoverX_), float(hoverY_), float(elevationAt(hoverX_, hoverY_)), c);
        for (int i = 0; i < 4; ++i) {
            const Vec2f a{originX() + c[i].x, originY() + c[i].y};
            const Vec2f b{originX() + c[(i + 1) % 4].x, originY() + c[(i + 1) % 4].y};
            r.drawLine(a.x, a.y, b.x, b.y, Color{1.f, 0.95f, 0.4f, 1.f}, 1.f);
        }
    }

    void drawReadout(otacon::IRenderer& r) const {
        const float y0 = H_ - 74;
        r.fillRect(0, y0 - 6, W_, H_ - y0 + 6, Color{0.03f, 0.04f, 0.06f, 0.92f});
        float y = y0;
        r.drawText("ONE POINT, THREE SPACES", 8, y, 1.f, Color{0.55f, 0.80f, 1.f, 1.f}); y += 11;

        const Vec2f local{mouse_.x - originX(), mouse_.y - originY()};
        const int e = hoverX_ >= 0 ? elevationAt(hoverX_, hoverY_) : 0;
        const Vec2f world = grid_.toWorld(local, float(e));
        char t[128];
        std::snprintf(t, sizeof t, "screen   %7.1f , %7.1f", local.x, local.y);
        r.drawText(t, 10, y, 1.f, Color{0.62f, 0.68f, 0.80f, 1.f}); y += 9;
        std::snprintf(t, sizeof t, "world    %7.2f , %7.2f", world.x, world.y);
        r.drawText(t, 10, y, 1.f, Color{0.62f, 0.68f, 0.80f, 1.f}); y += 9;
        if (hoverX_ >= 0) std::snprintf(t, sizeof t, "tile     %7d , %7d    elev %d", hoverX_, hoverY_, e);
        else              std::snprintf(t, sizeof t, "tile        (off the map)");
        r.drawText(t, 10, y, 1.f, Color{1.f, 0.95f, 0.4f, 1.f}); y += 13;

        const float px = W_ * 0.44f;
        float py = y0 + 11;
        r.drawText("screen = (wx-wy)*tileW/2 , (wx+wy)*tileH/2 - elev*step", px, py, 1.f,
                   Color{0.55f, 0.62f, 0.75f, 1.f}); py += 9;
        r.drawText("depth  = wx + wy      (elevation deliberately excluded)", px, py, 1.f,
                   Color{0.55f, 0.62f, 0.75f, 1.f}); py += 9;
        r.drawText("the building sorts by its far corner, so a unit in front of", px, py, 1.f,
                   Color{0.42f, 0.48f, 0.60f, 1.f}); py += 8;
        r.drawText("it draws over it - watch the two walkers circle through.", px, py, 1.f,
                   Color{0.42f, 0.48f, 0.60f, 1.f});

        if (showDepth_ && hoverX_ >= 0) {
            std::snprintf(t, sizeof t, "depth key %.2f", IsoGrid::depth(float(hoverX_), float(hoverY_)));
            r.drawText(t, 10, y, 1.f, Color{0.45f, 0.90f, 0.60f, 1.f});
        }
    }

    otacon::IRenderer* r_ = nullptr;
    IsoGrid grid_;
    otacon::TextureHandle tile_[kTerrainKinds]{}, unit_ = 0, building_ = 0;
    std::vector<std::uint8_t> height_, kind_;
    std::vector<Drawable> draw_;
    Vec2f unitPos_[kUnits]{}, mouse_{320, 200};
    float W_ = 640, H_ = 400, t_ = 0, camX_ = 0, camY_ = -40, lastNx_ = 0, lastNy_ = 0;
    int   hoverX_ = 0, hoverY_ = 0, buildTilesX_ = 3, buildTilesY_ = 3;
    std::uint32_t seed_ = 11;
    bool  showGrid_ = true, showDepth_ = false, flat_ = false, dragging_ = false;
    mutable char buf_[160]{};
};

} // namespace

Sample* makeIsoGrid() { return new IsoGridSample(); }

} // namespace samples
