/* ------------------------------------------------------- */

#define GL_GLEXT_PROTOTYPES
#include <GL/glcorearb.h>

#include <stdlib.h>
#include <tra/modules/nvidia/nvEncodeAPI.h>
#include <tra/modules/nvidia/nvidia-enc-opengl.h>
#include <tra/modules/nvidia/nvidia-enc.h>
#include <tra/modules/nvidia/nvidia.h>
#include <tra/registry.h>
#include <tra/module.h>
#include <tra/types.h>
#include <tra/log.h>

/* ------------------------------------------------------- */

typedef struct tra_nvencgl tra_nvencgl;

/* ------------------------------------------------------- */

static tra_encoder_api encoder_api;

/* ------------------------------------------------------- */

struct tra_nvencgl {
  NV_ENC_INPUT_RESOURCE_OPENGL_TEX* tex_array;     /* When we register a texture we need to map it into a `tex_array`. */
  uint32_t tex_count;                              /* The number of allocated `tex_array`. */
  tra_nvenc_resources* resources;                  /* The `tra_nvencgl` uses this context to register OpenGL textures with the encoder. */
  tra_nvenc* encoder;                              /* The NVIDIA encoder around which the `tra_nvencgl` is a thin layer. */
 };

/* ------------------------------------------------------- */

static const char* encoder_get_name();
static const char* encoder_get_author();
static int encoder_create(tra_encoder_settings* cfg, void* settings, tra_encoder_object** obj);
static int encoder_destroy(tra_encoder_object* obj);
static int encoder_encode(tra_encoder_object* obj, tra_sample* sample, uint32_t type, void* data);
static int encoder_flush(tra_encoder_object* obj);

/* ------------------------------------------------------- */

static int tra_nvencgl_ensure_registered_texture(tra_nvencgl* ctx, uint32_t textureId, NV_ENC_INPUT_RESOURCE_OPENGL_TEX** registeredTexture);

/* ------------------------------------------------------- */

int tra_nvencgl_create(
  tra_encoder_settings* cfg,
  tra_nvenc_settings* settings,
  tra_nvencgl** ctx
)
{
  tra_nvenc_resources_settings res_cfg = { 0 };
  tra_nvencgl* inst = NULL;
  int r = 0;

  /* ----------------------------------------------- */
  /* Validate                                        */
  /* ----------------------------------------------- */

  if (NULL == cfg) {
    TRAE("Cannot create the `tra_nvencgl` instance as the given `tra_encoder_settings*` is NULL.");
    r = -10;
    goto error;
  }

  if (0 == cfg->image_width) {
    TRAE("Cannot create the `tra_nvencgl` instance as the given `tra_encoder_settings::image_width` is 0. ");
    r = -20;
    goto error;
  }

  if (TRA_IMAGE_FORMAT_NV12 != cfg->image_format) {
    TRAE("Cannot create the `tra_nvencgl` instance as the `tra_encoder_settings::image_format` is not `TRA_IMAGE_FORMAT_NV12`. Currently this is the only format that we support.");
    r = -30;
    goto error;
  }

  if (NULL == settings) {
    TRAE("Cannot create the `tra_nvencgl` instance as the given `tra_nvenc_settings` is NULL.");
    r = -20;
    goto error;
  }

  if (NULL == ctx) {
    TRAE("Cannot create the `tra_nvencgl` instance as the given result pointer is NULL.");
    r = -30;
    goto error;
  }

  if (NULL != *ctx) {
    TRAE("Cannot create the `tra_nvencgl` instance as the given result pointer is not NULL. Did you already create it? Or maybe forgot to initialize to NULL?");
    r = -40;
    goto error;
  }
  
  /* ----------------------------------------------- */
  /* Create                                          */
  /* ----------------------------------------------- */

  inst = calloc(1, sizeof(tra_nvencgl));
  if (NULL == inst) {
    TRAE("Cannot create the `tra_nvencgl` instance as we failed to allocate it. Out of memory?");
    r = -50;
    goto error;
  }

  settings->device_type = NV_ENC_DEVICE_TYPE_OPENGL;

  r = tra_nvenc_create(cfg, settings, &inst->encoder);
  if (r < 0) {
    TRAE("Cannot create the `tra_nvencgl` intance as we failed to create the `tra_nvenc` instance.");
    r = -60;
    goto error;
  }

  /* Create the resources manager. */
  res_cfg.enc = inst->encoder;

  r = tra_nvenc_resources_create(&res_cfg, &inst->resources);
  if (r < 0) {
    TRAE("Cannot create teh `tra_nvencgl` instance as we failed to create the `tra_nvenc_resources` context.");
    r = -70;
    goto error;
  }

  /* Finally assign. */
  *ctx = inst;

 error:

  if (r < 0) {

    if (NULL != inst) {
      tra_nvencgl_destroy(inst);
      inst = NULL;
    }

    if (NULL != ctx) {
      *ctx = NULL;
    }
  }

  return r;
}

