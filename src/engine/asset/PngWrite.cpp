/*
===========================================================================

OTACON ENGINE
asset/PngWrite.cpp - in-house PNG encoder

Writes RGBA8 back out for screenshots and the cross-backend pixel diff.

Deliberately no compressor: the pixel data goes into stored DEFLATE blocks,
so the files are large but the code is short and there is nothing to get
wrong. A screenshot is written once and looked at once, which makes size the
cheapest thing to spend here.

===========================================================================
*/
#include "asset/Image.hpp"
#include <cstdio>
#include <vector>

namespace otacon {
namespace {

/*
==================
put32

Big-endian append, which is what every PNG field wants.
==================
*/
void put32(std::vector<std::uint8_t>& v, std::uint32_t x) {     // big-endian
    v.push_back(std::uint8_t(x >> 24)); v.push_back(std::uint8_t(x >> 16));
    v.push_back(std::uint8_t(x >> 8));  v.push_back(std::uint8_t(x));
}

/*
==================
crc32

The CRC each chunk carries, computed on the fly rather than from a table.
==================
*/
std::uint32_t crc32(const std::uint8_t* data, std::size_t len) {
    static std::uint32_t table[256];
    static bool init = false;
    if (!init) {
        for (std::uint32_t n = 0; n < 256; ++n) {
            std::uint32_t c = n;
            for (int k = 0; k < 8; ++k) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            table[n] = c;
        }
        init = true;
    }
    std::uint32_t c = 0xFFFFFFFFu;
    for (std::size_t i = 0; i < len; ++i) c = table[(c ^ data[i]) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}

/*
==================
adler32

The checksum the zlib wrapper ends with.
==================
*/
std::uint32_t adler32(const std::uint8_t* data, std::size_t len) {
    std::uint32_t a = 1, b = 0;
    for (std::size_t i = 0; i < len; ++i) { a = (a + data[i]) % 65521; b = (b + a) % 65521; }
    return (b << 16) | a;
}

/*
==================
writeChunk

Length, type, payload, CRC - the shape of every chunk in the file.
==================
*/
void writeChunk(std::vector<std::uint8_t>& out, const char* type,
                const std::uint8_t* data, std::size_t len) {
    put32(out, std::uint32_t(len));
    std::size_t crcStart = out.size();
    out.insert(out.end(), type, type + 4);
    if (len) out.insert(out.end(), data, data + len);
    put32(out, crc32(out.data() + crcStart, 4 + len));   // CRC over type + data
}

} // namespace

/*
==================
writePng

Signature, IHDR, the filtered scanlines wrapped in stored DEFLATE blocks
inside an IDAT, then IEND. Every scanline uses filter 0 (none), since there
is no compressor for a cleverer filter to help.
==================
*/
bool writePng(const char* path, const Image& img) {
    if (!img.valid()) return false;
    const int w = img.width, h = img.height;

    // 1) Filtered raw scanlines: each row prefixed with filter byte 0 (None).
    std::vector<std::uint8_t> raw;
    raw.reserve(std::size_t(h) * (1 + w * 4));
    for (int y = 0; y < h; ++y) {
        raw.push_back(0);
        const std::uint8_t* row = img.rgba.data() + std::size_t(y) * w * 4;
        raw.insert(raw.end(), row, row + std::size_t(w) * 4);
    }

    // 2) zlib stream: 2-byte header + stored DEFLATE blocks + adler32.
    std::vector<std::uint8_t> zlib;
    zlib.push_back(0x78); zlib.push_back(0x01);          // CMF, FLG (valid checksum)
    std::size_t pos = 0;
    while (pos < raw.size()) {
        std::size_t block = raw.size() - pos;
        if (block > 65535) block = 65535;
        bool last = (pos + block >= raw.size());
        zlib.push_back(last ? 1 : 0);                    // BFINAL, BTYPE=00 (stored)
        std::uint16_t len = std::uint16_t(block), nlen = std::uint16_t(~block);
        zlib.push_back(std::uint8_t(len)); zlib.push_back(std::uint8_t(len >> 8));
        zlib.push_back(std::uint8_t(nlen)); zlib.push_back(std::uint8_t(nlen >> 8));
        zlib.insert(zlib.end(), raw.begin() + pos, raw.begin() + pos + block);
        pos += block;
    }
    put32(zlib, adler32(raw.data(), raw.size()));

    // 3) Assemble the PNG.
    std::vector<std::uint8_t> out;
    const std::uint8_t sig[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    out.insert(out.end(), sig, sig + 8);

    std::uint8_t ihdr[13];
    ihdr[0] = std::uint8_t(w >> 24); ihdr[1] = std::uint8_t(w >> 16);
    ihdr[2] = std::uint8_t(w >> 8);  ihdr[3] = std::uint8_t(w);
    ihdr[4] = std::uint8_t(h >> 24); ihdr[5] = std::uint8_t(h >> 16);
    ihdr[6] = std::uint8_t(h >> 8);  ihdr[7] = std::uint8_t(h);
    ihdr[8] = 8;    // bit depth
    ihdr[9] = 6;    // color type RGBA
    ihdr[10] = 0; ihdr[11] = 0; ihdr[12] = 0;   // compression, filter, interlace
    writeChunk(out, "IHDR", ihdr, sizeof ihdr);
    writeChunk(out, "IDAT", zlib.data(), zlib.size());
    writeChunk(out, "IEND", nullptr, 0);

    std::FILE* f = std::fopen(path, "wb");
    if (!f) { std::fprintf(stderr, "[png] cannot write %s\n", path); return false; }
    std::fwrite(out.data(), 1, out.size(), f);
    std::fclose(f);
    return true;
}

} // namespace otacon
