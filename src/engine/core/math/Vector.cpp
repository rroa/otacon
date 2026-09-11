// Vector.cpp — out-of-line vector operations + explicit instantiation.
//
// length()/normalized() need a square root, which differs between scalar types
// (std::sqrt for float, our bit-by-bit sqrtFx for Fixed). Keeping them here in
// one translation unit, instantiated explicitly, avoids pulling <cmath> into
// every header and shows how a small math kernel is shared across dimensions.
#include "core/math/Vector.hpp"

namespace otacon {

template <typename T> T        TVec2<T>::length() const { return genSqrt(x * x + y * y); }
template <typename T> TVec2<T> TVec2<T>::normalized() const {
    T len = length();
    if (len == T(0)) return TVec2<T>(T(0), T(0));
    T inv = T(1) / len;
    return TVec2<T>(x * inv, y * inv);
}

template <typename T> T        TVec3<T>::length() const { return genSqrt(x * x + y * y + z * z); }
template <typename T> TVec3<T> TVec3<T>::normalized() const {
    T len = length();
    if (len == T(0)) return TVec3<T>(T(0), T(0), T(0));
    T inv = T(1) / len;
    return TVec3<T>(x * inv, y * inv, z * inv);
}

// Instantiate for both scalar types so float and Fixed builds both link.
template struct TVec2<float>;
template struct TVec3<float>;
template struct TVec4<float>;
#if defined(OTACON_SCALAR_FIXED)
template struct TVec2<Fixed>;
template struct TVec3<Fixed>;
template struct TVec4<Fixed>;
#endif

} // namespace otacon
