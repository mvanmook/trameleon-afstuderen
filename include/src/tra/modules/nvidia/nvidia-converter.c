/* ------------------------------------------------------- */

#include <stdlib.h>
#include <cuda.h>
#include <cuda_runtime.h>
#include <tra/modules/nvidia/nvidia-converter-kernels.h>
#include <tra/modules/nvidia/nvidia-converter.h>
#include <tra/modules/nvidia/nvidia.h>
#include <tra/profiler.h>
#include <tra/registry.h>
#include <tra/module.h>
#include <tra/types.h>
#include <tra/log.h>

/* ------------------------------------------------------- */

struct tra_nvconverter {

  tra_converter_callbacks callbacks;   /* The callback(s) that we copy from the `tra_converter_settings`. */
  
  /* Input */
  uint32_t input_type;                 /* The input memory type (`TRA_MEMORY_TYPE_*`) , e.g. if the input data is stored on device or in host memory. */
  uint32_t input_format;               /* The pixel format of the input buffers that we have to convert, e.g. `TRA_IMAGE_FORMAT_NV12`. */
  uint32_t input_width;                /* The width of the input buffers. */
  uint32_t input_height;               /* The height of the input buffers. */
  size_t input_stride;                 /* When the user provides host buffers we allocate device memory that has this stride. */
  uint8_t* input_ptr;                  /* When the user wants to convert host-buffers we first have to upload the data into a device buffer. This member represents this buffer and is allocated when we create the converter. */
  
  /* Output */
  uint32_t output_type;                /* The output memory type, (`TRA_MEMORY_TYPE_*`). */
  uint32_t output_format;              /* The output pixel format, (`TRA_IMAGE_FORMAT_`). */
  uint32_t output_width;               /* The requested output width. */
  uint32_t output_height;              /* The requested output height. */
  size_t output_stride;                /* The stride of our output device buffer. */
  uint8_t* output_ptr;                 /* We allocate a buffer on the device (GPU) into which we store the converted result. */

  /* Host memory */
  uint8_t* host_ptr;                   /* When the user requests to download the converted buffer into host memory, we copy the data into this bufferr. */
  size_t host_size;                    /* The number of bytes we can store in `host_ptr`. */
};

/* ------------------------------------------------------- */

static tra_converter_api converter_api; /* This struct is used when we register the converter api, see `tra_load()` below. */

/* ------------------------------------------------------- */

static const char* converter_get_name();
static const char* converter_get_author();
static int converter_create(tra_converter_settings* cfg, void* settings, tra_converter_object** obj);
static int converter_destroy(tra_converter_object* obj);
static int converter_convert(tra_converter_object* obj, uint32_t type, void* data);

/* ------------------------------------------------------- */

static int tra_nvconverter_convert_with_host_memory(tra_nvconverter* ctx, uint32_t type, void* data);
static int tra_nvconverter_convert_with_device_memory(tra_nvconverter* ctx, uint32_t type, void* data);
static int tra_nvconverter_output_to_host_memory(tra_nvconverter* ctx, uint32_t type, void* data);
static int tra_nvconverter_output_to_device_memory(tra_nvconverter* ctx, uint32_t type, void* data);

/* ------------------------------------------------------- */

/*
  This function will create an instance of the `tra_nvconverter`.
  First we make sure that the given settings are valid and that
  all data formats and types are supported. Then we allocate the
  required buffers. We will always allocate a device buffer that
  will store the converted data. When the output type is set to to
  host memory, we also have to allocate a host buffer. After
  resizing we will then copy the device buffer to the host.
 */
