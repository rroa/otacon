# Otacon — a didactic 2D game engine, three games, and fifteen samples

A teaching project in C++17: a small, reusable, **in-house** 2D engine
(**Otacon**), three games built on top of it, and fifteen samples that
demonstrate the engine on its own. Almost everything is written
from scratch — math (including a Q16.16 fixed-point scalar), n-D vectors, a
memory manager, time management, a debug runtime, the OpenGL function loader,
the PNG decoder *and* encoder, the CAF audio decoder, and the software mixer.
The only third-party dependency is the window library (GLFW or SDL2), fetched
automatically by CMake.

```
cmake -S . -B build          # first run fetches the window library automatically
cmake --build build -j8      # builds the engine, all three games and the samples

./build/bin/samples          # the engine samples — F1 for the list
./build/bin/canabalt
./build/bin/dino
./build/bin/flappy
```

On the **Vulkan** backend on macOS, launch through `tools/run.sh` instead so the
loader and MoltenVK ICD are found — `./tools/run.sh samples`, `./tools/run.sh
canabalt`, and so on. It is harmless on the OpenGL backends.

Per-platform setup — including the Linux packages GLFW needs and the Visual
Studio path on Windows — is in **[INSTALL.md](INSTALL.md)**.

## The games

| Exe | What | Logical res |
|-----|------|-------------|
| `canabalt` | Port of the open-source iOS/flixel **Canabalt** — exact player physics and the ported `Sequence.m` level generator. Four incremental modes: static box → gravity → run & jump → infinite rooftops. | 480×320 |
| `dino` | Port of Chromium's offline **T-Rex runner**. Four modes: geometry → mechanics → gameplay → pixel art. | 600×150 |
| `flappy` | An original **Flappy Bird**, grown one concept at a time. Ten `Scene` builds (kinematics, gravity, textures, pipes, animation, world, collision, scoring, HUD) that all stay alive and pageable at runtime. | 288×512 |

Each game is one `IGame`; none of them mentions GLFW, OpenGL, Vulkan or
CoreAudio. Adding a fourth game means adding one more `IGame` and one CMake
target — the engine does not change.

## What the engine is

**Otacon** (`src/engine/`, static lib `otacon`) is game-agnostic. Everything
crosses the boundary through four seams:

| Seam | Header | Implementations |
|------|--------|-----------------|
| `IGame` | `src/engine/IGame.hpp` | the three games above |
| `IWindow` | `src/engine/platform/Window.hpp` | GLFW 3.4, SDL2 |
| `IRenderer` | `src/engine/render/IRenderer.hpp` | OpenGL modern, OpenGL legacy, Vulkan |
| `IAudio` | `src/engine/audio/IAudio.hpp` | CoreAudio (macOS), silent stub elsewhere |

The renderer seam is what makes "identical across backends" *provable*: a
backend implements only triangle rasterization (`submitTriangles` /
`submitTextured`) plus frame lifecycle. Every higher-level primitive — filled
rect, rotated quad, line, outline, bitmap text — is tessellated once in the base
class, so all three backends emit byte-identical geometry.

On top of the seams: a flixel-faithful simulation (midpoint integrator,
swept-AABB collision separation, follow camera with parallax and screen quake),
a `Scene`/`Node` tree, and a particle `Emitter`.

## Engine samples

Fifteen screens demonstrating the engine itself, in one executable — paged the
same way Canabalt pages modes and Flappy pages builds. They link `otacon` and
nothing else: a sample that needed a game's code would mean a seam had leaked.
Every texture they use is generated in code, so there are no sample assets.

```
cmake --build build --target samples -j8   # or just `cmake --build build -j8`

./build/bin/samples                 # F1 for the list, [ / ] to page
./build/bin/samples --sample 5      # open one directly, 1-based
./tools/run.sh samples              # same, but sets the Vulkan loader vars first

./build/bin/samples --sample 8 --capture shot.png --frames 120
./build/bin/samples --sample 6 --record frames/ --frames 240
```

`--capture` renders N deterministic frames with the dev UI hidden, saves a PNG
and quits; `--record` writes every frame to a numbered PNG for assembling a
video. Both are how the screenshots and the cross-backend comparison are made.

