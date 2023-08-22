#ifndef TRA_NVIDIA_ENC_H
#define TRA_NVIDIA_ENC_H
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
  
  NVIDIA ENCODER 
  ===============

  GENERAL INFO:

    This file provides the API that can be used to create an
    encoder that uses the NVIDIA Video SDK through variants like
    CUDA, OpenGL, etc.  For each of these variants we provide a
    specific implementation. For example, we have a CUDA
    implementation which will take care of allocating the correct
    input buffers; the OpenGL implementation will prepare the
    encoder in such a way that we can use OpenGL textures as
    input. This `tra_nvenc` will be used by these variants.

  RESOURCES:

    When you want to encode a buffer, you have to register the
    buffer with the encoder first.  Below we provide the
    `tra_nvenc_resources_*()` API which is used to manage
    resources that the encoder uses. This API is used by the
    different variants of the NVIDIA encoder implementations. For
    example we have an encoder implementation that uses CUDA
    buffers, OpenGL textures, etc. All of these variants needs to
    manage their external buffers (CUDA buffers, OpenGL
    textures). To manage this they use the
    `tra_nvenc_resources_*()` API.

    Each encoder variant can use the `tra_nvenc_resources`
    context to manage its resource registrations with the encoder
    session. It takes care of registering resources with the
    encoder. To ensure that an external resource is registered
    you can use `tra_nvenc_resources_ensure_registration()`. This
    function will only register the given external resource
    once. When it has registered the resources, the resource will
    be given to the caller of this function via the output
    parameter.

  DEVELOPMENT:

    The `tra_nvenc` is the base encoder which takes care of
    setting up the encoder and handling most of the important
    work. Though the `tra_nvenc` can't be used on it's own. It
    needs an encoder variant that manages input buffers. These
    buffers are also referred to as external buffers. Have a look
    at e.g. the OpenGL or CUDA based variants for more details
    how these are implemented.

    It's important to know that the encoder variants will call
    `tra_nvenc_encode_with_registration()` when they want to
    encode an texture, CUDA buffer, etc. These buffers are
    registered with the encoder using the `tra_nvenc_resources`
    contents. The `tra_nvenc_get_input_buffer_count()` will
    return the number of input buffers that an encoder variant
    should allocate.

  REFERENCES:

    [0]: https://github.com/NVIDIA/video-sdk-samples/blob/master/Samples/NvCodec/NvEncoder/NvEncoderCuda.cpp "NVEncoderCuda"

 */

/* ------------------------------------------------------- */

#include <stdint.h>

/* ------------------------------------------------------- */

/*
  When we create an NVIDIA encoder we have to tell it what kind
  of "device" we use. E.g. when you create an NVIDIA encoder that
  support OpenGL textures as input then we have to create the
  encoder with the OPENGL device type.
 */
#define TRA_NVENC_DEVICE_TYPE_NONE       0x0000
#define TRA_NVENC_DEVICE_TYPE_CUDA       0x0001       /* Used by the CUDA based encoder variant. */
#define TRA_NVENC_DEVICE_TYPE_OPENGL     0x0002       /* Used by the OpenGL based encoder variant; this can be used to encode OpenGL textures. */
#define TRA_NVENC_DEVICE_TYPE_DIRECTX    0x0003       /* Used by the DirectX based encoder variant. */

/* ------------------------------------------------------- */

typedef struct tra_encoder_settings          tra_encoder_settings;
typedef struct tra_sample                    tra_sample;
typedef struct tra_nvenc                     tra_nvenc;
typedef struct tra_nvenc_settings            tra_nvenc_settings;
typedef struct tra_nvenc_resource            tra_nvenc_resource;
typedef struct tra_nvenc_resources           tra_nvenc_resources;
typedef struct tra_nvenc_resources_settings  tra_nvenc_resources_settings;
typedef struct tra_nvenc_registration        tra_nvenc_registration;

/* ------------------------------------------------------- */

struct tra_nvenc_settings {
  void* cuda_context_handle;      /* Pointer to a CUcontext, e.g. use `tra_cuda_api.context_get_handle()`.  */
  void* cuda_device_handle;       /* Pointer to a CUdevice, e.g. use `tra_cuda_api.device_get_handle()`.  */
  tra_nvenc_resources* resources; /* Set by e.g. `tra_nvenchost`, `tra_nvenccuda`, etc. The resource manager which is created and set by a concrete encoder implementation; e.g. `tra_nvenchost`, `tra_nvencgl` etc. This manages the input buffers. */
  uint32_t device_type;           /* Set by e.g. `tra_nvenchost`, `tra_nvenccuda`, etc. One of the `TRA_NVENC_DEVICE_TYPE_{CUDA,OPENGL,DIRECTX}` types. This is set by one to the encoder variants. */
};

/* ------------------------------------------------------- */

struct tra_nvenc_resources_settings {
  tra_nvenc* enc;                /* The encoder to which resources are registered. */
};

/* ------------------------------------------------------- */

struct tra_nvenc_resource {
  uint32_t resource_type;       /* The type of resource that we want to register, e.g. `NV_ENC_INPUT_RESOURCE_TYPE_OPENGL_TEX`. */
  uint32_t resource_stride;     /* The stride of the buffer that represents the resource. */
  void* resource_ptr;           /* Pointer to the (external) buffer. This can be e.g. a CUDA array, or an intermediate type that the NVENC API provides like `NV_ENC_INPUT_RESOURCE_OPENGL_TEX` */
};

/* ------------------------------------------------------- */

int tra_nvenc_create(tra_encoder_settings* cfg, tra_nvenc_settings* settings, tra_nvenc** ctx);
int tra_nvenc_destroy(tra_nvenc* ctx);
int tra_nvenc_flush(tra_nvenc* ctx);

/* ------------------------------------------------------- */

int tra_nvenc_encode_with_registration(tra_nvenc* ctx, tra_sample* sample, tra_nvenc_registration* reg);
int tra_nvenc_get_input_buffer_count(tra_nvenc* ctx, uint32_t* count); /* Get the number of input buffers that should be allocate. When we e.g. fee images from CPU memory, we need to create a couple of device buffers. This function gives you the number of buffers to create. See e.g. `nvidia-enc-host.c` where this is used. */

/* ------------------------------------------------------- */

int tra_nvenc_resources_create(tra_nvenc_resources_settings* cfg, tra_nvenc_resources** ctx);
int tra_nvenc_resources_destroy(tra_nvenc_resources* ctx);
int tra_nvenc_resources_ensure_registration(tra_nvenc_resources* ctx, tra_nvenc_resource* resourceToEnsure, tra_nvenc_registration** resourceRegistration); /* Add the given `tra_nvenc_resource*` to the resources context. */
int tra_nvenc_resources_get_usable_registration(tra_nvenc_resources* ctx, tra_nvenc_registration** usableRegistration, void** registeredResourcePtr);

/* ------------------------------------------------------- */

#endif
