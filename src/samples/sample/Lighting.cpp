// Lighting.cpp — sample 05: 2D lighting from a normal map, computed twice.
//
// A normal map fakes relief on a flat quad by storing, per texel, which way the
// surface is facing. Shading is then the same dot product you would use in 3D:
//
//     lit = albedo * (ambient + sum over lights of  max(0, N.L) * attenuation)
//
// Two things make this sample worth reading. First, the normal map is *derived*
// from the height field the bricks were drawn into (see art::brickSurface), so
// you can see where normals come from rather than being handed a lilac PNG.
//
// Second, the same maths runs on both sides of the renderer seam. The CPU path
// writes pixels into a otacon::Canvas and uploads one texture — it works on every
// backend, including GL legacy, which has no programmable stage at all. The GPU
// path compiles the equivalent GLSL through IRenderer::createEffect and runs it
// per fragment. Press E to switch. The arithmetic is the same, so the lighting
// matches; what differs is *where it is evaluated*. The CPU path shades the
// 320x208 canvas and the result is then scaled up, so it is softer, while the
// GPU path shades every screen fragment. That gap is the honest trade, and it is
// why the CPU path is the portable fallback rather than the default everywhere.
//
// Albedo and normal share ONE texture, packed side by side (albedo in the left
// half, normals in the right). That is why the GPU path needs no multi-texture
// support in the seam — the shader just offsets its UV by 0.5.
#include "Sample.hpp"
#include "common/Art.hpp"
#include "render/Canvas.hpp"
#include "render/IRenderer.hpp"
#include "platform/Window.hpp"
#include "core/debug/Debug.hpp"
#include <cmath>
#include <cstdio>
#include <cstring>

namespace samples {
namespace {

using otacon::Color;

constexpr int kSurfW = 320, kSurfH = 208;   // the brick surface, in texels
constexpr int kLights = 3;

// The GPU path. Identical arithmetic to shadePixel() below — deliberately
// written to line up statement for statement so the two can be compared.
const char* kLitFrag =
    "#version 330 core\n"
    "in vec2 vUV; in vec4 vTint; out vec4 oColor;\n"
    "uniform sampler2D uTex;\n"
    "uniform vec4 uLight0; uniform vec4 uLight1; uniform vec4 uLight2;\n"   // xy pos, z radius, w intensity
    "uniform vec4 uCol0;   uniform vec4 uCol1;   uniform vec4 uCol2;\n"
    "uniform vec4 uParams;\n"                                              // x ambient, y useNormals, z specular
    "vec3 shade(vec3 albedo, vec3 n, vec2 uv, vec4 L, vec3 lc) {\n"
    "  vec2 d = L.xy - uv;\n"
    "  d.y *= 0.65;\n"                          // the surface is wider than tall
    "  float dist = length(d);\n"
    "  float atten = max(0.0, 1.0 - dist / max(L.z, 0.0001));\n"
    "  atten *= atten;\n"
    "  vec3 ldir = normalize(vec3(d, 0.35));\n"
    "  float ndl = max(0.0, dot(n, ldir));\n"
    "  vec3 lit = albedo * ndl * atten * L.w * lc;\n"
    "  if (uParams.z > 0.5) {\n"
    "    vec3 h = normalize(ldir + vec3(0.0, 0.0, 1.0));\n"
    "    lit += lc * pow(max(0.0, dot(n, h)), 24.0) * atten * L.w * 0.35;\n"
    "  }\n"
    "  return lit;\n"
    "}\n"
    "void main(){\n"
    "  vec2 auv = vec2(vUV.x * 0.5, vUV.y);\n"         // left half: albedo
    "  vec2 nuv = vec2(vUV.x * 0.5 + 0.5, vUV.y);\n"   // right half: normals
    "  vec3 albedo = texture(uTex, auv).rgb;\n"
    "  vec3 n = vec3(0.0, 0.0, 1.0);\n"
    "  if (uParams.y > 0.5) n = normalize(texture(uTex, nuv).rgb * 2.0 - 1.0);\n"
    "  vec3 c = albedo * uParams.x;\n"
    "  c += shade(albedo, n, vUV, uLight0, uCol0.rgb);\n"
    "  c += shade(albedo, n, vUV, uLight1, uCol1.rgb);\n"
    "  c += shade(albedo, n, vUV, uLight2, uCol2.rgb);\n"
    "  oColor = vec4(c, 1.0) * vTint;\n"
    "}\n";

struct Light {
    float u, v;          // position in 0..1 surface space
    float radius, power;
    float r, g, b;
    float phase, speed, orbit;
};

class Lighting final : public Sample {
public:
    void init(SampleContext& ctx) override {
        r_ = ctx.renderer;
        W_ = float(ctx.logicalW); H_ = float(ctx.logicalH);

        art::brickSurface(kSurfW, kSurfH, albedo_, normal_);

        // Pack albedo | normal into one texture so the shader needs one sampler.
        otacon::Image packed;
        packed.width = kSurfW * 2; packed.height = kSurfH;
        packed.rgba.assign(std::size_t(packed.width) * packed.height * 4, 0);
        for (int y = 0; y < kSurfH; ++y) {
            std::memcpy(&packed.rgba[(std::size_t(y) * packed.width) * 4],
                        &albedo_.rgba[std::size_t(y) * kSurfW * 4], std::size_t(kSurfW) * 4);
            std::memcpy(&packed.rgba[(std::size_t(y) * packed.width + kSurfW) * 4],
                        &normal_.rgba[std::size_t(y) * kSurfW * 4], std::size_t(kSurfW) * 4);
        }
        packedTex_ = r_->createTexture(packed);
        canvas_.init(*r_, kSurfW, kSurfH);

        gpuAvailable_ = r_->supportsShaders();
        if (gpuAvailable_) {
            effect_ = r_->createEffect(kLitFrag, shaderLog_, sizeof shaderLog_);
            if (!effect_) gpuAvailable_ = false;
        } else {
            std::snprintf(shaderLog_, sizeof shaderLog_, "%s has no programmable stage", r_->name());
        }
    }
    void shutdown() override {
        if (!r_) return;
        if (effect_) { r_->destroyEffect(effect_); effect_ = 0; }
        if (packedTex_) { r_->destroyTexture(packedTex_); packedTex_ = 0; }
        canvas_.shutdown(*r_);
    }

