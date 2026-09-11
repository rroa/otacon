/*
===========================================================================

OTACON ENGINE
render/gl_modern/GLLoader.cpp - OpenGL function loader

Resolves the entry points the modern backend uses, through the window's
proc-address.

This has to exist. Windows only exports OpenGL 1.1 from opengl32.dll, so
everything from 2.0 on must be fetched at runtime - which is all GLAD and
GLEW are doing, for a much larger surface than we need.

===========================================================================
*/
#include "render/gl_modern/GLLoader.hpp"
#include "platform/Window.hpp"
#include <cstdio>

namespace otacon::glapi {

#define X(ret, name, args) PFN_##name name = nullptr;
OTACON_GL_FUNCS
#undef X

/*
==================
loadGLFunctions

Resolve every entry point in the X-macro list, and fail loudly on the first
one the driver does not have. Failing here beats a null call later.
==================
*/
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
