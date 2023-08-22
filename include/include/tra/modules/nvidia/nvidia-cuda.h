#ifndef TRA_NVIDIA_CUDA_H
#define TRA_NVIDIA_CUDA_H

/* ------------------------------------------------------- */

typedef struct tra_cuda_device tra_cuda_device;   /* Opaque type that represents `CUdevice` */
typedef struct tra_cuda_context tra_cuda_context; /* Opaque type that represents `CUcontext` */
typedef struct tra_cuda_api tra_cuda_api;

/* ------------------------------------------------------- */

struct tra_cuda_api {
  int (*init)(); /* Initialize Cuda */
  int (*device_list)(); /* List available devices */
  int (*device_create)(int num, tra_cuda_device** ctx);
  int (*device_destroy)(tra_cuda_device* ctx);
  int (*device_get_handle)(tra_cuda_device* ctx, void** handle);
  int (*context_create)(tra_cuda_device* device, int flags, tra_cuda_context** ctx);
  int (*context_destroy)(tra_cuda_context* ctx);
  int (*context_get_handle)(tra_cuda_context* ctx, void** handle);
};

/* ------------------------------------------------------- */

#endif
