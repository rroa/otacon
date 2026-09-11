#include "scene/Scene.hpp"
#include <cmath>

namespace otacon {

void Scene::render(IRenderer& r, const DebugRuntime& dbg) const {
    // 0) backdrop nodes — the farthest parallax, behind even the entity layer.
    for (const Node* n : backdrop_)
        if (n->visible) n->render(r, camera);
    // 1) the entity draw layer (back-most of the entities).
    for (const Entity* e : drawList_) {
        if (!e->exists || !e->visible || !e->renderable) continue;
        Vec2f s = camera.screen(*e);
        float x = s.x - e->offset.x, y = s.y - e->offset.y;
        float w = toFloat(e->size.x), h = toFloat(e->size.y);
        if (e->texture)
            r.drawImage(e->texture, x, y, w, h, e->uv0.x, e->uv0.y, e->uv1.x, e->uv1.y, e->color);
        else
            r.fillRect(x, y, w, h, e->color);
    }
    // 2) the scene nodes, in order, on top.
    for (const Node* n : nodes_)
        if (n->visible) n->render(r, camera);
    // 3) debug overlays.
    renderOverlays(r, dbg);
}

void Scene::renderOverlays(IRenderer& r, const DebugRuntime& dbg) const {
    const float W = float(camera.width()), H = float(camera.height());

    if (dbg.enabled(DebugView::Grid)) {
        Color g{1, 1, 1, 0.12f};
        float ox = std::fmod(toFloat(camera.scroll.x), 32.f);
        float oy = std::fmod(toFloat(camera.scroll.y), 32.f);
        for (float x = ox; x < W; x += 32.f) r.drawLine(x, 0, x, H, g, 1.f);
        for (float y = oy; y < H; y += 32.f) r.drawLine(0, y, W, y, g, 1.f);
    }

    if (dbg.enabled(DebugView::ParallaxBands)) {
        for (const Entity* e : drawList_) {
            if (!e->visible) continue;
            Vec2f s = camera.screen(*e);
            // Hue-ish tint keyed to the layer's x parallax factor.
            float f = e->scrollFactor.x;
            Color c{0.2f + 0.6f * std::fmin(f, 1.f), 0.4f, 1.f - 0.5f * std::fmin(f, 1.f), 0.9f};
            r.drawRectOutline(s.x - e->offset.x, s.y - e->offset.y,
                              toFloat(e->size.x), toFloat(e->size.y), c, 1.f);
        }
    }

    if (dbg.enabled(DebugView::Colliders)) {
        Color green{0.2f, 1.f, 0.3f, 1.f};
        for (const Entity* e : drawList_) {
            if (!e->solid || !e->exists) continue;
            Vec2f s = camera.screen(*e);
            r.drawRectOutline(s.x, s.y, toFloat(e->size.x), toFloat(e->size.y), green, 1.f);
        }
        // Nodes with their own (non-Entity) colliders draw them here too.
        for (const Node* n : nodes_)
            if (n->visible) n->renderColliders(r, camera);
    }

    if (dbg.enabled(DebugView::Velocities)) {
        Color yellow{1.f, 0.9f, 0.1f, 1.f};
        for (const Entity* e : drawList_) {
            if (!e->moves || !e->exists) continue;
            Vec2f s = camera.screen(*e);
            float cx = s.x + toFloat(e->size.x) * 0.5f, cy = s.y + toFloat(e->size.y) * 0.5f;
            r.drawLine(cx, cy, cx + toFloat(e->velocity.x) * 0.1f,
                       cy + toFloat(e->velocity.y) * 0.1f, yellow, 1.f);
        }
    }

    if (dbg.enabled(DebugView::CameraFocus)) {
        Color red{1.f, 0.2f, 0.2f, 1.f};
        float cx = W * 0.5f, cy = H * 0.5f;     // camera centers its target
        r.drawLine(cx - 6, cy, cx + 6, cy, red, 1.f);
        r.drawLine(cx, cy - 6, cx, cy + 6, red, 1.f);
    }
}

} // namespace otacon
