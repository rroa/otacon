#include "render/IRenderer.hpp"
#include "platform/Window.hpp"
#include "asset/Image.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace otacon {

TextureHandle IRenderer::createTexture(const Image& img, bool repeat) {
    if (!img.valid()) return 0;
    return createTexture(img.width, img.height, img.rgba.data(), repeat);
}

void IRenderer::drawImage(TextureHandle tex, float dx, float dy, float dw, float dh,
                          float u0, float v0, float u1, float v1, Color tint) {
    if (!tex) return;
    texScratch_.clear();
    texScratch_.push_back({dx,      dy,      u0, v0, tint});
    texScratch_.push_back({dx + dw, dy,      u1, v0, tint});
    texScratch_.push_back({dx + dw, dy + dh, u1, v1, tint});
    texScratch_.push_back({dx,      dy,      u0, v0, tint});
    texScratch_.push_back({dx + dw, dy + dh, u1, v1, tint});
    texScratch_.push_back({dx,      dy + dh, u0, v1, tint});
    emitTex(texScratch_.data(), texScratch_.size(), tex);
}

void IRenderer::drawImageRotated(TextureHandle tex, float cx, float cy, float w, float h,
                                 float angleDeg, float u0, float v0, float u1, float v1, Color tint) {
    if (!tex) return;
    float r = angleDeg * 3.14159265f / 180.f, s = std::sin(r), co = std::cos(r);
    float hw = w * 0.5f, hh = h * 0.5f;
    auto rot = [&](float dx, float dy, float u, float v) -> TexVertex {
        return {cx + dx * co - dy * s, cy + dx * s + dy * co, u, v, tint};
    };
    TexVertex a = rot(-hw, -hh, u0, v0), b = rot(hw, -hh, u1, v0),
              d = rot(hw, hh, u1, v1), e = rot(-hw, hh, u0, v1);
    texScratch_.clear();
    texScratch_.push_back(a); texScratch_.push_back(b); texScratch_.push_back(d);
    texScratch_.push_back(a); texScratch_.push_back(d); texScratch_.push_back(e);
    emitTex(texScratch_.data(), texScratch_.size(), tex);
}

bool IRenderer::init(IWindow* window, int logicalW, int logicalH) {
    logicalW_ = logicalW; logicalH_ = logicalH;
    scratch_.reserve(4096);
    return onInit(window);
}

void IRenderer::letterbox(int fbw, int fbh, int& x, int& y, int& w, int& h) const {
    if (logicalW_ <= 0 || logicalH_ <= 0 || fbw <= 0 || fbh <= 0) {
        x = 0; y = 0; w = fbw; h = fbh; return;
    }
    // Fit logical into the framebuffer at the largest uniform scale, then center.
    const double s = std::min(double(fbw) / logicalW_, double(fbh) / logicalH_);
    w = int(logicalW_ * s + 0.5);
    h = int(logicalH_ * s + 0.5);
    x = (fbw - w) / 2;
    y = (fbh - h) / 2;
}
void IRenderer::beginFrame(Color clear) {
    prevDrawCalls_ = frameDrawCalls_; prevVerts_ = frameVerts_;   // expose last frame's totals
    frameDrawCalls_ = 0; frameVerts_ = 0;
    onBeginFrame(clear);
}
void IRenderer::endFrame() { onEndFrame(); }

void IRenderer::emit(const Vertex* v, std::size_t n) {
    ++frameDrawCalls_; frameVerts_ += n; submitTriangles(v, n);
}
void IRenderer::emitTex(const TexVertex* v, std::size_t n, TextureHandle t) {
    ++frameDrawCalls_; frameVerts_ += n; submitTextured(v, n, t);
}

void IRenderer::pushQuad(float x0, float y0, float x1, float y1, Color c) {
    scratch_.push_back({x0, y0, c}); scratch_.push_back({x1, y0, c}); scratch_.push_back({x1, y1, c});
    scratch_.push_back({x0, y0, c}); scratch_.push_back({x1, y1, c}); scratch_.push_back({x0, y1, c});
}

void IRenderer::fillRect(float x, float y, float w, float h, Color c) {
    scratch_.clear();
    pushQuad(x, y, x + w, y + h, c);
    emit(scratch_.data(), scratch_.size());
}

