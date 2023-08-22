/* ------------------------------------------------------- */

#define GL_GLEXT_PROTOTYPES
#include <GL/glcorearb.h>

#include <cuda.h>
#include <cudaGL.h>
#include <tra/modules/opengl/opengl-interop-cuda.h>
#include <tra/modules/opengl/opengl-api.h>
#include <tra/modules/opengl/opengl.h>
#include <tra/modules/nvidia/nvidia.h>
#include <tra/profiler.h>
#include <tra/registry.h>
#include <tra/module.h>
#include <tra/utils.h>
#include <tra/types.h>
#include <tra/log.h>

/* ------------------------------------------------------- */

static const char* interop_get_name();
static const char* interop_get_author();
static int interop_create(tra_interop_settings* cfg, void* settings, tra_interop_object** obj);
static int interop_destroy(tra_interop_object* obj);
static int interop_upload(tra_interop_object* obj, uint32_t type, void* data);

/* ------------------------------------------------------- */

struct gl_cu {

  /* General */
  tra_interop_settings settings;
  uint32_t image_width;
  uint32_t image_height;

  /* OpenGL */
  tra_opengl_funcs gl;
  GLuint pbo_y;    /* The `GL_PIXEl_UNPACK_BUFFER` for interop with CUDA. This PBO stores the Y-plane */
  GLuint pbo_uv;   /* The `GL_PIXEl_UNPACK_BUFFER` for interop with CUDA. This PBO stores the UV-plane */
  GLuint tex_y;    /* The texture into which we copy the data from the `pbo_y`. */
  GLuint tex_uv;   /* The texture into which we copy the data from the `pbo_uv`. */
};

/* ------------------------------------------------------- */

static tra_interop_api interop_api;

/* ------------------------------------------------------- */

