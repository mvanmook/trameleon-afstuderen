/* ------------------------------------------------------- */

#include <stdlib.h>
#include <tra/module.h>
#include <tra/easy.h>
#include <tra/core.h>
#include <tra/log.h>

/* ------------------------------------------------------- */

struct tra_easy {
  uint32_t type;          
  tra_core* core_ctx;      
  tra_easy_app_api* app_api;       /* The API that implements the functionality for the easy application. For example this can be the API for a transcoder which manages the decoding, scaling, encoding, etc. */
  tra_easy_app_object* app_obj;    /* The application specific context; e.g. the data which a easy application needs. For example, this can be an instance of `tra_easy_app_encoder`. */
};

/* ------------------------------------------------------- */

int tra_easy_create(tra_easy_settings* cfg, tra_easy** ez) {

  tra_core_settings core_cfg = { 0 };
  tra_easy* inst = NULL;
  int r = 0;

  if (NULL == cfg) {
    TRAE("Cannot create the easy instance as the given `tra_easy_settings*` is NULL.");
    r = -10;
    goto error;
  }

  if (0 == cfg->type) {
    TRAE("Cannot create the easy intance as the given `tra_easy_settings::type` is not set.");
    r = -20;
    goto error;
  }
  
  if (NULL == ez) {
    TRAE("Cannot create the easy instance as the given `tra_easy**` is NULL.");
    r = -10;
    goto error;
  }

  if (NULL != *ez) {
    TRAE("Cannot create the easy instance as the given `*tra_easy**` is not NULL.");
    r = -20;
    goto error;
  }

  inst = calloc(1, sizeof(tra_easy));
  if (NULL == inst) {
    TRAE("Cannot create the easy instance. We failed to allocate `tra_easy`. Out of memory?");
    r = -30;
    goto error;
  }

  /* Create the essential Trameleon objects. */
  r = tra_core_create(&core_cfg, &inst->core_ctx);
  if (r < 0) {
    TRAE("Cannot create the easy instance. We failed to create the core context.");
    r = -40;
    goto error;
  }

  /* Create the application instance. */
  switch (cfg->type) {
    
    case TRA_EASY_APPLICATION_TYPE_ENCODER: {
      inst->app_api = &g_easy_encoder;
      break;
    }

    case TRA_EASY_APPLICATION_TYPE_DECODER: {
      inst->app_api = &g_easy_decoder;
      break;
    }

    case TRA_EASY_APPLICATION_TYPE_TRANSCODER: {
      inst->app_api = &g_easy_transcoder;
      break;
    }
    
    default: {
      TRAE("Cannot create the easy instance, unhandled application type.");
      r = -40;
      goto error;
    }
  }

  if (NULL == inst->app_api) {
    TRAE("The `tra_easy::app_api` hasn't been set. At this point it must have been set. We cannot create the application instance.");
    r = -50;
    goto error;
  }

  r = inst->app_api->create(inst, &inst->app_obj);
  if (r < 0) {
    TRAE("Cannot create the easy instance, we failed to create the application instance.");
    r = -60;
    goto error;
  }

  /* Finally assign. */
  *ez = inst;
  
 error:

  if (r < 0) {
    
    if (NULL != inst) {
      tra_easy_destroy(inst);
      inst = NULL;
    }

    if (NULL != ez) {
      *ez = NULL;
    }
  }
  
  return r;
}

/* ------------------------------------------------------- */

/*
  
  This function will forward the `destroy()` request to the
  application API. For example, the `app_api` which implements
  the easy application might have created a decoder. In that case
  it would deallocate the decoder instance when we call
  `tra_easy_app_api::destroy`.

  Once we've called the `destroy()` function of the application,
  we will free the `tra_easy` handle too. After calling this
  function you should not use the easy handle anymore.
  
 */
