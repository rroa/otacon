#include "canabalt/Crane.hpp"
#include "asset/Resources.hpp"
#include "core/math/Random.hpp"
#include "asset/Image.hpp"
#include <string>

using namespace otacon;

namespace canabalt {

namespace {
constexpr float kTile = 16.f;
constexpr float kAntH = 160.f;        // antenna offset above the beam
}

void Crane::load(Resources* res, const char* assetDir) {
    if (!res || beam_) return;
    auto tex = [&](const char* file, bool repeat) -> TextureHandle {
        return res ? res->texture(std::string(assetDir) + "/images/raw/" + file, repeat) : 0;
    };
    beam_          = tex("crane1.png", true);          // 96x32 girder, tiled across
    post_          = tex("crane2-filled.png", true);   // 32x32 tower, tiled down
    counterweight_ = tex("crane3.png", false);         // 64x48
    cabin_         = tex("crane4.png", false);         // 48x48
    pulley_        = tex("crane5.png", false);         // 32x64
    antenna_       = tex("antenna5-trimmed.png", false); // 16x120
}

void Crane::destroy(IRenderer* r) {
    // Nothing to free: these textures are owned by the engine's Resources
    // cache, which releases them once, after the game shuts down and while
    // the renderer is still alive. Freeing them here too would double-free.
    (void)r;
}

void Crane::draw(IRenderer& r, const Camera& cam, float wx, float wy, float ww, float wh,
                 std::uint32_t seed) const {
    if (!beam_) return;
    Vec2f s = cam.screenPoint({R(wx), R(wy)}, {1, 1});
    const float x = s.x, y = s.y, w = ww, h = wh;
    const float T = kTile;
    std::uint32_t st = seed;
    otacon::Random prng(st);
    auto rnd = [&]() { return prng.unit(); };   // engine generator, seeded per piece
    const bool left = rnd() < 0.5f;
    const float cx = w * 0.35f;
    const float pr = rnd();

    // Tower (post) drops from the beam to the roof below.
    const float postX = left ? x + cx : x + w - cx - 2 * T;
    r.drawImage(post_, postX, y + 2 * T, 2 * T, h - 2 * T, 0, 0, 1.f, (h - 2 * T) / 32.f);
    // Beam (the girder the player runs across), tiled along its length.
    r.drawImage(beam_, x, y, w, 2 * T, 0, 0, w / 96.f, 1.f);
    // Counterweight + cabin at the tower (cabin faces toward the long arm).
    r.drawImage(counterweight_, left ? x + 8 : x + w - 72, y + 4, 64, 48);
    const float cabX = left ? x + cx - 8 : x + w - cx - 40;
    if (left) r.drawImage(cabin_, cabX, y - 9, 48, 48, 1, 0, 0, 1);   // facing flipped
    else      r.drawImage(cabin_, cabX, y - 9, 48, 48, 0, 0, 1, 1);
    // Hanging pulley somewhere along the arm.
    const float pulX = left ? x + cx + 64 + pr * (w - cx - 128) : x + pr * (w - cx - 128);
    r.drawImage(pulley_, pulX, y + 20, 32, 64);
    // Three antennas standing on the beam.
    const float aY = y - kAntH + 40;
    r.drawImage(antenna_, x - 8 + (40 - 16) / 2.f, aY, 16, 120);
    r.drawImage(antenna_, left ? x + cx - 8 + (40 - 16) / 2.f : x + w - cx - 24 + (40 - 16) / 2.f, aY, 16, 120);
    r.drawImage(antenna_, x + w - 24 + (40 - 16) / 2.f, aY, 16, 120);
}

} // namespace canabalt
