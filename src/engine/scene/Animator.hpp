/*
===========================================================================

OTACON ENGINE
scene/Animator.hpp - sprite-sheet clips and the clock that drives them

Frame animation is two problems that are easy to meet tangled together:
*which* frame to show, which is a clock problem, and *where that frame lives in
the texture*, which is a UV problem. This separates them.

A Clip is a list of frame indices into a horizontal strip plus a rate. An
Animator owns the clock: tick it with a dt, ask it for the current frame, and
ask the sheet to turn that frame into a UV window. Nothing here touches the
renderer, so an animator can be stepped headlessly in a test.

===========================================================================
*/
#pragma once
#include "core/math/Scalar.hpp"
#include "render/RenderTypes.hpp"
#include <cstddef>

namespace otacon {

/*
==================
SpriteSheet

A horizontal strip of equally sized frames in one texture. Holding the frame
count rather than the pixel size is what keeps the UV maths to one divide.
==================
*/
struct SpriteSheet {
    TextureHandle texture = 0;
    int frameCount = 1;
    int frameW = 0, frameH = 0;

    // The UV window for frame f, clamped so a bad index cannot sample garbage.
    void uv(int f, float& u0, float& u1) const {
        const int n = frameCount > 0 ? frameCount : 1;
        f = f < 0 ? 0 : (f >= n ? n - 1 : f);
        u0 = float(f) / float(n);
        u1 = float(f + 1) / float(n);
    }
};

/*
==================
Clip

A named sequence of frame indices and how fast to walk them. The indices are a
list rather than a range so a ping-pong or a hold is just data - 0,1,2,3,2,1 is
a ping-pong, and a single-entry clip is a static pose.
==================
*/
struct Clip {
    const char* name   = "";
    const int*  frames = nullptr;
    int         count  = 0;
    float       fps    = 12.f;
    bool        loop   = true;
};

/*
==================
Animator

The clock. Deliberately separate from Clip so several entities can play the
same clip at different points in it without copying the frame list.
==================
*/
class Animator {
public:
    void play(const Clip* clip, bool restart = true) {
        if (clip_ == clip && !restart) return;
        clip_ = clip;
        cursor_ = 0;
        timer_ = 0.f;
        finished_ = false;
    }

    void update(Real dt) {
        if (!clip_ || clip_->count <= 0 || finished_) return;
        const float rate = clip_->fps > 0.f ? clip_->fps : 1.f;
        const float hold = 1.f / rate;
        timer_ += toFloat(dt);
        while (timer_ >= hold) {
            timer_ -= hold;
            if (cursor_ + 1 >= clip_->count) {
                // A non-looping clip stops on its last frame rather than
                // wrapping or going blank, which is almost always what a
                // one-shot (a death, a land) actually wants.
                if (clip_->loop) cursor_ = 0;
                else { finished_ = true; return; }
            } else {
                ++cursor_;
            }
        }
    }

    // Advance exactly one frame, ignoring the clock. For hand-cranking.
    void step(int n = 1) {
        if (!clip_ || clip_->count <= 0) return;
        cursor_ = ((cursor_ + n) % clip_->count + clip_->count) % clip_->count;
        timer_ = 0.f;
        finished_ = false;
    }

    int   frame() const { return (clip_ && clip_->count > 0) ? clip_->frames[cursor_] : 0; }
    int   cursor() const { return cursor_; }
    bool  finished() const { return finished_; }
    const Clip* clip() const { return clip_; }
    // How far through the current frame's hold we are, in 0..1 - for a HUD that
    // wants to show the clock rather than just the frame.
    float phase() const {
        if (!clip_ || clip_->fps <= 0.f) return 0.f;
        const float hold = 1.f / clip_->fps;
        const float p = timer_ / hold;
        return p < 0.f ? 0.f : (p > 1.f ? 1.f : p);
    }

private:
    const Clip* clip_ = nullptr;
    int   cursor_ = 0;
    float timer_ = 0.f;
    bool  finished_ = false;
};

} // namespace otacon
