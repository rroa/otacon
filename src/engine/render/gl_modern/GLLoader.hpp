// GLLoader.hpp — tiny in-house OpenGL function loader.
//
// Instead of pulling in GLAD/GLEW we declare the handful of GL types, enums and
// entry points the modern backend actually uses, then resolve them at runtime
// through the window's proc-address (glfwGetProcAddress / SDL_GL_GetProcAddress).
// This is portable (Windows only exports GL 1.1 from opengl32.dll; everything
// 2.0+ MUST be loaded this way) and shows exactly how a GL loader works.
#pragma once
#include <cstddef>
#include <cstdint>

namespace otacon { class IWindow; }

namespace otacon::glapi {

// ---- Minimal GL typedefs ---------------------------------------------------
using GLenum     = unsigned int;
using GLbitfield = unsigned int;
using GLuint     = unsigned int;
using GLint      = int;
using GLsizei    = int;
using GLfloat    = float;
using GLclampf   = float;
using GLboolean  = unsigned char;
using GLchar     = char;
using GLsizeiptr = std::intptr_t;
using GLvoid     = void;

// ---- Enums we use ----------------------------------------------------------
constexpr GLenum GL_TRIANGLES            = 0x0004;
constexpr GLenum GL_FLOAT                = 0x1406;
constexpr GLenum GL_FALSE                = 0;
constexpr GLenum GL_TRUE                 = 1;
constexpr GLenum GL_COLOR_BUFFER_BIT     = 0x4000;
constexpr GLenum GL_BLEND                = 0x0BE2;
constexpr GLenum GL_SRC_ALPHA            = 0x0302;
constexpr GLenum GL_ONE_MINUS_SRC_ALPHA  = 0x0303;
constexpr GLenum GL_ARRAY_BUFFER         = 0x8892;
constexpr GLenum GL_DYNAMIC_DRAW         = 0x88E8;
constexpr GLenum GL_FRAGMENT_SHADER      = 0x8B30;
constexpr GLenum GL_VERTEX_SHADER        = 0x8B31;
constexpr GLenum GL_COMPILE_STATUS       = 0x8B81;
constexpr GLenum GL_LINK_STATUS          = 0x8B82;
constexpr GLenum GL_FRONT_AND_BACK       = 0x0408;
constexpr GLenum GL_LINE                 = 0x1B01;
constexpr GLenum GL_FILL                 = 0x1B02;
constexpr GLenum GL_TEXTURE_2D           = 0x0DE1;
constexpr GLenum GL_TEXTURE_MIN_FILTER   = 0x2801;
constexpr GLenum GL_TEXTURE_MAG_FILTER   = 0x2800;
constexpr GLenum GL_NEAREST              = 0x2600;
constexpr GLenum GL_TEXTURE_WRAP_S       = 0x2802;
constexpr GLenum GL_TEXTURE_WRAP_T       = 0x2803;
constexpr GLenum GL_CLAMP_TO_EDGE        = 0x812F;
constexpr GLenum GL_REPEAT               = 0x2901;
constexpr GLenum GL_RGBA                 = 0x1908;
constexpr GLenum GL_UNSIGNED_BYTE        = 0x1401;
constexpr GLenum GL_TEXTURE0             = 0x84C0;

// ---- Function pointers (defined in GLLoader.cpp) ---------------------------
#define OTACON_GL_FUNCS                                                            \
  X(void,   Clear,           (GLbitfield))                                      \
  X(void,   ClearColor,      (GLclampf, GLclampf, GLclampf, GLclampf))          \
  X(void,   Viewport,        (GLint, GLint, GLsizei, GLsizei))                  \
  X(void,   Enable,          (GLenum))                                          \
  X(void,   Disable,         (GLenum))                                          \
  X(void,   BlendFunc,       (GLenum, GLenum))                                  \
  X(void,   DrawArrays,      (GLenum, GLint, GLsizei))                          \
  X(void,   PolygonMode,     (GLenum, GLenum))                                  \
  X(GLuint, CreateShader,    (GLenum))                                          \
  X(void,   ShaderSource,    (GLuint, GLsizei, const GLchar* const*, const GLint*)) \
  X(void,   CompileShader,   (GLuint))                                          \
  X(void,   GetShaderiv,     (GLuint, GLenum, GLint*))                          \
  X(void,   GetShaderInfoLog,(GLuint, GLsizei, GLsizei*, GLchar*))              \
  X(void,   DeleteShader,    (GLuint))                                          \
  X(GLuint, CreateProgram,   (void))                                           \
  X(void,   AttachShader,    (GLuint, GLuint))                                  \
  X(void,   LinkProgram,     (GLuint))                                          \
  X(void,   GetProgramiv,    (GLuint, GLenum, GLint*))                          \
  X(void,   GetProgramInfoLog,(GLuint, GLsizei, GLsizei*, GLchar*))             \
  X(void,   UseProgram,      (GLuint))                                          \
  X(void,   DeleteProgram,   (GLuint))                                          \
  X(GLint,  GetUniformLocation,(GLuint, const GLchar*))                         \
  X(void,   Uniform2f,       (GLint, GLfloat, GLfloat))                         \
  X(void,   GenVertexArrays, (GLsizei, GLuint*))                                \
  X(void,   BindVertexArray, (GLuint))                                          \
  X(void,   DeleteVertexArrays,(GLsizei, const GLuint*))                        \
  X(void,   GenBuffers,      (GLsizei, GLuint*))                                \
  X(void,   BindBuffer,      (GLenum, GLuint))                                  \
  X(void,   BufferData,      (GLenum, GLsizeiptr, const void*, GLenum))         \
  X(void,   DeleteBuffers,   (GLsizei, const GLuint*))                          \
  X(void,   EnableVertexAttribArray,(GLuint))                                   \
  X(void,   VertexAttribPointer,(GLuint, GLint, GLenum, GLboolean, GLsizei, const void*)) \
  X(void,   Uniform1i,       (GLint, GLint))                                    \
  X(void,   ActiveTexture,   (GLenum))                                          \
  X(void,   GenTextures,     (GLsizei, GLuint*))                                \
  X(void,   BindTexture,     (GLenum, GLuint))                                  \
  X(void,   DeleteTextures,  (GLsizei, const GLuint*))                          \
  X(void,   TexParameteri,   (GLenum, GLenum, GLint))                           \
  X(void,   TexImage2D,      (GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*)) \
  X(void,   ReadPixels,      (GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void*))

#define X(ret, name, args) using PFN_##name = ret (*) args; extern PFN_##name name;
OTACON_GL_FUNCS
#undef X

// Resolve every entry point. Returns false if any is missing.
bool loadGLFunctions(IWindow* window);

} // namespace otacon::glapi
