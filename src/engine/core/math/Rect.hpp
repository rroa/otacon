/*
===========================================================================

OTACON ENGINE
core/math/Rect.hpp - axis-aligned rectangle

2D collision system (mirrors the CGRect hulls used by flixel).

===========================================================================
*/
#pragma once
#include "core/math/Vector.hpp"

namespace otacon {

struct Rect {
    Real x{}, y{}, w{}, h{};
    constexpr Rect() = default;
    constexpr Rect(Real x_, Real y_, Real w_, Real h_) : x(x_), y(y_), w(w_), h(h_) {}

    Real right()  const { return x + w; }
    Real bottom() const { return y + h; }

    bool overlaps(const Rect& o) const {
        return !(right() < o.x || x > o.right() || bottom() < o.y || y > o.bottom());
    }
    bool contains(Real px, Real py) const {
        return px >= x && px <= right() && py >= y && py <= bottom();
    }
};

} // namespace otacon
