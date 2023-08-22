/* ------------------------------------------------------- */

#include <stdlib.h>
#include <cuda.h>
#include <tra/modules/nvidia/nvidia-enc-host.h>
#include <tra/modules/nvidia/nvidia-enc.h>
#include <tra/modules/nvidia/nvidia-easy.h>
#include <tra/modules/nvidia/nvEncodeAPI.h>
#include <tra/registry.h>
#include <tra/module.h>
#include <tra/types.h>
#include <tra/easy.h>
#include <tra/log.h>

/* ------------------------------------------------------- */

static tra_easy_api easy_encoder_api;
static tra_encoder_api encoder_api;

/* ------------------------------------------------------- */

struct tra_nvenchost {
  tra_nvenc* encoder;             /* The `tra_nvenc` does "all" the actual encoding work. The `tra_nvenchost` is a thin wrapper which manages external resources. */
  tra_nvenc_resources* resources; /* Manages and registers the device buffers that we allocate. These device buffers are used to transfer host memory into device memory. */

  /* Input */
  CUdeviceptr* input_buffers;     /* The CUDA buffers that we allocate; we keep a handle so we can deallocate them when shutting down. */
  uint32_t input_count;           /* The number of CUDA buffers that we've allocated. */
  uint32_t input_stride;          /* The pitch of the CUDA buffers that we allocate. */
  uint32_t input_format;
  uint32_t input_width;
  uint32_t input_height;
};

/* ------------------------------------------------------- */

static const char* encoder_get_name();
static const char* encoder_get_author();
static int encoder_create(tra_encoder_settings* cfg, void* settings, tra_encoder_object** obj);
static int encoder_destroy(tra_encoder_object* obj);
static int encoder_encode(tra_encoder_object* obj, tra_sample* sample, uint32_t type, void* data);
static int encoder_flush(tra_encoder_object* obj);

/* ------------------------------------------------------- */

static int easy_encoder_create(tra_easy* ez, tra_encoder_settings* cfg, void** enc);
static int easy_encoder_encode(void* enc, tra_sample* sample, uint32_t type, void* data);
static int easy_encoder_flush(void* enc);
 static int easy_encoder_destroy(void* enc);

/* ------------------------------------------------------- */

