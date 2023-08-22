#ifndef TRA_EASY_H
#define TRA_EASY_H
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

  GENERAL INFO:

   The "easy" layer of Trameleon provides an API that can be used
   to quickly build an application that makes use of the features
   that Trameleon provides. For example think about an
   application that wants to decode and render a H264 stream. Or
   an application that wants to transcode an input into several
   outputs. These are typical use cases for when you might want
   to use the easy layer.

   The goal of this easy layer is to make it as easy as possible
   for the end user to implement a certain use case; e.g. like a
   trancoder. These use cases are indicated by the application
   that that you use to create an easy instance. For example,
   `TRA_EASY_APPLICATION_TYPE_TRANSCODER` will setup an
   transcoder with the best possible performance. 


  ARCHITECTURE

    This diagram shows the architecture of the easy layer. The
    easy layer is mostly implemented in `easy.c`. This easy.c
    file creates the different applications (encoder, decoder,
    transcoder). For each purpose we create an application
    context. You can find these applications inside
    `easy-{type}.c` files. These applications are implemented via
    the application API called `tra_easy_app_api`. These are the
    elements you see underneath "APPLICATION". 

    A simple implementation is the easy encoder. This will select
    an encoder based on some logic. We try to select the best
    encoder for your system. For example, this can be a NVIDIA
    based encoder, VAAPI, X264, etc. When we select an NVIDIA
    based encoder, then the encoder needs to setup some contexts
    that it uses internally. The NVIDIA encoder for example needs
    a CUDA context. It's the job of the "MODULES" to setup the
    required contexts they need to e.g. encode, decoder, etc.

    If you're curious, you'll find this boiler plate code, that
    sets up a CUDA context inside the NVIDIA module, see
    `nvidia-easy.c`. This is the work the easy layer does for you
    so you don't have to bother about this. Of course you can
    control every encoder, decoder, etc. by simply using them
    directly without the easy layer.

                                
                                                                  MODULES
                                                                 ┌──────────────────────┐
                                                                 │                      │
                                                           ┌────►│ nvidia cuda enc      │
                                                           │     │                      │
                                                           │     └──────────────────────┘
                                                           │      nvidia-enc-cuda.c
                           APPLICATIONS                    │
                          ┌─────────────────┐              │     ┌──────────────────────┐
                          │                 │              │     │                      │
                    ┌────►│ easy encoder    ├──────────────┼────►│ nvidia host enc      │
                    │     │                 │              │     │                      │
                    │     └─────────────────┘              │     └──────────────────────┘
                    │                                      │      nvidia-enc-host.c
                    │     ┌─────────────────┐              │
                    │     │                 │              │     ┌──────────────────────┐
                    ├────►│ easy decoder    ├─────► ...    │     │                      │
                    │     │                 │              └────►│ x264 enc             │
     ┌──────────┐   │     └─────────────────┘                    │                      │
     │          │   │                                            └──────────────────────┘
     │ tra_easy ├───┤     ┌─────────────────┐
     │          │   │     │                 │
     └──────────┘   ├───► │ easy transcoder ├────► ...
      easy.c        │     │                 │
                    │     └─────────────────┘
                    │
                    │     ┌─────────────────┐
                    │     │                 │
                    └────►│ easy ...        ├────► ...
                          │                 │
                          └─────────────────┘
                           easy.c
  
   

   CONFIGURING APPLICATIONS

     As you might have noticed we like the the API of libcurl. It
     has proven itself to be a understandible and forward
     compatible API. To configure the easy layer we take the same
     appraoch as `curl_easy_setopt()`. We define several options
     that you can set. You can set these for example an encoder,
     decoder, etc.

     When you call `tra_easy_set()` we will forward the call to
     the application instance as the application instance knows
     what should be configured and what not. This allows an
     application implementation to give correct feedback. For
     example, when you create an encoder you should set the input
     with, height, format, etc.

   CREATING APPLICATIONS

     When you want to create your own applcation type, like the
     encoder, decoder, transcoder, etc. you can have a loot at
     e.g. `easy-encoder.c` or `easy-decoder.c`. You need to setup
     a `tra_easy_app_api` in a .c file where you define the
     functions that implement the API. It's then up to you to
     implement the application however you see fit.

     The `create()`, `init()` and `destroy()` control the
     allocation, setup and deallocation of you application. The
     purpose of the `create()` function, is that you can allocate
     the struct that you need to keep track of internal
     application state. IMPORTANT: you should not allocate the
     encoder, decoder, scaler, etc. instances in the `create()`
     function. This is postponed to the call to `init()`.

     After the user has called `tra_easy_create()` and we have
     called `tra_easy_api::create()`, we allow the user to
     configure the application using `tra_easy_set_opt()`.  Once
     the application has been configured the user calls
     `tra_easy_init()`. Because at this point, the user has set
     all their requirements, we can actually create the specific
     encoder, decoder, etc. instances. 
                           
 */

/* ------------------------------------------------------- */

#include <stdint.h>
#include <stdarg.h>
#include <tra/module.h>
#include <tra/api.h>
#include <tra/types.h>

/* ------------------------------------------------------- */

#define TRA_EASY_APPLICATION_TYPE_NONE       0
#define TRA_EASY_APPLICATION_TYPE_ENCODER    1
#define TRA_EASY_APPLICATION_TYPE_DECODER    2
#define TRA_EASY_APPLICATION_TYPE_CONVERTER  3
#define TRA_EASY_APPLICATION_TYPE_RENDERER   4
#define TRA_EASY_APPLICATION_TYPE_TRANSCODER 5

/* ------------------------------------------------------- */

