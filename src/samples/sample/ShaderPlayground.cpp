// ShaderPlayground.cpp — sample 13: the same effect, either side of the seam.
//
// This is the sample the shader seam exists for. Four effects, each written
// twice: once as a GLSL fragment shader compiled through
// IRenderer::createEffect, and once as a C++ loop writing into a otacon::Canvas. The
// source of whichever one is running is printed on screen next to the picture.
//
// Why both? Because the engine's promise is that every backend draws the same
// thing, and only one of the three has a programmable stage. GL modern compiles
// the GLSL; GL legacy is fixed-function immediate mode and Vulkan ships
// pre-built SPIR-V with no runtime compiler, so on those the CPU version runs
// and the sample still works. Press E to switch on a backend that has both, and
// note that the point is not that one is better — it is that the seam let the
// engine keep a portable answer without giving up the fast one.
//
// If a shader fails to compile, the driver's log is shown verbatim and the CPU
// path takes over. That is deliberate: a playground that goes blank on a typo
// teaches nothing.
#include "Sample.hpp"
#include "render/Canvas.hpp"
#include "render/IRenderer.hpp"
#include "platform/Window.hpp"
#include "core/debug/Debug.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace samples {
namespace {

using otacon::Color;

constexpr int kCanvasW = 288, kCanvasH = 192;

struct Effect {
    const char* name;
    const char* idea;         // one line: what the maths is doing
    const char* frag;         // GLSL
    const char* cpuSource;    // the C++ inner loop, as text, for the side panel
};

// Each shader is deliberately short enough to read whole on screen.
const Effect kEffects[] = {
{
    "plasma",
    "sum of sines in x, y and radius",
    "#version 330 core\n"
    "in vec2 vUV; in vec4 vTint; out vec4 oColor;\n"
    "uniform sampler2D uTex; uniform float uTime; uniform vec4 uParams;\n"
    "void main(){\n"
    "  vec2 p = vUV * uParams.x;\n"
    "  float t = uTime;\n"
    "  float v = sin(p.x + t)\n"
    "          + sin(p.y * 1.3 + t * 1.1)\n"
    "          + sin(length(p - uParams.x * 0.5) * 1.7 - t * 1.4);\n"
    "  v /= 3.0;\n"
    "  vec3 c = 0.5 + 0.5 * cos(6.2831 * (v + vec3(0.0, 0.33, 0.67)));\n"
    "  oColor = vec4(c, 1.0) * vTint;\n"
    "}\n",
    "float v = sin(px + t)\n"
    "        + sin(py*1.3 + t*1.1)\n"
    "        + sin(dist*1.7 - t*1.4);\n"
    "v /= 3;\n"
    "r = .5+.5*cos(TAU*(v+0.00));\n"
    "g = .5+.5*cos(TAU*(v+0.33));\n"
    "b = .5+.5*cos(TAU*(v+0.67));"
},
{
    "ripple",
    "distance from centre, offset in time",
    "#version 330 core\n"
    "in vec2 vUV; in vec4 vTint; out vec4 oColor;\n"
    "uniform sampler2D uTex; uniform float uTime; uniform vec4 uParams;\n"
    "void main(){\n"
    "  vec2 d = vUV - vec2(0.5);\n"
    "  float r = length(d);\n"
    "  float w = sin(r * uParams.x * 3.0 - uTime * 4.0);\n"
    "  float fall = exp(-r * 3.0);\n"
    "  float v = 0.5 + 0.5 * w * fall;\n"
    "  oColor = vec4(vec3(v * 0.35, v * 0.75, v), 1.0) * vTint;\n"
    "}\n",
    "float r = length(uv - 0.5);\n"
    "float w = sin(r*k*3 - t*4);\n"
    "float fall = exp(-r*3);\n"
    "float v = .5 + .5*w*fall;\n"
    "rgb = vec3(v*.35, v*.75, v);"
},
{
    "spiral",
    "polar coordinates: angle plus radius",
    "#version 330 core\n"
    "in vec2 vUV; in vec4 vTint; out vec4 oColor;\n"
    "uniform sampler2D uTex; uniform float uTime; uniform vec4 uParams;\n"
    "void main(){\n"
    "  vec2 d = vUV - vec2(0.5);\n"
    "  float a = atan(d.y, d.x);\n"
    "  float r = length(d);\n"
    "  float v = sin(a * uParams.x + r * 24.0 - uTime * 3.0);\n"
    "  v = smoothstep(-0.2, 0.2, v);\n"
    "  vec3 c = mix(vec3(0.08, 0.10, 0.22), vec3(1.0, 0.72, 0.30), v);\n"
    "  oColor = vec4(c, 1.0) * vTint;\n"
    "}\n",
    "float a = atan2(dy, dx);\n"
    "float r = length(d);\n"
    "float v = sin(a*arms + r*24 - t*3);\n"
    "v = smoothstep(-.2, .2, v);\n"
    "rgb = mix(dark, warm, v);"
},
{
    "checker warp",
    "a grid, with its coordinates bent first",
    "#version 330 core\n"
    "in vec2 vUV; in vec4 vTint; out vec4 oColor;\n"
    "uniform sampler2D uTex; uniform float uTime; uniform vec4 uParams;\n"
    "void main(){\n"
    "  vec2 uv = vUV;\n"
    "  uv.x += sin(uv.y * 8.0 + uTime) * 0.06;\n"
    "  uv.y += sin(uv.x * 8.0 - uTime) * 0.06;\n"
    "  vec2 g = floor(uv * uParams.x);\n"
    "  float c = mod(g.x + g.y, 2.0);\n"
    "  vec3 col = mix(vec3(0.10, 0.13, 0.20), vec3(0.45, 0.80, 0.95), c);\n"
    "  oColor = vec4(col, 1.0) * vTint;\n"
    "}\n",
    "u += sin(v*8 + t) * .06;\n"
    "v += sin(u*8 - t) * .06;\n"
    "int gx = floor(u*n), gy = floor(v*n);\n"
    "int c = (gx + gy) & 1;\n"
    "rgb = c ? light : dark;"
},
};
constexpr int kEffectCount = int(sizeof kEffects / sizeof kEffects[0]);

class ShaderPlayground final : public Sample {
public:
    void init(SampleContext& ctx) override {
        r_ = ctx.renderer;
        W_ = float(ctx.logicalW); H_ = float(ctx.logicalH);
        canvas_.init(*r_, kCanvasW, kCanvasH);
        // A 1x1 white texture: the effects ignore uTex and generate colour from
        // the UV alone, but a textured draw is what carries a fragment stage.
        const std::uint8_t white[4] = {255, 255, 255, 255};
        white_ = r_->createTexture(1, 1, white, false);

        gpuAvailable_ = r_->supportsShaders();
        if (gpuAvailable_) compileAll();
        else std::snprintf(log_, sizeof log_, "%s has no programmable stage", r_->name());
    }
    void shutdown() override {
        if (!r_) return;
        for (int i = 0; i < kEffectCount; ++i)
            if (effects_[i]) { r_->destroyEffect(effects_[i]); effects_[i] = 0; }
        if (white_) { r_->destroyTexture(white_); white_ = 0; }
        canvas_.shutdown(*r_);
    }

