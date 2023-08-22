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

  VAAPI 
  =====

  GENERAL INFO:

    This header can be used to create a vaapi based h264
    encoder. Implementing a VAAPI based encoder involves -a lot-
    of work; a lot more than you normally would expect from an
    h264 encoder. I haven't encountered an encoder API which
    requires you to generate the bitstream for the SPS, PPS and
    slice header yourself, but VAAPI requires you to do this (in
    most cases). 

    We also have to manage the GOP ourself; e.g. when do we
    create an IDR, I, P and/or B frame, we need to manage our
    reference frames, etc.

    Though most of the hard work has been implemented by `va_enc`
    and you only need to feed it input buffers and we will make
    sure that it gets encoded.

  GOP SETTINGS:

    We provide a `va_gop` type which is used to generate
    GOPs. This means that it tells us what kind of frame we
    should create. Normally an encoder will do this for us but
    with VAAPI we have to define the frame type we want to
    encode. For example, we start with an IDR, then a couple of P
    frames, then a I, then again a couple of P's and then again
    an IDR, e.g.

    +---------------------------------------+--------------------------------------------+
    | idr_period | intra_period | ip_period | Sequence example                           |
    +---------------------------------------+--------------------------------------------+
    | 6          | 3            | 1         | IDR, P, P, I, P, P, IDR                    |
    +---------------------------------------+--------------------------------------------+
    | 9          | 3            | 1         | IDR, P, P, I, P, P, I, P, P, IDR           |
    +---------------------------------------+--------------------------------------------+
    | 12         | 3            | 1         | IDR, P, P, I, P, P, I, P, P, I, P, P, IDR  |
    +---------------------------------------+--------------------------------------------+
    | 12         | 4            | 1         | IDR, P, P, P, I, P, P, P, I, P, P, P, IDR  |
    +---------------------------------------+--------------------------------------------+

  IMPORTANT: 

    Currently the `ip_period` of the `va_gop_settings` MUST be 1 as we don't support B-frames yet.
  
 */
#ifndef MO_VAAPI_ENC_H
#define MO_VAAPI_ENC_H

/* ------------------------------------------------------- */

#include <stdint.h>

/* ------------------------------------------------------- */

#define VA_GOP_FRAME_TYPE_IDR 1
#define VA_GOP_FRAME_TYPE_I   2
#define VA_GOP_FRAME_TYPE_P   3
#define VA_GOP_FRAME_TYPE_B   4

/* ------------------------------------------------------- */

typedef struct va_gop_picture          va_gop_picture;
typedef struct va_gop_settings         va_gop_settings;
typedef struct va_gop                  va_gop;
typedef struct va_enc                  va_enc;
typedef struct va_enc_settings         va_enc_settings;
typedef struct tra_sample              tra_sample;
typedef struct tra_encoder_callbacks   tra_encoder_callbacks;

/* ------------------------------------------------------- */

struct va_gop_settings {
  uint32_t idr_period;                 /* The period of IDR frames. */
  uint32_t intra_period;               /* The period of I frames. */
  uint32_t ip_period;                  /* The period between I and P frames: filled by B frames. */
  uint32_t log2_max_frame_num;         /* The log2 value for the maximum frame num; i.e.. (2 << [value]). This value must be between 4-16 inclusive. This limits the maximum value for `frame_num`. */
  uint32_t log2_max_pic_order_cnt_lsb; /* The log2 value for the `pic_order_cnt_lsb` field of a slice. Must be in the range between 4-16. */
};

struct va_gop_picture {
  uint8_t frame_type;                  /* IDR, I, P, or B frame type that we should create */
  uint64_t frame_num;                  /* The current encoding/decoding order. */
  uint64_t display_order;              /* The display order of the picture relative to the main timeline. E.g. this value is not the same as `order_cnt` as `order_cnt` is the display order relative to the last IDR; so in short it uses a different timeline. */
  uint64_t poc_lsb;                    /* The display order relative to the last IDR frame and wrapped (modulo) around `log2_max_pic_order_cnt_lsb`. This value is used to determine the `TopFieldOrderCnt` and `BottomFieldOrderCnt`. */
  uint64_t poc_top_field;              /* The picture order count for the top field. */
  uint8_t is_reference;                /* Is set to 1 when the current picture should be used as a reference. */
  uint16_t idr_id;                     /* Only valid when `frame_type == VA_GOP_FRAME_TYPE_IDR`. This holds the value of `idr_pic_id` that should be used in the slice header. We use a uint16_t here as we must wrap around uint16_max according to the spec.*/
};

struct va_enc_settings {
  va_gop_settings gop;
  tra_encoder_callbacks* callbacks;    /* [YOURS] The callbacks that will e.g. receive the encoded data. */
  uint32_t image_width;                /* Width of the video. */
  uint32_t image_height;               /* Height of the video. */
  uint32_t image_format;               /* The format of the video frames; e.g. YUV420, NV12, etc. */
  uint32_t num_ref_frames;             /* Maximum number of reference frames to use. Used when we create the SPS. */
};

/* ------------------------------------------------------- */

int va_gop_create(va_gop_settings* cfg, va_gop** ctx);
int va_gop_destroy(va_gop* ctx);
int va_gop_get_next_picture(va_gop* ctx, va_gop_picture* pic); /* This will increment the frame counter and set all the members of the given `va_gop_picture`. You own the `va_gop_picture`. */
int va_gop_print(va_gop* ctx);
int va_gop_picture_print(va_gop_picture* pic);
int va_gop_picture_print_oneliner(va_gop_picture* pic);

/* ------------------------------------------------------- */

int va_enc_create(va_enc_settings* cfg, va_enc** ctx);
int va_enc_destroy(va_enc* ctx);
int va_enc_encode(va_enc* ctx, tra_sample* sample, uint32_t type, uint8_t* data); /* Encode the given `data`. The `type` indicates what kind of `data` we receive. */
int va_enc_flush(va_enc* ctx);
int va_enc_print(va_enc* ctx);

/* ------------------------------------------------------- */

#endif
