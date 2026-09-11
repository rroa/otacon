#include "canabalt/Sounds.hpp"
#include "asset/Audio.hpp"
#include <string>

using namespace otacon;

namespace canabalt {

void Sounds::load(IAudio* audio, const char* assetDir) {
    audio_ = audio;
    if (!audio_) return;
    const std::string dir = std::string(assetDir) + "/sound/";
    auto one = [&](const char* file) -> SoundId {
        return audio_->createSound(loadCaf((dir + file + ".caf").c_str()));
    };
    auto group = [&](std::vector<SoundId>& g, const char* stem, int from, int to) {
        for (int i = from; i <= to; ++i) g.push_back(one((std::string(stem) + std::to_string(i)).c_str()));
    };
    group(foot_, "foot", 1, 4);
    group(footc_, "footc", 1, 4);
    group(jump_, "jump", 1, 3);
    group(window_, "window", 1, 2);
    group(glass_, "glass", 1, 2);
    group(obstacle_, "obstacle", 1, 3);
    group(flap_, "flap", 1, 3);
    tumble_  = one("tumble");
    wall_    = one("wall");
    crumble_ = one("crumble");
    flyby_   = one("flyby");
    bombPre_     = one("bomb_pre");
    bombHit_     = one("bomb_hit");
    bombExplode_ = one("bomb_explode");
    legLaunch_   = one("giant_leg");
    legStomp_    = one("giant_leg_release");
    // Music lives in a separate folder and is converted to LPCM CAF the same way
    // (assets/music/*.caf via afconvert). Each whole song sits in RAM and loops;
    // F10 cycles the three run themes.
    const std::string mdir = std::string(assetDir) + "/music/";
    const char* tracks[kTracks] = {"run", "daringescape", "machrunner"};
    for (int i = 0; i < kTracks; ++i)
        music_[i] = audio_->createSound(loadCaf((mdir + tracks[i] + ".caf").c_str()));
    titleMusic_ = audio_->createSound(loadCaf((mdir + "run-title.caf").c_str()));
}

// A private stream just for picking SFX variants, so which footstep plays can
// never shift the stream the level generator is reading.
SoundId Sounds::pick(const std::vector<SoundId>& g) {
    if (g.empty()) return 0;
    return g[std::size_t(rng_.rangeI(0, int(g.size())))];
}

void Sounds::one(SoundId id, float gain) { if (audio_ && id) audio_->play(id, gain); }

void Sounds::footstep()    { one(pick(foot_), 0.35f); }
void Sounds::footConcrete(){ one(pick(footc_), 0.4f); }
void Sounds::jump()        { one(pick(jump_), 0.8f); }
void Sounds::windowSmash() { one(pick(window_), 0.9f); }
void Sounds::glass()       { one(pick(glass_), 0.7f); }
void Sounds::obstacle()    { one(pick(obstacle_), 0.8f); }
// The flap recordings are very quiet (peak ~0.04 vs ~0.28 for footsteps), so a
// big gain is needed for the flock to actually be heard taking off.
void Sounds::flap()        { one(pick(flap_), 3.0f); }
void Sounds::tumble()      { one(tumble_, 0.9f); }
void Sounds::wall()        { one(wall_, 1.0f); }
void Sounds::crumble()     { one(crumble_, 0.9f); }
void Sounds::flyby()       { one(flyby_, 0.8f); }
void Sounds::bombPre()     { one(bombPre_, 0.7f); }
void Sounds::bombHit()     { one(bombHit_, 0.9f); }
void Sounds::bombExplode() { one(bombExplode_, 1.0f); }
void Sounds::legLaunch()   { one(legLaunch_, 0.9f); }
void Sounds::legStomp()    { one(legStomp_, 1.0f); }

void Sounds::playMusic() {
    playingGameplay_ = true;
    if (audio_ && music_[track_]) audio_->playMusic(music_[track_], true, 0.55f);
}
void Sounds::playTitleMusic() {
    playingGameplay_ = false;
    if (audio_ && titleMusic_) audio_->playMusic(titleMusic_, true, 0.6f);
}
void Sounds::stopMusic() { playingGameplay_ = false; if (audio_) audio_->stopMusic(); }

const char* Sounds::nextTrack() {
    static const char* names[kTracks] = {"RUN", "DARING ESCAPE", "MACH RUNNER"};
    track_ = (track_ + 1) % kTracks;
    if (playingGameplay_) playMusic();      // swap live if a run is underway
    return names[track_];
}

} // namespace canabalt
