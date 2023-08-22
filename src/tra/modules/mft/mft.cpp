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
#include <tra/api.h>
#include <tra/log.h>
#include <tra/module.h>
#include <tra/types.h>
}

#include "tra/modules/mft/MediaFoundationDecoder.h"
#include "tra/modules/mft/MediaFoundationEncoder.h"
#include "tra/modules/mft/MediaFoundationUtils.h"
#include <mfapi.h>
#include <vector>

/* ------------------------------------------------------- */

typedef struct encoder {
  tra::MediaFoundationEncoder enc;
} encoder;

/* ------------------------------------------------------- */

typedef struct decoder {
  tra::MediaFoundationDecoder dec;
} decoder;

/* ------------------------------------------------------- */

static const char *encoder_get_name();
static const char *encoder_get_author();
static int encoder_create(tra_encoder_settings *cfg, void *settings, tra_encoder_object **obj);
static int encoder_destroy(tra_encoder_object *obj);
static int encoder_encode(tra_encoder_object *obj, tra_sample *sample, uint32_t type, void *data);

/* ------------------------------------------------------- */
static const char *decoder_get_name();
static const char *decoder_get_author();
static int decoder_create(tra_decoder_settings *cfg, void *settings, tra_decoder_object **obj);
static int decoder_destroy(tra_decoder_object *obj);
static int decoder_decode(tra_decoder_object *obj, uint32_t type, void *data);

/* ------------------------------------------------------- */

static const char *encoder_get_name() {
  return "mftenc";
}

static const char *encoder_get_author() {
  return "alwaysl8";
}

/* ------------------------------------------------------- */

static int encoder_create(tra_encoder_settings *cfg, void *settings, tra_encoder_object **obj) {
  tra_encoder_settings *enc_cfg = (tra_encoder_settings *)calloc(1, sizeof(tra_encoder_settings));
  encoder *inst = NULL;
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

  if (0 == cfg->input_format) {
    TRAE("Cannot create the `mftenc` as the `image_format` is 0.");
    r = -70;
    goto error;
  }

  if (NULL == cfg->callbacks.on_encoded_data) {
    TRAE("Cannot create the `mftenc` as the `callbacks.on_encoded_data` is NULL.");
    r = -70;
    goto error;
  }

  if (NULL == cfg->callbacks.user) {
    TRAE("Cannot create the `mftenc` as the `callbacks.user` is NULL.");
    r = -70;
    goto error;
  }

  inst = (encoder *)calloc(1, sizeof(encoder));
  if (NULL == inst) {
    TRAE("Cannot create the `mftenc`, because we failed to allocate the `encoder` context.");
    r = -90;
    goto error;
  }

  enc_cfg->callbacks = cfg->callbacks;
  enc_cfg->input_format = cfg->input_format;
  enc_cfg->output_format = cfg->output_format;
  enc_cfg->profile = cfg->profile;
  enc_cfg->image_width = cfg->image_width;
  enc_cfg->image_height = cfg->image_height;
  enc_cfg->bitrate = cfg->bitrate;
  enc_cfg->fps_num = cfg->fps_num;
  enc_cfg->fps_den = cfg->fps_den;

  r = inst->enc.init(enc_cfg);
  if (0 > r) {
    TRAE("Cannot create the `mftenc` as we failed to initialize the `MediaFoundationEncoder`.");
    r = -90;
    goto error;
  }

  /* Assign the result. */
  *obj = (tra_encoder_object *)inst;

error:

  if (0 > r) {

    if (NULL != inst) {

      status = encoder_destroy((tra_encoder_object *)inst);
      if (0 > status) {
        TRAE("After we failed to create the encoder we also failed to cleanly shut it down.");
      }

      inst = NULL;
    }

    if (NULL != obj) { *obj = NULL; }
  }

  return r;
}

/* ------------------------------------------------------- */

