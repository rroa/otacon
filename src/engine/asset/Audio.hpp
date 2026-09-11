// Audio.hpp — a decoded PCM clip and the in-house CAF (Core Audio Format) loader.
//
// Sound effects ship as CAF files holding uncompressed little-endian
// 16-bit PCM (e.g. 22050 Hz, mono — see a game's assets/sound/convert.sh). CAF is a simple
// chunked, big-endian container: an 8-byte file header, then ('type', int64 size,
// payload) chunks. We only need two: 'desc' (the audio format) and 'data' (the
// samples). Everything is decoded to interleaved float in [-1, 1] so the mixer
// has one uniform representation.
#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>

namespace otacon {

struct AudioClip {
    std::vector<float> samples;   // interleaved, [-1, 1]
    int sampleRate = 22050;
    int channels   = 1;

    bool valid() const { return !samples.empty() && channels > 0; }
    int  frames() const { return channels ? int(samples.size()) / channels : 0; }
};

// Decode an in-memory CAF (LPCM: 8/16/24/32-bit int or 32/64-bit float, either
// endianness) to float samples. Returns an invalid clip on failure.
AudioClip decodeCaf(const std::uint8_t* data, std::size_t len);

// Read a CAF file from disk and decode it.
AudioClip loadCaf(const char* path);

} // namespace otacon
