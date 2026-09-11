/*
===========================================================================

OTACON ENGINE
render/Atlas.hpp - named regions inside one texture

Animator handles a strip of equal frames. This handles the other layout: a
packed sheet of differently sized pieces, each with a name.

The reason to care is in the Stress Test sample. Every texture change forces a
new draw call, so a scene drawing fifty sprites from fifty textures pays fifty
binds, while the same scene drawing them from one atlas pays one. Atlasing is
the single largest 2D draw-call win available, and it costs an indirection.

Regions are stored in pixels and converted to UVs on lookup, because pixels are
what a packer emits and what you can read off an image, while UVs are what the
renderer wants. Storing the readable form and converting once is the right way
round.

===========================================================================
*/
#pragma once
#include "render/IRenderer.hpp"
#include <cstring>
#include <string>
#include <vector>

namespace otacon {

struct AtlasRegion {
    std::string name;
    int x = 0, y = 0, w = 0, h = 0;     // pixels within the sheet
};

class Atlas {
public:
    void init(TextureHandle tex, int sheetW, int sheetH) {
        texture_ = tex; sheetW_ = sheetW > 0 ? sheetW : 1; sheetH_ = sheetH > 0 ? sheetH : 1;
    }
    void add(const char* name, int x, int y, int w, int h) {
        regions_.push_back({name ? name : "", x, y, w, h});
    }

    /*
    ==================
    addGrid

    The common case: a sheet cut into equal cells, named `prefix0`, `prefix1`...
    Saves declaring a strip cell by cell, which is where transcription errors
    come from.
    ==================
    */
    void addGrid(const char* prefix, int x0, int y0, int cellW, int cellH,
                 int cols, int rows) {
        char buf[128];
        for (int r = 0; r < rows; ++r)
            for (int c = 0; c < cols; ++c) {
                std::snprintf(buf, sizeof buf, "%s%d", prefix ? prefix : "", r * cols + c);
                add(buf, x0 + c * cellW, y0 + r * cellH, cellW, cellH);
            }
    }

    const AtlasRegion* find(const char* name) const {
        if (!name) return nullptr;
        for (const AtlasRegion& r : regions_)
            if (r.name == name) return &r;
        return nullptr;
    }
    bool has(const char* name) const { return find(name) != nullptr; }
    std::size_t size() const { return regions_.size(); }
    TextureHandle texture() const { return texture_; }
    const AtlasRegion& at(std::size_t i) const { return regions_[i]; }

    // Pixel rect -> UV window. The one conversion the whole class exists for.
    void uv(const AtlasRegion& r, float& u0, float& v0, float& u1, float& v1) const {
        u0 = float(r.x) / float(sheetW_);
        v0 = float(r.y) / float(sheetH_);
        u1 = float(r.x + r.w) / float(sheetW_);
        v1 = float(r.y + r.h) / float(sheetH_);
    }

    /*
    ==================
    draw

    Silently does nothing for an unknown name rather than drawing a wrong region
    or asserting. A missing sprite should be a hole you can see, not a crash in
    a shipping build -- and has() is there for when you want to check.
    ==================
    */
    void draw(IRenderer& r, const char* name, float dx, float dy,
              Color tint = {1, 1, 1, 1}) const {
        const AtlasRegion* reg = find(name);
        if (!reg || !texture_) return;
        float u0, v0, u1, v1; uv(*reg, u0, v0, u1, v1);
        r.drawImage(texture_, dx, dy, float(reg->w), float(reg->h), u0, v0, u1, v1, tint);
    }
    void drawScaled(IRenderer& r, const char* name, float dx, float dy, float dw, float dh,
                    Color tint = {1, 1, 1, 1}) const {
        const AtlasRegion* reg = find(name);
        if (!reg || !texture_) return;
        float u0, v0, u1, v1; uv(*reg, u0, v0, u1, v1);
        r.drawImage(texture_, dx, dy, dw, dh, u0, v0, u1, v1, tint);
    }
    void drawRotated(IRenderer& r, const char* name, float cx, float cy, float angleDeg,
                     Color tint = {1, 1, 1, 1}) const {
        const AtlasRegion* reg = find(name);
        if (!reg || !texture_) return;
        float u0, v0, u1, v1; uv(*reg, u0, v0, u1, v1);
        r.drawImageRotated(texture_, cx, cy, float(reg->w), float(reg->h), angleDeg,
                           u0, v0, u1, v1, tint);
    }

private:
    TextureHandle texture_ = 0;
    int sheetW_ = 1, sheetH_ = 1;
    std::vector<AtlasRegion> regions_;
};

} // namespace otacon
