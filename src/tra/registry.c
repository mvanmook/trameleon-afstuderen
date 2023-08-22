#if defined(__linux) || defined(__APPLE__)
#  include <dirent.h>
#  include <errno.h>
#endif

#include <stdio.h>
#include <stdlib.h>

#include <tra/log.h>
#include <tra/utils.h>
#include <tra/module.h>
#include <tra/registry.h>

/* ------------------------------------------------------- */

#define TRA_API_NAME_MAX_SIZE 16

/* ------------------------------------------------------- */

typedef struct tra_custom_api {
  char name[TRA_API_NAME_MAX_SIZE];
  void* api;
} tra_custom_api;

/* ------------------------------------------------------- */

struct tra_registry {
  tra_custom_api* custom_apis;
  tra_decoder_api** decoder_apis;
  tra_encoder_api** encoder_apis;
  tra_graphics_api** graphics_apis;
  tra_interop_api** interop_apis;
  tra_converter_api** converter_apis;
  tra_easy_api** easy_apis;
  uint32_t num_custom_apis;
  uint32_t num_decoder_apis;
  uint32_t num_encoder_apis;
  uint32_t num_graphics_apis;
  uint32_t num_interop_apis;
  uint32_t num_converter_apis;
  uint32_t num_easy_apis;
};

/* ------------------------------------------------------- */

static int registry_load_modules(tra_registry* reg);
static int registry_load_module(tra_registry* reg, const char* lib);

/* ------------------------------------------------------- */

int tra_registry_create(tra_registry** reg) {

  tra_registry* inst = NULL;
  int status = 0;
  int r = 0;

  if (NULL == reg) {
    TRAE("Cannot create the `tra_registry`: given result pointer is NULL.");
    return -10;
  }

  if (NULL != *reg) {
    TRAE("Cannot create hte `tra_registry`: it seems you've already created the registry or didn't initialize the result to NULL.");
    return -20;
  }

  inst = calloc(1, sizeof(tra_registry));
  if (NULL == inst) {
    TRAE("Cannot create the `tra_registry`: failed to allocate the instance.");
    return -30;
  }

  /* Scan directory for modules. */
  r = registry_load_modules(inst);
  if (r < 0) {
    TRAE("Failed to load the modules.");
    r = -40;
    goto error;
  }

  *reg = inst;

 error:

  if (r < 0) {
    
    if (NULL != inst) {
    
      status = tra_registry_destroy(inst);
      if (status < 0) {
        TRAE("After failling to create the registry, we also failed to cleanly destroy it.");
        r = -50;
      }
    
      inst = NULL;
    }

    if (NULL != reg) {
      *reg = NULL;
    }
  }
  
  return r;
}

/* ------------------------------------------------------- */

int tra_registry_destroy(tra_registry* reg) {

  TRAE("@todo we have to make sure that all instances have been deallocated. ");
  
  if (NULL == reg) {
    TRAE("Cannot destroy the `tra_registry`: given registry is NULL.");
    return -10;
  }

  if (NULL != reg->decoder_apis) {
    free(reg->decoder_apis);
  } 

  if (NULL != reg->encoder_apis) {
    free(reg->encoder_apis);
  }

  reg->decoder_apis = NULL;
  reg->encoder_apis = NULL;
  reg->num_decoder_apis = 0;
  reg->num_encoder_apis = 0;

  free(reg);
  reg = NULL;
  
  return 0;
}

/* ------------------------------------------------------- */

int tra_registry_add_decoder_api(tra_registry* reg, tra_decoder_api* api) {

  tra_decoder_api** tmp = NULL;
  
  if (NULL == reg) {
    TRAE("Cannot add a decoder api: the given `tra_registry*` is NULL.");
    return -10;
  }

  if (NULL == api) {
    TRAE("Cannot add a decoder api: the given `tra_decoder_api*` is NULL.");
    return -20;
  }

  if (NULL == api->create) {
    TRAE("Cannot add the decoder api: the `create()` function is not set.");
    return -30;
  }

  if (NULL == api->destroy) {
    TRAE("Cannot add the decoder api: the `destroy()` function is not set.");
    return -40;
  }

  if (NULL == api->decode) {
    TRAE("Cannot add the decoder api: the `decode()` function is not set.");
    return -50;
  }

  /* Allocate space for the decoder. */
  tmp = realloc(reg->decoder_apis, (reg->num_decoder_apis + 1) * sizeof(tra_decoder_api*));
  if (NULL == tmp) {
    TRAE("Cannot add the decoder api: failed to allocate storage.");
    return -60;
  }

  reg->decoder_apis = tmp;
  reg->decoder_apis[reg->num_decoder_apis] = api;
  reg->num_decoder_apis = reg->num_decoder_apis + 1;
  
  return 0;
}

