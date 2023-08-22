#ifndef TRA_REGISTRY_H
#define TRA_REGISTRY_H

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

  REGISTRY
  ========

  GENERAL INFO:

    Trameleon works with modules. A module contains the code that
    is used to create an encoder, decoder, etc. Trameleon can be
    compiled statically, where we create one fat static lib with
    all the code of the modules in one file. We can also build
    Trameleon using shared libraries. Because you can use
    Trameleon with shared libraries and automatic module
    discovery but also using static linking, we have to provide
    two APIs. The goal is that you can use each module without
    the supporting libraries that are used to automatically load
    modules, gets function pointers etc.

    When using shared libraries, we create a load-lime shared
    library for the core functions and for each module we built a
    separate run-time loaded library. When using shared
    libraries, we call `tra_load()` which each module must
    implement. The registry will load the libraries during
    run-time and calls this initialisation function `tra_load()`.
    This allows each module to register its supported encoders,
    decoders, converters, etc.

    During development we've tried to create a solution where we
    created a static core library and shared module libraries. I
    did not find a (clean) solution to get this working on
    Windows. Therefore we either link everything statically or
    using run-time loadable and load-time shared libraries. See
    [this][0] document for more background information regarding
    shared and static linking and the limits we have on Windows.

    On Windows, when creating a load-time shared library we have
    to mark the functions that we want to export. In most cases,
    each module should export the functions that allow a user to
    use the module separately from all others. You use
    `__declspec(dllexport)` for this. Because we only want to
    export these functions when Trameleon is build using shared
    linkage we define `TRA_LIB_DLL` in the `tra/api.h` header.

    Whenever you want to export functions make sure to add
    `TRA_LIB_DLL` in front of the function name, in your header.
    For modules you use `TRA_MODULE_DLL` to export the
    `tra_load()` function. See `tra/modules/mft/mft.cpp` for an
    example where we export the `tra_load()` function.

  REFERENCES:

    [0] research-working-set.md "Working Set Notes"

 */

/* ------------------------------------------------------- */

#include <tra/api.h>

/* ------------------------------------------------------- */

typedef struct tra_registry        tra_registry;
typedef struct tra_decoder_api     tra_decoder_api;
typedef struct tra_encoder_api     tra_encoder_api;
typedef struct tra_graphics_api    tra_graphics_api;
typedef struct tra_interop_api     tra_interop_api;
typedef struct tra_converter_api   tra_converter_api;
typedef struct tra_easy_api        tra_easy_api;

/* ------------------------------------------------------- */

TRA_LIB_DLL int tra_registry_create(tra_registry** reg);
TRA_LIB_DLL int tra_registry_destroy(tra_registry* reg);

TRA_LIB_DLL int tra_registry_add_api(tra_registry* reg, const char* name, void* api); /* Add a general API. */
TRA_LIB_DLL int tra_registry_get_api(tra_registry* reg, const char* name, void** result); /* Get a general API. */
TRA_LIB_DLL int tra_registry_print_apis(tra_registry* reg);

TRA_LIB_DLL int tra_registry_add_decoder_api(tra_registry* reg, tra_decoder_api* api);
TRA_LIB_DLL int tra_registry_get_decoder_api(tra_registry* reg, const char* name, tra_decoder_api** result);
TRA_LIB_DLL int tra_registry_print_decoder_apis(tra_registry* reg);

TRA_LIB_DLL int tra_registry_add_encoder_api(tra_registry* reg, tra_encoder_api* api);
TRA_LIB_DLL int tra_registry_get_encoder_api(tra_registry* reg, const char* name, tra_encoder_api** result);
TRA_LIB_DLL int tra_registry_print_encoder_apis(tra_registry* reg);

TRA_LIB_DLL int tra_registry_add_graphics_api(tra_registry* reg, tra_graphics_api* api);
TRA_LIB_DLL int tra_registry_get_graphics_api(tra_registry* reg, const char* name, tra_graphics_api** result);
TRA_LIB_DLL int tra_registry_print_graphics_apis(tra_registry* reg);

TRA_LIB_DLL int tra_registry_add_interop_api(tra_registry* reg, tra_interop_api* api);
TRA_LIB_DLL int tra_registry_get_interop_api(tra_registry* reg, const char* name, tra_interop_api** result);
TRA_LIB_DLL int tra_registry_print_interop_apis(tra_registry* reg);

TRA_LIB_DLL int tra_registry_add_converter_api(tra_registry* reg, tra_converter_api* api);
TRA_LIB_DLL int tra_registry_get_converter_api(tra_registry* reg, const char* name, tra_converter_api** result);
TRA_LIB_DLL int tra_registry_print_converter_apis(tra_registry* reg);

TRA_LIB_DLL int tra_registry_add_easy_api(tra_registry* reg, tra_easy_api* api);
TRA_LIB_DLL int tra_registry_get_easy_api(tra_registry* reg, const char* name, tra_easy_api** result);
TRA_LIB_DLL int tra_registry_find_easy_api(tra_registry* reg, const char* name, tra_easy_api** result); /* Checks if af an easy API with the given name exists in the registry. Similar to the `get()` alternative, though this won't error when the API does not exist. */
TRA_LIB_DLL int tra_registry_print_easy_apis(tra_registry* reg);

/* ------------------------------------------------------- */

#endif
