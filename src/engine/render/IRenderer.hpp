/*
===========================================================================

OTACON ENGINE
render/IRenderer.hpp - the renderer seam

Design choice that makes "identical behavior across backends" provable:
a backend implements ONLY triangle rasterization (submitTriangles) plus
frame lifecycle. Every higher-level shape — filled rect, oriented line,
rectangle outline, bitmap text — is tessellated once here in the base class,
so all three backends draw byte-for-byte identical geometry. The only thing
that differs per backend is *how* those triangles reach the screen.

===========================================================================
*/
#pragma once
#include "render/RenderTypes.hpp"
#include <cstddef>
#include <cstdio>
#include <vector>

namespace otacon {

class IWindow;

class IRenderer {
public:
    virtual ~IRenderer() = default;

    // logicalW/H define the virtual resolution everything is drawn in (480x320).
    bool init(IWindow* window, int logicalW, int logicalH);
    virtual void shutdown() = 0;

    void beginFrame(Color clear);
    void endFrame();

    virtual void setWireframe(bool on) = 0;
    virtual const char* name() const = 0;


/*
=============================================================================

                                  TEXTURES

=============================================================================
*/
    // ---- Textures ----------------------------------------------------------
    // `repeat` selects wrap mode: REPEAT lets UVs > 1 tile the texture (used to
    // tile wall bricks across a building in one draw); otherwise CLAMP_TO_EDGE.
    virtual TextureHandle createTexture(int w, int h, const std::uint8_t* rgba, bool repeat = false) = 0;
    virtual void          destroyTexture(TextureHandle t) = 0;
    TextureHandle createTexture(const struct Image& img, bool repeat = false);   // (IRenderer.cpp)

    // Replace the pixels of an existing texture, which must keep the same w/h.
    // This is the upload path for anything rasterized on the CPU each frame —
    // software lighting, the shader playground's portable path, a heat map. It
    // exists because re-creating a texture per frame would churn GPU objects.
    virtual void updateTexture(TextureHandle t, int w, int h, const std::uint8_t* rgba) = 0;

    // ---- Fragment effects (optional backend capability) --------------------
    // A backend with a programmable stage can compile a user fragment shader
    // that replaces the built-in textured shader for subsequent textured draws.
    // The engine keeps ownership of the vertex stage so the logical->clip
    // projection stays byte-identical across backends; only the fragment stage
    // is yours. The contract an effect is compiled against:
    //
    //     in  vec2 vUV;          // 0..1 across the drawn quad
    //     in  vec4 vTint;        // per-vertex tint
    //     out vec4 oColor;
    //     uniform sampler2D uTex;
    //     uniform vec2  uResolution;   // logical size, set by the engine
    //     uniform float uTime;         // seconds, set via setEffectTime()
    //
    // supportsShaders() is false wherever no programmable stage is reachable:
    // GL legacy is fixed-function, and the Vulkan backend ships pre-built SPIR-V
    // with no runtime compiler. Anything built on this MUST therefore also carry
    // a CPU path, so no sample becomes backend-exclusive.
    virtual bool supportsShaders() const { return false; }
    // Compile `fragmentSrc`. Returns 0 on failure, writing the driver's log into
    // `log` (which is what makes a live-editing playground usable).
    virtual ShaderHandle createEffect(const char* fragmentSrc, char* log, std::size_t logSize) {
        (void)fragmentSrc;
        if (log && logSize) std::snprintf(log, logSize, "%s has no programmable stage", name());
        return 0;
    }
    virtual void destroyEffect(ShaderHandle e) { (void)e; }
    // Bind an effect for subsequent textured draws; 0 restores the built-in one.
    virtual void useEffect(ShaderHandle e) { (void)e; }
    virtual void setEffectUniform(const char* name_, float x, float y = 0, float z = 0, float w = 0) {
        (void)name_; (void)x; (void)y; (void)z; (void)w;
    }
    virtual void setEffectTime(float seconds) { (void)seconds; }

    // Draw a sub-rectangle [u0,v0 .. u1,v1] of a texture into the logical-space
    // destination rect, multiplied by `tint`. UVs default to the whole image.
    void drawImage(TextureHandle tex, float dx, float dy, float dw, float dh,
                   float u0 = 0, float v0 = 0, float u1 = 1, float v1 = 1,
                   Color tint = {1, 1, 1, 1});

