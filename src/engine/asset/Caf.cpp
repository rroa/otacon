/*
===========================================================================

OTACON ENGINE
asset/Caf.cpp - in-house CAF audio decoder

Reads Core Audio Format files - the form the original game shipped its sounds
in - and decodes LPCM to interleaved float.

CAF is a big-endian chunked container, and only two chunks matter here: desc
gives the format, data gives the samples. Everything is normalised to float
on load so the mixer has exactly one representation to sum.

===========================================================================
*/
#include "asset/Audio.hpp"
#include <algorithm>
#include <cstdio>
#include <cstring>

namespace otacon {
namespace {

/*
==================
beU16

CAF is big-endian throughout.
==================
*/
std::uint16_t beU16(const std::uint8_t* p) { return std::uint16_t(p[0] << 8 | p[1]); }

/*
==================
beU32

CAF is big-endian throughout.
==================
*/
std::uint32_t beU32(const std::uint8_t* p) {
    return std::uint32_t(p[0]) << 24 | std::uint32_t(p[1]) << 16 |
           std::uint32_t(p[2]) << 8  | std::uint32_t(p[3]);
}

/*
==================
beU64

CAF is big-endian throughout.
==================
*/
std::uint64_t beU64(const std::uint8_t* p) {
    return std::uint64_t(beU32(p)) << 32 | beU32(p + 4);
}

/*
==================
beF64

The sample rate is stored as a big-endian IEEE double.
==================
*/
double beF64(const std::uint8_t* p) {
    std::uint64_t bits = beU64(p);
    double d; std::memcpy(&d, &bits, 8);
    return d;
}

/*
==================
sampleToFloat

One sample, whatever its width and endianness, normalised to [-1,1]. Signed
integer formats divide by their own max, so 16-bit and 24-bit material ends
up at the same loudness.
==================
*/
float sampleToFloat(const std::uint8_t* p, int bits, bool littleEndian, bool isFloat) {
    auto bytes = [&](int n) {                       // assemble `n` bytes, native order
        std::uint32_t v = 0;
        for (int i = 0; i < n; ++i)
            v |= std::uint32_t(p[littleEndian ? i : n - 1 - i]) << (8 * i);
        return v;
    };
    if (isFloat) {
        if (bits == 32) { std::uint32_t v = bytes(4); float f; std::memcpy(&f, &v, 4); return f; }
        if (bits == 64) {                            // assemble 8 bytes -> double
            std::uint64_t v = 0;
            for (int i = 0; i < 8; ++i)
                v |= std::uint64_t(p[littleEndian ? i : 7 - i]) << (8 * i);
            double d; std::memcpy(&d, &v, 8); return float(d);
        }
        return 0.f;
    }
    switch (bits) {
        case 8:  return (int(p[0]) - 128) / 128.f;            // CAF 8-bit PCM is unsigned
        case 16: return std::int16_t(bytes(2)) / 32768.f;
        case 24: { std::int32_t s = std::int32_t(bytes(3) << 8) >> 8; return s / 8388608.f; }
        case 32: return std::int32_t(bytes(4)) / 2147483648.f;
        default: return 0.f;
    }
}

} // namespace

/*
==================
decodeCaf

Walk the chunk list for desc and data, then convert every sample. A chunk
size of -1 means 'to the end of the file', which is how a stream that was
never finalised reports its length.
==================
*/
AudioClip decodeCaf(const std::uint8_t* data, std::size_t len) {
    AudioClip clip;
    if (!data || len < 8 || std::memcmp(data, "caff", 4) != 0) return clip;

    int   bits = 16, channels = 1;
    double rate = 22050.0;
    bool  little = true, isFloat = false;
    const std::uint8_t* pcm = nullptr;
    std::size_t pcmBytes = 0;

    std::size_t off = 8;                              // past 'caff' + version + flags
    while (off + 12 <= len) {
        const std::uint8_t* hdr = data + off;
        std::uint64_t size = beU64(hdr + 4);
        std::size_t body = off + 12;
        if (std::memcmp(hdr, "desc", 4) == 0 && body + 32 <= len) {
            rate     = beF64(data + body);
            // formatID at +8 ('lpcm'); we only support LPCM.
            std::uint32_t flags = beU32(data + body + 12);
            channels = int(beU32(data + body + 24));
            bits     = int(beU32(data + body + 28));
            isFloat  = (flags & 0x1u) != 0;           // kCAFLinearPCMFormatFlagIsFloat
            little   = (flags & 0x2u) != 0;           // kCAFLinearPCMFormatFlagIsLittleEndian
            if (std::memcmp(data + body + 8, "lpcm", 4) != 0) return clip;  // compressed: unsupported
        } else if (std::memcmp(hdr, "data", 4) == 0) {
            // The data chunk opens with a 4-byte mEditCount; samples follow it.
            // A size of -1 means "to end of file".
            std::size_t avail = (size == 0 || size == ~0ull) ? (len - body) : std::size_t(size);
            if (avail < 4 || body + 4 > len) break;
            pcm = data + body + 4;
            pcmBytes = std::min(avail - 4, len - (body + 4));
            break;                                     // data is last; stop
        }
        if (size == 0 || size == ~0ull) break;         // unsized non-data chunk: give up
        off = body + std::size_t(size);
    }

    if (!pcm || channels <= 0 || bits <= 0) return clip;
    const int stride = bits / 8;
    if (stride <= 0) return clip;
    const std::size_t count = pcmBytes / std::size_t(stride);
    clip.samples.resize(count);
    for (std::size_t i = 0; i < count; ++i)
        clip.samples[i] = sampleToFloat(pcm + i * stride, bits, little, isFloat);
    clip.sampleRate = int(rate);
    clip.channels   = channels;
    return clip;
}

/*
==================
loadCaf

Read a file into memory and decode it.
==================
*/
AudioClip loadCaf(const char* path) {
    AudioClip clip;
    std::FILE* f = std::fopen(path, "rb");
    if (!f) return clip;
    std::fseek(f, 0, SEEK_END);
    long n = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (n > 0) {
        std::size_t sz = std::size_t(n);
        std::vector<std::uint8_t> buf(sz);
        if (std::fread(buf.data(), 1, sz, f) == sz)
            clip = decodeCaf(buf.data(), buf.size());
    }
    std::fclose(f);
    return clip;
}

} // namespace otacon
