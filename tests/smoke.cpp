// smoke.cpp — headless engine self-tests. No window/renderer needed: the
// simulation is fully decoupled from rendering, so we can step it and assert.
// Run via the `otacon_smoke` target. Returns non-zero if any check fails.
#include "scene/Entity.hpp"
#include "scene/Collision.hpp"
#include "scene/Camera.hpp"
#include "scene/Emitter.hpp"
#include "core/math/Fixed.hpp"
#include "canabalt/Player.hpp"
#include "canabalt/Sequence.hpp"
#include "asset/Image.hpp"
#include "asset/Audio.hpp"
#include "audio/Mixer.hpp"
#include "core/math/Random.hpp"
#include "core/math/Noise.hpp"
#include "core/math/Ease.hpp"
#include "scene/TileMap.hpp"
#include "scene/PathFinder.hpp"
#include "scene/Verlet.hpp"
#include "scene/Steering.hpp"
#include "scene/SpatialGrid.hpp"
#include "scene/Raycast.hpp"
#include "scene/Animator.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

using namespace otacon;

static int failures = 0;
static void check(bool ok, const char* msg) {
    std::printf("[test] %-40s %s\n", msg, ok ? "PASS" : "FAIL");
    if (!ok) ++failures;
}

// A falling box must land on a static platform and come to rest on its surface.
static void testGravityLanding() {
    Entity ground; ground.pos = {R(0), R(200)}; ground.size = {R(400), R(100)};
    ground.fixed = true; ground.moves = true; ground.solid = true;

    Entity box; box.pos = {R(100), R(40)}; box.size = {R(16), R(18)};
    box.acceleration = {R(0), R(1200)}; box.maxVelocity = {R(10000), R(360)}; box.solid = true;

    std::vector<Entity*> group = {&ground};
    const Real dt = R(1.0f / 60.0f);
    bool everLanded = false;
    for (int i = 0; i < 240; ++i) {
        ground.update(dt);            // refresh static hulls (the bug we fixed)
        box.update(dt);
        collideWithGroup(box, group);
        if (box.onFloor) everLanded = true;
    }
    float y = toFloat(box.pos.y);
    check(everLanded, "gravity: box reports onFloor");
    check(y > 180.0f && y < 184.0f, "gravity: box rests on platform surface (~182)");
}

// Tapping jump while grounded must launch the player upward.
static void testPlayerJump() {
    canabalt::Player p;
    p.reset(R(40), R(200 - 18 - 4));
    Entity ground; ground.pos = {R(-200), R(200)}; ground.size = {R(2000), R(200)};
    ground.fixed = true; ground.moves = true; ground.solid = true;
    std::vector<Entity*> grp = {&ground};
    const Real dt = R(1.0f / 60.0f);

    // Settle on the ground (not touching).
    for (int i = 0; i < 30; ++i) { p.touching = false; ground.update(dt); p.update(dt); collideWithGroup(p, grp); }
    check(p.onFloor, "jump: player is grounded before the tap");
    float yBefore = toFloat(p.pos.y);

    // Tap: hold jump for a few frames.
    bool launched = false;
    for (int i = 0; i < 6; ++i) {
        p.touching = true; ground.update(dt); p.update(dt); collideWithGroup(p, grp);
        if (toFloat(p.velocity.y) < -10.f) launched = true;
    }
    check(launched, "jump: tapping while grounded launches upward (velocity.y<0)");
    check(toFloat(p.pos.y) < yBefore - 2.f, "jump: player actually rose off the roof");
}

// Running through a hallway window must emit a smash event (for glass shards).
static void testWindowSmash() {
    canabalt::Player p;
    p.reset(R(0), R(80 - 14));
    canabalt::GameRandom rng;
    canabalt::SequenceField field;
    field.init(&p, rng);
    Camera cam; cam.setViewport(480, 320);
    bool smashed = false;
    for (int i = 0; i < 3000 && !smashed; ++i) {
        p.pos.x = R(float(i) * 2);                 // march the player to the right
        field.update(R(1.0f / 60.0f), cam);
        if (!field.windowSmashes().empty()) smashed = true;
    }
    check(smashed, "glass: running through a hallway window emits a smash event");
}