void IRenderer::fillRotatedRect(float cx, float cy, float w, float h, float angleDeg, Color c) {
    float r = angleDeg * 3.14159265f / 180.f, s = std::sin(r), co = std::cos(r);
    float hw = w * 0.5f, hh = h * 0.5f;
    auto rot = [&](float dx, float dy) -> Vertex { return {cx + dx * co - dy * s, cy + dx * s + dy * co, c}; };
    Vertex a = rot(-hw, -hh), b = rot(hw, -hh), d = rot(hw, hh), e = rot(-hw, hh);
    scratch_.clear();
    scratch_.push_back(a); scratch_.push_back(b); scratch_.push_back(d);
    scratch_.push_back(a); scratch_.push_back(d); scratch_.push_back(e);
    emit(scratch_.data(), scratch_.size());
}

void IRenderer::drawRectOutline(float x, float y, float w, float h, Color c, float t) {
    scratch_.clear();
    pushQuad(x, y, x + w, y + t, c);                 // top
    pushQuad(x, y + h - t, x + w, y + h, c);         // bottom
    pushQuad(x, y, x + t, y + h, c);                 // left
    pushQuad(x + w - t, y, x + w, y + h, c);         // right
    emit(scratch_.data(), scratch_.size());
}

void IRenderer::drawLine(float x0, float y0, float x1, float y1, Color c, float t) {
    float dx = x1 - x0, dy = y1 - y0;
    float len = std::sqrt(dx * dx + dy * dy);
    if (len < 1e-6f) return;
    float nx = -dy / len * (t * 0.5f), ny = dx / len * (t * 0.5f);   // half-thickness normal
    scratch_.clear();
    scratch_.push_back({x0 + nx, y0 + ny, c}); scratch_.push_back({x1 + nx, y1 + ny, c}); scratch_.push_back({x1 - nx, y1 - ny, c});
    scratch_.push_back({x0 + nx, y0 + ny, c}); scratch_.push_back({x1 - nx, y1 - ny, c}); scratch_.push_back({x0 - nx, y0 - ny, c});
    emit(scratch_.data(), scratch_.size());
}

