/*
 * nuklear_impl.c — Provides NK_IMPLEMENTATION without the GLFW backend.
 *
 * Used for linking test binaries that depend on ui_infra.o but don't
 * need an actual GL context.  This satisfies all nk_* symbols without
 * pulling in GLFW or OpenGL.
 */
#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
#define NK_IMPLEMENTATION

#include "nuklear.h"