// computeVelocity must clamp to maxVelocity and respect the unbounded sentinel.
static void testComputeVelocity() {
    Real v = computeVelocity(R(350), R(1200), R(0), R(360), R(1.0f / 60.0f));
    check(toFloat(v) <= 360.5f, "computeVelocity clamps to maxVelocity");
}

// Q16.16 fixed-point sanity (independent of the build's scalar choice).
static void testFixed() {
    Fixed a(2.5f), b(4.0f);
    check(std::abs((a * b).toFloat() - 10.0f) < 0.01f, "fixed: 2.5 * 4.0 == 10");
    check(std::abs(sqrtFx(Fixed(9)).toFloat() - 3.0f) < 0.02f, "fixed: sqrt(9) == 3");
}

// The ported Sequence.m generator must spawn the player onto the launch
// hallway and let him run a while without dying (the start is survivable).
static void testGeneratorStart() {
    canabalt::Player p;
    p.reset(R(0), R(80 - 14));
    canabalt::GameRandom rng;               // deterministic by default
    canabalt::SequenceField field;
    field.init(&p, rng);
    Camera cam; cam.setViewport(480, 320);

    const Real dt = R(1.0f / 60.0f);
    bool landed = false;
    for (int i = 0; i < 120; ++i) {           // 2 s, no jumping
        p.touching = false;
        p.update(dt);
        field.update(dt, cam);
        collideWithGroup(p, field.collisionBlocks());
        cam.follow(&p, R(15));                // keep scroll roughly tracking so generator culls sanely
        if (p.onFloor) landed = true;
    }
    check(!field.collisionBlocks().empty(), "generator: produced collision blocks");
    check(landed, "generator: player lands on the launch hallway");
    check(!p.dead_, "generator: player survives the opening run");
    check(toFloat(p.pos.x) > 60.0f, "generator: player actually ran forward");
}

// The in-house PNG decoder must read the original game art (palette + RGBA).
static void testPng() {
#ifdef OTACON_ASSET_DIR
    Image hud = loadPng(OTACON_ASSET_DIR "/images/hud.png");
    check(hud.valid() && hud.width == 134 && hud.height == 16, "png: hud.png 134x16 RGBA decoded");

    Image plr = loadPng(OTACON_ASSET_DIR "/images/player2.png");
    check(plr.valid() && plr.width == 570 && plr.height == 60, "png: player2.png 570x60 palette decoded");
    bool transparent = false, opaque = false;     // palette tRNS must yield both
    for (std::size_t i = 3; i < plr.rgba.size(); i += 4) {
        if (plr.rgba[i] == 0) transparent = true;
        if (plr.rgba[i] == 255) opaque = true;
    }
    check(transparent && opaque, "png: player2 palette+tRNS alpha decoded");
#endif
}

// The in-house CAF decoder must read the original sound effects (LPCM 22050/mono).
static void testCaf() {
#ifdef OTACON_ASSET_DIR
    AudioClip c = loadCaf(OTACON_ASSET_DIR "/sound/crumble.caf");
    check(c.valid() && c.sampleRate == 22050 && c.channels == 1, "caf: crumble.caf 22050/mono LPCM decoded");
    check(c.frames() > 1000, "caf: crumble.caf has audio samples");
    bool nonzero = false, inRange = true;     // real signal, properly normalised
    for (float s : c.samples) { if (s != 0.f) nonzero = true; if (s < -1.001f || s > 1.001f) inRange = false; }
    check(nonzero && inRange, "caf: samples are non-silent and within [-1,1]");
#endif
}