int tra_nvconverter_create(
  tra_converter_settings* cfg,
  void* settings,
  tra_nvconverter** ctx
)
{

  TRAP_TIMER_BEGIN(prof, "tra_nvconverter_create");
    
  cudaError_t status = cudaSuccess;
  tra_nvconverter* inst = NULL;
  int result = 0;
  int r = 0;

  /* ----------------------------------------------- */
  /* Validate                                        */
  /* ----------------------------------------------- */
 
  if (NULL == cfg) {
    TRAE("Cannot create the converter as the given `tra_converter_settings*` is NULL.");
    r = -10;
    goto error;
  }

  if (0 == cfg->output_width) {
    TRAE("Cannot create the converter as the given `tra_converter_settings::output_width` is 0.");
    r = -20;
    goto error;
  }

  if (0 == cfg->output_height) {
    TRAE("Cannot create the converter as the given `tra_converter_settings::output_height` is 0.");
    r = -30;
    goto error;
  }

  if (0 == cfg->input_width) {
    TRAE("Cannot create the converter as the `tra_converter_settings::input_width` is 0.");
    r = -40;
    goto error;
  }

  if (0 == cfg->input_height) {
    TRAE("Cannot create the converter as the `tra_converter_settings::input_height` is 0.");
    r = -50;
    goto error;
  }

  if (NULL == cfg->callbacks.on_converted) {
    TRAE("Cannot create the converter as the `tra_converter_settings::callbacks` haven't been set.");
    r = -60;
    goto error;
  }

  if (TRA_IMAGE_FORMAT_NV12 != cfg->input_format) {
    TRAE("Cannot create the converter as the given `tra_converter_settings::input_format` is not TRA_IMAGE_FORMAT_NV12 and this is the only supported format.");
    r = -60;
    goto error;
  }

  if (TRA_IMAGE_FORMAT_NV12 != cfg->output_format) {
    TRAE("Cannot create the converter as the `tra_converter_settings::output_format` is not supported.");
    r = -70;
    goto error;
  }

  if (NULL == ctx) {
    TRAE("Cannot create the converter as the given `tra_nvconverter**` is NULL.");
    r = -70;
    goto error;
  }

  if (NULL != (*ctx)) {
    TRAE("Cannot create the converter as the given `*tra_nvconverter**` is not NULL. Already created? Or not initialized to NULL?");
    r = -80;
    goto error;
  }

  inst = calloc(1, sizeof(tra_nvconverter));
  if (NULL == inst) {
    TRAE("Cannot create the converter as we failed to allocate it.");
    r = -90;
    goto error;
  }

  TRAE("@todo validate the input and output types and if we support them. ");

  /* Setup */
  inst->callbacks.on_converted = cfg->callbacks.on_converted;
  inst->callbacks.user = cfg->callbacks.user;
  
  inst->input_type = cfg->input_type;
  inst->input_format = cfg->input_format;
  inst->input_width = cfg->input_width;
  inst->input_height = cfg->input_height;
  inst->input_type = cfg->input_type;

  inst->output_type = cfg->output_type;
  inst->output_format = cfg->output_format;
  inst->output_width = cfg->output_width;
  inst->output_height = cfg->output_height;

  /* ----------------------------------------------- */
  /* Allocate our buffers.                           */
  /* ----------------------------------------------- */

  /* I've added this check to make 100% sure we allocate the correct output buffer when we're going to add new pixel formats. */
  if (TRA_IMAGE_FORMAT_NV12 != cfg->input_format) {
    TRAE("Invalid image format, we have to allocate an output buffer for this type. (exiting). ");
    exit(EXIT_FAILURE);
  }

  /*
    When the user supplies host buffers we first have to upload
    them into a device buffer for which we allocate space here.
  */
  if (TRA_MEMORY_TYPE_IMAGE == cfg->input_type) {

    status = cudaMallocPitch(
      (void**) &inst->input_ptr,
      &inst->input_stride,
      inst->input_width,
      inst->input_height + inst->input_height / 2
    );

    if (cudaSuccess != status) {
      TRAE("Failed to allocate the device buffer for the input images that you want to convert.");
      r = -100;
      goto error;;
    }
  }

  /* Allocate NV12 buffer for converted output. */
  status = cudaMallocPitch(
    (void**) &inst->output_ptr,
    &inst->output_stride,
    inst->output_width,
    inst->output_height + (inst->output_height / 2)
  );

  if (cudaSuccess != status) {
    TRAE("Cannot create the sacler as we failed to allocate the output buffer. ");
    r = -110;
    goto error;
  }

  if (0 == inst->output_stride) {
    TRAE("Cannot create the converter as the stride that was returned by `cudaMallocPitch` is invalid (0).");
    r = -120;
    goto error;
  }

  /*
    
    When the user requested to receive that converted data as host
    memory we will allocate a buffer into which we copy the data
    from the device to host after converting. The `host_ptr` will
    be able to hold the same amount of memory as the device
    buffer that holds the converted image. This means that we're
    using a stride too.

    IMPORTANT: we expect the output format to be NV12
    
   */
  if (TRA_MEMORY_TYPE_IMAGE == cfg->output_type) {

    inst->host_size = 0;
    inst->host_size += inst->output_stride * (inst->output_height);       /* Y-plane */
    inst->host_size += inst->output_stride * (inst->output_height / 2);   /* UV-plane */
    
    status = cudaMallocHost((void**) &inst->host_ptr, inst->host_size);
    if (cudaSuccess != status) {
      TRAE("Failed to allocate the host buffer that we need to copy the converted device memory to host memory.");
      r = -120;
      goto error;
    }
  }
  
  /* Assign the result. */
  *ctx = inst;

 error:

  if (r < 0) {
    
    if (NULL != inst) {
      result = tra_nvconverter_destroy(inst);
      if (result < 0) {
        TRAE("After we failed to create the `tra_nvconverter` we also failed to cleanly destroy it.");
      }
    }

    if (NULL != ctx) {
      *ctx = NULL;
    }
  }

  TRAP_TIMER_END(prof, "tra_nvconverter_create");
  
  return r;
}