int gl_cu_create(
  tra_interop_settings* cfg,
  gl_cu_settings* settings,
  gl_cu** ctx
)
{

  TRAP_TIMER_BEGIN(prof_create, "gl_cu_create");
  
  gl_cu* inst = NULL;
  int result = 0;
  int r = 0;

  /* ----------------------------------------------- */
  /* Validate                                        */
  /* ----------------------------------------------- */
 
  if (NULL == cfg) {
    TRAE("Cannot create the `opengl-cuda` interop as the given `tra_interop_settings*` is NULL");
    r = -10;
    goto error;
  }

  if (NULL == ctx) {
    TRAE("Cannot create the `opengl-cuda` interop as the given result `gl_cu**` is NULL.");
    r = -20;
    goto error;
  }

  if (NULL != (*ctx)) {
    TRAE("Cannot create the `opengl-cuda` interop as the `*(gl_cu**)` is NOT NULL. Did you already create it or maybe forgot to initialize it to NULL?");
    r = -30;
    goto error;
  }
  
  if (0 == cfg->image_width) {
    TRAE("Cannot create the `opengl-cuda` interop as the given `image_width` is 0.");
    r = -40;
    goto error;
  }

  if (0 == cfg->image_height) {
    TRAE("Cannot create the `opengl-cuda` interop as the given `image_height` is 0.");
    r = -50;
    goto error;
  }

  if (TRA_IMAGE_FORMAT_NV12 != cfg->image_format) {
    TRAE("Cannot create the `opengl-cuda` interop as the given `image_format` is unupported. Currently we only support `TRA_IMAGE_FORMAT_NV12`.");
    r = -60;
    goto error;
  }
  
  if (NULL == cfg->callbacks.on_uploaded) {
    TRAE("Cannot create the `opengl-cuda` interop as the given `on_uploaded` callbacks wasn't set.");
    r = -80;
    goto error;
  }

  if (NULL == settings) {
    TRAE("Cannot create the `opengl-cuda` interop as the given `gl_cu_settings*` is NULL.");
    r = -70;
    goto error;
  }

  if (NULL == settings->gl_api) {
    TRAE("Cannot create the `opengl-cuda` interop as the given `gl_cu_settings::gl` member is NULL.");
    r = -70;
    goto error;
  }

  /* ----------------------------------------------- */
  /* Create and setup                                */
  /* ----------------------------------------------- */

  inst = calloc(1, sizeof(gl_cu));
  if (NULL == inst) {
    TRAE("Cannot create the `opengl-cuda` interop. Failed to allocate the `interop` instance.");
    r = -40;
    goto error;
  }

  /* Assign the GL functions that the user provided. */
  r = settings->gl_api->load_functions(&inst->gl);
  if (r < 0) {
    TRAE("Cannot create the `opengl-cuda` interop. Failed to load the OpenGL functions.");
    r = -50;
    goto error;
  }
  
  /* Create the buffers that we use. */
  inst->gl.glGenBuffers(1, &inst->pbo_y);
  if (0 == inst->pbo_y) {
    TRAE("Failed to create the Y PBO.");
    r = -10;
    goto error;
  }

  inst->gl.glGenBuffers(1, &inst->pbo_uv);
  if (0 == inst->pbo_uv) {
    TRAE("Failed to allocate the UV PBO.");
    r = -20;
    goto error;
  }

  inst->gl.glGenTextures(1, &inst->tex_y);
  if (0 == inst->tex_y) {
    TRAE("Failed to create a texture for the Y-plane.");
    r = -50;
    goto error;
  }

  inst->gl.glGenTextures(1, &inst->tex_uv);
  if (0 == inst->tex_uv) {
    TRAE("Failed to create the texture for the UV-plane.");
    r = -60;
    goto error;
  }

  /* Setup the PBOs */
  inst->gl.glBindBuffer(GL_PIXEL_UNPACK_BUFFER, inst->pbo_y);
  inst->gl.glBufferData(GL_PIXEL_UNPACK_BUFFER, cfg->image_width * cfg->image_height, NULL, GL_STREAM_DRAW);
  inst->gl.glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

  inst->gl.glBindBuffer(GL_PIXEL_UNPACK_BUFFER, inst->pbo_uv);
  inst->gl.glBufferData(GL_PIXEL_UNPACK_BUFFER, (cfg->image_width * cfg->image_height) / 2, NULL, GL_STREAM_DRAW);
  inst->gl.glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

  /* Setup Y-texture. */
  inst->gl.glBindTexture(GL_TEXTURE_2D, inst->tex_y);
  inst->gl.glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, cfg->image_width, cfg->image_height, 0, GL_RED, GL_UNSIGNED_BYTE, NULL);
  inst->gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  inst->gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  inst->gl.glBindTexture(GL_TEXTURE_2D, 0);

  /* Setup UV-texture. */
  inst->gl.glBindTexture(GL_TEXTURE_2D, inst->tex_uv);
  inst->gl.glTexImage2D(GL_TEXTURE_2D, 0, GL_RG, cfg->image_width / 2, cfg->image_height / 2, 0, GL_RG, GL_UNSIGNED_BYTE, NULL);
  inst->gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  inst->gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  inst->gl.glBindTexture(GL_TEXTURE_2D, 0);

  /* General members */
  inst->settings = *cfg;
  inst->image_width = cfg->image_width;
  inst->image_height = cfg->image_height;

  /* Finally assign the result. */
  *ctx = inst;
  
 error:

  if (r < 0) {
    
    if (NULL != inst) {
      
      result = gl_cu_destroy(inst);
      if (result < 0) {
        TRAE("After we failed to create the `opengl-cuda` interop we also failed to cleanly destroy it.");
      }

      inst = NULL;
    }

    if (NULL != ctx) {
      *ctx = NULL;
    }
  }

  TRAP_TIMER_END(prof_create, "gl_cu_create");

  return r;
}

/* ------------------------------------------------------- */

int gl_cu_destroy(gl_cu* ctx) {

  TRAP_TIMER_BEGIN(prof, "gl_cu_destroy");
    
  int result = 0;
  int r = 0;

  if (NULL == ctx) {
    TRAE("Cannot destroy the `opengl-cuda` interop objects as the given pointer is NULL.");
    result = -10;
    goto error;
  }

  if (0 != ctx->pbo_y) {
    ctx->gl.glDeleteBuffers(1, &ctx->pbo_y);
    ctx->pbo_y = 0;
  }

  if (0 != ctx->pbo_uv) {
    ctx->gl.glDeleteBuffers(1, &ctx->pbo_uv);
    ctx->pbo_uv = 0;
  }

  if (0 != ctx->tex_y) {
    ctx->gl.glDeleteTextures(1, &ctx->tex_y);
    ctx->tex_y = 0;
  }

  if (0 != ctx->tex_uv) {
    ctx->gl.glDeleteTextures(1, &ctx->tex_uv);
    ctx->tex_uv = 0;
  }

 error:
  
  TRAP_TIMER_END(prof, "gl_cu_destroy");
  
  return result;
}