int tra_registry_get_decoder_api(tra_registry* reg, const char* name, tra_decoder_api** result) {

  tra_decoder_api* api = NULL;
  uint32_t i = 0;
  int r = 0;
  
  if (NULL == reg) {
    TRAE("Cannot get the decoder api, given `tra_registry` is NULL.");
    return -10;
  }

  if (NULL == name) {
    TRAE("Cannot get the decoder api, given `name` is NULL.");
    return -20;
  }

  if (0 == strlen(name)) {
    TRAE("Cannot get the decoder api, given `name` is empty.");
    return -30;
  }

  if (NULL == result) {
    TRAE("Cannot get the decoder api, given result `tra_decoder_api**` is NULL.");
    return -40;
  }

  if (NULL != (*result)) {
    TRAE("Cannot get the decoder api, given result `*tra_decoder_api**` is NOT NULL. Did you initialize your variable to NULL?");
    return -50;
  }

  for (i = 0; i < reg->num_decoder_apis; ++i) {
    
    api = reg->decoder_apis[i];

    r = strcmp(name, api->get_name());
    if (0 == r) {
      *result = api;
      return 0;
    }
  }

  TRAE("Cannot get the decoder API `%s`. We didn't find an decoder API with that name.", name);
  
  return -60;
}

/* ------------------------------------------------------- */

int tra_registry_print_decoder_apis(tra_registry* reg) {

  uint32_t i = 0;
  tra_decoder_api* api = NULL;
  
  if (NULL == reg) {
    TRAE("Cannot print the decoders APIs, given `tra_registry` is NULL.");
    return -10;
  }

  for (i = 0; i < reg->num_decoder_apis; ++i) {
    api = reg->decoder_apis[i];
    TRAD("Decoder: %s", api->get_name());
  }

  return 0;
}

/* ------------------------------------------------------- */

int tra_registry_add_encoder_api(tra_registry* reg, tra_encoder_api* api) {

  tra_encoder_api** tmp = NULL;

  if (NULL == reg) {
    TRAE("Cannot add an encoder api: the given `tra_registry*` is NULL.");
    return -10;
  }

  if (NULL == api) {
    TRAE("Cannot add an encoder api: the given `tra_encoder_api*` is NULL.");
    return -20;
  }

  if (NULL == api->create) {
    TRAE("Cannot add the given encoder api; the `create()` function is not set.");
    return -30;
  }

  if (NULL == api->destroy) {
    TRAE("Cannot add the given encoder api; the `destroy()` function is not set.");
    return -40;
  }
  
  if (NULL == api->encode) {
    TRAE("Cannot add the given encoder api; the `encode()` function is not set.");
    return -50;
  }

  if (NULL == api->flush) {
    TRAE("Cannot add the given encoder api; the `flush()` function is not set.");
    return -60;
  }

  tmp = realloc(reg->encoder_apis, (reg->num_encoder_apis + 1) * sizeof(tra_encoder_api));
  if (NULL == tmp) {
    TRAE("Cannot add the encoder: failed to allocate storage.");
    return -70;
  }

  reg->encoder_apis = tmp;
  reg->encoder_apis[reg->num_encoder_apis] = api;
  reg->num_encoder_apis = reg->num_encoder_apis + 1;

  return 0;
}

/* ------------------------------------------------------- */

int tra_registry_print_encoder_apis(tra_registry* reg) {

  uint32_t i = 0;
  tra_encoder_api* api = NULL;
  
  if (NULL == reg) {
    TRAE("Cannot print the encoders APIs, given `tra_registry` is NULL.");
    return -10;
  }

  for (i = 0; i < reg->num_encoder_apis; ++i) {
    api = reg->encoder_apis[i];
    TRAD("Encoder: %s", api->get_name());
  }

  return 0;
}

/* ------------------------------------------------------- */