// ---- 3x5 bitmap font -------------------------------------------------------
// Each glyph is 5 rows of 3 bits (bit2=left .. bit0=right), top row first.
static std::array<std::uint8_t, 5> glyph(char ch) {
    if (ch >= 'a' && ch <= 'z') ch = char(ch - 'a' + 'A');
    switch (ch) {
        case '0': return {0b111,0b101,0b101,0b101,0b111};
        case '1': return {0b010,0b110,0b010,0b010,0b111};
        case '2': return {0b111,0b001,0b111,0b100,0b111};
        case '3': return {0b111,0b001,0b111,0b001,0b111};
        case '4': return {0b101,0b101,0b111,0b001,0b001};
        case '5': return {0b111,0b100,0b111,0b001,0b111};
        case '6': return {0b111,0b100,0b111,0b101,0b111};
        case '7': return {0b111,0b001,0b010,0b010,0b010};
        case '8': return {0b111,0b101,0b111,0b101,0b111};
        case '9': return {0b111,0b101,0b111,0b001,0b111};
        case 'A': return {0b111,0b101,0b111,0b101,0b101};
        case 'B': return {0b110,0b101,0b110,0b101,0b110};
        case 'C': return {0b111,0b100,0b100,0b100,0b111};
        case 'D': return {0b110,0b101,0b101,0b101,0b110};
        case 'E': return {0b111,0b100,0b110,0b100,0b111};
        case 'F': return {0b111,0b100,0b110,0b100,0b100};
        case 'G': return {0b111,0b100,0b101,0b101,0b111};
        case 'H': return {0b101,0b101,0b111,0b101,0b101};
        case 'I': return {0b111,0b010,0b010,0b010,0b111};
        case 'J': return {0b001,0b001,0b001,0b101,0b111};
        case 'K': return {0b101,0b101,0b110,0b101,0b101};
        case 'L': return {0b100,0b100,0b100,0b100,0b111};
        case 'M': return {0b101,0b111,0b111,0b101,0b101};
        case 'N': return {0b101,0b111,0b111,0b111,0b101};
        case 'O': return {0b111,0b101,0b101,0b101,0b111};
        case 'P': return {0b111,0b101,0b111,0b100,0b100};
        case 'Q': return {0b111,0b101,0b101,0b111,0b011};
        case 'R': return {0b110,0b101,0b110,0b101,0b101};
        case 'S': return {0b111,0b100,0b111,0b001,0b111};
        case 'T': return {0b111,0b010,0b010,0b010,0b010};
        case 'U': return {0b101,0b101,0b101,0b101,0b111};
        case 'V': return {0b101,0b101,0b101,0b101,0b010};
        case 'W': return {0b101,0b101,0b111,0b111,0b101};
        case 'X': return {0b101,0b101,0b010,0b101,0b101};
        case 'Y': return {0b101,0b101,0b010,0b010,0b010};
        case 'Z': return {0b111,0b001,0b010,0b100,0b111};
        case '.': return {0b000,0b000,0b000,0b000,0b010};
        case ',': return {0b000,0b000,0b000,0b010,0b100};
        case ':': return {0b000,0b010,0b000,0b010,0b000};
        case '-': return {0b000,0b000,0b111,0b000,0b000};
        case '+': return {0b000,0b010,0b111,0b010,0b000};
        case '=': return {0b000,0b111,0b000,0b111,0b000};
        case '/': return {0b001,0b001,0b010,0b100,0b100};
        case '%': return {0b101,0b001,0b010,0b100,0b101};
        case '(': return {0b001,0b010,0b010,0b010,0b001};
        case ')': return {0b100,0b010,0b010,0b010,0b100};
        case '[': return {0b011,0b010,0b010,0b010,0b011};
        case ']': return {0b110,0b010,0b010,0b010,0b110};
        case '!': return {0b010,0b010,0b010,0b000,0b010};
        case '<': return {0b001,0b010,0b100,0b010,0b001};
        case '>': return {0b100,0b010,0b001,0b010,0b100};
        case '#': return {0b101,0b111,0b101,0b111,0b101};
        // Punctuation the overlays and the samples' source listings need. A 3x5
        // cell cannot be subtle about it, but a legible approximation beats the
        // blank the default case would otherwise draw.
        case '*': return {0b000,0b101,0b010,0b101,0b000};
        case ';': return {0b000,0b010,0b000,0b010,0b100};
        case '_': return {0b000,0b000,0b000,0b000,0b111};
        case '\'': return {0b010,0b010,0b000,0b000,0b000};
        case '"': return {0b101,0b101,0b000,0b000,0b000};
        case '?': return {0b111,0b001,0b011,0b000,0b010};
        case '{': return {0b011,0b010,0b110,0b010,0b011};
        case '}': return {0b110,0b010,0b011,0b010,0b110};
        case '|': return {0b010,0b010,0b010,0b010,0b010};
        case '^': return {0b010,0b101,0b000,0b000,0b000};
        case '~': return {0b000,0b000,0b011,0b110,0b000};
        case '&': return {0b110,0b110,0b111,0b101,0b011};
        case '@': return {0b111,0b101,0b111,0b100,0b111};
        case '$': return {0b011,0b110,0b011,0b110,0b010};
        case '\\': return {0b100,0b100,0b010,0b001,0b001};
        case '`': return {0b100,0b010,0b000,0b000,0b000};
        default:  return {0,0,0,0,0};   // space / unknown
    }
}

float IRenderer::textWidth(const char* text, float scale) const {
    return float(std::strlen(text)) * 4.f * scale;   // 3px glyph + 1px gap
}

float IRenderer::drawText(const char* text, float x, float y, float scale, Color c) {
    scratch_.clear();
    float cx = x;
    for (const char* p = text; *p; ++p) {
        auto g = glyph(*p);
        for (int row = 0; row < 5; ++row)
            for (int col = 0; col < 3; ++col)
                if (g[row] & (1 << (2 - col))) {
                    float px = cx + col * scale, py = y + row * scale;
                    pushQuad(px, py, px + scale, py + scale, c);
                }
        cx += 4 * scale;
    }
    if (!scratch_.empty()) emit(scratch_.data(), scratch_.size());
    return cx - x;
}

void IRenderer::tryCapture() {
    if (!capturePath_) return;
    int w = 0, h = 0;
    std::vector<std::uint8_t> px;
    if (readPixels(w, h, px)) {
        Image img; img.width = w; img.height = h; img.rgba = std::move(px);
        if (writePng(capturePath_, img)) std::printf("[shot] wrote %s (%dx%d)\n", capturePath_, w, h);
    } else {
        std::fprintf(stderr, "[shot] backend can't read pixels\n");
    }
    capturePath_ = nullptr;
}

} // namespace otacon