int tra_nvenchost_create(
  tra_encoder_settings* cfg,
  tra_nvenc_settings* settings,
  tra_nvenchost** ctx
)
{
  tra_nvenc_registration* registration = NULL;
  tra_nvenc_resources_settings res_cfg = { 0 };
  tra_nvenc_resource resource = { 0 };
  tra_nvenchost* inst = NULL;
  CUresult result = CUDA_SUCCESS;
  CUdeviceptr buffer_ptr = 0;
  uint32_t buffer_count = 0;
  size_t buffer_stride = 0;
  uint32_t i = 0;
  int r = 0;

  if (NULL == ctx) {
    TRAE("Cannot create the `tra_nvenchost` as the given `tra_nvenchost**` is NULL.");
    r = -10;
    goto error;
  }

  if (NULL != *ctx) {
    TRAE("Cannot create the `tra_nvenchost` as the given `*tra_nvenchost**` is NULL.");
    r = -20;
    goto error;
  }

  if (NULL == cfg) {
    TRAE("Cannot create the `tra_nvenchost` as the given `tra_nvenc_settings*` is NULL.");
    r = -30;
    goto error;
  }

  if (0 == cfg->image_width) {
    TRAE("Cannot create the `tra_nvenchost` as the given `tra_nvenc_settings::image_width` is 0.");
    r = -40;
    goto error;
  }

  if (0 == cfg->image_height) {
    TRAE("Cannot create the `tra_nvenchost` as the given `tra_nvenc_settings::image_height` is 0.");
    r = -50;
    goto error;
  }

  /* Currently we only support NV12 input buffers. */
  if (TRA_IMAGE_FORMAT_NV12 != cfg->image_format) {
    TRAE("Cannot create the `tra_nvenvhost` as the given `tra_nvenc_settings::image_format` is not supported (yet).");
    r = -60;
    goto error;
  }

  /* Create `tra_nvenc` encoder context. */
  inst = calloc(1, sizeof(tra_nvenchost));
  if (NULL == inst) {
    TRAE("Cannot create the `tra_nvenchost` as we failed to allocate the context. Out of memory?");
    r = -30;
    goto error;
  }

  /*
    As we're copying the host buffers into CUDA device buffers
    we've to inform the encoder that we're going to use a CUDA
    device.
  */
  settings->device_type = NV_ENC_DEVICE_TYPE_CUDA;

  r = tra_nvenc_create(cfg, settings, &inst->encoder);
  if (r < 0) {
    TRAE("Cannot create the `tra_nvenchost` as we failed to create the `tra_nvenc` context.");
    r = -40;
    goto error;
  }

  /* Create resource manager which manages the external input buffers that we encode. */
  res_cfg.enc = inst->encoder;

  r = tra_nvenc_resources_create(&res_cfg, &inst->resources);
  if (r < 0) {
    TRAE("Cannot create the `tra_nvenchost` as we failed ot create the `tra_nvenc_resources` context that is used to handle input buffers.");
    r = -50;
    goto error;
  }

  /* Create the CUDA buffers into which we copy the host buffers that we want to encode. */
  r = tra_nvenc_get_input_buffer_count(inst->encoder, &buffer_count);
  if (r < 0) {
    TRAE("Cannot create the `tra_nvenchost` as we failed to get the input buffer count.");
    r = -60;
    goto error;
  }

  inst->input_buffers = calloc(buffer_count, sizeof(CUdeviceptr));
  if (NULL == inst->input_buffers) {
    TRAE("Cannot create the `tra_nvenchost` as we failed to allocate our buffer to holds the pointers to CUDA device memory.");
    r = -70;
    goto error;
  }

  for (i = 0; i < buffer_count; ++i) {
    
    result = cuMemAllocPitch(
      &buffer_ptr,                                   /* out: pointer to the allocated device buffer. */
      &buffer_stride,                                /* out: pitch of the allocated buffer. */
      cfg->image_width,                              /* width in bytes. @todo this is currently only correct for NV12 */
      cfg->image_height + (cfg->image_height * 0.5), /* height to of buffer */
      16                                             /* @todo: should we use a different value here? */
    );

    if (CUDA_SUCCESS != result) {
      TRAE("Failed to allocate an input buffer.");
      r = -70;
      goto error;
    }

    /* Register this resource. */
    resource.resource_type = NV_ENC_INPUT_RESOURCE_TYPE_CUDADEVICEPTR;
    resource.resource_ptr = buffer_ptr;
    resource.resource_stride = buffer_stride;
    
    r = tra_nvenc_resources_ensure_registration(inst->resources, &resource, &registration);
    if (r < 0) {
      TRAE("Failed to register our resource!");
      r = -80;
      goto error;
    }

    /* Keep track of the allocations */
    inst->input_buffers[i] = buffer_ptr;
  }

  /*
    We remember the image settings that we accept; see the encode
    function where we use them to make sure we recieve the
    correct data.
  */
  inst->input_count = buffer_count;
  inst->input_stride = buffer_stride;
  inst->input_width = cfg->image_width;
  inst->input_height = cfg->image_height;
  inst->input_format = cfg->image_format;
  
  /* Finally assign */
  *ctx = inst;

 error:

  if (r < 0) {

    if (NULL != inst) {
      tra_nvenchost_destroy(inst);
      inst = NULL;
    }

    if (NULL != ctx) {
      *ctx = NULL;
    }
  }

  return r;
}

/* ------------------------------------------------------- */

int tra_nvenchost_destroy(tra_nvenchost* ctx) {

  CUresult status = CUDA_SUCCESS;
  uint32_t i = 0;
  int result = 0;
  int r = 0;

  if (NULL == ctx) {
    TRAE("Cannot destroy the given `tra_nvenchost*` as it's NULL.");
    result = -10;
    goto error;
  }

  /* Before we can deallocate the buffers we've allocate we have to unregister them first. */
  if (NULL != ctx->resources) {
    
    r = tra_nvenc_resources_destroy(ctx->resources);
    if (r < 0) {
      TRAE("Failed to cleanly destroy the resources.");
      result -= 20;
    }
  }

  /* Now we can deallocate the buffers */
  if (NULL != ctx->input_buffers) {
    
    for (i = 0; i < ctx->input_count; ++i) {
      
      status = cuMemFree(ctx->input_buffers[i]);
      if (CUDA_SUCCESS != status) {
        TRAE("Failed to cleanly free the CUDA buffer.");
        result -= 30;
      }
    }

    free(ctx->input_buffers);
  }

  if (NULL != ctx->encoder) {
    r = tra_nvenc_destroy(ctx->encoder);
    if (r < 0) {
      TRAE("Failed to cleanly destroy the `tra_nvenc` instance.");
      result -= 40;
    }
  }
  
  ctx->encoder = NULL;
  ctx->resources = NULL;
  ctx->input_buffers = NULL;
  ctx->input_count = 0;
  ctx->input_stride = 0;
  ctx->input_format = 0;
  ctx->input_width = 0;
  ctx->input_height = 0;
  
  free(ctx);
  ctx = NULL;

 error:

  return r;
}

