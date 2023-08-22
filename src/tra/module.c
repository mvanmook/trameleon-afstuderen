/* ------------------------------------------------------- */

#include <stdlib.h>
#include <tra/log.h>
#include <tra/module.h>

/* ------------------------------------------------------- */

/* 
   The private `tra_encoder` type combines the API and the state
   that the API needs to keep; the state is kept in an opaque
   `tra_encoder_object` member. The `tra_encoder_object` can be set
   to anything an API needs for an encoder session.
 */
struct tra_encoder {
  tra_encoder_api* api;
  tra_encoder_object* obj;
};

/* ------------------------------------------------------- */

struct tra_decoder {
  tra_decoder_api* api;
  tra_decoder_object* obj;
};

/* ------------------------------------------------------- */

struct tra_graphics {
  tra_graphics_api* api;
  tra_graphics_object* obj;
};

/* ------------------------------------------------------- */

struct tra_interop {
  tra_interop_api* api;
  tra_interop_object* obj;
};

/* ------------------------------------------------------- */

struct tra_converter {
  tra_converter_api* api;
  tra_converter_object* obj;
};

/* ------------------------------------------------------- */

int tra_encoder_create(
  tra_encoder_api* api,
  tra_encoder_settings* cfg,
  void* settings, 
  tra_encoder** enc
)
{
  tra_encoder* inst = NULL;
  int result = 0;
  int r = 0;
  
  if (NULL == api) {
    TRAE("Cannot create an encoder instance. Given `tra_encoder_api*` is NULL.");
    return -1;
  }

  if (NULL == enc) {
    TRAE("Cannot create the encoder, given result `tra_encoder**` is NULL.");
    return -2;
  }

  if (NULL != (*enc)) {
    TRAE("Cannot create the encoder, given `*(tra_encoder**)` result is not initialized to NULL.");
    return -3;
  }

  inst = calloc(1, sizeof(tra_encoder));
  if (NULL == inst) {
    TRAE("Cannot create the encoder, failed to allocate the `tra_encoder`.");
    return -4;
  }

  inst->api = api;

  r = api->create(cfg, settings, &inst->obj);
  if (r < 0) {
    TRAE("Cannot create the encoder, the encoder api failed to create the instance.");
    r = -5;
    goto error;
  }

  /* Finally assign the result. */
  *enc = inst;

 error:

  if (r < 0) {
    
    if (NULL != inst) {
      
      result = tra_encoder_destroy(inst);
      if (result < 0) {
        TRAE("After we've failed to create the encoder, we also failed to cleanly destroy it.");
      }

      inst = NULL;
    }

    if (NULL != enc) {
      *enc = NULL;
    }
  }

  return r;
}

/* ------------------------------------------------------- */

int tra_encoder_destroy(tra_encoder* enc) {

  int result = 0;
  int r = 0;
    
  if (NULL == enc) {
    TRAE("Cannot destroy the encoder instance. Given `tra_encoder*` is NULL.");
    return -10;
  }

  /* Cleanup the encoder instance. */
  if (NULL != enc->api) {
    r = enc->api->destroy(enc->obj);
    if (r < 0) {
      TRAE("Failed to cleanly destroy the encoder.");
      result -= 20;
    }
  }

  enc->obj = NULL;
  enc->api = NULL;

  free(enc);
  enc = NULL;

  return result;
}

/* ------------------------------------------------------- */

int tra_encoder_encode(tra_encoder* enc, tra_sample* sample, uint32_t type, void* data) {

  if (NULL == enc) {
    TRAE("Cannot encode a video frame. The `api` member of the `tra_encoder*` is NULL.");
    return -10;
  }

  return enc->api->encode(enc->obj, sample, type, data);
}

/* ------------------------------------------------------- */

int tra_encoder_flush(tra_encoder* enc) {

  if (NULL == enc) {
    TRAE("Cannot flush the encoder as the given `tra_encoder*` is NULL.");
    return -10;
  }

  return enc->api->flush(enc->obj);
}

/* ------------------------------------------------------- */

int tra_decoder_create(
  tra_decoder_api* api,
  tra_decoder_settings* cfg,
  void* settings,
  tra_decoder** dec
)
{
  
  tra_decoder* inst = NULL;
  int result = 0;
  int r = 0;
  
  if (NULL == api) {
    TRAE("Cannot create a decoder instance. Given `tra_decoder_api*` is NULL.");
    return -1;
  }

  if (NULL == dec) {
    TRAE("Cannot create a decoder instance. Given `tra_decoder**` is NULL.");
    return -2;
  }

  if (NULL != (*dec)) {
    TRAE("Cannot create a decoder instance. Given `tra_decoder**` result is not initialized to NULL.");
    return -3;
  }

  inst = calloc(1, sizeof(tra_decoder));
  if (NULL == inst) {
    TRAE("Cannot create the decoder, failed to allocate the `tra_decoder`.");
    return -4;
  }

  inst->api = api;

  r = api->create(cfg, settings, &inst->obj);
  if (r < 0) {
    TRAE("Failed to create the decoder, the decoder api failed to create the instance.");
    r = -5;
    goto error;
  }

  /* Finally assign the result. */
  *dec = inst;

 error:

  if (r < 0) {

    if (NULL != inst) {
      
      result = tra_decoder_destroy(inst);
      if (result < 0) {
        TRAE("After we've failed to create the decoder, we also failed to cleanly destroy it.");
      }

      inst = NULL;
    }

    if (NULL != dec) {
      *dec = NULL;
    }
  }

  return r;
}

