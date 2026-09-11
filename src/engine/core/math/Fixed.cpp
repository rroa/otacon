/*
===========================================================================

OTACON ENGINE
core/math/Fixed.cpp - Q16.16 fixed-point helpers

The parts of the fixed-point scalar that do not belong in the header.

===========================================================================
*/
#include "core/math/Fixed.hpp"
#include <cstdio>

namespace otacon {

/*
==================
sqrtFx

Integer square root by restoring digit-by-digit binary long division. No
floating point anywhere, which is the entire point of the fixed-point build:
the result is bit-identical on every machine.
==================
*/
Fixed sqrtFx(Fixed f) {
    if (f <= Fixed(0)) return Fixed(0);
    std::uint64_t n = std::uint64_t(f.raw()) << Fixed::kFracBits;
    std::uint64_t res = 0;
    std::uint64_t bit = std::uint64_t(1) << 62;
    while (bit > n) bit >>= 2;
    while (bit != 0) {
        if (n >= res + bit) {
            n -= res + bit;
            res = (res >> 1) + bit;
        } else {
            res >>= 1;
        }
        bit >>= 2;
    }
    return Fixed::fromRaw(Fixed::raw_t(res));
}

/*
==================
toString

Decimal rendering, for the debug overlays.
==================
*/
std::string toString(Fixed f) {
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.5f", f.toDouble());
    return std::string(buf);
}

} // namespace otacon