int tra_easy_destroy(tra_easy* ez) {

  tra_easy_app_api* api = NULL;
  int result = 0;
  int r = 0;

  if (NULL == ez) {
    TRAE("Cannot destroy the `tra_easy*` as it's NULL.");
    return -10;
  }

  api = ez->app_api;

  /* Use the application to cleanup it's own contexts. */
  if (NULL != api
      && NULL != api->destroy)
    {
      r = api->destroy(ez->app_obj);
      if (r < 0) {
        TRAE("Failed to cleanly destroy the easy application.");
        result -= 20;
      }
    }

  ez->type = 0;
  ez->core_ctx = NULL;
  ez->app_api = NULL;
  ez->app_obj = NULL;

  free(ez);
  ez = NULL;

 error:
  return result;
}

/* ------------------------------------------------------- */

int tra_easy_set_opt(tra_easy* ez, uint32_t opt, ...) {

  int did_start = 0;
  va_list args;
  int r = 0;

  if (NULL == ez) {
    TRAE("Cannot set the option as the given `tra_easy*` is NULL.");
    r = -10;
    goto error;
  }

  if (NULL == ez->app_api) {
    TRAE("Cannot set the option as the `app_api` member of the given `tra_easy*` is NULL. Did you create it?");
    r = -20;
    goto error;
  }

  if (NULL == ez->app_api->set_opt) {
    TRAE("Cannot set an option as the `app_api` has not implemented the `set_opt` function.");
    r = -30;
    goto error;
  }

  va_start(args, opt);
  did_start = 1;
  
  r = ez->app_api->set_opt(ez->app_obj, opt, args);
  if (r < 0) {
    TRAE("Failed to set the option.");
    r = -40;
    goto error;
  }

 error:

  if (1 == did_start) {
    va_end(args);
  }
  
  return r;
}

/* ------------------------------------------------------- */

int tra_easy_init(tra_easy* ez) {

  int r = 0;

  if (NULL == ez) {
    TRAE("Cannot initialize the `tra_easy` as the it's NULL.");
    r = -10;
    goto error;
  }

  if (NULL == ez->app_api) {
    TRAE("Cannot initialize the `tra_easy` as the api hasn't been set.");
    r = -20;
    goto error;
  }

  if (NULL == ez->app_api->init) {
    TRAE("Cannot initialize the `tra_easy` as the `init()` function is not implemented.");
    r = -30;
    goto error;
  }

  r = ez->app_api->init(ez, ez->app_obj);
  if (r < 0) {
    TRAE("Failed to initialize the `tra_easy`.");
    r = -40;
    goto error;
  }

 error:
  return r;
}

/* ------------------------------------------------------- */

/* Encode the given data using the easy application. */
int tra_easy_encode(tra_easy* ez, tra_sample* sample, uint32_t type, void* data) {

  int r = 0;
  
  if (NULL == ez) {
    TRAE("Cannot encode as the given `tra_easy*` is NULL.");
    r = -10;
    goto error;
  }

  if (NULL == ez->app_api) {
    TRAE("Cannot encode as the given `tra_easy` has no application api set.");
    r = -20;
    goto error;
  }

  if (NULL == ez->app_api->encode) {
    TRAE("Cannot encode as the given `tra_easy` application has no `encode()` function set.");
    r = -30;
    goto error;
  }

  if (NULL == sample) {
    TRAE("Cannot encode as the given `tra_sample*` is NULL>");
    r = -40;
    goto error;
  }

  r = ez->app_api->encode(ez->app_obj, sample, type, data);
  if (r < 0) {
    TRAE("Failed to encode using `tra_easy`.");
    r = -40;
    goto error;
  }

 error:

  return r;
}

/* ------------------------------------------------------- */