/* ------------------------------------------------------- */

/*
  
  GENERAL INFO:
  
    This function will accept a `TRA_MEMORY_TYPE_IMAGE` and
    upload it to the device. Once uploaded it can be encoded.
    We first copy the luma plane after which we copy the chroma
    plane; currently only supporting NV12.

    I'm applying -a lot- of verbose validation here. Besides I
    think it's good practice, setting up a `tra_memory_image`
    with the correct strides, plane pointers, heights and widths
    is often a source of errors. We make sure that the given
    image has been correctly setup and if not we will log an
    error.

  TODO:

    - Check if my usage of `cuMemcpy2D()` is correct when copying the luma plane.
    - It seems to be an "aligned" copy; I've to research what kind of copies we can make.
    - Look into asynchronous copies.
    - Handle different chroma formats.
    - Do we need to push/pop the context?
    
 */
int tra_nvenchost_encode(
  tra_nvenchost* ctx,
  tra_sample* sample,
  uint32_t type,
  void* data
)
{
  tra_nvenc_registration* reg = NULL;
  tra_memory_image* src_image = NULL;
  CUresult result = CUDA_SUCCESS;
  CUDA_MEMCPY2D mem = { 0 };
  void* buffer_ptr = NULL;
  int r = 0;
  
  /* ----------------------------------------------- */
  /* Validate                                        */
  /* ----------------------------------------------- */
  
  if (NULL == ctx) {
    TRAE("Cannot encode as the given `tra_nvenchost*` is NULL.");
    r = -10;
    goto error;
  }

  if (NULL == ctx->resources) {
    TRAE("Cannot encode as the given `tra_nvenchost::resources` is NULL.");
    r = -20;
    goto error;
  }

  if (0 == ctx->input_stride) {
    TRAE("Cannot encode as the given `tra_nvenchost::input_stride` is 0.");
    r = -30;
    goto error;
  }

  if (0 == ctx->input_height) {
    TRAE("Cannot encode as the given `tra_nvenchost::input_height` is 0.");
    r = -40;
    goto error;
  }

  if (0 == ctx->input_width) {
    TRAE("Cannot encode as the given `tra_nvenchost::input_width` is 0.");
    r = -50;
    goto error;
  }

  if (TRA_MEMORY_TYPE_IMAGE != type) {
    TRAE("Cannot encode as the given memory type is invalid; we only accept `TRA_MEMORY_TYPE_IMAGE` for now. ");
    r = -30;
    goto error;
  }

  src_image = (tra_memory_image*) data;

  if (src_image->image_width != ctx->input_width) {
    TRAE("Cannot encode as the given input image has a different `image_width` than we've prepared for.");
    r = -40;
    goto error;
  }

  if (src_image->image_height != ctx->input_height) {
    TRAE("Cannot encode as the given input image has a different `image_height` than we've prepared for.");
    r = -50;
    goto error;
  }

  if (src_image->image_format != ctx->input_format) {
    TRAE("Cannot encode as the given input image has a different `image_format` than we've prepared for.");
    r = -60;
    goto error;
  }

  if (0 == src_image->plane_strides[0]) {
    TRAE("Cannot encode as the given input image has its `plane_strides[0]` not set. We need this to copy the image from host to device memory.");
    r = -70;
    goto error;
  }

  if (0 == src_image->plane_strides[1]) {
    TRAE("Cannot encode as the given input image has its `plane_strides[1]` not set. We need this to copy the image from host to device memory.");
    r = -80;
    goto error;
  }

  if (NULL == src_image->plane_data[0]) {
    TRAE("Cannot encode as the given input image has its `plane_data[0]` not set.");
    r = -80;
    goto error;
  }
  
  if (NULL == src_image->plane_data[1]) {
    TRAE("Cannot encode as the given input image has its `plane_data[1]` not set.");
    r = -80;
    goto error;
  }

  if (0 == src_image->plane_heights[0]) {
    TRAE("Cannot encode as the given input image has its `plane_heights[0]` not set.");
    r = -80;
    goto error;
  }

  if (0 == src_image->plane_heights[1]) {
    TRAE("Cannot encode as the given input image has its `plane_heights[1]` not set.");
    r = -80;
    goto error;
  }

  /* ----------------------------------------------- */
  /* Copy source image into device buffer.           */
  /* ----------------------------------------------- */

  r = tra_nvenc_resources_get_usable_registration(ctx->resources, &reg, &buffer_ptr);
  if (r < 0) {
    TRAE("Cannot encode as we failed to retrieve a registration entry. We need to this entry to copy the given image into (e.g. transfer from host to device). ");
    r = -70;
    goto error;
  }
  
  /* Copy the LUMA data into he buffer. */
  mem.srcMemoryType = CU_MEMORYTYPE_HOST; 
  mem.srcPitch = src_image->plane_strides[0];
  mem.srcDevice = NULL; 
  mem.srcHost = src_image->plane_data[0];
  mem.dstMemoryType = CU_MEMORYTYPE_DEVICE; /* We copy from CPU to GPU */
  mem.dstDevice = buffer_ptr;               /* The pointer to the destination buffer into which we copy the given image. */
  mem.dstPitch = ctx->input_stride; 
  mem.WidthInBytes = ctx->input_width;      /* @todo this is currently valid for NV12, but we might want to change it for other types.  */
  mem.Height = src_image->plane_heights[0];

  result = cuMemcpy2D(&mem);
  if (CUDA_SUCCESS != result) {
    TRAE("Failed to copy the luma data into the buffer.");
    r = -100;
    goto error;
  }

  /* Copy the CHROMA data into the buffer. */
  mem.srcHost = src_image->plane_data[1];
  mem.srcPitch = src_image->plane_strides[1];
  mem.dstDevice = (CUdeviceptr) ((uint8_t*)buffer_ptr) + ctx->input_height * ctx->input_stride; 
  mem.dstPitch = ctx->input_stride;
  mem.WidthInBytes = ctx->input_width; /* @todo this is currently valid for NV12. When we add support for different formats we have to check this. */
  mem.Height = src_image->plane_heights[1];

  result = cuMemcpy2D(&mem);
  if (CUDA_SUCCESS != result) {
    TRAE("Failed to copy the chroma data into the buffer.");
    r = -110;
    goto error;
  }

  /* ----------------------------------------------- */
  /* Finally encode                                  */
  /* ----------------------------------------------- */

  r = tra_nvenc_encode_with_registration(ctx->encoder, sample, reg);
  if (r < 0) {
    TRAE("Failed to encode with a registration.");
    r = -120;
    goto error;
  }

 error:
  return r;
}

