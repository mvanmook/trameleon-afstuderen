#ifndef TRA_API_H
#define TRA_API_H

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

  API
  ===

  GENERAL INFO:

    This `api.h` header helps us to export functions when
    building shared libraries. This is especially important when
    creating shared libraries (.dll) on Windows. When we're
    building a shared library on Windows we will set the
    `TRA_LIB_DLL` to `__declspec(dllexport)` which makes sure
    that the functions are exported. We do this this for both the
    tra library (which contains log.c, time.c, registry.c, etc)
    and the modules. For modules we use `TRA_MODULE_DLL` that can
    be used to export the `tra_load()` function of modules.
  
    For the modules we need to export the `tra_load()` function,
    otherwise we can't load it using the `GetProcAddress()`
    function on Windows.

  DEVELOPMENT INFO:

    Whenever you want a function to be exported so that a user
    can call the function when linking dynamically, you MUST put
    `TRA_LIB_DLL` in front of the function declaration. See
    `core.h` and the `registry.h` for an example and more
    details. As we want each encoder, decoder, etc. to be usable
    independently from the registry system you, as a developer,
    has to make sure that the functions that create, destroy,
    etc. your module use this macro.

    The registry is a system that makes use of load-time shared
    libraries. This means that when you use the registry (via the
    core functions, see `core.h`), we will load the shared
    library, then call a specific load function called
    `tra_load()` which registers the module (encoder, decoder,
    etc). with the registry. This is a somehwat different route
    you can take to load these modules.
  
  REFERENCES:

    [0] tra/register.h  "See registry.h for more info about dynamnic and static linking.

*/

/* ------------------------------------------------------- */

#if defined(_WIN32)
#  if defined(TRA_LIB_EXPORT)
#    define TRA_LIB_DLL __declspec(dllexport)
#  endif
#  if defined(TRA_LIB_IMPORT)
#    define TRA_LIB_DLL __declspec(dllimport)
#  endif
#  if defined(TRA_MODULE_EXPORT)
#    define TRA_MODULE_DLL __declspec(dllexport)
#  endif
#endif

/* ------------------------------------------------------- */

#if !defined(TRA_LIB_DLL)
#  define TRA_LIB_DLL
#endif

#if !defined(TRA_MODULE_DLL)
#  define TRA_MODULE_DLL 
#endif

/* ------------------------------------------------------- */

#endif /* TRA_API_H */