int tra_registry_get_encoder_api(tra_registry* reg, const char* name, tra_encoder_api** result) {

  tra_encoder_api* api = NULL;
  uint32_t i = 0;
  int r = 0;
  
  if (NULL == reg) {
    TRAE("Cannot get the encoder api, given `tra_registry` is NULL.");
    return -10;
  }

  if (NULL == name) {
    TRAE("Cannot get the encoder api, given `name` is NULL.");
    return -20;
  }

  if (0 == strlen(name)) {
    TRAE("Cannot get the encoder api, given `name` is empty.");
    return -30;
  }

  if (NULL == result) {
    TRAE("Cannot get the encoder api, given result `tra_encoder_api**` is NULL.");
    return -40;
  }

  if (NULL != (*result)) {
    TRAE("Cannot get the encoder api, given result `*tra_encoder_api**` is NOT NULL. Did you initialize your variable to NULL?");
    return -50;
  }

  for (i = 0; i < reg->num_encoder_apis; ++i) {
    
    api = reg->encoder_apis[i];

    r = strcmp(name, api->get_name());
    if (0 == r) {
      *result = api;
      return 0;
    }
  }

  TRAE("Cannot get the encoder API `%s`. We didn't find an encoder API with that name.", name);
  
  return -60;
}

/* ------------------------------------------------------- */

int tra_registry_add_graphics_api(tra_registry* reg, tra_graphics_api* api) {

  tra_graphics_api** tmp = NULL;

  TRAE("@todo check all required api functions for the graphics layer.");
  
  if (NULL == reg) {
    TRAE("Cannot add a graphics api: the given `tra_registry*` is NULL.");
    return -10;
  }

  if (NULL == api) {
    TRAE("Cannot add a graphics api: the given `tra_graphics_api*` is NULL.");
    return -20;
  }

  if (NULL == api->create) {
    TRAE("Cannot add the graphics api: the `create()` function is not set.");
    return -30;
  }

  if (NULL == api->destroy) {
    TRAE("Cannot add the graphics api: the `destroy()` function is not set.");
    return -40;
  }

  if (NULL == api->draw) {
    TRAE("Cannot add the graphics api: the `draw()` function is not set.");
    return -50;
  }

  /* Allocate space for the graphics. */
  tmp = realloc(reg->graphics_apis, (reg->num_graphics_apis + 1) * sizeof(tra_graphics_api*));
  if (NULL == tmp) {
    TRAE("Cannot add the graphics api: failed to allocate storage.");
    return -60;
  }

  reg->graphics_apis = tmp;
  reg->graphics_apis[reg->num_graphics_apis] = api;
  reg->num_graphics_apis = reg->num_graphics_apis + 1;
  
  return 0;
}

/* ------------------------------------------------------- */

int tra_registry_get_graphics_api(tra_registry* reg, const char* name, tra_graphics_api** result) {

  tra_graphics_api* api = NULL;
  uint32_t i = 0;
  int r = 0;
  
  if (NULL == reg) {
    TRAE("Cannot get the graphics api, given `tra_registry` is NULL.");
    return -10;
  }

  if (NULL == name) {
    TRAE("Cannot get the graphics api, given `name` is NULL.");
    return -20;
  }

  if (0 == strlen(name)) {
    TRAE("Cannot get the graphics api, given `name` is empty.");
    return -30;
  }

  if (NULL == result) {
    TRAE("Cannot get the graphics api, given result `tra_graphics_api**` is NULL.");
    return -40;
  }

  if (NULL != (*result)) {
    TRAE("Cannot get the graphics api, given result `*tra_graphics_api**` is NOT NULL. Did you initialize your variable to NULL?");
    return -50;
  }

  for (i = 0; i < reg->num_graphics_apis; ++i) {
    
    api = reg->graphics_apis[i];

    r = strcmp(name, api->get_name());
    if (0 == r) {
      *result = api;
      return 0;
    }
  }

  TRAE("Cannot get the graphics API `%s`. We didn't find an graphics API with that name.", name);
  
  return -60;
}

/* ------------------------------------------------------- */

int tra_registry_print_graphics_apis(tra_registry* reg) {

  uint32_t i = 0;
  tra_graphics_api* api = NULL;
  
  if (NULL == reg) {
    TRAE("Cannot print the graphicss APIs, given `tra_registry` is NULL.");
    return -10;
  }

  for (i = 0; i < reg->num_graphics_apis; ++i) {
    api = reg->graphics_apis[i];
    TRAD("Graphics: %s", api->get_name());
  }

  return 0;
}