#define TRA_EOPT_NONE              1
#define TRA_EOPT_INPUT_SIZE        2  /* e.g. tra_easy_set_opt(ez, TRA_EOPT_INPUT_SIZE, 1280, 720) */
#define TRA_EOPT_INPUT_FORMAT      3  /* e.g. tra_easy_set_opt(ez, TRA_EOPT_INPUT_FORMAT, TRA_IMAGE_FORMAT_NV12) */
#define TRA_EOPT_OUTPUT_TYPE       4  /* e.g. tra_easy_set_opt(ez, TRA_EOPT_OUTPUT_TYPE, TRA_MEMORY_TYPE_IMAGE) */
#define TRA_EOPT_FPS               5  /* e.g. tra_easy_set_opt(ez, TRA_EOPT_FPS, 25, 1) */
#define TRA_EOPT_ENCODED_CALLBACK  6
#define TRA_EOPT_ENCODED_USER      7
#define TRA_EOPT_FLUSHED_CALLBACK  9
#define TRA_EOPT_FLUSHED_USER      10
#define TRA_EOPT_DECODED_CALLBACK  11
#define TRA_EOPT_DECODED_USER      12

/* ------------------------------------------------------- */

typedef struct tra_easy_app_object         tra_easy_app_object;
typedef struct tra_easy_app_api            tra_easy_app_api;     /* The API that is used to create an easy application (for example to create an encoder, transcoder, etc. application). This for example might create a `tra_easy_app_encoder` instance.  */
typedef struct tra_easy_settings           tra_easy_settings;
typedef struct tra_easy                    tra_easy;
typedef struct tra_core                    tra_core;
typedef struct tra_transcode_profile       tra_transcode_profile;
typedef struct tra_transcode_list_settings tra_transcode_list_settings;
typedef struct tra_transcode_list          tra_transcode_list;

/* ------------------------------------------------------- */

extern tra_easy_app_api g_easy_encoder;
extern tra_easy_app_api g_easy_decoder;
extern tra_easy_app_api g_easy_transcoder;

/* ------------------------------------------------------- */

struct tra_easy_settings {
  uint32_t type;                  /* The type of application that you want to create. */
};

/* ------------------------------------------------------- */

struct tra_transcode_profile {
  uint32_t width;
  uint32_t height;
  uint32_t bitrate;
  void* user;
};

/* ------------------------------------------------------- */

struct tra_transcode_list_settings {
  int dummy;
};

/* ------------------------------------------------------- */

struct tra_transcode_list {
  tra_transcode_profile* profile_array;
  uint32_t profile_count;
};

/* ------------------------------------------------------- */

struct tra_easy_app_api {
  int (*create)(tra_easy* ez, tra_easy_app_object** obj);  /* Used by the application specific type so it can allocate w/e it needs. E.g. the transcoder instance will allocate an encoder. When the application needs to keep track of its own application context it MUST set the given `tra_easy_app**` to this context.  */
  int (*init)(tra_easy* ez, tra_easy_app_object* obj);    /* This is called after creating the `tra_easy` and before encoding, decoding, etc. This will initialize all the required contexts. */
  int (*destroy)(tra_easy_app_object* obj); /* Used by the application specific type so it can deallocate any internal contexts it uses. E.g. the encoder might have allocated an CUDA based encoder. */
  int (*encode)(tra_easy_app_object* obj, tra_sample* sample, uint32_t type, void* data);  /* An application can implement an `encode()` function when it needs to allocate something. */
  int (*decode)(tra_easy_app_object* obj, uint32_t type, void* data);
  int (*flush)(tra_easy_app_object* obj); /* Should flush e.g. the encoder in case of an encoder application. */
  int (*set_opt)(tra_easy_app_object* obj, uint32_t opt, va_list args); /* Set an option; used to configure the application. */
};

/* ------------------------------------------------------- */

TRA_LIB_DLL int tra_easy_create(tra_easy_settings* cfg,  tra_easy** ez);
TRA_LIB_DLL int tra_easy_init(tra_easy* ez); /* After creating and setting the options, this will create all the required contexts. The functionality is implemented by an application (see the top of this file). . */
TRA_LIB_DLL int tra_easy_destroy(tra_easy* ez);
TRA_LIB_DLL int tra_easy_encode(tra_easy* ez, tra_sample* sample, uint32_t type, void* data);
TRA_LIB_DLL int tra_easy_decode(tra_easy* ez, uint32_t type, void* data);
TRA_LIB_DLL int tra_easy_flush(tra_easy* ez); /* Flush the easy application; e.g. to flush the encoder queue. */

/* ------------------------------------------------------- */

TRA_LIB_DLL int tra_easy_set_opt(tra_easy* ez, uint32_t opt, ...); /* Set an option for the created application. */
TRA_LIB_DLL int tra_easy_get_core_context(tra_easy* ez, tra_core** ctx);
TRA_LIB_DLL int tra_easy_select_api(tra_easy* ez, const char* names[], tra_easy_api** result); /* This function iterate over the NULL terminated array of "easy API names" and selects the first one that matches. This is used to e.g. find the easy API of a module. */

/* ------------------------------------------------------- */

TRA_LIB_DLL int tra_transcode_list_create(tra_transcode_list_settings* cfg, tra_transcode_list** list);
TRA_LIB_DLL int tra_transcode_list_destroy(tra_transcode_list* list);
TRA_LIB_DLL int tra_transcode_list_add_profile(tra_transcode_list* list, tra_transcode_profile* profile);
TRA_LIB_DLL int tra_transcode_list_print(tra_transcode_list* list);

/* ------------------------------------------------------- */

#endif
