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

  X264 MODULE
  ===========

  GENERAL INFO:
  
    This file creates the `x264enc` encoder that uses
    libx264. The libx264 API is simple and therefore this
    implementation is a good example of how to create a custom
    module for the Trameleon library.

 */
/* ------------------------------------------------------- */

#include <stdlib.h>
#include <stdint.h>
#include <x264.h>

#include <tra/modules/x264/x264.h>
#include <tra/registry.h>
#include <tra/module.h>
#include <tra/types.h>
#include <tra/easy.h>
#include <tra/log.h>

/* ------------------------------------------------------- */

static tra_encoder_api encoder_api;
static tra_easy_api easy_api;

/* ------------------------------------------------------- */

/* Struct that keeps track the x264 encoder instance. */
typedef struct encoder {

  /* general */
  tra_encoder_settings settings;

  /* x264 */
  x264_t* handle;
  x264_picture_t pic_in;
  x264_picture_t pic_out;
  uint32_t width;
  uint32_t height;
  
} encoder;

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

static int encoder_map_image_format(uint32_t inFormat, uint32_t* outFormat); /* This maps the image format from this library to X264. */

/* ------------------------------------------------------- */

static const char* encoder_get_name() {
  return "x264enc";
}

static const char* encoder_get_author() {
  return "roxlu";
}

/* ------------------------------------------------------- */

/* 

   The initialization of the x264_t is mostly based on the
   `example.c` from the `x264` repository. The `x264enc` module
   is a simple example wrapper for the Trameleon library.

   The `example.c` assumes that you use `x264_picture_alloc()` 
   to allocate the buffers for the picture. Therefore, the example
   will also call `x264_picture_clean()` to deallocate the internally
   allocated buffers. We don't need to do this. 
   
   As the client passes memory into the `encode()` function we
   don't have to allocate the buffers for the `x264_picture_t`.
   See `encoder_encode()` where we set points to the data that was
   passed into the function. 

 */
static int encoder_create(
  tra_encoder_settings* cfg,
  void* settings, /* Custom x264 specific settings, for now not used. */
  tra_encoder_object** obj
)
{
  tra_x264_settings* mod_cfg = NULL;
  const char* cfg_preset = "ultrafast";
  const char* cfg_profile = "baseline";
  const char* cfg_tune = "zerolatency";
  x264_param_t param = { 0 };
  uint32_t img_fmt_cfg = 0;
  uint32_t img_fmt_x264 = 0;
  encoder* inst = NULL;
  int result = 0;
  int r = 0;

  /* ----------------------------------------------- */
  /* Validate                                        */
  /* ----------------------------------------------- */

  if (NULL == obj) {
    TRAE("Cannot create the `x264enc` instance because the  given `tra_encoder_object**` is NULL.");
    r = -20;
    goto error;
  }

  if (NULL != (*obj)) {
    TRAE("Cannot create the `x264enc` instance because the given `*(tra_encoder_object**)` is not NULL. Already created?");
    r = -30;
    goto error;
  }

  if (NULL == cfg->callbacks.on_encoded_data) {
    TRAE("Cannot create the `x264enc` instance because the given `tra_encoder_settings::on_encoded_data` is NULL. ");
    r = -40;
    goto error;
  }

  if (0 == cfg->image_width) {
    TRAE("Cannot create the `x264enc` instance because the `image_width` is 0.");
    r = -50;
    goto error;
  }

  if (0 == cfg->image_height) {
    TRAE("Cannot create the `x264enc` instance because the `image_height` is 0.");
    r = -60;
    goto error;
  }

  if (0 == cfg->image_format) {
    TRAE("Cannot create the `x264enc` instance because the `image_format` is 0.");
    r = -70;
    goto error;
  }

  /* Map the image format from the Trameleon type to the x264 type. */
  r = encoder_map_image_format(cfg->image_format, &img_fmt_x264);
  if (r < 0) {
    TRAE("Cannot create the `x264enc` instance as the image format is not supported.");
    r = -100;
    goto error;
  }

  /* ----------------------------------------------- */
  /* Create                                          */
  /* ----------------------------------------------- */
  
  inst = calloc(1, sizeof(encoder));
  if (NULL == inst) {
    TRAE("Cannot create a `x264enc` instance because we failed to allocate context data.");
    r = -110;
    goto error;
  }

  inst->settings = *cfg;

  /* Allow the user to override our defaults. */
  mod_cfg = (tra_x264_settings*) settings;
  if (NULL != mod_cfg) {

    if (NULL != mod_cfg->preset) {
      cfg_preset = mod_cfg->preset;
    }
    
    if (NULL != mod_cfg->profile) {
      cfg_profile = mod_cfg->profile;
    }

    if (NULL != mod_cfg->tune) {
      cfg_tune = mod_cfg->tune;
    }
  }
  
  /* Load default params. */
  r = x264_param_default_preset(&param, cfg_preset, cfg_tune);
  if (r < 0) {
    TRAE("Cannot create the `x264enc` instance because we failed to create the default preset.");
    r = -120;
    goto error;
  }

  /* !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! */
  /*
    IMPORRTANT: all the settings below should
    be configurable. Do not assume that these
    hardcoded values will stay the same. In the
    future we'll allow the user to have fine
    grained control over the encoder
    settings.
  */
  /* !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! */
  
  /* Configure non-default params. */
  param.i_bitdepth = 8;
  param.i_csp = img_fmt_x264;
  param.i_width = cfg->image_width;
  param.i_height = cfg->image_height;
  param.b_vfr_input = 0;
  param.b_repeat_headers = 1;
  param.b_annexb = 0;
  param.i_keyint_max = 25;

  /* Apply profile restrictions. */
  r = x264_param_apply_profile(&param, cfg_profile);
  if (r < 0) {
    TRAE("Cannot create the `x264enc` instance because we failed to apply the profile restrictions.");
    r = -130;
    goto error;
  }

  /* Only initialize and set some members. See function description. */
  x264_picture_init(&inst->pic_in);
  x264_picture_init(&inst->pic_out);

  inst->pic_in.img.i_csp = img_fmt_x264;
  inst->pic_in.img.i_plane = 2;
  
  inst->handle = x264_encoder_open(&param);
  if (NULL == inst->handle) {
    TRAE("Cannot create the `x264enc` instance because we failed to open the x264 encoder.");
    r = -140;
    goto error;
  }

  inst->width = param.i_width;
  inst->height = param.i_height;

  /* Finally assign the output variable. */
  *obj = (tra_encoder_object*)inst;

 error:

  if (r < 0) {
    
    if (NULL != inst) {
      
      result = encoder_destroy((tra_encoder_object*)inst);
      if (result < 0) {
        TRAE("After failing to create a x264 encoder context, we failed to cleanly destroy it.");
      }
      
      inst = NULL;
    }

    if (NULL != obj) {
      *obj = NULL;
    }
  }
      
  return r;
}

