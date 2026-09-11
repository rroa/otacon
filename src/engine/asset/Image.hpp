// Image.hpp — a decoded 32-bit image and the in-house PNG loader.
#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>

namespace otacon {

struct Image {
    int width = 0, height = 0;
    std::vector<std::uint8_t> rgba;   // width*height*4, row-major, top-to-bottom

    bool valid() const {
        return width > 0 && height > 0 &&
               rgba.size() == std::size_t(width) * std::size_t(height) * 4;
    }
};

// Decode an in-memory PNG (8-bit; grayscale/RGB/palette/gray+alpha/RGBA;
// non-interlaced) to RGBA8. Returns an invalid Image on failure.
Image decodePng(const std::uint8_t* data, std::size_t len);

// Read a PNG file from disk and decode it.
Image loadPng(const char* path);

// Encode an RGBA8 image to a PNG file (in-house; uses uncompressed DEFLATE
// blocks, so files are larger but no compressor is needed). Returns false on
// I/O error.
bool writePng(const char* path, const Image& img);

} // namespace otacon
