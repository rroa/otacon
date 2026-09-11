// RayShapes.cpp — sample 17: the queries that are not the solver.
//
// Collision.cpp answers "these two overlap, push them apart". Two engine headers
// answer the other questions, and they come up constantly:
//
//   scene/Raycast.hpp   what is the first thing along this line, and where?
//   scene/Shapes.hpp    circles and capsules, for the volumes a box gets wrong
//
// A ray returns the surface NORMAL, not just a distance, and the reflected
// bounce drawn here is why: with a distance alone you know you hit something and
// nothing about what to do next.
//
// The right-hand panel is the shape half. A blast radius, an aggro range, a
// sword arc -- forcing those through an AABB gives corners where there should
// not be any, and the circle-vs-box test here is a distance to the clamped
// point rather than a span overlap, which is exactly the difference.
#include "Sample.hpp"
#include "core/math/Random.hpp"
#include "scene/Raycast.hpp"
#include "scene/Shapes.hpp"
#include "scene/TileMap.hpp"
#include "render/IRenderer.hpp"
#include "platform/Window.hpp"
#include "core/debug/Debug.hpp"
#include <cmath>
#include <cstdio>
#include <vector>

namespace samples {
namespace {

using otacon::Color;
using otacon::Rect;
using otacon::Vec2f;
using otacon::Circle;
using otacon::Capsule;
namespace ray = otacon::ray;
namespace shape = otacon::shape;

constexpr int kMapW = 26, kMapH = 18;
constexpr float kTile = 14.f;

class RayShapes final : public Sample {
public:
    void init(SampleContext& ctx) override { W_ = float(ctx.logicalW); H_ = float(ctx.logicalH); }

    void enter() override {
        mode_ = 0; bounces_ = 3; t_ = 0; showFan_ = true;
        buildMap();
        target_ = {180.f, 150.f};
    }

    void handleInput(const otacon::InputFrame& in) override {
        mouse_ = {in.mouseNx * W_, in.mouseNy * H_};
        if (in.isPressed(otacon::Action::Jump)) mode_ = 1 - mode_;
        if (in.isPressed(otacon::Action::Aux6)) showFan_ = !showFan_;
        if (in.isPressed(otacon::Action::Aux4)) buildMap();
        if (in.selectSlot >= 1 && in.selectSlot <= 6) bounces_ = in.selectSlot;
    }

    void update(otacon::Real dt) override { t_ += otacon::toFloat(dt); }

    void render(otacon::IRenderer& r, const otacon::DebugRuntime&) override {
        if (mode_ == 0) renderRays(r); else renderShapes(r);
    }

    const char* status() const override {
        if (mode_ == 0)
            std::snprintf(buf_, sizeof buf_, "raycast  bounces %d  %s", bounces_,
                          showFan_ ? "fan on" : "single ray");
        else
            std::snprintf(buf_, sizeof buf_, "shapes  circle/capsule overlap + manifolds");
        return buf_;
    }
    const char* keys() const override {
        return "SPACE  raycast / shapes\nmouse  aims the ray, moves the probe\nrays:  1-6 bounce count   E fan   F4 new map\n";
    }

private:
    float ox() const { return 8.f; }
    float oy() const { return layout::kTop + 6.f; }

    void buildMap() {
        map_.resize(kMapW, kMapH, 0);
        map_.firstSolid = 1;
        for (int x = 0; x < kMapW; ++x) { map_.set(x, 0, 1); map_.set(x, kMapH - 1, 1); }
        for (int y = 0; y < kMapH; ++y) { map_.set(0, y, 1); map_.set(kMapW - 1, y, 1); }
        otacon::Random rng(0x8A17u + seed_++);
        for (int i = 0; i < 14; ++i) {
            const int w = rng.rangeI(1, 5), h = rng.rangeI(1, 5);
            const int x = rng.rangeI(2, kMapW - w - 2), y = rng.rangeI(2, kMapH - h - 2);
            for (int yy = y; yy < y + h; ++yy)
                for (int xx = x; xx < x + w; ++xx) map_.set(xx, yy, 1);
        }
    }

