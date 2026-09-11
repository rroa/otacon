/*
===========================================================================

OTACON ENGINE
core/math/Random.hpp - the engine's random source

Every engine needs one of these, and until it had one, fifteen different places
in this project had each written their own linear congruential generator -
including the camera's quake and the particle emitter. That is the smell this
exists to remove.

Two sources, switchable, because games want both at different times:

  Deterministic  a fixed-seed LCG. The same seed gives the same stream, so a
                 level regenerates identically, a replay replays, and a
                 screenshot taken on one machine matches another. This is the
                 default, because reproducible is the more useful default when
                 you are debugging.

  Entropy        seeded from std::random_device via a Mersenne Twister. Every
                 run differs, which is what a shipping game usually wants.

A generator is a value: hold your own instead of sharing a global, and two
systems can neither perturb each other's stream nor be made non-reproducible by
the order they happen to run in. That is why the camera's shake keeps a private
one - a cosmetic effect must never shift the stream a level generator is reading.

===========================================================================
*/
#pragma once
#include "core/math/Vector.hpp"
#include <cstdint>
#include <random>

namespace otacon {

class Random {
public:
    enum class Source { Deterministic, Entropy };

    static constexpr std::uint32_t kDefaultSeed = 0x07AC05u;

    explicit Random(std::uint32_t s = kDefaultSeed) : state_(s ? s : 1u), seed_(s ? s : 1u) {}

    /*
    ==================
    seed / reseed
    ==================
    */
    void seed(std::uint32_t s) { seed_ = s ? s : 1u; state_ = seed_; }
    std::uint32_t currentSeed() const { return seed_; }
    // Restart the deterministic stream from the seed it was given. The whole
    // point of a fixed seed is being able to do this.
    void restart() { state_ = seed_; }

    void setSource(Source s) {
        source_ = s;
        if (source_ == Source::Entropy) { std::random_device rd; mt_.seed(rd()); }
        else restart();
    }
    Source source() const { return source_; }
    const char* sourceName() const { return source_ == Source::Deterministic ? "DET(LCG)" : "RANDOM"; }

    /*
    ==================
    next

    The raw 32 bits. Numerical Recipes' LCG constants in the deterministic case:
    not a CSPRNG, and not meant to be - it is small, fast, and above all exactly
    reproducible, which is what a game wants from this.
    ==================
    */
    std::uint32_t next() {
        if (source_ == Source::Deterministic) {
            state_ = state_ * 1664525u + 1013904223u;
            return state_;
        }
        return mt_();
    }

    // The top 24 bits make the mantissa: the low bits of an LCG are the weakest,
    // so taking from the top is not an optimisation but a correctness habit.
    float unit() { return float(next() >> 8) / float(1u << 24); }

    float range(float a, float b) { return a + (b - a) * unit(); }
    // [a, b) - half-open, so rangeI(0, n) indexes an n-element array.
    int   rangeI(int a, int b) { return b <= a ? a : a + int(next() % std::uint32_t(b - a)); }
    bool  chance(float p) { return unit() < p; }
    int   sign() { return (next() & 1u) ? 1 : -1; }

    /*
    ==================
    onUnitCircle / inUnitCircle

    A direction, and a point. inUnitCircle uses the sqrt of a uniform radius:
    sampling radius uniformly would crowd the centre, because area grows with r^2.
    ==================
    */
    Vec2f onUnitCircle() {
        const float a = unit() * 6.28318530718f;
        return {std::cos(a), std::sin(a)};
    }
    Vec2f inUnitCircle() {
        const Vec2f d = onUnitCircle();
        const float r = std::sqrt(unit());
        return {d.x * r, d.y * r};
    }
    Vec2f inRect(float w, float h) { return {range(0.f, w), range(0.f, h)}; }

    /*
    ==================
    pick / shuffle
    ==================
    */
    template <typename T>
    const T& pick(const T* items, int n) { return items[rangeI(0, n > 0 ? n : 1)]; }

    // Fisher-Yates, walked backwards. Any other loop shape is subtly biased.
    template <typename T>
    void shuffle(T* items, int n) {
        for (int i = n - 1; i > 0; --i) {
            const int j = rangeI(0, i + 1);
            T tmp = items[i]; items[i] = items[j]; items[j] = tmp;
        }
    }

private:
    Source        source_ = Source::Deterministic;
    std::uint32_t state_  = kDefaultSeed;
    std::uint32_t seed_   = kDefaultSeed;
    std::mt19937  mt_;
};

} // namespace otacon
