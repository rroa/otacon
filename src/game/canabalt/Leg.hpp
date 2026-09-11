// Leg.hpp — renders the giant robot leg for a LEG sequence.
//
// The leg drops out of the sky (the generator animates its world-y) and stomps
// the roof. It is two pieces: the lower leg/foot (giant_leg_bottom) that lands on
// the building, and the upper leg (giant_leg_top) hanging above it. The drop and
// the stomp event are driven by the generator; this only draws the current pose.
#pragma once
#include "asset/Resources.hpp"
#include "scene/Camera.hpp"
#include "render/IRenderer.hpp"

namespace canabalt {

class Leg {
public:
    void load(otacon::Resources* res, const char* assetDir);
    void destroy(otacon::IRenderer* r);
    bool loaded() const { return bottom_ != 0; }
    // (legX,legY) is the world top-left of the lower leg (128x512).
    void draw(otacon::IRenderer& r, const otacon::Camera& cam, float legX, float legY) const;

private:
    otacon::TextureHandle top_ = 0, bottom_ = 0;
};

} // namespace canabalt
