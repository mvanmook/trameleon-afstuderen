#ifndef TRA_MODULE_H
#define TRA_MODULE_H

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

    This file contains several types that are commonly used by a
    client of the Trameleon library. These include encoder
    settings, decoder settings, etc.
    

  FRAMES PER SECOND:

    In "encoder land" there has been confusion about what value
    to use for the framerate numerator and denominator. FPS
    defines the number of frames per seconds; not the time one
    frame takes up. 

    When we have 25 frames per seconds use this:

       fps_num       25
       -------  =>  --- 
       fps_den       1

       
  ENCODER API:

    The `tra_encoder_api` should be implemented by encoders. This
    API has an `encode()` function. This function will receive a
    `type` and `void* data`. The `type` describes what kind of
    `data` it receives. This can be e.g. a
    `TRA_MEMORY_TYPE_HOST_IMAGE` which is represented by a
    `tra_image` struct. The `tra_image` represents host
    memory. An encoder might also be able to handle device
    memory. In the case that an encoder supports device memory,
    the type can be `TRA_MEMORY_TYPE_DEVICE_IMAGE`. The actual
    `data` will be whatever the encoder accepts. Decoders will
    produce data in their supported memory types. E.g. for the
    NVIDIA decoder, this can be a pointer to device
    memory. Encoders can have support for device memory and/or
    host memory.

    
  DECODER API:

    The decoder implements the `decode()` function. Similar to
    the encoder API, this function receive a type and `void*`
    which describes the kind of data it receives. For example,
    the type will be `TRA_MEMORY_TYPE_HOST_H264` which described
    annex-b h264 host memory. 
   

  GRAPHICS API:

    The graphics API is used to render video. The graphics API is
    tightly coupled with a graphics engine context like
    OpenGL. In most cases this requires some sort of windowing
    system. This API is currently experimental. We have
    implemented a graphics API that can render NV12 buffers. You
    can use an interop API to transfer decoded buffers from
    e.g. CUDA into OpenGL textures.

    
  INTEROP API:

    The interop API is used to connect the decoder API with a
    graphics API. For example, the interop can be used to
    transfer device memory from the NVIDIA decoder into
    OpenGL. Or it could be used to transfer decoded host memory
    into OpenGL.


  CONVERTER API:

    The converter API is used in a transcoding pipeline. It takes
    decoded buffers and converts them into another resolution and
    optionally other pixel formats. For example the NVIDIA based
    converter receives decoded device pointers which are then
    used to scale into a different resolution.

  REFERENCES:

    [0]: https://stackoverflow.com/questions/37113736/is-this-the-correct-way-to-use-va-arg-with-pointer-to-function "StackOverflow info on retrieving function pointers with `va_arg()`.

*/

/* ------------------------------------------------------- */

#include "capabilityHandler.h"
#include <stdint.h>
#include <tra/api.h>

/* ------------------------------------------------------- */

typedef struct tra_encoder               tra_encoder;            /* Public opaque type that you use to interact with the public API functions that start with `tra_encoder_`. */
typedef struct tra_encoder_api           tra_encoder_api;        /* Defines the interface for an encoder. */
typedef struct tra_encoder_object        tra_encoder_object;     /* Opaque: set to the encoder specific context. This is used when we're using the registry API that loads shared libraries. */
typedef struct tra_encoder_callbacks     tra_encoder_callbacks;
typedef struct tra_encoder_settings      tra_encoder_settings;
typedef struct tra_encoded_data          tra_encoded_data;
                                         
typedef struct tra_decoder               tra_decoder;            /* Public opaque type that you use to interact with the public API functions that start with `tra_decoder_`. */
typedef struct tra_decoder_api           tra_decoder_api;        /* Defines the interface for a decoder. */
typedef struct tra_decoder_object        tra_decoder_object;     /* Opaque: set to the decoder specific context. This is used when we're using the registry API that loads shared libraries. */
typedef struct tra_decoder_callbacks     tra_decoder_callbacks;
typedef struct tra_decoder_settings      tra_decoder_settings;
                                         
