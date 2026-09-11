#include "canabalt/Billboard.hpp"
#include "core/math/Random.hpp"
#include "asset/Image.hpp"
#include <cstdio>
#include <string>

using namespace otacon;

namespace canabalt {

namespace {
constexpr float kTile = 16.f;
}

void Billboard::load(IRenderer* r, const char* assetDir) {
    if (!r || topMid_) return;
    auto tex = [&](const char* file, bool repeat) -> TextureHandle {
        Image im = loadPng((std::string(assetDir) + "/images/raw/" + file).c_str());
        return im.valid() ? r->createTexture(im, repeat) : 0;
    };
    topL_   = tex("billboard_top-left.png", false);   topMid_ = tex("billboard_top-middle.png", true);   topR_ = tex("billboard_top-right.png", false);
    midL_   = tex("billboard_middle-left.png", true); midR_   = tex("billboard_middle-right.png", true);
    botL_   = tex("billboard_bottom-left.png", false); botMid_ = tex("billboard_bottom-middle.png", true); botR_ = tex("billboard_bottom-right.png", false);
    catL_   = tex("billboard_catwalk-left.png", false); catMid_ = tex("billboard_catwalk-middle.png", true); catR_ = tex("billboard_catwalk-right.png", false);
    postTop_ = tex("billboard_post2.png", false);
    dmg_[0] = tex("billboard_dmg1-filled.png", false);
    dmg_[1] = tex("billboard_dmg2-filled.png", false);
    dmg_[2] = tex("billboard_dmg3-filled.png", false);
}

void Billboard::destroy(IRenderer* r) {
    if (!r) return;
    for (TextureHandle t : {topL_, topMid_, topR_, midL_, midR_, botL_, botMid_, botR_,
                            catL_, catMid_, catR_, postTop_, dmg_[0], dmg_[1], dmg_[2]})
        if (t) r->destroyTexture(t);
}

void Billboard::draw(IRenderer& r, const Camera& cam, float wx, float wy, float ww, float wh,
                     float signH, std::uint32_t seed) const {
    if (!topMid_) return;
    const float T = kTile;
    if (signH < 4 * T || ww < 4 * T) return;
    Vec2f s = cam.screenPoint({R(wx), R(wy)}, {1, 1});
    const float x = s.x, y = s.y, w = ww, h = wh;
    const float top = y - signH;                 // top of the sign panel

    // Support post (solid) + its cap, holding the sign above the roof.
    r.fillRect(x + w / 2 - T + 3, y, 2 * T - 6, h, Color::rgb(0x4d4d59));
    if (postTop_) r.drawImage(postTop_, x + w / 2 - T, y + 12, 32, 16);
    // Sign panel fill.
    r.fillRect(x + 2 * T - 1, top + 2 * T, w - 4 * T + 2, signH - 4 * T, Color::rgb(0x868696));
    // Tiled edges.
    r.drawImage(topMid_, x + 2 * T, top, w - 4 * T, 2 * T, 0, 0, (w - 4 * T) / 32.f, 1.f);
    r.drawImage(botMid_, x + 2 * T, y - 2 * T, w - 4 * T, 2 * T, 0, 0, (w - 4 * T) / 32.f, 1.f);
    r.drawImage(midL_, x + 1, top + 2 * T, 2 * T, signH - 4 * T, 0, 0, 1.f, (signH - 4 * T) / 32.f);
    r.drawImage(midR_, x + w - 2 * T - 1, top + 2 * T, 2 * T, signH - 4 * T, 0, 0, 1.f, (signH - 4 * T) / 32.f);
    // Corners.
    r.drawImage(topL_, x + 1, top, 31, 32);
    r.drawImage(topR_, x + w - 2 * T, top, 31, 32);
    r.drawImage(botL_, x + 1, y - 2 * T, 31, 32);
    r.drawImage(botR_, x + w - 2 * T, y - 2 * T, 31, 32);
    // One random damage decal in the panel.
    std::uint32_t st = seed;
    otacon::Random prng(st);
    auto rnd = [&]() { return prng.unit(); };   // engine generator, seeded per piece
    if (rnd() < 0.5f) {
        int d = int(rnd() * 3) % 3;
        if (dmg_[d]) r.drawImage(dmg_[d], x + 2 * T + rnd() * (w - 4 * T - 64),
                                 top + 2 * T + rnd() * (signH - 4 * T - 64), 64, 64);
    }
    // Catwalk along the bottom (the ledge the player runs on).
    r.drawImage(catMid_, x + T, y, w - 2 * T, 13, 0, 0, (w - 2 * T) / 16.f, 1.f);
    r.drawImage(catL_, x, y, 16, 16);
    r.drawImage(catR_, x + w - T, y, 16, 16);
}

} // namespace canabalt
