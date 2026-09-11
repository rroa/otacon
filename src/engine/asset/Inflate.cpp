#include "asset/Inflate.hpp"

namespace otacon::inflate {
namespace {

// LSB-first bit reader over a byte buffer.
struct BitReader {
    const std::uint8_t* data;
    std::size_t len;
    std::size_t pos = 0;      // byte position
    std::uint32_t bitbuf = 0; // accumulated bits
    int bitcount = 0;         // valid bits in bitbuf
    bool error = false;

    int getBit() {
        if (bitcount == 0) {
            if (pos >= len) { error = true; return 0; }
            bitbuf = data[pos++];
            bitcount = 8;
        }
        int b = bitbuf & 1;
        bitbuf >>= 1;
        --bitcount;
        return b;
    }
    int getBits(int n) {        // n bits, LSB first
        int v = 0;
        for (int i = 0; i < n; ++i) v |= getBit() << i;
        return v;
    }
    void alignByte() { bitcount = 0; }
};

// Canonical Huffman table (counts + symbols), "puff"-style.
struct Huffman {
    int counts[16] = {0};
    std::vector<int> symbols;

    void build(const int* lengths, int n) {
        for (int i = 0; i < 16; ++i) counts[i] = 0;
        for (int i = 0; i < n; ++i) counts[lengths[i]]++;
        counts[0] = 0;
        int offsets[16]; offsets[0] = 0; offsets[1] = 0;
        for (int len = 1; len < 15; ++len) offsets[len + 1] = offsets[len] + counts[len];
        symbols.assign(n, 0);
        for (int i = 0; i < n; ++i)
            if (lengths[i]) symbols[offsets[lengths[i]]++] = i;
    }
    int decode(BitReader& br) const {
        int code = 0, first = 0, index = 0;
        for (int len = 1; len <= 15; ++len) {
            code |= br.getBit();
            int count = counts[len];
            if (code - first < count) return symbols[index + (code - first)];
            index += count;
            first += count;
            first <<= 1;
            code <<= 1;
        }
        return -1;   // invalid
    }
};

// RFC 1951 length / distance tables.
const int kLenBase[29]   = {3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258};
const int kLenExtra[29]  = {0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0};
const int kDistBase[30]  = {1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577};
const int kDistExtra[30] = {0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13};
const int kCLOrder[19]   = {16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15};

void buildFixed(Huffman& lit, Huffman& dist) {
    int litLen[288];
    for (int i = 0;   i < 144; ++i) litLen[i] = 8;
    for (int i = 144; i < 256; ++i) litLen[i] = 9;
    for (int i = 256; i < 280; ++i) litLen[i] = 7;
    for (int i = 280; i < 288; ++i) litLen[i] = 8;
    lit.build(litLen, 288);
    int distLen[30];
    for (int i = 0; i < 30; ++i) distLen[i] = 5;
    dist.build(distLen, 30);
}

bool inflateBlock(BitReader& br, const Huffman& lit, const Huffman& dist,
                  std::vector<std::uint8_t>& out) {
    for (;;) {
        int sym = lit.decode(br);
        if (br.error || sym < 0) return false;
        if (sym == 256) return true;            // end of block
        if (sym < 256) {
            out.push_back(std::uint8_t(sym));
        } else {
            sym -= 257;
            if (sym >= 29) return false;
            int length = kLenBase[sym] + br.getBits(kLenExtra[sym]);
            int dsym = dist.decode(br);
            if (dsym < 0 || dsym >= 30) return false;
            int distance = kDistBase[dsym] + br.getBits(kDistExtra[dsym]);
            if (std::size_t(distance) > out.size()) return false;
            std::size_t from = out.size() - distance;
            for (int i = 0; i < length; ++i) out.push_back(out[from + i]);   // handles overlap
        }
        if (br.error) return false;
    }
}

bool readDynamic(BitReader& br, Huffman& lit, Huffman& dist) {
    int hlit  = br.getBits(5) + 257;
    int hdist = br.getBits(5) + 1;
    int hclen = br.getBits(4) + 4;
    int clLen[19] = {0};
    for (int i = 0; i < hclen; ++i) clLen[kCLOrder[i]] = br.getBits(3);
    Huffman clTree; clTree.build(clLen, 19);

    int lengths[288 + 32] = {0};
    int n = 0, total = hlit + hdist;
    while (n < total) {
        int sym = clTree.decode(br);
        if (br.error || sym < 0) return false;
        if (sym < 16) {
            lengths[n++] = sym;
        } else if (sym == 16) {
            if (n == 0) return false;
            int rep = br.getBits(2) + 3;
            int prev = lengths[n - 1];
            while (rep-- && n < total) lengths[n++] = prev;
        } else if (sym == 17) {
            int rep = br.getBits(3) + 3;
            while (rep-- && n < total) lengths[n++] = 0;
        } else { // 18
            int rep = br.getBits(7) + 11;
            while (rep-- && n < total) lengths[n++] = 0;
        }
    }
    lit.build(lengths, hlit);
    dist.build(lengths + hlit, hdist);
    return true;
}

} // namespace

bool raw(const std::uint8_t* data, std::size_t len, std::vector<std::uint8_t>& out) {
    BitReader br{data, len};
    bool final = false;
    while (!final) {
        final = br.getBit();
        int type = br.getBits(2);
        if (br.error) return false;
        if (type == 0) {                 // stored / uncompressed
            br.alignByte();
            if (br.pos + 4 > len) return false;
            int blen = data[br.pos] | (data[br.pos + 1] << 8);
            br.pos += 4;                 // skip LEN + NLEN
            if (br.pos + blen > len) return false;
            out.insert(out.end(), data + br.pos, data + br.pos + blen);
            br.pos += blen;
        } else if (type == 1) {          // fixed Huffman
            Huffman lit, dist; buildFixed(lit, dist);
            if (!inflateBlock(br, lit, dist, out)) return false;
        } else if (type == 2) {          // dynamic Huffman
            Huffman lit, dist;
            if (!readDynamic(br, lit, dist)) return false;
            if (!inflateBlock(br, lit, dist, out)) return false;
        } else {
            return false;                // reserved
        }
    }
    return true;
}

bool zlib(const std::uint8_t* data, std::size_t len, std::vector<std::uint8_t>& out) {
    if (len < 6) return false;
    // byte0: CMF (compression method/flags), byte1: FLG. Method must be 8.
    if ((data[0] & 0x0F) != 8) return false;
    std::size_t start = 2;
    if (data[1] & 0x20) start += 4;      // FDICT present -> skip 4-byte dict id
    return raw(data + start, len - start, out);
}

} // namespace otacon::inflate
