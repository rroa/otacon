/*
===========================================================================

OTACON ENGINE
asset/Inflate.hpp - DEFLATE decoder

(RFC 1950). Written from scratch (no zlib/miniz) so the PNG loader is fully
our own. The algorithm: read LSB-first bits, decode Huffman-coded symbols
(literals + length/distance pairs), and reconstruct the byte stream via an
LZ77 sliding window.

===========================================================================
*/
#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>

namespace otacon::inflate {

// Decompress a raw DEFLATE stream. Returns false on malformed input.
bool raw(const std::uint8_t* data, std::size_t len, std::vector<std::uint8_t>& out);

// Decompress a zlib stream (2-byte header + DEFLATE + adler32 trailer). The
// adler32 checksum is parsed past but not verified.
bool zlib(const std::uint8_t* data, std::size_t len, std::vector<std::uint8_t>& out);

} // namespace otacon::inflate
