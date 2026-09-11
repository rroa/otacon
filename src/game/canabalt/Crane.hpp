// Crane.hpp — renders a construction crane for a CRANE sequence.
//
// The crane's horizontal beam is the thin ledge the player runs across (the
// collision block), with a tower (post) dropping to the roof below, a cabin +
// counterweight at the tower, a hanging pulley, and three antennas. The crane
// faces left or right at random; that choice (and the pulley position) come from
// the per-building seed so the crane is stable frame to frame.
#pragma once
#include "asset/Resources.hpp"
#include "scene/Camera.hpp"
#include "render/IRenderer.hpp"
#include <cstdint>

namespace canabalt {

class Crane {
public:
    void load(otacon::Resources* res, const char* assetDir);
    void destroy(otacon::IRenderer* r);
    bool loaded() const { return beam_ != 0; }
    void draw(otacon::IRenderer& r, const otacon::Camera& cam,
              float wx, float wy, float ww, float wh, std::uint32_t seed) const;

private:
    otacon::TextureHandle beam_ = 0, post_ = 0, counterweight_ = 0,
                          cabin_ = 0, pulley_ = 0, antenna_ = 0;
};

} // namespace canabalt
