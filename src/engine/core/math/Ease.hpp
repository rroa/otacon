/*
===========================================================================

OTACON ENGINE
core/math/Ease.hpp - interpolation and easing curves

Every one of these takes t in 0..1 and returns a shaped 0..1. That single
convention is what makes them composable: anything you can lerp, you can ease,
and swapping the feel of a motion is swapping which function you passed.

Two things worth knowing before reaching for them:

  * lerp with a per-frame factor is frame-rate dependent. `a += (b-a) * 0.1f`
    converges faster at 120fps than at 30. Use damp() when you want a smooth
    follow that behaves the same at any frame rate - it is the exponential form
    that actually has that property.

  * back and elastic overshoot deliberately, so they leave 0..1 in the middle.
    Do not use them on anything that must stay in range, like a colour channel.

===========================================================================
*/
#pragma once
#include <cmath>

namespace otacon::ease {

inline float clamp01(float t) { return t < 0.f ? 0.f : (t > 1.f ? 1.f : t); }
inline float lerp(float a, float b, float t) { return a + (b - a) * t; }
// Where does v sit between a and b? The inverse of lerp.
inline float inverseLerp(float a, float b, float v) { return (b - a) != 0.f ? (v - a) / (b - a) : 0.f; }
inline float remap(float v, float inA, float inB, float outA, float outB) {
    return lerp(outA, outB, inverseLerp(inA, inB, v));
}

inline float smoothstep(float t) { t = clamp01(t); return t * t * (3.f - 2.f * t); }
// Ken Perlin's smootherstep: second derivative is zero at both ends too, so a
// camera easing on this has no perceptible jerk entering or leaving the move.
inline float smootherstep(float t) { t = clamp01(t); return t * t * t * (t * (t * 6.f - 15.f) + 10.f); }

inline float quadIn(float t)  { t = clamp01(t); return t * t; }
inline float quadOut(float t) { t = clamp01(t); return 1.f - (1.f - t) * (1.f - t); }
inline float quadInOut(float t) {
    t = clamp01(t);
    return t < 0.5f ? 2.f * t * t : 1.f - std::pow(-2.f * t + 2.f, 2.f) * 0.5f;
}

inline float cubicIn(float t)  { t = clamp01(t); return t * t * t; }
inline float cubicOut(float t) { t = clamp01(t); const float u = 1.f - t; return 1.f - u * u * u; }
inline float cubicInOut(float t) {
    t = clamp01(t);
    return t < 0.5f ? 4.f * t * t * t : 1.f - std::pow(-2.f * t + 2.f, 3.f) * 0.5f;
}

inline float sineIn(float t)  { return 1.f - std::cos(clamp01(t) * 1.57079632679f); }
inline float sineOut(float t) { return std::sin(clamp01(t) * 1.57079632679f); }

inline float expoOut(float t) { t = clamp01(t); return t >= 1.f ? 1.f : 1.f - std::pow(2.f, -10.f * t); }

// Overshoots past 1 then settles: the "anticipation" feel for a UI element.
inline float backOut(float t) {
    t = clamp01(t);
    const float c1 = 1.70158f, c3 = c1 + 1.f, u = t - 1.f;
    return 1.f + c3 * u * u * u + c1 * u * u;
}

// Decaying oscillation. Expensive and loud; use sparingly.
inline float elasticOut(float t) {
    t = clamp01(t);
    if (t <= 0.f || t >= 1.f) return t;
    const float c4 = 6.28318530718f / 3.f;
    return std::pow(2.f, -10.f * t) * std::sin((t * 10.f - 0.75f) * c4) + 1.f;
}

inline float bounceOut(float t) {
    t = clamp01(t);
    const float n1 = 7.5625f, d1 = 2.75f;
    if (t < 1.f / d1)      return n1 * t * t;
    if (t < 2.f / d1)      { t -= 1.5f / d1;   return n1 * t * t + 0.75f; }
    if (t < 2.5f / d1)     { t -= 2.25f / d1;  return n1 * t * t + 0.9375f; }
    t -= 2.625f / d1;      return n1 * t * t + 0.984375f;
}

/*
==================
damp

A frame-rate independent approach toward a target. `rate` is roughly how sharply
it converges; the exponential is what makes the result identical whether it was
reached in one big step or several small ones, which a naive per-frame lerp is
not.
==================
*/
inline float damp(float current, float target, float rate, float dt) {
    return target + (current - target) * std::exp(-rate * dt);
}

} // namespace otacon::ease