/* ------------------------------------------------------- */

int tra_registry_add_interop_api(tra_registry* reg, tra_interop_api* api) {

  tra_interop_api** tmp = NULL;
  
  if (NULL == reg) {
    TRAE("Cannot add a interop api: the given `tra_registry*` is NULL.");
    return -10;
  }

  if (NULL == api) {
    TRAE("Cannot add a interop api: the given `tra_interop_api*` is NULL.");
    return -20;
  }

  if (NULL == api->create) {
    TRAE("Cannot add the interop api: the `create()` function is not set.");
    return -30;
  }

  if (NULL == api->destroy) {
    TRAE("Cannot add the interop api: the `destroy()` function is not set.");
    return -40;
  }

  if (NULL == api->upload) {
    TRAE("Cannot add the interop api: the `uplaod()` function is not set.");
    return -50;
  }

  /* Allocate space for the interop. */
  tmp = realloc(reg->interop_apis, (reg->num_interop_apis + 1) * sizeof(tra_interop_api*));
  if (NULL == tmp) {
    TRAE("Cannot add the interop api: failed to allocate storage.");
    return -60;
  }

  reg->interop_apis = tmp;
  reg->interop_apis[reg->num_interop_apis] = api;
  reg->num_interop_apis = reg->num_interop_apis + 1;
  
  return 0;
}

/* ------------------------------------------------------- */

int tra_registry_get_interop_api(tra_registry* reg, const char* name, tra_interop_api** result) {

  tra_interop_api* api = NULL;
  uint32_t i = 0;
  int r = 0;
  
  if (NULL == reg) {
    TRAE("Cannot get the interop api, given `tra_registry` is NULL.");
    return -10;
  }

  if (NULL == name) {
    TRAE("Cannot get the interop api, given `name` is NULL.");
    return -20;
  }

  if (0 == strlen(name)) {
    TRAE("Cannot get the interop api, given `name` is empty.");
    return -30;
  }

  if (NULL == result) {
    TRAE("Cannot get the interop api, given result `tra_interop_api**` is NULL.");
    return -40;
  }

  if (NULL != (*result)) {
    TRAE("Cannot get the interop api, given result `*tra_interop_api**` is NOT NULL. Did you initialize your variable to NULL?");
    return -50;
  }

  for (i = 0; i < reg->num_interop_apis; ++i) {
    
    api = reg->interop_apis[i];

    r = strcmp(name, api->get_name());
    if (0 == r) {
      *result = api;
      return 0;
    }
  }

  TRAE("Cannot get the interop API `%s`. We didn't find an interop API with that name.", name);
  
  return -60;
}

/* ------------------------------------------------------- */

int tra_registry_print_interop_apis(tra_registry* reg) {

  uint32_t i = 0;
  tra_interop_api* api = NULL;
  
  if (NULL == reg) {
    TRAE("Cannot print the interops APIs, given `tra_registry` is NULL.");
    return -10;
  }

  for (i = 0; i < reg->num_interop_apis; ++i) {
    api = reg->interop_apis[i];
    TRAD("Interop: %s", api->get_name());
  }

  return 0;
}

/* ------------------------------------------------------- */

int tra_registry_add_converter_api(tra_registry* reg, tra_converter_api* api) {

  tra_converter_api** tmp = NULL;
  
  if (NULL == reg) {
    TRAE("Cannot add a converter api: the given `tra_registry*` is NULL.");
    return -10;
  }

  if (NULL == api) {
    TRAE("Cannot add a converter api: the given `tra_converter_api*` is NULL.");
    return -20;
  }

  if (NULL == api->create) {
    TRAE("Cannot add the converter api: the `create()` function is not set.");
    return -30;
  }

  if (NULL == api->convert) {
    TRAE("Cannot add the converter api: the `convert()` function is not set.");
    return -40;
  }

  /* Allocate space for the converter. */
  tmp = realloc(reg->converter_apis, (reg->num_converter_apis + 1) * sizeof(tra_converter_api*));
  if (NULL == tmp) {
    TRAE("Cannot add the converter api: failed to allocate storage.");
    return -50;
  }

  reg->converter_apis = tmp;
  reg->converter_apis[reg->num_converter_apis] = api;
  reg->num_converter_apis = reg->num_converter_apis + 1;
  
  return 0;
}

