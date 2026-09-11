#include "canabalt/Leg.hpp"
#include "asset/Image.hpp"
#include <string>

using namespace otacon;

namespace canabalt {

void Leg::load(IRenderer* r, const char* assetDir) {
    if (!r || bottom_) return;
    auto tex = [&](const char* file) -> TextureHandle {
        Image im = loadPng((std::string(assetDir) + "/images/raw/" + file).c_str());
        return im.valid() ? r->createTexture(im, false) : 0;
    };
    top_    = tex("giant_leg_top.png");
    bottom_ = tex("giant_leg_bottom.png");
}

void Leg::destroy(IRenderer* r) {
    if (!r) return;
    if (top_) r->destroyTexture(top_);
    if (bottom_) r->destroyTexture(bottom_);
}

void Leg::draw(IRenderer& r, const Camera& cam, float legX, float legY) const {
    if (!bottom_) return;
    Vec2f s = cam.screenPoint({R(legX), R(legY)}, {1, 1});
    // Upper leg hangs above the lower leg, lower leg/foot lands on the roof.
    if (top_) r.drawImage(top_, s.x, s.y - 420.f, 128, 512);
    r.drawImage(bottom_, s.x, s.y, 128, 512);
}

} // namespace canabalt
