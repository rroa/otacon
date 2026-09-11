// CoreAudioDevice.cpp — the macOS audio output backend (Apple only).
//
// Drives a DefaultOutput AudioUnit with a render callback that pulls mixed PCM
// from the in-house Mixer. We hand the unit a 22050 Hz stereo-float client
// format (matching the SFX); the unit resamples to whatever the device wants.
// This is the only OS-specific audio code — everything above it is in-house.
#ifdef __APPLE__
#include "audio/IAudio.hpp"
#include "audio/Mixer.hpp"
#include "asset/Audio.hpp"
#include <AudioUnit/AudioUnit.h>
#include <AudioToolbox/AudioToolbox.h>

namespace otacon {
namespace {

class CoreAudioDevice final : public IAudio {
public:
    CoreAudioDevice() {
        mix_.setRate(kRate);

        AudioComponentDescription desc{};
        desc.componentType = kAudioUnitType_Output;
        desc.componentSubType = kAudioUnitSubType_DefaultOutput;
        desc.componentManufacturer = kAudioUnitManufacturer_Apple;
        AudioComponent comp = AudioComponentFindNext(nullptr, &desc);
        if (!comp || AudioComponentInstanceNew(comp, &unit_) != noErr) { unit_ = nullptr; return; }

        AudioStreamBasicDescription fmt{};
        fmt.mSampleRate       = kRate;
        fmt.mFormatID         = kAudioFormatLinearPCM;
        fmt.mFormatFlags      = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked;
        fmt.mChannelsPerFrame = 2;
        fmt.mBitsPerChannel   = 32;
        fmt.mFramesPerPacket  = 1;
        fmt.mBytesPerFrame    = fmt.mChannelsPerFrame * 4;   // interleaved float
        fmt.mBytesPerPacket   = fmt.mBytesPerFrame;
        AudioUnitSetProperty(unit_, kAudioUnitProperty_StreamFormat,
                             kAudioUnitScope_Input, 0, &fmt, sizeof fmt);

        AURenderCallbackStruct cb{};
        cb.inputProc = &CoreAudioDevice::renderCb;
        cb.inputProcRefCon = this;
        AudioUnitSetProperty(unit_, kAudioUnitProperty_SetRenderCallback,
                             kAudioUnitScope_Input, 0, &cb, sizeof cb);

        if (AudioUnitInitialize(unit_) != noErr || AudioOutputUnitStart(unit_) != noErr) {
            AudioComponentInstanceDispose(unit_);
            unit_ = nullptr;
            return;
        }
        running_ = true;
    }

    ~CoreAudioDevice() override {
        if (unit_) {
            if (running_) AudioOutputUnitStop(unit_);
            AudioUnitUninitialize(unit_);
            AudioComponentInstanceDispose(unit_);
        }
    }

    SoundId createSound(const AudioClip& clip) override { return mix_.add(clip); }
    void play(SoundId id, float gain) override { if (running_) mix_.play(id, gain); }
    void playMusic(SoundId id, bool loop, float gain) override { if (running_) mix_.playMusic(id, loop, gain); }
    void stopMusic() override { mix_.stopMusic(); }
    void setMasterVolume(float v) override { mix_.setMaster(v); }

private:
    static constexpr int kRate = 22050;

    static OSStatus renderCb(void* ref, AudioUnitRenderActionFlags*, const AudioTimeStamp*,
                             UInt32, UInt32 frames, AudioBufferList* io) {
        auto* self = static_cast<CoreAudioDevice*>(ref);
        if (io->mNumberBuffers > 0)
            self->mix_.render(static_cast<float*>(io->mBuffers[0].mData), int(frames), 2);
        return noErr;
    }

    AudioUnit unit_ = nullptr;
    bool      running_ = false;
    Mixer     mix_;
};

} // namespace

IAudio* createAudio() { return new CoreAudioDevice(); }

} // namespace otacon
#endif // __APPLE__
