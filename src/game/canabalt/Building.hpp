// Building.hpp — renders a building facade for a collision block.
//
// A building is not a flat wall: it has a roof cap (or a floor cap for hallways)
// along the running surface, then alternating rows of plain wall and windows
// down the face, with wall edges on the left and right. There are 4 wall styles
// and 4 window styles, so buildings vary between plain, brick, riveted, etc.
// Tiles are 16px tall (window tiles are 64px wide) and repeat-wrapped, so each
// horizontal strip and each edge is a single textured draw. The tile textures
// are loaded once and shared by every building.
#pragma once
#include "asset/Resources.hpp"
#include "scene/Entity.hpp"
#include "scene/Camera.hpp"
#include "render/IRenderer.hpp"
#include <cstdint>

namespace canabalt {

class Building {
public:
    void load(otacon::Resources* res, const char* assetDir);
    void destroy(otacon::IRenderer* r);
    bool loaded() const { return wallMid_[0] != 0; }

    // Draw the facade for the world rect (wx,wy,ww,wh). `hallway` swaps the roof
    // cap for a floor cap; `ceiling` (the hallway's top slab) is just tiled wall;
    // `escape` hangs a fire escape (emergency staircase) off the right edge.
    // `roofType`/`floorType` pick the cap style (6 roofs, 2 floors) — like the
    // original, each building rolls its own so the skyline isn't one repeated cap.
    void draw(otacon::IRenderer& r, const otacon::Camera& cam,
              float wx, float wy, float ww, float wh, int wallType, int windowType,
              bool hallway, bool ceiling, bool escape, int roofType = 0, int floorType = 0) const;

    // Draw a hallway/tunnel: the building above the opening (walls+windows), the
    // dark interior you run through, and the floor strips. (wx,wy) is the floor
    // top-left; hallHeight is the clear opening height. `seed` deterministically
    // scatters doors along the back wall (skipped when `doors` is false, e.g. the
    // opening tunnel).
    void drawHall(otacon::IRenderer& r, const otacon::Camera& cam,
                  float wx, float wy, float ww, float hallHeight, int wallType, int windowType,
                  std::uint32_t seed = 0, bool doors = false) const;

    static constexpr int kRoofStyles = 6, kFloorStyles = 2;

private:
    otacon::TextureHandle wallL_[4]{}, wallR_[4]{}, wallMid_[4]{}, window_[4]{};
    otacon::TextureHandle roofL_[kRoofStyles]{},  roofM_[kRoofStyles]{},  roofR_[kRoofStyles]{};
    otacon::TextureHandle floorL_[kFloorStyles]{}, floorM_[kFloorStyles]{}, floorR_[kFloorStyles]{};
    otacon::TextureHandle escape_ = 0;        // fire escape, tiled vertically (16x32)
    otacon::TextureHandle hall1_ = 0, hall2_ = 0;   // hallway floor strips (48x16)
    otacon::TextureHandle doors_ = 0;               // 4 door frames (15x24) for tunnels
};

} // namespace canabalt