/* ------------------------------------------------------- */

int tra_nvencgl_destroy(tra_nvencgl* ctx) {

  int result = 0;
  int r = 0;

  TRAE("@todo When a user wants to encode OpenGL textures we must first unregister them with the encoder before deallocating the texture itself. As the user owns the textures I'm not sure yet how to create a clean API for this. ");

  if (NULL == ctx) {
    TRAE("Cannot destroy the `tra_nvencgl` instance as the given pointer is NULL.");
    result -= 10;
    goto error;
  }

  /* Unregister and deallocate any registrations that we did. */
  if (NULL != ctx->resources) {
    r = tra_nvenc_resources_destroy(ctx->resources);
    if (r < 0) {
      TRAE("Failed to cleanly destroy the registrations.");
      result -= 15;
    }
  }

  if (NULL != ctx->tex_array) {
    free(ctx->tex_array);
  }

  if (NULL != ctx->encoder) {
    r = tra_nvenc_destroy(ctx->encoder);
    if (r < 0) {
      TRAE("Failed to cleanly destroy the `tra_nvenc*`.");
      result -= 20;
    }
  }

  /* Reset memory */
  ctx->tex_array = NULL;
  ctx->tex_count = 0;
  ctx->resources = NULL;
  ctx->encoder = NULL;

  /* Deallocate */
  free(ctx);
  ctx = NULL;

 error:
  return result;
}

/* ------------------------------------------------------- */

int tra_nvencgl_encode(
  tra_nvencgl* ctx,
  tra_sample* sample,
  uint32_t type,
  void* data
)
{
  NV_ENC_INPUT_RESOURCE_OPENGL_TEX* texture_registration = NULL;
  tra_nvenc_registration* encoder_registration = NULL;
  tra_memory_opengl_nv12* gl_mem = NULL;
  tra_nvenc_resource resource = { 0 };
  int r = 0;

  if (NULL == ctx) {
    TRAE("Cannot encode as the given `tra_nvencgl*` is NULL.");
    r = -10;
    goto error;
  }

  if (NULL == ctx->resources) {
    TRAE("Cannot encode as the `tra_nvencgl::resources` member is NULL.");
    r = -20;
    goto error;
  }

  if (NULL == data) {
    TRAE("Cannot encode as the given data pointer is NULL.");
    r = -30;
    goto error;
  }

  if (TRA_MEMORY_TYPE_OPENGL_NV12 != type) {
    TRAE("Cannot encode as the given input type is not supported.");
    r = -40;
    goto error;
  }

  gl_mem = (tra_memory_opengl_nv12*) data;
  
  if (0 == gl_mem->tex_id) {
    TRAE("Cannot encode as the given `tra_memory_opengl_nv12::tex_id` is 0. Texture not created?");
    r = -50;
    goto error;
  }

  if (0 == gl_mem->tex_stride) {
    TRAE("Cannot encode as the given `tra_memory_opengl_nv12::tex_stride` hasn't been set.");
    r = -60;
    goto error;
  }
  
  /* Find our texture registration, that is used to register the texture with the encoder. */
  r = tra_nvencgl_ensure_registered_texture(ctx, gl_mem->tex_id, &texture_registration);
  if (r < 0) {
    TRAE("Failed to ensure a registered texture.");
    r = -70;
    goto error;
  }
  
  /* Find the encoder registration entry. */
  resource.resource_type = NV_ENC_INPUT_RESOURCE_TYPE_OPENGL_TEX;
  resource.resource_ptr = texture_registration;
  resource.resource_stride = gl_mem->tex_stride;

  r = tra_nvenc_resources_ensure_registration(ctx->resources, &resource, &encoder_registration);
  if (r < 0) {
    TRAE("Failed to register our resource!");
    r = -70;
    goto error;
  }

  r = tra_nvenc_encode_with_registration(ctx->encoder, sample, encoder_registration);
  if (r < 0) {
    TRAE("Failed to encode the given input buffer.");
    r = -90;
    goto error;
  }

 error:
  return r;
}

/* ------------------------------------------------------- */