/* ------------------------------------------------------- */

int tra_nvenchost_flush(tra_nvenchost* ctx) {

  int r = 0;

  if (NULL == ctx) {
    TRAE("Cannot flush the encoder as the given `tra_nvenchost*` is NULL.");
    r = -10;
    goto error;
  }

  if (NULL == ctx->encoder) {
    TRAE("Cannot flush the encodder as the `encoder` member of `tra_nvenchost` is NULL.");
    r = -20;
    goto error;
  }

  r = tra_nvenc_flush(ctx->encoder);

  if (r < 0) {
    TRAE("An error occured while trying to flush the encoder.");
    r = -30;
    goto error;
  }

 error:
  return r;
}

/* ------------------------------------------------------- */

static const char* encoder_get_name() {
  return "nvenchost";
}

static const char* encoder_get_author() {
  return "roxlu";
}

static int encoder_create(tra_encoder_settings* cfg, void* settings, tra_encoder_object** obj) {
  return tra_nvenchost_create(cfg, settings, (tra_nvenchost**) obj);
}

static int encoder_destroy(tra_encoder_object* obj) {
  return tra_nvenchost_destroy((tra_nvenchost*) obj);
}

static int encoder_encode(tra_encoder_object* obj, tra_sample* sample, uint32_t type, void* data) {
  return tra_nvenchost_encode((tra_nvenchost*) obj, sample, type, data);
}

static int encoder_flush(tra_encoder_object* obj) {
  return tra_nvenchost_flush((tra_nvenchost*) obj);
}

