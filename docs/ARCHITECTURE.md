# Otacon — architecture

How the engine is put together, and why it is put together that way. For the
API reference see `docs/otacon-engine.pdf` (or `docs/html/index.html`); this
document is about structure and the reasoning behind it.

About 6,700 lines under `src/engine/`.

---

## 1. The shape

Three trees, and one rule that governs all of them.

```
src/engine/     the Otacon engine        — knows about no game
src/game/       canabalt, dino, flappy   — know about no graphics API
src/samples/    fifteen sample screens   — link the engine, nothing else
```

> **The golden rule:** engine code never mentions a game, and game code never
> mentions GLFW, OpenGL, Vulkan or CoreAudio.

Everything that crosses that boundary goes through one of four interfaces, and
there are deliberately only four:

| Seam | Header | Implementations |
|------|--------|-----------------|
| `IGame` | `IGame.hpp` | canabalt, dino, flappy, the sample gallery |
| `IWindow` | `platform/Window.hpp` | GLFW 3.4, SDL2 |
| `IRenderer` | `render/IRenderer.hpp` | GL modern, GL legacy, Vulkan |
| `IAudio` | `audio/IAudio.hpp` | CoreAudio, null stub |

Each seam has **at least two implementations**. That is not an accident — an
interface with one implementation is a guess at an abstraction, not an
abstraction. The second SDL2 window backend and the fixed-function GL backend
exist largely to keep the other two honest.

```
                    ┌─────────────────────────────┐
                    │  canabalt · dino · flappy   │
                    │  samples                    │
                    └──────────────┬──────────────┘
                                   │  IGame
                    ┌──────────────┴──────────────┐
                    │           App               │   the only place that knows
                    │  window · time · debug      │   what order things happen in
                    └───┬────────┬────────┬───────┘
              IWindow   │        │        │  IAudio
                        │   IRenderer     │
              ┌─────────┴──┐  ┌──┴─────┐  ┌┴──────────┐
              │ GLFW  SDL2 │  │ GL mod │  │ CoreAudio │
              │            │  │ GL leg │  │ null      │
              │            │  │ Vulkan │  │           │
              └────────────┘  └────────┘  └───────────┘
```

---

## 2. The renderer contract

This is the most opinionated part of the engine, and the one that buys the most.

**A backend implements only two drawing entry points:**

```cpp
virtual void submitTriangles(const Vertex* verts, std::size_t count) = 0;
virtual void submitTextured(const TexVertex* verts, std::size_t count, TextureHandle tex) = 0;
```

Everything else — `fillRect`, `fillRotatedRect`, `drawRectOutline`, `drawLine`,
`drawImage`, `drawImageRotated`, `drawText` — is tessellated into triangles
**once**, in `IRenderer.cpp`, in the base class. A backend never sees a
rectangle. It cannot, therefore, disagree about what a rectangle is.

```
 game calls  r.drawRectOutline(...)
                    │
        IRenderer.cpp tessellates          ← one implementation, shared
                    │
             submitTriangles()             ← the only thing a backend writes
                    │
      ┌─────────────┼─────────────┐
   GL modern     GL legacy      Vulkan
   VBO+shader    glBegin        pipeline
```

Consequences worth naming:

- **Cross-backend identity is provable, not asserted.** `tools/backend-diff.sh`
  renders the same deterministic frame in each backend and compares PNGs by
  SHA-256. Because geometry is shared, any difference is a rasteriser
  difference, which is a much smaller and more interesting claim to check.
- **Lines have portable thickness.** `drawLine` is a rotated quad, not
  `GL_LINES` — line-width support is one of the least portable corners of every
  graphics API.
- **Outlines are four filled rects,** not four lines, so corners are square and
  thickness is exact at any size.
- **Text is a 3×5 bitmap font** defined as five rows of three bits per glyph,
  emitted as one quad per lit pixel. No glyph atlas, no font library.

### Logical space and letterboxing

Everything is authored in a **logical** coordinate space — origin top-left, y
downward, matching flixel — and each backend applies its own projection. Games
pick their own logical size (`canabalt` 480×320, `dino` 600×150, `flappy`
288×512, `samples` 640×400).

`IRenderer::letterbox()` computes the largest centred sub-rect of the
framebuffer matching the logical aspect. Backends clear the whole window and
then restrict the viewport to it, so a window of any shape shows the content
undistorted with bars rather than stretching it.

### The capability escape hatch

Two sample screens need per-pixel control, which the triangle-only contract
deliberately does not offer. Rather than weaken the contract, the seam grew two
things:

```cpp
// Always available: re-upload an existing texture's pixels.
virtual void updateTexture(TextureHandle t, int w, int h, const std::uint8_t* rgba) = 0;

// Optional: a user fragment shader. Ask before using.
virtual bool         supportsShaders() const { return false; }
virtual ShaderHandle createEffect(const char* fragmentSrc, char* log, std::size_t logSize);
virtual void         useEffect(ShaderHandle e);
virtual void         setEffectUniform(const char* name, float x, float y, float z, float w);
```

The design points here are worth stating explicitly:

- **`supportsShaders()` is a question, not an assumption.** GL legacy is
  fixed-function and has no programmable stage at all; the Vulkan backend ships
  pre-compiled SPIR-V with no runtime compiler. Both answer `false`, honestly.
- **The engine keeps the vertex stage.** An effect replaces the *fragment*
  shader only, so a user shader cannot move geometry and invalidate the
  cross-backend contract for everything else on screen.
- **Anything built on an effect must carry a CPU path.** Both samples that use
  one do, via `updateTexture`. That is what stops a capability from silently
  becoming a backend requirement.

### Known limits

`IRenderer` **does not batch** — each primitive is submitted on its own, so
draw calls track item count roughly 1:1. The Stress Test sample measures exactly
this, and batching is the obvious next optimisation.

---

## 3. Compile-time configuration

Backend and scalar choices are made **at build time**, not runtime. There is no
dispatch table and no way to reach a backend the build did not include.

```
config/build.cfg  ──►  cmake/ReadBuildCfg.cmake  ──►  OTACON_* defines
   GRAPHICS_BACKEND         parses KEY = VALUE           OTACON_BACKEND_GL_MODERN
   WINDOW_BACKEND           re-runs on change            OTACON_WINDOW_GLFW
   SCALAR_TYPE                                           OTACON_SCALAR_FLOAT
   MEMORY_TRACKING                                       OTACON_MEMORY_TRACKING
   DEBUG_RUNTIME                                         OTACON_DEBUG_RUNTIME
```

Only the selected backend's sources are added to the target, so an unused
backend is not merely inactive — it is not compiled, and its SDK is not a
dependency. `RendererFactory.cpp` and `PlatformFactory.cpp` are each a short
`#if` that resolves the one factory function that exists.

### The pluggable scalar

`Real` is either `float` or a Q16.16 fixed-point type, chosen by `SCALAR_TYPE`:

```cpp
#if defined(OTACON_SCALAR_FIXED)
using Real = Fixed;                 // 32-bit, 16 integer bits, 16 fractional
#else
using Real = float;                 // matches the original game exactly
#endif
```

All simulation code is written against `Real` and the `s*` helpers (`sabs`,
`ssqrt`, `sfloor`), so it compiles unchanged either way. The fixed-point build
is a teaching option rather than the default, and the reason is instructive:
Q16.16 tops out at ±32767.99998, and an endless runner's world X grows without
bound. The limitation is the lesson.

---

## 4. The frame

`App::run()` is the only place that knows the order of a frame.

```
  ┌─ App::run ──────────────────────────────────────────────┐
  │                                                          │
  │  steps = time.beginFrame()      how many sim steps?      │
  │  window.pollEvents()                                     │
  │  in = window.input()                                     │
  │  App::handleGlobalInput(in)     engine-owned keys first  │
  │  game.handleInput(in)           then the game's          │
  │                                                          │
  │  for (i < steps)  game.update(dt)                        │
  │                                                          │
  │  renderer.beginFrame(game.clearColor())                  │
  │  game.render(renderer, debug)                            │
  │  App::drawHud()  /  drawPerf()                           │
  │  renderer.endFrame()            screenshot written here  │
  │  window.swapBuffers()                                    │
  └──────────────────────────────────────────────────────────┘
```

A game never polls an event queue, never swaps a buffer, and never decides how
many simulation steps a frame is worth.

### Time

`TimeManager` offers two policies, switchable at runtime with `F5`:

- **Variable** — `elapsed = now - last`, clamped to `maxElapsed` (1/20 s). What
  the original flixel game does, so it is the default and the feel is unchanged.
  The clamp is what stops a breakpoint or a dragged window from teleporting
  everything through a wall on resume.
- **Fixed** — accumulate real time, emit N whole steps of a fixed dt.
  Reproducible regardless of frame rate.

Time scaling (`+`/`-`) scales the *input* elapsed time rather than the dt handed
out, so slow motion preserves fixed-timestep determinism.

### Input

Backends map physical keys onto **logical `Action`s** (`Jump`, `NextMode`,
`TogglePause`, `Aux1`…`Aux8`, …). The key tables in `GlfwWindow.cpp` and
`Sdl2Window.cpp` are the only places in the project that name a key code.
`InputFrame` carries `held` (level) and `pressed` (rising edge), plus a
normalised pointer position and a `selectSlot` for the number row.

