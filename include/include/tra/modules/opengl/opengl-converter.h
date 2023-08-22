#ifndef TRA_OPENGL_CONVERTER_H
#define TRA_OPENGL_CONVERTER_H
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

  OPENGL CONVERTER
  =================

  GENERAL INFO:

    The OpenGL converter is used to convert OpenGL textures. For
    example, when you want to encode OpenGL textures, you can
    directly provide OpenGL textures to the NVIDIA based encoder;
    though the textures need to be stored in a pixel format that
    the encoder accepts; e.g. NV12. You'll most likely use OpenGL
    to render a frame into a framebuffer textures which is
    GL_RGBA; therefore you can use this converter to convert from
    GL_RGBA into NV12.

    
 */

/* ------------------------------------------------------- */

#include <stdint.h>

/* ------------------------------------------------------- */

#define TRA_CONVERT_TYPE_OPENGL_NONE                 0x0000 /* "none" type; unset. */
#define TRA_CONVERT_TYPE_OPENGL_RGB24                0X1001 /* Convert using `tra_convert_opengl_rgb24` */
#define TRA_CONVERT_TYPE_OPENGL_NV12_SINGLE_TEXTURE  0x1002 /* Convert using `tra_convert_opengl_nv12` */

/* ------------------------------------------------------- */

typedef struct tra_converter_settings                   tra_converter_settings;
typedef struct tra_opengl_converter_settings            tra_opengl_converter_settings;
typedef struct tra_opengl_converter                     tra_opengl_converter;
typedef struct tra_opengl_api                           tra_opengl_api;
typedef struct tra_convert_opengl_rgb24                 tra_convert_opengl_rgb24;
typedef struct tra_convert_opengl_nv12_single_texture   tra_convert_opengl_nv12_single_texture;

/* ------------------------------------------------------- */

struct tra_convert_opengl_rgb24 {
  uint32_t tex_id;
};

struct tra_convert_opengl_nv12_single_texture {
  uint32_t tex_id;               /* One texture which holds the luma and chroma planes. */
  uint32_t tex_stride;           /* The stride that the NV12 texture uses. */
};

/* ------------------------------------------------------- */

struct tra_opengl_converter_settings {
  tra_opengl_api* gl_api;  /* The OpenGL API that we use to load the GL functions that we need. Make sure you've made the context current when creating this converter. */
};

/* ------------------------------------------------------- */

int tra_opengl_converter_create(tra_converter_settings* cfg, tra_opengl_converter_settings* settings, tra_opengl_converter** ctx);
int tra_opengl_converter_destroy(tra_opengl_converter* ctx);
int tra_opengl_converter_convert(tra_opengl_converter* ctx, uint32_t type, void* data);

/* ------------------------------------------------------- */

#endif
