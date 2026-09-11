// PipesScene.hpp — Build 3: motion, via scrolling pipes.
//
// Same textured bird and gravity as Build 2, but now a field of pipe pairs
// scrolls in from the right at a constant speed. The lesson: with only a flat
// background you can't tell the bird is "moving" — it's the pipes sweeping past
// (the bird's X is pinned) that sells the motion. Drawn as plain geometry; no
// collision, no scoring — just experimentation. Pipe texture comes in Build 4.
#pragma once
#include "flappy/scene/TextureScene.hpp"
#include "flappy/sim/Pipe.hpp"
#include <random>
#include <vector>

namespace flappy {

// Not 'final': Build 4 (PipeTextureScene) extends this, reusing the pipe field.
class PipesScene : public TextureScene {
public:
    void enter() override;
    void update(otacon::Real dt) override;
    void render(otacon::IRenderer& r) const override;   // layered: bg → pipes → fg → bird
    const char* name() const override { return "3 - pipes (moving)"; }
    const char* status() const override;

protected:
    std::vector<Pipe> pipes_;            // shared with the textured-pipe build

    // Render layers, drawn in this order. Builds override the slice they change:
    // Build 4 the pipes, Build 6 the background + foreground, Build 5 the bird.
    virtual void drawBackground(otacon::IRenderer&) const {}   // default: clear color shows through
    virtual void drawPipes(otacon::IRenderer& r) const;        // default: green rectangles
    virtual void drawForeground(otacon::IRenderer&) const {}   // default: nothing (ground arrives in Build 6)

private:
    void spawn(float x);                 // append a pipe pair with a random gap

    std::mt19937      rng_{0xF1A99u};    // fixed seed → deterministic captures
    mutable char      status_[64]{};
};

} // namespace flappy
