// Decoration.hpp — scatters rooftop props (AC units, pipes, antennas, dishes,
// skylights, roof access, reservoirs, chain-link fences) across a flat roof.
//
// This is a port of the original generator's decorateSeq: it walks the roof in
// strips and rolls each prop in. To stay deterministic (and stable frame to
// frame) it drives every roll from the building's decor seed instead of a live
// RNG, so the same building always grows the same clutter.
#pragma once
#include "asset/Resources.hpp"
#include "scene/Camera.hpp"
#include "render/IRenderer.hpp"
#include <cstdint>

namespace canabalt {

class Decoration {
public:
    void load(otacon::Resources* res, const char* assetDir);
    void destroy(otacon::IRenderer* r);
    bool loaded() const { return ac_ != 0; }
    // Decorate the roof of the building rect (wx,wy = top-left, ww = width).
    void draw(otacon::IRenderer& r, const otacon::Camera& cam,
              float wx, float wy, float ww, std::uint32_t seed) const;

private:
    otacon::TextureHandle ac_ = 0, pipe1L_ = 0, pipe1R_ = 0,
                          pipe2L_ = 0, pipe2M_ = 0, pipe2R_ = 0,
                          antL_ = 0, antR_ = 0, ant2_ = 0, ant3_ = 0,
                          ant4_ = 0, ant5_ = 0, ant6_ = 0, dishes_ = 0,
                          skylight_ = 0, access_ = 0, reservoir_ = 0, fence_ = 0;
};

} // namespace canabalt
