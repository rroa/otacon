# Installing and running Otacon

Everything is built with CMake. The only dependency you have to supply is a
compiler and CMake itself — the window library (GLFW or SDL2) is downloaded and
built automatically on the first configure, which is why **the first build needs
a network connection and `git` on your `PATH`**.

> **Verification status.** This project is developed on macOS, and the macOS
> instructions below are exercised routinely: all three graphics backends and
> both window backends are built and run there. The Linux and Windows sections
> are derived from the build files and the source rather than from a test run on
> those platforms — the code has no POSIX-only headers and no platform
> assumptions outside guarded blocks, but treat them as a careful reading rather
> than a certified recipe. If something there is wrong, it is a bug worth filing.

---

## Requirements

| | Minimum | Notes |
|---|---|---|
| CMake | 3.16 | `cmake --version` |
| Compiler | C++17 | AppleClang 11+, GCC 9+, Clang 9+, MSVC 2019+ |
| Git | any | used by CMake's `FetchContent` on the first configure |
| OpenGL | 2.1 / 3.3 | 3.3 core for the default backend; 2.1 for `GL_LEGACY` |
| Vulkan SDK | optional | only for `GRAPHICS_BACKEND=VULKAN` |
| Tectonic | optional | only to rebuild the PDF manuals (`tools/build-docs.sh`) |

Disk: a GLFW build tree runs **50–60 MB** (31 MB of that the fetched GLFW), an
SDL2 one about **310 MB**, since SDL is a much larger thing to build. Measured on
macOS; other toolchains will differ, but the ratio will not.

---

## macOS

```bash
xcode-select --install          # Apple Clang + headers, if you have not already
brew install cmake              # or from cmake.org

git clone <your-remote> otacon && cd otacon
cmake -S . -B build
cmake --build build -j8

./build/bin/samples             # F1 for the sample list
./build/bin/canabalt
./build/bin/dino
./build/bin/flappy
```

Audio works out of the box: the CoreAudio backend is compiled in on Apple
platforms.

### Vulkan on macOS

Vulkan runs through MoltenVK and needs the loader on the library path, so
**launch through the wrapper** rather than the binary directly:

```bash
brew install molten-vk vulkan-loader
cmake -S . -B build -DGRAPHICS_BACKEND=VULKAN
cmake --build build -j8

./tools/run.sh samples          # sets DYLD_FALLBACK_LIBRARY_PATH + VK_ICD_FILENAMES
./tools/run.sh canabalt
```

Running `./build/bin/samples` directly on this backend will fail to find the
loader. `tools/run.sh` takes the target to launch — `canabalt` (the default),
`dino`, `flappy` or `samples` — and is harmless on the OpenGL backends.

---

## Linux

GLFW is built from source, so it needs the X11 (or Wayland) development headers.
This is the step most likely to bite you, and the error it produces when they are
missing comes from GLFW's CMake rather than from this project.

**Debian / Ubuntu**

```bash
sudo apt install build-essential cmake git \
     libgl1-mesa-dev libx11-dev libxrandr-dev libxinerama-dev \
     libxcursor-dev libxi-dev
# Wayland instead of X11: add libwayland-dev libxkbcommon-dev wayland-protocols
```

**Fedora / RHEL**

```bash
sudo dnf install gcc-c++ cmake git \
     mesa-libGL-devel libX11-devel libXrandr-devel libXinerama-devel \
     libXcursor-devel libXi-devel
```

**Arch**

```bash
sudo pacman -S base-devel cmake git mesa libx11 libxrandr libxinerama libxcursor libxi
```

Then:

```bash
git clone <your-remote> otacon && cd otacon
cmake -S . -B build
cmake --build build -j$(nproc)

./build/bin/samples
```

### Two things to expect on Linux

* **Audio is silent.** The only real audio backend is CoreAudio. Everywhere else
  the factory returns a stub that accepts every call and does nothing, so the
  games run normally and simply make no sound. This is deliberate — see
  `audio/NullAudio.cpp` — not a misconfiguration on your machine.
* **Vulkan** needs the loader and headers (`vulkan-validationlayers`,
  `libvulkan-dev` / `vulkan-loader` / `vulkan-devel` depending on the distro).
  `tools/run.sh` is macOS-specific in what it sets, but harmless; on Linux a
  correctly installed loader needs no wrapper, so run the binary directly.

---

## Windows

Two supported routes. **Visual Studio is the easier one.**

### Visual Studio (MSVC)

Install *Visual Studio 2019 or newer* with the **Desktop development with C++**
workload, which brings MSVC, CMake and Git. Then, from a Developer Command
Prompt:

```bat
git clone <your-remote> otacon
cd otacon
cmake -S . -B build
cmake --build build --config Release -j8

build\bin\Release\samples.exe
```

Note the `--config Release` and the extra `Release\` path segment: the Visual
Studio generator is multi-config, so the build type is chosen at build time
rather than at configure time, and binaries land one directory deeper than on the
single-config generators used elsewhere.

You can also open the generated `build\otacon_canabalt.sln` and build from the
IDE; set `samples`, `canabalt`, `dino` or `flappy` as the startup project.

### MinGW-w64 / MSYS2

```bash
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake git
cmake -S . -B build -G "MinGW Makefiles"
cmake --build build -j8

./build/bin/samples.exe
```

### Three things to expect on Windows

* **Audio is silent**, for the same reason as Linux.
* **OpenGL needs no extra SDK.** Windows exports only OpenGL 1.1 from
  `opengl32.dll`, which is exactly why the modern backend ships its own function
  loader (`render/gl_modern/GLLoader`) instead of depending on GLAD or GLEW.
  `GL_LEGACY` uses only 1.1 entry points and needs no loader either.
* **The scripts in `tools/` are bash.** Use Git Bash, MSYS2 or WSL for
  `run.sh`, `backend-diff.sh` and `build-docs.sh`; nothing in the build itself
  needs them, and on Windows there is no loader path to set, so you can launch
  the executables directly.

---

## Choosing backends

Edit `config/build.cfg` and re-run CMake, or override per-configure on the
command line. Both are equivalent; the file is the persistent form.

```bash
cmake -S . -B build -DGRAPHICS_BACKEND=GL_LEGACY -DWINDOW_BACKEND=SDL2
```

| Key | Values | Default |
|---|---|---|
| `GRAPHICS_BACKEND` | `GL_MODERN` · `GL_LEGACY` · `VULKAN` | `GL_MODERN` |
| `WINDOW_BACKEND` | `GLFW` · `SDL2` | `GLFW` |
| `SCALAR_TYPE` | `FLOAT` · `FIXED` | `FLOAT` |
| `MEMORY_TRACKING` | `ON` · `OFF` | `ON` |
| `DEBUG_RUNTIME` | `ON` · `OFF` | `ON` |

Use a **separate build directory per configuration** rather than reconfiguring
one in place — it is faster and avoids stale-cache surprises:

```bash
cmake -S . -B build-vk  -DGRAPHICS_BACKEND=VULKAN
cmake -S . -B build-sdl -DWINDOW_BACKEND=SDL2
```

Selecting `SDL2` makes the first configure fetch and build SDL, which takes
noticeably longer than GLFW and turns a ~50 MB build tree into a ~310 MB one.

---

## Checking it works

```bash
cmake --build build --target otacon_smoke -j8
./build/bin/otacon_smoke
```

Headless — no window, no renderer — because the simulation is decoupled from
rendering. It exercises the integrator, collision, the emitter, fixed-point
math, the level generator, the PNG and CAF decoders and the audio mixer, and
prints a line per check. A clean run ends with `ALL PASS (0 failures)`.

To confirm a graphics backend renders, capture a frame without needing to look
at a window:

```bash
./build/bin/samples --sample 1 --capture shot.png --frames 30
```

That renders 30 deterministic frames with the dev UI hidden, writes a PNG
through the in-house encoder and quits. `tools/backend-diff.sh` (bash) does this
across all three backends and compares the results.

---

## Troubleshooting

**`Could NOT find OpenGL`** — install the GL development package
(`libgl1-mesa-dev`, `mesa-libGL-devel`). On Windows this should not happen; MSVC
finds `opengl32.lib` in the SDK.

**GLFW fails to configure, complaining about X11** — the development headers are
missing. Install the list for your distro above.

**The first configure hangs or fails to download** — `FetchContent` clones GLFW
(and SDL2, if selected) from GitHub. It needs network access and `git` on the
`PATH`. Behind a proxy, configure git's proxy settings first.

**`vkCreateInstance` fails / no Vulkan device** — the loader cannot find an ICD.
On macOS install `molten-vk` and launch via `tools/run.sh`. On Linux install the
loader and your vendor's driver package.

**The window opens but is black, or the game runs with no sound** — no sound off
macOS is expected, not a fault. A black window is worth reporting; try
`-DGRAPHICS_BACKEND=GL_LEGACY`, which has the fewest requirements of the three
and will run almost anywhere with a GL 2.1 context.

**`CMakeCache.txt` directory mismatch** — a build tree was moved or copied.
Delete the build directory and configure again; CMake caches absolute paths.