static int encoder_destroy(tra_encoder_object *obj) {

  encoder *ctx = NULL;
  int result = 0;
  int r;

  if (NULL == obj) {
    TRAE("Cannot cleanly destroy the `mftenc` as the given `tra_encoder_object*` is NULL.");
    return -1;
  }

  ctx = (encoder *)obj;

  r = ctx->enc.shutdown();
  if (0 > r) {
    TRAE("Cannot cleanly destroy the `mftenc` as the `MediaFoundationEncoder::shutdown()` returned "
         "an error.");
    result -= 10;
  }

  free(obj);
  obj = NULL;

  return result;
}

/* ------------------------------------------------------- */

static int encoder_encode(tra_encoder_object *obj, uint32_t type, void *data) {
  encoder *ctx = NULL;
  int r = 0;

  if (NULL == obj) {
    TRAE("Cannot encode using `mftenc` as the given `tra_encoder_object*` is NULL.");
    r = -10;
    goto error;
  }

  if (NULL == data) {
    TRAE("Cannot encode using `mftenc` as the given `data` is NULL.");
    r = -40;
    goto error;
  }

  ctx = (encoder *)obj;

  r = ctx->enc.encode((tra_memory_image *)data);
  if (0 > r) {
    TRAE("Cannot encode using `mftenc`, something went wrong when calling "
         "`MediaFoundationEncoder::encode()`.");
    r = -50;
    goto error;
  }

error:

  return r;
}

/* ------------------------------------------------------- */

static int encoder_flush(tra_encoder_object *obj) {
  encoder *ctx = NULL;
  int r = 0;

  if (NULL == obj) {
    TRAE("Cannot flush the encoder as the given `tra_encoder_object*` is NULL.");
    r = -10;
    goto error;
  }

  ctx = (encoder *)obj;
  r = ctx->enc.flush();
  if (0 > r) {
    TRAE("Failed to flush the encoder.");
    goto error;
  }

error:

  return r;
}

/* ------------------------------------------------------- */

static int encoder_drain(tra_encoder_object *obj) {
  encoder *ctx = NULL;
  int r = 0;

  if (NULL == obj) {
    TRAE("Cannot flush the encoder as the given `tra_encoder_object*` is NULL.");
    r = -10;
    goto error;
  }

  ctx = (encoder *)obj;
  r = ctx->enc.drain();
  if (0 > r) {
    TRAE("Failed to flush the encoder.");
    goto error;
  }

error:

  return r;
}

/* ------------------------------------------------------- */

static const char *decoder_get_name() {
  return "mftdec";
}

static const char *decoder_get_author() {
  return "alwaysl8";
}

/* ------------------------------------------------------- */

static int decoder_create(tra_decoder_settings *cfg, void *settings, tra_decoder_object **obj) {
  tra_decoder_settings dec_cfg = {0};
  decoder *inst = NULL;
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

  inst = (decoder *)calloc(1, sizeof(decoder));
  if (NULL == inst) {
    TRAE("Cannot create the `mftdec` instance as we failed to allocate the decoder context.");
    r = -40;
    goto error;
  }

  dec_cfg.image_width = cfg->image_width;
  dec_cfg.image_height = cfg->image_height;
  dec_cfg.callbacks.on_decoded_data = cfg->callbacks.on_decoded_data;
  dec_cfg.callbacks.user = cfg->callbacks.user;
  dec_cfg.input_format = cfg->input_format;
  dec_cfg.output_format = cfg->output_format;
  dec_cfg.fps_den = cfg->fps_den;
  dec_cfg.fps_num = cfg->fps_num;
  dec_cfg.bitrate = cfg->bitrate;

  r = inst->dec.init(dec_cfg);
  if (0 > r) {
    TRAE("Failed to initialize the MediaFoundationDecoder");
    r = -50;
    goto error;
  }

  /* Assign the result */
  *obj = (tra_decoder_object *)inst;

error:

  if (0 > r) {

    if (NULL != inst) {

      result = decoder_destroy((tra_decoder_object *)inst);
      if (0 > result) {
        TRAE("After we failed to create the `mftdec` instance we also failed to cleanly destroy "
             "it.");
      }

      inst = NULL;
    }

    if (NULL != obj) { *obj = NULL; }
  }

  return r;
}