`App` consumes the debug and presentation actions before the game sees the
frame, so no game can shadow them and no game has to implement them.

---

## 5. The scene model

Two collaborating concepts, and the split matters:

- **`Entity`** — simulation data. An AABB with velocity, acceleration, drag and
  max velocity, integrated by a flixel-style midpoint scheme. This is what
  collides. The engine's `FlxObject`.
- **`Node`** — a renderable, updatable element of the scene tree. Longer-lived
  scene pieces: particle emitters, sprite drawers, facade renderers.

`Scene` owns a camera, an entity draw list (rebuilt each frame by the game) and
two node lists, and renders them in one fixed order:

```
  backdrop nodes      farthest parallax, behind everything
  entity layer        solid rect, or textured sprite if entity.texture is set
  foreground nodes    emitters, sprite drawers
  debug overlays      grid · parallax bands · colliders · velocities · focus
```

Nothing draws on the side. That is what lets a debug view be written once and
work for every game and every backend.

### Motion and collision

`Entity::updateMotion` is a midpoint integrator: apply half the velocity change,
move by the resulting averaged velocity, apply the other half. It also **grows
the swept collision hulls** by the distance travelled —

```
        hull the solver tests
   ┌───────────────────────────┐
   │  ┌─────┐         ┌─────┐  │
   │  │ was │  ────►  │ now │  │      a body that moved far enough to pass
   │  └─────┘         └─────┘  │      through a wall still overlaps it here
   └───────────────────────────┘
```

— which is the whole anti-tunnelling story. The solver never has to know how a
body moved.

`Collision.cpp` then separates **one axis at a time**: X to completion, then Y.
Deliberately narrow — AABB only, no restitution, no rotation. That narrowness is
why a platformer built on it feels tight: a body that lands stops dead instead of
chattering, and the two axes cannot fight each other into a jitter.

### Camera

One multiply is the entire parallax system:

```cpp
screen = floor(world + eps) + floor(scroll * scrollFactor)
```

`scrollFactor` 0 pins to the screen (that is all a HUD is), 1 is locked to the
world, above 1 rushes past in front. The quake offset is added last, so a shake
moves the whole screen uniformly rather than each layer differently — and is
deliberately omitted from the inverse `worldFromScreen`, so a cursor hits what
it is over rather than what the shake moved under it.

---

## 6. Core subsystems

All in-house.

### Math

| | |
|---|---|
| `Scalar` / `Fixed` | the pluggable `Real`: 32-bit float, or Q16.16 |
| `Vector` / `Rect` | 2/3/4-D vectors, AABB |
| `Random` | deterministic LCG or entropy-seeded MT, switchable |
| `Noise` | value noise, fbm, ridged |
| `Ease` | lerp, smoothstep, the easing curves, frame-rate independent `damp` |

`Random` deserves a note. Before it existed, **fifteen** places in this project
had written their own linear congruential generator — including the camera's
quake and the particle emitter. A generator is a *value* here rather than a
global: hold your own, and two systems can neither perturb each other's stream
nor be made irreproducible by the order they happen to run in. That is exactly
why the camera's shake keeps a private one — a cosmetic effect must never shift
the stream a level generator is reading.

### Memory

Three allocators, each a classic pattern, chosen explicitly at the call site:

| | Strategy | Free | Use for |
|---|---|---|---|
| **Tracked heap** | malloc + a header recording size, tag and call site | individual | general, and anything you want named in a leak report |
| **Arena** | bump a pointer | all at once | per-frame scratch — O(1), cannot fragment |
| **Pool** | fixed-size free list threaded through the free blocks | individual, O(1) | many short-lived same-type objects |

Global `new`/`delete` are **deliberately not overridden**. Third-party code
allocates through its own paths regardless, and keeping ours explicit means you
can see at the call site which strategy a piece of code chose. `mem::report()`
runs at shutdown and names anything still live.

Tags: `General`, `Simulation`, `Render`, `Geometry`, `Particles`, `Debug`.

### Debug runtime

A bitfield of independently toggleable views (`Colliders`, `CameraFocus`,
`ParallaxBands`, `Grid`, `Wireframe`, `Hud`, `Velocities`, `Perf`) plus curated
presets, pause and single-step. Kept as plain state rather than baked into the
renderer, which is why every backend gets the same visualisation for free.

### Simulation and AI

