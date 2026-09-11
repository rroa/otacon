// Pipe.hpp — one pipe pair in the scrolling field.
//
// A pair is fully described by its left edge X and the top of its gap: the top
// pipe is everything above the gap, the bottom pipe everything below it. Width
// and gap height are shared constants (cfg). Pipes only move in X.
#pragma once

namespace flappy {

struct Pipe {
    float x      = 0;       // left edge, logical px
    float gapY   = 0;       // top of the gap (bottom pipe starts at gapY + kPipeGap)
    bool  passed = false;   // already counted for score (Build 8)
};

} // namespace flappy
