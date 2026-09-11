# Otacon — a didactic 2D game engine, and three games on it

A teaching project in C++17: a small, reusable, **in-house** 2D engine
(**Otacon**) with three games built on top of it. Almost everything is written
from scratch — math (including a Q16.16 fixed-point scalar), n-D vectors, a
memory manager, time management, a debug runtime, the OpenGL function loader,
the PNG decoder *and* encoder, the CAF audio decoder, and the software mixer.
The only third-party dependency is the window library (GLFW or SDL2), fetched
automatically by CMake.

```
cmake -S . -B build          # first run fetches the window library automatically
cmake --build build -j8
./build/bin/canabalt         # or ./tools/run.sh (needed for the Vulkan backend on macOS)
./build/bin/dino
./build/bin/flappy
```

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

Each game also accepts `--capture <png> [--frames N]` to render N deterministic
frames with the UI hidden and quit; `flappy` additionally accepts
`--record <dir>` (a PNG sequence) and `--demo` (self-play, for recording).

## Documentation

The engine and Canabalt are documented separately (PDF + HTML). Rebuild the PDFs
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
| `tests/smoke.cpp` | Headless engine self-tests (`otacon_smoke`) |
| `config/build.cfg` | Backend / scalar selection |
| `cmake/`, `tools/` | Config parser; run, docs and backend-diff scripts |
| `reference/` | Original Canabalt and Chromium sources (studied, not built) |

`reference/` is not tracked — it holds the upstream sources the ports were read
from, and is fetched separately.

## Status

Everything above runs. All three graphics backends (GL modern, GL legacy,
Vulkan) and both window backends (GLFW, SDL2) are implemented and
interchangeable, textured sprites and audio are in, the `FIXED` (Q16.16) build
compiles and runs, and the headless test suite passes.