/* ------------------------------------------------------- */

static int decoder_destroy(tra_decoder_object *obj) {

  decoder *ctx = NULL;
  int result = 0;
  int r = 0;

  if (NULL == obj) {
    TRAE(
        "Cannot cleanly destroy the `mftdec` instance as the given `tra_decoder_object*` is NULL.");
    return -1;
  }

  ctx = (decoder *)obj;

  r = ctx->dec.shutdown();
  if (0 > r) {
    TRAE("Failed to cleanly destroy the `mftdec` instance.");
    result -= 10;
  }

  free(obj);
  obj = NULL;

  return result;
}

/* ------------------------------------------------------- */

static int decoder_decode(tra_decoder_object *obj, uint32_t type, void *data) {

  decoder *ctx = NULL;
  int r = 0;

  LONGLONG timestamp = 0;
  LONGLONG duration = 0;

  if (NULL == obj) {
    TRAE("Cannot decode as the given `tra_decoder_object*` is NULL.");
    r = -10;
    goto error;
  }

  if (NULL == data) {
    TRAE("Cannot decode as the given `data` is NULL.");
    r = -30;
    goto error;
  }

  ctx = (decoder *)obj;

  r = ctx->dec.decode((tra_memory_h264 *)data, timestamp, duration);
  if (0 > r) {
    TRAE("Cannot decode using `mftdec`, something went wrong while calling `decode()`.");
    r = -50;
    goto error;
  }

error:

  return r;
}

/* ------------------------------------------------------- */

HRESULT CheckInputType(IMFTransform *pTransform, IMFMediaType *pType) {
  HRESULT hr = S_OK;
  IMFMediaType *pInputType = NULL;

  // Check if the input type is supported.
  for (DWORD i = 0;; i++) {
    hr = pTransform->GetInputAvailableType(0, i, &pInputType);
    if (S_OK != hr) { break; }

    DWORD bMatch;
    hr = pInputType->IsEqual(pType, &bMatch);
    if (S_OK == hr) { break; }

    Release(&pInputType);
  }

  Release(&pInputType);

  return hr;
}

HRESULT CheckOutputType(IMFTransform *pTransform, IMFMediaType *pType) {
  HRESULT hr = S_OK;
  IMFMediaType *pInputType = NULL;

  // Check if the input type is supported.
  for (DWORD i = 0;; i++) {
    hr = pTransform->GetOutputAvailableType(0, i, &pInputType);
    if (S_OK != hr) { break; }

    DWORD bMatch = FALSE;
    hr = pInputType->IsEqual(pType, &bMatch);
    if (S_OK == hr) { break; }

    Release(&pInputType);
  }

  Release(&pInputType);
  return hr;
}


