// GLModernRenderer.cpp — OpenGL core-profile 3.3 backend (shaders + VBO/VAO).
//
// Geometry arrives as logical-space triangle lists (solid via submitTriangles,
// textured via submitTextured); the vertex shaders project logical 480x320
// (origin top-left, y down) into clip space, so output is identical to the
// other backends. See docs/backends for the cross-backend contract.
#include "render/IRenderer.hpp"
#include "render/gl_modern/GLLoader.hpp"
#include "platform/Window.hpp"
#include <cstdio>
#include <cstring>

using namespace otacon::glapi;

namespace otacon {

static const char* kVert =
    "#version 330 core\n"
    "layout(location=0) in vec2 aPos;\n"
    "layout(location=1) in vec4 aColor;\n"
    "uniform vec2 uViewport;\n"
    "out vec4 vColor;\n"
    "void main(){\n"
    "  vec2 ndc = vec2(aPos.x/(uViewport.x*0.5)-1.0, 1.0-aPos.y/(uViewport.y*0.5));\n"
    "  gl_Position = vec4(ndc, 0.0, 1.0); vColor = aColor;\n"
    "}\n";
static const char* kFrag =
    "#version 330 core\n"
    "in vec4 vColor; out vec4 oColor;\n"
    "void main(){ oColor = vColor; }\n";

static const char* kTexVert =
    "#version 330 core\n"
    "layout(location=0) in vec2 aPos;\n"
    "layout(location=1) in vec2 aUV;\n"
    "layout(location=2) in vec4 aTint;\n"
    "uniform vec2 uViewport;\n"
    "out vec2 vUV; out vec4 vTint;\n"
    "void main(){\n"
    "  vec2 ndc = vec2(aPos.x/(uViewport.x*0.5)-1.0, 1.0-aPos.y/(uViewport.y*0.5));\n"
    "  gl_Position = vec4(ndc, 0.0, 1.0); vUV = aUV; vTint = aTint;\n"
    "}\n";
static const char* kTexFrag =
    "#version 330 core\n"
    "in vec2 vUV; in vec4 vTint; out vec4 oColor;\n"
    "uniform sampler2D uTex;\n"
    "void main(){ oColor = texture(uTex, vUV) * vTint; }\n";

class GLModernRenderer final : public IRenderer {
public:
    void shutdown() override {
        if (vbo_) DeleteBuffers(1, &vbo_);
        if (vao_) DeleteVertexArrays(1, &vao_);
        if (prog_) DeleteProgram(prog_);
        if (texVbo_) DeleteBuffers(1, &texVbo_);
        if (texVao_) DeleteVertexArrays(1, &texVao_);
        if (texProg_) DeleteProgram(texProg_);
        effect_ = 0;   // effects are owned by whoever created them (the sample)
    }
    const char* name() const override { return "OpenGL Modern (core 3.3)"; }
    void setWireframe(bool on) override { wireframe_ = on; }

