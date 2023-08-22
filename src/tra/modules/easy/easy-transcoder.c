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

  
  EASY TRANSCODER
  ===============

  GENERAL INFO:

    The easy transcoder can be used to take an input stream
    (e.g. h264), decode it and then encode it again into a format
    which is different then the input format.  For example, you
    can transcode into a different resolution, different bitrate,
    etc. This easy transcoder is part of the "easy" layer of
    Trameleon. This layer hides certain boilerplate code and the
    goal of this layer is to make it as easy as possible for the
    user to user Trameleon.

 */

/* ------------------------------------------------------- */

#include <stdlib.h>
#include <tra/easy.h>
#include <tra/log.h>
#include <tra/transcoder.h>

/* ------------------------------------------------------- */

typedef struct tra_easy_app_transcoder tra_easy_app_transcoder;

/* ------------------------------------------------------- */

struct tra_easy_app_transcoder {
  tra_decoder_settings decoder_cfg;
  tra_easy_api* decoder_api;
  //void* decoder_ctx;
  //tra_transcoder_list* transcode_profiles;
};

struct tra_easy_app_object{
  tra_transcoder* transcoder_obj;
  tra_encoder_api* encoder_api;
  tra_decoder_api* decoder_api;
  tra_transcoder_settings *cfg;
  trans_settings* settings;
};

/* ------------------------------------------------------- */

static int tra_easy_transcoder_create(tra_easy* ez, tra_easy_app_object** obj);
static int tra_easy_transcoder_init(tra_easy* ez, tra_easy_app_object* obj);
static int tra_easy_transcoder_destroy(tra_easy_app_object* ez);
static int tra_easy_transcoder_decode(tra_easy_app_object* ez, uint32_t type, void* data);
static int tra_easy_transcoder_set_opt(tra_easy_app_object* ez, uint32_t opt, va_list args);

/* ------------------------------------------------------- */

static int tra_easy_transcoder_create(tra_easy* ez, tra_easy_app_object** obj) {

  const char* decoder_names[] = {
    "nvdec",
    NULL
  };
  
  tra_easy_app_transcoder* inst = NULL;
  tra_easy_api* decoder_api = NULL;
  int r = 0;

  if (NULL == ez) {
    TRAE("Cannot create the easy transcoder application as the given `tra_easy*` is NULL.");
    r = -10;
    goto error;
  }

  if (NULL == obj) {
    TRAE("Cannot create the easy transcoder application as the given output argument `tra_easy_app_object**` is NULL.");
    r = -20;
    goto error;
  }

  if (NULL != *obj) {
    TRAE("Cannot create the easy transcoder application as the given output argument `*tra_easy_app_object**` is not NULL. Did you already create the app, or forgot to initialize to NULL maybe?");
    r = -30;
    goto error;
  }

  TRAE("@todo implement the create");
  
 error:
  return r;
}

/* ------------------------------------------------------- */

static int tra_easy_transcoder_init(tra_easy* ez, tra_easy_app_object* obj) {

  int r = 0;

  if(NULL == obj){
    TRAE("tra_easy_app_object not initialized");
    r = -10;
    goto error;
  }

  if(NULL == obj->transcoder_obj){
    TRAE("transcoder_obj not initialized");
    r = -10;
    goto error;
  }

  if(NULL == obj->settings){
    TRAE("settings not initialized");
    r = -10;
    goto error;
  }

  if(NULL == obj->encoder_api){
    TRAE("encoder_api not chosen");
    r = -10;
    goto error;
  }

  if(NULL == obj->decoder_api){
    TRAE("decoder_api not chosen");
    r = -10;
    goto error;
  }

  if(NULL == obj->cfg){
    TRAE("transcoder configuration not initialized");
    r = -10;
    goto error;
  }

  r = tra_combinded_transcoder_create(obj->encoder_api, obj->decoder_api, obj->cfg, obj->settings, &obj->transcoder_obj);
  if(0 > r) {
    TRAE("failed to initiate easy transcoder");
    goto error;
  }

 error:
  return r;
}

/* ------------------------------------------------------- */

static int tra_easy_transcoder_destroy(tra_easy_app_object* ez) {

  int r = 0;

  if(NULL == ez){
    TRAE("tra_easy_app_object not initialized");
    r = -10;
    goto error;
  }

  if(NULL == ez->transcoder_obj){
    TRAE("transcoder_obj not initialized");
    r = -10;
    goto error;
  }

  r = tra_combinded_transcoder_destroy(ez->transcoder_obj);
  if(0 > r) {
    TRAE("failed to destroy easy transcoder");
    goto error;
  }

 error:
  return r;
}

/* ------------------------------------------------------- */

static int tra_easy_transcoder_decode(tra_easy_app_object* ez, uint32_t type, void* data) {
  
  int r = 0;

  if(NULL == ez){
    TRAE("tra_easy_app_object not initialized");
    r = -10;
    goto error;
  }

  if(NULL == ez->transcoder_obj){
    TRAE("transcoder_obj not initialized");
    r = -10;
    goto error;
  }

  r = tra_combinded_transcoder_transcode(ez->transcoder_obj, type, data);
  if(0 > r) {
    TRAE("failed to transcode using easy transcoder");
    goto error;
  }

 error:
  return r;
}

/* ------------------------------------------------------- */

static int tra_easy_transcoder_set_opt(tra_easy_app_object* ez, uint32_t opt, va_list args) {

  int r = 0;

 error:
  return r;
}

/* ------------------------------------------------------- */

tra_easy_app_api g_easy_transcoder = {
  .create = tra_easy_transcoder_create,
  .init = tra_easy_transcoder_init,
  .destroy = tra_easy_transcoder_destroy,
  .encode = NULL,
  .decode = tra_easy_transcoder_decode,
  .flush = NULL,
  .set_opt = tra_easy_transcoder_set_opt
};
  
/* ------------------------------------------------------- */
