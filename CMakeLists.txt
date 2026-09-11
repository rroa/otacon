cmake_minimum_required(VERSION 3.16)
project(otacon_canabalt LANGUAGES CXX C)

# ----------------------------------------------------------------------------
# Build configuration (config/build.cfg) -> OTACON_* variables.
# Override on the command line with -DGRAPHICS_BACKEND=... etc if desired.
# ----------------------------------------------------------------------------
include(${CMAKE_SOURCE_DIR}/cmake/ReadBuildCfg.cmake)
otacon_read_build_cfg(${CMAKE_SOURCE_DIR}/config/build.cfg)

set(GRAPHICS_BACKEND "${OTACON_GRAPHICS_BACKEND}" CACHE STRING "GL_LEGACY|GL_MODERN|VULKAN")
set(WINDOW_BACKEND   "${OTACON_WINDOW_BACKEND}"   CACHE STRING "GLFW|SDL2")
set(SCALAR_TYPE      "${OTACON_SCALAR_TYPE}"      CACHE STRING "FLOAT|FIXED")
set(MEMORY_TRACKING  "${OTACON_MEMORY_TRACKING}"  CACHE STRING "ON|OFF")
set(DEBUG_RUNTIME    "${OTACON_DEBUG_RUNTIME}"    CACHE STRING "ON|OFF")
message(STATUS "Otacon: graphics=${GRAPHICS_BACKEND} window=${WINDOW_BACKEND} scalar=${SCALAR_TYPE}")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# ============================================================================
#  Otacon engine — a reusable static library. Game-agnostic; Canabalt (and a
#  future Flappy Bird) link against it.
# ============================================================================
file(GLOB_RECURSE ENGINE_CORE CONFIGURE_DEPENDS
  ${CMAKE_SOURCE_DIR}/src/engine/core/*.cpp
  ${CMAKE_SOURCE_DIR}/src/engine/scene/*.cpp
  ${CMAKE_SOURCE_DIR}/src/engine/asset/*.cpp
  ${CMAKE_SOURCE_DIR}/src/engine/audio/*.cpp
  ${CMAKE_SOURCE_DIR}/src/engine/render/IRenderer.cpp
  ${CMAKE_SOURCE_DIR}/src/engine/render/RendererFactory.cpp
  ${CMAKE_SOURCE_DIR}/src/engine/platform/PlatformFactory.cpp
  ${CMAKE_SOURCE_DIR}/src/engine/App.cpp)

set(ENGINE_DEFS "")
set(ENGINE_LIBS "")
set(ENGINE_BACKEND_SRC "")

include(FetchContent)

# ---- Windowing backend (pluggable) -----------------------------------------
if(WINDOW_BACKEND STREQUAL "GLFW")
  list(APPEND ENGINE_DEFS OTACON_WINDOW_GLFW)
  file(GLOB W_SRC ${CMAKE_SOURCE_DIR}/src/engine/platform/glfw/*.cpp)
  list(APPEND ENGINE_BACKEND_SRC ${W_SRC})
  set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
  set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(glfw GIT_REPOSITORY https://github.com/glfw/glfw.git GIT_TAG 3.4)
  FetchContent_MakeAvailable(glfw)
  list(APPEND ENGINE_LIBS glfw)
elseif(WINDOW_BACKEND STREQUAL "SDL2")
  list(APPEND ENGINE_DEFS OTACON_WINDOW_SDL2)
  file(GLOB W_SRC ${CMAKE_SOURCE_DIR}/src/engine/platform/sdl2/*.cpp)
  list(APPEND ENGINE_BACKEND_SRC ${W_SRC})
  # Build SDL2 statically so there is no runtime .dylib to locate (and skip its
  # own tests/examples) — same self-contained spirit as the GLFW FetchContent.
  set(SDL_SHARED OFF CACHE BOOL "" FORCE)
  set(SDL_STATIC ON  CACHE BOOL "" FORCE)
  set(SDL_TEST   OFF CACHE BOOL "" FORCE)
  set(SDL2_DISABLE_INSTALL ON CACHE BOOL "" FORCE)
  FetchContent_Declare(SDL2 GIT_REPOSITORY https://github.com/libsdl-org/SDL.git GIT_TAG release-2.30.9)
  FetchContent_MakeAvailable(SDL2)
  list(APPEND ENGINE_LIBS SDL2::SDL2-static)
else()
  message(FATAL_ERROR "Unknown WINDOW_BACKEND: ${WINDOW_BACKEND}")
endif()

# ---- Graphics backend ------------------------------------------------------
if(GRAPHICS_BACKEND STREQUAL "GL_MODERN")
  list(APPEND ENGINE_DEFS OTACON_BACKEND_GL_MODERN)
  file(GLOB G_SRC ${CMAKE_SOURCE_DIR}/src/engine/render/gl_modern/*.cpp)
  find_package(OpenGL REQUIRED)
  list(APPEND ENGINE_LIBS OpenGL::GL)
elseif(GRAPHICS_BACKEND STREQUAL "GL_LEGACY")
  list(APPEND ENGINE_DEFS OTACON_BACKEND_GL_LEGACY)
  file(GLOB G_SRC ${CMAKE_SOURCE_DIR}/src/engine/render/gl_legacy/*.cpp)
  find_package(OpenGL REQUIRED)
  list(APPEND ENGINE_LIBS OpenGL::GL)
elseif(GRAPHICS_BACKEND STREQUAL "VULKAN")
  list(APPEND ENGINE_DEFS OTACON_BACKEND_VULKAN)
  file(GLOB G_SRC ${CMAKE_SOURCE_DIR}/src/engine/render/vulkan/*.cpp)
  if(APPLE)
    list(APPEND CMAKE_PREFIX_PATH /opt/homebrew /usr/local)   # Homebrew Vulkan loader + MoltenVK
  endif()
  find_package(Vulkan REQUIRED)
  list(APPEND ENGINE_LIBS Vulkan::Vulkan)
else()
  message(FATAL_ERROR "Unknown GRAPHICS_BACKEND: ${GRAPHICS_BACKEND}")
endif()
list(APPEND ENGINE_BACKEND_SRC ${G_SRC})

if(SCALAR_TYPE STREQUAL "FIXED")
  list(APPEND ENGINE_DEFS OTACON_SCALAR_FIXED)
else()
  list(APPEND ENGINE_DEFS OTACON_SCALAR_FLOAT)
endif()
if(MEMORY_TRACKING STREQUAL "ON")
  list(APPEND ENGINE_DEFS OTACON_MEMORY_TRACKING)
endif()
if(DEBUG_RUNTIME STREQUAL "ON")
  list(APPEND ENGINE_DEFS OTACON_DEBUG_RUNTIME)
endif()

add_library(otacon STATIC ${ENGINE_CORE} ${ENGINE_BACKEND_SRC})
target_include_directories(otacon PUBLIC ${CMAKE_SOURCE_DIR}/src/engine)
target_compile_definitions(otacon PUBLIC ${ENGINE_DEFS})
target_link_libraries(otacon PUBLIC ${ENGINE_LIBS})
if(APPLE)
  target_link_libraries(otacon PUBLIC "-framework Cocoa" "-framework IOKit" "-framework CoreVideo"
                                      "-framework AudioToolbox" "-framework AudioUnit" "-framework CoreAudio")
endif()

# ============================================================================
#  Canabalt — the game, built on Otacon.
# ============================================================================
file(GLOB_RECURSE CANABALT_SRC CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/src/game/canabalt/*.cpp)
add_executable(canabalt ${CANABALT_SRC})
target_include_directories(canabalt PRIVATE ${CMAKE_SOURCE_DIR}/src/game)
target_link_libraries(canabalt PRIVATE otacon)
target_compile_definitions(canabalt PRIVATE OTACON_ASSET_DIR="${CMAKE_SOURCE_DIR}/src/game/canabalt/assets")

# ============================================================================
#  Dino — Chromium's offline runner, ported as a didactic Otacon game.
# ============================================================================
file(GLOB_RECURSE DINO_SRC CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/src/game/dino/*.cpp)
add_executable(dino ${DINO_SRC})
target_include_directories(dino PRIVATE ${CMAKE_SOURCE_DIR}/src/game)
target_link_libraries(dino PRIVATE otacon)
target_compile_definitions(dino PRIVATE OTACON_ASSET_DIR="${CMAKE_SOURCE_DIR}/src/game/dino/assets")

# ============================================================================
#  Flappy Bird — an original infinite flapper, built progressively as scene builds.
# ============================================================================
file(GLOB_RECURSE FLAPPY_SRC CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/src/game/flappy/*.cpp)
add_executable(flappy ${FLAPPY_SRC})
target_include_directories(flappy PRIVATE ${CMAKE_SOURCE_DIR}/src/game)
target_link_libraries(flappy PRIVATE otacon)
target_compile_definitions(flappy PRIVATE OTACON_ASSET_DIR="${CMAKE_SOURCE_DIR}/src/game/flappy/assets")

# ============================================================================
#  Engine samples — fifteen screens demonstrating the engine itself, hosted by
#  one executable. Not a game: this links otacon and nothing else, which is the
#  point (a sample that needed a game's code would mean the seam leaked).
# ============================================================================
file(GLOB_RECURSE SAMPLES_SRC CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/src/samples/*.cpp)
add_executable(samples ${SAMPLES_SRC})
target_include_directories(samples PRIVATE ${CMAKE_SOURCE_DIR}/src/samples)
target_link_libraries(samples PRIVATE otacon)

# ============================================================================
#  Headless engine smoke tests (no window). Build & run: otacon_smoke
# ============================================================================
if(EXISTS ${CMAKE_SOURCE_DIR}/tests/smoke.cpp)
  # Includes a couple of game sources so the level generator can be tested too.
  add_executable(otacon_smoke
    ${CMAKE_SOURCE_DIR}/tests/smoke.cpp
    ${CMAKE_SOURCE_DIR}/src/game/canabalt/Player.cpp
    ${CMAKE_SOURCE_DIR}/src/game/canabalt/Sequence.cpp)
  target_include_directories(otacon_smoke PRIVATE ${CMAKE_SOURCE_DIR}/src/game)
  target_link_libraries(otacon_smoke PRIVATE otacon)
  target_compile_definitions(otacon_smoke PRIVATE OTACON_ASSET_DIR="${CMAKE_SOURCE_DIR}/src/game/canabalt/assets")
endif()

message(STATUS "Otacon: configured  (engine lib 'otacon' + games 'canabalt', 'dino', 'flappy' + 'samples')")
