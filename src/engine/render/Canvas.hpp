/*
===========================================================================

OTACON ENGINE
render/Canvas.hpp - a CPU pixel buffer that lives on the GPU as one texture

The portable half of the "write your own pixels" story. Rasterise into the
buffer with plain C++ - per-pixel lighting, a fractal, a heat map - then commit()
uploads the whole thing through IRenderer::updateTexture.

Because the arithmetic happens on the CPU, the result is identical on every
backend, including GL legacy, which has no programmable stage at all. Anything
that also offers a GPU shader path keeps this as its reference implementation.

Per-pixel work is O(w*h), so a canvas is usually smaller than the window and
scaled up on draw: 320x200 is ~64k pixels, which stays comfortable at 60fps
while still looking like real per-pixel shading.

===========================================================================
*/
#pragma once
#include "render/IRenderer.hpp"
#include <cstdint>
#include <vector>

namespace otacon {

class Canvas {
public:
    void init(IRenderer& r, int w, int h) {
        w_ = w; h_ = h;
        px_.assign(std::size_t(w) * h * 4, 0);
        tex_ = r.createTexture(w_, h_, px_.data(), false);
    }
    void shutdown(IRenderer& r) {
        if (tex_) { r.destroyTexture(tex_); tex_ = 0; }
        px_.clear(); w_ = h_ = 0;
    }

    int width()  const { return w_; }
    int height() const { return h_; }
    bool valid() const { return tex_ != 0; }
    TextureHandle texture() const { return tex_; }
    std::uint8_t* pixels() { return px_.data(); }

    void clear(std::uint8_t r_, std::uint8_t g, std::uint8_t b, std::uint8_t a = 255) {
        for (std::size_t i = 0; i < px_.size(); i += 4) {
            px_[i] = r_; px_[i + 1] = g; px_[i + 2] = b; px_[i + 3] = a;
        }
    }
    // No bounds check: callers loop over the canvas extents, and this is the
    // inner loop of every per-pixel sample.
    void set(int x, int y, std::uint8_t r_, std::uint8_t g, std::uint8_t b, std::uint8_t a = 255) {
        std::uint8_t* p = &px_[(std::size_t(y) * w_ + x) * 4];
        p[0] = r_; p[1] = g; p[2] = b; p[3] = a;
    }
    // Float in 0..1, clamped and converted once — the usual shading output.
    void setf(int x, int y, float r_, float g, float b, float a = 1.f) {
        auto q = [](float v) -> std::uint8_t {
            return std::uint8_t(v <= 0.f ? 0 : (v >= 1.f ? 255 : int(v * 255.f + 0.5f)));
        };
        set(x, y, q(r_), q(g), q(b), q(a));
    }

    // Push the CPU buffer to the GPU. One upload per frame, not per pixel.
    void commit(IRenderer& r) {
        if (tex_) r.updateTexture(tex_, w_, h_, px_.data());
    }
    // Draw the canvas scaled into a logical-space rect.
    void draw(IRenderer& r, float x, float y, float w, float h) const {
        if (tex_) r.drawImage(tex_, x, y, w, h);
    }

private:
    std::vector<std::uint8_t> px_;
    TextureHandle tex_ = 0;
    int w_ = 0, h_ = 0;
};

} // namespace otacon