    void enter() override {
        t_ = 0; ambient_ = 0.12f; useNormals_ = true; specular_ = false;
        useGpu_ = gpuAvailable_; showMap_ = 0; animate_ = true;
        const Light init[kLights] = {
            {0.30f, 0.40f, 0.55f, 1.15f, 1.00f, 0.82f, 0.55f, 0.0f, 0.55f, 0.26f},
            {0.70f, 0.60f, 0.48f, 1.00f, 0.35f, 0.65f, 1.00f, 2.1f, 0.40f, 0.30f},
            {0.50f, 0.25f, 0.40f, 0.85f, 1.00f, 0.35f, 0.40f, 4.0f, 0.70f, 0.20f},
        };
        for (int i = 0; i < kLights; ++i) lights_[i] = init[i];
    }

    void handleInput(const otacon::InputFrame& in) override {
        if (in.isPressed(otacon::Action::Jump)) useNormals_ = !useNormals_;
        if (in.isPressed(otacon::Action::Aux6) && gpuAvailable_) useGpu_ = !useGpu_;   // E
        if (in.isPressed(otacon::Action::Aux4)) specular_ = !specular_;                // F4
        if (in.isPressed(otacon::Action::Aux5)) showMap_ = (showMap_ + 1) % 3;         // F6
        if (in.isPressed(otacon::Action::Aux7)) animate_ = !animate_;                  // F7
        if (in.selectSlot == 1) ambient_ = 0.f;
        if (in.selectSlot == 2) ambient_ = 0.12f;
        if (in.selectSlot == 3) ambient_ = 0.35f;
        // Light 0 follows the pointer while the right button is down.
        if (in.dragHeld) {
            const float sx = surfX_(), sy = surfY_(), sw = surfW_(), sh = surfH_();
            lights_[0].u = (in.mouseNx * W_ - sx) / sw;
            lights_[0].v = (in.mouseNy * H_ - sy) / sh;
            dragging_ = true;
        } else dragging_ = false;
    }

    void update(otacon::Real dt) override {
        const float d = otacon::toFloat(dt);
        t_ += d;
        if (!animate_) return;
        for (int i = 0; i < kLights; ++i) {
            Light& L = lights_[i];
            if (i == 0 && dragging_) continue;
            const float a = t_ * L.speed + L.phase;
            L.u = 0.5f + std::cos(a) * L.orbit * 1.35f;
            L.v = 0.5f + std::sin(a * 1.3f) * L.orbit;
        }
    }

    void render(otacon::IRenderer& r, const otacon::DebugRuntime&) override {
        const float sx = surfX_(), sy = surfY_(), sw = surfW_(), sh = surfH_();

        if (showMap_ == 1) {          // raw albedo, no lighting
            r.drawImage(packedTex_, sx, sy, sw, sh, 0.f, 0.f, 0.5f, 1.f);
        } else if (showMap_ == 2) {   // the normal map itself
            r.drawImage(packedTex_, sx, sy, sw, sh, 0.5f, 0.f, 1.f, 1.f);
        } else if (useGpu_ && effect_) {
            renderGpu(r, sx, sy, sw, sh);
        } else {
            renderCpu(r, sx, sy, sw, sh);
        }

        // Light markers, drawn in logical space over the surface.
        for (int i = 0; i < kLights; ++i) {
            const Light& L = lights_[i];
            const float px = sx + L.u * sw, py = sy + L.v * sh;
            const Color c{L.r, L.g, L.b, 1.f};
            r.drawRectOutline(px - 4, py - 4, 8, 8, c, 1.f);
            r.drawLine(px - 7, py, px + 7, py, c, 1.f);
            r.drawLine(px, py - 7, px, py + 7, c, 1.f);
        }

        drawPanel(r, sx, sy, sw, sh);
    }

