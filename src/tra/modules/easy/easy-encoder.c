/* ------------------------------------------------------- */

#include <stdlib.h>
#include <tra/easy.h>
#include <tra/log.h>

/* ------------------------------------------------------- */

typedef struct tra_easy_app_encoder tra_easy_app_encoder;  /* The context for an easy encoder. */

/* ------------------------------------------------------- */

struct tra_easy_app_encoder {
  tra_encoder_settings encoder_cfg;
  tra_easy_api* encoder_api;
  void* encoder_ctx;
};

/* ------------------------------------------------------- */

static int tra_easy_encoder_create(tra_easy* ez, tra_easy_app_object** obj);
static int tra_easy_encoder_init(tra_easy* ez, tra_easy_app_object* obj);
static int tra_easy_encoder_destroy(tra_easy_app_object* obj);
static int tra_easy_encoder_encode(tra_easy_app_object* obj, tra_sample* sample, uint32_t type, void* data);
static int tra_easy_encoder_flush(tra_easy_app_object* obj);
static int tra_easy_encoder_set_opt(tra_easy_app_object* obj, uint32_t opt, va_list args);

/* ------------------------------------------------------- */

/*

  This function creates an easy encoder application. As Trameleon
  by itself should not know about the modules which have been
  registered, we use an `tra_easy_api` interface. This interface
  can be implemented by module developers. For example, we have a
  NVIDIA module which provides easy implementations for encoders
  (the NVIDIA module has specialised encoders for encoding CPU
  memory, CUDA memory and GL textures for example).
  
  This function tries to find the best easy encoder. At the time
  of writing this, we use a prioritised list of easy encoder
  names. In the future we should implement something smarter to
  select an encoder.

  Once we have the `encoder_api` that implements te easy encoder
  interface for some module, we can create the application and
  encoder context. By calling `encoder_create()` we call one of
  the easy implementations that a module provides.

  By calling `encoder_create()` we allow the easy implmentation
  of a module, to initialize certains dependencies it needs.  For
  example, the NVIDIA encoder will first check if the CUDA API
  has been loaded, a CUDA context has been created etc. These are
  the things that the easy layer will perform for the user.
  
*/

static int tra_easy_encoder_create(tra_easy* ez, tra_easy_app_object** result) {

  const char* encoders[] = {
    "nvenchost",
    "nvenccuda",
    "x264enc",
    NULL
  };

  tra_encoder_settings enc_cfg = { 0 };
  tra_easy_app_encoder* app_inst = NULL;
  tra_easy_api* encoder_api = NULL;
  int r = 0;

  if (NULL == ez) {
    TRAE("Cannot create the easy encoder as the `tra_easy*` is NULL.");
    r = -20;
    goto error;
  }

  if (NULL == result) {
    TRAE("Cannot create the easy encoder as the given result `tra_easy_app_object**` is NULL.");
    r = -20;
    goto error;
  }

  if (NULL != *result) {
    TRAE("Cannot create the easy encoder as the given `*tra_easy_app_object**` is not NULL.");
    r = -30;
    goto error;
  }
  
  /* Select the best encoder. */
  r = tra_easy_select_api(ez, encoders, &encoder_api);
  if (r < 0) {
    TRAE("Something went wrong while trying to select an encoder.");
    r = -10;
    goto error;
  }

  if (NULL == encoder_api) {
    TRAE("Failed to find an encoder API, cannot create the easy encoder app.");
    r = -20;
    goto error;
  }

  /* When we've found an encoder we can allocate the encoder app. */
  app_inst = calloc(1, sizeof(tra_easy_app_encoder));
  if (NULL == app_inst) {
    TRAE("Failed to allocate the `tra_easy_app_encoder`. Out of memory?");
    r = -30;
    goto error;
  }

  /* Setup the app context + easy context */
  app_inst->encoder_api = encoder_api;

  /* Assign the result argument. */
  *result = (tra_easy_app_object*) app_inst;

  TRAE("@todo we need to handle errors here.");
  
 error:
  return r;
}

/* ------------------------------------------------------- */