    // ---- raycasting --------------------------------------------------------
    void renderRays(otacon::IRenderer& r) {
        const float bx = ox(), by = oy();
        for (int y = 0; y < kMapH; ++y)
            for (int x = 0; x < kMapW; ++x)
                if (map_.solid(x, y))
                    r.fillRect(bx + x * kTile, by + y * kTile, kTile - 0.6f, kTile - 0.6f,
                               Color{0.20f, 0.23f, 0.31f, 1.f});

        const Vec2f origin{bx + kMapW * kTile * 0.5f, by + kMapH * kTile * 0.5f};

        // A fan of rays, to make line of sight legible as a shape rather than a
        // single line: everything lit is reachable from the origin.
        if (showFan_) {
            for (int i = 0; i < 90; ++i) {
                const float a = float(i) / 90.f * 6.28318f;
                const Vec2f dir{std::cos(a) * 600.f, std::sin(a) * 600.f};
                const otacon::RayHit h = ray::vsTileMap({origin.x - bx, origin.y - by}, dir, map_, kTile);
                const Vec2f end = h.hit ? Vec2f{bx + h.point.x, by + h.point.y}
                                        : Vec2f{origin.x + dir.x, origin.y + dir.y};
                r.drawLine(origin.x, origin.y, end.x, end.y, Color{0.30f, 0.55f, 0.85f, 0.16f}, 1.f);
            }
        }

        // The aimed ray, bounced off each surface using the returned normal.
        Vec2f p{origin.x - bx, origin.y - by};
        Vec2f d{mouse_.x - origin.x, mouse_.y - origin.y};
        const float len = std::sqrt(d.x * d.x + d.y * d.y);
        if (len > 1e-3f) { d.x = d.x / len * 900.f; d.y = d.y / len * 900.f; }

        hits_ = 0;
        for (int b = 0; b < bounces_; ++b) {
            const otacon::RayHit h = ray::vsTileMap(p, d, map_, kTile);
            const Vec2f from{bx + p.x, by + p.y};
            if (!h.hit) {
                r.drawLine(from.x, from.y, bx + p.x + d.x, by + p.y + d.y,
                           Color{1.f, 0.85f, 0.35f, 0.9f}, 1.f);
                break;
            }
            ++hits_;
            const Vec2f at{bx + h.point.x, by + h.point.y};
            r.drawLine(from.x, from.y, at.x, at.y, Color{1.f, 0.85f, 0.35f, 1.f}, 2.f);
            // The normal, drawn: this is the thing a bare distance cannot give you.
            r.drawLine(at.x, at.y, at.x + h.normal.x * 12.f, at.y + h.normal.y * 12.f,
                       Color{0.45f, 1.f, 0.60f, 1.f}, 1.f);
            r.drawRectOutline(at.x - 3, at.y - 3, 6, 6, Color{1.f, 0.45f, 0.40f, 1.f}, 1.f);
            r.drawRectOutline(bx + h.tileX * kTile, by + h.tileY * kTile, kTile, kTile,
                              Color{1.f, 0.45f, 0.40f, 0.5f}, 1.f);

            // Reflect: d - 2(d.n)n. Nudge off the surface so the next cast does
            // not immediately re-hit the cell it just left.
            const float dn = d.x * h.normal.x + d.y * h.normal.y;
            d = {d.x - 2.f * dn * h.normal.x, d.y - 2.f * dn * h.normal.y};
            p = {h.point.x + h.normal.x * 0.01f, h.point.y + h.normal.y * 0.01f};
        }
        r.fillRect(origin.x - 3, origin.y - 3, 6, 6, Color{1.f, 1.f, 1.f, 1.f});

        const float px = bx + kMapW * kTile + 12;
        float y = by;
        r.drawText("RAYCAST", px, y, 1.f, Color{0.55f, 0.80f, 1.f, 1.f}); y += 11;
        r.drawText("vsTileMap: a DDA walk,", px, y, 1.f, Color{0.62f, 0.68f, 0.80f, 1.f}); y += 8;
        r.drawText("so it costs the length", px, y, 1.f, Color{0.62f, 0.68f, 0.80f, 1.f}); y += 8;
        r.drawText("of the ray, not the size", px, y, 1.f, Color{0.62f, 0.68f, 0.80f, 1.f}); y += 8;
        r.drawText("of the map.", px, y, 1.f, Color{0.62f, 0.68f, 0.80f, 1.f}); y += 14;
        char t[64]; std::snprintf(t, sizeof t, "bounces  %d / %d", hits_, bounces_);
        r.drawText(t, px, y, 1.f, Color{1.f, 0.85f, 0.35f, 1.f}); y += 14;
        r.drawText("green = surface normal", px, y, 1.f, Color{0.45f, 1.f, 0.60f, 1.f}); y += 9;
        r.drawText("a bounce needs the face,", px, y, 1.f, Color{0.42f, 0.48f, 0.60f, 1.f}); y += 8;
        r.drawText("not just the distance", px, y, 1.f, Color{0.42f, 0.48f, 0.60f, 1.f});
    }