    void enter() override {
        which_ = 0; t_ = 0; param_ = 8.f; useGpu_ = gpuAvailable_; showSource_ = true;
    }

    void handleInput(const otacon::InputFrame& in) override {
        if (in.isPressed(otacon::Action::Jump)) which_ = (which_ + 1) % kEffectCount;
        if (in.selectSlot >= 1 && in.selectSlot <= kEffectCount) which_ = in.selectSlot - 1;
        if (in.isPressed(otacon::Action::Aux6) && gpuAvailable_) useGpu_ = !useGpu_;   // E
        if (in.isPressed(otacon::Action::Aux4)) showSource_ = !showSource_;            // F4
        if (in.isPressed(otacon::Action::Aux5)) param_ = std::max(2.f, param_ - 2.f);  // F6
        if (in.isPressed(otacon::Action::Aux7)) param_ = std::min(40.f, param_ + 2.f); // F7
    }

    void update(otacon::Real dt) override { t_ += otacon::toFloat(dt); }

    void render(otacon::IRenderer& r, const otacon::DebugRuntime&) override {
        const float vx = 8, vy = layout::kTop + 6;
        const float vw = showSource_ ? W_ * 0.46f : W_ - 16;
        const float vh = vw * float(kCanvasH) / float(kCanvasW);

        const bool gpuNow = useGpu_ && effects_[which_] != 0;
        if (gpuNow) {
            r.useEffect(effects_[which_]);
            r.setEffectTime(t_);
            r.setEffectUniform("uParams", param_, 0, 0, 0);
            r.drawImage(white_, vx, vy, vw, vh);
            r.useEffect(0);
        } else {
            renderCpu(r);
            canvas_.draw(r, vx, vy, vw, vh);
        }
        r.drawRectOutline(vx, vy, vw, vh, Color{0.25f, 0.32f, 0.44f, 1.f}, 1.f);

        if (showSource_) drawSource(r, vx + vw + 10, vy, gpuNow);
        drawFooter(r, vx, vy + vh, vw, gpuNow);
    }

