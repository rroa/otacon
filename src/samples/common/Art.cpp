#include "common/Art.hpp"
#include "core/math/Noise.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

namespace samples::art {

using otacon::Image;

// ---------------------------------------------------------------------------
Image fromAscii(const char* const* rows, int w, int h, const Ink* palette, int inkCount) {
    Image img;
    img.width = w; img.height = h;
    img.rgba.assign(std::size_t(w) * h * 4, 0);
    for (int y = 0; y < h; ++y) {
        const char* row = rows[y];
        for (int x = 0; x < w; ++x) {
            const char c = row[x];
            if (c == '\0') break;            // short row -> rest stays transparent
            if (c == '.' || c == ' ') continue;
            for (int i = 0; i < inkCount; ++i) {
                if (palette[i].ch != c) continue;
                std::uint8_t* p = &img.rgba[(std::size_t(y) * w + x) * 4];
                p[0] = (palette[i].rgb >> 16) & 0xFF;
                p[1] = (palette[i].rgb >> 8) & 0xFF;
                p[2] = palette[i].rgb & 0xFF;
                p[3] = 255;
                break;
            }
        }
    }
    return img;
}

// ---------------------------------------------------------------------------
// The hero. Head and torso are shared by every frame; only the legs change.
// That split is the animation lesson in miniature: a run cycle is mostly one
// pose with a few rows swapped, which is why sprite sheets compress so well.
namespace {

constexpr int kHeroW = 12, kHeroH = 16, kHeroFrames = 6;
constexpr int kLegRow = 12;           // rows 0..11 shared, 12..15 per frame

const char* kHeroUpper[kLegRow] = {
    "............",
    "...oooo.....",
    "..ohhhhho...",
    "..ohhhhhho..",
    "..osssshho..",
    "..osesssho..",
    "..osssssho..",
    "...ooooo....",
    "..obbbbbo...",
    ".obbbbbbbo..",
    ".obbbbbbbo..",
    "..obbbbbo...",
};

// 4 run frames (contact, pass, contact-opposite, pass), then idle, then jump.
const char* kHeroLegs[kHeroFrames][kHeroH - kLegRow] = {
    { "..oppppo....", "..opo.opo...", ".ofo...ofo..", ".oo.....oo.." },   // 0 contact
    { "..oppppo....", "..oppppo....", "...opppo....", "...offo....." },   // 1 pass
    { "..oppppo....", "..opo.opo...", "..ofo..ofo..", "..oo....oo.." },   // 2 contact
    { "..oppppo....", "...opppo....", "...opppo....", "....offo...." },   // 3 pass
    { "..oppppo....", "..oppppo....", "..opo.opo...", ".off...ffo.." },   // 4 idle
    { "..oppppo....", "..opppppo...", ".ofo...oppo.", ".oo......oo." },   // 5 jump
};

const Ink kHeroInk[] = {
    {'o', 0x1a1a24},   // outline
    {'h', 0x8b4a2b},   // hair
    {'s', 0xf0c090},   // skin
    {'e', 0x1a1a24},   // eye
    {'b', 0x4a90d9},   // shirt
    {'p', 0x2d4a7a},   // trousers
    {'f', 0x3a2a1a},   // shoes
};

} // namespace

Image heroSheet(int& frameW, int& frameH, int& frameCount) {
    frameW = kHeroW; frameH = kHeroH; frameCount = kHeroFrames;

    Image strip;
    strip.width = kHeroW * kHeroFrames;
    strip.height = kHeroH;
    strip.rgba.assign(std::size_t(strip.width) * strip.height * 4, 0);

    for (int f = 0; f < kHeroFrames; ++f) {
        // Assemble this frame's 16 rows: shared upper body + its own legs.
        const char* rows[kHeroH];
        for (int y = 0; y < kLegRow; ++y) rows[y] = kHeroUpper[y];
        for (int y = kLegRow; y < kHeroH; ++y) rows[y] = kHeroLegs[f][y - kLegRow];

        Image frame = fromAscii(rows, kHeroW, kHeroH, kHeroInk,
                                int(sizeof kHeroInk / sizeof kHeroInk[0]));
        // Blit into the strip at column f.
        for (int y = 0; y < kHeroH; ++y) {
            std::memcpy(&strip.rgba[(std::size_t(y) * strip.width + f * kHeroW) * 4],
                        &frame.rgba[std::size_t(y) * kHeroW * 4],
                        std::size_t(kHeroW) * 4);
        }
    }
    return strip;
}

// ---------------------------------------------------------------------------
// Value noise: a lattice of random values, smoothly interpolated. Cheap, and
// enough to make a flat colour read as a surface.
namespace {

// The engine owns value noise now (core/math/Noise.hpp); this is the one call
// site's shorthand for it, kept so the surface texturing below reads unchanged.
float smoothNoise(float x, float y, std::uint32_t seed) {
    return otacon::noise::value(x, y, seed);
}

void putRGB(Image& img, int x, int y, float r, float g, float b, float a = 1.f) {
    auto q = [](float v) { return std::uint8_t(v <= 0 ? 0 : (v >= 1 ? 255 : int(v * 255 + 0.5f))); };
    std::uint8_t* p = &img.rgba[(std::size_t(y) * img.width + x) * 4];
    p[0] = q(r); p[1] = q(g); p[2] = q(b); p[3] = q(a);
}

} // namespace

Image tileset(int& tileSize, int& tileCount) {
    tileSize = 16; tileCount = 6;
    const int T = tileSize, N = tileCount;

    Image img;
    img.width = T * N; img.height = T;
    img.rgba.assign(std::size_t(img.width) * T * 4, 0);

    // base colour + how strongly noise modulates it, per tile
    struct TileDef { float r, g, b, amp; };
    const TileDef defs[6] = {
        {0.33f, 0.62f, 0.28f, 0.22f},   // 0 grass
        {0.50f, 0.36f, 0.22f, 0.18f},   // 1 dirt
        {0.46f, 0.47f, 0.52f, 0.20f},   // 2 stone
        {0.16f, 0.38f, 0.68f, 0.14f},   // 3 water
        {0.84f, 0.76f, 0.52f, 0.12f},   // 4 sand
        {0.55f, 0.24f, 0.22f, 0.10f},   // 5 brick
    };

    for (int t = 0; t < N; ++t) {
        const TileDef& d = defs[t];
        for (int y = 0; y < T; ++y) {
            for (int x = 0; x < T; ++x) {
                float n = smoothNoise(x * 0.45f, y * 0.45f, 1000u + t * 77u) - 0.5f;
                float shade = 1.f + n * 2.f * d.amp;
                // Brick gets mortar lines drawn over the noise, offset every
                // other course — the one tile that is structured, not textural.
                if (t == 5) {
                    int course = y / 5;
                    int xo = (x + (course % 2) * 8) % 16;
                    if (y % 5 == 0 || xo == 0) shade *= 0.62f;
                }
                // Water gets a horizontal ripple instead of a flat wash.
                if (t == 3) shade *= 1.f + 0.10f * std::sin(float(y) * 1.6f + n * 3.f);
                putRGB(img, t * T + x, y, d.r * shade, d.g * shade, d.b * shade);
            }
        }
    }
    return img;
}

Image dot(int size) {
    Image img;
    img.width = size; img.height = size;
    img.rgba.assign(std::size_t(size) * size * 4, 0);
    const float c = (size - 1) * 0.5f, inv = 1.f / (c + 0.0001f);
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            float dx = (x - c) * inv, dy = (y - c) * inv;
            float d = std::sqrt(dx * dx + dy * dy);
            // Smooth falloff to zero at the rim so particles have no hard edge.
            float a = d >= 1.f ? 0.f : (1.f - d) * (1.f - d);
            putRGB(img, x, y, 1.f, 1.f, 1.f, a);
        }
    }
    return img;
}

