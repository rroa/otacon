#include "render/IRenderer.hpp"

namespace otacon {

#if defined(OTACON_BACKEND_GL_MODERN)
IRenderer* createRendererGLModern();
IRenderer* createRenderer() { return createRendererGLModern(); }
const char* graphicsBackendName() { return "OpenGL (modern / core 3.3 + shaders)"; }
#elif defined(OTACON_BACKEND_GL_LEGACY)
IRenderer* createRendererGLLegacy();
IRenderer* createRenderer() { return createRendererGLLegacy(); }
const char* graphicsBackendName() { return "OpenGL (legacy / fixed-function 2.1)"; }
#elif defined(OTACON_BACKEND_VULKAN)
IRenderer* createRendererVulkan();
IRenderer* createRenderer() { return createRendererVulkan(); }
const char* graphicsBackendName() { return "Vulkan"; }
#else
#error "No GRAPHICS_BACKEND selected"
#endif

} // namespace otacon
