/* ------------------------------------------------------- */

#include <stdlib.h>
#include <tra/log.h>
#include <tra/core.h>
#include <tra/registry.h>
#include <tra/module.h>

/* ------------------------------------------------------- */

struct tra_core {
  tra_registry* registry;
};
  
/* ------------------------------------------------------- */

int tra_core_create(tra_core_settings* cfg, tra_core** ctx) {

  int r = 0;
  tra_core* inst = NULL;
  
  if (NULL == ctx) {
    TRAE("Cannot create the `tra_core`: given result pointer is NULL.");
    return -1;
  }
  
  if (NULL != *ctx) {
    TRAE("Cannot create the `tra_core`: it seems you've already created the core or didn't initialize the variable to NULL.");
    return -2;
  }

  inst = calloc(1, sizeof(tra_core));
  if (NULL == inst) {
    TRAE("Failed to create the `tra_core`: failed to allocate the instance.");
    return -3;
  }

  r = tra_registry_create(&inst->registry);
  if (r < 0) {
    TRAE("Failed to create the `tra_core`: couldn't create the registry.");
    r = -4;
    goto error;
  }

  *ctx = inst;

 error:
  
  if (r < 0) {
    
    if (NULL != inst) {
      tra_core_destroy(inst);
    }

    if (NULL != ctx) {
      *ctx = NULL;
    }
  }

  return r;
}

/* ------------------------------------------------------- */

int tra_core_destroy(tra_core* ctx) {

  int result = 0;
  int r = 0;
  
  if (NULL == ctx) {
    TRAE("Cannot destroy the `tra_core`: given core is NULL.");
    return -1;
  }

  if (NULL != ctx->registry) {
    r = tra_registry_destroy(ctx->registry);
    if (r < 0) {
      TRAE("Failed to cleanly destroy the registry.");
      result -= 10;
    }
  }

  ctx->registry = NULL;
  
  free(ctx);
  ctx = NULL;

  return result;
}

/* ------------------------------------------------------- */

int tra_core_encoder_create(
  tra_core* ctx,
  const char* name,
  tra_encoder_settings* cfg,
  void* settings,
  tra_encoder** enc
)
{

  tra_encoder_api* api = NULL;
  int r = 0;
  
  if (NULL == ctx) {
    TRAE("Cannot create an encoder as the given `tra_core*` is NULL.");
    return -1;
  }

  if (NULL == name) {
    TRAE("Cannot create an encoder as the given API name is NULL.");
    return -2;
  }

  if (0 == strlen(name)) {
    TRAE("Cannot create an encoder as the given API name is empty.");
    return -3;
  }
  
  if (NULL == ctx->registry) {
    TRAE("Cannot create an encoder as it seems that the `tra_core::registry` member hasn't been initialized.");
    return -4;
  }

  r = tra_registry_get_encoder_api(ctx->registry, name, &api);
  if (r < 0) {
    TRAE("Cannot create an encoder as failed to find an encoder with the name `%s`.", name);
    return -5;
  }

  r = tra_encoder_create(api, cfg, settings, enc);
  if (r < 0) {
    TRAE("Failed to create an encoder instance for `%s`.", name);
    return -6;
  }
  
  return r;
}

/* ------------------------------------------------------- */

int tra_core_decoder_create(
  tra_core* ctx,
  const char* name,
  tra_decoder_settings* cfg,
  void* settings,
  tra_decoder** dec
)
{
  int r = 0;
  tra_decoder_api* api = NULL;
  
  if (NULL == ctx) {
    TRAE("Cannot create a decoder as the given `tra_core*` is NULL.");
    return -1;
  }

  if (NULL == name) {
    TRAE("Cannot create a decoder as the given API name is NULL.");
    return -2;
  }

  if (0 == strlen(name)) {
    TRAE("Cannot create a decoder as the given API name is empty.");
    return -3;
  }
  
  if (NULL == ctx->registry) {
    TRAE("Cannot create a decoder as it seems that the `tra_core::registry` member hasn't been initialized.");
    return -4;
  }

  r = tra_registry_get_decoder_api(ctx->registry, name, &api);
  if (r < 0) {
    TRAE("Cannot create a decoder as failed to find a decoder with the name `%s`.", name);
    return -5;
  }

  r = tra_decoder_create(api, cfg, settings, dec);
  if (r < 0) {
    TRAE("Failed to create a encoder instance for `%s`.", name);
    return -6;
  }
  
  return r;
}

