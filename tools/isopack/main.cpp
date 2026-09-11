/*
===========================================================================

OTACON TOOLS
tools/isopack/main.cpp - downsample and atlas an isometric sprite set

Kenney's isometric prototype pack ships 240 sprites on a 256x512 canvas. Loaded
naively that is 240 * 256 * 512 * 4 = 125 MB of texture memory for one tileset,
which is not a thing you can ship. Worse, most of that is empty: a quarter-block
occupies 66x72 pixels of its 256x512 canvas and the rest is transparent.

So this does the three things an asset pipeline has to do before art is usable:

  DOWNSAMPLE   by an integer factor, with a box filter that weights colour by
               alpha. Averaging RGBA naively pulls transparent black into the
               edges and leaves every sprite with a dark fringe.

  TRIM         to the opaque bounding box, recording the offset that was
               removed. The offset goes in the manifest so the game can put the
               sprite back exactly where the artist placed it -- which is what
               preserves the pack's shared anchor, and with it the property that
               every sprite composes when drawn at one position.

  PACK         into one atlas, so a whole tileset is one texture and one bind.
               Shelf packing: sort by height, fill rows. Not optimal, but it is
               ten lines and it wastes a few percent, and an optimal packer is a
               research project.

Output is an atlas PNG plus a text manifest that otacon::Atlas can load.

  isopack <src-dir> <out.png> <out.atlas> [divisor]

Everything here goes through the engine's own PNG decoder and encoder, which is
the point: if the codec cannot survive 240 real palette PNGs from a third party,
better to find out in a tool than in a game.

===========================================================================
*/
#include "asset/Image.hpp"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <string>
#include <vector>

using otacon::Image;

namespace {

struct Sprite {
    std::string name;
    Image img;                 // trimmed
    int offX = 0, offY = 0;    // where the trim removed from, in OUTPUT pixels
    int srcW = 0, srcH = 0;    // the original canvas, also in output pixels
    int x = 0, y = 0;          // placement in the atlas
};

/*
==================
downsample

Box filter by an integer divisor, weighting colour by alpha.

The weighting is the whole point. A transparent pixel still has RGB bytes -- in
these PNGs, usually zero -- so a plain average drags the edge of every sprite
toward black. Accumulating colour * alpha and dividing by the summed alpha gives
the average of only the pixels that are actually there.
==================
*/
Image downsample(const Image& src, int n) {
    if (n <= 1) return src;
    Image out;
    out.width = (src.width + n - 1) / n;
    out.height = (src.height + n - 1) / n;
    out.rgba.assign(std::size_t(out.width) * out.height * 4, 0);

    for (int oy = 0; oy < out.height; ++oy) {
        for (int ox = 0; ox < out.width; ++ox) {
            long r = 0, g = 0, b = 0, a = 0, aw = 0;
            int count = 0;
            for (int sy = oy * n; sy < (oy + 1) * n && sy < src.height; ++sy) {
                for (int sx = ox * n; sx < (ox + 1) * n && sx < src.width; ++sx) {
                    const std::uint8_t* p = &src.rgba[(std::size_t(sy) * src.width + sx) * 4];
                    r += long(p[0]) * p[3];
                    g += long(p[1]) * p[3];
                    b += long(p[2]) * p[3];
                    a += p[3];
                    aw += p[3];
                    ++count;
                }
            }
            std::uint8_t* q = &out.rgba[(std::size_t(oy) * out.width + ox) * 4];
            if (aw > 0) {
                q[0] = std::uint8_t(r / aw);
                q[1] = std::uint8_t(g / aw);
                q[2] = std::uint8_t(b / aw);
            }
            q[3] = std::uint8_t(count > 0 ? a / count : 0);
        }
    }
    return out;
}

// The opaque bounding box. Returns false for a fully transparent image, which
// is worth skipping rather than packing a 1x1 hole.
bool opaqueBounds(const Image& img, int& x0, int& y0, int& x1, int& y1) {
    x0 = img.width; y0 = img.height; x1 = -1; y1 = -1;
    for (int y = 0; y < img.height; ++y)
        for (int x = 0; x < img.width; ++x)
            if (img.rgba[(std::size_t(y) * img.width + x) * 4 + 3] > 2) {
                if (x < x0) x0 = x;
                if (x > x1) x1 = x;
                if (y < y0) y0 = y;
                if (y > y1) y1 = y;
            }
    return x1 >= x0 && y1 >= y0;
}

Image crop(const Image& src, int x0, int y0, int w, int h) {
    Image out;
    out.width = w; out.height = h;
    out.rgba.assign(std::size_t(w) * h * 4, 0);
    for (int y = 0; y < h; ++y)
        std::memcpy(&out.rgba[std::size_t(y) * w * 4],
                    &src.rgba[(std::size_t(y0 + y) * src.width + x0) * 4],
                    std::size_t(w) * 4);
    return out;
}

void blit(Image& dst, const Image& src, int dx, int dy) {
    for (int y = 0; y < src.height; ++y)
        std::memcpy(&dst.rgba[(std::size_t(dy + y) * dst.width + dx) * 4],
                    &src.rgba[std::size_t(y) * src.width * 4],
                    std::size_t(src.width) * 4);
}

std::vector<std::string> pngsIn(const char* dir) {
    std::vector<std::string> out;
    DIR* d = opendir(dir);
    if (!d) return out;
    while (dirent* e = readdir(d)) {
        const std::string n = e->d_name;
        if (n.size() > 4 && n.compare(n.size() - 4, 4, ".png") == 0) out.push_back(n);
    }
    closedir(d);
    std::sort(out.begin(), out.end());
    return out;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 4) {
        std::fprintf(stderr, "usage: isopack <src-dir> <out.png> <out.atlas> [divisor]\n");
        return 1;
    }
    const char* srcDir = argv[1];
    const char* outPng = argv[2];
    const char* outManifest = argv[3];
    const int divisor = argc > 4 ? std::atoi(argv[4]) : 4;