/* ------------------------------------------------------- */

int tra_nvconverter_destroy(tra_nvconverter* ctx) {

  TRAP_TIMER_BEGIN(prof, "tra_nvconverter_destroy");
  
  cudaError_t status = cudaSuccess;
  int result = 0;
  int r = 0;

  if (NULL == ctx) {
    TRAE("Cannot destroy the converter as it's NULL.");
    result = -10;
    goto error;
  }

  if (NULL != ctx->input_ptr) {
    status = cudaFree(ctx->input_ptr);
    if (cudaSuccess != status) {
      TRAE("Failed to cleanly free the input device buffer.");
      result -= 20;
    }
  }

  if (NULL != ctx->output_ptr) {
    status = cudaFree(ctx->output_ptr);
    if (cudaSuccess != status) {
      TRAE("Failed to cleanly free the output device buffer.");
      result -= 30;
    }
  }

  if (NULL != ctx->host_ptr) {
    status = cudaFreeHost(ctx->host_ptr);
    if (cudaSuccess != status) {
      TRAE("Failed to cleanly free the host buffer.");
      result -= 40;
    }
  }

  ctx->input_type = TRA_MEMORY_TYPE_NONE;
  ctx->input_format = TRA_IMAGE_FORMAT_NONE;
  ctx->input_width = 0;
  ctx->input_height = 0;
  ctx->input_ptr = NULL;

  ctx->output_type = TRA_MEMORY_TYPE_NONE;
  ctx->output_width = 0;
  ctx->output_height = 0;
  ctx->output_stride = 0;
  ctx->output_ptr = NULL;

  ctx->host_ptr = NULL;
  ctx->host_size = 0;

 error:
  
  TRAP_TIMER_END(prof, "tra_nvconverter_destroy");
  
  return result;
}

/* ------------------------------------------------------- */

/*
  This function can be used to convert an image which is either in
  device or host memory. After converting we will "output" the
  converted buffer; this means that we call the `on_converted()`
  callback. When the user has requested to receive host memory
  buffers, we have to copy the converted device buffers first.
 */
int tra_nvconverter_convert(tra_nvconverter* ctx, uint32_t type, void* data) {

  TRAP_TIMER_BEGIN(prof, "tra_nvconverter_convert");
  
  int r = 0;

  if (NULL == ctx) {
    TRAE("Cannot convert as the given `tra_nvconverter*` is NULL.");
    r = -10;
    goto error;
  }
  
  /* ----------------------------------------------- */
  /* Resize                                          */
  /* ----------------------------------------------- */

  switch (ctx->input_type) {
    
    case TRA_MEMORY_TYPE_CUDA: {
      r = tra_nvconverter_convert_with_device_memory(ctx, type, data);
      break;
    }

    case TRA_MEMORY_TYPE_IMAGE: {
      r = tra_nvconverter_convert_with_host_memory(ctx, type, data);
      break;
    }
    
    default: {
      TRAE("Cannot convert as the given input type is not supported: %s.", tra_memorytype_to_string(ctx->input_type));
      r = -20;
      goto error;
    }
  }

  if (r < 0) {
    TRAE("Something went wrong while resizing.");
    r = -30;
    goto error;
  }

  /* ----------------------------------------------- */
  /* Export and notify user.                         */
  /* ----------------------------------------------- */

  switch (ctx->output_type) {
    
    case TRA_MEMORY_TYPE_CUDA: {
      r = tra_nvconverter_output_to_device_memory(ctx, type, data);
      break;
    }

    case TRA_MEMORY_TYPE_IMAGE: {
      r = tra_nvconverter_output_to_host_memory(ctx, type, data);
      break;
    }
    
    default: {
      TRAE("We've successfully converted the input buffer but we don't know how to export it to %s memory.", tra_memorytype_to_string(ctx->output_type));
      r = -40;
      goto error;
    }
  }

  if (r < 0) {
    TRAE("Failed to output the converted buffer.");
    r = -50;
    goto error;
  }

 error:

  TRAP_TIMER_END(prof, "tra_nvconverter_convert");
  
  return r;
}

