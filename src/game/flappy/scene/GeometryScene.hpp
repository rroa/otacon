// GeometryScene.hpp — Build 1: pure kinematics, no physics.
//
// A yellow square stands in for the bird at its fixed horizontal position. Hold
// the flap key to rise, release to sink — both at a constant speed. There is no
// gravity, no collision, no world: the only thing to feel here is position and
// how input maps to vertical movement. Gravity arrives in the next build.
#pragma once
#include "flappy/scene/Scene.hpp"
#include "flappy/sim/Bird.hpp"

namespace flappy {

class GeometryScene final : public Scene {
public:
    void enter() override;
    void handleInput(const otacon::InputFrame& in) override;
    void update(otacon::Real dt) override;
    void render(otacon::IRenderer& r) const override;
    const char* name() const override { return "1 - geometry / kinematics"; }

private:
    Bird bird_;
    bool rising_ = false;   // flap key held this frame
};

} // namespace flappy
