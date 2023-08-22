#ifndef TRA_EASY_NVIDIA_H
#define TRA_EASY_NVIDIA_H

/*

  quicknote: as the nvidia easy layer need a cuda context and
  api, we create one `tra_easy_nvapp` that takes care of
  allocating and initializing the required instances. These
  required instances are used by the nvidia easy layer.
  
 */
/* ------------------------------------------------------- */

typedef struct tra_easy_nvapp_settings tra_easy_nvapp_settings;
typedef struct tra_easy_nvapp          tra_easy_nvapp;
typedef struct tra_easy                tra_easy;
typedef struct tra_cuda_context        tra_cuda_context;
typedef struct tra_cuda_device         tra_cuda_device;
typedef struct tra_cuda_api            tra_cuda_api;

/* ------------------------------------------------------- */

struct tra_easy_nvapp_settings {
  tra_easy* easy_ctx;
};

/* ------------------------------------------------------- */

struct tra_easy_nvapp {
  tra_cuda_context* cuda_context;
  tra_cuda_device* cuda_device;
  tra_cuda_api* cuda_api;
  void* cuda_device_handle;
  void* cuda_context_handle;
};

/* ------------------------------------------------------- */

int tra_easy_nvapp_ensure(tra_easy_nvapp_settings* cfg, tra_easy_nvapp** ctx); /* This will allocate the `tra_nvapp` when is hasn't been allocated yet. We make sure that only one instance will be created. */

/* ------------------------------------------------------- */

#endif
