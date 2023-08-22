#ifndef TRA_OPENGL_CUDA_H
#define TRA_OPENGL_CUDA_H

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

  OPENGL INTEROP WITH CUDA
  ========================

  GENERAL INFO:

    This interop module implements a way to share buffers between
    CUDA and OpenGL. The goal is to make decoded frames available
    to OpenGL and to share OpenGL buffers with CUDA so they can
    be encoded easily.

  USAGE:

    You can use this module via the `opengl-cuda` interop module
    or directly by using the public functions as listed below.
    When using the core functions the `gl_cu` is represented by a
    `tra_interop` instance.
    
*/    

/* ------------------------------------------------------- */

#include <stdint.h>

/* ------------------------------------------------------- */

typedef struct gl_cu                 gl_cu;
typedef struct gl_cu_settings        gl_cu_settings;
typedef struct tra_interop_settings  tra_interop_settings;
typedef struct tra_opengl_api        tra_opengl_api;

/* ------------------------------------------------------- */

struct gl_cu_settings {
  tra_opengl_api* gl_api; /* The OpenGL API */
};

/* ------------------------------------------------------- */

int gl_cu_create(tra_interop_settings* cfg, gl_cu_settings* settings, gl_cu** ctx);
int gl_cu_destroy(gl_cu* ctx);
int gl_cu_upload(gl_cu* ctx, uint32_t type, void* data);

/* ------------------------------------------------------- */

#endif
