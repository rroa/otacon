// GravityScene.hpp — Build 1.b: gravity, the one idea.
//
// Same yellow square, but now nothing holds it up: gravity is a constant
// downward *acceleration*, and a flap is a single upward kick to the velocity.
// The gravity force is adjustable at runtime (F1/F2) so you can feel how the
// number changes the fall. Still no collision and no world — if you stop
// flapping the bird simply accelerates off the bottom of the screen, which is
// exactly the point of this build.
#pragma once
#include "flappy/scene/Scene.hpp"
#include "flappy/sim/Bird.hpp"

namespace flappy {

// Not 'final': Build 2 (TextureScene) extends this, reusing the physics.
class GravityScene : public Scene {
public:
    void enter() override;
    void handleInput(const otacon::InputFrame& in) override;
    void update(otacon::Real dt) override;
    void render(otacon::IRenderer& r) const override;
    const char* name() const override { return "1.b - gravity"; }
    const char* status() const override;

protected:
    // Protected so a textured build (Build 2) can reuse this exact physics and
    // override only how the bird is drawn.
    Bird  bird_;
    float gravity_ = 0;             // px/s^2, tunable at runtime

private:
    mutable char status_[64]{};
};

} // namespace flappy
