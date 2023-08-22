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

  MEDIA FOUNDATION TRANSFORM MODULE
  =================================

  GENERAL INFO:

    This file provides the `mftenc` and `mftdec` encoder and
    decoder. This module uses [Media Foundation Transforms][0].

  REFERENCES:

    [0]: https://docs.microsoft.com/en-us/windows/win32/medfound/about-mfts "About MFTs"
  
 */
 
/* ------------------------------------------------------- */

extern "C" {  
#  include <tra/registry.h>
#  include <tra/module.h>
#  include <tra/types.h>
#  include <tra/log.h>
#  include <tra/api.h>
}

#include <tra/modules/mft/MediaFoundationEncoder.h>
#include <tra/modules/mft/MediaFoundationDecoder.h>

/* ------------------------------------------------------- */

typedef struct encoder {
  tra::MediaFoundationEncoder enc;
} encoder;

/* ------------------------------------------------------- */

typedef struct decoder {
  tra::MediaFoundationDecoder dec;
} decoder;

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
  return "mftenc";
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
  tra::MediaFoundationEncoderSettings enc_cfg = { 0 };
  encoder* inst = NULL;
  int status = 0;
  int r = 0;

  if (NULL == cfg) {
    TRAE("Cannot create the `mftenc` because the given config is NULL.");
    r = -10;
    goto error;
  }

  if (NULL == obj) {
    TRAE("Cannot create the `mftenc` as the given output `obj` is NULL.");
    r = -20;
    goto error;
  }

  if (NULL != (*obj)) {
    TRAE("Cannot crete the `mftenc` as the given output `*obj` is not NULL. Already created?");
    r = -40;
    goto error;
  }

  if (0 == cfg->image_width) {
    TRAE("Cannot create the `mftenc` as the `image_width` is 0.");
    r = -50;
    goto error;
  }

  if (0 == cfg->image_height) {
    TRAE("Cannot create the `mftenc` as the `image_height is 0.");
    r = -60;
    goto error;
  }

  if (0 == cfg->image_format) {
    TRAE("Cannot create the `mftenc` as the `image_format` is 0.");
    r = -70;
    goto error;
  }

  inst = (encoder*) calloc(1, sizeof(encoder));
  if (NULL == inst) {
    TRAE("Cannot create the `mftenc`, because we failed to allocate the `encoder` context.");
    r = -80;
    goto error;
  }

  enc_cfg.on_encoded_data = cfg->callbacks.on_encoded_data;
  enc_cfg.user = cfg->callbacks.user;
  enc_cfg.image_format = cfg->image_format;
  enc_cfg.image_width = cfg->image_width;
  enc_cfg.image_height = cfg->image_height;
  enc_cfg.bitrate = 700e3;
  enc_cfg.fps_num = 1;
  enc_cfg.fps_den = 25;

  r = inst->enc.init(enc_cfg);
  if (r < 0) {
    TRAE("Cannot create the `mftenc` as we failed to initialize the `MediaFoundationEncoder`.");
    r = -90;
    goto error;
  }

  /* Assign the result. */
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
    TRAE("Cannot cleanly destroy the `mftenc` as the given `tra_encoder_object*` is NULL.");
    return -1;
  }

  ctx = (encoder*) obj;

  r = ctx->enc.shutdown();
  if (r < 0) {
    TRAE("Cannot cleanly destroy the `mftenc` as the `MediaFoundationEncoder::shutdown()` returned an error.");
    result -= 10;
  }

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

  TRAD("Encoding ..");
    
  if (NULL == obj) {
    TRAE("Cannot encode using `mftenc` as the given `tra_encoder_object*` is NULL.");
    r = -10;
    goto error;
  }

  if (NULL == sample) {
    TRAE("Cannot encode using `mftenc` as the given `tra_sample*` is NULL.");
    r = -20;
    goto error;
  }
    
  if (TRA_MEMORY_TYPE_IMAGE != type) {
    TRAE("Cannot encode using `mftenc` as the given input type is not supported.");
    r = -30;
    goto error;
  }

  if (NULL == data) {
    TRAE("Cannot encode using `mftenc` as the given `data` is NULL.");
    r = -40;
    goto error;
  }

  ctx = (encoder*) obj;

  r = ctx->enc.encode(sample, type, data);
  if (r < 0) {
    TRAE("Cannot encode using `mftenc`, something went wrong when calling `MediaFoundationEncoder::encode()`.");
    r = -50;
    goto error;
  }

 error:
  return r;
}