| # | Sample | What it shows |
|---|--------|---------------|
| 1 | Hello Sprite | four ways to draw, one triangle path underneath |
| 2 | Sprite Animation | a frame is a clock problem and a UV problem |
| 3 | Tilemap World | a world of indices, drawn only where you can see |
| 4 | Parallax Camera | depth is one multiply: `scroll * scrollFactor` |
| 5 | 2D Lighting + Normals | the same N·L on the CPU and on the GPU |
| 6 | Particles | an emitter is a pool of ordinary entities |
| 7 | Physics Playground | AABB separation, one axis at a time, no bounce |
| 8 | Pendulum | the integrator you pick is visible in the energy |
| 9 | Chain & Rope | verlet points, and relaxation as the stiffness knob |
| 10 | Platformer Controller | game feel is five timers you can switch off |
| 11 | Top-Down Movement | normalise the stick, and let the camera lag |
| 12 | Pathfinding / Boids | one global search, one set of local rules |
| 13 | Shader Playground | the same effect either side of the renderer seam |
| 14 | Procedural Dungeon | generation you can single-step, with a seed |
| 15 | Stress Test | find the wall, and learn which wall it is |

Adding a sample is one `.cpp` plus one row in `src/samples/Registry.cpp` — CMake
globs the directory and the gallery reads the table.

### Two paths for per-pixel work

Samples 5 and 13 need per-pixel control, which sits awkwardly against the
engine's central promise that every backend draws the same thing — only GL
modern has a programmable stage. So both exist:

* **CPU path** — the sample shades a `Canvas` in plain C++ and uploads it with
  `IRenderer::updateTexture`. Runs on all three backends, GL legacy included.
* **GPU path** — `IRenderer::createEffect` compiles a fragment shader. GL modern
  reports `supportsShaders() == true` and runs it; GL legacy (fixed-function)
  and Vulkan (pre-built SPIR-V, no runtime compiler) report false.

Every shader-using sample keeps the CPU path as its reference, so nothing is
backend-exclusive and the cross-backend contract still means something. Press
`E` in either sample to switch; the panel always says which path is live and why.

## Configuring the build

Edit `config/build.cfg` and re-run CMake (it reconfigures automatically when the
file changes). Any key can also be overridden per-configure, e.g.
`-DGRAPHICS_BACKEND=VULKAN`.

| Key | Values | Meaning |
|-----|--------|---------|
| `GRAPHICS_BACKEND` | `GL_MODERN` (default) / `GL_LEGACY` / `VULKAN` | Renderer pipeline |
| `WINDOW_BACKEND` | `GLFW` (default) / `SDL2` | Window / input / context library |
| `SCALAR_TYPE` | `FLOAT` (default) / `FIXED` | Simulation number type (Q16.16) |
| `MEMORY_TRACKING` | `ON` / `OFF` | Allocation tracking + leak report |
| `DEBUG_RUNTIME` | `ON` / `OFF` | Runtime debug views |

## Controls

