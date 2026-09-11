// Registry.cpp — the one place that knows the full sample list.
//
// Each sample .cpp keeps its class in an anonymous namespace and exposes a
// single factory function; this file declares those factories and puts them in
// order. Adding a sample is: write the .cpp, declare it here, add one row.
// Nothing else in the project changes — not CMake (the target globs), not the
// gallery, not main().
#include "Sample.hpp"

namespace samples {

Sample* makeHelloSprite();
Sample* makeSpriteAnimation();
Sample* makeTilemapWorld();
Sample* makeParallaxCamera();
Sample* makeLighting();
Sample* makeParticles();
Sample* makePhysicsPlayground();
Sample* makePendulum();
Sample* makeChainRope();
Sample* makePlatformerController();
Sample* makeTopDownMovement();
Sample* makePathfindingBoids();
Sample* makeShaderPlayground();
Sample* makeProceduralDungeon();
Sample* makeStressTest();

namespace {
const SampleInfo kSamples[] = {
    {"Hello Sprite",       "four ways to draw, one triangle path underneath",      makeHelloSprite},
    {"Sprite Animation",   "a frame is a clock problem and a UV problem",          makeSpriteAnimation},
    {"Tilemap World",      "a world of indices, drawn only where you can see",     makeTilemapWorld},
    {"Parallax Camera",    "depth is one multiply: scroll x scrollFactor",         makeParallaxCamera},
    {"2D Lighting + Normals", "the same N.L on the CPU and on the GPU",            makeLighting},
    {"Particles",          "an emitter is a pool of ordinary entities",            makeParticles},
    {"Physics Playground", "AABB separation, one axis at a time, no bounce",       makePhysicsPlayground},
    {"Pendulum",           "the integrator you pick is visible in the energy",     makePendulum},
    {"Chain & Rope",       "verlet points, and relaxation as the stiffness knob",  makeChainRope},
    {"Platformer Controller", "game feel is five timers you can switch off",       makePlatformerController},
    {"Top-Down Movement",  "normalise the stick, and let the camera lag",          makeTopDownMovement},
    {"Pathfinding / Boids","one global search, one set of local rules",            makePathfindingBoids},
    {"Shader Playground",  "the same effect either side of the renderer seam",     makeShaderPlayground},
    {"Procedural Dungeon", "generation you can single-step, with a seed",          makeProceduralDungeon},
    {"Stress Test",        "find the wall, and learn which wall it is",            makeStressTest},
};
} // namespace

int sampleCount() { return int(sizeof kSamples / sizeof kSamples[0]); }

const SampleInfo& sampleInfo(int index) {
    const int n = sampleCount();
    if (index < 0) index = 0;
    if (index >= n) index = n - 1;
    return kSamples[index];
}

} // namespace samples
