// Rng.hpp — the samples' deterministic random source.
//
// Every sample seeds its own Rng with a fixed constant, so a sample looks the
// same on every run and on every machine. That is what lets tools/backend-diff.sh
// compare a sample's frame across backends: identical input -> identical pixels.
// (Canabalt's GameRandom can flip to OS entropy because a runner *wants* a new
// city each play; a sample wants to be reproducible, so there is no such switch.)
#pragma once
#include <cstdint>

namespace samples {

class Rng {
public:
    explicit Rng(std::uint32_t seed = 0x51EED) : s_(seed ? seed : 1u) {}
    void reseed(std::uint32_t seed) { s_ = seed ? seed : 1u; }

    std::uint32_t next() { s_ = s_ * 1664525u + 1013904223u; return s_; }   // Numerical Recipes LCG
    float unit()  { return float(next() >> 8) / float(1u << 24); }          // [0,1)
    float range(float a, float b) { return a + (b - a) * unit(); }
    int   rangeI(int a, int b) { return b <= a ? a : a + int(next() % std::uint32_t(b - a)); }
    bool  chance(float p) { return unit() < p; }

private:
    std::uint32_t s_;
};

} // namespace samples