    TextureHandle createTexture(int w, int h, const std::uint8_t* rgba, bool repeat) override {
        GLuint tex = 0; GLenum wrap = repeat ? GL_REPEAT : GL_CLAMP_TO_EDGE;
        GenTextures(1, &tex);
        BindTexture(GL_TEXTURE_2D, tex);
        TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);   // crisp pixel art
        TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap);
        TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap);
        TexImage2D(GL_TEXTURE_2D, 0, int(GL_RGBA), w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
        return TextureHandle(tex);
    }
    void destroyTexture(TextureHandle t) override {
        if (t) { GLuint id = t; DeleteTextures(1, &id); }
    }
    void updateTexture(TextureHandle t, int w, int h, const std::uint8_t* rgba) override {
        if (!t || !rgba) return;
        BindTexture(GL_TEXTURE_2D, GLuint(t));
        // TexSubImage2D re-uses the existing storage — no reallocation per frame.
        TexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    }

    // ---- Fragment effects --------------------------------------------------
    // The one backend with a reachable programmable stage, so this is where the
    // live-GLSL path lives. An effect re-uses the engine's textured vertex
    // shader (kTexVert) and swaps only the fragment stage.
    bool supportsShaders() const override { return true; }

    ShaderHandle createEffect(const char* fragmentSrc, char* log, std::size_t logSize) override {
        if (log && logSize) log[0] = '\0';
        GLuint vs = compile(GL_VERTEX_SHADER, kTexVert, nullptr, 0);
        if (!vs) return 0;
        GLuint fs = compile(GL_FRAGMENT_SHADER, fragmentSrc, log, logSize);
        if (!fs) { DeleteShader(vs); return 0; }
        GLuint p = CreateProgram();
        AttachShader(p, vs); AttachShader(p, fs); LinkProgram(p);
        GLint ok = 0; GetProgramiv(p, GL_LINK_STATUS, &ok);
        DeleteShader(vs); DeleteShader(fs);
        if (!ok) {
            if (log && logSize) GetProgramInfoLog(p, GLsizei(logSize), nullptr, log);
            DeleteProgram(p);
            return 0;
        }
        return ShaderHandle(p);
    }
    void destroyEffect(ShaderHandle e) override {
        if (e == effect_) effect_ = 0;
        if (e) DeleteProgram(GLuint(e));
    }
    void useEffect(ShaderHandle e) override { effect_ = GLuint(e); }
    void setEffectTime(float seconds) override { effectTime_ = seconds; }
    void setEffectUniform(const char* name_, float x, float y, float z, float w) override {
        if (!effect_ || !name_) return;
        UseProgram(effect_);
        GLint loc = GetUniformLocation(effect_, name_);
        if (loc >= 0) Uniform4f(loc, x, y, z, w);
    }
    bool readPixels(int& w, int& h, std::vector<std::uint8_t>& rgba) override {
        int fbw = 0, fbh = 0; window_->framebufferSize(fbw, fbh);
        w = fbw; h = fbh;
        rgba.assign(std::size_t(w) * h * 4, 0);
        ReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
        // GL origin is bottom-left; flip rows to top-to-bottom.
        std::vector<std::uint8_t> tmp(std::size_t(w) * 4);
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
        if (!loadGLFunctions(window_)) return false;

        prog_ = buildProgram(kVert, kFrag);
        texProg_ = buildProgram(kTexVert, kTexFrag);
        if (!prog_ || !texProg_) return false;
        uViewport_ = GetUniformLocation(prog_, "uViewport");
        texUViewport_ = GetUniformLocation(texProg_, "uViewport");
        texSampler_ = GetUniformLocation(texProg_, "uTex");

        // Solid VAO/VBO: pos(2) + color(4).
        GenVertexArrays(1, &vao_); BindVertexArray(vao_);
        GenBuffers(1, &vbo_); BindBuffer(GL_ARRAY_BUFFER, vbo_);
        EnableVertexAttribArray(0);
        VertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (const void*)0);
        EnableVertexAttribArray(1);
        VertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (const void*)(2 * sizeof(float)));

        // Textured VAO/VBO: pos(2) + uv(2) + tint(4).
        GenVertexArrays(1, &texVao_); BindVertexArray(texVao_);
        GenBuffers(1, &texVbo_); BindBuffer(GL_ARRAY_BUFFER, texVbo_);
        EnableVertexAttribArray(0);
        VertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(TexVertex), (const void*)0);
        EnableVertexAttribArray(1);
        VertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(TexVertex), (const void*)(2 * sizeof(float)));
        EnableVertexAttribArray(2);
        VertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(TexVertex), (const void*)(4 * sizeof(float)));

        Enable(GL_BLEND);
        BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        return true;
    }

    void onBeginFrame(Color clear) override {
        int fbw = 0, fbh = 0; window_->framebufferSize(fbw, fbh);
        Viewport(0, 0, fbw, fbh);
        PolygonMode(GL_FRONT_AND_BACK, wireframe_ ? GL_LINE : GL_FILL);
        ClearColor(clear.r, clear.g, clear.b, clear.a);
        Clear(GL_COLOR_BUFFER_BIT);                 // clear the whole window (letterbox bars)
        int vx, vy, vw, vh; letterbox(fbw, fbh, vx, vy, vw, vh);
        Viewport(vx, vy, vw, vh);                    // draw logical content aspect-correct, centered
    }
    void onEndFrame() override { tryCapture(); }

    // Each submit rebinds its own program+VAO so solid and textured draws can
    // interleave freely.
    void submitTriangles(const Vertex* verts, std::size_t count) override {
        if (!count) return;
        UseProgram(prog_);
        Uniform2f(uViewport_, float(logicalW_), float(logicalH_));
        BindVertexArray(vao_);
        BindBuffer(GL_ARRAY_BUFFER, vbo_);
        BufferData(GL_ARRAY_BUFFER, GLsizeiptr(count * sizeof(Vertex)), verts, GL_DYNAMIC_DRAW);
        DrawArrays(GL_TRIANGLES, 0, GLsizei(count));
    }

    void submitTextured(const TexVertex* verts, std::size_t count, TextureHandle tex) override {
        if (!count || !tex) return;
        // A bound effect replaces the fragment stage; the vertex stage (and so
        // the projection) is the engine's either way.
        // The built-in path uses locations cached at init; only an effect (rare,
        // and re-bound on edit) pays for name lookups.
        const GLuint prog = effect_ ? effect_ : texProg_;
        UseProgram(prog);
        if (effect_) {
            Uniform2f(GetUniformLocation(prog, "uViewport"), float(logicalW_), float(logicalH_));
            GLint loc = GetUniformLocation(prog, "uResolution");
            if (loc >= 0) Uniform2f(loc, float(logicalW_), float(logicalH_));
            loc = GetUniformLocation(prog, "uTime");
            if (loc >= 0) Uniform1f(loc, effectTime_);
            loc = GetUniformLocation(prog, "uTex");
            if (loc >= 0) Uniform1i(loc, 0);
        } else {
            Uniform2f(texUViewport_, float(logicalW_), float(logicalH_));
            Uniform1i(texSampler_, 0);
        }
        ActiveTexture(GL_TEXTURE0);
        BindTexture(GL_TEXTURE_2D, GLuint(tex));
        BindVertexArray(texVao_);
        BindBuffer(GL_ARRAY_BUFFER, texVbo_);
        BufferData(GL_ARRAY_BUFFER, GLsizeiptr(count * sizeof(TexVertex)), verts, GL_DYNAMIC_DRAW);
        DrawArrays(GL_TRIANGLES, 0, GLsizei(count));
    }

