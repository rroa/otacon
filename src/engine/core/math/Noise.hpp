/*
===========================================================================

OTACON ENGINE
core/math/Noise.hpp - value noise and fractal sums

Noise is how you get variety that does not look random. A lattice of hashed
values, smoothly interpolated, gives you something continuous: sample two nearby
points and you get two nearby values. That is what separates it from calling a
random generator per pixel, which just gives you static.

Value noise rather than Perlin: it is a few lines, it has no gradient table, and
at the scale a 2D game uses it - terrain heights, tile shading, cloud masks - the
difference is not visible. fbm() sums octaves at halving amplitude and doubling
frequency, which is what turns one smooth wobble into something that reads as
detail at several scales at once.

Everything here is a pure function of its arguments and a seed, so it is
reproducible, thread-safe and needs no state.

===========================================================================
*/
#pragma once
#include <cmath>
#include <cstdint>

namespace otacon::noise {

/*
==================
hash

An integer hash, not a random generator: the same lattice point must give the
same value every time it is asked, from anywhere, or the noise would not be
continuous between calls.
==================
*/
inline float hash(int x, int y, std::uint32_t seed = 0) {
    std::uint32_t n = std::uint32_t(x) * 374761393u + std::uint32_t(y) * 668265263u + seed * 2654435761u;
    n = (n ^ (n >> 13)) * 1274126177u;
    return float((n ^ (n >> 16)) & 0xFFFFFFu) / float(0xFFFFFFu);
}

/*
==================
value

Bilinear interpolation between four lattice corners, with the weights run
through a smoothstep first. Without that easing the derivative is discontinuous
at every lattice line and the result shows visible creases.
==================
*/
inline float value(float x, float y, std::uint32_t seed = 0) {
    const int xi = int(std::floor(x)), yi = int(std::floor(y));
    float fx = x - float(xi), fy = y - float(yi);
    fx = fx * fx * (3.f - 2.f * fx);
    fy = fy * fy * (3.f - 2.f * fy);
    const float a = hash(xi,     yi,     seed), b = hash(xi + 1, yi,     seed);
    const float c = hash(xi,     yi + 1, seed), d = hash(xi + 1, yi + 1, seed);
    return (a * (1 - fx) + b * fx) * (1 - fy) + (c * (1 - fx) + d * fx) * fy;
}

// One-dimensional, for terrain height and anything else indexed by a single axis.
inline float value1(float x, std::uint32_t seed = 0) { return value(x, 0.5f, seed); }

/*
==================
fbm

Fractional Brownian motion: octaves at doubling frequency and halving amplitude,
normalised so the result stays in 0..1 whatever the octave count.
==================
*/
inline float fbm(float x, float y, int octaves = 4, float lacunarity = 2.f,
                 float gain = 0.5f, std::uint32_t seed = 0) {
    float sum = 0.f, amp = 1.f, norm = 0.f, fx = x, fy = y;
    for (int i = 0; i < (octaves > 0 ? octaves : 1); ++i) {
        sum  += value(fx, fy, seed + std::uint32_t(i) * 101u) * amp;
        norm += amp;
        amp  *= gain;
        fx   *= lacunarity;
        fy   *= lacunarity;
    }
    return norm > 0.f ? sum / norm : 0.f;
}

inline float fbm1(float x, int octaves = 4, std::uint32_t seed = 0) {
    return fbm(x, 0.5f, octaves, 2.f, 0.5f, seed);
}

/*
==================
ridged

fbm folded about its midpoint, which turns the smooth hills into creases. The
usual choice for mountain ridges and for cracks.
==================
*/
inline float ridged(float x, float y, int octaves = 4, std::uint32_t seed = 0) {
    const float v = fbm(x, y, octaves, 2.f, 0.5f, seed);
    return 1.f - std::fabs(v * 2.f - 1.f);
}

} // namespace otacon::noise
