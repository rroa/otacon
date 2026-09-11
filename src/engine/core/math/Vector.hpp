// Vector.hpp — in-house vector math for 2/3/4 dimensions.
//
// We template each vector on the scalar type T so the same code serves the
// float build and the Fixed (Q16.16) build. The heavier operations (length,
// normalize, distance) live in Vector.cpp and are explicitly instantiated
// there for every (dimension x scalar) combination the engine uses.
#pragma once
#include "core/math/Scalar.hpp"

namespace otacon {

template <typename T>
struct TVec2 {
    T x, y;
    constexpr TVec2() : x(T(0)), y(T(0)) {}
    constexpr TVec2(T x_, T y_) : x(x_), y(y_) {}

    TVec2 operator+(const TVec2& o) const { return {x + o.x, y + o.y}; }
    TVec2 operator-(const TVec2& o) const { return {x - o.x, y - o.y}; }
    TVec2 operator-()              const { return {-x, -y}; }
    TVec2 operator*(T s)           const { return {x * s, y * s}; }
    TVec2& operator+=(const TVec2& o) { x += o.x; y += o.y; return *this; }
    TVec2& operator-=(const TVec2& o) { x -= o.x; y -= o.y; return *this; }
    TVec2& operator*=(T s)            { x *= s; y *= s; return *this; }
    bool operator==(const TVec2& o) const { return x == o.x && y == o.y; }

    T     dot(const TVec2& o) const { return x * o.x + y * o.y; }
    T     length() const;           // Vector.cpp
    TVec2 normalized() const;       // Vector.cpp
};

template <typename T>
struct TVec3 {
    T x, y, z;
    constexpr TVec3() : x(T(0)), y(T(0)), z(T(0)) {}
    constexpr TVec3(T x_, T y_, T z_) : x(x_), y(y_), z(z_) {}

    TVec3 operator+(const TVec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    TVec3 operator-(const TVec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    TVec3 operator*(T s)           const { return {x * s, y * s, z * s}; }
    TVec3& operator+=(const TVec3& o) { x += o.x; y += o.y; z += o.z; return *this; }

    T     dot(const TVec3& o) const { return x * o.x + y * o.y + z * o.z; }
    TVec3 cross(const TVec3& o) const {
        return {y * o.z - z * o.y, z * o.x - x * o.z, x * o.y - y * o.x};
    }
    T     length() const;           // Vector.cpp
    TVec3 normalized() const;       // Vector.cpp
};

template <typename T>
struct TVec4 {
    T x, y, z, w;
    constexpr TVec4() : x(T(0)), y(T(0)), z(T(0)), w(T(0)) {}
    constexpr TVec4(T x_, T y_, T z_, T w_) : x(x_), y(y_), z(z_), w(w_) {}
    TVec4 operator+(const TVec4& o) const { return {x + o.x, y + o.y, z + o.z, w + o.w}; }
    TVec4 operator*(T s)           const { return {x * s, y * s, z * s, w * s}; }
    T     dot(const TVec4& o) const { return x * o.x + y * o.y + z * o.z + w * o.w; }
};

template <typename T>
inline TVec2<T> lerp(const TVec2<T>& a, const TVec2<T>& b, T t) {
    return a + (b - a) * t;
}

// Engine-facing aliases use the pluggable Real; explicit float aliases are
// handy for the renderer (GPUs are float regardless of the sim scalar).
using Vec2  = TVec2<Real>;
using Vec3  = TVec3<Real>;
using Vec4  = TVec4<Real>;
using Vec2f = TVec2<float>;
using Vec3f = TVec3<float>;
using Vec4f = TVec4<float>;

} // namespace otacon
