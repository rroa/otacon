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
// The generator itself is otacon::Random; what lives here is the game's choice
// of WHICH source to use and the fixed seed that makes a run reproducible.
//
// Press F1 in-game to flip between them and watch the level regenerate. Things
// worth discussing with the class: what "seeding" means, why a fixed seed gives
// reproducibility, LCG quality vs. a real CSPRNG, and when each is the right
// tool. ----------------------------------------------------------------------
#pragma once
#include "core/math/Random.hpp"
#include <cstdint>

namespace canabalt {

class GameRandom {
public:
    enum class Mode { Deterministic, Nondeterministic };

    void setMode(Mode m) {
        mode_ = m;
        rng_.setSource(m == Mode::Deterministic ? otacon::Random::Source::Deterministic
                                                : otacon::Random::Source::Entropy);
    }
    Mode mode() const { return mode_; }
    const char* modeName() const { return rng_.sourceName(); }

    // Re-seed for a fresh level. Deterministic -> the fixed seed (reproducible);
    // Nondeterministic -> fresh OS entropy (different each time).
    void restart() { setMode(mode_); }

    // Uniform float in [0, 1) -- the one primitive the generator needs.
    float unit() { return rng_.unit(); }

private:
    // "CALA"-ish, fixed on purpose: the same seed regenerates the same city.
    static constexpr std::uint32_t kFixedSeed = 0xCA1Au;
    Mode           mode_ = Mode::Deterministic;
    otacon::Random rng_{kFixedSeed};
};

} // namespace canabalt