private:
    // `outLog` is optional: the built-in shaders just print to stderr, while a
    // user effect needs the driver's text handed back so it can be displayed.
    static GLuint compile(GLenum type, const char* src, char* outLog = nullptr, std::size_t outLogSize = 0) {
        GLuint s = CreateShader(type);
        ShaderSource(s, 1, &src, nullptr);
        CompileShader(s);
        GLint ok = 0; GetShaderiv(s, GL_COMPILE_STATUS, &ok);
        if (!ok) {
            char log[1024]; GetShaderInfoLog(s, 1024, nullptr, log);
            if (outLog && outLogSize) std::snprintf(outLog, outLogSize, "%s", log);
            else std::fprintf(stderr, "[gl] shader compile error: %s\n", log);
            DeleteShader(s); return 0;
        }
        return s;
    }
    static GLuint buildProgram(const char* vsrc, const char* fsrc) {
        GLuint vs = compile(GL_VERTEX_SHADER, vsrc);
        GLuint fs = compile(GL_FRAGMENT_SHADER, fsrc);
        if (!vs || !fs) return 0;
        GLuint p = CreateProgram();
        AttachShader(p, vs); AttachShader(p, fs); LinkProgram(p);
        GLint ok = 0; GetProgramiv(p, GL_LINK_STATUS, &ok);
        DeleteShader(vs); DeleteShader(fs);
        if (!ok) { char log[1024]; GetProgramInfoLog(p, 1024, nullptr, log);
                   std::fprintf(stderr, "[gl] link error: %s\n", log); return 0; }
        return p;
    }

    IWindow* window_ = nullptr;
    GLuint prog_ = 0, vao_ = 0, vbo_ = 0;
    GLuint texProg_ = 0, texVao_ = 0, texVbo_ = 0;
    GLint  uViewport_ = -1, texUViewport_ = -1, texSampler_ = -1;
    GLuint effect_ = 0;            // bound user fragment effect (0 = built-in)
    float  effectTime_ = 0.f;
    bool   wireframe_ = false;
};

IRenderer* createRendererGLModern() { return new GLModernRenderer(); }

} // namespace otacon
