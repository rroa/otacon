#include "audio/Mixer.hpp"
#include <algorithm>

namespace otacon {

SoundId Mixer::add(const AudioClip& clip) {
    if (!clip.valid()) return 0;
    Sound s;
    s.channels = clip.channels;
    if (clip.sampleRate == rate_) {
        s.data = clip.samples;
    } else {
        // Nearest-neighbour resample to the mixer rate (SFX are already 22050, so
        // this is just defensive — keeps render() free of rate conversion).
        const int ch = clip.channels;
        const std::size_t srcFrames = clip.samples.size() / std::size_t(ch);
        const double ratio = double(rate_) / double(clip.sampleRate);
        const std::size_t dstFrames = std::size_t(double(srcFrames) * ratio);
        s.data.resize(dstFrames * std::size_t(ch));
        for (std::size_t i = 0; i < dstFrames; ++i) {
            std::size_t srcI = std::min(srcFrames - 1, std::size_t(double(i) / ratio));
            for (int c = 0; c < ch; ++c)
                s.data[i * ch + c] = clip.samples[srcI * ch + c];
        }
    }
    std::lock_guard<std::mutex> lock(mtx_);
    if (sounds_.empty()) sounds_.emplace_back();   // reserve id 0
    sounds_.push_back(std::move(s));
    return SoundId(sounds_.size() - 1);
}

void Mixer::play(SoundId id, float gain) {
    std::lock_guard<std::mutex> lock(mtx_);
    if (id == 0 || id >= sounds_.size() || sounds_[id].data.empty()) return;
    if (voices_.size() >= kMaxVoices) voices_.erase(voices_.begin());   // steal the oldest
    voices_.push_back({id, 0, gain, false, true});
}

void Mixer::playMusic(SoundId id, bool loop, float gain) {
    std::lock_guard<std::mutex> lock(mtx_);
    if (id == 0 || id >= sounds_.size() || sounds_[id].data.empty()) { music_.active = false; return; }
    music_ = {id, 0, gain, loop, true};
}

void Mixer::stopMusic() {
    std::lock_guard<std::mutex> lock(mtx_);
    music_.active = false;
}

void Mixer::render(float* out, int frames, int outChannels) {
    const int oc = outChannels > 0 ? outChannels : 2;
    std::fill(out, out + std::size_t(frames) * oc, 0.f);

    std::lock_guard<std::mutex> lock(mtx_);
    const float master = master_.load();

    // Background music: one voice that wraps to the start when it reaches the end.
    if (music_.active && music_.sound < sounds_.size()) {
        const Sound& m = sounds_[music_.sound];
        const int mc = m.channels;
        const std::size_t mFrames = m.data.size() / std::size_t(mc);
        const float g = music_.gain * master;
        for (int i = 0; i < frames && mFrames; ++i) {
            if (music_.pos >= mFrames) { if (music_.loop) music_.pos = 0; else { music_.active = false; break; } }
            const float l = m.data[music_.pos * mc] * g;
            const float r = (mc >= 2 ? m.data[music_.pos * mc + 1] : m.data[music_.pos * mc]) * g;
            float* o = out + std::size_t(i) * oc;
            o[0] += l; if (oc >= 2) o[1] += r;
            ++music_.pos;
        }
    }

    for (std::size_t v = 0; v < voices_.size();) {
        Voice& voice = voices_[v];
        const Sound& snd = sounds_[voice.sound];
        const int sc = snd.channels;
        const std::size_t srcFrames = snd.data.size() / std::size_t(sc);
        int i = 0;
        for (; i < frames && voice.pos < srcFrames; ++i, ++voice.pos) {
            const float g = voice.gain * master;
            // Mono -> both ears; stereo -> matched ears; anything else -> ch0.
            const float l = snd.data[voice.pos * sc] * g;
            const float r = (sc >= 2 ? snd.data[voice.pos * sc + 1] : snd.data[voice.pos * sc]) * g;
            float* o = out + std::size_t(i) * oc;
            o[0] += l;
            if (oc >= 2) o[1] += r;
        }
        if (voice.pos >= srcFrames) voices_.erase(voices_.begin() + v);   // finished
        else ++v;
    }

    // Soft clamp so a pile-up of voices can't wrap.
    const std::size_t n = std::size_t(frames) * oc;
    for (std::size_t k = 0; k < n; ++k)
        out[k] = std::max(-1.f, std::min(1.f, out[k]));
}

} // namespace otacon