/* ------------------------------------------------------- */

int tra_registry_get_converter_api(tra_registry* reg, const char* name, tra_converter_api** result) {

  tra_converter_api* api = NULL;
  uint32_t i = 0;
  int r = 0;
  
  if (NULL == reg) {
    TRAE("Cannot get the converter api, given `tra_registry` is NULL.");
    return -10;
  }

  if (NULL == name) {
    TRAE("Cannot get the converter api, given `name` is NULL.");
    return -20;
  }

  if (0 == strlen(name)) {
    TRAE("Cannot get the converter api, given `name` is empty.");
    return -30;
  }

  if (NULL == result) {
    TRAE("Cannot get the converter api, given result `tra_converter_api**` is NULL.");
    return -40;
  }

  if (NULL != (*result)) {
    TRAE("Cannot get the converter api, given result `*tra_converter_api**` is NOT NULL. Did you initialize your variable to NULL?");
    return -50;
  }

  for (i = 0; i < reg->num_converter_apis; ++i) {
    
    api = reg->converter_apis[i];

    r = strcmp(name, api->get_name());
    if (0 == r) {
      *result = api;
      return 0;
    }
  }

  TRAE("Cannot get the converter API `%s`. We didn't find an converter API with that name.", name);
  
  return -60;
}

/* ------------------------------------------------------- */

int tra_registry_print_converter_apis(tra_registry* reg) {

  uint32_t i = 0;
  tra_converter_api* api = NULL;
  
  if (NULL == reg) {
    TRAE("Cannot print the converters APIs, given `tra_registry` is NULL.");
    return -10;
  }

  for (i = 0; i < reg->num_converter_apis; ++i) {
    api = reg->converter_apis[i];
    TRAD("Converter: %s", api->get_name());
  }

  return 0;
}

/* ------------------------------------------------------- */

int tra_registry_add_easy_api(tra_registry* reg, tra_easy_api* api) {

  tra_easy_api** tmp = NULL;
  
  if (NULL == reg) {
    TRAE("Cannot add a easy api: the given `tra_registry*` is NULL.");
    return -10;
  }

  if (NULL == api) {
    TRAE("Cannot add a easy api: the given `tra_easy_api*` is NULL.");
    return -20;
  }

  /* Allocate space for the easy. */
  tmp = realloc(reg->easy_apis, (reg->num_easy_apis + 1) * sizeof(tra_easy_api*));
  if (NULL == tmp) {
    TRAE("Cannot add the easy api: failed to allocate storage.");
    return -50;
  }

  reg->easy_apis = tmp;
  reg->easy_apis[reg->num_easy_apis] = api;
  reg->num_easy_apis = reg->num_easy_apis + 1;
  
  return 0;
}

/* ------------------------------------------------------- */

int tra_registry_get_easy_api(tra_registry* reg, const char* name, tra_easy_api** result) {

  tra_easy_api* api = NULL;
  uint32_t i = 0;
  int r = 0;
  
  if (NULL == reg) {
    TRAE("Cannot get the easy api, given `tra_registry` is NULL.");
    return -10;
  }

  if (NULL == name) {
    TRAE("Cannot get the easy api, given `name` is NULL.");
    return -20;
  }

  if (0 == strlen(name)) {
    TRAE("Cannot get the easy api, given `name` is empty.");
    return -30;
  }

  if (NULL == result) {
    TRAE("Cannot get the easy api, given result `tra_easy_api**` is NULL.");
    return -40;
  }

  if (NULL != (*result)) {
    TRAE("Cannot get the easy api, given result `*tra_easy_api**` is NOT NULL. Did you initialize your variable to NULL?");
    return -50;
  }

  for (i = 0; i < reg->num_easy_apis; ++i) {
    
    api = reg->easy_apis[i];

    r = strcmp(name, api->get_name());
    if (0 == r) {
      *result = api;
      return 0;
    }
  }

  TRAE("Cannot get the easy API `%s`. We didn't find an easy API with that name.", name);
  
  return -60;
}

/* ------------------------------------------------------- */

/*
  Similar to `tra_register_get_easy_api()`, though this function
  will not return an error when the API wasn't found. When the
  API wasn't found we simply return 0 and set `result` to
  NULL. This function should be used in cases when you expect
  that an API does not exist. This is particularly the case for
  the easy layer as we don't know what hardware the API runs on.

  @todo we might want to add some logic into a function that is
  used by both `tra_registry_get_easy_api` and
  `tra_registry_find_easy_api` as the only difference is the
  final return value.
  
 */
