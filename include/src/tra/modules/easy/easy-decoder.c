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


  EASY DECODER
  =============

  GENERAL INFO:

    This easy decoder can be used when you want to decode
    e.g. H264. This easy application will make sure that we
    select the best decoder API that's available. E.g. when there
    is support for a hardware decoder, we will make sure that
    that decoder is selected.

    This decoder layer is a thin layer on top of the actual
    decoder. You as a user will create a `tra_easy` instance via
    the API that you can find in `easy.{h,c}`. When use the
    `TRA_EASY_APPLICATION_DECODER`, the functions in this file
    will be used to select a decoder.

    Most of the actual decoding features of the easy layer will
    be implemented by the modules themselves.
  
 */
/* ------------------------------------------------------- */

#include <stdlib.h>
#include <tra/easy.h>
#include <tra/log.h>

/* ------------------------------------------------------- */

typedef struct tra_easy_app_decoder tra_easy_app_decoder;

/* ------------------------------------------------------- */

struct tra_easy_app_decoder {
  tra_decoder_settings decoder_cfg; /* The setttings that we pass into the decoder when we initialize it. */
  tra_easy_api* decoder_api; /* The easy API imlementation of a module; e.g. the NVIDIA module. */
  void* decoder_ctx; /* The actual decoder instance */
};

/* ------------------------------------------------------- */

static int tra_easy_decoder_create(tra_easy* ez, tra_easy_app_object** obj);
static int tra_easy_decoder_init(tra_easy* ez, tra_easy_app_object* obj);
static int tra_easy_decoder_destroy(tra_easy_app_object* ez);
static int tra_easy_decoder_decode(tra_easy_app_object* ez, uint32_t type, void* data);
static int tra_easy_decoder_set_opt(tra_easy_app_object* ez, uint32_t opt, va_list args);

/* ------------------------------------------------------- */

static int tra_easy_decoder_create(tra_easy* ez, tra_easy_app_object** obj) {

  const char* decoders[] = {
    "nvdec",
    NULL
  };

  tra_easy_app_decoder* app_inst = NULL;
  tra_easy_api* decoder_api = NULL;
  int r = 0;

  if (NULL == ez) {
    TRAE("Cannot create the easy decoder application. Given `tra_easy*` is NULL.");
    r = -10;
    goto error;
  }

  if (NULL == obj) {
    TRAE("Cannot create the easy decoder application. The given `tra_easy_app_object**` is NULL.");
    r = -20;
    goto error;
  }

  if (NULL != *obj) {
    TRAE("Cannot create the easy decoder application. The given `*tra_easy_app_object**` is not NULL. Make sure to initialize your output variable to NULL.");
    r = -30;
    goto error;
  }

  r = tra_easy_select_api(ez, decoders, &decoder_api);
  if (r < 0) {
    TRAE("Cannot create the easy decoder application . We failed to select a decoder.");
    r = -40;
    goto error;
  }

  if (NULL == decoder_api) {
    TRAE("Cannot create the easy decoder application. We failed to select a decode API.");
    r = -50;
    goto error;
  }

  app_inst = calloc(1, sizeof(tra_easy_app_decoder));
  if (NULL == app_inst) {
    TRAE("Cannot create the easy decoder application. We failed to allocate the easy decoder application instance. Out of memory?");
    r = -60;
    goto error;
  }

  /* Setup */
  app_inst->decoder_api = decoder_api;

  /* Assign */
  *obj = (tra_easy_app_object*) app_inst;

 error:

  TRAE("@todo handle error case and destroy!");
    
  return r;
}

/* ------------------------------------------------------- */

/*
  
  During a typical easy workflow, the user calls
  `tra_easy_create()` to allocate an application. Then he will
  configure the application using `tra_easy_set_opt()`.  Once
  configured, the user calls` tra_easy_init()`. At this point we
  can use the requested configuration to initialize our encoder,
  decoder, etc. In this case we create the decoder.
  
 */
static int tra_easy_decoder_init(tra_easy* ez, tra_easy_app_object* obj) {

  tra_easy_app_decoder* app = NULL;
  tra_easy_api* decoder = NULL;
  int r = 0;

  app = (tra_easy_app_decoder*) obj;
  if (NULL == app) {
    TRAE("Cannot initialize the easy decoder. Given `tra_easy_app_object*` is NULL.");
    r = -10;
    goto error;
  }

  decoder = app->decoder_api;
  if (NULL == decoder) {
    TRAE("Cannot initialize the easy decoder. The `tra_easy_app_decoder::decoder_api` is NULL.");
    r = -20;
    goto error;
  }

  if (NULL == decoder->decoder_create) {
    TRAE("Cannot initialize the easy decoder. The `tra_easy_api::decoder_create()` function is NULL.");
    r = -30;
    goto error;
  }

  r = decoder->decoder_create(ez, &app->decoder_cfg, &app->decoder_ctx);
  if (r < 0) {
    TRAE("Cannot initialize the easy decoder, we failed to create the decoder instance.");
    r = -40;
    goto error;
  }

 error:
  return r;
}

