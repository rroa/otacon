/*
===========================================================================

OTACON ENGINE
core/Tween.hpp - animating a value over time

Ease.hpp shapes a 0..1. This drives one.

Most of what reads as polish in a game is a tween: a menu sliding in, a pickup
popping, a health bar catching up, a screen fading. Written by hand each time it
is a float, a duration, a timer and an if, repeated per effect -- which is how
"just add a fade" turns into four new members on a class that had nothing to do
with fading.

A tween writes through a pointer, so it animates a value you already have rather
than one you have to go and read back. That makes it usable on a struct field
you do not control, which is where these are usually needed.

===========================================================================
*/
#pragma once
#include "core/math/Ease.hpp"
#include "core/math/Scalar.hpp"
#include "core/math/Vector.hpp"
#include <cstdint>
#include <functional>
#include <vector>

namespace otacon {

using TweenHandle = std::uint32_t;      // 0 == invalid
using EaseFn = float (*)(float);

class Tweens {
public:
    /*
    ==================
    to

    Animate *target from its current value to `end`. The start is captured now,
    not at construction of some builder, so chaining a second tween onto a value
    mid-flight begins from where it actually is.
    ==================
    */
    TweenHandle to(float* target, float end, float seconds,
                   EaseFn easing = ease::smoothstep, std::function<void()> done = {}) {
        if (!target || seconds <= 0.f) {
            if (target) *target = end;
            if (done) done();
            return 0;
        }
        Entry e;
        e.handle = ++next_;
        e.target = target;
        e.from = *target;
        e.to = end;
        e.duration = seconds;
        e.easing = easing ? easing : ease::smoothstep;
        e.done = std::move(done);
        entries_.push_back(std::move(e));
        return e.handle;
    }

    // Two channels driven by one clock, so an x/y move cannot desynchronise.
    TweenHandle toVec(Vec2f* target, Vec2f end, float seconds,
                      EaseFn easing = ease::smoothstep, std::function<void()> done = {}) {
        if (!target) return 0;
        to(&target->x, end.x, seconds, easing);
        return to(&target->y, end.y, seconds, easing, std::move(done));
    }

    // Cancel where it stands. `settle` jumps it to its end first, which is what
    // you want when skipping an intro rather than freezing it half-played.
    void cancel(TweenHandle h, bool settle = false) {
        for (Entry& e : entries_) {
            if (e.handle != h || !e.alive) continue;
            if (settle && e.target) *e.target = e.to;
            e.alive = false;
            return;
        }
    }
    // Every tween writing to this address. The form you need when an object dies
    // and must not be written through afterwards.
    void cancelTarget(const float* target) {
        for (Entry& e : entries_) if (e.target == target) e.alive = false;
    }
    void cancelAll() { entries_.clear(); }

    bool active(TweenHandle h) const {
        for (const Entry& e : entries_) if (e.handle == h && e.alive) return true;
        return false;
    }
    std::size_t count() const {
        std::size_t n = 0;
        for (const Entry& e : entries_) if (e.alive) ++n;
        return n;
    }

    void update(Real dt) {
        const float d = toFloat(dt);
        if (d <= 0.f) return;

        for (std::size_t i = 0; i < entries_.size(); ++i) {
            Entry& e = entries_[i];
            if (!e.alive) continue;
            e.elapsed += d;
            const float t = e.elapsed >= e.duration ? 1.f : e.elapsed / e.duration;
            const float k = e.easing(t);
            if (e.target) *e.target = e.from + (e.to - e.from) * k;

            if (t >= 1.f) {
                e.alive = false;
                // Copy before calling: the callback may start another tween and
                // reallocate the vector out from under `e`.
                const std::function<void()> fn = e.done;
                if (fn) fn();
            }
        }
        std::size_t w = 0;
        for (std::size_t r = 0; r < entries_.size(); ++r) {
            if (!entries_[r].alive) continue;
            // Guard the self-move. Moving an element onto itself is a
            // self-move-assignment, and for std::function that leaves the
            // target in a valid but UNSPECIFIED state -- in practice empty. On
            // the pass where nothing has been retired, w == r for every entry,
            // so an unguarded compaction silently erases every callback it was
            // supposed to be preserving.
            if (w != r) entries_[w] = std::move(entries_[r]);
            ++w;
        }
        entries_.resize(w);
    }

private:
    struct Entry {
        TweenHandle handle = 0;
        float* target = nullptr;
        float from = 0.f, to = 0.f;
        float duration = 0.f, elapsed = 0.f;
        EaseFn easing = ease::smoothstep;
        bool alive = true;
        std::function<void()> done;
    };

    std::vector<Entry> entries_;
    TweenHandle next_ = 0;
};

} // namespace otacon
