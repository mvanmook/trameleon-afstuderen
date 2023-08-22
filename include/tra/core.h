#ifndef TRA_CORE_H
#define TRA_CORE_H

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

  CORE
  ====

  GENERAL INFO:

    The core is the "heart" of the Trameleon API. Currently, its
    most important job is to create encoders, decoder,
    converters, etc. When you create the instance of `tra_core`
    it will search for shared libraries and calls `tra_load()` on
    each of the found modules. To do this, it uses the
    `tra_registry`.

  DEVELOPMENT INFO:

    Currently we only implement `*_create()` functions that act
    as factory methods for things like encoders, decoders,
    converters, etc. When you want to deallocate these instances,
    use the `tra_[type]_destroy()` function of the specific type,
    for example `tra_decoder_destroy()`.

  TODO:

    @todo(roxlu): Currently we've hardcoded the path from where
    we load the shared libraries. We have to make this a setting
    that can be passed into `tra_core_create()`.
  
 */

/* ------------------------------------------------------- */

#include <tra/api.h>
#include <tra/transcoder.h>

/* ------------------------------------------------------- */

typedef struct tra_core                  tra_core;
typedef struct tra_core_settings         tra_core_settings;
typedef struct tra_encoder_settings      tra_encoder_settings;
typedef struct tra_encoder               tra_encoder;
typedef struct tra_decoder               tra_decoder;
typedef struct tra_decoder_settings      tra_decoder_settings;
typedef struct tra_graphics              tra_graphics;
typedef struct tra_graphics_settings     tra_graphics_settings;
typedef struct tra_interop               tra_interop;
typedef struct tra_interop_settings      tra_interop_settings;
typedef struct tra_converter             tra_converter;
typedef struct tra_converter_settings    tra_converter_settings;
typedef struct tra_easy_api              tra_easy_api;

/* ------------------------------------------------------- */

struct tra_core_settings {
  uint8_t dummy; /* As we currently have no settings we need to add one dummy member to conform the C standard (w/o this won't compile on Windows). */
};

/* ------------------------------------------------------- */

TRA_LIB_DLL int tra_core_create(tra_core_settings* cfg, tra_core** ctx);
TRA_LIB_DLL int tra_core_destroy(tra_core* ctx);
TRA_LIB_DLL int tra_core_encoder_create(tra_core* ctx, const char* api, tra_encoder_settings* cfg, void* settings, tra_encoder** enc);
TRA_LIB_DLL int tra_core_decoder_create(tra_core* ctx, const char* api, tra_decoder_settings* cfg, void* settings, tra_decoder** dec);
TRA_LIB_DLL int tra_core_transcoder_create(tra_core* ctx, const char* decoder_api, tra_decoder_settings* decoder_cfg, void* decoder_settings, const char* encoder_api, tra_encoder_settings* encoder_cfg, void* encoder_settings, tra_transcoder** trans);
TRA_LIB_DLL int tra_core_graphics_create(tra_core* ctx, const char* api, tra_graphics_settings* cfg, void* settings, tra_graphics** gfx);
TRA_LIB_DLL int tra_core_interop_create(tra_core* ctx, const char* api, tra_interop_settings* cfg, void* settings, tra_interop** inter);
TRA_LIB_DLL int tra_core_converter_create(tra_core* ctx, const char* api, tra_converter_settings* cfg, void* settings, tra_converter** converter);
TRA_LIB_DLL int tra_core_api_list(tra_core* ctx);
TRA_LIB_DLL int tra_core_api_get(tra_core* ctx, const char* name, void** result); /* Get a generic API; can be any API that a module registers. This can be e.g. an API which provides OpenGL features, or CUDA, or ... It's up to the implementer to implement what he finds necessary. */
TRA_LIB_DLL int tra_core_easy_get(tra_core* ctx, const char* name, tra_easy_api** result); /* Get a pointer to an easy API implementation. */
TRA_LIB_DLL int tra_core_easy_find(tra_core* ctx, const char* name, tra_easy_api** result); /* Tries to find the given API name. When found it will be set. */
                      
/* ------------------------------------------------------- */

#endif