int getCapabilitiesEncoder(capability *(*output)[], uint32_t size) {
  HRESULT hr = S_OK;
  int x = 0;
  int r = 0;
  for (DWORD i = 0;; i++) {
    IMFTransform *encoder_transform = NULL;
    IMFMediaType *pInputType = NULL;
    hr = encoder_transform->GetInputAvailableType(0, i, &pInputType);
    if (S_OK != hr) { break; }

    capability *cap = NULL;

    // codec
    GUID codec;
    hr = pInputType->GetGUID(MF_MT_SUBTYPE, &codec);
    //@todo check if encoder codec

    r = convert_mft_to_trameleon_image_format(codec, *(uint32_t *)cap->value);
    cap->type = TYPE_UINT;
    cap->name = "codec";

    // resoultion
    hr = MFGetAttributeSize(pInputType, MF_MT_FRAME_SIZE, ((uint32_t *)(*cap->requirements[0])->value),
                            ((uint32_t *)(*cap->requirements[1])->value));
    (*cap->requirements[0])->type = TYPE_UINT;
    (*cap->requirements[1])->type = TYPE_UINT;
    (*cap->requirements[0])->name = "resolution_num";
    (*cap->requirements[1])->name = "resolution_den";
    (*(*cap->requirements[0])->requirements) = NULL;
    (*(*cap->requirements[1])->requirements) = NULL;
    // fps
    hr = MFGetAttributeRatio(pInputType, MF_MT_FRAME_RATE, ((uint32_t *)(*cap->requirements[2])->value),
                             ((uint32_t *)(*cap->requirements[3])->value));
    (*cap->requirements[2])->type = TYPE_UINT;
    (*cap->requirements[3])->type = TYPE_UINT;
    (*cap->requirements[2])->name = "fps_num";
    (*cap->requirements[3])->name = "fps_den";
    (*(*cap->requirements[2])->requirements) = NULL;
    (*(*cap->requirements[3])->requirements) = NULL;
    // aspect ratio
    hr = MFGetAttributeRatio(pInputType, MF_MT_PIXEL_ASPECT_RATIO,
                             ((uint32_t *)(*cap->requirements[4])->value),
                             ((uint32_t *)(*cap->requirements[5])->value));
    (*cap->requirements[4])->type = TYPE_UINT;
    (*cap->requirements[5])->type = TYPE_UINT;
    (*cap->requirements[4])->name = "aspect_ratio_num";
    (*cap->requirements[5])->name = "aspect_ratio_den";
    (*(*cap->requirements[4])->requirements) = NULL;
    (*(*cap->requirements[5])->requirements) = NULL;

    // check each input for available outputs
    encoder_transform->SetInputType(0, pInputType, 0);
    int z = 0;
    for (DWORD j = 0;; j++) {
      IMFMediaType *pOutputType;
      encoder_transform->GetOutputAvailableType(0, i, &pOutputType);
      // bitrate
      pOutputType->GetUINT32(MF_MT_AVG_BITRATE, ((uint32_t *)(*cap->requirements[3 + z * 3])->value));
      (*cap->requirements[3 + z * 3])->type = TYPE_UINT;
      (*cap->requirements[3 + z * 3])->name = "bitrate";
      (*(*cap->requirements[3 + z * 3])->requirements) = NULL;
      // aspect ratio
      MFGetAttributeRatio(pOutputType, MF_MT_PIXEL_ASPECT_RATIO,
                          ((uint32_t *)(*cap->requirements[4 + z * 3])->value),
                          ((uint32_t *)(*cap->requirements[5 + z * 3])->value));
      (*cap->requirements[4 + z * 3])->type = TYPE_UINT;
      (*cap->requirements[4 + z * 3])->name = "aspect_ratio_num";
      (*(*cap->requirements[4 + z * 3])->requirements) = NULL;
      (*cap->requirements[5 + z * 3])->type = TYPE_UINT;
      (*cap->requirements[5 + z * 3])->name = "aspect_ratio_den";
      (*(*cap->requirements[5 + z * 3])->requirements) = NULL;
      Release(&pOutputType);
      z++;
    }

    (*output)[x] = cap;
    Release(&pInputType);
    x++;
  }
  size = x;
  return 0;
}