// The software mixer must lay a mono one-shot into both stereo channels, advance
// it, stop at the end, and free the voice.
static void testMixer() {
    AudioClip clip; clip.sampleRate = 22050; clip.channels = 1;
    clip.samples = {0.5f, -0.5f, 0.25f};
    Mixer mix; mix.setRate(22050);
    SoundId id = mix.add(clip);
    check(id != 0, "mixer: add returns a valid id");
    mix.play(id, 1.0f);

    float out[8] = {0};                 // 4 stereo frames
    mix.render(out, 4, 2);
    bool laid = out[0] == 0.5f && out[1] == 0.5f &&    // frame 0 -> L=R
                out[2] == -0.5f && out[3] == -0.5f &&  // frame 1
                out[4] == 0.25f && out[5] == 0.25f &&  // frame 2
                out[6] == 0.f && out[7] == 0.f;        // frame 3: clip exhausted
    check(laid, "mixer: mono one-shot fills both ears and stops at the end");

    float tail[4] = {1, 1, 1, 1};
    mix.render(tail, 2, 2);             // voice should be gone now -> silence
    check(tail[0] == 0.f && tail[1] == 0.f && tail[2] == 0.f && tail[3] == 0.f,
          "mixer: finished voice is removed");

    // Music: a looping voice wraps to the start at the end, and stops on demand.
    Mixer mus; mus.setRate(22050);
    SoundId mid = mus.add(clip);        // 3 samples
    mus.playMusic(mid, true, 1.0f);
    float mout[8] = {0};
    mus.render(mout, 4, 2);             // frame 3 should wrap back to sample 0
    check(mout[0] == 0.5f && mout[4] == 0.25f && mout[6] == 0.5f, "mixer: music loops at the end");
    mus.stopMusic();
    float msil[4] = {1, 1, 1, 1};
    mus.render(msil, 2, 2);
    check(msil[0] == 0.f && msil[1] == 0.f, "mixer: stopMusic silences the track");
}

// The in-house PNG encoder must round-trip through the decoder losslessly.
static void testPngRoundtrip() {
    Image src; src.width = 7; src.height = 5;
    src.rgba.resize(7 * 5 * 4);
    for (int i = 0; i < 7 * 5; ++i) {
        src.rgba[i*4+0] = std::uint8_t(i * 7);
        src.rgba[i*4+1] = std::uint8_t(255 - i * 3);
        src.rgba[i*4+2] = std::uint8_t(i * 11);
        src.rgba[i*4+3] = std::uint8_t(i % 2 ? 0 : 255);
    }
    const char* path = "/tmp/otacon_roundtrip.png";
    bool wrote = writePng(path, src);
    Image back = loadPng(path);
    check(wrote && back.valid() && back.width == 7 && back.height == 5, "png: encoder round-trips dimensions");
    check(wrote && back.rgba == src.rgba, "png: encoder round-trips pixels exactly");
}

// The particle emitter must burst, move particles, and recycle them off-screen.
static void testEmitter() {
    Emitter em;
    em.particleSize = {5, 5};
    em.minSpeed = {-50, -150}; em.maxSpeed = {150, -50}; em.gravity = 400;
    em.minRotation = -360; em.maxRotation = 360;
    em.init(20);
    em.start(true, 20);
    check(em.liveCount() == 20, "emitter: burst spawns N particles");

    Camera cam; cam.setViewport(480, 320); cam.scroll = {R(0), R(0)};
    for (int i = 0; i < 600; ++i) em.update(R(1.0f / 60.0f), cam);   // 10 s
    check(em.liveCount() == 0, "emitter: particles recycle once off-screen");
}


