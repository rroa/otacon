/*
===========================================================================

OTACON ENGINE
scene/Scene.hpp - camera, entity layer and node tree

Nodes the Scene owns and drives. Each frame the Scene ticks every node's
update() and then renders, in order: the entity layer (each Entity as a solid
or textured rect, parallax via its scrollFactor), then the nodes on top
(emitters, sprite drawers), then the debug overlays. Keeping the node list
here is what makes the Scene drive all rendering — nothing draws on the side.

Entities are the simulation/collision data (buildings, the player box); the
game rebuilds the draw list each frame. Nodes are longer-lived scene pieces
(particle emitters, the player sprite, building facades) added once per mode.

===========================================================================
*/
#pragma once
#include "scene/Camera.hpp"
#include "scene/Node.hpp"
#include "core/debug/Debug.hpp"
#include "render/IRenderer.hpp"
#include <vector>

namespace otacon {

class Scene {
public:
    Color  bgColor{0.69f, 0.69f, 0.75f, 1.f};
    Camera camera;

    // Entity draw layer (rebuilt each frame by the game).
    void clear() { drawList_.clear(); }
    void add(Entity* e) { drawList_.push_back(e); }
    const std::vector<Entity*>& entities() const { return drawList_; }

    // Scene nodes (added once per mode; persist across frames). Backdrop nodes
    // render *before* the entity layer — for the farthest parallax pieces (the
    // distant walkers) that must sit behind the skyline/midground entities.
    void addBackdropNode(Node* n) { backdrop_.push_back(n); }
    void addNode(Node* n) { nodes_.push_back(n); }
    void clearNodes() { backdrop_.clear(); nodes_.clear(); }

    void update(Real dt) {
        for (Node* n : backdrop_) n->update(dt, camera);
        for (Node* n : nodes_)    n->update(dt, camera);
    }
    void render(IRenderer& r, const DebugRuntime& dbg) const;

private:
    std::vector<Entity*> drawList_;
    std::vector<Node*>   backdrop_;
    std::vector<Node*>   nodes_;
    void renderOverlays(IRenderer& r, const DebugRuntime& dbg) const;
};

} // namespace otacon