/*
  Here we options specifically for the easy encoder
  application. Below you can see what options we expect the user
  to configure.
*/
static int tra_easy_encoder_set_opt(
  tra_easy_app_object* obj,
  uint32_t opt,
  va_list args
)
{
  tra_easy_app_encoder* app = NULL;
  int r = 0;

  app = (tra_easy_app_encoder*) obj;
  if (NULL == app) {
    TRAE("Cannot set the option the application context is NULL.");
    r = -20;
    goto error;
  }

  switch (opt) {

    case TRA_EOPT_INPUT_SIZE: {
      app->encoder_cfg.image_width = va_arg(args, uint32_t);
      app->encoder_cfg.image_height = va_arg(args, uint32_t);
      break;
    }

    case TRA_EOPT_INPUT_FORMAT: {
      app->encoder_cfg.image_format = va_arg(args, uint32_t);
      break;
    }

    case TRA_EOPT_FPS: {
      app->encoder_cfg.fps_num = va_arg(args, uint32_t);
      app->encoder_cfg.fps_den = va_arg(args, uint32_t);
      break;
    }

    case TRA_EOPT_ENCODED_CALLBACK: {
      app->encoder_cfg.callbacks.on_encoded_data = va_arg(args, tra_encoded_callback);
      break;
    }

    case TRA_EOPT_FLUSHED_CALLBACK: {
      app->encoder_cfg.callbacks.on_flushed = va_arg(args, tra_flushed_callback);
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

/*
  This function should be called after the user has created an
  intance of `tra_easy` and configured it using the
  `tra_easy_set_opt()` functions. When the easy application has
  been configured, you can call this function. This function will
  create the encoder instance for the easy encoder app.
 */
static int tra_easy_encoder_init(tra_easy* ez, tra_easy_app_object* obj) {

  tra_easy_app_encoder* app = NULL;
  tra_easy_api* encoder = NULL;
  int r = 0;

  if (NULL == ez) {
    TRAE("Cannot initialize the encoder as the given `tra_easy*` is NULL.");
    r = -10;
    goto error;
  }

  app = (tra_easy_app_encoder*) obj;
  if (NULL == app) {
    TRAE("Cannot initialize the encoder as the given application context is NULL.");
    r = -20;
    goto error;
  }

  encoder = app->encoder_api;
  if (NULL == encoder) {
    TRAE("Cannot initialize the encoder as the `encoder_api` of the easy encoder app is NULL. Did you create it?");
    r = -30;
    goto error;
  }

  if (NULL == encoder->encoder_create) {
    TRAE("Cannot initialize the encoder as the `encoder_create()` function is not implemented.");
    r = -40;
    goto error;
  }
  
  r = encoder->encoder_create(ez, &app->encoder_cfg, &app->encoder_ctx);
  if (r < 0) {
    TRAE("Failed to initialize the easy encoder instance.");
    r = -50;
    goto error;
  }
         
 error:

  return r;
}

/* ------------------------------------------------------- */

static int tra_easy_encoder_destroy(tra_easy_app_object* obj) {
  
  int r = 0;

  TRAE("@todo cleanup and destroy the encoder application.");

 error:
  return r;
}

/* ------------------------------------------------------- */

/*
  This function of the easy encoder application, will call
  forward the encode request to the encoder api that was select.
 */
static int tra_easy_encoder_encode(
  tra_easy_app_object* obj, 
  tra_sample* sample,
  uint32_t type,
  void* data
)
{
  tra_easy_app_encoder* app = NULL;
  tra_easy_api* encoder = NULL;
  int r = 0;

  app = (tra_easy_app_encoder*) obj;
  if (NULL == app) {
    TRAE("Cannot encode as the application context is NULL.");
    r = -20;
    goto error;
  }

  encoder = app->encoder_api;
  if (NULL == encoder) {
    TRAE("Cannot encode as the `encoder_api` of the `tra_easy_app_encoder` is NULL.");
    r = -30;
    goto error;
  }

  if (NULL == encoder->encoder_encode) {
    TRAE("Cannot encode as the `encoder_encode()` function of the encoder easy api is NULL.");
    r = -40;
    goto error;
  }

  /* Now, call the encode function of the encoder of the module (e.g. nvenccuda, nvenchost, etc). */ 
  r = encoder->encoder_encode(app->encoder_ctx, sample, type, data);
  if (r < 0) {
    TRAE("Cannot encode, the easy encoder returned an error.");
    r = -50;
    goto error;
  }
  
 error:
  return r;
}

/* ------------------------------------------------------- */

static int tra_easy_encoder_flush(tra_easy_app_object* obj) {
  
  tra_easy_app_encoder* app = NULL;
  tra_easy_api* encoder = NULL;
  int r = 0;

  app = (tra_easy_app_encoder*) obj;
  if (NULL == app) {
    TRAE("Cannot flush as the application context is NULL.");
    r = -20;
    goto error;
  }

  encoder = app->encoder_api;
  if (NULL == encoder) {
    TRAE("Cannot flush as the `encoder_api` of the `tra_easy_app_encoder` is NULL.");
    r = -30;
    goto error;
  }

  if (NULL == encoder->encoder_flush) {
    TRAE("Cannot flush as the `encoder_flush()` function of the encoder easy api is NULL.");
    r = -40;
    goto error;
  }

  /* Now, call the flush function of the encoder of the module (e.g. nvenccuda, nvenchost, etc). */ 
  r = encoder->encoder_flush(app->encoder_ctx);
  if (r < 0) {
    TRAE("Cannot flush, the easy encoder returned an error.");
    r = -50;
    goto error;
  }
  
 error:
  return r;
}

/* ------------------------------------------------------- */

tra_easy_app_api g_easy_encoder = {
  .create = tra_easy_encoder_create,
  .init = tra_easy_encoder_init,
  .destroy = tra_easy_encoder_destroy,
  .encode = tra_easy_encoder_encode,
  .decode = NULL, /* The encoder app shouldn't provide a decode function. */
  .flush = tra_easy_encoder_flush,
  .set_opt = tra_easy_encoder_set_opt,
};

/* ------------------------------------------------------- */