// ---------------------------------------------------------------------------
// A fixed seed must give the same stream every time, on every machine -- that is
// the whole contract the deterministic source exists to provide.
static void testRandom() {
    Random a(1234), b(1234);
    bool same = true;
    for (int i = 0; i < 256; ++i) if (a.next() != b.next()) { same = false; break; }
    check(same, "random: same seed gives the same stream");

    Random c(99);
    const std::uint32_t first = c.next();
    c.restart();
    check(c.next() == first, "random: restart rewinds to the seed");

    Random d(7);
    bool inRange = true;
    for (int i = 0; i < 4096; ++i) {
        const float u = d.unit();
        if (u < 0.f || u >= 1.f) { inRange = false; break; }
    }
    check(inRange, "random: unit() stays in [0,1)");

    // rangeI is half-open, so it must never return the upper bound.
    Random e(11);
    bool bounded = true;
    for (int i = 0; i < 4096; ++i) { const int v = e.rangeI(0, 4); if (v < 0 || v > 3) { bounded = false; break; } }
    check(bounded, "random: rangeI(0,4) yields 0..3");

    Random f(5);
    const Vec2f dir = f.onUnitCircle();
    check(std::fabs(std::sqrt(dir.x * dir.x + dir.y * dir.y) - 1.f) < 1e-4f,
          "random: onUnitCircle returns a unit vector");
}

// Noise must be a pure function of its inputs, and continuous -- two nearby
// samples must be nearby, or it is static rather than noise.
static void testNoise() {
    check(noise::value(3.7f, 2.1f, 9) == noise::value(3.7f, 2.1f, 9), "noise: same input gives same output");
    float worst = 0.f;
    for (int i = 0; i < 200; ++i) {
        const float x = float(i) * 0.01f;
        worst = std::max(worst, std::fabs(noise::value(x, 0.5f, 1) - noise::value(x + 0.01f, 0.5f, 1)));
    }
    check(worst < 0.25f, "noise: neighbouring samples are close (continuous)");
    bool bounded = true;
    for (int i = 0; i < 500 && bounded; ++i) {
        const float v = noise::fbm(float(i) * 0.13f, float(i) * 0.07f, 5);
        if (v < 0.f || v > 1.f) bounded = false;
    }
    check(bounded, "noise: fbm stays within 0..1");
}

static void testEase() {
    check(std::fabs(ease::smoothstep(0.f)) < 1e-6f && std::fabs(ease::smoothstep(1.f) - 1.f) < 1e-6f,
          "ease: smoothstep pins 0 and 1");
    check(std::fabs(ease::lerp(10.f, 20.f, 0.5f) - 15.f) < 1e-6f, "ease: lerp midpoint");
    check(std::fabs(ease::inverseLerp(10.f, 20.f, 15.f) - 0.5f) < 1e-6f, "ease: inverseLerp inverts lerp");
    // damp must be frame-rate independent: one big step == several small ones.
    float one = ease::damp(0.f, 100.f, 5.f, 0.5f);
    float many = 0.f;
    for (int i = 0; i < 50; ++i) many = ease::damp(many, 100.f, 5.f, 0.01f);
    check(std::fabs(one - many) < 0.5f, "ease: damp is frame-rate independent");
}

// A* must find the shortest route on an open grid, and report failure rather
// than a wrong answer when the goal is walled off.
static void testPathFinder() {
    TileMap map;
    map.resize(20, 12, 0);
    map.firstSolid = 1;

    PathFinder pf;
    pf.heuristic = PathFinder::Heuristic::Manhattan;
    check(pf.search(map, 0, 0, 19, 11), "pathfind: finds a route across open ground");
    // 4-way Manhattan distance is 19 + 11 = 30 steps, so 31 cells inclusive.
    check(pf.pathLength() == 31, "pathfind: the route is the shortest one");

    // Dijkstra must agree on length while exploring strictly more of the map.
    const int greedyVisited = pf.visited();
    pf.heuristic = PathFinder::Heuristic::None;
    pf.search(map, 0, 0, 19, 11);
    check(pf.pathLength() == 31, "pathfind: h=0 (Dijkstra) finds the same length");
    check(pf.visited() > greedyVisited, "pathfind: h=0 explores more than a heuristic");

    for (int y = 0; y < 12; ++y) map.set(10, y, 1);      // a full wall
    pf.heuristic = PathFinder::Heuristic::Manhattan;
    check(!pf.search(map, 0, 0, 19, 11), "pathfind: reports failure when walled off");
    check(pf.path().empty(), "pathfind: no path returned on failure");
}

