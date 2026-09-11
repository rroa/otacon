/*
===========================================================================

OTACON ENGINE
scene/Emitter.cpp - particle emitter

A port of flixel's FlxEmitter. The pool is a fixed array of ordinary
Entities, recycled round-robin, which is the whole design: a particle falls,
drags and spins through the same integrator as a player, so there is no
second physics path to keep in step with the first.

Recycling round-robin rather than searching for a free slot means emitting is
O(1) and the pool never fragments; the cost is that a burst larger than the
pool eats its own oldest particles, which is the right trade for an effect.

===========================================================================
*/
#include "scene/Emitter.hpp"

namespace otacon {

/*
==================
Emitter::init

Allocate the pool. Everything starts dead: a particle only exists between
an emit and its cull.
==================
*/
void Emitter::init(int count) {
    pool_.assign(std::size_t(count < 1 ? 1 : count), Entity{});
    for (Entity& p : pool_) {
        p.exists = false; p.visible = false; p.solid = false; p.moves = true;
        p.size = {R(particleSize.x), R(particleSize.y)};
        p.color = color;
    }
    next_ = 0;
}

/*
==================
Emitter::start

Explode bursts the whole quantity at once and then switches itself off,
because a burst is a one-shot; continuous leaves the timer running.
==================
*/
void Emitter::start(bool explode, int quantity) {
    quantity_ = quantity;
    timer_ = 0;
    if (explode) {
        int n = quantity_ > 0 ? quantity_ : int(pool_.size());
        for (int i = 0; i < n; ++i) emitParticle();
        on_ = false;                  // a burst is one-shot
    } else {
        on_ = true;                   // keep emitting on the delay interval
    }
}

/*
=====================
Emitter::emitParticle

Roll one particle's spawn state from the configured ranges. Frame choice is
rolled once here rather than animated later, so a sheet-backed particle
keeps one look for its whole life.
=====================
*/
void Emitter::emitParticle() {
    Entity& p = pool_[next_];
    p.exists = true; p.visible = true; p.dead = false; p.solid = false; p.moves = true;
    p.size = {R(particleSize.x), R(particleSize.y)};
    p.color = color;
    p.scrollFactor = scrollFactor;
    p.pos = {R(position.x + unit() * area.x), R(position.y + unit() * area.y)};
    p.velocity = {R(range(minSpeed.x, maxSpeed.x)), R(range(minSpeed.y, maxSpeed.y))};
    p.acceleration = {R(0), R(gravity)};
    p.drag = {R(drag.x), R(drag.y)};
    p.maxVelocity = {R(10000), R(10000)};
    p.angularVelocity = R(range(minRotation, maxRotation));
    p.angle = R(unit() * 360.f - 180.f);
    // Pick a random frame from the sheet (uv stored on the particle).
    int frames = frameCols * frameRows;
    if (frames > 1) {
        int f = int(unit() * float(frames)) % frames;
        float fw = 1.f / float(frameCols), fh = 1.f / float(frameRows);
        float fx = float(f % frameCols), fy = float(f / frameCols);
        p.uv0 = {fx * fw, fy * fh};
        p.uv1 = {(fx + 1) * fw, (fy + 1) * fh};
    } else {
        p.uv0 = {0, 0}; p.uv1 = {1, 1};
    }
    next_ = (next_ + 1) % pool_.size();
}

/*
==================
Emitter::update

Emit on the interval if continuous, integrate everything alive, and cull
anything well past the screen edge. The cull margin is generous on purpose:
a particle that is briefly off-screen may still come back.
==================
*/
void Emitter::update(Real dt, const Camera& cam) {
    if (on_ && delay > 0) {           // continuous emission
        timer_ += toFloat(dt);
        while (timer_ > delay) { timer_ -= delay; emitParticle(); }
    }
    const float W = float(cam.width()), H = float(cam.height());
    for (Entity& p : pool_) {
        if (!p.exists) continue;
        p.update(dt);
        Vec2f s = cam.screen(p);      // recycle once fully off-screen
        if (s.x < -64 || s.x > W + 64 || s.y < -64 || s.y > H + 64) p.exists = false;
    }
}

/*
==================
Emitter::render

Textured particles draw a rotated sheet frame tinted only by alpha, so the
art keeps its own colour; untextured ones are rotated solid quads.
==================
*/
void Emitter::render(IRenderer& r, const Camera& cam) const {
    for (const Entity& p : pool_) {
        if (!p.exists || !p.visible) continue;
        Vec2f s = cam.screen(p);
        if (texture) {
            // Textured: draw the particle's frame at sprite size, tinted only by
            // alpha so the grey art keeps its own shades.
            float w = spriteSize.x, h = spriteSize.y;
            Color tint{1, 1, 1, p.color.a};
            r.drawImageRotated(texture, s.x + w * 0.5f, s.y + h * 0.5f, w, h,
                               toFloat(p.angle), p.uv0.x, p.uv0.y, p.uv1.x, p.uv1.y, tint);
        } else {
            float w = toFloat(p.size.x), h = toFloat(p.size.y);
            r.fillRotatedRect(s.x + w * 0.5f, s.y + h * 0.5f, w, h, toFloat(p.angle), p.color);
        }
    }
}

/*
==================
Emitter::liveCount

How many particles are alive. The number that decides whether an effect is
affordable, so it is worth having cheaply to hand.
==================
*/
int Emitter::liveCount() const {
    int n = 0;
    for (const Entity& p : pool_) if (p.exists) ++n;
    return n;
}

} // namespace otacon
