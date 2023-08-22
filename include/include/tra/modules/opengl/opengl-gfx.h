#ifndef TRA_OPENGL_GFX_H
#define TRA_OPENGL_GFX_H

/*
  ┌─────────────────────────────────────────────────────────────────────────────────────┐
  │                                                                                     │
  │   ████████╗██████╗  █████╗ ███╗   ███╗███████╗██╗     ███████╗ ██████╗ ███╗   ██╗   │
  │   ╚══██╔══╝██╔══██╗██╔══██╗████╗ ████║██╔════╝██║     ██╔════╝██╔═══██╗████╗  ██║   │
  │      ██║   ██████╔╝███████║██╔████╔██║█████╗  ██║     █████╗  ██║   ██║██╔██╗ ██║   │
  │      ██║   ██╔══██╗██╔══██║██║╚██╔╝██║██╔══╝  ██║     ██╔══╝  ██║   ██║██║╚██╗██║   │
  │      ██║   ██║  ██║██║  ██║██║ ╚═╝ ██║███████╗███████╗███████╗╚██████╔╝██║ ╚████║   │
  │      ╚═╝   ╚═╝  ╚═╝╚═╝  ╚═╝╚═╝     ╚═╝╚══════╝╚══════╝╚══════╝ ╚═════╝ ╚═╝  ╚═══╝   │
  │                                                                 www.trameleon.org   │
  └─────────────────────────────────────────────────────────────────────────────────────┘

  OPENGL GRAPHICS
  ================

  GENERAL INFO:

    This graphics module provides functions that allow you to
    display decoded pictures. Currently this graphics interop is
    experimental and I'm not 100% sure yet if the approach we
    take here is correct. For example we're loading the GL
    functions on the fly which means an OpenGL context must be
    made current by the user. Maybe it makes more sense that the
    user provides an already initialized `tra_opengl_funcs`
    struct.

 TODO:

    @todo I've added some comments here as it needs to be
    resolved. This current implementation is build around the
    NVIDIA decoder. This means that it depends on CUDA and uses
    CUDA structs. I might have to separate some of the features
    into separate modules; e.g. `opengl-cuda-gfx` or
    `cuda-opengl-gfx`? Hmm... currently the CUDA dependent code
    is only related to uploading content; the rest of the opengl
    graphics code could be shared.
    
 */
/* ------------------------------------------------------- */

#include <stdint.h>

/* ------------------------------------------------------- */

typedef struct gl_gfx                gl_gfx;
typedef struct gl_gfx_settings       gl_gfx_settings;
typedef struct tra_graphics_settings tra_graphics_settings;
typedef struct tra_opengl_api        tra_opengl_api; 

/* ------------------------------------------------------- */

struct gl_gfx_settings {
  tra_opengl_api* gl_api; /* The OpenGL API */
};

/* ------------------------------------------------------- */

int gl_gfx_create(tra_graphics_settings* cfg, gl_gfx_settings* settings, gl_gfx** ctx);
int gl_gfx_destroy(gl_gfx* ctx);
int gl_gfx_draw(gl_gfx* ctx, uint32_t type, void* data);

/* ------------------------------------------------------- */

#endif 
