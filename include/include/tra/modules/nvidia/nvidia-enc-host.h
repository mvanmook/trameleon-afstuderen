#ifndef TRA_NVIDIA_ENC_HOST_H
#define TRA_NVIDIA_ENC_HOST_H
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

  NVIDIA ENCODER FOR CPU RESOURCES
  ================================

  GENERAL INFO:

    The `tra_nvenchost` instance can be used to encode buffers
    which reside in host memory (CPU memory). Similar to the
    `tra_nvencgl` encoder which can use textures as input, this
    is a thin wrapper that manages resources and uses `tra_nvenc`
    for most of the work.

    When we want to encode buffers from CPU memory, we first
    have to copy them into device buffers. We use CUDA to copy
    the host memory to device memory. Once the CPU buffers have
    been copied into device memory, encode them. But we
    have to do a bit more work before we can use the device
    buffers.

    So, we have to get the host memory buffers onto the
    device. We simply copy them using the cuda memory copy
    functions. These functions ensure that the data moves from
    host to device memory. But ... before the encoder can use
    these device buffers, we have to register them with the
    encoder. That's the second big part what this encoder does:
    it registers the allocated device buffers with the encoder.
    
      ┌────────┐
      │        │
      │  cuda  ├──┐
      │        │  │
      └────────┘  │                                    NV_ENC_REGISTER_RESOURCE
                  │
      ┌────────┐  │    ┌─────────────────────────┐     ┌────────────────────┐
      │        │  │    │                         │     │                    │
      │ opengl ├──┼───►│ nvEncRegisterResource() ├────►│ registeredResource │
      │        │  │    │                         │     │                    │
      └────────┘  │    └─────────────────────────┘     └────────────────────┘
                  │
      ┌────────┐  │
      │        │  │
      │ d3d12  ├──┘
      │        │
      └────────┘

    
  HOW MANY BUFFERS SHOULD WE CREATE?

    Based on the ["note"][1] from the online documentation, we
    should allocate `1 + NB` input and output buffers, where NB
    is the number of B-frames between successive P frames. See
    `tra_nvenc_create()` where we set the `num_input_buffers` and
    `num_output_buffers`; this is based on
    `NvEncoder::CreateEncoder()` where they set the
    `m_nEncoderBuffer` to the number of buffers.

  REFERENCES:

    [0]: https://docs.nvidia.com/video-technologies/video-codec-sdk/nvenc-video-encoder-api-prog-guide/#preparing-input-buffers-for-encoding
    [1]: https://docs.nvidia.com/video-technologies/video-codec-sdk/nvenc-video-encoder-api-prog-guide/#creating-resources-required-to-hold-inputoutput-data
    
 */

/* ------------------------------------------------------- */

#include <stdint.h>

/* ------------------------------------------------------- */

typedef struct tra_nvenchost        tra_nvenchost;
typedef struct tra_encoder_settings tra_encoder_settings;
typedef struct tra_sample           tra_sample;
typedef struct tra_nvenc_settings   tra_nvenc_settings;

/* ------------------------------------------------------- */

int tra_nvenchost_create(tra_encoder_settings* cfg, tra_nvenc_settings* settings, tra_nvenchost** ctx);
int tra_nvenchost_destroy(tra_nvenchost* ctx);
int tra_nvenchost_encode(tra_nvenchost* ctx, tra_sample* sample, uint32_t type, void* data);
int tra_nvenchost_flush(tra_nvenchost* ctx); 

/* ------------------------------------------------------- */

#endif