    const char* status() const override {
        std::snprintf(buf_, sizeof buf_, "%s  %s  param %.0f%s",
                      kEffects[which_].name, (useGpu_ && effects_[which_]) ? "GPU" : "CPU",
                      param_, gpuAvailable_ ? "" : "  [no shader support]");
        return buf_;
    }
    const char* keys() const override {
        return "SPACE / 1-4  pick an effect\nE      GPU shader / CPU loop\nF4     show or hide the source\nF6/F7  the effect's parameter";
    }

private:
    void compileAll() {
        bool any = false;
        for (int i = 0; i < kEffectCount; ++i) {
            char err[512] = {0};
            effects_[i] = r_->createEffect(kEffects[i].frag, err, sizeof err);
            if (!effects_[i] && err[0] && !any) {
                std::snprintf(log_, sizeof log_, "%s: %s", kEffects[i].name, err);
                any = true;
            }
        }
        if (!any) std::snprintf(log_, sizeof log_, "all %d effects compiled", kEffectCount);
    }

    // The CPU twin of whichever effect is selected. Written to mirror the GLSL
    // statement for statement, so the panel's two source listings line up.
    void renderCpu(otacon::IRenderer& r) {
        const float TAU = 6.2831853f;
        for (int y = 0; y < kCanvasH; ++y) {
            const float v = (y + 0.5f) / kCanvasH;
            for (int x = 0; x < kCanvasW; ++x) {
                const float u = (x + 0.5f) / kCanvasW;
                float cr = 0, cg = 0, cb = 0;
                switch (which_) {
                    case 0: {
                        const float px = u * param_, py = v * param_;
                        const float dx = px - param_ * 0.5f, dy = py - param_ * 0.5f;
                        const float dist = std::sqrt(dx * dx + dy * dy);
                        float val = std::sin(px + t_)
                                  + std::sin(py * 1.3f + t_ * 1.1f)
                                  + std::sin(dist * 1.7f - t_ * 1.4f);
                        val /= 3.f;
                        cr = 0.5f + 0.5f * std::cos(TAU * (val + 0.00f));
                        cg = 0.5f + 0.5f * std::cos(TAU * (val + 0.33f));
                        cb = 0.5f + 0.5f * std::cos(TAU * (val + 0.67f));
                        break;
                    }
                    case 1: {
                        const float dx = u - 0.5f, dy = v - 0.5f;
                        const float rad = std::sqrt(dx * dx + dy * dy);
                        const float w = std::sin(rad * param_ * 3.f - t_ * 4.f);
                        const float fall = std::exp(-rad * 3.f);
                        const float val = 0.5f + 0.5f * w * fall;
                        cr = val * 0.35f; cg = val * 0.75f; cb = val;
                        break;
                    }
                    case 2: {
                        const float dx = u - 0.5f, dy = v - 0.5f;
                        const float a = std::atan2(dy, dx);
                        const float rad = std::sqrt(dx * dx + dy * dy);
                        float val = std::sin(a * param_ + rad * 24.f - t_ * 3.f);
                        val = smoothstep(-0.2f, 0.2f, val);
                        cr = 0.08f + (1.00f - 0.08f) * val;
                        cg = 0.10f + (0.72f - 0.10f) * val;
                        cb = 0.22f + (0.30f - 0.22f) * val;
                        break;
                    }
                    default: {
                        float uu = u, vv = v;
                        uu += std::sin(v * 8.f + t_) * 0.06f;
                        vv += std::sin(u * 8.f - t_) * 0.06f;
                        const int gx = int(std::floor(uu * param_));
                        const int gy = int(std::floor(vv * param_));
                        const bool c = ((gx + gy) & 1) != 0;
                        cr = c ? 0.45f : 0.10f; cg = c ? 0.80f : 0.13f; cb = c ? 0.95f : 0.20f;
                        break;
                    }
                }
                canvas_.setf(x, y, cr, cg, cb, 1.f);
            }
        }
        canvas_.commit(r);
    }
    static float smoothstep(float e0, float e1, float x) {
        float t = (x - e0) / (e1 - e0);
        t = t < 0 ? 0 : (t > 1 ? 1 : t);
        return t * t * (3.f - 2.f * t);
    }

