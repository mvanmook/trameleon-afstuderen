/*
  ---------------------------------------------------------------

                       ██████╗  ██████╗ ██╗  ██╗██╗     ██╗   ██╗
                       ██╔══██╗██╔═══██╗╚██╗██╔╝██║     ██║   ██║
                       ██████╔╝██║   ██║ ╚███╔╝ ██║     ██║   ██║
                       ██╔══██╗██║   ██║ ██╔██╗ ██║     ██║   ██║
                       ██║  ██║╚██████╔╝██╔╝ ██╗███████╗╚██████╔╝
                       ╚═╝  ╚═╝ ╚═════╝ ╚═╝  ╚═╝╚══════╝ ╚═════╝ 

                                                    www.roxlu.com
                                            www.twitter.com/roxlu
  
  ---------------------------------------------------------------


  VAAPI DECODER
  =============

  GENERAL INFO:

    This file provides the API for the first draft (June 2022) of
    the VAAPI based decoder wrapper. The goal of this API is to 
    provide a simple interface for highly efficient decoding of
    video. Currently we focus solely on H264 and in the future
    we will add support for other codecs.

 */

#ifndef MO_VAAPI_DEC_H
#define MO_VAAPI_DEC_H

/* ------------------------------------------------------- */

#include <stdint.h>

/* ------------------------------------------------------- */

typedef struct va_dec_settings         va_dec_settings;
typedef struct va_dec                  va_dec;
typedef struct tra_decoder_callbacks   tra_decoder_callbacks;
  
/* ------------------------------------------------------- */

struct va_dec_settings {
  tra_decoder_callbacks* callbacks; /* The callback info. */
  uint32_t image_width;             /* Width of the picture in the video. */
  uint32_t image_height;            /* Height of the picture in the video. */
};

/* ------------------------------------------------------- */

int va_dec_create(va_dec_settings* cfg, va_dec** ctx);
int va_dec_destroy(va_dec* ctx);
int va_dec_decode(va_dec* ctx, uint32_t type, void* data); /* Decodes the given data; see `tra/types.h` for the available types. */

/* ------------------------------------------------------- */

#endif
