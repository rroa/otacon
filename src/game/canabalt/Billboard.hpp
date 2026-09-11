// Billboard.hpp — renders a billboard sign for a BILLBOARD sequence.
//
// A billboard is a big framed sign standing above the roof: a solid panel with
// tiled top/bottom/side edges and corner pieces, a catwalk along the bottom that
// is the thin ledge the player runs across, and a post holding it up from the
// roof. The sign height comes from the generator (Piece.aux).
#pragma once
#include "scene/Camera.hpp"
#include "render/IRenderer.hpp"
#include <cstdint>

namespace canabalt {

class Billboard {
public:
    void load(otacon::IRenderer* r, const char* assetDir);
    void destroy(otacon::IRenderer* r);
    bool loaded() const { return topMid_ != 0; }
    void draw(otacon::IRenderer& r, const otacon::Camera& cam,
              float wx, float wy, float ww, float wh, float signH, std::uint32_t seed) const;

private:
    otacon::TextureHandle topL_ = 0, topMid_ = 0, topR_ = 0;
    otacon::TextureHandle midL_ = 0, midR_ = 0;
    otacon::TextureHandle botL_ = 0, botMid_ = 0, botR_ = 0;
    otacon::TextureHandle catL_ = 0, catMid_ = 0, catR_ = 0;
    otacon::TextureHandle postTop_ = 0, dmg_[3]{};
};

} // namespace canabalt