int tra_decoder_destroy(tra_decoder* dec) {

  int status = 0;
  int r = 0;
  
  if (NULL == dec) {
    TRAE("Cannot destroy the decoder. Given `tra_decoder*` is NULL.");
    return -1;
  }

  if (NULL != dec->api) {
    r = dec->api->destroy(dec->obj);
    if (r < 0) {
      TRAE("Failed to cleanly destroy the decoder instance.");
      status -= 10;
    }
  }

  dec->obj = NULL;
  dec->api = NULL;

  free(dec);
  dec = NULL;

  return status;
}

int tra_decoder_decode(tra_decoder* dec, uint32_t type, void* data) {

  if (NULL == dec) {
    TRAE("Cannot decode as the given `tra_decoder*` is NULL.");
    return -1;
  }

  if (NULL == data) {
    TRAE("Cannot decode as the given `data*` is NULL.");
    return -2;
  }

  return dec->api->decode(dec->obj, type, data);
}

/* ------------------------------------------------------- */

int tra_graphics_create(
  tra_graphics_api* api,
  tra_graphics_settings* cfg,
  void* settings,
  tra_graphics** gfx
)
{

  tra_graphics* inst = NULL;
  int result = 0;
  int r = 0;
  
  if (NULL == api) {
    TRAE("Cannot create a graphics instance. Given `tra_graphics_api*` is NULL.");
    r = -10;
    goto error;
  }

  if (NULL == gfx) {
    TRAE("Cannot create a graphics instance. Given `tra_graphics**` is NULL.");
    r = -20;
    goto error;
  }

  if (NULL != (*gfx)) {
    TRAE("Cannot create a graphics intance. Given `*(tra_graphics**)` is not NULL. Did you already create it or forgot to initialize the value to NULL?");
    r = -30;
    goto error;
  }

  inst = calloc(1, sizeof(tra_graphics));
  if (NULL == inst) {
    TRAE("Cannot create an graphics instance. Failed to allocate an instancde of `tra_graphics`.");
    r = -40;
    goto error;
  }

  inst->api = api;

  r = api->create(cfg, settings, &inst->obj);
  if (r < 0) {
    TRAE("Failed to create the graphics instance.");
    r = -50;
    goto error;
  }

  /* Finally assign. */
  *gfx = inst;

 error:

  if (r < 0) {
    
    if (NULL != inst) {
      result = tra_graphics_destroy(inst);
      if (result < 0) {
        TRAE("After we've failed to create the graphics instance, we also failed to cleanly destroy it.");
      }
    }

    inst = NULL;
  }
  
  return r;
}

int tra_graphics_destroy(tra_graphics* ctx) {

  TRAE("@todo destroy the graphics.");
}

int tra_graphics_draw(tra_graphics* ctx, uint32_t type, void* data) {

  int r = 0;
  
  if (NULL == ctx) {
    TRAE("Cannot draw using the given `tra_graphics*` as it's NULL.");
    r = -10;
    goto error;
  }

  if (NULL == ctx->api) {
    TRAE("Cannot draw using the given `tra_graphics` as it's `api` member is NULL.");
    r = -20;
    goto error;
  }

  if (NULL == ctx->obj) {
    TRAE("Cannot draw using the givne `tra_graphics` as the `obj` instance is NULL. Not created?");
    r = -30;
    goto error;
  }

  r = ctx->api->draw(ctx->obj, type, data);
  if (r < 0) {
    TRAE("Something went wrong while trying to draw.");
    r = -40;
    goto error;
  }

 error:

  return r;
}
 
/* ------------------------------------------------------- */