/* ------------------------------------------------------- */

/*
  This function will convert a NV12 image which is stored in
  device (CUDA) memory. We expect to that `data` is represented
  by a `tra_decoded_device_memory` and it's `data` member by a
  `tra_cuda_device_memory`.
 */
static int tra_nvconverter_convert_with_device_memory(
  tra_nvconverter* ctx,
  uint32_t type,
  void* data
)
{
  TRAP_TIMER_BEGIN(prof, "tra_nvconverter_convert_with_device_memory");

  tra_memory_cuda* cuda_mem = NULL;
  int r = 0;

  /* ----------------------------------------------- */
  /* Validate input                                  */
  /* ----------------------------------------------- */

  if (NULL == ctx) {
    TRAE("Cannot convert as the given `tra_nvconverter*` is NULL.");
    r = -10;
    goto error;
  }

  if (TRA_MEMORY_TYPE_CUDA != type) {
    TRAE("Cannot convert as the given type is not `TRA_MEMORY_TYPE_CUDA`.");
    r = -20;
    goto error;
  }

  if (NULL == data) {
    TRAE("Cannot convert as the the given `data` is NULL.");
    r = -30;
    goto error;
  }
  
  cuda_mem = (tra_memory_cuda*) data;
  if (0 == cuda_mem->stride) {
    TRAE("Cannot convert the given device image as the stride hasn't been set.");
    r = -60;
    goto error;
  }

  if (0 == cuda_mem->ptr) {
    TRAE("Cannot convert as the given device image pointer is 0.");
    r = -70;
    goto error;
  }

  /* ----------------------------------------------- */
  /* Validate state                                  */
  /* ----------------------------------------------- */

  if (TRA_IMAGE_FORMAT_NV12 != ctx->input_format) {
    TRAE("Cannot convert as we currently only support `TRA_IMAGE_FORMAT_NV12`.");
    r = -80;
    goto error;
  }

  if (0 == ctx->input_width) {
    TRAE("Cannot convert as our `input_width` is 0.");
    r = -90;
    goto error;
  }

  if (0 == ctx->input_height) {
    TRAE("Cannot convert as our `input_height` is 0.");
    r = -100;
    goto error;
  }

  if (0 == ctx->output_width) {
    TRAE("Cannot convert as our `output_height` is 0.");
    r = -110;
    goto error;
  }

  if (0 == ctx->output_height) {
    TRAE("Cannot convert as our `output_height` is 0.");
    r = -120;
    goto error;
  }

  if (0 == ctx->output_stride) {
    TRAE("Cannot convert as our `output_stride` is 0.");
    r = -130;
    goto error;
  }

  if (NULL == ctx->output_ptr) {
    TRAE("Cannot convert as our `output_ptr` is NULL.");
    r = -140;
    goto error;
  }

  if (TRA_MEMORY_TYPE_CUDA != ctx->output_type) {
    TRAE("Cannot convert as currently we only support `TRA_MEMORY_TYPE_CUDA` as the output type.");
    r = -150;
    goto error;
  }

  /* ----------------------------------------------- */
  /* Resize                                          */
  /* ----------------------------------------------- */

  r = tra_cuda_resize_nv12(
    (void*)cuda_mem->ptr,
    ctx->input_width,
    ctx->input_height,
    cuda_mem->stride,
    ctx->output_ptr,
    ctx->output_width,
    ctx->output_height,
    ctx->output_stride
  );

  if (r < 0) {
    TRAE("Something went wrong while running the resize kernel.");
    r = -160;
    goto error;
  }
      
 error:

  TRAP_TIMER_END(prof, "tra_nvconverter_convert_with_device_memory");
  
  return r;
}

