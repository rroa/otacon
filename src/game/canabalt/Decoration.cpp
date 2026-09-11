#include "canabalt/Decoration.hpp"
#include "asset/Resources.hpp"
#include "core/math/Random.hpp"
#include "asset/Image.hpp"
#include <string>

using namespace otacon;

namespace canabalt {

namespace {
constexpr float kDec = 20.f;   // decoration grid step (decSize)
constexpr float kAh  = 160.f;  // antenna height above the roof
constexpr float kT   = 16.f;
}

void Decoration::load(Resources* res, const char* assetDir) {
    if (!res || ac_) return;
    auto tex = [&](const char* file, bool repeat) -> TextureHandle {
        return res ? res->texture(std::string(assetDir) + "/images/raw/" + file, repeat) : 0;
    };
    ac_       = tex("ac-trimmed.png", false);
    pipe1L_   = tex("pipe1-left.png", false);  pipe1R_ = tex("pipe1-right.png", false);
    pipe2L_   = tex("pipe2-left.png", false);  pipe2M_ = tex("pipe2-middle.png", false); pipe2R_ = tex("pipe2-right.png", false);
    antL_     = tex("antenna-left.png", false); antR_ = tex("antenna-right.png", false);
    ant2_     = tex("antenna2-trimmed.png", false); ant3_ = tex("antenna3-trimmed.png", false);
    ant4_     = tex("antenna4-trimmed.png", false); ant5_ = tex("antenna5-trimmed.png", false);
    ant6_     = tex("antenna6-trimmed.png", false); dishes_ = tex("dishes-trimmed.png", false);
    skylight_ = tex("skylight.png", false);    access_ = tex("access.png", false);
    reservoir_ = tex("reservoir-trimmed.png", false);
    fence_    = tex("fence-trimmed.png", true);
}

void Decoration::destroy(IRenderer* r) {
    // Nothing to free: these textures are owned by the engine's Resources
    // cache, which releases them once, after the game shuts down and while
    // the renderer is still alive. Freeing them here too would double-free.
    (void)r;
}

void Decoration::draw(IRenderer& r, const Camera& cam, float wx, float wy, float ww,
                      std::uint32_t seed) const {
    if (!ac_) return;
    Vec2f o = cam.screenPoint({R(wx), R(wy)}, {1, 1});   // roof top-left in screen space
    const float ox = o.x, oy = o.y, sw = ww;
    std::uint32_t st = seed;
    otacon::Random prng(st);
    auto rnd = [&]() { return prng.unit(); };   // engine generator, seeded per piece
    auto img = [&](TextureHandle t, float x, float y, float w, float h) { if (t) r.drawImage(t, x, y, w, h); };

    // AC units along the parapet.
    for (int i = 0, n = int(sw / 40); i < n; ++i)
        if (rnd() < 0.30f) img(ac_, ox + kDec + 40 * i, oy - kDec, 19, 20);

    if (rnd() < 0.5f) {
        // Pipe runs (two styles) + tall antennas.
        for (int i = 0, n = int((sw - 100) / 120); i < n; ++i)
            if (rnd() < 0.35f) {
                float x = ox + kDec + 120 * i;
                img(pipe1L_, x, oy - kDec, 13, 20);
                img(pipe1R_, x + 100 - 12, oy - kDec, 12, 20);
                r.fillRect(x + 13, oy - kDec, 100 - 12 - 13, 8, Color::rgb(0x4d4d59));
            }
        for (int i = 0, n = int((sw - 40) / 80); i < n; ++i)
            if (rnd() < 0.35f) {
                float x = ox + kDec + 80 * i + 2;
                img(pipe2L_, x, oy - kDec * 2 + 5, 15, 35);
                img(pipe2M_, x + 15, oy - kDec * 2 + 4, 10, 11);
                img(pipe2R_, x + 25, oy - kDec * 2 + 5, 12, 35);
            }
        if (rnd() < 0.5f)
            for (int i = 0, n = int((sw - 32) / 40); i < n; ++i)
                if (rnd() < 0.30f) {
                    float x = ox + kDec + 40 * i;
                    switch (int(rnd() * 7) % 7) {
                        case 0: img(antL_, x, oy - kAh, 18, 160); img(antR_, x + 18, oy - kAh + 160 - 19, 22, 19); break;
                        case 1: img(ant2_, x + 3, oy - kAh, 17, 160); break;
                        case 2: img(ant3_, x, oy - kAh, 19, 160); break;
                        case 3: img(ant4_, x, oy - kAh + 40, 31, 120); break;
                        case 4: img(ant5_, x + (40 - 16) / 2.f, oy - kAh + 40, 16, 120); break;
                        case 5: img(ant6_, x + 6, oy - kAh + 4, 29, 156); break;
                        case 6: img(dishes_, x + 1, oy - kAh, 39, 160); break;
                    }
                }
    } else {
        // Skylights, roof access + reservoirs.
        for (int i = 0, n = int(sw / 140); i < n; ++i)
            if (rnd() < 0.5f) img(skylight_, ox + kDec + 140 * i, oy - kDec + 1, 80, 20);
        for (int i = 0, n = int(sw / 200); i < n; ++i)
            if (rnd() < 0.25f) img(access_, ox + kDec + 200 * i, oy - 30, 60, 30);
        for (int i = 0, n = int(sw / 200); i < n; ++i)
            if (rnd() < 0.5f) img(reservoir_, ox + kDec + 200 * i + 2, oy - kDec * 6, 76, 120);
    }

    // Chain-link fence along the roof edge.
    if (rnd() < 0.4f) {
        int n = int(sw / (kT * 2)) - 2;
        if (n > 0) r.drawImage(fence_, ox + kT * 2, oy - 19, n * kT * 2, 19, 0, 0, float(n), 1.f);
    }
}

} // namespace canabalt
