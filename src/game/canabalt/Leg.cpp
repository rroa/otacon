#include "canabalt/Leg.hpp"
#include "asset/Resources.hpp"
#include "asset/Image.hpp"
#include <string>

using namespace otacon;

namespace canabalt {

void Leg::load(Resources* res, const char* assetDir) {
    if (!res || bottom_) return;
    auto tex = [&](const char* file) -> TextureHandle {
        return res ? res->texture(std::string(assetDir) + "/images/raw/" + file) : 0;
    };
    top_    = tex("giant_leg_top.png");
    bottom_ = tex("giant_leg_bottom.png");
}

void Leg::destroy(IRenderer* r) {
    // Nothing to free: these textures are owned by the engine's Resources
    // cache, which releases them once, after the game shuts down and while
    // the renderer is still alive. Freeing them here too would double-free.
    (void)r;
}

void Leg::draw(IRenderer& r, const Camera& cam, float legX, float legY) const {
    if (!bottom_) return;
    Vec2f s = cam.screenPoint({R(legX), R(legY)}, {1, 1});
    // Upper leg hangs above the lower leg, lower leg/foot lands on the roof.
    if (top_) r.drawImage(top_, s.x, s.y - 420.f, 128, 512);
    r.drawImage(bottom_, s.x, s.y, 128, 512);
}

} // namespace canabalt