    const char* status() const override {
        std::snprintf(buf_, sizeof buf_, "%s  normals %s  ambient %.2f  spec %s%s",
                      showMap_ ? (showMap_ == 1 ? "ALBEDO ONLY" : "NORMAL MAP")
                               : (useGpu_ ? "GPU shader" : "CPU per-pixel"),
                      useNormals_ ? "on" : "OFF", ambient_, specular_ ? "on" : "off",
                      gpuAvailable_ ? "" : "  [no shader support]");
        return buf_;
    }
    const char* keys() const override {
        return "SPACE  normal map on/off (flat vs relief)\nE      CPU path / GPU shader\nF4     specular highlight\nF6     lit / albedo / normal map\nF7     freeze the lights\n1-3    ambient level\nright-drag  move light 1";
    }

private:
    float surfW_() const { return W_ - 172.f; }
    float surfH_() const { return surfW_() * float(kSurfH) / float(kSurfW); }
    float surfX_() const { return 8.f; }
    float surfY_() const { return layout::kTop + 6.f; }

    // ---- the CPU path: the reference implementation ------------------------
    void renderCpu(otacon::IRenderer& r, float sx, float sy, float sw, float sh) {
        for (int y = 0; y < kSurfH; ++y) {
            for (int x = 0; x < kSurfW; ++x) {
                float cr, cg, cb;
                shadePixel(x, y, cr, cg, cb);
                canvas_.setf(x, y, cr, cg, cb, 1.f);
            }
        }
        canvas_.commit(r);
        canvas_.draw(r, sx, sy, sw, sh);
    }

    // One texel. Kept deliberately close to kLitFrag's shade().
    void shadePixel(int x, int y, float& outR, float& outG, float& outB) const {
        const std::size_t i = (std::size_t(y) * kSurfW + x) * 4;
        const float ar = albedo_.rgba[i] / 255.f;
        const float ag = albedo_.rgba[i + 1] / 255.f;
        const float ab = albedo_.rgba[i + 2] / 255.f;

        float nx = 0.f, ny = 0.f, nz = 1.f;
        if (useNormals_) {
            nx = normal_.rgba[i] / 255.f * 2.f - 1.f;
            ny = normal_.rgba[i + 1] / 255.f * 2.f - 1.f;
            nz = normal_.rgba[i + 2] / 255.f * 2.f - 1.f;
            const float len = std::sqrt(nx * nx + ny * ny + nz * nz);
            if (len > 0.0001f) { nx /= len; ny /= len; nz /= len; }
        }

        const float u = (x + 0.5f) / kSurfW, v = (y + 0.5f) / kSurfH;
        float cr = ar * ambient_, cg = ag * ambient_, cb = ab * ambient_;

        for (int k = 0; k < kLights; ++k) {
            const Light& L = lights_[k];
            float dx = L.u - u, dy = (L.v - v) * 0.65f;
            const float dist = std::sqrt(dx * dx + dy * dy);
            float atten = 1.f - dist / (L.radius > 0.0001f ? L.radius : 0.0001f);
            if (atten <= 0.f) continue;
            atten *= atten;

            const float lz = 0.35f;
            const float ll = std::sqrt(dx * dx + dy * dy + lz * lz);
            const float lx = dx / ll, ly = dy / ll, lzn = lz / ll;
            float ndl = nx * lx + ny * ly + nz * lzn;
            if (ndl < 0.f) ndl = 0.f;

            const float s = ndl * atten * L.power;
            cr += ar * s * L.r; cg += ag * s * L.g; cb += ab * s * L.b;

            if (specular_) {
                // Half-vector against a viewer straight on (0,0,1).
                float hx = lx, hy = ly, hz = lzn + 1.f;
                const float hl = std::sqrt(hx * hx + hy * hy + hz * hz);
                hx /= hl; hy /= hl; hz /= hl;
                float ndh = nx * hx + ny * hy + nz * hz;
                if (ndh > 0.f) {
                    float spec = std::pow(ndh, 24.f) * atten * L.power * 0.35f;
                    cr += L.r * spec; cg += L.g * spec; cb += L.b * spec;
                }
            }
        }
        outR = cr; outG = cg; outB = cb;
    }

