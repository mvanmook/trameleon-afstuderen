/* ------------------------------------------------------- */

#include <cuda.h>
#include <tra/modules/nvidia/nvidia-cuda.h>
#include <tra/registry.h>
#include <tra/log.h>

/* ------------------------------------------------------- */

struct tra_cuda_device {
  CUdevice handle;
};

struct tra_cuda_context {
  CUcontext handle;
};

/* ------------------------------------------------------- */

static int cuda_init(); 
static int cuda_device_list(); /* Prints a list with available NVIDIA devices. */
static int cuda_device_create(int num, tra_cuda_device** ctx);
static int cuda_device_destroy(tra_cuda_device* ctx);
static int cuda_device_get_handle(tra_cuda_device* device, void** handle);
static int cuda_context_create(tra_cuda_device* device, int flags, tra_cuda_context** result);
static int cuda_context_destroy(tra_cuda_context* ctx);
static int cuda_context_get_handle(tra_cuda_context* ctx, void** handle);

/* ------------------------------------------------------- */

static tra_cuda_api cuda_api;

/* ------------------------------------------------------- */

static int cuda_init() {
  
  CUresult result = CUDA_SUCCESS;

  result = cuInit(0);
  if (CUDA_SUCCESS != result) {
    TRAE("Failed to initialize CUDA");
    return -1;
  }
  
  return 0;
}

/* ------------------------------------------------------- */

static int cuda_device_list() {

  const char* err_str = NULL;
  CUresult result = CUDA_SUCCESS;
  CUdevice device = 0;
  char name[256] = { 0 };
  int num_devices = 0;
  int i = 0;

  result = cuDeviceGetCount(&num_devices);
  if (CUDA_SUCCESS != result) {
    cuGetErrorString(result, &err_str);
    TRAE("Cannot list CUDA devices, failed to get the number of cuda devices: %s.", err_str);
    return -1;
  }

  for (i = 0; i < num_devices; ++i) {

    result = cuDeviceGet(&device, i);
    if (CUDA_SUCCESS != result) {
      cuGetErrorString(result, &err_str);
      TRAE("Cannot list CUDA devices, failed to access a CUDA device for index %d: %s.", i, err_str);
      continue;
    }

    result = cuDeviceGetName(name, sizeof(name), device);
    if (CUDA_SUCCESS != result) {
      cuGetErrorString(result, &err_str);
      TRAE("Failed list CUDA devices, failed to get the CUDA device name for index %d: %s.", i, err_str);
      continue;
    }

    TRAD("Cuda device %d: %s", i, name);
  }
  
  return 0;
}

/* ------------------------------------------------------- */

static int cuda_device_create(int num, tra_cuda_device** ctx) {
  
  CUresult result = CUDA_SUCCESS;
  tra_cuda_device* inst = NULL;
  const char* err_str = NULL;
  int r = 0;
  
  if (NULL == ctx) {
    TRAE("Cannot create device, given `tra_cuda_device**` is NULL.");
    r = -10;
    goto error;
  }

  if (NULL != (*ctx)) {
    TRAE("Cannot create the device, given `*(tra_cuda_device**)` is not NULL. Already created or not initialized to NULL?");
    r = -20;
    goto error;
  }

  inst = calloc(1, sizeof(tra_cuda_device));
  if (NULL == inst) {
    TRAE("Cannot create the device, failed to allocate. Out of memory?");
    r = -30;
    goto error;
  }

  result = cuDeviceGet(&inst->handle, num);
  if (CUDA_SUCCESS != result) {
    cuGetErrorString(result, &err_str);
    TRAE("Failed to access a CUDA device for index %d: %s.", num, err_str);
    r = -40;
    goto error;
  }

  TRAD("tra_cuda_device->handle: %p", &inst->handle);
  
  /* Assign the result */
  *ctx = inst;
  
 error:

  if (r < 0) {
    
    if (NULL != inst) {
      cuda_device_destroy(inst);
      inst = NULL;
    }

    if (NULL != ctx) {
      *ctx = NULL;
    }
  }
  
  return r;
}

/* ------------------------------------------------------- */

static int cuda_device_destroy(tra_cuda_device* ctx) {

  if (NULL == ctx) {
    TRAE("Cannot destroy the given `tra_cuda_device*` as it's NULL.");
    return -1;
  }

  ctx->handle = 0;
  
  free(ctx);
  ctx = NULL;

  return 0;
}

