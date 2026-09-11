// GameRandom.hpp — the game's switchable random-number source.
//
// CLASS NOTE -------------------------------------------------------------
// The original Canabalt drew randomness from `arc4random` (FlxU.random), a
// non-deterministic OS generator: every play-through produces a different city.
// For a teaching port we want to be able to CHOOSE:
//
//   * DETERMINISTIC  — a tiny Linear Congruential Generator (LCG) seeded with a
//                      FIXED constant. Same seed -> same number stream -> the
//                      EXACT same level every run. Great for debugging, replays
//                      and lockstep netcode, because the world is reproducible.
//
//   * NONDETERMINISTIC — seeded from OS entropy (std::random_device), the
//                      portable stand-in for arc4random. Every run differs, like
//                      the shipping game.
//
// Press F1 in-game to flip between them and watch the level regenerate. Things
// worth discussing with the class: what "seeding" means, why a fixed seed gives
// reproducibility, LCG quality vs. a real CSPRNG, and when each is the right
// tool. ----------------------------------------------------------------------
#pragma once
#include <cstdint>
#include <random>

namespace canabalt {

class GameRandom {
public:
    enum class Mode { Deterministic, Nondeterministic };

    void setMode(Mode m) { mode_ = m; restart(); }
    Mode mode() const { return mode_; }
    const char* modeName() const {
        return mode_ == Mode::Deterministic ? "DET(LCG)" : "RANDOM";
    }

    // Re-seed for a fresh level. Deterministic -> the fixed seed (reproducible);
    // Nondeterministic -> fresh OS entropy (different each time).
    void restart() {
        if (mode_ == Mode::Deterministic) {
            lcg_ = kFixedSeed;
        } else {
            std::random_device rd;
            mt_.seed(rd());
        }
    }

    // Uniform float in [0, 1) — the one primitive the generator needs.
    float unit() {
        std::uint32_t bits;
        if (mode_ == Mode::Deterministic) {
            // Numerical Recipes LCG constants.
            lcg_ = lcg_ * 1664525u + 1013904223u;
            bits = lcg_;
        } else {
            bits = mt_();   // Mersenne Twister, entropy-seeded
        }
        return float(bits >> 8) / float(1u << 24);   // top 24 bits -> [0,1)
    }

private:
    static constexpr std::uint32_t kFixedSeed = 0xCA1Au;  // "CALA"-ish, fixed on purpose
    Mode          mode_ = Mode::Deterministic;
    std::uint32_t lcg_  = kFixedSeed;
    std::mt19937  mt_;
};

} // namespace canabalt
