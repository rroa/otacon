/*
===========================================================================

OTACON ENGINE
core/math/Vector.cpp - vector math, explicitly instantiated

The operations heavy enough not to want in the header - length, normalize,
distance - defined once and explicitly instantiated for every scalar the
engine actually uses.

Explicit instantiation rather than a header-only template keeps the maths in
one translation unit and makes the set of supported scalars a deliberate
list rather than whatever happened to get used.

===========================================================================
*/
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