/* ------------------------------------------------------- */

/*
  This function is called via `tra_easy_destroy()` and allows us
  to deallocate ourself and the actual decoder context. For
  example, when we're using the NVIDIA decoder, then the NVIDIA
  decoder easy layer will deallocate its instance (see
  nvidia-dec.c).
 */
static int tra_easy_decoder_destroy(tra_easy_app_object* obj) {

  tra_easy_app_decoder* app = NULL;
  tra_easy_api* dec_api = NULL;
  void* dec_ctx = NULL;
  int result = 0;
  int r = 0;

  app = (tra_easy_app_decoder*) obj;
  if (NULL == app) {
    TRAE("Cannot destroy the decoder as the given `tra_easy_app_object*` is NULL.");
    result = -10;
    goto error;
  }

  /* Allow the easy layer of the module to deallocate it's resources. */
  dec_api = app->decoder_api;
  dec_ctx = app->decoder_ctx;
  
  if (NULL != dec_api
      && NULL != dec_ctx
      && NULL != dec_api->decoder_destroy
  )
    {
      r = dec_api->decoder_destroy(dec_ctx);
      if (r < 0) {
        TRAE("Failed to cleanly destroy the decoder.");
        result = -40;
      }
    }
  
  app->decoder_ctx = NULL;
  app->decoder_api = NULL;

  free(app);
  app = NULL;

 error:
  return result;
}

/* ------------------------------------------------------- */


/*
  This is the core function of this easy application
  implementation. Here we use the module implementation to decode
  the given input. This implementation can be, for example, the
  `nvdec`.
 */
static int tra_easy_decoder_decode(tra_easy_app_object* obj, uint32_t type, void* data) {

  tra_easy_app_decoder* app = NULL;
  tra_easy_api* decoder = NULL;
  int r = 0;

  app = (tra_easy_app_decoder*) obj;
  if (NULL == app) {
    TRAE("Cannot decode as the application context is NULL. Did you use `tra_easy_create()` and `tra_easy_init()` ?");
    r = -10;
    goto error;
  }

  decoder = app->decoder_api;
  if (NULL == decoder) {
    TRAE("Cannot decode as the `decoder_api` of the `tra_easy_app_decoder` is NULL.");
    r = -20;
    goto error;
  }

  if (NULL == decoder->decoder_decode) {
    TRAE("Cannot decode as the `decoder_decode()` function of the `tra_easy_api` is NULL.");
    r = -30;
    goto error;
  }

  r = decoder->decoder_decode(app->decoder_ctx, type, data);
  if (r < 0) {
    TRAE("Failed to decode using the `tra_easy`.");
    r = -40;
    goto error;
  }

 error:
  return r;
}

/* ------------------------------------------------------- */

static int tra_easy_decoder_set_opt(
  tra_easy_app_object* obj,
  uint32_t opt,
  va_list args
)
{
  tra_easy_app_decoder* app = NULL;
  int r = 0;

  app = (tra_easy_app_decoder*) obj;
  if (NULL == app) {
    TRAE("Cannot set the easy decoder application option as the given `tra_easy_app_object*` is NULL.");
    r = -10;
    goto error;
  }

  switch (opt) {
    
    case TRA_EOPT_INPUT_SIZE: {
      app->decoder_cfg.image_width = va_arg(args, uint32_t);
      app->decoder_cfg.image_height = va_arg(args, uint32_t);
      break;
    }

    case TRA_EOPT_OUTPUT_TYPE: {
      app->decoder_cfg.output_type = va_arg(args, uint32_t);
      break;
    }

    case TRA_EOPT_DECODED_CALLBACK: {
      app->decoder_cfg.callbacks.on_decoded_data = va_arg(args, tra_decoded_callback);
      break;
    }

    case TRA_EOPT_DECODED_USER: {
      app->decoder_cfg.callbacks.user = va_arg(args, void*);
      break;
    }

    default: {
      TRAE("Unhandled option.");
      r = -10;
      goto error;
    }
  }
  
 error:
  return r;
}

/* ------------------------------------------------------- */

tra_easy_app_api g_easy_decoder = {
  .create = tra_easy_decoder_create,
  .init = tra_easy_decoder_init,
  .destroy = tra_easy_decoder_destroy,
  .encode = NULL,
  .decode = tra_easy_decoder_decode,
  .flush = NULL,
  .set_opt = tra_easy_decoder_set_opt,
};

/* ------------------------------------------------------- */