| | |
|---|---|
| `Entity` + `Collision` | AABB bodies, axis-separated, swept hulls |
| `Raycast` | ray vs AABB (slab), ray vs tilemap (DDA), line of sight |
| `SpatialGrid` | uniform-grid broad phase, so collision need not be O(n²) |
| `Verlet` | position-based dynamics: ropes, bridges, cloth |
| `PathFinder` | A* over a `TileMap`, with the heuristic as the knob |
| `Steering` | flocking, plus seek/flee/arrive |
| `Emitter` | particles, as ordinary entities |
| `TileMap` | a grid of indices, drawn with view culling |
| `Animator` | sprite-sheet clips and the clock that drives them |

`Collision` answers "these two overlap, push them apart". `Raycast` answers the
other question a game asks constantly — "what is the first thing along this line,
and where exactly did I hit it?" — which is behind enemy line of sight, hitscan
weapons, ground probes, mouse picking and an AI's "is that jump survivable" test.
It returns the surface normal, not just a distance, because that is what a slide
or a bounce needs.

Two of these carry a lesson beyond their API. `SpatialGrid` exists because
`collideWithGroup` tests every pair: fine at thirty bodies, quietly ruinous at
three hundred. And `Animator`'s clock derives its frame count by **division**
rather than the obvious `while (timer >= hold) timer -= hold;` — repeated
subtraction accumulates a rounding error per iteration, and a compiler is free
to constant-fold a loop of that shape at higher precision than the runtime path,
at which point the same code advances a different number of frames depending on
whether it was folded. One division is one rounding.

### Assets

Written from scratch, and the reason is didactic rather than ideological — each
decoder is a small worked example of a real format:

- **PNG decode** (`Png.cpp` + `Inflate.cpp`) — a DEFLATE decoder (LZ77 +
  Huffman, RFC 1951) under a chunk parser, with the per-scanline filters undone
  including the Paeth predictor.
- **PNG encode** (`PngWrite.cpp`) — stored DEFLATE blocks only, no compressor.
  Large files, short code; a screenshot is written once and read once.
- **CAF decode** (`Caf.cpp`) — a big-endian chunked container, LPCM in any width
  or endianness, normalised to float on load.

### Audio

`IAudio` is the seam; the shared software `Mixer` sums a pool of one-shot voices
plus one music voice into interleaved stereo float. Clips are resampled on
upload so the render callback never has to.

Two threads meet in the mixer: `play()` on the game thread, `render()` on the
platform's real-time callback, guarded by a short mutex. A production engine
would use a lock-free queue — blocking the audio thread is how you get a click —
and the honest simple version is the deliberate choice at this scale.

The factory **never returns null**: with no working device you get a stub that
swallows everything, so "sound is missing" can never become "it crashes there".

---

## 7. Extension points

**Add a game** — implement `IGame`, add a CMake target linking `otacon`. Nothing
in `src/engine/` changes. The three existing games differ in almost every way a
game can (logical resolution, camera model, level design, ported vs original)
and share the library unmodified.

**Add a sample** — one `.cpp` exposing a factory, plus one row in
`src/samples/Registry.cpp`. CMake globs the directory; the gallery reads the
table.

**Add a graphics backend** — implement `submitTriangles`, `submitTextured`,
`createTexture`, `updateTexture`, the frame hooks and `readPixels`. Leave
`supportsShaders()` false unless there is a real programmable stage. Add a
branch to `RendererFactory.cpp` and the CMake backend block.

**Add a window backend** — implement `IWindow`, map physical keys to `Action`s,
add a branch to `PlatformFactory.cpp`.

---

## 8. Deliberate non-goals

Stated plainly, because each is a choice rather than an omission:

- **No draw batching.** Each primitive is its own submission.
- **No rotation or restitution in collision.** AABB, axis-separated. It is what
  makes the platformer feel right; `Verlet` covers the soft-body cases instead.
- **No runtime shader compilation on Vulkan.** SPIR-V is pre-built into headers;
  `supportsShaders()` says so.
- **No scene graph transforms.** Positions are world-space; the camera projects.
  There is no matrix stack.
- **No asset hot-reload, no editor, no scripting.**
- **Fixed-point is opt-in,** and cannot represent an endless runner's world X.

---

## 9. Verification

| | |
|---|---|
| `tools/backend-diff.sh` | renders one deterministic frame per graphics backend, compares by SHA-256 |
| `otacon_smoke` | headless: integrator, collision, emitter, fixed-point, level generator, PNG/CAF decode, mixer |
| `--capture <png> --frames N` | any executable: N deterministic frames, dev UI hidden, then quit |
| `--record <dir>` | `samples` and `flappy`: every frame as a numbered PNG |

The simulation is fully decoupled from rendering, which is what makes the
headless tests possible at all — `otacon_smoke` steps physics, collision and the
level generator with no window and no renderer.