int tra_easy_decode(tra_easy* ez, uint32_t type, void* data) {

  int r = 0;

  if (NULL == ez) {
    TRAE("Cannot decode as the given `tra_easy*` is NULL.");
    r = -10;
    goto error;
  }

  if (NULL == ez->app_api) {
    TRAE("Cannot decode as the given `tra_easy` has no application API set.");
    r = -20;
    goto error;
  }

  if (NULL == ez->app_api->decode) {
    TRAE("Cannot decode as the given `tra_easy` application API has no `decode()` function set.");
    r = -30;
    goto error;
  }

  if (NULL == data) {
    TRAE("Cannot decode as the given `data*` is NULL.");
    r = -40;
    goto error;
  }

  r = ez->app_api->decode(ez->app_obj, type, data);
  if (r < 0) {
    TRAE("Failed to decode using `tra_easy`.");
    r = -50;
    goto error;
  }

 error:

  return r;
}

/* ------------------------------------------------------- */

/* Flush the encoder, decoder, etc. */
int tra_easy_flush(tra_easy* ez) {

  int r = 0;
  
  if (NULL == ez) {
    TRAE("Cannot flush as the given `tra_easy*` is NULL.");
    r = -10;
    goto error;
  }

  if (NULL == ez->app_api) {
    TRAE("Cannot flush as the given `tra_easy` has not application api set.");
    r = -20;
    goto error;
  }

  if (NULL == ez->app_api->flush) {
    TRAE("Cannot flush as the given `tra_easy` application has no `flush()` function set.");
    r = -30;
    goto error;
  }

  r = ez->app_api->flush(ez->app_obj);
  if (r < 0) {
    TRAE("Failed to flush using `tra_easy`.");
    r = -40;
    goto error;
  }

 error:

  return r;
}

/* ------------------------------------------------------- */

int tra_easy_get_core_context(tra_easy* ez, tra_core** ctx) {

  int r = 0;
  
  if (NULL == ez) {
    TRAE("Cannot get the core context as the given `tra_easy*` is NULL.");
    r = -10;
    goto error;
  }

  if (NULL == ctx) {
    TRAE("Cannot get the core context as the result argument is NULL.");
    r = -20;
    goto error;
  }

  if (NULL == ez->core_ctx) {
    TRAE("Cannot get the core context as the `tra_easy::core_ctx` is NULL. Did you create the easy handle?");
    r = -30;
    goto error;
  }

  *ctx = ez->core_ctx;

 error:
  return r;
}

/* ------------------------------------------------------- */

/*
  
  This function will try to get an easy API based on the `names`
  array. The `names` array should be a prioritized list of API
  names. The first entry has the highest preference. When we find
  an easy API that matches one of the names, we retrieve the API
  from the registry and set it to `result` and return 0.  When we
  can't find any of the given `names` we return 0 and assign NULL
  to the `result`.

  When an error occurs we return < 0. When no error occurs but
  we couldn't find any of the `names`, we return 0 and set `result`
  to NULL.
  
 */
int tra_easy_select_api(
  tra_easy* ez,
  const char* names[],
  tra_easy_api** result
)
{
  const char** name = NULL;
  tra_easy_api* api = NULL;
  int r = 0;

  if (NULL == ez) {
    TRAE("Cannot find an easy api as the given `tra_easy*` is NULL.");
    r = -10;
    goto error;
  }

  if (NULL == ez->core_ctx) {
    TRAE("Cannot find an easy api as the `tra_easy::core_ctx` member is NULL. Did you create the easy instance?");
    r = -20;
    goto error;
  }

  if (NULL == result) {
    TRAE("Cannot select an API as the result argument is NULL so we cannot assign it.");
    r = -30;
    goto error;
  }

  if (NULL != *result) {
    TRAE("Cannot select an API as the given result argument seems to point to an API already. Make sure to initialize the result to NULL.");
    r = -40;
    goto error;
  }
  
  /* Iterate over the encoder names and try to get the API */
  name = names;
  
  while (NULL != *name) {

    api = NULL;
    
    r = tra_core_easy_find(ez->core_ctx, *name, &api);
    if (r < 0) {
      TRAE("Failed to get the easy encoder: `%s`.", *name);
      r = -30;
      goto error;
    }

    /* We found an API! */
    if (NULL != api) {
      break;
    }

    name++;
  }

  if (NULL != api) {
    *result = api;
  }

 error:
  return r;
}
  
