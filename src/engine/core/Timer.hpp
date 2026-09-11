/*
===========================================================================

OTACON ENGINE
core/Timer.hpp - deferred and repeating callbacks

"Do this in two seconds" and "do this every half second" are constant in game
code, and without somewhere to put them they become a float member, a manual
decrement and an if -- per effect, per entity, forever.

Handles rather than raw callbacks, because the thing you schedule usually
outlives the moment you scheduled it, and cancelling by identity is the only
safe way to stop a timer belonging to an entity that has just died.

Timers are stepped explicitly with update(dt), not from a global clock. That
keeps them inside whatever time scale the caller is running: pause the
simulation and its timers pause with it, which is almost never what a
wall-clock timer would do.

===========================================================================
*/
#pragma once
#include "core/math/Scalar.hpp"
#include <cstdint>
#include <functional>
#include <vector>

namespace otacon {

using TimerHandle = std::uint32_t;      // 0 == invalid

class Timers {
public:
    /*
    ==================
    after / every

    after() fires once and retires. every() fires forever until cancelled.
    Both return a handle; ignoring it is fine for a fire-and-forget effect.
    ==================
    */
    TimerHandle after(float seconds, std::function<void()> fn) {
        return add(seconds, std::move(fn), false, 1);
    }
    TimerHandle every(float seconds, std::function<void()> fn) {
        return add(seconds, std::move(fn), true, 0);
    }
    // Repeat a fixed number of times, then retire.
    TimerHandle repeat(float seconds, int count, std::function<void()> fn) {
        return add(seconds, std::move(fn), true, count);
    }

    void cancel(TimerHandle h) {
        if (!h) return;
        for (Entry& e : entries_) if (e.handle == h) { e.alive = false; return; }
    }
    void cancelAll() { entries_.clear(); }

    bool active(TimerHandle h) const {
        for (const Entry& e : entries_) if (e.handle == h && e.alive) return true;
        return false;
    }
    // How long until this timer next fires, for a cooldown readout.
    float remaining(TimerHandle h) const {
        for (const Entry& e : entries_) if (e.handle == h && e.alive) return e.remaining;
        return 0.f;
    }
    std::size_t count() const {
        std::size_t n = 0;
        for (const Entry& e : entries_) if (e.alive) ++n;
        return n;
    }

    /*
    ==================
    update

    One tick. Two things here are deliberate and easy to get wrong.

    A callback may schedule or cancel timers, so the list can be reallocated
    mid-iteration: iterate by index, re-read the entry after every call, and
    never hold a reference across one.

    An interval shorter than dt would otherwise silently drop firings, so the
    loop catches up -- but with a bound, because a zero interval would
    otherwise hang the frame.
    ==================
    */
    void update(Real dt) {
        const float d = toFloat(dt);
        if (d <= 0.f) return;

        for (std::size_t i = 0; i < entries_.size(); ++i) {
            if (!entries_[i].alive) continue;
            entries_[i].remaining -= d;

            // Fire slightly early rather than exactly at zero. A one-second
            // timer stepped at 1/60 subtracts sixty times, and the accumulated
            // rounding leaves `remaining` a hair ABOVE zero -- so a strict test
            // silently defers it by a whole frame.
            //
            // The tolerance is RELATIVE to the interval because the error is
            // too. It is worst in the fixed-point build, where Q16.16 cannot
            // represent 1/60 and quantises it 0.025% short: a one-second timer
            // ends 244us away, and a ten-second one ten times that. A fixed
            // epsilon would cover the first and not the second. A thousandth of
            // the interval tracks the accumulation and stays far below anything
            // observable; the absolute floor keeps a zero interval sane.
            const float tol = entries_[i].interval * kFireTolerance + 1e-6f;
            int guard = 0;
            while (entries_[i].alive && entries_[i].remaining <= tol && guard++ < kMaxCatchUp) {
                const std::function<void()> fn = entries_[i].fn;   // copy: fn may cancel us

                if (entries_[i].repeating) {
                    entries_[i].remaining += entries_[i].interval;
                    if (entries_[i].left > 0 && --entries_[i].left == 0) entries_[i].alive = false;
                    if (entries_[i].interval <= 0.f) entries_[i].alive = false;  // would spin
                } else {
                    entries_[i].alive = false;
                }
                if (fn) fn();
            }
        }
        // Compact once per tick rather than erasing mid-iteration.
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
    static constexpr int   kMaxCatchUp = 16;
    static constexpr float kFireTolerance = 1e-3f;   // a thousandth of the interval

    struct Entry {
        TimerHandle handle = 0;
        float interval = 0.f, remaining = 0.f;
        int   left = 0;            // firings remaining; 0 == unlimited
        bool  repeating = false, alive = true;
        std::function<void()> fn;
    };

    TimerHandle add(float seconds, std::function<void()> fn, bool repeating, int count) {
        Entry e;
        e.handle = ++next_;
        e.interval = seconds;
        e.remaining = seconds;
        e.repeating = repeating;
        e.left = count;
        e.fn = std::move(fn);
        entries_.push_back(std::move(e));
        return e.handle;
    }

    std::vector<Entry> entries_;
    TimerHandle next_ = 0;
};

} // namespace otacon
