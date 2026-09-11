/*
===========================================================================

OTACON ENGINE
audio/IAudio.hpp - the audio output seam

The game decodes its sounds in-house (asset/Audio.hpp) and hands the PCM to the
device with createSound(); thereafter it just fires one-shots with play(). The
concrete device owns an in-house software Mixer and a thin platform output
(CoreAudio on macOS; a silent stub elsewhere) — the only OS dependency, exactly
like GLFW is for windowing. The factory never returns null: with no working
device you get a stub that swallows everything, so the game code is unchanged.

===========================================================================
*/
#pragma once
#include <cstdint>

namespace otacon {

struct AudioClip;
using SoundId = std::uint32_t;     // 0 = invalid

class IAudio {
public:
    virtual ~IAudio() = default;

    // Upload a decoded clip; returns a handle to play later (0 on failure).
    virtual SoundId createSound(const AudioClip& clip) = 0;
    // Fire a one-shot voice for a previously-created sound.
    virtual void play(SoundId id, float gain = 1.f) = 0;
    // The single background-music voice (a previously-created sound), looped by
    // default. Calling it again swaps the track; stopMusic() silences it.
    virtual void playMusic(SoundId id, bool loop = true, float gain = 0.6f) = 0;
    virtual void stopMusic() = 0;
    // Overall output level in [0, 1].
    virtual void setMasterVolume(float v) = 0;
};

// Build the platform's audio device (CoreAudio on Apple, silent stub otherwise).
IAudio* createAudio();

} // namespace otacon