/* ------------------------------------------------------- */

int tra_transcode_list_create(tra_transcode_list_settings* cfg, tra_transcode_list** list) {

  tra_transcode_list* inst = NULL;
  int r = 0;
  
  if (NULL == cfg) {
    TRAE("Cannot create the transcode list as the given `tra_transcode_list_settings*` is NULL.");
    r = -10;
    goto error;
  }

  if (NULL == list) {
    TRAE("Cannot create the transcode list as the given output argument `tra_transcode_list**` is NULL.");
    r = -20;
    goto error;
  }

  if (NULL != *list) {
    TRAE("Cannot create the transcode list as the given output argument `*tra_transcode_list**` is not NULL. Did you already create it, or maybe forgot to initialize to NULL?");
    r = -30;
    goto error;
  }

  inst = calloc(1, sizeof(tra_transcode_list));
  if (NULL == inst) {
    TRAE("Cannot create the transcode list, we failed to allocate the list. Out of memory?");
    r = -40;
    goto error;
  }

  /* Setup */
  inst->profile_count = 0;
  inst->profile_array = NULL;

  /* Assign */
  *list = inst;

 error:

  if (r < 0) {
    
    if (NULL != inst) {
      tra_transcode_list_destroy(inst);
      inst = NULL;
    }

    if (NULL != list) {
      *list = NULL;
    }
  }    

  return r;
}

/* ------------------------------------------------------- */

int tra_transcode_list_destroy(tra_transcode_list* list) {

  int result = 0;
  int r = 0;

  if (NULL == list) {
    TRAE("Cannot destroy the profile list as it's NULL.");
    return -10;
  }

  if (NULL != list->profile_array) {
    free(list->profile_array);
  }

  list->profile_array = NULL;
  list->profile_count = 0;

  free(list);
  list = NULL;
  
  return result;
}

/* ------------------------------------------------------- */

int tra_transcode_list_add_profile(tra_transcode_list* list, tra_transcode_profile* profile) {

  tra_transcode_profile* new_array = NULL;
  int r = 0;

  if (NULL == list) {
    TRAE("Cannot add a transcode profile to the list as the given list is NULL.");
    r = -10;
    goto error;
  }

  if (NULL == profile) {
    TRAE("Cannot add the transcode profile to the list as it's NULL.");
    r = -20;
    goto error;
  }

  /* Validate the profile */
  if (0 == profile->width) {
    TRAE("Cannot add the transcode profile as the `width` is 0.");
    r = -30;
    goto error;
  }

  if (0 == profile->height) {
    TRAE("Cannot add the transcode profile as the `height` is 0.");
    r = -40;
    goto error;
  }

  new_array = realloc(list->profile_array, (list->profile_count + 1) * sizeof(tra_transcode_profile));
  if (NULL == new_array) {
    TRAE("Cannot add the transcode profile, we failed to grow the profile array.");
    r = -50;
    goto error;
  }

  list->profile_array = new_array;
  
  /* Copy the given profile into the array. */
  memcpy(list->profile_array + list->profile_count, profile, sizeof(tra_transcode_profile));

  list->profile_count = list->profile_count + 1;

 error:
  return r;
}

/* ------------------------------------------------------- */

int tra_transcode_list_print(tra_transcode_list* list) {

  tra_transcode_profile* profile = NULL;
  uint32_t i = 0;
  int r = 0;
  
  if (NULL == list) {
    TRAE("Cannot print the transcode list as the given `tra_transcode_list*` is NULL.");
    r = -10;
    goto error;
  }

  for (i = 0; i < list->profile_count; ++i) {

    profile = list->profile_array + i;

    TRAD("tra_transcode_profile");
    TRAD("  width: %u", profile->width);
    TRAD("  height: %u", profile->height);
    TRAD("  bitrate: %u", profile->bitrate);    
    TRAD("");
  }

 error:
  return r;
}

/* ------------------------------------------------------- */