/* ------------------------------------------------------- */

int tra_core_graphics_create(
  tra_core* ctx,
  const char* name,
  tra_graphics_settings* cfg,
  void* settings,
  tra_graphics** gfx
)
{
  tra_graphics_api* api = NULL;
  int r = 0;
  
  if (NULL == ctx) {
    TRAE("Cannot create a graphics instance as the given `tra_core*` is NULL.");
    return -1;
  }

  if (NULL == name) {
    TRAE("Cannot create a graphics instance as the given API name is NULL.");
    return -2;
  }

  if (0 == strlen(name)) {
    TRAE("Cannot create a graphics instance as the given API name is empty.");
    return -3;
  }
  
  if (NULL == ctx->registry) {
    TRAE("Cannot create a graphics instance as it seems that the `tra_core::registry` member hasn't been initialized.");
    return -4;
  }

  r = tra_registry_get_graphics_api(ctx->registry, name, &api);
  if (r < 0) {
    TRAE("Cannot create a graphics instance as failed to find a graphics with the name `%s`.", name);
    return -5;
  }

  r = tra_graphics_create(api, cfg, settings, gfx);
  if (r < 0) {
    TRAE("Failed to create a graphics instance for `%s`.", name);
    return -6;
  }
  
  return r;
}

/* ------------------------------------------------------- */

int tra_core_interop_create(
  tra_core* ctx,
  const char* name,
  tra_interop_settings* cfg,
  void* settings,
  tra_interop** interop
)
{
  tra_interop_api* api = NULL;
  int r = 0;
  
  if (NULL == ctx) {
    TRAE("Cannot create a interop as the given `tra_core*` is NULL.");
    return -1;
  }

  if (NULL == name) {
    TRAE("Cannot create a interop as the given API name is NULL.");
    return -2;
  }

  if (0 == strlen(name)) {
    TRAE("Cannot create a interop as the given API name is empty.");
    return -3;
  }
  
  if (NULL == ctx->registry) {
    TRAE("Cannot create a interop as it seems that the `tra_core::registry` member hasn't been initialized.");
    return -4;
  }

  r = tra_registry_get_interop_api(ctx->registry, name, &api);
  if (r < 0) {
    TRAE("Cannot create an interop as we failed to find an interop with the name `%s`.", name);
    return -5;
  }

  r = tra_interop_create(api, cfg, settings, interop);
  if (r < 0) {
    TRAE("Failed to create an interop instance for `%s`.", name);
    return -6;
  }
  
  return r;
}

/* ------------------------------------------------------- */

int tra_core_converter_create(
  tra_core* ctx,
  const char* name,
  tra_converter_settings* cfg,
  void* settings,
  tra_converter** converter
)
{
  tra_converter_api* api = NULL;
  int r = 0;
  
  if (NULL == ctx) {
    TRAE("Cannot create a converter as the given `tra_core*` is NULL.");
    return -1;
  }

  if (NULL == name) {
    TRAE("Cannot create a converter as the given API name is NULL.");
    return -2;
  }

  if (0 == strlen(name)) {
    TRAE("Cannot create a converter as the given API name is empty.");
    return -3;
  }
  
  if (NULL == ctx->registry) {
    TRAE("Cannot create a converter as it seems that the `tra_core::registry` member hasn't been initialized.");
    return -4;
  }

  r = tra_registry_get_converter_api(ctx->registry, name, &api);
  if (r < 0) {
    TRAE("Cannot create an converter as we failed to find an converter with the name `%s`.", name);
    return -5;
  }

  r = tra_converter_create(api, cfg, settings, converter);
  if (r < 0) {
    TRAE("Failed to create an converter instance for `%s`.", name);
    return -6;
  }
  
  return r;
}

/* ------------------------------------------------------- */

