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

  NETINT MODULE
  =============

  GENERAL INFO:
  
    This file provides the `nienc` and "nidec" encoder and
    decoder. This module uses the [NETINT XCoder][0] library.

  REFERENCES:

   [0]: https://netint.com/video-transcoder/ "NETINT Transcoder"

 */

/* ------------------------------------------------------- */

#include <stdlib.h>
#include <tra/modules/netint/netint-dec.h>
#include <tra/modules/netint/netint-enc.h>
#include <tra/registry.h>
#include <tra/module.h>
#include <tra/types.h>
#include <tra/log.h>

/* ------------------------------------------------------- */

typedef struct encoder {
  netint_enc* enc;  
} encoder;

/* ------------------------------------------------------- */

typedef struct decoder {
  netint_dec* dec;  
} decoder;

/* ------------------------------------------------------- */

static tra_encoder_api encoder_api;
static tra_decoder_api decoder_api;

/* ------------------------------------------------------- */

static const char* encoder_get_name();
static const char* encoder_get_author();
static int encoder_create(tra_encoder_settings* cfg, void* settings, tra_encoder_object** obj);
static int encoder_destroy(tra_encoder_object* obj);
static int encoder_encode(tra_encoder_object* obj, tra_sample* sample, uint32_t type, void* data);

/* ------------------------------------------------------- */

static const char* decoder_get_name();
static const char* decoder_get_author();
static int decoder_create(tra_decoder_settings* cfg, void* settings, tra_decoder_object** obj);
static int decoder_destroy(tra_decoder_object* obj);
static int decoder_decode(tra_decoder_object* obj, uint32_t type, void* data);
 
/* ------------------------------------------------------- */

static const char* encoder_get_name() {
  return "nienc";
}

static const char* encoder_get_author() {
  return "roxlu";
}

/* ------------------------------------------------------- */