/* ------------------------------------------------------- */

/*

  This function is called when the user will supply NV12 buffers
  which are stored in host (CPU) memory. This function will
  upload the given `data` which must be represented by a
  `tra_memory_image` instance. Once uploaded we wil convert it.

 */
static int tra_nvconverter_convert_with_host_memory(
  tra_nvconverter* ctx,
  uint32_t type,
  void* data
)
{

  TRAP_TIMER_BEGIN(prof, "tra_nvconverter_convert_with_host_memory");
  
  cudaError_t status = cudaSuccess;
  tra_memory_image* img = NULL;
  uint8_t* dev_y = NULL;
  uint8_t* dev_uv = NULL;
  int r = 0;

  /* ----------------------------------------------- */
  /* VALIDATE                                        */
  /* ----------------------------------------------- */
  
  if (NULL == ctx) {
    TRAE("Cannot convert as the given `tra_nvconverter*` is NULL.");
    r = -10;
    goto error;
  }

  if (0 == ctx->input_stride) {
    TRAE("Cannot convert the given image as the `input_stride` member of our converter is 0. We should have allocated device memory and the `input_stride` member should hold the stride of this buffer. ");
    r = -20;
    goto error;
  }

  if (NULL == ctx->input_ptr) {
    TRAE("Cannot convert the given image the `input_ptr` member of the converter is NULL. Did you create the converter using `TRA_MEMORY_TYPE_IMAGE` as `input_type`?");
    r = -30;
    goto error;
  }

  if (TRA_MEMORY_TYPE_IMAGE != type) {
    TRAE("Cannot convert as the given `type` is not a `TRA_MEMORY_TYPE_IMAGE`.");
    r = -20;
    goto error;
  }

  if (NULL == data) {
    TRAE("Cannot convert as the given `data` pointer is NULL.");
    r = -30;
    goto error;
  }

  img = (tra_memory_image*) data;
  
  if (NULL == img->plane_data[0]) {
    TRAE("Cannot convert as the plane data of the given `tra_memory_image` is NULL.");
    r = -40;
    goto error;
  }

  if (0 == img->plane_strides[0]) {
    TRAE("Cannot convert as the `plane_strides[0]` of the given `tra_memory_image` is 0.");
    r = -50;
    goto error;
  }

  if (ctx->input_width != img->image_width) {
    TRAE("The given `tra_memory_image` has a different width than the width you passed into the create function of the converter. Check your settings.");
    r = -60;
    goto error;
  }

  if (ctx->input_height != img->image_height) {
    TRAE("The given `tra_memory_image` has a different height than the height you passed into the create function of the converter. Check your settings.");
    r = -70;
    goto error;
  }

  if (TRA_IMAGE_FORMAT_NV12 != img->image_format) {
    TRAE("Cannot convert as the `image_format` is not `TRA_IMAGE_FORMAT_NV12` which is the only image format we currently support. ");
    r = -80;
    goto error;
  }

  /* ----------------------------------------------- */
  /* Upload                                          */
  /* ----------------------------------------------- */

  /* Set the destination pointers (device). */
  dev_y = ctx->input_ptr;
  dev_uv = ctx->input_ptr + (ctx->input_height * ctx->input_stride);
  
  /* Copy Y-plane */
  status = cudaMemcpy2D(
    dev_y,                   /* Destination address */
    ctx->input_stride,        /* Pitch of destination */
    img->plane_data[0],      /* Source address */
    img->plane_strides[0],   /* Pitch of source */
    img->image_width,        /* Width of matrix transfer */
    img->image_height,       /* Height of matrix transfer */
    cudaMemcpyHostToDevice   /* Type of transfer. */
  );

  if (cudaSuccess != status) {
    TRAE("Failed to copy the Y-plane data into the device input buffer.");
    r = -70;
    goto error;
  }

  /* Copy UV-plane */
  status = cudaMemcpy2D(
    dev_uv,                  /* Destination address */
    ctx->input_stride,        /* Pitch of destination */
    img->plane_data[1],      /* Source address */
    img->plane_strides[1],   /* Pitch of source */
    img->image_width,        /* Width of matrix transfer */
    img->image_height / 2,   /* Height of matrix transfer */
    cudaMemcpyHostToDevice   /* Type of transfer. */
  );

  if (cudaSuccess != status) {
    TRAE("Failed to copy the UV-plane data into the device input buffer.");
    r = -70;
    goto error;
  }

  /* ----------------------------------------------- */
  /* Resize                                          */
  /* ----------------------------------------------- */

  r = tra_cuda_resize_nv12(
    (void*)ctx->input_ptr,
    ctx->input_width,
    ctx->input_height,
    ctx->input_stride,
    ctx->output_ptr,
    ctx->output_width,
    ctx->output_height,
    ctx->output_stride
  );

  if (r < 0) {
    TRAE("Something went wrong while running the resize kernel.");
    r = -80;
    goto error;
  }

 error:

  TRAP_TIMER_END(prof, "tra_nvconverter_convert_with_host_memory");
  
  return r;
}

