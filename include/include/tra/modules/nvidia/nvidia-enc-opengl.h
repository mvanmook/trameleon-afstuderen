#ifndef TRA_NVIDIA_ENC_OPENGL_H
#define TRA_NVIDIA_ENC_OPENGL_H
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
  
  NVIDIA ENCODER FOR OPENGL RESOURCES
  ====================================
  
  GENERAL INFO:
  
    The `tra_nvencgl` instance can be used to encode textures
    from OpenGL. This is a thin wrapper around the core NVIDIA
    encoder. We've created this speratate encoder as we need a
    context that manages the interop between OpenGL and
    NVIDIA. The `tra_nvencgl` will manage the OpenGl textures
    and registrations with the encoder.
  
    When we want to encode OpenGL textures we need to use the
    `NV_ENC_INPUT_RESOURCE_OPENGL_TEX` struct when registering
    the external buffers. The `tra_nvencgl` will allocate a
    `NV_ENC_INUPUT_RESOURCE_OPENGL_TEX` for each texture that
    is passed into `tra_nvencgl_encode()`, but only once for each
    unique texture.
  
  DEVELOPMENT INFO:

    When you want to use an OpenGL texture as source for the
    encoder, you have to pass a `tra_memory_opengl_nv12` struct
    as input for the encoder. The texture that you want to encode
    MUST be a GL_TEXTURE_2D at the moment. The NVIDIA encoder
    also supports GL_TEXTURE_RECTANGLE; though currently we
    haven't added support for this. See
    `tra_nvencgl_ensure_registered_texture()` where this target
    type is used.
   
 */

/* ------------------------------------------------------- */

#include <stdint.h>

/* ------------------------------------------------------- */

typedef struct tra_nvencgl          tra_nvencgl;
typedef struct tra_encoder_settings tra_encoder_settings;
typedef struct tra_sample           tra_sample;
typedef struct tra_nvenc_settings   tra_nvenc_settings;

/* ------------------------------------------------------- */

int tra_nvencgl_create(tra_encoder_settings* cfg, tra_nvenc_settings* settings, tra_nvencgl** ctx);
int tra_nvencgl_destroy(tra_nvencgl* ctx);
int tra_nvencgl_encode(tra_nvencgl* ctx, tra_sample* sample, uint32_t type, void* data); /* Encode using the given `type`. The `type` can be e.g. `TRA_MEMORY_TYPE_HOST_IMAGE`, see types.h for other types. */
int tra_nvencgl_flush(tra_nvencgl* ctx);

/* ------------------------------------------------------- */

#endif