/* ------------------------------------------------------- */

/*

  GENERAL INFO:
  
    This function will upload the given `data`, which should be a
    CUDA device pointer, into OpenGL textures. It will uplaod (or
    actually copy) the data into a Y and UV texture that can
    be used with OpenGL. By using this function we can skip a
    memory copy from device (CUDA) to host and then back to
    device (OpenGL) again.
    
    Though currently we're still copying the given device pointer
    into the PBO and then we copy those into the textures. I'm
    not sure if this means we're doing a redundant copy as it
    seems we're doing two copies (these copies are done on device
    though). See [this][0] forum post I've created about this.

  REFERENCES:

   [0]: https://forums.developer.nvidia.com/t/fastest-solution-to-present-decoded-frames-with-opengl-with-nvdec/233551 "Fastest solution ... with NVIDEC"
  
 */
int gl_cu_upload(
  gl_cu* ctx,
  uint32_t type,
  void* data
)
{
  
  TRAP_TIMER_BEGIN(prof, "gl_cu_upload");
  
  tra_decoded_device_memory* dev_mem = NULL;
  tra_cuda_device_memory* cuda_mem = NULL;
  tra_upload_opengl_nv12 uploaded = { 0 };
  CUgraphicsResource reg_y = { 0 };
  CUgraphicsResource reg_uv = { 0 };
  CUdeviceptr ptr_y = { 0 };
  CUdeviceptr ptr_uv = { 0 };
  CUDA_MEMCPY2D mem_copy = { 0 };
  size_t mapped_size = 0;
  CUresult result = CUDA_SUCCESS;
  int is_y_registered = 0;
  int is_y_mapped = 0;
  int is_uv_registered = 0;
  int is_uv_mapped = 0;
  int r = 0;

  /* ----------------------------------------------- */
  /* Validate                                        */
  /* ----------------------------------------------- */

  if (NULL == ctx) {
    TRAE("Cannot upload data as the given `gl_cu*` is NULL.");
    r = -10;
    goto error;
  }
  
  if (TRA_MEMORY_TYPE_DEVICE_IMAGE != type) {
    TRAE("Received decoded data but it's not stored on device; we can only handle device memory at this point.");
    r = -20;
    goto error;
  }

  if (NULL == data) {
    TRAE("Cannot upload the data as it's NULL.");
    r = -30;
    goto error;
  }

  dev_mem = (tra_decoded_device_memory*) data;
  if (TRA_DEVICE_MEMORY_TYPE_CUDA != dev_mem->type) {
    TRAE("We received decoded device memory but it's not CUDA memory. We expect it to be CUDA memory.");
    r = -40;
    goto error;
  }

  if (NULL == dev_mem->data) {
    TRAE("The received CUDA device memory data is NULL. ");
    r = -50;
    goto error;
  }

  cuda_mem = (tra_cuda_device_memory*) dev_mem->data;
  if (0 == cuda_mem->ptr) {
    TRAE("The receivced cuda device memory pointer is NULL.");
    r = -60;
    goto error;
  }

  if (0 == ctx->pbo_y) {
    TRAE("Cannot handle the decode data as our `pbo` is not created.");
    r = -40;
    goto error;
  }

  if (0 == ctx->image_width) {
    TRAE("Cannot handle the decoded data as the `image_width` 0.");
    r = -50;
    goto error;
  }

  if (0 == ctx->image_height) {
    TRAE("Cannot hanbdle the decoded data as the `image_height` is 0.");
    r = -60;
    goto error;
  }

  if (NULL == ctx->settings.callbacks.on_uploaded) {
    TRAE("Cannot handled the decoded data as the `on_uploaded` callbacks was not set. Makes no sense to upload when you're not going to handle the data");
    r = -70;
    goto error;
  }

  /* ----------------------------------------------- */
  /* Get pointer to registered PBO                   */
  /* ----------------------------------------------- */

  /* Register + map */
  result = cuGraphicsGLRegisterBuffer(&reg_y, ctx->pbo_y, CU_GRAPHICS_REGISTER_FLAGS_WRITE_DISCARD);
  if (CUDA_SUCCESS != result) {
    TRAE("Failed to register the Y-PBO.");
    r = -40;
    goto error;
  }

  is_y_registered = 1;

  result = cuGraphicsGLRegisterBuffer(&reg_uv, ctx->pbo_uv, CU_GRAPHICS_REGISTER_FLAGS_WRITE_DISCARD);
  if (CUDA_SUCCESS != result) {
    TRAE("Failed to register the UV-PBO.");
    r = -50;
    goto error;
  }

  is_uv_registered = 1;

  /* Map the PBO so it's accessible by CUDA */
  result = cuGraphicsMapResources(1, &reg_y, 0);
  if (CUDA_SUCCESS != result) {
    TRAE("Failed to map the registered PBO resource.");
    r = -50;
    goto error;
  }

  is_y_mapped = 1;

  result = cuGraphicsMapResources(1, &reg_uv, 0);
  if (CUDA_SUCCESS != result) {
    TRAE("Failed to map the registerd UV-PBO resource.");
    r = -70;
    goto error;
  }

  is_uv_mapped = 1;

  /* Now that we've mapped the PBO registration we can get the device pointer that represents this mapping. */
  result = cuGraphicsResourceGetMappedPointer(&ptr_y, &mapped_size, reg_y);
  if (CUDA_SUCCESS != result) {
    TRAE("Failed to get the mapped pointer for Y.");
    r = -60;
    goto error;
  }
  
  result = cuGraphicsResourceGetMappedPointer(&ptr_uv, &mapped_size, reg_uv);
  if (CUDA_SUCCESS != result) {
    TRAE("Failed to get the mapped pointer for UV.");
    r = -60;
    goto error;
  }

  /* ----------------------------------------------- */
  /* Copy decoded data into PBO.                     */
  /* ----------------------------------------------- */

  /* Copy into the Y-PBO */
  mem_copy.srcMemoryType = CU_MEMORYTYPE_DEVICE;
  mem_copy.srcDevice = cuda_mem->ptr;
  mem_copy.srcPitch = cuda_mem->pitch;
  mem_copy.dstMemoryType = CU_MEMORYTYPE_DEVICE;
  mem_copy.dstDevice = ptr_y;
  mem_copy.dstPitch = ctx->image_width;
  mem_copy.WidthInBytes = ctx->image_width;
  mem_copy.Height = ctx->image_height;
  
  result = cuMemcpy2DAsync(&mem_copy, 0);
  if (CUDA_SUCCESS != result) {
    TRAE("Failed to copy the decoded buffer.");
    r = -70;
    goto error;
  }

  /* Copy into the UV-PBO */
  mem_copy.srcMemoryType = CU_MEMORYTYPE_DEVICE;
  mem_copy.srcDevice = cuda_mem->ptr + (cuda_mem->pitch * ctx->image_height);
  mem_copy.srcPitch = cuda_mem->pitch;
  mem_copy.dstMemoryType = CU_MEMORYTYPE_DEVICE;
  mem_copy.dstDevice = ptr_uv;
  mem_copy.dstPitch = ctx->image_width; /* The image width is half the size of the Y-plane, though we have 2 values U and V per pixel; so the destination pitch is `(2 * half_width) = width` */
  mem_copy.WidthInBytes = ctx->image_width;
  mem_copy.Height = ctx->image_height / 2;
  
  result = cuMemcpy2DAsync(&mem_copy, 0);
  if (CUDA_SUCCESS != result) {
    TRAE("Failed to copy the decoded buffer.");
    r = -80;
    goto error;
  }

  /* This performs a copy directly from the PBO into the textures. */
  ctx->gl.glBindBuffer(GL_PIXEL_UNPACK_BUFFER, ctx->pbo_y);
  ctx->gl.glBindTexture(GL_TEXTURE_2D, ctx->tex_y);
  ctx->gl.glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, ctx->image_width, ctx->image_height, GL_RED, GL_UNSIGNED_BYTE, 0);

  ctx->gl.glBindBuffer(GL_PIXEL_UNPACK_BUFFER, ctx->pbo_uv);
  ctx->gl.glBindTexture(GL_TEXTURE_2D, ctx->tex_uv);
  ctx->gl.glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, ctx->image_width / 2 , ctx->image_height / 2, GL_RG, GL_UNSIGNED_BYTE, 0);

  ctx->gl.glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

  /* Now that we've uploaded the data, notify the user. */
  uploaded.tex_y = ctx->tex_y;
  uploaded.tex_uv = ctx->tex_uv;

  r = ctx->settings.callbacks.on_uploaded(
    TRA_UPLOAD_TYPE_OPENGL_NV12,
    &uploaded,
    ctx->settings.callbacks.user
  );

  if (r < 0) {
    TRAE("The user failed to handled an uploaded decoded frame correctly.");
    r = -90;
    goto error;
  }
  
 error:

  /*
    This is where we unmap the resources which will cause a
     timeline sync.  @todo this is something where we could
     improve performance.
  */
  TRAP_TIMER_BEGIN(prof_um, "gl_cu_upload:unmap");
  
  if (1 == is_y_registered) {
    cuGraphicsUnmapResources(1, &reg_y, 0);
    is_y_registered = 0;
  }
  
  if (1 == is_y_mapped) {
    cuGraphicsUnregisterResource(reg_y);
    is_y_mapped = 0;
  }

  if (1 == is_uv_registered) {
    cuGraphicsUnmapResources(1, &reg_uv, 0);
    is_uv_registered = 0;
  }
  
  if (1 == is_uv_mapped) {
    cuGraphicsUnregisterResource(reg_uv);
    is_uv_mapped = 0;
  }
  
  TRAP_TIMER_END(prof_um, "gl_cu_upload:unmap");

  TRAP_TIMER_END(prof, "gl_cu_upload");

  return r;
}