/* ------------------------------------------------------- */

/*
  This function is called when the user has requested to receive
  the converted buffers as host memory. This means that we have
  to copy the device memory into host memory. The kernel will
  always run on device memory and in most cases you want to keep
  the converted data on the GPU, but this function will allow you
  to get the data in host memory.
 */
static int tra_nvconverter_output_to_host_memory(
  tra_nvconverter* ctx,
  uint32_t type,
  void* data
)
{

  TRAP_TIMER_BEGIN(prof, "tra_nvconverter_output_to_host_memory");
  
  cudaError_t status = cudaSuccess;
  tra_memory_image img = { 0 };
  int r = 0;

  if (NULL == ctx) {
    TRAE("Cannot output the converted data to host memory as the given `tra_nvconverter*` is NULL.");
    r = -10;
    goto error;
  }

  if (NULL == ctx->host_ptr) {
    TRAE("Cannot output the converted data to host memory as our `host_ptr` is NULL.");
    r = -20;
    goto error;
  }

  if (NULL == ctx->callbacks.on_converted) {
    TRAE("Cannot output the converted image to host memory as the `on_converted` callback hasn't been set.");
    r = -40;
    goto error;
  }

  status = cudaMemcpy(
    ctx->host_ptr,   /* Destination address. */
    ctx->output_ptr, /* Source address */
    ctx->host_size,  /* Number of bytes to copy */
    cudaMemcpyDeviceToHost
  );

  if (cudaSuccess != status) {
    TRAE("Failed to copy the converted device buffer into our host buffer.");
    r = -50;
    goto error;
  }

  img.image_format = TRA_IMAGE_FORMAT_NV12;
  img.image_width = ctx->output_width;
  img.image_height = ctx->output_height;
  img.plane_count = 2;
    
  img.plane_data[0] = ctx->host_ptr; /* Y-plane */
  img.plane_data[1] = ctx->host_ptr + (ctx->output_height * ctx->output_stride); /* UV-plane */
  img.plane_data[2] = NULL;

  img.plane_strides[0] = ctx->output_stride;
  img.plane_strides[1] = ctx->output_stride;
  img.plane_strides[2] = 0;

  img.plane_heights[0] = ctx->output_height;
  img.plane_heights[1] = ctx->output_height / 2;
  img.plane_heights[2] = 0;

  r = ctx->callbacks.on_converted(
    ctx->output_type,
    &img,
    ctx->callbacks.user
  );

  if (r < 0) {
    TRAE("The `on_converted()` calblack returned an error (%d)", r);
    r = -60;
    goto error;
  }
  
 error:
  
  TRAP_TIMER_END(prof, "tra_nvconverter_output_to_host_memory");
  
  return r;
}

/* ------------------------------------------------------- */

