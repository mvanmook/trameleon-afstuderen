/* ------------------------------------------------------- */

#include <stdlib.h>
#include <cuda.h>
#include <tra/modules/nvidia/nvEncodeAPI.h>
#include <tra/modules/nvidia/nvidia-enc-cuda.h>
#include <tra/modules/nvidia/nvidia-enc.h>
#include <tra/modules/nvidia/nvidia.h>
#include <tra/registry.h>
#include <tra/module.h>
#include <tra/types.h>
#include <tra/log.h>

/* ------------------------------------------------------- */

static tra_encoder_api encoder_api;

/* ------------------------------------------------------- */

static const char* encoder_get_name();
static const char* encoder_get_author();
static int encoder_create(tra_encoder_settings* cfg, void* settings, tra_encoder_object** obj);
static int encoder_destroy(tra_encoder_object* obj);
static int encoder_encode(tra_encoder_object* obj, tra_sample* sample, uint32_t type, void* data);
static int encoder_flush(tra_encoder_object* obj);

/* ------------------------------------------------------- */

struct tra_nvenccuda {
  tra_nvenc* encoder;
  tra_nvenc_resources* resources; /* This context manages the input buffers and registers them with the encoder. See header for more info. */
};

/* ------------------------------------------------------- */

int tra_nvenccuda_create(
  tra_encoder_settings* cfg,
  tra_nvenc_settings* settings,
  tra_nvenccuda** ctx
)
{
  tra_nvenc_resources_settings res_cfg = { 0 };
  tra_nvenccuda* inst = NULL;
  int r = 0;

  if (NULL == ctx) {
    TRAE("Cannot create the `tra_nvenccuda` as the given `tra_nvenccuda**` is NULL.");
    r = -10;
    goto error;
  }

  if (NULL != *ctx) {
    TRAE("Cannot create the `tra_nvenccuda` as the given `*tra_nvenccuda**` is not NULL. Already created? Not initialized to NULL?");
    r = -20;
    goto error;
  }

  inst = calloc(1, sizeof(tra_nvenccuda));
  if (NULL == inst) {
    TRAE("Cannot create the `tra_nvenccuda` as we failed to allocate the context.");
    r = -30;
    goto error;
  }

  settings->device_type = NV_ENC_DEVICE_TYPE_CUDA;

  r = tra_nvenc_create(cfg, settings, &inst->encoder);
  if (r < 0) {
    TRAE("Cannot create the `tra_nvenccuda` as we failed to create the `tra_nvenc` instance.");
    r = -40;
    goto error;
  }

  /* Create resources manager which we use to register incomning cuda buffers with the encoder. */
  res_cfg.enc = inst->encoder;

  r = tra_nvenc_resources_create(&res_cfg, &inst->resources);
  if (r < 0) {
    TRAE("Cannot create the `tra_nvenccuda` as we failed to create the resources manager.");
    r = -50;
    goto error;
  }

  /* Finally assign */
  *ctx = inst;
  
 error:

  if (r < 0) {
    
    if (NULL != inst) {
      tra_nvenccuda_destroy(inst);
      inst = NULL;
    }

    if (NULL != ctx) {
      *ctx = NULL;
    }
  }
  
  return r;
}

/* ------------------------------------------------------- */

int tra_nvenccuda_destroy(tra_nvenccuda* ctx) {

  int result = 0;
  int r = 0;

  TRAE("@todo Similar as the OpenGL based encoder, we have to follows a specific order when we're deallocating resources. E.g. we first must unregister the resources, then deallocate these resources, then cleanup/shutdown the cuda context, etc.");

  if (NULL == ctx) {
    TRAE("Cannot destroy the `tra_nvenccuda*` as it's NULL.");
    result -= 10;
    goto error;
  }

  /* Destroy the resources context that we've used to register CUDA buffers. */
  if (NULL != ctx->resources) {
    r = tra_nvenc_resources_destroy(ctx->resources);
    if (r < 0) {
      TRAE("Failed to cleanly destroy the `tra_nvenc_resources` instance.");
      result -= 20;
    }
  }

  /* Destroy the encoder instance. */
  if (NULL != ctx->encoder) {
    r = tra_nvenc_destroy(ctx->encoder);
    if (r < 0) {
      TRAE("Failed to cleanly destroy the `tra_nvenc` instance.");
      result -= 30;
    }
  }
  
  ctx->resources = NULL;
  ctx->encoder = NULL;

 error:
  return result;
}

/* ------------------------------------------------------- */

