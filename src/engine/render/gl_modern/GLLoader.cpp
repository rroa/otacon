#include "render/gl_modern/GLLoader.hpp"
#include "platform/Window.hpp"
#include <cstdio>

namespace otacon::glapi {

#define X(ret, name, args) PFN_##name name = nullptr;
OTACON_GL_FUNCS
#undef X

bool loadGLFunctions(IWindow* w) {
    bool ok = true;
#define X(ret, name, args)                                                   \
    name = reinterpret_cast<PFN_##name>(w->glProcAddress("gl" #name));       \
    if (!name) { std::fprintf(stderr, "[gl] missing entry point gl%s\n", #name); ok = false; }
    OTACON_GL_FUNCS
#undef X
    return ok;
}

} // namespace otacon::glapi