/*
  This converter can either convert directly into device memory
  or host memory. The convert operation itself, will always take
  place in device memory. This function will export the converted
  memory directly w/o copying it back to host memory. As the
  convert takes place in device memory we only have to perform
  some validation and then call the callback with the device
  memory.
*/
static int tra_nvconverter_output_to_device_memory(
  tra_nvconverter* ctx,
  uint32_t type,
  void* data
)
{
  TRAP_TIMER_BEGIN(prof, "tra_nvconverter_output_to_device_memory");
  
  tra_memory_cuda cuda_output_mem; 
  int r = 0;

  if (NULL == ctx) {
    TRAE("Cannot output to device memory as the given `tra_nvconverter*` is NULL.");
    r = -10;
    goto error;
  }
  
  if (NULL == ctx->callbacks.on_converted) {
    TRAE("Cannot convert as the `on_converted` callback hasn't been set.");
    r = -20;
    goto error;
  }

  if (NULL == ctx->output_ptr) {
    TRAE("Cannot convert as the `output_ptr` of the `tra_nvconverter` is NULL. Not initialized?");
    r = -30;
    goto error;
  }

  /* Notify the user that we've converted the data. */
  cuda_output_mem.ptr = (CUdeviceptr) ctx->output_ptr;
  cuda_output_mem.stride = ctx->output_stride;
  
  r = ctx->callbacks.on_converted(
    ctx->output_type,
    &cuda_output_mem,
    ctx->callbacks.user
  );

  if (r < 0) {
    TRAE("The `on_converted()` callback returned an error (%d).", r);
    r = -40;
    goto error;
  }

 error:

  TRAP_TIMER_END(prof, "tra_nvconverter_output_to_device_memory");
  
  return r;
}

/* ------------------------------------------------------- */

static const char* converter_get_name() {
  return "nvconverter";
}

static const char* converter_get_author() {
  return "roxlu";
}

/* ------------------------------------------------------- */

static int converter_create(
  tra_converter_settings* cfg,
  void* settings,
  tra_converter_object** obj
)
{
  tra_nvconverter* inst = NULL;
  int status = 0;
  int r = 0;
  
  if (NULL == cfg) {
    TRAE("Cannot create the converter, given `tra_converter_settings*` is NULL");
    r = -10;
    goto error;
  }

  if (NULL == obj) {
    TRAE("Cannot create the converter as the given output `tra_converter_object**` is NULL.");
    r = -20;
    goto error;
  }

  if (NULL != *(obj)) {
    TRAE("Cannot create the converter as the given `*tra_converter_object**` is not NULL. Already created? Not initialized to NULL?");
    r = -30;
    goto error;
  }

  r = tra_nvconverter_create(cfg, settings, (tra_nvconverter**) &inst);
  if (r < 0) {
    TRAE("Cannot create the converter, the call to `tra_nvconverter_create()` returned an error.");
    r = -40;
    goto error;
  }

  /* Finally assign */
  *obj = (tra_converter_object*) inst;

 error:

  if (r < 0) {
    
    if (NULL != inst) {
      status = tra_nvconverter_destroy(inst);
      inst = NULL;
    }

    /* Make sure that the output is set to NULL when an error occured. */
    if (NULL != obj) {
      *obj = NULL;
    }
  }
  
  return r;
}

/* ------------------------------------------------------- */

static int converter_destroy(tra_converter_object* obj) {

  int r = 0;
  
  if (NULL == obj) {
    TRAE("Cannot destroy the converter as it's NULL.");
    return -10;
  }

  r = tra_nvconverter_destroy((tra_nvconverter*) obj);
  if (r < 0) {
    TRAE("Failed to cleanly destroy the converter.");
    return -20;
  }

  return r;
}

/* ------------------------------------------------------- */

static int converter_convert(tra_converter_object* obj, uint32_t type, void* data) {
  return tra_nvconverter_convert((tra_nvconverter*) obj, type, data);
}

/* ------------------------------------------------------- */

int tra_load(tra_registry* reg) {

  int r = 0;

  if (NULL == reg) {
    TRAE("Cannot register the NVIDIA converter as the given `tra_registry*` is NULL.");
    return -10;
  }

  r = tra_registry_add_converter_api(reg, &converter_api);
  if (r < 0) {
    TRAE("Failed to register the `nvconverter` api.");
    return -20;
  }

  return r;
}

/* ------------------------------------------------------- */

static tra_converter_api converter_api = {
  .get_name = converter_get_name,
  .get_author = converter_get_author,
  .create = converter_create,
  .destroy = converter_destroy,
  .convert = converter_convert,
};

/* ------------------------------------------------------- */