int tra_registry_find_easy_api(tra_registry* reg, const char* name, tra_easy_api** result) {

  tra_easy_api* api = NULL;
  uint32_t i = 0;
  int r = 0;
  
  if (NULL == reg) {
    TRAE("Cannot find the easy api, given `tra_registry` is NULL.");
    return -10;
  }

  if (NULL == name) {
    TRAE("Cannot find the easy api, given `name` is NULL.");
    return -20;
  }

  if (0 == strlen(name)) {
    TRAE("Cannot find the easy api, given `name` is empty.");
    return -30;
  }

  if (NULL == result) {
    TRAE("Cannot find the easy api, given result `tra_easy_api**` is NULL.");
    return -40;
  }

  if (NULL != (*result)) {
    TRAE("Cannot find the easy api, given result `*tra_easy_api**` is NOT NULL. Did you initialize your variable to NULL?");
    return -50;
  }

  for (i = 0; i < reg->num_easy_apis; ++i) {
    
    api = reg->easy_apis[i];

    r = strcmp(name, api->get_name());
    if (0 == r) {
      *result = api;
      return 0;
    }
  }

  return 0;
}

/* ------------------------------------------------------- */

int tra_registry_print_easy_apis(tra_registry* reg) {

  uint32_t i = 0;
  tra_easy_api* api = NULL;
  
  if (NULL == reg) {
    TRAE("Cannot print the easys APIs, given `tra_registry` is NULL.");
    return -10;
  }

  for (i = 0; i < reg->num_easy_apis; ++i) {
    api = reg->easy_apis[i];
    TRAD("Easy: %s", api->get_name());
  }

  return 0;
}

/* ------------------------------------------------------- */

int tra_registry_add_api(tra_registry* reg, const char* name, void* api) {

  int r = 0;
  tra_custom_api* tmp = NULL;
  tra_custom_api* inst = NULL;
  
  if (NULL == reg) {
    TRAE("Cannot add a custom api: given `tra_registry` is NULL.");
    return -1;
  }

  if (NULL == name) {
    TRAE("Cannot add a custom api: given `name` is NULL.");
    return -2;
  }

  if (0 == strlen(name)) {
    TRAE("Cannot add a custom api: given `name` is empty.");
    return -3;
  }

  if (strlen(name) + 1 >= TRA_API_NAME_MAX_SIZE) {
    TRAE("Cannot add the custom api: the given `name` is too large, can only be `%zu` bytes.", TRA_API_NAME_MAX_SIZE);
    return -4;
  }

  tmp = realloc(reg->custom_apis, (reg->num_custom_apis + 1) * sizeof(tra_custom_api));
  if (NULL == tmp) {
    TRAE("Cannot add a custom api: failed to allocate storage.");
    return -5;
  }

  /* Update our array with custom APIs. */
  reg->custom_apis = tmp;

  /* Set the members for the given API. */
  inst = reg->custom_apis + reg->num_custom_apis;
  inst->api = api;
  reg->num_custom_apis = reg->num_custom_apis + 1;
  
  strncpy(inst->name, name, sizeof(inst->name));

  return r;
}

int tra_registry_get_api(tra_registry* reg, const char* name, void** result) {

  int r = 0;
  uint32_t i = 0;
  tra_custom_api* inst = NULL;

  if (NULL == reg) {
    TRAE("Cannot get an api: given `tra_registry*` is NULL.");
    return -1;
  }

  if (NULL == name) {
    TRAE("Cannot get an api: given `name` is NULL.");
    return -2;
  }

  if (0 == strlen(name)) {
    TRAE("Cannot get an api: given `name` is empty.");
    return -3;
  }

  if (NULL == result) {
    TRAE("Cannot get an api: given `result` is NULL.");
    return -4;
  }

  if (NULL != (*result)) {
    TRAE("Cannot get an api: given `result` is not initialized to NULL. ");
    return -5;
  }

  for (i = 0; i < reg->num_custom_apis; ++i) {

    inst = reg->custom_apis + i;

    r = strcmp(name, inst->name);
    if (0 == r) {
      *result = inst->api;
      return 0;
    }
  }

  TRAE("Cannot get an api: the api `%s` was not found..", name);
  
  return -6;
}