typedef struct tra_graphics              tra_graphics;           /* Public opaque type that you use to interact with the public API functions that start with `tra_graphics_`. */ 
typedef struct tra_graphics_api          tra_graphics_api;       /* The interface for graphics interop. */
typedef struct tra_graphics_settings     tra_graphics_settings;  /* Settings which are used when we create an graphics interop instance. */
typedef struct tra_graphics_object       tra_graphics_object;    /* Opaque type which represents a graphics instance. This is used when we're using the registry API that loads shared libraries. */

typedef struct tra_interop               tra_interop;            /* Public opaque type that you use to interact with the public API functions that start with `tra_interop`. */
typedef struct tra_interop_api           tra_interop_api;
typedef struct tra_interop_callbacks     tra_interop_callbacks;
typedef struct tra_interop_settings      tra_interop_settings;
typedef struct tra_interop_object        tra_interop_object;

typedef struct tra_converter             tra_converter;              /* Public opaque type that you use to interact with the public API functions that start with `tra_converter`. */
typedef struct tra_converter_api         tra_converter_api;
typedef struct tra_converter_callbacks   tra_converter_callbacks;
typedef struct tra_converter_settings    tra_converter_settings;
typedef struct tra_converter_object      tra_converter_object;

typedef struct tra_easy_api              tra_easy_api;           /* The API an easy implementation must provide. */
typedef struct tra_easy                  tra_easy;               /* The easy instance context; handles state for an application that uses the easy layer. */

typedef struct tra_registry              tra_registry;
typedef struct tra_sample                tra_sample;

/* ------------------------------------------------------- */

/* Callback functions */
typedef int(*tra_encoded_callback)(uint32_t type, void* data, void* user);   /* Typedef of the function that received encoded data. We need a `typedef` because otherwise we can't use `va_arg()` to set the callback in `easy.c`, see [this link][0]. */
typedef int(*tra_flushed_callback)(void* user);                              /* Typedef of the function that gets called when the encoder has flushed it's data. */
typedef int(*tra_decoded_callback)(uint32_t type, void* data,  void* user);  /* Used by decoders to notify the user about decoded data. */
typedef int(*tra_uploaded_callback)(uint32_t type, void* data, void* user);  /* Used by the interop. */
typedef int(*tra_converted_callback)(uint32_t type, void* data, void* user); /* Used by the converters. */

/* ------------------------------------------------------- */

typedef int tra_load_func(tra_registry* reg); 

/* ------------------------------------------------------- */

struct tra_decoder_callbacks {
  tra_decoded_callback on_decoded_data;            /* Gets called by the decoder. `type` is set to any of the `TRA_MEMORY_TYPE_*` type that you can find in `types.h`. */
  void* user;                                      /* Private data that is passed into the callbacks. */
};

/* ------------------------------------------------------- */

struct tra_decoder_settings {
  tra_decoder_callbacks callbacks; /* The callbacks that get called by the decoder. */
  uint32_t image_width;            /* The width of the video frames. */
  uint32_t image_height;           /* The height of the video frames. */
  uint32_t output_format;            /* The preferred output type which can be any of the `TRA_OUTPUT_TYPE_*` types. This can be used to implement decode on device and then use the decoded on-device memory to perform a (different) encode using the on-device memory to skip a memory copy. */
  uint32_t input_format;
  uint32_t fps_num;
  uint32_t fps_den;
  uint32_t bitrate;
};

/* ------------------------------------------------------- */


typedef struct capability capability;

struct tra_decoder_api {
  
  /* Plugin meta data */
  const char* (*get_name)();
  const char* (*get_author)();

  /* Plugin API */
  int (*create)(tra_decoder_settings* cfg, void* settings, tra_decoder_object** obj);
  int (*destroy)(tra_decoder_object* obj);
  int (*decode)(tra_decoder_object* obj, uint32_t type, void* data);
  int (*isCapable)(tra_decoder_settings* cfg);
  int (*getCapability)(capability* (*output)[], uint32_t size);
};

/* ------------------------------------------------------- */

struct tra_encoder_callbacks {
  tra_encoded_callback on_encoded_data;              /* Gets called by the encoder. `type` is set to one of the `TRA_MEMORY_TYPE_*` types that you can find in `types.h`. */
  tra_flushed_callback on_flushed;                   /* When the user calls `tra_encoder_flush()` we will flush all queued frames and call this function when the encoder has flushed all frames. */
  void* user;                                        /* Private data that is passed into the callbacks. */
};

/* ------------------------------------------------------- */

