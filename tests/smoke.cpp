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

int main() {
    std::printf("[test] scalar build = %s\n", kScalarName);
    testPngRoundtrip();
    testEmitter();
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