int tra_core_api_list(tra_core* ctx) {

  int r = 0;
  
  if (NULL == ctx) {
    TRAE("Cannot print the APIs as the given `tra_core` is NULL.");
    return -1;
  }

  if (NULL == ctx->registry) {
    TRAE("Cannot print the APIs as the given `tra_core::registry` member is NULL. Did you create the core?");
    return -2;
  }

  r = tra_registry_print_apis(ctx->registry);
  if (r < 0) {
    TRAE("Failed to print the APIs.");
    return -3;
  }

  r = tra_registry_print_decoder_apis(ctx->registry);
  if (r < 0) {
    TRAE("Failed to print the decoder APIs.");
    return -4;
  }

  r = tra_registry_print_encoder_apis(ctx->registry);
  if (r < 0) {
    TRAE("Failed to print the encoder APIs.");
    return -5;
  }

  return 0;
}

/* ------------------------------------------------------- */

int tra_core_api_get(tra_core* ctx, const char* name, void** result) {

  int r = 0;
  
  if (NULL == ctx) {
    TRAE("Cannot get an API: given `tra_core` is NULL.");
    return -1;
  }

  if (NULL == ctx->registry) {
    TRAE("Cannot get an API: the `tra_core::registry` member is NULL. Did you create the core?");
    return -2;
  }

  if (NULL == name) {
    TRAE("Cannot get an API as the given name is NULL.");
    return -3;
  }

  r = tra_registry_get_api(ctx->registry, name, result);
  if (r < 0) {
    TRAE("Failed to get the API: `%s`.", name);
    return -3;
  }

  return 0;
}

/* ------------------------------------------------------- */

/*
  
  This function will check if one of the modules has registered
  the `easy` API for the given name. A module, for example the
  NVIDIA module, can provide several functions which make its
  features available through the easy layer. The easy layer in
  Trameleon tries to hide as many steps as possible to quickly
  implement certain applications.

  This sets the `tra_easy_api` to the functions that manage the
  easy access to the implementation (like an encoder, decoder,
  etc).
  
 */
int tra_core_easy_get(tra_core* ctx, const char* name, tra_easy_api** result) {

  int r = 0;
  
  if (NULL == ctx) {
    TRAE("Cannot get the easy api as the given `tra_core*` is NULL.");
    return -10;
  }

  if (NULL == ctx->registry) {
    TRAE("Cannot get the easy api as the `tra_core::registry` member is NULL. Did you create the core?");
    return -20;
  }

  if (NULL == result) {
    TRAE("Cannot get the easy api as the given `name` is NULL.");
    return -30;
  }

  if (NULL == name) {
    TRAE("Cannot get the easy api as the given `name` is NULL.");
    return -40;
  }

  r = tra_registry_get_easy_api(ctx->registry, name, result);
  if (r < 0) {
    TRAE("Cannot get the easy api; something went wrong while trying to get the easy api for `%s`.", name);
    return -50;
  }

  return 0;
}

/* ------------------------------------------------------- */

/*
  Tries to find the given API name. When found it will be
  set. This is similar to `tra_core_easy_get()`, though this
  won't result in an error when the API wasn't found; it will
  make sure that `result` will be set to NULL when the API was
  not found.
*/
int tra_core_easy_find(tra_core* ctx, const char* name, tra_easy_api** result) {

  int r = 0;

  if (NULL == ctx) {
    TRAE("Cannot find the easy api as the given `tra_core*` is NULL.");
    return -10;
  }

  if (NULL == ctx->registry) {
    TRAE("Cannot find the easy api as the `tra_core::registry` member is NULL. Did you create the core?");
    return -20;
  }

  if (NULL == result) {
    TRAE("Cannot find the easy api as the given output argument is NULL.");
    return -30;
  }

  if (NULL == name) {
    TRAE("Cannot find the easy api as the given `name` is NULL.");
    return -40;
  }

  r = tra_registry_find_easy_api(ctx->registry, name, result);
  if (r < 0) {
    TRAE("Cannot find the easy api; something went wrong while trying to get the easy api for `%s`.", name);
    return -50;
  }

  return 0;
}

/* ------------------------------------------------------- */