    void drawSource(otacon::IRenderer& r, float px, float py, bool gpuNow) const {
        float y = py;
        r.drawText(gpuNow ? "GLSL FRAGMENT SHADER" : "C++ INNER LOOP", px, y, 1.f,
                   Color{0.55f, 0.80f, 1.f, 1.f});
        y += 11;
        r.drawText(kEffects[which_].idea, px, y, 1.f, Color{1.f, 0.85f, 0.35f, 1.f});
        y += 12;
        // Print the source line by line; the GLSL listing skips its boilerplate
        // header so the interesting part fits.
        const char* src = gpuNow ? kEffects[which_].frag : kEffects[which_].cpuSource;
        int skip = gpuNow ? 3 : 0;
        char line[160];
        int li = 0;
        for (const char* p = src; ; ++p) {
            if (*p == '\n' || *p == '\0') {
                line[li] = '\0';
                if (skip > 0) --skip;
                else if (li > 0) { r.drawText(line, px, y, 1.f, Color{0.62f, 0.70f, 0.82f, 1.f}); y += 8; }
                else y += 4;
                li = 0;
                if (*p == '\0') break;
            } else if (li < int(sizeof line) - 1) {
                line[li++] = *p;
            }
        }
    }

    void drawFooter(otacon::IRenderer& r, float x, float y, float w, bool gpuNow) const {
        y += 6;
        for (int i = 0; i < kEffectCount; ++i) {
            char t[40]; std::snprintf(t, sizeof t, "%d %s", i + 1, kEffects[i].name);
            const float cx = x + i * (w / kEffectCount);
            r.drawText(t, cx, y, 1.f, i == which_ ? Color{1.f, 0.85f, 0.35f, 1.f}
                                                  : Color{0.48f, 0.54f, 0.66f, 1.f});
        }
        y += 12;
        r.drawText(gpuNow ? "> running on the GPU (createEffect + useEffect)"
                          : "> running on the CPU (otacon::Canvas + updateTexture)",
                   x, y, 1.f, gpuNow ? Color{0.45f, 0.90f, 0.60f, 1.f} : Color{0.55f, 0.75f, 1.f, 1.f});
        y += 10;
        char px2[96];
        std::snprintf(px2, sizeof px2, gpuNow ? "one draw call, shaded per screen fragment"
                                              : "%d CPU pixels, uploaded as one texture",
                      kCanvasW * kCanvasH);
        r.drawText(px2, x, y, 1.f, Color{0.45f, 0.51f, 0.62f, 1.f});
        y += 10;
        r.drawText(log_, x, y, 1.f, gpuAvailable_ ? Color{0.42f, 0.48f, 0.60f, 1.f}
                                                  : Color{1.f, 0.60f, 0.45f, 1.f});
    }

    otacon::IRenderer* r_ = nullptr;
    otacon::TextureHandle white_ = 0;
    otacon::ShaderHandle  effects_[kEffectCount]{};
    otacon::Canvas canvas_;
    float  W_ = 640, H_ = 400, t_ = 0, param_ = 8.f;
    int    which_ = 0;
    bool   useGpu_ = false, gpuAvailable_ = false, showSource_ = true;
    char   log_[512]{};
    mutable char buf_[160]{};
};

} // namespace

Sample* makeShaderPlayground() { return new ShaderPlayground(); }

} // namespace samples
