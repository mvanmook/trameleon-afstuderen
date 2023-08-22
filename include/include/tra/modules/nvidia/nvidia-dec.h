#ifndef TRA_NVIDIA_DEC_H
#define TRA_NVIDIA_DEC_H
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
  
  NVIDIA DECODER
  ==============

  GENERAL INFO:

    This is the public API that is used by trameleon to create
    the NVIDIA based decoder. This code is partly based on [this
    example][0].

  REFERENCES:

    [0]: https://github.com/NVIDIA/video-sdk-samples/blob/master/Samples/NvCodec/NvDecoder/NvDecoder.cpp
    [1]: https://docs.nvidia.com/video-technologies/video-codec-sdk/nvdec-video-decoder-api-prog-guide/index.html "NVIDIA decoder programmers guide."
    
 */

/* ------------------------------------------------------- */

#include <stdint.h>
#include <tra/api.h>

/* ------------------------------------------------------- */

typedef struct tra_nvdec            tra_nvdec;
typedef struct tra_nvdec_settings   tra_nvdec_settings;
typedef struct tra_decoder_settings tra_decoder_settings;

/* ------------------------------------------------------- */

struct tra_nvdec_settings {
  void* cuda_context_handle; /* Pointer to a `CUcontext`, e.g. use `tra_cuda_api.context_get_handle()`. */
  void* cuda_device_handle;  /* Pointer to a CUdevice, e.g. use `tra_cuda_api.device_get_handle()`.  */
};

/* ------------------------------------------------------- */

TRA_LIB_DLL int tra_nvdec_create(tra_decoder_settings* cfg, tra_nvdec_settings* settings, tra_nvdec** ctx);
TRA_LIB_DLL int tra_nvdec_destroy(tra_nvdec* ctx);
TRA_LIB_DLL int tra_nvdec_decode(tra_nvdec* ctx, uint32_t type, void* data);

/* ------------------------------------------------------- */

#endif
