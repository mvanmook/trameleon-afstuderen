#ifndef TRA_TYPES_H
#define TRA_TYPES_H

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

  TYPES
  =====

  GENERAL INFO:

    The `types.h` contains several types that are used by the
    different parts that make up Trameleon. This file will only
    contains library wide types. Whenever an encoder, decoder,
    converter, etc. needs a special type, it will be added to one
    of the headers of that specific implementation.

  DEVELOPMENT INFO:

    TRA_IMAGE_FORMAT_{*}: 

      The `TRA_IMAGE_FORMAT_*` are used throughout the Trameleon
      library. They are most often used by encoders, decoders and
      converters. The naming is based on the color space types
      `X264_CSP_*` as found in [the libx264 header][0]. The YUV
      image format does not only define how the luma and chroma
      planes are sampled, but also how the data is stored in
      memory.

      Video encoders and decoders will most often use some sort
      of YUV image format. YUV image formats are indicated by an
      YUV sampling notation like: Y:U:V or sometimes described as
      J:a:b and indicate how often the Y, U and V are
      sampled. The Y:U:V define an rectangle of 2 rows where each
      row contains 4 pixels. The Y defines how many luma samples
      will be taken; this is a given and will be 4. The U indices
      how many chroma samples are taking for the first row and V
      for the second row. For example, the 4:2:0 notation,
      indicates that for the first row, 2 chroma samples are
      taken and 0 for the second row. This means that every
      second row, is ignored and therefore resulting in an image
      resolution with a chroma-plane height that is half the size
      of the luma-plane height. The first row samples 2 chroma
      values for each 4 pixels; which means the width is also
      half of the luma-plane width.
 
      One of the formats which is popular by encoders is NV12 and
      deserves a little bit more explanation. NV12 is also called
      a semi-planar format as it contains a luma plane and a
      interleaved chroma plane. The bytes in the interleaved
      chroma plane are stored as Cr, Cb, Cr, Cb, etc. When you
      encounter a NV12 image format you know that it will have
      `plane_count` of 2. The image resolution of the luma plane
      is `width x height` and the resolution of the chroma plane
      is (half_width x half_height). Don't be confused that the
      number of bytes per row of the chroma plane is the same as
      the number of bytes per row of the luma plane. The reason
      for this, is that although the image resolution is half the
      size of the luma plane, we are actually storing two bytes
      per chroma value; one for Cb and one for Cr.

    TRA_UPLOAD_TYPE_{*}:

      Currently the TRA_UPLOAD_TYPE_{*} is used by the interop
      implementation. An interop implementation "glues" together
      a decoder and a graphics API. See `module.h` for more
      information.

    TRA_DRAW_TYPE_{*}:

      One of the original development goals of Trameleon was to
      provide a library which would speed up development, but
      still giving users the ability to be as close to the
      hardware as possible. To speed up development for video
      players we have defined a graphics layer which can handle
      decoded buffers and render them using a graphics API. This
      `TRA_DRAW_TYPE_{*}` is used by these graphics modules.


    TRA_MEMORY_TYPE_{*}:

      The `TRA_MEMORY_TYPE_{*}` are especially important for
      encoders, decoders and converters. As Trameleon is partly
      an abstraction API, we have to be flexible regarding the
      data that is passed into encoders and decoders. For
      example, one implementation might want to use OpenGL
      textures, another one CUDA buffers or just CPU
      memory. We're using these types, as we don't know what
      other types we might want to handle in the future.

      Currently we have just a couple of memory types. When a new
      encoder or decodes comes along with a different kind of
      memory type, it can use any "free" value. You might wonder
      what the free values are. As we do need to register them,
      you'll be able to see the list of available memory types
      on the [Trameleon][1] website.

      @todo(roxlu): create/add a list with memory types that can
      be used for new implementations.

  REFERENCES:

    [0]: https://github.com/mirror/x264/blob/master/x264.h#L242 "libx264.h color space types."
    [1]: https://www.trameleon.org "Trameleon website."

 */

/* ------------------------------------------------------- */

#include <stdint.h>
#include <tra/api.h>

/* ------------------------------------------------------- */

#define TRA_MAX_IMAGE_PLANES 3

/* ------------------------------------------------------- */

/* Image Formats based on X264 (image_format, u32) . */

#define TRA_IMAGE_FORMAT_NONE 0x0000                                 /* Invalid mode     */
#define TRA_IMAGE_FORMAT_I400 0x0001                                 /* monochrome 4:0:0 */
#define TRA_IMAGE_FORMAT_I420 0x0002                                 /* yuv 4:2:0 planar (y plane, u plane, v plane) */
#define TRA_IMAGE_FORMAT_YV12 0x0003                                 /* yvu 4:2:0 planar */
#define TRA_IMAGE_FORMAT_NV12 0x0004                                 /* yuv 4:2:0, with one y plane and one packed u+v */
#define TRA_IMAGE_FORMAT_NV21 0x0005                                 /* yuv 4:2:0, with one y plane and one packed v+u */
#define TRA_IMAGE_FORMAT_I422 0x0006                                 /* yuv 4:2:2 planar */
#define TRA_IMAGE_FORMAT_YV16 0x0007                                 /* yvu 4:2:2 planar */
#define TRA_IMAGE_FORMAT_NV16 0x0008                                 /* yuv 4:2:2, with one y plane and one packed u+v */
#define TRA_IMAGE_FORMAT_YUYV 0x0009                                 /* yuyv 4:2:2 packed */
#define TRA_IMAGE_FORMAT_UYVY 0x000a                                 /* uyvy 4:2:2 packed */
#define TRA_IMAGE_FORMAT_V210 0x000b                                 /* 10-bit yuv 4:2:2 packed in 32 */
#define TRA_IMAGE_FORMAT_I444 0x000c                                 /* yuv 4:4:4 planar */
#define TRA_IMAGE_FORMAT_YV24 0x000d                                 /* yvu 4:4:4 planar */
#define TRA_IMAGE_FORMAT_BGR  0x000e                                 /* packed bgr 24bits */
#define TRA_IMAGE_FORMAT_BGRA 0x000f                                 /* packed bgr 32bits */
#define TRA_IMAGE_FORMAT_RGB  0x0010                                 /* packed rgb 24bits */