/* ------------------------------------------------------- */

static const char* decoder_get_name() {
  return "mftdec";
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
  tra::MediaFoundationDecoderSettings dec_cfg = { 0 };
  decoder* inst = NULL;
  int result = 0;
  int r = 0;
  
  if (NULL == cfg) {
    TRAE("Cannot create the `mftdec` instance because the given config is NULL.");
    r = -10;
    goto error;
  }

  if (NULL == obj) {
    TRAE("Cannot create the `mftdec` instance as the given `obj` is NULL.");
    r = -20;
    goto error;
  }

  if (NULL != (*obj)) {
    TRAE("Cannot create the `mftdec` instance as the given `*obj` is not NULL. Already created?");
    r = -30;
    goto error;
  }

  inst = (decoder*) calloc(1, sizeof(decoder));
  if (NULL == inst) {
    TRAE("Cannot create the `mftdec` instance as we failed to allocate the decoder context.");
    r = -40;
    goto error;
  }

  dec_cfg.image_width = cfg->image_width;
  dec_cfg.image_height = cfg->image_height;
  dec_cfg.on_decoded_data = cfg->callbacks.on_decoded_data;
  dec_cfg.user = cfg->callbacks.user;

  r = inst->dec.init(dec_cfg);
  if (r < 0) {
    TRAE("Failed to initialize the MediaFoundationDecoder");
    r = -50;
    goto error;
  }

  /* Assign the result */
  *obj = (tra_decoder_object*) inst;

 error:
  
  if (r < 0) {
    
    if (NULL != inst) {
      
      result = decoder_destroy((tra_decoder_object*) inst);
      if (result < 0) {
        TRAE("After we failed to create the `mftdec` instance we also failed to cleanly destroy it.");
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
    TRAE("Cannot cleanly destroy the `mftdec` instance as the given `tra_decoder_object*` is NULL.");
    return -1;
  }

  ctx = (decoder*) obj;

  r = ctx->dec.shutdown();
  if (r < 0) {
    TRAE("Failed to cleanly destroy the `mftdec` instance.");
    result -= 10;
  }

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
  
  r = ctx->dec.decode(type, data);
  if (r < 0) {
    TRAE("Cannot decode using `mftdec`, something went wrong while calling `decode()`.");
    r = -50;
    goto error;
  }

 error:
  return r;
}

/* ------------------------------------------------------- */

extern "C" {
  
  /*

    The `tra_load()` function is called when the DLL of the MFT
    module is loaded. Here we register our encoder and
    decoder. This implementation is slightly different than the
    other, pure C implementation where use a static +
    initializaer list to setup the `tra_encoder_api` and
    `tra_decoder_api` structs. This doesn't work with C++; at
    least not with MSVC and the default flags.

    Because the registry only copies the pointer, not the struct,
    we declare it as static.

   */
  TRA_MODULE_DLL int tra_load(tra_registry* reg) {

    static tra_encoder_api encoder_api = {};
    static tra_decoder_api decoder_api = {};
    int r = 0;

    if (NULL == reg) {
      TRAE("Cannot register the mft encoder and decoder because the given `tra_registry*` is NULL.");
      return -1;
    }

    /* Register the encoder. */
    encoder_api.get_name = encoder_get_name;
    encoder_api.get_author = encoder_get_author;
    encoder_api.create = encoder_create;
    encoder_api.destroy = encoder_destroy;
    encoder_api.encode = encoder_encode;

    r = tra_registry_add_encoder_api(reg, &encoder_api);
    if (r < 0) {
      TRAE("Cannot register the `mftenc` plugin.");
      return -2;
    }

    /* Register the decoder. */
    decoder_api.get_name = decoder_get_name;
    decoder_api.get_author = decoder_get_author;
    decoder_api.create = decoder_create;
    decoder_api.destroy = decoder_destroy;
    decoder_api.decode = decoder_decode;

    r = tra_registry_add_decoder_api(reg, &decoder_api);
    if (r < 0) {
      TRAE("Cannot register the `mftdec` plugin.");
      return -2;
    }

    return 0;
  }
  
} /* extern "C" */

/* ------------------------------------------------------- */
  


  