    // ---- the GPU path: same maths, one draw --------------------------------
    void renderGpu(otacon::IRenderer& r, float sx, float sy, float sw, float sh) {
        r.useEffect(effect_);
        r.setEffectTime(t_);
        const char* posNames[kLights] = {"uLight0", "uLight1", "uLight2"};
        const char* colNames[kLights] = {"uCol0", "uCol1", "uCol2"};
        for (int i = 0; i < kLights; ++i) {
            const Light& L = lights_[i];
            r.setEffectUniform(posNames[i], L.u, L.v, L.radius, L.power);
            r.setEffectUniform(colNames[i], L.r, L.g, L.b, 1.f);
        }
        r.setEffectUniform("uParams", ambient_, useNormals_ ? 1.f : 0.f, specular_ ? 1.f : 0.f, 0.f);
        r.drawImage(packedTex_, sx, sy, sw, sh);
        r.useEffect(0);           // always restore, or the HUD renders through it
    }

    void drawPanel(otacon::IRenderer& r, float sx, float sy, float sw, float sh) const {
        const float px = sx + sw + 10;
        float y = sy;
        r.drawText("PATH", px, y, 1.f, Color{0.55f, 0.80f, 1.f, 1.f}); y += 11;
        r.drawText(useGpu_ ? "> GPU fragment shader" : "> CPU per-pixel", px, y, 1.f,
                   Color{1.f, 0.85f, 0.35f, 1.f}); y += 10;
        char px2[64];
        std::snprintf(px2, sizeof px2, "%d x %d = %d px", kSurfW, kSurfH, kSurfW * kSurfH);
        r.drawText(px2, px, y, 1.f, Color{0.55f, 0.62f, 0.75f, 1.f}); y += 9;
        if (!gpuAvailable_) {
            r.drawText("GPU path unavailable:", px, y, 1.f, Color{1.f, 0.55f, 0.45f, 1.f}); y += 8;
            r.drawText(shaderLog_, px, y, 1.f, Color{0.75f, 0.45f, 0.40f, 1.f}); y += 8;
            r.drawText("the CPU path still runs", px, y, 1.f, Color{0.55f, 0.62f, 0.75f, 1.f}); y += 8;
        }
        y += 6;

        r.drawText("SURFACE", px, y, 1.f, Color{0.55f, 0.80f, 1.f, 1.f}); y += 11;
        r.drawText(useNormals_ ? "normals: ON" : "normals: OFF (flat)", px, y, 1.f,
                   useNormals_ ? Color{0.45f, 0.95f, 0.6f, 1.f} : Color{1.f, 0.6f, 0.4f, 1.f}); y += 9;
        char amb[48]; std::snprintf(amb, sizeof amb, "ambient: %.2f", ambient_);
        r.drawText(amb, px, y, 1.f, Color{0.55f, 0.62f, 0.75f, 1.f}); y += 9;
        r.drawText(specular_ ? "specular: on" : "specular: off", px, y, 1.f,
                   Color{0.55f, 0.62f, 0.75f, 1.f}); y += 14;

        r.drawText("N.L", px, y, 1.f, Color{0.55f, 0.80f, 1.f, 1.f}); y += 10;
        r.drawText("lit = albedo *", px, y, 1.f, Color{0.62f, 0.68f, 0.80f, 1.f}); y += 8;
        r.drawText("  (ambient + sum", px, y, 1.f, Color{0.62f, 0.68f, 0.80f, 1.f}); y += 8;
        r.drawText("   max(0,N.L)*att)", px, y, 1.f, Color{0.62f, 0.68f, 0.80f, 1.f}); y += 14;

        r.drawText("the normal map is", px, y, 1.f, Color{0.42f, 0.48f, 0.60f, 1.f}); y += 8;
        r.drawText("derived from the", px, y, 1.f, Color{0.42f, 0.48f, 0.60f, 1.f}); y += 8;
        r.drawText("brick height field", px, y, 1.f, Color{0.42f, 0.48f, 0.60f, 1.f});

        r.drawText("SPACE flat/relief   E cpu/gpu   F6 view", 8, H_ - 11, 1.f,
                   Color{0.45f, 0.51f, 0.62f, 1.f});
    }

    otacon::IRenderer* r_ = nullptr;
    otacon::Image albedo_, normal_;
    otacon::TextureHandle packedTex_ = 0;
    otacon::ShaderHandle  effect_ = 0;
    otacon::Canvas canvas_;
    Light  lights_[kLights]{};
    float  W_ = 640, H_ = 400, t_ = 0, ambient_ = 0.12f;
    int    showMap_ = 0;
    bool   useNormals_ = true, specular_ = false, useGpu_ = false;
    bool   gpuAvailable_ = false, animate_ = true, dragging_ = false;
    char   shaderLog_[512]{};
    mutable char buf_[160]{};
};

} // namespace

Sample* makeLighting() { return new Lighting(); }

} // namespace samples
