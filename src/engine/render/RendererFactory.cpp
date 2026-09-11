/*
===========================================================================

OTACON ENGINE
render/RendererFactory.cpp - graphics backend selection

Resolves the compiled-in graphics backend. The choice is made once at build
time from config/build.cfg, so there is no runtime dispatch and no way for a
backend the build did not include to be reachable at all.

===========================================================================
*/
#include "render/IRenderer.hpp"

namespace otacon {

#if defined(OTACON_BACKEND_GL_MODERN)
IRenderer* createRendererGLModern();

/*
==================
createRenderer
==================
*/
IRenderer* createRenderer() { return createRendererGLModern(); }

/*
===================
graphicsBackendName
===================
*/
const char* graphicsBackendName() { return "OpenGL (modern / core 3.3 + shaders)"; }
#elif defined(OTACON_BACKEND_GL_LEGACY)
IRenderer* createRendererGLLegacy();

/*
==================
createRenderer
==================
*/
IRenderer* createRenderer() { return createRendererGLLegacy(); }

/*
===================
graphicsBackendName
===================
*/
const char* graphicsBackendName() { return "OpenGL (legacy / fixed-function 2.1)"; }
#elif defined(OTACON_BACKEND_VULKAN)
IRenderer* createRendererVulkan();

/*
==================
createRenderer
==================
*/
IRenderer* createRenderer() { return createRendererVulkan(); }

/*
===================
graphicsBackendName
===================
*/
const char* graphicsBackendName() { return "Vulkan"; }
#else
#error "No GRAPHICS_BACKEND selected"
#endif

} // namespace otacon
