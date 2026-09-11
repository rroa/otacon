#include "platform/Window.hpp"

namespace otacon {

// Each backend provides these; only one is compiled in (see CMakeLists.txt).
#if defined(OTACON_WINDOW_GLFW)
IWindow* createWindowGlfw(const WindowConfig&);
IWindow* createWindow(const WindowConfig& cfg) { return createWindowGlfw(cfg); }
const char* windowBackendName() { return "GLFW"; }
#elif defined(OTACON_WINDOW_SDL2)
IWindow* createWindowSdl2(const WindowConfig&);
IWindow* createWindow(const WindowConfig& cfg) { return createWindowSdl2(cfg); }
const char* windowBackendName() { return "SDL2"; }
#else
#error "No WINDOW_BACKEND selected"
#endif

} // namespace otacon