static int encoder_create(
  tra_encoder_settings* cfg,
  void* settings,
  tra_encoder_object** obj
)
{
  encoder* inst = NULL;
  int status = 0;
  int r = 0;

  if (NULL == cfg) {
    TRAE("Cannot create the `nienc` because the given config is NULL.");
    r = -10;
    goto error;
  }

  if (NULL == obj) {
    TRAE("Cannot create the `nienc` as the given output `obj` is NULL.");
    r = -20;
    goto error;
  }

  if (NULL != (*obj)) {
    TRAE("Cannot crete the `nienc` as the given output `*obj` is not NULL. Already created?");
    r = -30;
    goto error;
  }

  if (0 == cfg->image_width) {
    TRAE("Cannot create the `nienc` as the `image_width` is 0.");
    r = -40;
    goto error;
  }

  if (0 == cfg->image_height) {
    TRAE("Cannot create the `nienc` as the `image_height is 0.");
    r = -50;
    goto error;
  }

  if (0 == cfg->image_format) {
    TRAE("Cannot create the `nienc` as the `image_format` is 0.");
    r = -60;
    goto error;
  }

  inst = calloc(1, sizeof(encoder));
  if (NULL == inst) {
    TRAE("Cannot create the `nienc` as we failed to allocate the `encoder` context.");
    r = -70;
    goto error;
  }

  r = netint_enc_create(cfg, &inst->enc);
  if (r < 0) {
    TRAE("Cannot create the `nienc` as we failed to create the `netint_enc` instance.");
    r = -90;
    goto error;
  }

  /* Assign the result */
  *obj = (tra_encoder_object*) inst;

 error:
  
  if (r < 0) {
    
    if (NULL != inst) {
      
      status = encoder_destroy((tra_encoder_object*)inst);
      if (status < 0) {
        TRAE("After we failed to create the encoder we also failed to cleanly shut it down.");
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

static int encoder_destroy(tra_encoder_object* obj) {

  encoder* ctx = NULL;
  int result = 0;
  int r = 0;

  if (NULL == obj) {
    TRAE("Cannot cleanly destroy `nienc` as the given `tra_encoder_object*` is NULL.");
    return -1;
  }

  ctx = (encoder*) obj;

  if (NULL != ctx->enc) {
    r = netint_enc_destroy(ctx->enc);
    if (r < 0) {
      TRAE("Failed to cleanly destroy the `nienc`.");
      result -= 10;
    }
  }

  ctx->enc = NULL;

  free(obj);
  obj = NULL;

  return result;
}

/* ------------------------------------------------------- */

static int encoder_encode(
  tra_encoder_object* obj,
  tra_sample* sample,
  uint32_t type,
  void* data
)
{
  encoder* ctx = NULL;
  int r = 0;

  if (NULL == obj) {
    TRAE("Cannot encode using `nienc` as the given `tra_encoder_object*` is NULL.");
    r = -10;
    goto error;
  }

  if (NULL == sample) {
    TRAE("Cannot encode using `nienc` as the given `tra_sample*` is NULL`. ");
    r = -20;
    goto error;
  }

  if (TRA_MEMORY_TYPE_IMAGE != type) {
    TRAE("Cannot encode using `nienc` as the given input type is not supported.");
    r = -30;
    goto error;
  }

  if (NULL == data) {
    TRAE("Cannot encode using `nienc` as the given `data` is NULL.");
    r = -40;
    goto error;
  }

  ctx = (encoder*) obj;

  r = netint_enc_encode(ctx->enc, sample, type, data);
  if (r < 0) {
    TRAE("Cannot encode using `nienc`, something went wrong when calling `va_enc_encode()`.");
    r = -50;
    goto error;
  }
  
 error:
  return r;
}

/* ------------------------------------------------------- */

static const char* decoder_get_name() {
  return "nidec";
}

static const char* decoder_get_author() {
  return "roxlu";
}

/* ------------------------------------------------------- */

static int decoder_create(
  tra_decoder_settings* cfg,
  void* settings,
  tra_decoder_object** obj
)
{
  decoder* inst = NULL;
  int result = 0;
  int r = 0;
  
  if (NULL == cfg) {
    TRAE("Cannot create the `nidec` instance because the given config is NULL.");
    r = -10;
    goto error;
  }

  if (NULL == obj) {
    TRAE("Cannot create the `nidec` instance as the given `obj` is NULL.");
    r = -20;
    goto error;
  }

  if (NULL != (*obj)) {
    TRAE("Cannot create the `nidec` instance as the given `*obj` is not NULL. Already created?");
    r = -30;
    goto error;
  }

  inst = calloc(1, sizeof(decoder));
  if (NULL == inst) {
    TRAE("Cannot create the `nidec` instance as we failed to allocate the decoder context.");
    r = -40;
    goto error;
  }

  r = netint_dec_create(cfg, &inst->dec);
  if (r < 0) {
    TRAE("Cannot create the `nidec` instance as we failed to create the `netint_dec` instance.");
    r = -50;
    goto error;
  }

  /* Assign the result. */
  *obj = (tra_decoder_object*) inst;
  
 error:

  if (r < 0) {
    
    if (NULL != inst) {
      
      result = decoder_destroy((tra_decoder_object*) inst);
      if (result < 0) {
        TRAE("After we failed to create the `nidec` instance we also failed to cleanly destroy it.");
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

static int decoder_destroy(tra_decoder_object* obj) {

  decoder* ctx = NULL;
  int result = 0;
  int r = 0;

  if (NULL == obj) {
    TRAE("Cannot cleanly destroy the `nidec` instance as the given `tra_decoder_object*` is NULL.");
    return -1;
  }

  ctx = (decoder*) obj;

  if (NULL != ctx->dec) {
    r = netint_dec_destroy(ctx->dec);
    if (r < 0) {
      TRAE("Failed to cleanly destroy the `netint_dec` instance.");
      result -= 10;
    }
  }

  ctx->dec = NULL;

  free(obj);
  obj = NULL;
  
  return result;
}

/* ------------------------------------------------------- */

static int decoder_decode(tra_decoder_object* obj, uint32_t type, void* data) {

  decoder* ctx = NULL;
  int r = 0;

  if (NULL == obj) {
    TRAE("Cannot decode as the given `tra_decoder_object*` is NULL.");
    r = -10;
    goto error;
  }

  if (TRA_MEMORY_TYPE_HOST_H264 != type) {
    TRAE("Cannot decode as the given `type` is not supported.");
    r = -20;
    goto error;
  }

  if (NULL == data) {
    TRAE("Cannot decode as the given `data` is NULL.");
    r = -30;
    goto error;
  }

  ctx = (decoder*) obj;
  if (NULL == ctx->dec) {
    TRAE("Cannot decode as the `dec` memmber of the context is NULL.");
    r = -40;
    goto error;
  }

  r = netint_dec_decode(ctx->dec, type, data);
  if (r < 0) {
    TRAE("Cannot decode using `nidec`, something went wrong while calling `netint_dec_decode()`.");
    r = -50;
    goto error;
  }

 error:
  return r;
}

/* ------------------------------------------------------- */

/* 
   When Trameleon is linked dynamically we load modules when we
   create the `tra_core` instance. This will scan a directory
   with libraries. For each lib, it will call `tra_load()`. This
   gives us the opportunity to register the plugins that this
   module provides. In this case we register the `nienc` and
   `nidec` plugins.
 */
int tra_load(tra_registry* reg) {

  int r = 0;
  
  if (NULL == reg) {
    TRAE("Cannot load the netint which provides `nienc` and `nidec` as the given `tra_registry` is NULL.");
    return -1;
  }

  r = tra_registry_add_encoder_api(reg, &encoder_api);
  if (r < 0) {
    TRAE("Failed to register the `nienc` plugin.");
    return -2;
  }

  r = tra_registry_add_decoder_api(reg, &decoder_api);
  if (r < 0) {
    TRAE("Failed to register the `nidec` plugin.");
    return -3;
  }

  TRAI("Registered the `nienc` plugin.");
  TRAI("Registered the `nidec` plugin.");

  return 0;
}

/* ------------------------------------------------------- */

static tra_encoder_api encoder_api = {
  .get_name = encoder_get_name,
  .get_author = encoder_get_author,
  .create = encoder_create,
  .destroy = encoder_destroy,
  .encode = encoder_encode
};

/* ------------------------------------------------------- */

static tra_decoder_api decoder_api = {
  .get_name = decoder_get_name,
  .get_author = decoder_get_author,
  .create = decoder_create,
  .destroy = decoder_destroy,
  .decode = decoder_decode
};

/* ------------------------------------------------------- */