int tra_nvencgl_flush(tra_nvencgl* ctx) {

  int r = 0;

  if (NULL == ctx) {
    TRAE("Cannot flush the encoder as the given `tra_nvencgl*` is NULL.");
    r = -10;
    goto error;
  }

  if (NULL == ctx->encoder) {
    TRAE("Cannot flush the encoder as the `encoder` member of the given `tra_nvencgl` is NULL.");
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

/*
  
  When we want to use a OpenGL texture with the NVIDIA encoder,
  we have to creat a `NV_ENC_INPUT_RESOURCE_OPENGL_TEX` instance
  and then use that to register the texture. This function will
  check if the given textureId was already received and if we
  already have a `NV_ENC_INPUT_RESOURCE_OPENGL_TEX` instance we
  will return it in the output argument.
  
 */
static int tra_nvencgl_ensure_registered_texture(
  tra_nvencgl* ctx,
  uint32_t textureId,
  NV_ENC_INPUT_RESOURCE_OPENGL_TEX** registeredTexture
)
{
  NV_ENC_INPUT_RESOURCE_OPENGL_TEX* reg = NULL;
  uint32_t i = 0;
  int r = 0;
  
  if (NULL == ctx) {
    TRAE("Cannot ensure the registered texture as the given `tra_nvencgl` is NULL.");
    r = -10;
    goto error;
  }

  if (0 == textureId) {
    TRAE("Cannot ensure the registered texture as the given `textureId` is 0. This is an invalid texture id.");
    r = -20;
    goto error;
  }

  if (NULL == registeredTexture) {
    TRAE("Cannot ensure the registered texture as the given result pointer is NULL.");
    r = -30;
    goto error;
  }
  
  /* Check if we already have the given texture id stored. */
  for (i = 0; i < ctx->tex_count; ++i) {

    reg = ctx->tex_array + i;

    /* When we found a match, set the result and return. */
    if (reg->texture == textureId) {
      *registeredTexture = reg;
      r = 0;
      goto error;
    }
  }

  /*
    When we arrive here it means the texture hasn't registered
    yet. So, we allocate a location in our array.
  */
  if (NULL != ctx->tex_array) {

    reg = realloc(ctx->tex_array, (ctx->tex_count + 1) * sizeof(NV_ENC_INPUT_RESOURCE_OPENGL_TEX));
    if (NULL == reg) {
      TRAE("Cannot ensure the registrered texture. Failed to reallocate the `tex_array` array.");
      r = -30;
      goto error;
    }

    ctx->tex_array = reg;
    ctx->tex_count = ctx->tex_count + 1;
  }
  else {

    /* We only arrive here the first time. */
    ctx->tex_array = calloc(1, sizeof(NV_ENC_INPUT_RESOURCE_OPENGL_TEX));
    if (NULL == ctx->tex_array) {
      TRAE("Cannot ensure the registered texture as we failed to allocate the `NV_ENC_INPUT_RESOURCE_OPENGL_TEX` instance. Out of memory?");
      r = -30;
      goto error;
    }

    ctx->tex_count = 1;
  }

  /* Setup the registration. */
  reg = ctx->tex_array + (ctx->tex_count - 1);
  reg->texture = textureId;
  reg->target = GL_TEXTURE_2D; /* @todo we should make sure that this matches the texture type. */

  /* Assign the result. */
  *registeredTexture = reg;

 error:
  return r;
}

/* ------------------------------------------------------- */

static const char* encoder_get_name() {
  return "nvencgl";
}

static const char* encoder_get_author() {
  return "roxlu";
}

static int encoder_create(tra_encoder_settings* cfg, void* settings, tra_encoder_object** obj) {
  return tra_nvencgl_create(cfg, settings, (tra_nvencgl**) obj);
}

static int encoder_destroy(tra_encoder_object* obj) {
  return tra_nvencgl_destroy((tra_nvencgl*) obj);
}

static int encoder_encode(tra_encoder_object* obj, tra_sample* sample, uint32_t type, void* data) {
  return tra_nvencgl_encode((tra_nvencgl*) obj, sample, type, data);
}

static int encoder_flush(tra_encoder_object* obj) {
  return tra_nvencgl_flush((tra_nvencgl*) obj);
}

/* ------------------------------------------------------- */

int tra_load(tra_registry* reg) {

  int r = 0;

  if (NULL == reg) {
    TRAE("Cannot register the `nvencgl` encoder as the given `tra_registry*` is NULL.");
    return -10;
  }

  r = tra_registry_add_encoder_api(reg, &encoder_api);
  if (r < 0) {
    TRAE("Failed to register the `nvencgl` encoder.");
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
