/*
===========================================================================

OTACON ENGINE
render/gl_legacy/GLLegacyRenderer.cpp - OpenGL fixed-function backend

The same triangles as the modern backend, through glBegin/glVertex and
glOrtho instead of shaders and buffers.

It exists for two reasons. It still runs where a core profile is not
available, and it is the proof that the renderer seam is drawn in the right
place: a backend with no programmable stage at all, and no vertex buffers,
implements the same interface and produces the same picture. It is also why
supportsShaders() has to be a capability question rather than an assumption.

===========================================================================
*/
#include "render/IRenderer.hpp"
#include "platform/Window.hpp"
#include <cstring>

#if defined(__APPLE__)
#  define GL_SILENCE_DEPRECATION 1
#  include <OpenGL/gl.h>
#elif defined(_WIN32)
// <GL/gl.h> on Windows is not self-contained: it uses WINGDIAPI and APIENTRY
// without defining them, so <windows.h> has to come first or the header will
// not compile. NOMINMAX and WIN32_LEAN_AND_MEAN keep it from dragging in the
// min/max macros and half of the Win32 API along with it.
#  define WIN32_LEAN_AND_MEAN 1
#  define NOMINMAX 1
#  include <windows.h>
#  include <GL/gl.h>
#else
#  include <GL/gl.h>
#endif

namespace otacon {

class GLLegacyRenderer final : public IRenderer {
public:
    void shutdown() override {}
    const char* name() const override { return "OpenGL Legacy (fixed-function 2.1)"; }
    void setWireframe(bool on) override { wireframe_ = on; }

    TextureHandle createTexture(int w, int h, const std::uint8_t* rgba, bool repeat) override {
        GLuint tex = 0; GLint wrap = repeat ? GL_REPEAT : GL_CLAMP_TO_EDGE;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
        return TextureHandle(tex);
    }
    void updateTexture(TextureHandle t, int w, int h, const std::uint8_t* rgba) override {
        if (!t || !rgba) return;
        glBindTexture(GL_TEXTURE_2D, GLuint(t));
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    }
    // supportsShaders() stays false: this backend is fixed-function immediate
    // mode, so there is no programmable stage to compile an effect into.
    void destroyTexture(TextureHandle t) override {
        if (t) { GLuint id = t; glDeleteTextures(1, &id); }
    }
    bool readPixels(int& w, int& h, std::vector<std::uint8_t>& rgba) override {
        int fbw = 0, fbh = 0; window_->framebufferSize(fbw, fbh);
        w = fbw; h = fbh;
        rgba.assign(std::size_t(w) * h * 4, 0);
        glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
        std::vector<std::uint8_t> tmp(std::size_t(w) * 4);   // flip bottom-left -> top-left
        for (int y = 0; y < h / 2; ++y) {
            std::uint8_t* a = rgba.data() + std::size_t(y) * w * 4;
            std::uint8_t* b = rgba.data() + std::size_t(h - 1 - y) * w * 4;
            std::memcpy(tmp.data(), a, tmp.size());
            std::memcpy(a, b, tmp.size());
            std::memcpy(b, tmp.data(), tmp.size());
        }
        return true;
    }

protected:
    bool onInit(IWindow* window) override {
        window_ = window;
        window_->makeContextCurrent();
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        return true;
    }
    void onBeginFrame(Color clear) override {
        int fbw = 0, fbh = 0; window_->framebufferSize(fbw, fbh);
        glViewport(0, 0, fbw, fbh);
        glPolygonMode(GL_FRONT_AND_BACK, wireframe_ ? GL_LINE : GL_FILL);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        // top-left origin, y downward — same convention as the modern shader.
        glOrtho(0.0, double(logicalW_), double(logicalH_), 0.0, -1.0, 1.0);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        glClearColor(clear.r, clear.g, clear.b, clear.a);
        glClear(GL_COLOR_BUFFER_BIT);                // clear the whole window (letterbox bars)
        int vx, vy, vw, vh; letterbox(fbw, fbh, vx, vy, vw, vh);
        glViewport(vx, vy, vw, vh);                   // draw logical content aspect-correct, centered
    }
    void onEndFrame() override { tryCapture(); }

    void submitTriangles(const Vertex* v, std::size_t count) override {
        glDisable(GL_TEXTURE_2D);                    // solid: don't sample a stale texture
        glBegin(GL_TRIANGLES);
        for (std::size_t i = 0; i < count; ++i) {
            glColor4f(v[i].c.r, v[i].c.g, v[i].c.b, v[i].c.a);
            glVertex2f(v[i].x, v[i].y);
        }
        glEnd();
    }

    void submitTextured(const TexVertex* v, std::size_t count, TextureHandle tex) override {
        if (!count || !tex) return;
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, GLuint(tex));
        glBegin(GL_TRIANGLES);
        for (std::size_t i = 0; i < count; ++i) {
            glColor4f(v[i].tint.r, v[i].tint.g, v[i].tint.b, v[i].tint.a);
            glTexCoord2f(v[i].u, v[i].v);
            glVertex2f(v[i].x, v[i].y);
        }
        glEnd();
        glDisable(GL_TEXTURE_2D);
    }

private:
    IWindow* window_ = nullptr;
    bool wireframe_ = false;
};

/*
======================
createRendererGLLegacy

The factory RendererFactory.cpp resolves to when GL_LEGACY is compiled in.
======================
*/
IRenderer* createRendererGLLegacy() { return new GLLegacyRenderer(); }

} // namespace otacon