/* ------------------------------------------------------- */

static const char* interop_get_name() {
  return "opengl-interop-cuda";
}

/* ------------------------------------------------------- */

static const char* interop_get_author() {
  return "roxlu";
}

/* ------------------------------------------------------- */

static int interop_create(
  tra_interop_settings* cfg,
  void* settings,
  tra_interop_object** obj
)
{
  gl_cu* inst = NULL;
  int result = 0;
  int r = 0;

  if (NULL == cfg) {
    TRAE("Cannot create the `opengl-cuda` interop as the given `tra_interop_settings*` is NULL.");
    r = -10;
    goto error;
  }

  if (NULL == obj) {
    TRAE("Cannot create the `opengl-cuda` interop as the given `tra_interop_obj**` is NULL.");
    r = -20;
    goto error;
  }

  r = gl_cu_create(cfg, settings, &inst);
  if (r < 0) {
    TRAE("Cannot create the `opengl-cuda` interop. Failed to create an instance.");
    r = -30;
    goto error;
  }

  *obj = (tra_interop_object*) inst;

 error:

  if (r < 0) {
    if (NULL != inst) {
      result = interop_destroy((tra_interop_object*) inst);
      if (result < 0) {
        TRAE("Failed to create the `opengl-cuda` and also failed to cleanly destroy the instance.");
      }
    }
  }
                      
  return r;
}