/* ------------------------------------------------------- */

/*

  Destroy the encoder instance. Note that we don't have call
  `x264_picture_clean()` to deallocate the `pic_in` member. We
  did not use `x264_picture_alloc()`, so it doesn't have internal
  data that needs to be free'd.  The `pic_out` member points to
  data that is managed by the encoder itself; no need to clean
  that either.

 */
static int encoder_destroy(tra_encoder_object* obj) {

  encoder* ctx = NULL;

  if (NULL == obj) {
    TRAE("Cannot cleanly destroy the x264 encoder as the given `tra_encoder_object*` is NULL.");
    return -1;
  }

  ctx = (encoder*) obj;

  if (NULL != ctx->handle) {
    x264_encoder_close(ctx->handle);
  }

  ctx->handle = NULL;
  
  free(obj);
  obj = NULL;

  return 0;
}

/* ------------------------------------------------------- */

static int encoder_encode(tra_encoder_object* obj, tra_sample* sample, uint32_t type, void* data) {

  tra_memory_h264 encoded_data = { 0 };
  tra_memory_image* input_image = NULL;
  x264_nal_t* nal_ptrs = NULL;
  encoder* ctx = NULL;
  int nal_count = 0;
  int frame_size = 0;

  if (NULL == obj) {
    TRAE("Cannot encode using x264, given encoder instance is NULL.");
    return -1;
  }

  if (NULL == sample) {
    TRAE("Cannot encode using x264, given sample is NULL.");
    return -2;
  }
  
  if (TRA_MEMORY_TYPE_IMAGE != type) {
    TRAE("Cannot encode using x264, we only support the `TRA_MEMORY_TYPE_IMAGE` input type.");
    return -3;
  }

  if (NULL == data) {
    TRAE("Cannot encode using x264, the given `data` is NULL.");
    return -4;
  }
  
  input_image = (tra_memory_image*) data;
  if (TRA_IMAGE_FORMAT_I420 != input_image->image_format
      && TRA_IMAGE_FORMAT_NV12 != input_image->image_format
      && TRA_IMAGE_FORMAT_YUYV != input_image->image_format)
    {
      TRAE("Cannot encode using x264, unsupported format.");
      return -5;
    }

  ctx = (encoder*) obj;
  if (NULL == ctx->handle) {
    TRAE("Cannot encode using x264, the `encoder::handle` member is NULL. Did you create the instance?");
    return -6;
  }

  if (0 == ctx->width) {
    TRAE("Cannot encode using x264, the `width` is not set on the context.");
    return -7;
  }
  
  if (0 == ctx->height) {
    TRAE("Cannot encode using x264, the `height` is not set on the context.");
    return -8;
  }
  
  /* Setup the input picture. */
  ctx->pic_in.img.plane[0] = input_image->plane_data[0];
  ctx->pic_in.img.plane[1] = input_image->plane_data[1];
  ctx->pic_in.img.plane[2] = input_image->plane_data[2];
  ctx->pic_in.img.i_stride[0] = input_image->plane_strides[0];
  ctx->pic_in.img.i_stride[1] = input_image->plane_strides[1];
  ctx->pic_in.img.i_stride[2] = input_image->plane_strides[2];
  ctx->pic_in.img.i_stride[3] = 0;
  ctx->pic_in.i_pts = sample->pts;

  /* Encode */
  frame_size = x264_encoder_encode(
    ctx->handle,
    &nal_ptrs,
    &nal_count,
    &ctx->pic_in,
    &ctx->pic_out
  );

  if (frame_size < 0) {
    TRAE("Cannot encode using x264, failed to encode a frame.");
    return -9;
  }

  if (frame_size > 0) {

    encoded_data.size = frame_size;
    encoded_data.data = nal_ptrs->p_payload;
    
    ctx->settings.callbacks.on_encoded_data(
      TRA_MEMORY_TYPE_H264,
      &encoded_data,
      ctx->settings.callbacks.user
    );
  }

  return 0;
}

