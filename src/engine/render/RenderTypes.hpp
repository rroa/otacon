// RenderTypes.hpp — shared, backend-agnostic rendering types.
#pragma once
#include <cstdint>

namespace otacon {

struct Color {
    float r = 1, g = 1, b = 1, a = 1;
    constexpr Color() = default;
    constexpr Color(float r_, float g_, float b_, float a_ = 1.f) : r(r_), g(g_), b(b_), a(a_) {}

    // The original game specifies colors as 0xRRGGBB / 0xAARRGGBB hex literals.
    static Color rgb(std::uint32_t hex) {
        return { ((hex >> 16) & 0xFF) / 255.f, ((hex >> 8) & 0xFF) / 255.f, (hex & 0xFF) / 255.f, 1.f };
    }
    static Color argb(std::uint32_t hex) {
        return { ((hex >> 16) & 0xFF) / 255.f, ((hex >> 8) & 0xFF) / 255.f, (hex & 0xFF) / 255.f,
                 ((hex >> 24) & 0xFF) / 255.f };
    }
};

// A vertex in *logical* screen space (0..logicalW, 0..logicalH, origin
// top-left, y downward — matching flixel's screen coordinates). Each backend
// applies its own orthographic projection to turn this into clip space.
struct Vertex {
    float x, y;
    Color c;
};

// A textured vertex: position + UV (0..1) + tint (multiplied with the texel).
struct TexVertex {
    float x, y, u, v;
    Color tint;
};

// Opaque GPU texture id (0 == none). Backends interpret it (a GL texture name,
// a Vulkan descriptor index, …).
using TextureHandle = std::uint32_t;

} // namespace otacon
