// Node.hpp — a element of the scene tree.
//
// A Scene owns an ordered list of Nodes and drives them: it ticks each node's
// update() and then walks them in order to render(). Concrete nodes are things
// like a layer of sprites, a particle emitter, or a custom drawer. This is what
// keeps rendering in one place — the Scene renders the tree, nothing draws on
// the side.
#pragma once
#include "core/math/Scalar.hpp"

namespace otacon {

class IRenderer;
class Camera;

class Node {
public:
    virtual ~Node() = default;
    bool visible = true;

    // Per-frame animation for this node (particles, wrapping, etc.). Gameplay
    // that couples nodes (collision, camera follow) stays in the game systems.
    // The camera is handed in since most animation here is camera-relative.
    virtual void update(Real dt, const Camera& cam) { (void)dt; (void)cam; }

    // Draw this node through the camera. Called only when `visible`.
    virtual void render(IRenderer& r, const Camera& cam) const = 0;

    // Optional: outline this node's collision shapes for the C (colliders) debug
    // view. Nodes that own hand-rolled colliders not backed by scene Entities
    // (e.g. transient roof obstacles) override this so they still show up in the
    // overlay. Default does nothing.
    virtual void renderColliders(IRenderer& r, const Camera& cam) const { (void)r; (void)cam; }
};

} // namespace otacon
