// Sounds.hpp — the game's sound bank: loads every SFX once and exposes named
// one-shot triggers. Variant groups (footsteps, jumps, window smashes…) pick a
// random member each time, the way the original does, via a small private RNG so
// the level-generation stream is never touched. All decoding is in-house (CAF);
// playback goes through the engine's IAudio device.
#pragma once
#include "audio/IAudio.hpp"
#include <cstdint>
#include <vector>

namespace canabalt {

class Sounds {
public:
    void load(otacon::IAudio* audio, const char* assetDir);

    void footstep();      // foot1-4, quieter (plays constantly while running)
    void footConcrete();  // footc1-4: the metal clang of running over a crane
    void jump();          // jump1-3
    void windowSmash();   // window1-2
    void glass();         // glass1-2
    void obstacle();      // obstacle1-3
    void tumble();        // hard landing / roll
    void wall();          // crashed into a wall
    void crumble();       // building collapse / quake
    void flap();          // pigeon flush (flap1-3)
    void flyby();         // jet
    void bombPre();       // bomb whistling down
    void bombHit();       // bomb impact
    void bombExplode();   // bomb blast
    void legLaunch();     // giant leg lifting
    void legStomp();      // giant leg release

    // Background music (looping). The single music voice swaps between these.
    void playMusic();          // the current run theme
    void playTitleMusic();     // the title theme
    void stopMusic();
    const char* nextTrack();   // cycle the run theme; returns the new track's name

private:
    otacon::SoundId pick(const std::vector<otacon::SoundId>& g);
    void one(otacon::SoundId id, float gain);

    static constexpr int kTracks = 3;
    otacon::IAudio* audio_ = nullptr;
    std::vector<otacon::SoundId> foot_, footc_, jump_, window_, glass_, obstacle_, flap_;
    otacon::SoundId tumble_ = 0, wall_ = 0, crumble_ = 0, flyby_ = 0;
    otacon::SoundId bombPre_ = 0, bombHit_ = 0, bombExplode_ = 0, legLaunch_ = 0, legStomp_ = 0;
    otacon::SoundId music_[kTracks] = {0, 0, 0}, titleMusic_ = 0;
    int  track_ = 0;
    bool playingGameplay_ = false;
    std::uint32_t rng_ = 0x9E3779B9u;
};

} // namespace canabalt