    const std::vector<std::string> files = pngsIn(srcDir);
    if (files.empty()) {
        std::fprintf(stderr, "isopack: no PNGs in %s\n", srcDir);
        return 1;
    }
    std::printf("isopack: %zu files from %s, divisor %d\n", files.size(), srcDir, divisor);

    std::vector<Sprite> sprites;
    std::size_t rawBytes = 0, keptBytes = 0;
    for (const std::string& f : files) {
        const Image src = otacon::loadPng((std::string(srcDir) + "/" + f).c_str());
        if (!src.valid()) { std::fprintf(stderr, "  skip (decode failed): %s\n", f.c_str()); continue; }
        rawBytes += std::size_t(src.width) * src.height * 4;

        const Image small = downsample(src, divisor);
        int x0, y0, x1, y1;
        if (!opaqueBounds(small, x0, y0, x1, y1)) continue;   // fully transparent

        Sprite s;
        s.name = f.substr(0, f.size() - 4);
        s.img = crop(small, x0, y0, x1 - x0 + 1, y1 - y0 + 1);
        s.offX = x0; s.offY = y0;
        s.srcW = small.width; s.srcH = small.height;
        keptBytes += std::size_t(s.img.width) * s.img.height * 4;
        sprites.push_back(std::move(s));
    }
    if (sprites.empty()) { std::fprintf(stderr, "isopack: nothing to pack\n"); return 1; }

    // Shelf packing: tallest first, fill rows left to right.
    std::vector<Sprite*> order;
    order.reserve(sprites.size());
    for (Sprite& s : sprites) order.push_back(&s);
    std::sort(order.begin(), order.end(),
              [](const Sprite* a, const Sprite* b) { return a->img.height > b->img.height; });

    int atlasW = 1024;
    while (true) {
        int x = 0, y = 0, shelfH = 0;
        bool fits = true;
        for (Sprite* s : order) {
            if (x + s->img.width + 1 > atlasW) { x = 0; y += shelfH + 1; shelfH = 0; }
            s->x = x; s->y = y;
            x += s->img.width + 1;
            shelfH = std::max(shelfH, s->img.height);
        }
        const int used = y + shelfH;
        // Keep it roughly square: a very tall thin atlas wastes as much as a
        // short wide one, and some drivers dislike extreme aspect ratios.
        if (used <= atlasW * 2 || atlasW >= 8192) { fits = true; }
        if (fits) { break; }
        atlasW *= 2;
    }
    int atlasH = 0;
    {
        int x = 0, y = 0, shelfH = 0;
        for (Sprite* s : order) {
            if (x + s->img.width + 1 > atlasW) { x = 0; y += shelfH + 1; shelfH = 0; }
            s->x = x; s->y = y;
            x += s->img.width + 1;
            shelfH = std::max(shelfH, s->img.height);
        }
        atlasH = y + shelfH;
    }

    Image atlas;
    atlas.width = atlasW; atlas.height = atlasH;
    atlas.rgba.assign(std::size_t(atlasW) * atlasH * 4, 0);
    for (const Sprite& s : sprites) blit(atlas, s.img, s.x, s.y);

    if (!otacon::writePng(outPng, atlas)) {
        std::fprintf(stderr, "isopack: could not write %s\n", outPng);
        return 1;
    }

    std::FILE* m = std::fopen(outManifest, "w");
    if (!m) { std::fprintf(stderr, "isopack: could not write %s\n", outManifest); return 1; }
    std::fprintf(m, "# otacon atlas manifest\n");
    std::fprintf(m, "# atlas <width> <height> <sourceCanvasW> <sourceCanvasH>\n");
    std::fprintf(m, "# region <name> <x> <y> <w> <h> <offsetX> <offsetY>\n");
    std::fprintf(m, "#\n# offsetX/offsetY are where the trim removed from, so a sprite drawn at\n");
    std::fprintf(m, "# (pos + offset) sits exactly where it did on the original canvas --\n");
    std::fprintf(m, "# which is what preserves this pack's shared anchor.\n");
    std::fprintf(m, "atlas %d %d %d %d\n", atlasW, atlasH, sprites[0].srcW, sprites[0].srcH);
    for (const Sprite& s : sprites)
        std::fprintf(m, "region %s %d %d %d %d %d %d\n",
                     s.name.c_str(), s.x, s.y, s.img.width, s.img.height, s.offX, s.offY);
    std::fclose(m);

    std::printf("  packed   %zu sprites into %dx%d\n", sprites.size(), atlasW, atlasH);
    std::printf("  raw      %.1f MB  (%zu sprites at full canvas)\n", double(rawBytes) / (1024 * 1024), files.size());
    std::printf("  trimmed  %.1f MB\n", double(keptBytes) / (1024 * 1024));
    std::printf("  atlas    %.1f MB in VRAM\n", double(std::size_t(atlasW) * atlasH * 4) / (1024 * 1024));
    std::printf("  wrote    %s + %s\n", outPng, outManifest);
    return 0;
}