// ---------------------------------------------------------------------------
void brickSurface(int w, int h, Image& albedo, Image& normal) {
    albedo.width = w; albedo.height = h;
    albedo.rgba.assign(std::size_t(w) * h * 4, 0);
    normal.width = w; normal.height = h;
    normal.rgba.assign(std::size_t(w) * h * 4, 0);

    const int brickW = 32, brickH = 16, mortar = 2;

    // Step 1: build a height field. Bricks sit proud, mortar sits recessed, and
    // a little noise keeps the faces from looking like flat plastic.
    std::vector<float> height(std::size_t(w) * h, 0.f);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            int course = y / brickH;
            int bx = (x + (course % 2) * (brickW / 2)) % brickW;
            int by = y % brickH;
            bool isMortar = bx < mortar || by < mortar;
            float hgt = isMortar ? 0.f : 1.f;
            if (!isMortar) {
                // Bevel the brick edges so light catches them.
                float ex = float(std::min(bx - mortar, brickW - 1 - bx)) / 4.f;
                float ey = float(std::min(by - mortar, brickH - 1 - by)) / 3.f;
                hgt *= std::min(1.f, std::min(ex, ey));
                hgt = 0.35f + 0.65f * hgt;
                hgt += (smoothNoise(x * 0.8f, y * 0.8f, 7u) - 0.5f) * 0.15f;
            }
            height[std::size_t(y) * w + x] = hgt;
        }
    }

    // Step 2: albedo — colour the bricks and mortar, modulated by slow noise so
    // no two bricks are quite the same shade.
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            float hgt = height[std::size_t(y) * w + x];
            bool isMortar = hgt < 0.2f;
            float tone = smoothNoise(float(x / brickW) * 3.f, float(y / brickH) * 3.f, 21u);
            float r, g, b;
            if (isMortar) { r = g = b = 0.55f + 0.05f * tone; }
            else {
                r = 0.46f + 0.22f * tone;
                g = 0.20f + 0.10f * tone;
                b = 0.17f + 0.07f * tone;
            }
            float grain = 1.f + (smoothNoise(x * 1.7f, y * 1.7f, 33u) - 0.5f) * 0.18f;
            putRGB(albedo, x, y, r * grain, g * grain, b * grain);
        }
    }

    // Step 3: normals, derived from the height field by central differences.
    // The surface normal is the cross product of the two tangent vectors
    // (1,0,dh/dx) and (0,1,dh/dy), which works out to (-dh/dx, -dh/dy, 1)
    // normalized. Encoded into RGB the usual way: n*0.5+0.5, so a flat face
    // (0,0,1) becomes the familiar lilac (128,128,255).
    auto H = [&](int x, int y) {
        x = x < 0 ? 0 : (x >= w ? w - 1 : x);
        y = y < 0 ? 0 : (y >= h ? h - 1 : y);
        return height[std::size_t(y) * w + x];
    };
    const float strength = 2.4f;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            float dhdx = (H(x + 1, y) - H(x - 1, y)) * 0.5f * strength;
            float dhdy = (H(x, y + 1) - H(x, y - 1)) * 0.5f * strength;
            float nx = -dhdx, ny = -dhdy, nz = 1.f;
            float len = std::sqrt(nx * nx + ny * ny + nz * nz);
            nx /= len; ny /= len; nz /= len;
            putRGB(normal, x, y, nx * 0.5f + 0.5f, ny * 0.5f + 0.5f, nz * 0.5f + 0.5f);
        }
    }
}

} // namespace samples::art
