/*
===========================================================================

OTACON ENGINE
core/math/Scalar.hpp - the pluggable real-number type

Compile with -DOTACON_SCALAR_FIXED to make `Real` a Q16.16 fixed-point
number; otherwise `Real` is a 32-bit float (the default, which reproduces the
original game's math exactly). All simulation/math code is written against
`Real` and the `otacon::s*` helpers below, so it compiles unchanged either way.

===========================================================================
*/
#pragma once
#include "core/math/Fixed.hpp"
#include <cmath>

namespace otacon {

#if defined(OTACON_SCALAR_FIXED)
using Real = Fixed;
inline Real  sabs(Real v)            { return abs(v); }
inline Real  ssqrt(Real v)           { return sqrtFx(v); }
inline Real  sfloor(Real v)          { return floorFx(v); }
inline float toFloat(Real v)         { return v.toFloat(); }
inline int   toInt(Real v)           { return v.toInt(); }
inline Real  R(float v)              { return Real(v); }   // literal helper
constexpr const char* kScalarName = "fixed (Q16.16)";
#else
using Real = float;
inline Real  sabs(Real v)            { return std::fabs(v); }
inline Real  ssqrt(Real v)           { return std::sqrt(v); }
inline Real  sfloor(Real v)          { return std::floor(v); }
inline float toFloat(Real v)         { return v; }
inline int   toInt(Real v)           { return int(v); }
inline Real  R(float v)              { return v; }
constexpr const char* kScalarName = "float (IEEE-754)";
#endif

// Flixel's rounding epsilon, used by collision and screen rounding.
inline Real roundingError() { return R(1e-5f); }

// Type-generic sqrt: both overloads exist in every build so templated math
// (e.g. Vector.cpp, instantiated for float regardless of the Real choice)
// resolves correctly whether T is float or Fixed.
inline float genSqrt(float v) { return std::sqrt(v); }
inline Fixed genSqrt(Fixed v) { return sqrtFx(v); }

} // namespace otacon