int isCapabilitibleEncoder(tra_encoder_settings *cfg) {
  int r = 0;
  HRESULT hr = S_OK;
  tra_encoder_settings enc_cfg = {0};
  enc_cfg.callbacks = cfg->callbacks;
  enc_cfg.input_format = cfg->input_format;
  enc_cfg.output_format = cfg->output_format;
  enc_cfg.image_width = cfg->image_width;
  enc_cfg.image_height = cfg->image_height;
  enc_cfg.bitrate = cfg->bitrate;
  enc_cfg.fps_num = cfg->fps_num;
  enc_cfg.fps_den = cfg->fps_den;

  IMFTransform *encoder_transform = NULL;

  GUID input_subtype;
  GUID output_subtype;

  IMFMediaType *pMediaTypeIn = NULL;
  IMFMediaType *pMediaTypeOut = NULL;

  r = convert_trameleon_to_mft_image_format(input_subtype, enc_cfg.input_format);
  if(0 > r){
    TRAE("failed to get convert input subtype");
    r = -10;
    goto error;
  }

  r = convert_trameleon_to_mft_image_format(output_subtype, enc_cfg.input_format);
  if(0 > r){
    TRAE("failed to get convert output subtype");
    r = -10;
    goto error;
  }

  r = tra::MediaFoundationEncoder::findEncoder(input_subtype, output_subtype, &encoder_transform);
  if(0 > r){
    TRAE("failed to create transform");
    r = -10;
    goto error;
  }


  r = tra::MediaFoundationEncoder::createInputMediaType(&pMediaTypeIn, enc_cfg, encoder_transform, 0);
  if (0 > r) {
    TRAE("cannot create input media type");
    r = -10;
    goto error;
  };

  hr = CheckInputType(encoder_transform, pMediaTypeIn);
  if (S_OK != hr) {
    TRAE("invalid input media type");
    r = -10;
    goto error;
  };

  r = tra::MediaFoundationEncoder::createOutputMediaYype(&pMediaTypeOut, enc_cfg);
  if (0 > r) {
    TRAE("cannot create output media type");
    goto error;
  };

  hr = CheckOutputType(encoder_transform, pMediaTypeOut);
  if (S_OK != hr) {
    TRAE("invalid output media type");
    r = -10;
    goto error;
  };

error:

  return r;
}

int getCapabilitiesDecoder(capability *(*output)[], uint32_t size) {
  HRESULT hr = S_OK;
  int x = 0;
  int r = 0;
  for (DWORD i = 0;; i++) {
    IMFTransform *mpEncoder = NULL;
    IMFMediaType *pInputType = NULL;
    hr = mpEncoder->GetInputAvailableType(0, i, &pInputType);
    if (S_OK != hr) { break; }

    capability *cap = NULL;

    // codec
    GUID codec;
    hr = pInputType->GetGUID(MF_MT_SUBTYPE, &codec);
    //@todo check if decoder codec
    r = convert_mft_to_trameleon_image_format(codec, *(uint32_t *)cap->value);
    cap->type = TYPE_UINT;
    cap->name = "codec";

    // resoultion
    hr = MFGetAttributeSize(pInputType, MF_MT_FRAME_SIZE, ((uint32_t *)(*cap->requirements[0])->value),
                            ((uint32_t *)(*cap->requirements[1])->value));
    (*cap->requirements[0])->type = TYPE_UINT;
    (*cap->requirements[1])->type = TYPE_UINT;
    (*cap->requirements[0])->name = "resolution_num";
    (*cap->requirements[1])->name = "resolution_den";
    (*(*cap->requirements[0])->requirements) = NULL;
    (*(*cap->requirements[1])->requirements) = NULL;
    // fps
    hr = MFGetAttributeRatio(pInputType, MF_MT_FRAME_RATE, ((uint32_t *)(*cap->requirements[2])->value),
                             ((uint32_t *)(*cap->requirements[3])->value));
    (*cap->requirements[2])->type = TYPE_UINT;
    (*cap->requirements[3])->type = TYPE_UINT;
    (*cap->requirements[2])->name = "fps_num";
    (*cap->requirements[3])->name = "fps_den";
    (*(*cap->requirements[2])->requirements) = NULL;
    (*(*cap->requirements[3])->requirements) = NULL;
    // aspect ratio
    hr = MFGetAttributeRatio(pInputType, MF_MT_PIXEL_ASPECT_RATIO,
                             ((uint32_t *)(*cap->requirements[4])->value),
                             ((uint32_t *)(*cap->requirements[5])->value));
    (*cap->requirements[4])->type = TYPE_UINT;
    (*cap->requirements[5])->type = TYPE_UINT;
    (*cap->requirements[4])->name = "aspect_ratio_num";
    (*cap->requirements[5])->name = "aspect_ratio_den";
    (*(*cap->requirements[4])->requirements) = NULL;
    (*(*cap->requirements[5])->requirements) = NULL;

    // check each input for available outputs
    mpEncoder->SetInputType(0, pInputType, 0);
    int z = 0;
    for (DWORD j = 0;; j++) {
      IMFMediaType *pOutputType;
      mpEncoder->GetOutputAvailableType(0, i, &pOutputType);
      // bitrate
      pOutputType->GetUINT32(MF_MT_AVG_BITRATE, ((uint32_t *)(*cap->requirements[3 + z * 3])->value));
      (*cap->requirements[3 + z * 3])->type = TYPE_UINT;
      (*cap->requirements[3 + z * 3])->name = "bitrate";
      (*(*cap->requirements[3 + z * 3])->requirements) = NULL;
      // aspect ratio
      MFGetAttributeRatio(pOutputType, MF_MT_PIXEL_ASPECT_RATIO,
                          ((uint32_t *)(*cap->requirements[4 + z * 3])->value),
                          ((uint32_t *)(*cap->requirements[5 + z * 3])->value));
      (*cap->requirements[4 + z * 3])->type = TYPE_UINT;
      (*cap->requirements[4 + z * 3])->name = "aspect_ratio_num";
      (*(*cap->requirements[4 + z * 3])->requirements) = NULL;
      (*cap->requirements[5 + z * 3])->type = TYPE_UINT;
      (*cap->requirements[5 + z * 3])->name = "aspect_ratio_den";
      (*(*cap->requirements[5 + z * 3])->requirements) = NULL;
      Release(&pOutputType);
      z++;
    }

    (*output)[x] = cap;
    Release(&pInputType);
    x++;
  }
  size = x;
  return 0;
}