int tra_registry_print_apis(tra_registry* reg) {

  uint32_t i = 0;
  tra_custom_api* api = NULL;
  
  if (NULL == reg) {
    TRAE("Cannot print the APIs as the given `tra_registry*` is NULL.");
    return -1;
  }

  for (i = 0; i < reg->num_custom_apis; ++i) {
    tra_custom_api* api = reg->custom_apis + i;
    TRAD("API: %s", api->name);
  }
  
  return 0;
}

/* ------------------------------------------------------- */

#if defined(_WIN32)

static int registry_load_modules(tra_registry* reg) {

  WIN32_FIND_DATA find_data = { 0 };
  HANDLE find_handle = NULL;
  BOOL find_next = FALSE;
  char path[1024] = { 0 };
  int r = 0;
  
  TRAE("@todo implement registry_load_modules on Windows.");

  find_handle = FindFirstFile("./../lib/*.dll", &find_data);
  if (INVALID_HANDLE_VALUE == find_handle) {
    TRAE("Cannot load moduels, failed to open a find handle to the `./../lib/` dir.");
    r = -10;
    goto error;
  }

  do {
    
    TRAD(" %s", find_data.cFileName);

    /* Construct the path to the module that we want to load. */
    r = snprintf(path, sizeof(path), "./../lib/%s", find_data.cFileName);
    if (r < 0) {
      TRAE("Failed to create the path to the loadable module.");
      r = -20;
      goto error;
    }

    r = registry_load_module(reg, path);
    if (r < 0) {
      TRAE("Failed to load a module: `%s`.", path);
      r = -30;
      goto error;
    }

    find_next = FindNextFile(find_handle, &find_data);
    
  } while (TRUE == find_next);
           
 error:

  if (NULL != find_handle) {
    FindClose(find_handle);
    find_handle = NULL;
  }
  
  return r;
}

#endif

#if defined(__linux) || defined(__APPLE_)

static int registry_load_modules(tra_registry* reg) {

  const char* lib_prefix = "libtra-"; /* Currently the modules must start with this on Linux. */
  const char* lib_path = "./../lib";
  struct dirent* entry = NULL;
  char path[1024] = { 0 };
  DIR* dir = NULL;
  int r = 0;

  dir = opendir(lib_path);
  if (NULL == dir) {
    TRAE("Failed to open the directory (%s) from where we want to load modules: %s", lib_path, strerror(errno));
    return -1;
  }

  while ( (entry = readdir(dir)) ) {

    /* Check if the file entry starts with our prefix. */
    r = strncmp(entry->d_name, lib_prefix, strlen(lib_prefix));
    if (0 != r) {
      continue;
    }

    /* Construct the path to the module that we want to load. */
    r = snprintf(path, sizeof(path), "%s/%s", lib_path, entry->d_name);
    if (r < 0) {
      TRAE("Failed to create the path to the loadable module.");
      continue;
    }

    r = registry_load_module(reg, path);
    if (r < 0) {
      TRAE("Failed to load a module: `%s`.", path);
      continue;
    }

    TRAD("Loaded module `%s`.", path);
  }
  
  return 0;
}
#endif /* _linux || APPLE __ */

/* ------------------------------------------------------- */

static int registry_load_module(tra_registry* reg, const char* lib) {

  int r = 0;
  void* module = NULL;
  tra_load_func* load = NULL;
  
  if (NULL == reg) {
    TRAE("Cannot load a module, given `tra_registry*` is NULL.");
    return -1;
  }

  if (NULL == lib) {
    TRAE("Cannot load the module, given lib path is NULL.");
    return -2;
  }

  if (0 == strlen(lib)) {
    TRAE("Cannot load the module, given path is empty.");
    return -3;
  }

  r = tra_load_module(lib, &module);
  if (r < 0) {
    TRAE("Failed to load the module from `%s`.", lib);
    return -4;
  }

  r = tra_load_function(module, "tra_load", (void**)&load);
  if (r < 0) {
    TRAE("Failed to load the `tra_load()` function for the module `%s`.", lib);
    return -5;
  }

  r = load(reg);
  if (r < 0) {
    TRAE("Failed to load the module from `%s`.", lib);
    return -6;
  }
 
  return 0;
}

/* ------------------------------------------------------- */