**Engine-wide** (every game): `` ` `` = hide ALL dev UI for clean play ·
`H` = perf+memory overlay · `V`/`Tab` = cycle debug view preset ·
`C`/`X`/`Z`/`G` = colliders/parallax/wireframe/grid · `P`/`O` = pause/step ·
`+`/`-` = time scale (slow-mo) · **`F5`** = timestep variable↔fixed ·
`F12` = screenshot (in-house PNG encoder) · `M` = software cursor ·
`F9` = mute · `R` = reset · `Esc` = quit.

**Canabalt:** `Space`/click = jump (hold = higher) · **`1`–`4`** = jump straight
to a mode (or `]`/`[` to cycle) · `F6` = cycle the geometry→sprite reveal ·
`F8` = all geometry ↔ all sprites · `F7` = particles on/off · `E` = burst sparks
at the mouse (mode 1) · `F10` = next music track · right-mouse drag = grab/move
a box. Teaching toggles: `F1` = RNG deterministic↔random · `F2` = startup quake
on/off · `F3`/`F4` = quake weaker/stronger.

**Dino:** `Space`/click = jump · `E` = speed drop · `1`–`4` or `]`/`[` = mode ·
`F6`/`F8` = art reveal · `F1` = deterministic↔random obstacles ·
`F2` = collision-part boxes.

**Flappy:** `Space`/`Up`/`W` = flap · `]`/`[` = next/prev build ·
`F1`/`F2` = gravity −/+ (build 1.b).

**Samples:** `F1` = the sample list · `F2` = per-sample help · `]`/`[` = page ·
`R` = reset. Every other key belongs to the open sample; `F2` lists them.

## Verification

`tools/backend-diff.sh` renders the same deterministic frame in each graphics
backend and compares the PNGs — the rigorous proof of the cross-backend
contract. The headless `otacon_smoke` target additionally unit-tests the
integrator, collision, the emitter, the fixed-point math, the level generator,
the PNG/CAF decoders and the audio mixer:

```
cmake --build build --target otacon_smoke -j8
./build/bin/otacon_smoke
```

Each game and the samples accept `--capture <png> [--frames N]`: render N
deterministic frames with the dev UI hidden, save a PNG, quit. `samples` also
takes `--sample N` to pick the screen, and both `samples` and `flappy` accept
`--record <dir>` to write every frame as a numbered PNG. `flappy` additionally
has `--demo`, which self-plays the newest build.

## Documentation

* **[INSTALL.md](INSTALL.md)** — installing and running on macOS, Linux and
  Windows: prerequisites, per-distro packages, backend selection, and
  troubleshooting.
* **[docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)** — how the engine is put
  together and why: the four seams, the renderer contract, the frame, the scene
  model, extension points, and the deliberate non-goals. Start here; it reads in
  the browser without building anything.

The engine and Canabalt also have full manuals (PDF + HTML). Rebuild the PDFs
with `tools/build-docs.sh` (needs `tectonic`).

* **Engine (Otacon):** `docs/otacon-engine.pdf` / `docs/html/index.html` (source `docs/otacon-engine.tex`).
* **Game (Canabalt):** `src/game/canabalt/docs/canabalt.pdf` / `…/docs/html/index.html` (source `…/docs/canabalt.tex`).

## Layout

| Path | What |
|------|------|
| `src/engine/` | The Otacon engine — no game knowledge |
| `src/game/canabalt/` | Canabalt, with its own `assets/` and `docs/` |
| `src/game/dino/` | The Chromium T-Rex runner, with its own `assets/` |
| `src/game/flappy/` | Flappy Bird, one `Scene` per build, with its own `assets/` |
| `src/samples/` | Fifteen engine samples (exe `samples`) |
| `INSTALL.md` | Per-platform install and run instructions |
| `docs/` | Architecture notes + the engine manual (source, PDF, HTML) |
| `tests/smoke.cpp` | Headless engine self-tests (`otacon_smoke`) |
| `config/build.cfg` | Backend / scalar selection |
| `cmake/`, `tools/` | Config parser; run, docs and backend-diff scripts |
| `reference/` | Original Canabalt and Chromium sources (studied, not built) |

`reference/` is not tracked — it holds the upstream sources the ports were read
from, and is fetched separately.

## Assets & licensing

### Canabalt

This is a port written to study the original. The source release explicitly
permits that use; it does not permit redistribution, which is why **this
repository is private**.

* **Upstream:** [github.com/ericjohnson/canabalt-ios](https://github.com/ericjohnson/canabalt-ios)
  — the Canabalt source code release of December 29, 2010.
* *Canabalt* is a registered trademark of **Semi Secret Software, LLC**, and is
  copyright © 2009–2010 Semi Secret Software, LLC.
* The music — *"RUN!"*, *"Daring Escape"* and *"Mach Runner"* — is copyright
  © 2009–2010 **Danny Baranowsky**.
* The `flixel-ios` engine in that release is MIT-licensed. **Everything else —
  all other source and all game data — remains Semi Secret's**, under terms that
  say you *"cannot redistribute our source code"* or data from the original game,
  but *"can use our source code for personal entertainment or education
  purposes."* The full text is `GAME_LICENSE.TXT` in the upstream release.

What that means here, concretely:

| | |
|---|---|
| `src/engine/`, `src/samples/`, `src/game/canabalt/*.cpp` | Written for this project. A port and an original engine — ours. |
| `src/game/canabalt/assets/` | The **original game's** art, audio and data. Semi Secret's and Danny Baranowsky's, included so the port runs. Not ours to redistribute. |
| `reference/` | The upstream release itself, read while porting. Untracked — see `.gitignore` — and never built. |

Because the assets are tracked, **this repository cannot simply be flipped to
public**: git keeps history, so they would have to be removed from every commit
first. Publishing the code alone is fine — the engine and all fifteen samples
are self-contained, since the samples generate every texture they use in code.

### The other two games

* **Dino** — `assets/images/sprite.png` is the offline-runner sprite sheet from
  the Chromium source tree, which is BSD-3-Clause.
* **Flappy Bird** — the sprites and sounds come from
  [github.com/samuelcust/flappy-bird-assets](https://github.com/samuelcust/flappy-bird-assets)
  (see `assets/sound/convert.sh`). *Flappy Bird* is © Dong Nguyen / .GEARS;
  treat this art as third-party too. The game code itself is original.

None of the above is legal advice — it is a record of where each file came from
and what its stated terms are, so the question can be answered without digging.

## Status

Everything above runs. All three graphics backends (GL modern, GL legacy,
Vulkan) and both window backends (GLFW, SDL2) are implemented and
interchangeable, textured sprites and audio are in, the `FIXED` (Q16.16) build
compiles and runs, the fifteen samples run on every backend, and the headless
test suite passes.