/* ------------------------------------------------------- */

#define TRA_UPLOAD_TYPE_NONE          0x0000                         /* Uset upload type. */
#define TRA_UPLOAD_TYPE_OPENGL_NV12   0x0001                         /* Used with the interop to indicate that it has uploaded NV12 data into e.g. textures. The `data` member of the holds an interop specific type, e.g. `tra_upload_opengl_nv12`. */

/* ------------------------------------------------------- */

#define TRA_DRAW_TYPE_NONE            0x0000                         /* Unset draw type. */
#define TRA_DRAW_TYPE_OPENGL_NV12     0x0001                         /* Draw NV12 data using OpenGL. This means that you need to use the `tra_draw_opengl_nv12` struct. */

/* ------------------------------------------------------- */

#define TRA_MEMORY_TYPE_NONE          0X0000  
#define TRA_MEMORY_TYPE_IMAGE         0x0001                        /* Memory is represented by `tra_memory_image` */
#define TRA_MEMORY_TYPE_H264          0X0002                        /* Memory is represented by `tra_memory_h264` */
#define TRA_MEMORY_TYPE_CUDA          0X0003                        /* Memory is represented by `tra_memory_cuda`, see NVIDIA module.  */
#define TRA_MEMORY_TYPE_OPENGL_NV12   0x0004                        /* Memory is represented by `tra_memory_opengl_nv12`, see the NVIDIA module. */
                                                
/* ------------------------------------------------------- */

#define TRA_MEMORY_FLAG_NONE           (0)
#define TRA_MEMORY_FLAG_IS_KEY_FRAME   (1) 

/* ------------------------------------------------------- */

typedef struct tra_memory_image tra_memory_image;
typedef struct tra_memory_h264  tra_memory_h264;
typedef struct tra_sample       tra_sample;

/* ------------------------------------------------------- */

/*
  @todo(roxlu): currently we pass this into the encoder, and we
  most likely will integrate this more tightly when we're going
  to do our first experiments with a fully working transcode
  pipeline. Though as I don't know the details yet, this is
  mostly a placeholder.
*/
struct tra_sample {                                                 /* A pointer to a `tra_sample` is passed into the `encode()` function of an encoder. */
  int64_t pts;                                                      /* The presentation timestamp of the data that you want to encode. */
}; 

/* ------------------------------------------------------- */

struct tra_memory_image {                                           /* The `tra_memory_image` is often used as input for an encoder that encodes CPU memory. An decoder might output a `tra_memory_image` in e.g. the NV12 format. */
  uint32_t image_format;                                            /* One of the `TRA_IMAGE_FORMAT_*` as defined at the top of this file. */
  uint16_t image_width;                                             /* The width of the image. */
  uint16_t image_height;                                            /* The height of the image. */
  uint8_t* plane_data[TRA_MAX_IMAGE_PLANES];                        /* Pointers to the image data; e.g. one the image format is planar, only `plane_data[0]` will be set, otherwise every element might point to it's first byte. E.g. this is the case for NV12. */
  uint16_t plane_strides[TRA_MAX_IMAGE_PLANES];                     /* The stride in bytes of each image plane. */
  uint16_t plane_heights[TRA_MAX_IMAGE_PLANES];                     /* The heights of each plane; certain YUV sampling will result in a height which e.g. half height as the `image_image` height for certain planes. */
  uint16_t plane_count;                                             /* The number of planes for this `image_format`. For each plane, the values in `plane_data`, `plane_height` and `plane_strides` should be set. */
};

/* ------------------------------------------------------- */

struct tra_memory_h264 {                                            /* The `tra_memory_h264` is used by encoders and CPU based decoders. It should hold Annex-B H264. */
  uint8_t* data;                                                    /* Pointer to the H264.  */
  uint32_t size;                                                    /* The size of the `data` in bytes. */
  uint32_t flags;                                                   /* One of the `TRA_MEMORY_FLAG_*` values. */
};

/* ------------------------------------------------------- */

TRA_LIB_DLL int tra_memoryimage_print(tra_memory_image* img);       /* Mostly used during debugging; prints information about the given `tra_memory_image`. */
TRA_LIB_DLL const char* tra_imageformat_to_string(uint32_t fmt);    /* Converts the given `TRA_IMAGE_FOMRAT_*` into a string. Used for debugging purposes. */
TRA_LIB_DLL const char* tra_memorytype_to_string(uint32_t type);    /* Converst the given `TRA_MEMORY_TYPE*` into a string. Mostly used for debugging purposes. */

/* ------------------------------------------------------- */

#endif