    // ---- shapes ------------------------------------------------------------
    void renderShapes(otacon::IRenderer& r) {
        const float cx = W_ * 0.34f, cy = H_ * 0.5f;
        const Rect box(otacon::R(cx - 60), otacon::R(cy - 40), otacon::R(120), otacon::R(80));
        const Circle probe{mouse_, 26.f};

        const bool hitBox = shape::overlaps(probe, box);
        r.fillRect(otacon::toFloat(box.x), otacon::toFloat(box.y),
                   otacon::toFloat(box.w), otacon::toFloat(box.h),
                   hitBox ? Color{0.45f, 0.30f, 0.28f, 1.f} : Color{0.20f, 0.23f, 0.31f, 1.f});
        drawCircle(r, probe.centre, probe.radius,
                   hitBox ? Color{1.f, 0.55f, 0.40f, 1.f} : Color{0.45f, 0.80f, 1.f, 1.f});

        // The clamped point: the whole circle-vs-box test in one marker.
        const Vec2f cp = shape::closestPointInRect(probe.centre, box);
        r.drawLine(probe.centre.x, probe.centre.y, cp.x, cp.y, Color{1.f, 0.85f, 0.35f, 0.8f}, 1.f);
        r.fillRect(cp.x - 2, cp.y - 2, 4, 4, Color{1.f, 0.85f, 0.35f, 1.f});

        // A capsule the probe can be tested against too.
        const Capsule cap{{W_ * 0.68f, cy - 60}, {W_ * 0.80f, cy + 50}, 18.f};
        const bool hitCap = shape::overlaps(cap, probe);
        drawCapsule(r, cap, hitCap ? Color{1.f, 0.55f, 0.40f, 1.f} : Color{0.35f, 0.55f, 0.45f, 1.f});
        const Vec2f sp = shape::closestPointOnSegment(probe.centre, cap.a, cap.b);
        r.drawLine(probe.centre.x, probe.centre.y, sp.x, sp.y, Color{1.f, 0.85f, 0.35f, 0.5f}, 1.f);

        // The manifold against a fixed circle: normal and depth, drawn.
        const Circle anchor{{W_ * 0.34f, cy + 130.f}, 22.f};
        drawCircle(r, anchor.centre, anchor.radius, Color{0.40f, 0.45f, 0.60f, 1.f});
        const otacon::Manifold m = shape::resolve(anchor, probe);
        if (m.hit) {
            r.drawLine(m.contact.x, m.contact.y,
                       m.contact.x + m.normal.x * m.depth, m.contact.y + m.normal.y * m.depth,
                       Color{1.f, 0.40f, 0.40f, 1.f}, 2.f);
            char t[64]; std::snprintf(t, sizeof t, "depth %.1f", m.depth);
            r.drawText(t, m.contact.x + 6, m.contact.y - 8, 1.f, Color{1.f, 0.40f, 0.40f, 1.f});
        }

        float y = layout::kTop + 6;
        r.drawText("SHAPES", 8, y, 1.f, Color{0.55f, 0.80f, 1.f, 1.f}); y += 11;
        r.drawText("yellow marker = the closest point.", 8, y, 1.f, Color{0.62f, 0.68f, 0.80f, 1.f}); y += 8;
        r.drawText("circle-vs-box is the distance to it,", 8, y, 1.f, Color{0.62f, 0.68f, 0.80f, 1.f}); y += 8;
        r.drawText("not an overlap of x and y spans -", 8, y, 1.f, Color{0.62f, 0.68f, 0.80f, 1.f}); y += 8;
        r.drawText("which is what gets corners wrong.", 8, y, 1.f, Color{0.62f, 0.68f, 0.80f, 1.f});
        r.drawText("drag the probe into a corner to see it", 8, H_ - 12, 1.f, Color{0.42f, 0.48f, 0.60f, 1.f});
    }

    static void drawCircle(otacon::IRenderer& r, Vec2f c, float rad, Color col) {
        const int n = 40;
        for (int i = 0; i < n; ++i) {
            const float a0 = float(i) / n * 6.28318f, a1 = float(i + 1) / n * 6.28318f;
            r.drawLine(c.x + std::cos(a0) * rad, c.y + std::sin(a0) * rad,
                       c.x + std::cos(a1) * rad, c.y + std::sin(a1) * rad, col, 1.f);
        }
    }
    static void drawCapsule(otacon::IRenderer& r, const Capsule& cap, Color col) {
        drawCircle(r, cap.a, cap.radius, col);
        drawCircle(r, cap.b, cap.radius, col);
        const Vec2f d = shape::normalize({cap.b.x - cap.a.x, cap.b.y - cap.a.y});
        const Vec2f n{-d.y, d.x};
        r.drawLine(cap.a.x + n.x * cap.radius, cap.a.y + n.y * cap.radius,
                   cap.b.x + n.x * cap.radius, cap.b.y + n.y * cap.radius, col, 1.f);
        r.drawLine(cap.a.x - n.x * cap.radius, cap.a.y - n.y * cap.radius,
                   cap.b.x - n.x * cap.radius, cap.b.y - n.y * cap.radius, col, 1.f);
    }

    otacon::TileMap map_;
    Vec2f mouse_{320, 200}, target_{180, 150};
    float W_ = 640, H_ = 400, t_ = 0;
    int   mode_ = 0, bounces_ = 3, hits_ = 0;
    std::uint32_t seed_ = 0;
    bool  showFan_ = true;
    mutable char buf_[96]{};
};

} // namespace

Sample* makeRayShapes() { return new RayShapes(); }

} // namespace samples
