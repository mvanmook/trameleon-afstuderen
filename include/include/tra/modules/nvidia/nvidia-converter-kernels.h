#ifndef TRA_NVIDIA_CONVERTER_KERNELS_H
#define TRA_NVIDIA_CONVERTER_KERNELS_H
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

  SCALER KERNEL(S)
  ================

  GENERAL INFO:

    This file contains the scaler functions that are used by the
    `nvidia-scaler.c` source to resize images. The implementation
    makes use of CUDA and needs the CUDA compiler. See
    `nvidia-scaler-kernel.cu` for the implementation.
  
 */

/* ------------------------------------------------------- */

#include <stdint.h>

/* ------------------------------------------------------- */

#if defined(__cplusplus)
  extern "C" {
#endif

/* ------------------------------------------------------- */

int tra_cuda_resize_nv12(
  void* inputDevicePtr,          /* The CUDA pointer to the device memory which you want to resize. */
  uint32_t inputWidth,           /* The width of the input image. */
  uint32_t inputHeight,          /* The height of the input image. */
  uint32_t inputPitch,           /* The pitch of the input image memory buffer. */
  void* outputDevicePtr,         /* The CUDA pointer where we should store the resized buffer. It's up to you to make sure the buffer is large enough to hold the resized buffer. */
  uint32_t outputWidth,          /* The width of the output image. */
  uint32_t outputHeight,         /* The height of the output image. */
  uint32_t outputPitch           /* The pitch of the output buffer. */
);

/* ------------------------------------------------------- */

#if defined(__cplusplus)
  }
#endif

/* ------------------------------------------------------- */

#endif
