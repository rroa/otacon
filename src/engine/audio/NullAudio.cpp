/*
===========================================================================

OTACON ENGINE
audio/NullAudio.cpp - silent audio stub

The fallback device for platforms with no backend compiled in. It accepts
every call and does nothing.

The factory never returns null, and that is the point: a game with no working
audio device runs exactly the same code as one with a working device, so
'sound is missing' can never become 'the game crashes on that machine'.

===========================================================================
*/
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

/*
==================
createAudio

The factory on platforms without a real backend.
==================
*/
IAudio* createAudio() { return new NullAudio(); }

} // namespace otacon
#endif // !__APPLE__