/* ------------------------------------------------------- */

static int encoder_flush(tra_encoder_object* obj) {

  int r = 0;

  TRAE("@todo implement the x264 flush!");

 error:
  return r;
}

/* ------------------------------------------------------- */

static int easy_encoder_create(tra_easy* ez, tra_encoder_settings* cfg, void** enc) {

  tra_encoder_object* inst = NULL;
  int r = 0;
  
  if (NULL == enc) {
    TRAE("Cannot create the x264 easy encoder as the given output argument is NULL.");
    r = -10;
    goto error;
  }

  r = encoder_create(cfg, NULL, &inst);
  if (r < 0) {
    TRAE("Failed to create the x264 easy encoder.");
    r = -20;
    goto error;
  }

  /* Assign */
  *enc = inst;
  
 error:
  
  if (r < 0) {
    
    if (NULL != inst) {
      encoder_destroy(inst);
      inst = NULL;
    }

    if (NULL != enc) {
      *enc = NULL;
    }
  }
  
  return r;
}

/* ------------------------------------------------------- */

static int easy_encoder_encode(void* enc, tra_sample* sample, uint32_t type, void* data) {
  return encoder_encode(enc, sample, type, data);
}

/* ------------------------------------------------------- */

static int easy_encoder_flush(void* enc) {

  int r = 0;

 error:
  return r;
}

/* ------------------------------------------------------- */

static int easy_encoder_destroy(void* enc) {
  return encoder_destroy(enc);
}

/* ------------------------------------------------------- */

/* 
   This maps the image format from this library to X264. This
   function will return < 0 in case of an error OR when the
   `inFormat` is not supported.
*/
static int encoder_map_image_format(uint32_t inFormat, uint32_t* outFormat) {

  if (NULL == outFormat) {
    TRAE("Cannot map the input format to a X264, the given `outFormat*` is NULL.");
    return -1;
  }

  switch (inFormat) {
    
    case TRA_IMAGE_FORMAT_I420: {
      *outFormat = X264_CSP_I420;
      return 0;
    }
    
    case TRA_IMAGE_FORMAT_NV12: {
      *outFormat = X264_CSP_NV12;
      return 0;
    }

    case TRA_IMAGE_FORMAT_YUYV: {
      *outFormat = X264_CSP_YUYV;
      return 0;
    }
  };

  TRAE("Cannot map the input format to a X264 format, the unhandled ");
  return -2;
}

/* ------------------------------------------------------- */

/* 
   The `tra_load()` function is called when the x264 module is
   loaded as a shared library. Here we register the available
   plugins, which is only the encoder in case of x264. Other
   modules might also register decoders, scalers, etc.
*/
int tra_load(tra_registry* reg) {

  int r = 0;
  
  r = tra_registry_add_encoder_api(reg, &encoder_api);
  if (r < 0) {
    TRAE("Failed to register the `x264` encoder.");
    return -1;
  }

  r = tra_registry_add_easy_api(reg, &easy_api);
  if (r < 0) {
    TRAE("Failed to register the `x264` easy encoder.");
    return -30;
  }

  TRAI("Registered the `x264enc` plugin.");
  
  return 0;
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

static tra_easy_api easy_api = {
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
  