    // Same, but the w x h quad is rotated by `angleDeg` around its centre
    // (cx,cy) — for textured particles that spin.
    void drawImageRotated(TextureHandle tex, float cx, float cy, float w, float h,
                          float angleDeg, float u0 = 0, float v0 = 0, float u1 = 1, float v1 = 1,
                          Color tint = {1, 1, 1, 1});


/*
=============================================================================

                                 PRIMITIVES

=============================================================================
*/
    // ---- High-level 2D primitives (implemented in IRenderer.cpp) ----------
    void fillRect(float x, float y, float w, float h, Color c);
    // Filled rect rotated by `angleDeg` around its centre (cx,cy) — for particles.
    void fillRotatedRect(float cx, float cy, float w, float h, float angleDeg, Color c);
    void drawRectOutline(float x, float y, float w, float h, Color c, float thickness = 1.f);
    void drawLine(float x0, float y0, float x1, float y1, Color c, float thickness = 1.f);
    // 3x5 bitmap text; scale 1 => 3x5 px glyphs. Returns advance width in px.
    float drawText(const char* text, float x, float y, float scale, Color c);
    float textWidth(const char* text, float scale) const;

    int logicalWidth()  const { return logicalW_; }
    int logicalHeight() const { return logicalH_; }

    // Per-frame draw statistics (previous frame's totals).
    int         drawCalls()   const { return prevDrawCalls_; }
    std::size_t vertexCount() const { return prevVerts_; }

    // Request a screenshot: the *next* endFrame saves the framebuffer as a PNG.
    // (Deferred so the backend can read the image at a valid point in the frame
    // — e.g. Vulkan must copy before present, while the image is still acquired.)
    void captureNextFrame(const char* path) { capturePath_ = path; }

protected:

/*
=============================================================================

                                BACKEND HOOKS

=============================================================================
*/
    // Backend hooks.
    virtual bool onInit(IWindow* window) = 0;
    virtual void onBeginFrame(Color clear) = 0;
    virtual void onEndFrame() = 0;
    // Rasterize a triangle list given in logical screen coordinates.
    virtual void submitTriangles(const Vertex* verts, std::size_t count) = 0;
    // Rasterize a textured triangle list with the given texture bound.
    virtual void submitTextured(const TexVertex* verts, std::size_t count, TextureHandle tex) = 0;
    // Optional backend pixel readback for screenshots (RGBA8, top-to-bottom).
    virtual bool readPixels(int& w, int& h, std::vector<std::uint8_t>& rgba) { (void)w; (void)h; (void)rgba; return false; }
    // Called by a backend's onEndFrame at a valid point: if a capture is pending,
    // read pixels and write the PNG. (Vulkan handles capture inline instead.)
    void tryCapture();
    const char* capturePath_ = nullptr;

    int logicalW_ = 480, logicalH_ = 320;

    // Largest centered sub-rect of the (fbw x fbh) framebuffer whose aspect
    // matches logicalW_:logicalH_ — i.e. the letterboxed draw area. When the
    // window already matches the logical aspect this returns the full
    // framebuffer (so same-aspect games are unaffected); otherwise it pillar-
    // or letter-boxes so logical content is never stretched. Backends clear the
    // whole framebuffer, then restrict the viewport to this rect.
    void letterbox(int fbw, int fbh, int& x, int& y, int& w, int& h) const;

    // Scratch buffer reused each primitive call to avoid per-call allocation.
    std::vector<Vertex> scratch_;
    std::vector<TexVertex> texScratch_;
    void pushQuad(float x0, float y0, float x1, float y1, Color c);

    // Draw-stat counters (counted in the base, so every backend reports them).
    void emit(const Vertex* v, std::size_t n);
    void emitTex(const TexVertex* v, std::size_t n, TextureHandle t);
    int         frameDrawCalls_ = 0, prevDrawCalls_ = 0;
    std::size_t frameVerts_ = 0, prevVerts_ = 0;
};

// Selects the compiled-in backend (render/RendererFactory.cpp).
IRenderer* createRenderer();
const char* graphicsBackendName();

} // namespace otacon