struct tra_encoder_settings {
  tra_encoder_callbacks callbacks; /* The callbacks that are set by the user and called by the encoder. */
  uint32_t image_width;            /* Width of the video frames. */
  uint32_t image_height;           /* Height of the video frames. */
  uint32_t input_format;           /* Pixel format of the video frames. */
  uint32_t output_format;          /* The preferred output type which can be any of the `TRA_OUTPUT_TYPE_*` types. This can be used to implement decode on device and then use the decoded on-device memory to perform a (different) encode using the on-device memory to skip a memory copy. */
  uint32_t fps_num;                /* Framerate numerator. For 25 frames per seconds, `fps_num` and `fps_den` will be `fps_num = 25`, `fps_den = 1`.  */
  uint32_t fps_den;                /* Framerate denominator. For 25 frames per seconds, `fps_num` and `fps_den` will be `fps_num = 25`, `fps_den = 1`.  */
  uint32_t bitrate;
  uint32_t profile;
};

/* ------------------------------------------------------- */

struct tra_encoder_api {

  /* Plugin meta data */
  const char* (*get_name)();
  const char* (*get_author)();

  /* Plugin API */
  int (*create)(tra_encoder_settings* cfg, void* settings, tra_encoder_object** obj); 
  int (*destroy)(tra_encoder_object* obj);
  int (*encode)(tra_encoder_object* obj, uint32_t type, void* data);
  int (*flush)(tra_encoder_object* obj);
  int (*isCapable)(tra_encoder_settings* cfg);
  int (*getCapability)(capability* (*output)[], uint32_t size);
};

/* ------------------------------------------------------- */

struct tra_graphics_settings {
  uint32_t image_width;  /* When uploading decoded buffers a graphics module need to know the image width to allocate a texture. */
  uint32_t image_height; /* When uploading decoded buffers a graphics module need to know the image height to allocate a texture. */
  uint32_t image_format; /* The pixel format that the graphics module receives from a decoder or what it should generate when the user wants to download data to will be fed into an encoder. */
};

/* ------------------------------------------------------- */

struct tra_graphics_api {

  /* Plugin meta data */
  const char* (*get_name)();
  const char* (*get_author)();

  /* Plugin API */
  int (*create)(tra_graphics_settings* cfg, void* settings, tra_graphics_object** obj);
  int (*destroy)(tra_graphics_object* obj);
  int (*draw)(tra_graphics_object* obj, uint32_t type, void* data); 
};

/* ------------------------------------------------------- */

struct tra_interop_callbacks {
  tra_uploaded_callback on_uploaded; /* Gets called when an interop has uploaded data. */
  void* user;
};

/* ------------------------------------------------------- */

struct tra_interop_settings {
  tra_interop_callbacks callbacks; /* The interop callbacks; e.g. the interop will call `on_uploaded()` when it has uploaded decoded data for example. */
  uint32_t image_width;            /* When uploading decoded buffers a interop module need to know the image width to allocate a texture. */
  uint32_t image_height;           /* When uploading decoded buffers a interop module need to know the image height to allocate a texture. */
  uint32_t image_format;           /* The pixel format that the interop module receives from a decoder or what it should generate when the user wants to download data to will be fed into an encoder. */
};

/* ------------------------------------------------------- */

struct tra_interop_api {

  /* Plugin meta data */
  const char* (*get_name)();
  const char* (*get_author)();

  /* Plugin API */
  int (*create)(tra_interop_settings* cfg, void* settings, tra_interop_object** obj);
  int (*destroy)(tra_interop_object* obj);
  int (*upload)(tra_interop_object* obj, uint32_t type, void* data);
};

/* ------------------------------------------------------- */

struct tra_converter_callbacks {
  tra_converted_callback on_converted;       /* Gets called when a scaler has converted the input data. The `type` is one of the `TRA_MEMORY_TYPE_*` and `data` is represented by a struct based on the `type`. */
  void* user;                                /* The user pointer that will be passed as `user` into the callback. */
};

