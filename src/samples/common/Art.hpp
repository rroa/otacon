// Art.hpp — every texture the samples use, generated in code.
//
// No sample loads a file. Two reasons, both deliberate:
//   1. A sample that borrowed Canabalt's sprites would make the *engine's*
//      documentation depend on a *game's* assets — exactly the direction the
//      project forbids.
//   2. Generated art is reproducible, so a sample's frame is identical on every
//      machine, which is what lets the cross-backend pixel diff mean anything.
//
// Two techniques are on show here. Character art is written as ASCII pixel art
// with a palette (readable, editable, and how a lot of real pixel art starts
// life), while surfaces — tiles, bricks, the particle dot — are computed from
// noise and distance fields, because those are textural rather than drawn.
#pragma once
#include "asset/Image.hpp"

namespace samples::art {

// ---- ASCII pixel art -------------------------------------------------------
// One palette entry per character used by the art. '.' is always transparent.
struct Ink { char ch; std::uint32_t rgb; };

// Build an RGBA image from `h` rows of exactly `w` characters. Any character
// missing from the palette is treated as transparent.
otacon::Image fromAscii(const char* const* rows, int w, int h, const Ink* palette, int inkCount);

// ---- Generated sheets ------------------------------------------------------
// The samples' character: a horizontal strip of 12x16 frames, in order
//   0..3 run cycle, 4 idle, 5 jump/fall.
// Returns the strip; frame size and count come back through the out-params.
otacon::Image heroSheet(int& frameW, int& frameH, int& frameCount);

// A 16x16 tile strip: 0 grass, 1 dirt, 2 stone, 3 water, 4 sand, 5 brick.
otacon::Image tileset(int& tileSize, int& tileCount);

// A soft radial dot, used wherever particles want to be round rather than square.
otacon::Image dot(int size);

// A brick wall's albedo plus the matching tangent-space normal map. The normal
// map is *derived* from the height field the bricks are drawn into (central
// differences), which is how normal maps are actually produced — and seeing the
// two come from one source is the point of the lighting sample.
void brickSurface(int w, int h, otacon::Image& albedo, otacon::Image& normal);

} // namespace samples::art
