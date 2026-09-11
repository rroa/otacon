#include "core/math/Fixed.hpp"
#include <cstdio>

namespace otacon {

// Integer square root of a Q16.16 value via the classic bit-by-bit method.
// We compute sqrt over the raw integer scaled up so the result lands back in
// Q16.16: sqrt(x) in real == sqrt(raw/2^16) == sqrt(raw)/2^8, so we work on
// (raw << 16) and the result is already Q16.16.
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

std::string toString(Fixed f) {
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.5f", f.toDouble());
    return std::string(buf);
}

} // namespace otacon