/* ------------------------------------------------------- */

static int cuda_context_create(tra_cuda_device* device, int flags, tra_cuda_context** ctx) {  

  tra_cuda_context* inst = NULL;
  CUresult result = CUDA_SUCCESS;
  const char* err_str = NULL;
  int r = 0;

  if (NULL == device) {
    TRAE("Cannot create the cuda context, the given `tra_cuda_device*` is NULL.");
    r = -10;
    goto error;
  }

  if (NULL == ctx) {
    TRAE("Cannot create the cuda context, the given output `tra_cuda_context**` is NULL.");
    r = -20;
    goto error;
  }

  if (NULL != (*ctx)) {
    TRAE("Cannot create the cuda context, the given `*(tra_cuda_context**)` is not NULL. Already created? Not iniitallized to NULL?");
    r = -30;
    goto error;
  }


  inst = calloc(1, sizeof(tra_cuda_context));
  if (NULL == inst) {
    TRAE("Cannot create the cuda context, failed to allocate. Out of memory?");
    r = -40;
    goto error;
  }
  
  result = cuCtxCreate(
    &inst->handle,
    flags,
    device->handle
  );
  
  if (CUDA_SUCCESS != result) {
    cuGetErrorString(result, &err_str);
    TRAE("Failed to create the cuda context: %s", err_str);
    r = -50;
    goto error;
  }

  /* Assign the result. */
  *ctx = inst;

 error:
  
  if (r < 0) {

    if (NULL != inst) {
      cuda_context_destroy(inst);
      inst = NULL;
    }

    if (NULL != ctx) {
      *ctx = NULL;
    }
  }

  return r;
}

/* ------------------------------------------------------- */

static int cuda_context_destroy(tra_cuda_context* ctx) {

  CUresult result = CUDA_SUCCESS;
  const char* err_str = NULL;
  int status = 0;
  int r = 0;

  if (NULL == ctx) {
    TRAE("Cannot destroy the cuda context, the given context is NULL.");
    return -10;
  }
  
  result = cuCtxDestroy(ctx->handle);
  if (CUDA_SUCCESS != result) {
    cuGetErrorString(result, &err_str);
    TRAE("Failed to cleanly destroy the CUDA context: %s", err_str);
    status -= 20;
  }

  ctx->handle = 0;

  free(ctx);
  ctx = NULL;

  return status;
}

/* ------------------------------------------------------- */

static int cuda_device_get_handle(tra_cuda_device* device, void** handle) {
  
  if (NULL == device) {
    TRAE("Cannot get cuda device handle as the given `tra_cuda_device*` is NULL.");
    return -10;
  }

  if (NULL == handle) {
    TRAE("Cannot get cuda device handle as the given output `void**` is NULL.");
    return -20;
  }
  
  *handle = (void*) &device->handle;

  return 0;
}

/* ------------------------------------------------------- */
  
static int cuda_context_get_handle(tra_cuda_context* ctx, void** handle) {

  if (NULL == ctx) {
    TRAE("Cannot get cuda context handle as the given `tra_cuda-context` is NULL.");
    return -10;
  }

  if (NULL == handle) {
    TRAE("Cannot get the cuda context handle as the given output `void**` is NULL.");
    return -20;
  }

  *handle = (void*) &ctx->handle;
  
  return 0;
}

/* ------------------------------------------------------- */

int tra_load(tra_registry* reg) {

  int r = 0;

  if (NULL == reg) {
    TRAE("Cannot load the cuda api as the given `tra_registry*` is NULL.");
    return -10;
  }

  r = tra_registry_add_api(reg, "cuda", &cuda_api);
  if (r < 0) {
    TRAE("Failed to register the `cuda` api.");
    return -20;
  }

  TRAI("Registered the `cuda` api.");

  return 0;
}

/* ------------------------------------------------------- */

static tra_cuda_api cuda_api = {
  .init = cuda_init,
  .device_list = cuda_device_list,
  .device_create = cuda_device_create,
  .device_destroy = cuda_device_destroy,
  .device_get_handle = cuda_device_get_handle,
  .context_create = cuda_context_create, 
  .context_destroy = cuda_context_destroy,
  .context_get_handle = cuda_context_get_handle, 
};

/* ------------------------------------------------------- */
