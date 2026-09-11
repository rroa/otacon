/*
===========================================================================

OTACON ENGINE
asset/Png.cpp - in-house PNG decoder

Decodes 8-bit PNG - greyscale, RGB, palette, grey+alpha and RGBA,
non-interlaced - to RGBA8, on top of the DEFLATE decoder in Inflate.cpp.

The interesting part of PNG is not the container but the filtering: every
scanline is stored as a delta against its neighbours, chosen per line by the
encoder, and undoing that is most of the work below.

===========================================================================
*/
#include "asset/Image.hpp"
#include "asset/Inflate.hpp"
#include <cstdio>
#include <cstring>

namespace otacon {
namespace {

/*
==================
be32

PNG is big-endian throughout, regardless of the host.
==================
*/
std::uint32_t be32(const std::uint8_t* p) {
    return (std::uint32_t(p[0]) << 24) | (std::uint32_t(p[1]) << 16) |
           (std::uint32_t(p[2]) << 8) | std::uint32_t(p[3]);
}

/*
==================
channelsFor

Samples per pixel for a colour type. Palette images report one, because the
sample is an index rather than a colour.
==================
*/
int channelsFor(int colorType) {
    switch (colorType) {
        case 0: return 1;   // grayscale
        case 2: return 3;   // RGB
        case 3: return 1;   // palette index
        case 4: return 2;   // grayscale + alpha
        case 6: return 4;   // RGBA
        default: return 0;
    }
}

/*
==================
paeth

The Paeth predictor: pick whichever of the left, above or upper-left
neighbour is closest to their linear estimate. It is the most effective of
the five filters on photographic data and the only one that needs more
than an add.
==================
*/
int paeth(int a, int b, int c) {
    int p = a + b - c;
    int pa = p > a ? p - a : a - p;
    int pb = p > b ? p - b : b - p;
    int pc = p > c ? p - c : c - p;
    if (pa <= pb && pa <= pc) return a;
    return (pb <= pc) ? b : c;
}

} // namespace

/*
==================
decodePng

Walk the chunk stream for IHDR, PLTE, tRNS and the IDAT payload, inflate it,
then undo the per-scanline filter and expand whatever sample format came
out into RGBA8.
==================
*/
Image decodePng(const std::uint8_t* data, std::size_t len) {
    Image img;
    static const std::uint8_t kSig[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    if (len < 8 || std::memcmp(data, kSig, 8) != 0) {
        std::fprintf(stderr, "[png] bad signature\n");
        return img;
    }

    int width = 0, height = 0, bitDepth = 0, colorType = 0, interlace = 0;
    std::vector<std::uint8_t> palette;     // RGB triples
    std::vector<std::uint8_t> trns;        // palette alpha (colorType 3)
    std::vector<std::uint8_t> idat;        // concatenated compressed data

    std::size_t pos = 8;
    bool sawIHDR = false;
    while (pos + 8 <= len) {
        std::uint32_t clen = be32(data + pos);
        const std::uint8_t* type = data + pos + 4;
        const std::uint8_t* chunk = data + pos + 8;
        if (pos + 12 + clen > len) break;          // truncated

        if (std::memcmp(type, "IHDR", 4) == 0) {
            width = int(be32(chunk));
            height = int(be32(chunk + 4));
            bitDepth = chunk[8];
            colorType = chunk[9];
            interlace = chunk[12];
            sawIHDR = true;
        } else if (std::memcmp(type, "PLTE", 4) == 0) {
            palette.assign(chunk, chunk + clen);
        } else if (std::memcmp(type, "tRNS", 4) == 0) {
            trns.assign(chunk, chunk + clen);
        } else if (std::memcmp(type, "IDAT", 4) == 0) {
            idat.insert(idat.end(), chunk, chunk + clen);
        } else if (std::memcmp(type, "IEND", 4) == 0) {
            break;
        }
        pos += 12 + clen;                          // length + type + data + CRC
    }

    if (!sawIHDR) { std::fprintf(stderr, "[png] missing IHDR\n"); return img; }
    if (interlace != 0) { std::fprintf(stderr, "[png] interlaced PNGs unsupported\n"); return img; }
    int channels = channelsFor(colorType);
    if (channels == 0) { std::fprintf(stderr, "[png] unsupported color type %d\n", colorType); return img; }
    // 8-bit covers RGB/RGBA/gray/palette; 1/2/4-bit only apply to gray (0) and
    // palette (3), where each pixel is a single packed sub-byte value.
    const bool subByte = (bitDepth == 1 || bitDepth == 2 || bitDepth == 4) && (colorType == 0 || colorType == 3);
    if (bitDepth != 8 && !subByte) { std::fprintf(stderr, "[png] unsupported bit depth %d\n", bitDepth); return img; }

    std::vector<std::uint8_t> raw;
    if (!inflate::zlib(idat.data(), idat.size(), raw)) {
        std::fprintf(stderr, "[png] inflate failed\n");
        return img;
    }

    // Bytes per pixel (rounded up to 1 for sub-byte formats) and bytes per row.
    const int bpp = (channels * bitDepth + 7) / 8 < 1 ? 1 : (channels * bitDepth + 7) / 8;
    const std::size_t stride = (std::size_t(width) * channels * bitDepth + 7) / 8;
    if (raw.size() < std::size_t(height) * (stride + 1)) {
        std::fprintf(stderr, "[png] short pixel data\n");
        return img;
    }

    // Undo the per-scanline filters in place into `recon`.
    std::vector<std::uint8_t> recon;
    recon.resize(std::size_t(height) * stride);
    for (int y = 0; y < height; ++y) {
        const std::uint8_t* rin = raw.data() + std::size_t(y) * (stride + 1);
        int filter = rin[0];
        const std::uint8_t* in = rin + 1;
        std::uint8_t* row = recon.data() + std::size_t(y) * stride;
        std::uint8_t* prev = (y > 0) ? recon.data() + std::size_t(y - 1) * stride : nullptr;
        for (std::size_t x = 0; x < stride; ++x) {
            int a = (x >= std::size_t(bpp)) ? row[x - bpp] : 0;
            int b = prev ? prev[x] : 0;
            int c = (prev && x >= std::size_t(bpp)) ? prev[x - bpp] : 0;
            int v = in[x];
            switch (filter) {
                case 0: break;
                case 1: v += a; break;
                case 2: v += b; break;
                case 3: v += (a + b) / 2; break;
                case 4: v += paeth(a, b, c); break;
                default: std::fprintf(stderr, "[png] bad filter %d\n", filter); return img;
            }
            row[x] = std::uint8_t(v);
        }
    }

    // Read the single packed sample for pixel x (palette index or gray level).
    const int maxVal = (1 << bitDepth) - 1;
    auto sample1 = [&](const std::uint8_t* row, int x) -> int {
        if (bitDepth == 8) return row[x];
        const int perByte = 8 / bitDepth;
        const int shift = (perByte - 1 - (x % perByte)) * bitDepth;
        return (row[x / perByte] >> shift) & maxVal;
    };

    // Expand to RGBA8.
    img.width = width; img.height = height;
    img.rgba.resize(std::size_t(width) * height * 4);
    for (int y = 0; y < height; ++y) {
        const std::uint8_t* row = recon.data() + std::size_t(y) * stride;
        std::uint8_t* out = img.rgba.data() + std::size_t(y) * width * 4;
        for (int x = 0; x < width; ++x) {
            std::uint8_t r, g, b, a = 255;
            switch (colorType) {
                case 0: r = g = b = std::uint8_t(sample1(row, x) * 255 / maxVal); break;  // gray
                case 2: r = row[x*3]; g = row[x*3+1]; b = row[x*3+2]; break;
                case 4: r = g = b = row[x*2]; a = row[x*2+1]; break;
                case 6: r = row[x*4]; g = row[x*4+1]; b = row[x*4+2]; a = row[x*4+3]; break;
                case 3: {
                    int idx = sample1(row, x);
                    if (std::size_t(idx) * 3 + 2 < palette.size()) {
                        r = palette[idx*3]; g = palette[idx*3+1]; b = palette[idx*3+2];
                    } else { r = g = b = 0; }
                    a = (std::size_t(idx) < trns.size()) ? trns[idx] : 255;
                    break;
                }
                default: r = g = b = 0; break;
            }
            out[x*4] = r; out[x*4+1] = g; out[x*4+2] = b; out[x*4+3] = a;
        }
    }
    return img;
}

/*
==================
loadPng

Read a file into memory and decode it.
==================
*/
Image loadPng(const char* path) {
    Image img;
    std::FILE* f = std::fopen(path, "rb");
    if (!f) { std::fprintf(stderr, "[png] cannot open %s\n", path); return img; }
    std::fseek(f, 0, SEEK_END);
    long sz = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (sz <= 0) { std::fclose(f); return img; }
    std::vector<std::uint8_t> buf;
    buf.resize(std::size_t(sz));
    std::size_t rd = std::fread(buf.data(), 1, buf.size(), f);
    std::fclose(f);
    if (rd != buf.size()) return img;
    return decodePng(buf.data(), buf.size());
}

} // namespace otacon