// A pinned rope must hang from its anchor and settle, and more relaxation
// passes must leave it measurably less stretched.
static void testVerlet() {
    VerletBody body;
    body.makeRope(100.f, 20.f, 20, 8.f);
    body.iterations = 20;
    for (int i = 0; i < 240; ++i) body.step(1.f / 60.f);

    check(body.points[0].pinned && std::fabs(body.points[0].x - 100.f) < 0.001f,
          "verlet: the pinned point never moves");
    check(body.points.back().y > body.points.front().y, "verlet: the rope hangs downward");
    check(body.strain() < 0.10f, "verlet: 20 passes hold the links near rest length");

    VerletBody loose;
    loose.makeRope(100.f, 20.f, 20, 8.f);
    loose.iterations = 1;
    for (int i = 0; i < 240; ++i) loose.step(1.f / 60.f);
    check(loose.strain() > body.strain(), "verlet: fewer passes means more stretch");
}

// The broad phase must return everything a pair loop would, and nothing twice.
static void testSpatialGrid() {
    std::vector<Entity> bodies(40);
    std::vector<Entity*> all;
    for (std::size_t i = 0; i < bodies.size(); ++i) {
        bodies[i].pos = {R(float(i % 8) * 30.f), R(float(i / 8) * 30.f)};
        bodies[i].size = {R(16), R(16)};
        all.push_back(&bodies[i]);
    }
    SpatialGrid grid(32.f);
    grid.rebuild(all);

    std::vector<Entity*> near;
    grid.query(bodies[0], near);
    check(!near.empty(), "spatial: query finds neighbours");
    bool self = false, dup = false;
    for (std::size_t i = 0; i < near.size(); ++i) {
        if (near[i] == &bodies[0]) self = true;
        for (std::size_t j = i + 1; j < near.size(); ++j) if (near[i] == near[j]) dup = true;
    }
    check(!self, "spatial: query never returns the body itself");
    check(!dup, "spatial: query never returns a duplicate");
    check(grid.largestBucket() < bodies.size(), "spatial: the grid actually partitions");
}

// Flocking with only separation must push agents apart; the average pair
// distance is the measurable version of that.
static void testFlock() {
    Flock flock;
    flock.params.separationWeight = 2.f;
    flock.params.alignmentWeight = 0.f;
    flock.params.cohesionWeight = 0.f;
    Random rng(3);
    for (int i = 0; i < 24; ++i) flock.add(rng.range(100.f, 140.f), rng.range(100.f, 140.f), 0.f, 0.f);

    auto spread = [&]() {
        float sum = 0.f; int n = 0;
        for (std::size_t i = 0; i < flock.boids.size(); ++i)
            for (std::size_t j = i + 1; j < flock.boids.size(); ++j) {
                const float dx = flock.boids[j].x - flock.boids[i].x;
                const float dy = flock.boids[j].y - flock.boids[i].y;
                sum += std::sqrt(dx * dx + dy * dy); ++n;
            }
        return n ? sum / float(n) : 0.f;
    };
    const float before = spread();
    for (int i = 0; i < 60; ++i) flock.step(1.f / 60.f);
    check(spread() > before, "flock: separation alone pushes agents apart");
}

