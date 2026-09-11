// Mixer.hpp — the in-house software mixer shared by every audio backend.
//
// Holds the uploaded sounds and a pool of active one-shot voices, and sums them
// into an interleaved stereo float buffer. `play()` runs on the game thread and
// `render()` on the platform's real-time audio thread, so a short mutex guards
// the voice list (fine at this scale; a teaching port, not a DAW). Sounds are
// resampled to the mixer rate on upload, so `render()` never has to.
#pragma once
#include "audio/IAudio.hpp"
#include "asset/Audio.hpp"
#include <atomic>
#include <cstdint>
#include <mutex>
#include <vector>

namespace otacon {

class Mixer {
public:
    void setRate(int rate) { rate_ = rate > 0 ? rate : 22050; }
    void setMaster(float v) { master_.store(v < 0 ? 0 : v); }

    SoundId add(const AudioClip& clip);            // game thread; returns 1-based id
    void    play(SoundId id, float gain);          // game thread
    void    playMusic(SoundId id, bool loop, float gain);   // game thread
    void    stopMusic();                           // game thread
    void    render(float* out, int frames, int outChannels);   // audio thread

private:
    struct Sound { std::vector<float> data; int channels = 1; };
    struct Voice { std::uint32_t sound = 0; std::size_t pos = 0; float gain = 1.f;
                   bool loop = false; bool active = false; };

    static constexpr std::size_t kMaxVoices = 32;

    std::vector<Sound>  sounds_;     // index 0 unused so id 0 stays "invalid"
    std::vector<Voice>  voices_;     // one-shots
    Voice               music_;      // the single looping music voice
    std::mutex          mtx_;
    std::atomic<float>  master_{1.f};
    int                 rate_ = 22050;
};

} // namespace otacon
