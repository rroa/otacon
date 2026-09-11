#include "canabalt/Building.hpp"
#include "asset/Image.hpp"
#include <cstdio>
#include <string>

using namespace otacon;

namespace canabalt {

namespace {
constexpr float kTile    = 16.f;  // wall / roof tile size
constexpr float kWindowW = 64.f;  // window tile width
constexpr float kEscapeH = 32.f;  // fire-escape tile height (16x32, tiled down)
}

void Building::load(IRenderer* r, const char* assetDir) {
    if (!r || wallMid_[0]) return;
    auto tex = [&](const char* file) -> TextureHandle {
        Image im = loadPng((std::string(assetDir) + "/images/raw/" + file).c_str());
        return im.valid() ? r->createTexture(im, /*repeat=*/true) : 0;   // repeat to tile
    };
    char name[48];
    for (int i = 0; i < 4; ++i) {
        std::snprintf(name, sizeof name, "wall%d-left.png", i + 1);   wallL_[i]   = tex(name);
        std::snprintf(name, sizeof name, "wall%d-middle.png", i + 1); wallMid_[i] = tex(name);
        std::snprintf(name, sizeof name, "wall%d-right.png", i + 1);  wallR_[i]   = tex(name);
        std::snprintf(name, sizeof name, "window%d.png", i + 1);      window_[i]  = tex(name);
    }
    for (int i = 0; i < kRoofStyles; ++i) {
        std::snprintf(name, sizeof name, "roof%d-left.png", i + 1);   roofL_[i] = tex(name);
        std::snprintf(name, sizeof name, "roof%d-middle.png", i + 1); roofM_[i] = tex(name);
        std::snprintf(name, sizeof name, "roof%d-right.png", i + 1);  roofR_[i] = tex(name);
    }
    for (int i = 0; i < kFloorStyles; ++i) {
        std::snprintf(name, sizeof name, "floor%d-left.png", i + 1);   floorL_[i] = tex(name);
        std::snprintf(name, sizeof name, "floor%d-middle.png", i + 1); floorM_[i] = tex(name);
        std::snprintf(name, sizeof name, "floor%d-right.png", i + 1);  floorR_[i] = tex(name);
    }
    escape_ = tex("escape-trimmed-filled.png");
    hall1_  = tex("hall1.png");
    hall2_  = tex("hall2.png");
    doors_  = tex("doors.png");            // 4 frames of 15x24
}

void Building::destroy(IRenderer* r) {
    if (!r) return;
    for (int i = 0; i < 4; ++i)
        for (TextureHandle t : {wallL_[i], wallR_[i], wallMid_[i], window_[i]}) if (t) r->destroyTexture(t);
    for (int i = 0; i < kRoofStyles; ++i)
        for (TextureHandle t : {roofL_[i], roofM_[i], roofR_[i]}) if (t) r->destroyTexture(t);
    for (int i = 0; i < kFloorStyles; ++i)
        for (TextureHandle t : {floorL_[i], floorM_[i], floorR_[i]}) if (t) r->destroyTexture(t);
    for (TextureHandle t : {escape_, hall1_, hall2_, doors_})
        if (t) r->destroyTexture(t);
}

// A hallway: building above the opening + dark interior + floor strips. The
// player runs through the opening (yCeil..yFloor) and the building above hangs
// down as the ceiling.
void Building::drawHall(IRenderer& r, const Camera& cam, float wx, float wy, float ww,
                        float hallHeight, int wallType, int windowType,
                        std::uint32_t seed, bool doors) const {
    if (ww < 2 * kTile || hallHeight < kTile) return;
    Vec2f s = cam.screenPoint({R(wx), R(wy)}, {1, 1});
    const float x = s.x, yFloor = s.y, w = ww;
    const int wt = wallType & 3, wn = windowType & 3;
    const float yCeil = yFloor - hallHeight;
    const float aboveTop = yFloor - wy - 128.f;           // world y = -128
    const float aboveH = yCeil - aboveTop;
    auto strip = [&](TextureHandle t, float px, float py, float pw, float ph, float tileW) {
        if (t) r.drawImage(t, px, py, pw, ph, 0, 0, pw / tileW, ph / kTile);
    };
    auto column = [&](TextureHandle t, float px, float py, float pw, float ph) {
        if (t) r.drawImage(t, px, py, pw, ph, 0, 0, pw / kTile, ph / kTile);
    };
    // Building above the opening: side edges + alternating wall/window rows
    // climbing up from the ceiling.
    if (aboveH > kTile) {
        column(wallL_[wt], x, aboveTop, kTile, aboveH);
        column(wallR_[wt], x + w - kTile, aboveTop, kTile, aboveH);
        const int rows = int(aboveH / kTile / 2);
        for (int i = 0; i <= rows; ++i)
            strip(wallMid_[wt], x + kTile, yCeil - (1 + i * 2) * kTile, w - 2 * kTile, kTile, kTile);
        for (int i = 0; i < rows; ++i)
            strip(window_[wn], x + kTile, yCeil - (2 + i * 2) * kTile, w - 2 * kTile, kTile, kWindowW);
    }
    // Dark tunnel interior.
    r.fillRect(x, yCeil, w, hallHeight, Color::rgb(0x35353d));
    // Doors scattered along the back wall, standing on the floor (Hall.m). Every
    // fourth tile is a candidate; ~65% get a door, each a random one of 4 frames.
    if (doors && doors_) {
        const int slots = int(w / kTile - 3.f) / 4;
        std::uint32_t rng = seed ? seed : 1u;
        auto next = [&] { rng = rng * 1664525u + 1013904223u; return float(rng >> 8) / float(1u << 24); };
        for (int i = 1; i < slots; ++i) {
            if (next() > 0.65f) continue;
            const int frame = int(next() * 4.f) & 3;
            const float dx = x + float(i) * kTile * 4.f - kTile;
            const float dy = yFloor - 2.f * kTile - 24.f;   // stand on the floor band's top
            const float u0 = float(frame) * 15.f / 60.f;
            r.drawImage(doors_, dx, dy, 15.f, 24.f, u0, 0.f, u0 + 15.f / 60.f, 1.f);
        }
    }
    // Floor strips (hall2 under hall1, the running surface), tiled across.
    if (hall2_) r.drawImage(hall2_, x, yFloor - 2 * kTile, w, kTile, 0, 0, w / 48.f, 1.f);
    if (hall1_) r.drawImage(hall1_, x, yFloor - kTile, w, kTile, 0, 0, w / 48.f, 1.f);
}

void Building::draw(IRenderer& r, const Camera& cam, float wx, float wy, float ww, float wh,
                    int wallType, int windowType, bool hallway, bool ceiling, bool escape,
                    int roofType, int floorType) const {
    Vec2f s = cam.screenPoint({R(wx), R(wy)}, {1, 1});
    const float x = s.x, y = s.y;
    const float w = ww, h = wh;
    if (w < 2 * kTile || h < kTile) return;
    const int wt = wallType & 3, wn = windowType & 3;

    // A horizontal strip: one quad whose UVs tile the texture across its width.
    auto strip = [&](TextureHandle t, float px, float py, float pw, float ph, float tileW) {
        if (t) r.drawImage(t, px, py, pw, ph, 0, 0, pw / tileW, ph / kTile);
    };
    // A vertical edge column.
    auto column = [&](TextureHandle t, float px, float py, float pw, float ph) {
        if (t) r.drawImage(t, px, py, pw, ph, 0, 0, pw / kTile, ph / kTile);
    };

    // The hallway ceiling is just a slab of wall.
    if (ceiling) { strip(wallMid_[wt], x, y, w, h, kTile); return; }

    // Cap along the running surface: corners + a tiled middle (roof or floor),
    // in the building's own randomly-chosen style.
    const int rt = ((roofType % kRoofStyles) + kRoofStyles) % kRoofStyles;
    const int ft = ((floorType % kFloorStyles) + kFloorStyles) % kFloorStyles;
    TextureHandle capL = hallway ? floorL_[ft] : roofL_[rt];
    TextureHandle capM = hallway ? floorM_[ft] : roofM_[rt];
    TextureHandle capR = hallway ? floorR_[ft] : roofR_[rt];
    if (capL) r.drawImage(capL, x, y, kTile, kTile);
    strip(capM, x + kTile, y, w - 2 * kTile, kTile, kTile);
    if (capR) r.drawImage(capR, x + w - kTile, y, kTile, kTile);

    // Left/right wall edges run the full height below the cap.
    column(wallL_[wt], x, y + kTile, kTile, h - kTile);
    column(wallR_[wt], x + w - kTile, y + kTile, kTile, h - kTile);

    // Face: alternating wall rows (odd rows) and window rows (even rows).
    const float fx = x + kTile, fw = w - 2 * kTile;
    const int rows = int((h / kTile - 1) / 2);
    for (int i = 0; i <= rows; ++i)
        strip(wallMid_[wt], fx, y + (1 + i * 2) * kTile, fw, kTile, kTile);
    for (int i = 0; i < rows; ++i)
        strip(window_[wn], fx, y + (2 + i * 2) * kTile, fw, kTile, kWindowW);

    // Fire escape: a 16-wide strip that hangs off the right edge (in the alley,
    // not over the windows), tiled down the full height — like the original's
    // RepeatBlock at x = building right edge.
    if (escape && escape_)
        r.drawImage(escape_, x + w, y + kTile, kTile, h,
                    0, 0, 1.f, h / kEscapeH);
}

} // namespace canabalt
