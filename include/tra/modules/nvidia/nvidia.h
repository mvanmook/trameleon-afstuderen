#ifndef TRA_MODULE_NVIDIA_H
#define TRA_MODULE_NVIDIA_H

/* ------------------------------------------------------- */

#include <cuda.h>

/* ------------------------------------------------------- */

typedef struct tra_memory_cuda         tra_memory_cuda;
typedef struct tra_memory_opengl_nv12  tra_memory_opengl_nv12;

/* ------------------------------------------------------- */

/*
  When the user creates a decoder and requests the decoder to
  keep the decoded data on the GPU, we use this type to pass the
  decoded data from the NVIDIA decoder into the NVIDIA
  encoder. See `nvidia-dec.c` where this is used.
*/

struct tra_memory_cuda {
  CUdeviceptr ptr;  /* Device pointer to the memory that holds e.g. a decoded frame. */
  uint32_t stride;  /* The pitch of the data; required when we feed this into the encoder and resizer. */  
};

/* ------------------------------------------------------- */

struct tra_memory_opengl_nv12 {
  uint32_t tex_id;
  uint32_t tex_stride;
};

/* ------------------------------------------------------- */

#endif