int tra_interop_create(
  tra_interop_api* api,
  tra_interop_settings* cfg,
  void* settings,
  tra_interop** ctx
)
{

  tra_interop* inst = NULL;
  int result = 0;
  int r = 0;
  
  if (NULL == api) {
    TRAE("Cannot create a interop instance. Given `tra_interop_api*` is NULL.");
    r = -10;
    goto error;
  }

  if (NULL == ctx) {
    TRAE("Cannot create a interop instance. Given `tra_interop**` is NULL.");
    r = -20;
    goto error;
  }

  if (NULL != (*ctx)) {
    TRAE("Cannot create a interop intance. Given `*(tra_interop**)` is not NULL. Did you already create it or forgot to initialize the value to NULL?");
    r = -30;
    goto error;
  }

  inst = calloc(1, sizeof(tra_interop));
  if (NULL == inst) {
    TRAE("Cannot create an interop instance. Failed to allocate an instancde of `tra_interop`.");
    r = -40;
    goto error;
  }

  inst->api = api;

  r = api->create(cfg, settings, &inst->obj);
  if (r < 0) {
    TRAE("Failed to create the interop instance.");
    r = -50;
    goto error;
  }

  /* Finally assign. */
  *ctx = inst;

 error:

  if (r < 0) {
    
    if (NULL != inst) {
      result = tra_interop_destroy(inst);
      if (result < 0) {
        TRAE("After we've failed to create the interop instance, we also failed to cleanly destroy it.");
      }
    }

    inst = NULL;
  }
  
  return r;
}

/* ------------------------------------------------------- */

int tra_interop_destroy(tra_interop* ctx) {

  TRAE("@todo destroy the interop.");
}

/* ------------------------------------------------------- */

/* Upload the data you received from e.g. a decoder using the given interop api. */
int tra_interop_upload(tra_interop* ctx, uint32_t type, void* data) {

  int r = 0;
  
  if (NULL == ctx) {
    TRAE("Cannot upload the given data using the interop API as the given `tra_interop_api*` is NULL.");
    r = -10;
    goto error;
  }

  if (NULL == ctx->api) {
    TRAE("Cannot upload the given data as the interop API is NULL. Did you create an instance of a specific interop api?");
    r = -20;
    goto error;
  }

  if (NULL == ctx->obj) {
    TRAE("Cannot upload the given data as the interop API instance is NULL. Did you successfully create it?");
    r = -30;
    goto error;
  }

  if (NULL == data) {
    TRAE("Cannot upload the given data using the interop API as the given data pointer is NULL.");
    r = -40;
    goto error;
  }

  r = ctx->api->upload(ctx->obj, type, data);
  if (r < 0) {
    TRAE("Failed to upload the interop using the interop API. ");
    r = -50;
    goto error;
  }

 error:

  return r;
}

/* ------------------------------------------------------- */

int tra_converter_create(
  tra_converter_api* api,
  tra_converter_settings* cfg,
  void* settings, 
  tra_converter** ctx
)
{
  tra_converter* inst = NULL;
  int result = 0;
  int r = 0;
  
  if (NULL == api) {
    TRAE("Cannot create an converter instance. Given `tra_converter_api*` is NULL.");
    return -1;
  }

  if (NULL == ctx) {
    TRAE("Cannot create the converter, given result `tra_converter**` is NULL.");
    return -2;
  }

  if (NULL != (*ctx)) {
    TRAE("Cannot create the converter, given `*(tra_converter**)` result is not initialized to NULL.");
    return -3;
  }

  inst = calloc(1, sizeof(tra_converter));
  if (NULL == inst) {
    TRAE("Cannot create the converter, failed to allocate the `tra_converter`.");
    return -4;
  }

  inst->api = api;

  r = api->create(cfg, settings, &inst->obj);
  if (r < 0) {
    TRAE("Cannot create the converter, the converter api failed to create the instance.");
    r = -5;
    goto error;
  }

  /* Finally assign the result. */
  *ctx = inst;

 error:

  if (r < 0) {
    
    if (NULL != inst) {
      
      result = tra_converter_destroy(inst);
      if (result < 0) {
        TRAE("After we've failed to create the converter, we also failed to cleanly destroy it.");
      }

      inst = NULL;
    }

    if (NULL != ctx) {
      *ctx = NULL;
    }
  }

  return r;
}

int tra_converter_destroy(tra_converter* ctx) {

  int status = 0;
  int r = 0;
    
  if (NULL == ctx) {
    TRAE("Cannot destroy the converter instance. Given `tra_converter*` is NULL.");
    return -10;
  }

  /* Cleanup the converter instance. */
  if (NULL != ctx->api) {
    r = ctx->api->destroy(ctx->obj);
    if (r < 0) {
      TRAE("Failed to cleanly destroy the converter.");
      status -= 20;
    }
  }

  ctx->obj = NULL;
  ctx->api = NULL;

  free(ctx);
  ctx = NULL;

  return status;
}

int tra_converter_convert(tra_converter* ctx, uint32_t type, void* data) {

  int r = 0;
  
  if (NULL == ctx) {
    TRAE("Cannot convert as the given converter is NULL.");
    return -10;
  }

  if (NULL == ctx->api) {
    TRAE("Cannot convert as the `tra_converter::api` is NULL. did you create the converter sucessfully?");
    r = -20;
    goto error;
  }

  r = ctx->api->convert(ctx->obj, type, data);
  if (r < 0) {
    TRAE("Failed to convert.");
    r = -30;
    goto error;
  }

 error:
  return r;
}

/* ------------------------------------------------------- */
