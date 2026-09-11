// Emitter.hpp — particle emitter (Otacon engine), a port of flixel FlxEmitter.
//
// Owns a fixed pool of particle Entities and recycles them round-robin. Two
// modes: "explode" (burst `quantity` at once — gibs, glass shards) and
// continuous (one particle every `delay` seconds — drifting smoke). Particles
// are ordinary Entities, so they fall/drift under the same physics as everything
// else (gravity via acceleration.y, drag, and spin via angularVelocity). They
// render as solid rotated quads, or — when `texture` is set — as a random frame
// of a sprite sheet, spinning with the particle's angle.
#pragma once
#include "scene/Entity.hpp"
#include "scene/Camera.hpp"
#include "scene/Node.hpp"
#include "render/IRenderer.hpp"
#include <cstdint>
#include <vector>

namespace otacon {

class Emitter : public Node {
public:
    // ---- spawn configuration (set before start) ----
    Vec2f position{0, 0};            // emitter origin (world units)
    Vec2f area{0, 0};                // particles spawn randomly within this box
    Vec2f minSpeed{-100, -100};      // velocity range
    Vec2f maxSpeed{100, 100};
    float minRotation = -360, maxRotation = 360;   // deg/sec spin range
    float gravity = 400;
    Vec2f drag{0, 0};
    float delay = 0.1f;              // continuous: seconds between particles
    Vec2f scrollFactor{1, 1};        // parallax for the particles
    Color color{1, 1, 1, 1};
    Vec2f particleSize{2, 2};

    // Optional sprite sheet. When `texture` is set particles draw a random frame
    // from a (frameCols x frameRows) grid at `spriteSize`, spinning with the
    // particle's angle; otherwise they draw as solid `color` quads. Toggling
    // `texture` on/off is how the reveal switches particles geometry<->sprite.
    TextureHandle texture = 0;
    Vec2f spriteSize{8, 8};
    int   frameCols = 1, frameRows = 1;

    void init(int count);            // allocate the pool
    void seed(std::uint32_t s) { rng_ = s ? s : 1u; }

    void start(bool explode, int quantity = 0);   // begin emitting
    void stop() { on_ = false; }
    void update(Real dt, const Camera& cam) override;      // emit + integrate + cull
    void render(IRenderer& r, const Camera& cam) const override;

    void emitAt(Vec2f p) { position = p; }
    int  liveCount() const;

private:
    void emitParticle();
    float unit() { rng_ = rng_ * 1664525u + 1013904223u; return float(rng_ >> 8) / float(1u << 24); }
    float range(float a, float b) { return a + (b - a) * unit(); }

    std::vector<Entity> pool_;
    std::size_t next_ = 0;
    bool   on_ = false;
    float  timer_ = 0;
    int    quantity_ = 0;
    std::uint32_t rng_ = 0xBADC0DE;
};

} // namespace otacon