/* ------------------------------------------------------- */

static int interop_destroy(tra_interop_object* obj) {

  int r = 0;
  
  if (NULL == obj) {
    TRAE("Cannot destroy the given `opengl-cuda` interop instance as it's NULL.");
    return -10;
  }

  r = gl_cu_destroy((gl_cu*) obj);
  if (r < 0) {
    TRAE("Failed to cleanly destroy the `opengl-cuda` interop instance.");
    return -20;
  }
    
  return 0;
}

/* ------------------------------------------------------- */

static int interop_upload(tra_interop_object* obj, uint32_t type, void* data) {

  int r = 0;

  if (NULL == obj) {
    TRAE("Cannot upload as the given `tra_interop_object*` is NULL.");
    return -10;
  }

  r = gl_cu_upload((gl_cu*) obj, type, data);
  if (r < 0) {
    TRAE("Failed to upload.");
    return -20;
  }

  return r;
}

/* ------------------------------------------------------- */

/*
  This is called when `tra_core` is created and the registry
  scans the module directory. This should register this OpenGL
  interop module.
*/
int tra_load(tra_registry* reg) {

  int r = 0;

  if (NULL == reg) {
    TRAE("Cannot load the OpenGL interop module as the given `tra_registry*` is NULL.");
    return -10;
  }

  r = tra_registry_add_interop_api(reg, &interop_api);
  if (r < 0) {
    TRAE("Failed to register the `opengl-cuda` interop api.");
    return -30;
  }

  return 0;
}

/* ------------------------------------------------------- */

static tra_interop_api interop_api = {
  .get_name = interop_get_name,
  .get_author = interop_get_author,
  .create = interop_create,
  .destroy = interop_destroy,
  .upload = interop_upload,
};
  
/* ------------------------------------------------------- */