int tra_nvenccuda_encode(
  tra_nvenccuda* ctx,
  tra_sample* sample,
  uint32_t type,
  void* data
)
{
  tra_nvenc_registration* registration = NULL;
  tra_nvenc_resource resource = { 0 };
  tra_memory_cuda* cuda_mem = NULL;
  int r = 0;

  if (NULL == ctx) {
    TRAE("Cannot encode as the given `tra_nvenccuda*` is NULL.");
    r = -10;
    goto error;
  }

  if (NULL == ctx->resources) {
    TRAE("Cannot encode as the `tra_nvenccuda::resources` member is NULL.");
    r = -20;
    goto error;
  }

  if (NULL == data) {
    TRAE("Cannot encode as the given data pointer is NULL.");
    r = -30;
    goto error;
  }

  if (TRA_MEMORY_TYPE_CUDA != type) {
    TRAE("Cannot encode as we expect the given data to hold a `TRA_MEMORY_TYPE_CUDA` but the given types indicates something else.");
    r = -40;
    goto error;
  }

  cuda_mem = (tra_memory_cuda*) data;
  
  if (0 == cuda_mem->stride) {
    TRAE("Cannot encode as the `stride` of the given `tra_memory_cuda` is 0.");
    r = -50;
    goto error;
  }

  if (0 == cuda_mem->ptr) {
    TRAE("Cannot encode as the `ptr` member of the given `cuda_mem` is NULL.");
    r = -60;
    goto error;
  }

  /*
    Here we make sure that given resource is registered with the
    encoder. `registration` will be set to the registration point
    that we need to use when encoding.
  */
  resource.resource_type = NV_ENC_INPUT_RESOURCE_TYPE_CUDADEVICEPTR;
  resource.resource_ptr = (void*) cuda_mem->ptr;
  resource.resource_stride = cuda_mem->stride;
    
  r = tra_nvenc_resources_ensure_registration(ctx->resources, &resource, &registration);
  if (r < 0) {
    TRAE("Failed to register our resource!");
    r = -80;
    goto error;
  }

  /*
    The `..._ensure_registration()` function will register new
    external buffers (the cuda pointers we get) or find the
    previsous registration point for the given input buffer.
    When we have a registration we can encode.
   */
  r = tra_nvenc_encode_with_registration(ctx->encoder, sample, registration);
  if (r < 0) {
    TRAE("Failed to encode the given input buffer.");
    r = -90;
    goto error;
  }
  
 error:
  return r;
}

/* ------------------------------------------------------- */

int tra_nvenccuda_flush(tra_nvenccuda* ctx) {
  
  int r = 0;

  if (NULL == ctx) {
    TRAE("Cannot flush the encoder as the given `tra_nvenccuda*` is NULL.");
    r = -10;
    goto error;
  }

  if (NULL == ctx->encoder) {
    TRAE("Cannot flush the encoder as the `encoder` member of `tra_nvenccuda` is NULL.");
    r = -20;
    goto error;
  }

  r = tra_nvenc_flush(ctx->encoder);
  if (r < 0) {
    TRAE("An error occured while trying to flush the encoder.");
    r = -20;
    goto error;
  }

 error:
  return r;
}

/* ------------------------------------------------------- */

static const char* encoder_get_name() {
  return "nvenccuda";
}

static const char* encoder_get_author() {
  return "roxlu";
}

static int encoder_create(tra_encoder_settings* cfg, void* settings, tra_encoder_object** obj) {
  return tra_nvenccuda_create(cfg, settings, (tra_nvenccuda**) obj);
}

static int encoder_destroy(tra_encoder_object* obj) {
  return tra_nvenccuda_destroy((tra_nvenccuda*) obj);
}

static int encoder_encode(tra_encoder_object* obj, tra_sample* sample, uint32_t type, void* data) {
  return tra_nvenccuda_encode((tra_nvenccuda*) obj, sample, type, data);
}

static int encoder_flush(tra_encoder_object* obj) {
  return tra_nvenccuda_flush((tra_nvenccuda*) obj);
}

/* ------------------------------------------------------- */

int tra_load(tra_registry* reg) {

  int r = 0;

  if (NULL == reg) {
    TRAE("Cannot register the `nvenccuda` encoder as the given `tra_registry*` is NULL.");
    return -10;
  }

  r = tra_registry_add_encoder_api(reg, &encoder_api);
  if (r < 0) {
    TRAE("Failed to register the `nvenccuda` encoder.");
    return -20;
  }

  return r;
}

/* ------------------------------------------------------- */

static tra_encoder_api encoder_api = {
  .get_name = encoder_get_name,
  .get_author = encoder_get_author,
  .create = encoder_create,
  .destroy = encoder_destroy,
  .encode = encoder_encode,
  .flush = encoder_flush,
};

/* ------------------------------------------------------- */