struct tra_converter_settings {
  tra_converter_callbacks callbacks;         /* The callback(s) that will be called by the scaler. */
  uint32_t input_format;                     /* `TRA_IMAGE_FORMAT_*`. The pixel format that will be passed into `convert()`. This can be used by a scaler to preallocate output buffers. */
  uint32_t input_type;                       /* `TRA_MEMORY_TYPE_*`, What kind of data are you going to pass into `convert()`. This is used to e.g. pre-allocate buffers that can be used to store the scaled data. */
  uint32_t input_width;                      /* The width of the images that we're going to convert into `output_width`.  */
  uint32_t input_height;                     /* The height of the image that we're going to convert into `output_height`. */
  uint32_t output_format;                    /* The `TRA_IMAGE_FORMAT_*` of the output format that you want to receive. */
  uint32_t output_type;                      /* `TRA_MEMORY_TYPE_*`, How you would like to receive the convertd data; e.g. on device or cpu data. */
  uint32_t output_width;                     /* The width of the output image that you want to create. */
  uint32_t output_height;                    /* The height of the output image that you want to create. */
};

/* ------------------------------------------------------- */

struct tra_converter_api {

  /* Plugin meta data */
  const char* (*get_name)();
  const char* (*get_author)();

  /* Plugin API */
  int (*create)(tra_converter_settings* cfg, void* settings, tra_converter_object** obj);
  int (*destroy)(tra_converter_object* obj);
  int (*convert)(tra_converter_object* obj, uint32_t type, void* data);
};

/* ------------------------------------------------------- */

struct tra_easy_api {

  /* Plugin meta data */
  const char* (*get_name)();
  const char* (*get_author)();

  /* Easy Encoder API */
  int (*encoder_create)(tra_easy* ez, tra_encoder_settings* cfg, void** enc);
  int (*encoder_encode)(void* enc, tra_sample* sample, uint32_t type, void* data);
  int (*encoder_flush)(void* enc); /* Should flush e.g. an encoder or decoder. */
  int (*encoder_destroy)(void* enc);

  /* Easy Decoder API */
  int (*decoder_create)(tra_easy* ez, tra_decoder_settings* cfg, void** dec);
  int (*decoder_decode)(void* dec, uint32_t type, void* data);
  int (*decoder_destroy)(void* dec);
};

/* ------------------------------------------------------- */

/* Encode  */
TRA_LIB_DLL int tra_encoder_create(tra_encoder_api* api, tra_encoder_settings* cfg, void* settings, tra_encoder** enc);
TRA_LIB_DLL int tra_encoder_destroy(tra_encoder* enc);
TRA_LIB_DLL int tra_encoder_encode(tra_encoder* enc, uint32_t type, void* data);
TRA_LIB_DLL int tra_encoder_flush(tra_encoder* enc); /* Flush all currently queued frames. */

/* Decode */
TRA_LIB_DLL int tra_decoder_create(tra_decoder_api* api, tra_decoder_settings* cfg, void* settings, tra_decoder** dec);
TRA_LIB_DLL int tra_decoder_destroy(tra_decoder* dec);
TRA_LIB_DLL int tra_decoder_decode(tra_decoder* dec, uint32_t type, void* data);

/* Render decoded buffers (e.g. via OpenGL, D3D9, D3D11, etc. */
TRA_LIB_DLL int tra_graphics_create(tra_graphics_api* api, tra_graphics_settings* cfg, void* settings, tra_graphics** gfx);
TRA_LIB_DLL int tra_graphics_destroy(tra_graphics* api);
TRA_LIB_DLL int tra_graphics_draw(tra_graphics* api, uint32_t type, void* data); /* Generic call to draw something. This can be used to draw a NV12 buffer for example. */

/* Transfer data from decoder to graphics */
TRA_LIB_DLL int tra_interop_create(tra_interop_api* api, tra_interop_settings* cfg, void* settings, tra_interop** ctx);
TRA_LIB_DLL int tra_interop_destroy(tra_interop* api);
TRA_LIB_DLL int tra_interop_upload(tra_interop* api, uint32_t type, void* data); /* Upload the data you received from e.g. a decoder using the given interop api. */

/* Convert (scale and pixel format) */
TRA_LIB_DLL int tra_converter_create(tra_converter_api* api, tra_converter_settings* cfg, void* settings, tra_converter** ctx);
TRA_LIB_DLL int tra_converter_destroy(tra_converter* api);
TRA_LIB_DLL int tra_converter_convert(tra_converter* api, uint32_t type, void* data);

/* ------------------------------------------------------- */

#endif