int isCapabilitibleDecoder(tra_decoder_settings *cfg) {
  int r = 0;
  HRESULT hr = S_OK;
  tra_decoder_settings dec_cfg = {0};
  dec_cfg.image_width = cfg->image_width;
  dec_cfg.image_height = cfg->image_height;
  dec_cfg.callbacks.on_decoded_data = cfg->callbacks.on_decoded_data;
  dec_cfg.callbacks.user = cfg->callbacks.user;

  IMFTransform *decoder_transform = NULL;

  IMFMediaType *input_media_type = NULL;
  IMFMediaType *output_media_type = NULL;

  r = tra::MediaFoundationDecoder::createInputMediaType(&input_media_type, dec_cfg);
  if (0 > r) {
    TRAE("cannot create input media type");
    goto error;
  }

  hr = CheckInputType(decoder_transform, input_media_type);
  if (S_OK != hr) {
    TRAE("invalid input media type");
    r = -1;
    goto error;
  }

  r = tra::MediaFoundationDecoder::createOutputMediaType(output_media_type, dec_cfg);
  if (0 > r) {
    TRAE("cannot create output media type");
    goto error;
  }

  hr = CheckOutputType(decoder_transform, output_media_type);
  if (S_OK != hr) {
    TRAE("invalid output media type");
    r = -1;
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
  initializer list to setup the `tra_encoder_api` and
  `tra_decoder_api` structs. This doesn't work with C++; at
  least not with MSVC and the default flags.

  Because the registry only copies the pointer, not the struct,
  we declare it as static.

 */
TRA_MODULE_DLL int tra_load(tra_registry *reg) {

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
  encoder_api.flush = encoder_drain;
  encoder_api.isCapable = isCapabilitibleEncoder;
  encoder_api.getCapability = getCapabilitiesEncoder;

  r = tra_registry_add_encoder_api(reg, &encoder_api);
  if (0 > r) {
    TRAE("Cannot register the `mftenc` plugin.");
    return -2;
  }

  /* Register the decoder. */
  decoder_api.get_name = decoder_get_name;
  decoder_api.get_author = decoder_get_author;
  decoder_api.create = decoder_create;
  decoder_api.destroy = decoder_destroy;
  decoder_api.decode = decoder_decode;
  decoder_api.isCapable = isCapabilitibleDecoder;
  decoder_api.getCapability = getCapabilitiesDecoder;

  r = tra_registry_add_decoder_api(reg, &decoder_api);
  if (0 > r) {
    TRAE("Cannot register the `mftdec` plugin.");
    return -2;
  }

  return 0;
}
}