/* ------------------------------------------------------- */

/*
  Here we implement the easy layer for the NVIDIA encoder
  "nvenchost". We use the `tra_easy_nvapp` which makes sure we
  have access to a CUDA context and device. W/o this easy layer,
  the user should make sure to provide these to the encoder.
 */
static int easy_encoder_create(
  tra_easy* ez, 
  tra_encoder_settings* cfg,
  void** enc
)
{
  tra_easy_nvapp_settings nvapp_cfg = { 0 };
  tra_nvenc_settings nvenc_cfg = { 0 };
  tra_easy_nvapp* nvapp = NULL;
  tra_nvenchost* inst = NULL;
  int r = 0;
  
  TRAE("@todo create the easy encoder instance.");

  if (NULL == enc) {
    TRAE("Cannot create the easy encoder as the given output argument is NULL.");
    r = -10;
    goto error;
  }

  /* Setup the nvenc settings which needs a context and device handle. */
  nvapp_cfg.easy_ctx = ez;

  r = tra_easy_nvapp_ensure(&nvapp_cfg, &nvapp);
  if (r < 0) {
    TRAE("Failed to ensure the `tra_easy_nvapp`.");
    r = -10;
    goto error;
  }

  if (NULL == nvapp) {
    TRAE("Failed to ensure the `tra_easy_nvapp`.");
    r = -20;
    goto error;
  }

  /* Now that we have the context and device handle, create the encoder. */
  nvenc_cfg.cuda_context_handle = nvapp->cuda_context_handle;
  nvenc_cfg.cuda_device_handle = nvapp->cuda_context_handle;

  r = tra_nvenchost_create(cfg, &nvenc_cfg, &inst);
  if (r < 0) {
    TRAE("Failed to create the `nvenchost` encoder for the easy layer.");
    r = -30;
    goto error;
  }

  /* Finally assign. */
  *enc = inst;

  TRAE("@todo handle error situation where we can't create the encoder.");

 error:
  return r;
}

/* ------------------------------------------------------- */

static int easy_encoder_encode(
  void* enc,
  tra_sample* sample,
  uint32_t type,
  void* data
)
{
  int r = 0;
  
  if (NULL == enc) {
    TRAE("Cannot encode as the given encoder pointer is NULL.");
    r = -10;
    goto error;
  }

  r = tra_nvenchost_encode(enc, sample, type, data);
  if (r < 0) {
    TRAE("Failed to encode the given data via the easy layer");
    r = -20;
    goto error;
  }

 error:
  return r;
}

/* ------------------------------------------------------- */

/*
  Flush the encoder, the given `void* enc` is an instance of the
  `tra_nvenchost` encoder.
*/
static int easy_encoder_flush(void* enc) {

  int r = 0;
  
  if (NULL == enc) {
    TRAE("Cannot flush as the given encoder pointer is NULL.");
    r = -10;
    goto error;
  }

  r = tra_nvenchost_flush(enc);
  if (r < 0) {
    TRAE("Failed to flush the encoder via the easy layer");
    r = -20;
    goto error;
  }

 error:
  return r;
}

/* ------------------------------------------------------- */

static int easy_encoder_destroy(void* enc) {
  
  int result = 0;
  int r = 0;

  TRAE("@todo DESTROY THE EASY ENCODER");

 error:
  return r;
}

/* ------------------------------------------------------- */

int tra_load(tra_registry* reg) {

  int r = 0;

  if (NULL == reg) {
    TRAE("Cannot register the `nvenchost` encoder as the given `tra_registry*` is NULL.");
    return -10;
  }

  r = tra_registry_add_encoder_api(reg, &encoder_api);
  if (r < 0) {
    TRAE("Failed to register the `nvenchost` encoder.");
    return -20;
  }

  r = tra_registry_add_easy_api(reg, &easy_encoder_api);
  if (r < 0) {
    TRAE("Failed to register the `nvenchost` easy encoder.");
    return -30;
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

static tra_easy_api easy_encoder_api = {
  .get_name = encoder_get_name,
  .get_author = encoder_get_author,
  .encoder_create = easy_encoder_create,
  .encoder_encode = easy_encoder_encode,
  .encoder_flush = easy_encoder_flush,
  .encoder_destroy = easy_encoder_destroy,
  .decoder_create = NULL,
  .decoder_decode = NULL,
  .decoder_destroy = NULL,
};

/* ------------------------------------------------------- */
