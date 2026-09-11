// NullAudio.cpp — the silent audio backend for platforms without a real one yet
// (everything that isn't Apple). It accepts sounds and play() calls and does
// nothing, so the game runs unchanged. Mirrors the SDL2 window stub.
#ifndef __APPLE__
#include "audio/IAudio.hpp"

namespace otacon {
namespace {

class NullAudio final : public IAudio {
public:
    SoundId createSound(const AudioClip&) override { return next_++; }
    void play(SoundId, float) override {}
    void playMusic(SoundId, bool, float) override {}
    void stopMusic() override {}
    void setMasterVolume(float) override {}
private:
    SoundId next_ = 1;
};

} // namespace

IAudio* createAudio() { return new NullAudio(); }

} // namespace otacon
#endif // !__APPLE__