static void testAnimator() {
    static const int frames[] = {0, 1, 2, 3};
    const Clip run{"run", frames, 4, 10.f, true};
    Animator a;
    a.play(&run);
    check(a.frame() == 0, "animator: starts on the first frame");
    for (int i = 0; i < 6; ++i) a.update(R(1.f / 60.f));   // 0.1s = one hold at 10fps
    check(a.frame() == 1, "animator: advances on the clock");
    a.step(3);
    check(a.frame() == 0, "animator: a looping clip wraps");

    const Clip once{"once", frames, 4, 60.f, false};
    Animator b;
    b.play(&once);
    for (int i = 0; i < 60; ++i) b.update(R(1.f / 60.f));
    // Split rather than combined, so a failure says which half broke.
    check(b.finished(), "animator: a one-shot reports finished");
    check(b.frame() == 3, "animator: a one-shot stops on its last frame");
}


// A ray must report not just THAT it hit but where and on which face -- the
// normal is what a slide or a bounce needs, and a bare distance cannot give it.
static void testRaycast() {
    const Rect box(R(10), R(10), R(10), R(10));      // x 10..20, y 10..20

    RayHit h = ray::vsRect({0, 15}, {40, 0}, box);   // straight at its left face
    check(h.hit, "ray: hits a box in its path");
    check(std::fabs(h.point.x - 10.f) < 0.01f, "ray: reports the entry point");
    check(h.normal.x < -0.5f, "ray: reports the face that was struck");

    check(!ray::vsRect({0, 100}, {40, 0}, box).hit, "ray: misses a box it passes by");
    // Length matters: the same direction, too short to arrive.
    check(!ray::vsRect({0, 15}, {5, 0}, box).hit, "ray: a short ray stops before the box");
    // Axis-parallel rays make 1/dir infinite; that must fall out, not misbehave.
    check(ray::vsRect({15, 0}, {0, 40}, box).hit, "ray: an axis-parallel ray still works");
    check(ray::vsRect({15, 15}, {40, 0}, box).hit, "ray: starting inside counts as a hit");

    // Nearest-hit selection, not first-in-array.
    Entity near, far;
    near.pos = {R(30), R(10)}; near.size = {R(10), R(10)}; near.solid = true;
    far.pos  = {R(60), R(10)}; far.size  = {R(10), R(10)}; far.solid = true;
    std::vector<Entity*> group = {&far, &near};      // deliberately far-first
    RayHit g = ray::vsGroup({0, 15}, {100, 0}, group);
    check(g.hit && g.entity == &near, "ray: vsGroup returns the nearest, not the first");

    // DDA across a tilemap, and the line-of-sight question it stands in for.
    TileMap map;
    map.resize(16, 8, 0);
    map.firstSolid = 1;
    for (int y = 0; y < 8; ++y) map.set(6, y, 1);    // a wall at column 6
    RayHit t = ray::vsTileMap({8.f, 40.f}, {200.f, 0.f}, map, 16.f);
    check(t.hit && t.tileX == 6, "ray: DDA finds the first solid tile");
    check(!ray::lineOfSight({8.f, 40.f}, {200.f, 40.f}, map, 16.f), "ray: the wall blocks line of sight");
    check(ray::lineOfSight({8.f, 40.f}, {80.f, 40.f}, map, 16.f), "ray: clear ground does not");
}

int main() {
    // Unbuffered: if a check crashes the process, a fully-buffered stdout would
    // discard every line printed up to that point and the log would be empty --
    // which is exactly when you most need to know how far it got.
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    std::printf("[test] scalar build = %s\n", kScalarName);
    testPngRoundtrip();
    testEmitter();
    testRandom();
    testNoise();
    testEase();
    testPathFinder();
    testVerlet();
    testSpatialGrid();
    testFlock();
    testAnimator();
    testRaycast();
    testGravityLanding();
    testPlayerJump();
    testComputeVelocity();
    testFixed();
    testGeneratorStart();
    testWindowSmash();
    testPng();
    testCaf();
    testMixer();
    std::printf("[test] %s (%d failure%s)\n", failures ? "FAILURES" : "ALL PASS",
                failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
