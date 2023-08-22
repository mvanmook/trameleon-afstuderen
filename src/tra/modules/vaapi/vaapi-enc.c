#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <errno.h>

#include <va/va_x11.h>
#include <va/va.h>
#include <va/va_enc_h264.h>

#include <tra/modules/vaapi/vaapi-utils.h>
#include <tra/modules/vaapi/vaapi-enc.h>
#include <tra/golomb.h>
#include <tra/module.h>
#include <tra/types.h>
#include <tra/log.h>

/* ------------------------------------------------------- */

/* Entropy modes, used in the PPS */
#define VA_ENTROPY_MODE_CAVLC 0      /* Context-adaptive variable length coding. Supported by all profiles, faster but less compression. */
#define VA_ENTROPY_MODE_CABAC 1      /* Context-based adaptive binary coding. Not supported by Baseline and Extended profiles, more processing, more compression. */

/* ------------------------------------------------------- */

struct va_gop {
  
  va_gop_settings settings;

  /* frame_num + display_order */
  uint64_t frame_counter;             /* Internally used to determine what kind of frame we need to generate. Each time you call `va_gop_get_next_picture()` we increment this. */
  uint64_t frame_num_curr;            /* The current `frame_num` value; is incremented for each reference picture; is reset to 0 when the current picture (see `va_gop_get_next_picture()` is an IDR frame). */
  uint64_t frame_num_max;             /* The maximum value for `frame_num` of a `va_gop_picture`. */
  uint64_t idr_count;                 /* The number of generated IDR frames. This is used to set `idr_id` of the `va_gop_picture` in case we're dealing with an IDR frame. This value is used in the `idr_pic_id` field of the slice header and wraps around uint16_max. */

  /* pic_order_cnt_lsb */
  uint64_t last_idr_display_order;    /* Holds the display order value of the last generated IDR. This is used to calculate `pic_order_cnt_lsb`. */
  uint64_t poc_max;                   /* The maximum value we can use for `pic_order_cnt_lsb` of the slice header. Based on the `log2_max_pic_order_cnt_lsb` you set in the `va_gop_settings`. */

  /* TopFieldOrderCnt + BottomFieldOrderCnt */
  uint64_t prev_poc_lsb;
  uint64_t prev_poc_msb;
};

/* ------------------------------------------------------- */

struct va_enc {

  /* General */
  va_enc_settings settings;                   /* We keep a copy of the given settings. */
  va_gop* gop;                                /* The context that we use to generate the GOPs e..g [IDR, P, B, B, I, ...]. The `va_gop` also manages all the encode order, display order, etc. related values. */
  va_gop_picture gop_pic;                     /* We use this picture with `va_gop` to determine what kind of slice/frame we should generate; what frame number to use, what pic_order_cnt_lsb value etc. */
  tra_golomb_writer* bitstream;               /* The bitstream writer that we use to generate the packed bitstream. See the `enc_render_packed_{sequence, picture, slice}()` functions. */

  /* Context management */
  Display* xorg_display;                      /* Represents our connection with X11 */
  VADisplay va_display;             
  VAConfigID config_id;                       /* We create a config instance that represents our "settings", see `va_enc_create()`. */
  VAContextID context_id;                     /* The encoder context. */
  VAProfile profile;                          /* The H264 profile to use. */
  int ver_minor;                              /* VAAPI minor version. Set when we initialize VA. */
  int ver_major;                              /* VAAPI major version. Set when we initialize VA. */

  uint8_t level_idc;                          /* The `level_idc` value that is used in the SPS; defines constraints on the data processing. */
  uint32_t sampling_format;                   /* The pixel format that we require for the input YUV images. VAAPI makes a distincting between the used sampling format and it's storage. This represents the sampling format: YUV420, YUV422, YUV44. */
  uint32_t storage_format;                    /* VAAPI makes a distinction between storage and sampling format. The storage format describes how the yuv data is layout out in memory. */
  uint32_t width_mb_aligned;                  /* The VA API needs to use values that are aligned with macroblocks. This value holds the width that is aligned to the closes macroblock where a macroblock is 16 pixels. */
  uint32_t height_mb_aligned;                 /* Same as `width_mb_aligned` but for the height. */
  uint32_t width_in_mbs;                      /* The width in macroblocks; 16 pix. */
  uint32_t height_in_mbs;                     /* The height ihn macroblocks; 16 pix. */
  uint8_t entropy_mode;                       /* 0 = CAVLC (support in all profiles, less processing, not so good), 1 = CABAC (not supported by Baseline, Extended profiles). */
  uint8_t is_using_packed_headers;            /* 1 = yes, 0 = no. When set to 1 we assume that we must generate the h264 bitstream for certain elements (pps, sps, slice header). */
  uint8_t constraint_set_flag;                /* Based on the chosen profile we set the constraint flags which are used in the SPS. See `h264encode.c` from the va-utils repository. */

  /* Parameter Buffers */
  VAEncSequenceParameterBufferH264 seq_param; /* VA's representation of the SPS, though be aware that this is not 100% the same as the SPS. */
  VAEncPictureParameterBufferH264 pic_param;  /* VA's representation of the PPS, though be aware that this is not 100% the same as the PPS. */
  VAEncSliceParameterBufferH264 slice_param;  

  /* Surfaces and buffers */
  VABufferID* coded_buffers;                  /* The buffers into which the H264 data is stored. */
  VASurfaceID* src_surfaces;                  /* This get's filled with input data (YUV NV12)  */
  VASurfaceID* ref_surfaces;                  /* The buffers that hold the reference surfaces (internal representation that VA uses). */
  uint32_t num_coded_buffers;                 /* The maximum number of coded buffers. */
  uint32_t num_src_surfaces;                  /* The maximum number of src surfaces. */
  uint32_t num_ref_surfaces;                  /* The maximum number of reference surfaces. */
  uint32_t curr_coded_buffer_index;           /* @todo: see `enc_render_picture()` where we provide the destination that will hold the coded buffers. */
  uint32_t curr_src_surface_index;            /* Current index into the source surfaces array */
  uint32_t curr_ref_surface_index;            /* @todo: see `enc_render_picture()` where we provide a surface using this index. */

  /* Reference frames */
  uint32_t short_ref_count;                   /* The number of short term refernce frames; see `numShortTerm` in the spec. This is used to store reference frames in `ref_frames`. See `enc_update_reference_frames()`. */
  uint32_t ref_list0_max;                     /* The maximum number of reference pictures the encoder can store in list0. Retrieved from the encoder capabilities when available. */
  uint32_t ref_list1_max;                     /* The maximum number of reference pictures the encoder can store in list1. Retrieved from the encoder capabilities when available. */ 
  VAPictureH264 ref_frames[16];               /* Whenever a `va_gop_picture` is marked as a reference it's added to this list in a first-in, first-out order. */
  VAPictureH264 ref_list0[32];                /* Depending on the current frame type (P or B) this list holds the reference frames sorted by either the decode order (frame_num for P-slices, short-term) or display order (B-slices, short-term). 32 because we can have up to 32 fields.*/
  VAPictureH264 ref_list1[32];                /* This list is use to store reference frames for B-slices. 32 because we can have up to 32 fields */
};

/* ------------------------------------------------------- */

static int gop_settings_validate(va_gop_settings* cfg); /* Returns 0 when the given settings are valid; otherwise < 0. */
static int gop_get_picture_frame_type(va_gop* ctx, uint64_t counter, uint8_t* frameType);
static int gop_get_picture_display_order(va_gop* ctx, uint64_t counter, uint8_t frameType, uint64_t* displayOrder);
static int gop_get_picture_order_count(va_gop* ctx, uint8_t frameType, uint64_t displayOrder, uint64_t* poc);
static int gop_get_picture_top_field_order_count(va_gop* ctx, uint8_t frameType, uint8_t isReference, uint64_t picOrderCntLsb, uint64_t* topFieldOrderCnt);
static int gop_get_picture_frame_num(va_gop* ctx, uint8_t frameType, uint8_t isReference, uint64_t* frameNum);
static int gop_get_picture_reference_flag(va_gop* ctx, uint8_t frameType, uint8_t* isReference);
static int gop_get_picture_idr_id(va_gop* ctx, uint8_t frameType, uint16_t* idrId); /* Sets the given `idrId` when we're dealing with an IDR frame; we also increment the counter if so. */
static const char* gop_frame_type_to_string(uint8_t type);

/* ------------------------------------------------------- */

static int enc_init_encode(va_enc* ctx);
static int enc_render_sequence(va_enc* ctx);         /* Called once per sequence (GOP). This mostly sets up the SPS and sends it to the server. This must be called once per sequence. */
static int enc_render_picture(va_enc* ctx);          /* Called once per frame. This mostly sets up the PPS and sends it to the server.  Note that this is not exactly the same as creating a PPS. */
static int enc_render_slice(va_enc* ctx);            /* Called once per frame. This will render (send) a slice parameter struct to the server. The slice parameter contains e.g. the slice type, list0, list1, etc. */
static int enc_render_packed_sequence(va_enc* ctx);  /* Generates the SPS bitstream */
static int enc_render_packed_picture(va_enc* ctx);   /* This does NOT (only) generate the PPS; is called for each picture. */
static int enc_render_packed_slice(va_enc* ctx);     /* Generates the bitstream for a slice. */
static int enc_update_reference_frames(va_enc* ctx); /* Adds the given `VAPictureH264` to the `ref_frames` and applies the first-in/first-out process but only when the curernt `va_gop_picture` member (gop_pic) is marked as reference (is_reference = 1). . */
static int enc_update_reference_lists(va_enc* ctx);  /* This function uses the `ref_frames` to generate the `ref_list0` and `ref_list1`. The `ref_list0` and `ref_list1` are used as the `RefPicList{0,1}` members of the `slice_param` member of `va_enc`. */
static int enc_save_coded_data(va_enc* ctx);         /* When we've delivered a raw YUV frame to the encoder and took all the required steps like creating the sequence, picture param, bit stream, we can use this function to finally extract/receive the encoded data. */

static int enc_print_image(VAImage* img);
static int enc_pring_seq_param(VAEncSequenceParameterBufferH264* sps);
static int enc_print_pic_param(VAEncPictureParameterBufferH264* pps);

static uint8_t enc_is_slice_p(uint8_t sliceType);  /* Returns 1 if the given slice type represents a P slice, otherwise 0. */
static uint8_t enc_is_slice_b(uint8_t sliceType);  /* Returns 1 if the given slice type represents a B slice, otherwise 0. */
static uint8_t enc_is_slice_i(uint8_t sliceType);  /* Returns 1 if the given slice type represents a I slice, otherwise 0. */
  
static const char* enc_entropymode_to_string(uint8_t mode);
static const char* enc_profile_to_string(VAProfile profile);
static const char* enc_sampling_format_to_string(uint32_t fmt);

/* ------------------------------------------------------- */

int va_gop_create(va_gop_settings* cfg, va_gop** ctx) {
  
  int r = 0;
  va_gop* inst = NULL;

  if (NULL == cfg) {
    TRAE("Cannot create the `va_gop`, the given `va_gop_settings*` is NULL.");
    return -1;
  }

  if (NULL == ctx) {
    TRAE("Cannot create the `va_gop`, the given `va_gop**` is NULL.");
    return -2;
  }

  if (NULL != (*ctx)) {
    TRAE("Cannot create the `va_gop`, the given `*va_gop**` is not NULL. Already initialized? Or variable not initialized to NULL?");
    return -3;
  }

  r = gop_settings_validate(cfg);
  if (r < 0) {
    TRAE("Cannot create the `va_gop`, the given `va_gop_settings` are invalid.");
    return -4;
  }
  
  inst = calloc(1, sizeof(va_gop));
  if (NULL == inst) {
    TRAE("Failed to allocate the `va_gop` instance.");
    return -4;
  }

  inst->settings = *cfg;
  inst->frame_counter = 0;
  inst->frame_num_curr = 0;
  inst->frame_num_max = (2 << cfg->log2_max_frame_num);
  inst->poc_max = (2 << cfg->log2_max_pic_order_cnt_lsb);
  inst->last_idr_display_order = 0;
  inst->prev_poc_lsb = 0;
  inst->prev_poc_msb = 0;

  *ctx = inst;

 error:

  /* Not used; but added if we in the future add some initialisation that can fail. */
  if (r < 0) {
    
    if (NULL != inst) {
      va_gop_destroy(inst);
      inst = NULL;
    }
    
    if (NULL != ctx) {
      *ctx = NULL;
    }
  }

  return r;
}
  
/* ------------------------------------------------------- */

int va_gop_destroy(va_gop* ctx) {

  if (NULL == ctx) {
    TRAE("Cannot destroy the given `va_gop*` as it's NULL.");
    return -1;
  }

  free(ctx);
  ctx = NULL;

  return 0;
}

/* ------------------------------------------------------- */

/*

  This function will setup all the necessary information for the
  current picture. You call this function at the start of each
  encode loop. This will set the frame type based on the GOP
  settings, the display order, frame_num, etc.

 */
int va_gop_get_next_picture(va_gop* ctx, va_gop_picture* pic) {

  int r = 0;
  
  if (NULL == ctx) {
    TRAE("Cannot get the next picture; given `va_gop*` is NULL.");
    r = -10;
    goto error;
  }

  if (NULL == pic) {
    TRAE("Cannot get the next picture; given `pic**` is NULL.");
    r = -20;
    goto error;
  }

  /* frame_type */
  r = gop_get_picture_frame_type(ctx, ctx->frame_counter, &pic->frame_type);
  if (r < 0) {
    TRAE("Cannot get the next picture; failed to get the frame type.");
    r = -30;
    goto error;
  }

  /* display_order */
  r = gop_get_picture_display_order(ctx, ctx->frame_counter, pic->frame_type, &pic->display_order);
  if (r < 0) {
    TRAE("Cannot get the next picture; failed to get the display order.");
    r = -40;
    goto error;
  }

  /* is_reference */
  r = gop_get_picture_reference_flag(ctx, pic->frame_type, &pic->is_reference);
  if (r < 0) {
    TRAE("Cannot get the next picture; failed to get the `is_reference` flag.");
    r = -50;
    goto error;
  }

  /* pic_order_cnt_lsb */
  r = gop_get_picture_order_count(ctx, pic->frame_type, pic->display_order, &pic->poc_lsb);
  if (r < 0) {
    TRAE("Cannot get the next picture; failed to get the `poc_lsb`.");
    r = -60;
    goto error;
  }

  /* TopFieldOrderCnt */
  r = gop_get_picture_top_field_order_count(ctx, pic->frame_type, pic->is_reference, pic->poc_lsb, &pic->poc_top_field);
  if (r < 0) {
    TRAE("Cannot get the next picture; failed to get the `TopFieldOrderCnt`.");
    r = -70;
    goto error;
  }

  /* frame_num */
  r = gop_get_picture_frame_num(ctx, pic->frame_type, pic->is_reference, &pic->frame_num);
  if (r < 0) {
    TRAE("Cannot get the next picture; failed to get the `frame_num`.");
    r = -80;
    goto error;
  }

  /* When the current frame is an IDR frame, we increment the counter and update the picture. */
  r = gop_get_picture_idr_id(ctx, pic->frame_type, &pic->idr_id);
  if (r < 0) {
    TRAE("Cannot get the next picture; failed to get the idr_id. ");
    r = -90;
    goto error;
  }
  
  /* ... increment the frame counter. */
  ctx->frame_counter++;

 error:

  if (r < 0) {
    TRAE("@todo reset `va_gop_picture`. ");
  }

  return r;
}

/* ------------------------------------------------------- */

/* Returns 0 when the given settings are valid; otherwise < 0. */
static int gop_settings_validate(va_gop_settings* cfg) {

  if (NULL == cfg) {
    TRAE("Cannot validate the given `va_gop_settings` as the given `cfg*` is NULL.");
    return -1;
  }

  if (cfg->log2_max_frame_num < 4
      || cfg->log2_max_frame_num > 16)
    {
      TRAE("The given `va_gop_settings` are invalid, the `log2_max_frame_num` must be a value between 4-16 inclusive. E.g. a value of 4 means (2 << 4) = 32. ");
      return -2;
    }

  if (cfg->log2_max_pic_order_cnt_lsb < 4
      || cfg->log2_max_pic_order_cnt_lsb > 16)
    {
      TRAE("The given `va_gop_settings` are invalid, the `log2_max_pic_order_cnt_lsb` must be a vale between 4-16 inclusive. This determines the maximum value we can use for the `pic_order_cnt_lsb`. ");
      return -3;
    }

  return 0;
}

/* ------------------------------------------------------- */

/*
   This function will return if the given frame type should be
   used as a reference or not. Every frame could be used as a
   reference, though we only support all frames which are not
   B-frames.
 */
static int gop_get_picture_reference_flag(va_gop* ctx, uint8_t frameType, uint8_t* isReference) {

  if (NULL == ctx) {
    TRAE("Cannot get the picture reference flag, as the given `va_gop` is NULL.");
    return -1;
  }

  if (NULL == isReference) {
    TRAE("Cannot get the picture referencde flag, the given `isReference*` is NULL.");
    return -2;
  }

  switch (frameType) {
    
    case VA_GOP_FRAME_TYPE_IDR:
    case VA_GOP_FRAME_TYPE_I:
    case VA_GOP_FRAME_TYPE_P: {
      *isReference = 1;
      return 0;
    }
    
    case VA_GOP_FRAME_TYPE_B: {
      *isReference = 0;
      return 0;
    }

    default: {
      TRAE("Unhandled frame type; cannot determine if it's a reference. ");
      return -3;
    }
  }

  return -4;
}

/* ------------------------------------------------------- */

/* 
   Sets the given `idrId` when we're dealing with an IDR frame;
   we also increment the counter if so. The `va_gop_picture` has
   a field `idr_id` which is set for each IDR frame. This is an
   ID which uniquely identifies the IDR picture and is added to
   the slice header of an IDR frame. The value that is stored in 
   the slice header wraps around uint16_t max. 
*/
static int gop_get_picture_idr_id(va_gop* ctx, uint8_t frameType, uint16_t* idrId) {

  if (NULL == ctx) {
    TRAE("Cannot set the `idr_id` as the given `va_gop*` is NULL.");
    return -1;
  }
  
  if (NULL == idrId) {
    TRAE("Cannot set the `idr_id` as the given `idrId` is NULL.");
    return -2;
  }
  
  if (VA_GOP_FRAME_TYPE_IDR != frameType) {
    *idrId = 0;
    return 0;
  }

  /* When we're dealing with an IDR frame set it and increment. */
  *idrId = ctx->idr_count;
  ctx->idr_count++;

  return 0;
}


/* ------------------------------------------------------- */

/*

  This function calculates the value for `pic_order_cnt_lsb`
  which is stored in the slice header. This value is used by the
  decoder to determine the value of `TopFieldOrderCnt` and
  `BottomFieldOrderCnt`.  These Top/Bottom order counts determine
  when the decoded frame should be displayed. The spec refers to
  this as the Picture Order Count (POC). The POC represents the
  display order value of the picture and is relative to the last
  IDR.

  The `pic_order_cnt_lsb` value, which is stored in
  `va_gop_picture::poc_lsb` uses a value that is local to the
  IDR timeline, modulo it's maximum value. The maximum value is
  set with the `log2_max_pic_order_cnt_lsb_minus4` attribute of
  the SPS. As this value is wrapped, this value loops. It can
  happen that for the same GOP the same values are used.

  There is no note about the `lsb` and `msb` meaning in the
  spec. My understanding is that these values represent the
  least-significant-bits and most-significant-bits of the _final_
  values for `TopFieldOrderCnt` and `BottomFieldCnt`. We only
  store the LSB part in the bitstream and the decoder calculates
  and keeps track of the MSB part. This is all about limiting the
  amount of bits that is required to calculate the display frame
  order. Paragraph 8.2.1 describes the algorithm how to calculate
  `TopFieldOrderCnt` and `BottomFieldOrderCnt`. It uses a
  solution that determines if the LSB value wrapped and if so it
  _remembers_ current `pic_order_cnt_lsb` value. This remembered
  `pic_order_cnt_lsb` value is then added for the following
  frames to calculate the display order relative to the last IDR.

  We need the current `frameType` because the value of
  `pic_order_cnt_lsb` is relative to the IDR timeline; e.g. when
  the current frame is an IDR frame, the `pic_order_cnt_lsb`
  value will be set to 0. 

  The `displayOrder` that we accept is the frame number that
  indicates when the current picture should be displayed. This is
  the display order that is relative to the main timeline. E.g.,
  when we have a GOP like: [IDR, P, B, B, B] then the
  `displayOrder` values will be: [0, 4, 1, 2, 3]. The P frame is
  stored earlier in the bitstream because the decoder needs
  information from the P frame when it decodes the other B
  frames. E.g. the B frames use the P frame as a reference.
  [This image][0] show this a bit more visually.

  REFERENCES:

    [0]: https://imgur.com/a/91RBGda "Image to explain display order"

 */
static int gop_get_picture_order_count(va_gop* ctx, uint8_t frameType, uint64_t displayOrder, uint64_t* poc) {

  if (NULL == ctx) {
    TRAE("Cannot get the picture order count as the given `va_gop*` is NULL.");
    return -1;
  }

  if (NULL == poc) {
    TRAE("Cannot get he picture order count as the given result `poc*` is NULL.");
    return -2;
  }

  if (VA_GOP_FRAME_TYPE_IDR == frameType) {
    ctx->last_idr_display_order = displayOrder;
  }
  
  *poc = (displayOrder - ctx->last_idr_display_order) % ctx->poc_max;

  return 0;
}

/* ------------------------------------------------------- */

/*
  **IMPORTANT**:  we implement the picture order count type 0 

  This function calculates `TopFieldOrderCnt`. This is the order
  number that should be used to determine when to display the
  current picture relative to the last IDR. The bitstream only
  stores the `least-significant-bits` of the display order in the
  field `pic_order_cnt_lsb` of the slice header.

  The terminology can be confusing; the _lsb_ part means that the
  encoder stores the `least-significant-bits` in the
  bitstream. The decoder (or encoder) keeps track of the
  most-significant-bits. 

  This boils down to the variables `PicOrderCntMsb` and
  `PicOrderCntLsb` according to the spec. We don't shift any
  least significant bits, into the bitstream or something like
  that. We store a small value in `pic_order_cnt_lsb` and keep
  track whenever this small value wraps around it's maximum. We
  do this while taking frame reordering into account. The
  algorithm for this is described in section 8.2.1.

  In order to calculate the `{Top,Bottom}FieldOrderCnt` values
  we have to keep track of the `pic_order_cnt_lsb` value of the
  previous reference picture. 
  
 */
static int gop_get_picture_top_field_order_count(
  va_gop* ctx,
  uint8_t frameType,
  uint8_t isReference,
  uint64_t picOrderCntLsb,
  uint64_t* topFieldOrderCnt
)
{
  uint64_t half_size = 0;
  uint64_t poc_msb = 0;

  if (NULL == ctx) {
    TRAE("Cannot calculate the top field order count, given `va_gop*` is NULL.");
    return -1;
  }

  if (NULL == topFieldOrderCnt) {
    TRAE("Cannot calculate the top field order count, given `topFieldOrderCnt*` is NULL.");
    return -2;
  }

  if (ctx->settings.log2_max_pic_order_cnt_lsb < 4) {
    TRAE("Canot calculate the top field order count as the given `va_gop` has an invalid value for `log2_max_pic_order_cnt`. Must be >= 4.");
    return -3;
  }

  /* On IDR we reset the counters. */
  if (VA_GOP_FRAME_TYPE_I == frameType) {
    ctx->prev_poc_msb = 0;
    ctx->prev_poc_lsb = 0;
  }

  half_size = ctx->poc_max / 2;

  /* Calculate the MSB part */
  if (picOrderCntLsb < ctx->prev_poc_lsb
      && (ctx->prev_poc_lsb - picOrderCntLsb) >= half_size)
    {
      poc_msb = ctx->prev_poc_msb + ctx->poc_max;
    }
  else if (picOrderCntLsb > ctx->prev_poc_lsb
           && (picOrderCntLsb - ctx->prev_poc_lsb) > half_size)
    {
      poc_msb = ctx->prev_poc_msb - ctx->poc_max;
    }
  else {
    poc_msb = ctx->prev_poc_msb;
  }

  *topFieldOrderCnt = poc_msb + picOrderCntLsb;

  /* Remember the POC values when the current frame is a reference frame. */
  if (1 == isReference) {
    ctx->prev_poc_msb = poc_msb;
    ctx->prev_poc_lsb = picOrderCntLsb;
  }
  
  return 0;
}

/* ------------------------------------------------------- */

/*

  The `frame_num` is incremented for each reference picture.
  When the current `frame_type` is a B-frame, we don't increment
  this number because we don't use B-frames as references. The
  `frame_num` determines the decoding order. We make sure that we
  wrap the `frame_num` value when it reaches the maximum value.
  Note that `frame_num` increments -after- each reference frame.

 */
static int gop_get_picture_frame_num(va_gop* ctx, uint8_t frameType, uint8_t isReference, uint64_t* frameNum) {

  if (NULL == ctx) {
    TRAE("Cannot get the `frame_num` as the given `va_gop*` is NULL.");
    return -1;
  }

  if (NULL == frameNum) {
    TRAE("Cannot get the `frame_num` as the given result `frameNum*` is NULL.");
    return -2;
  }
  
  if (VA_GOP_FRAME_TYPE_IDR == frameType) {
    ctx->frame_num_curr = 0;
  }
  
  *frameNum = ctx->frame_num_curr;
  
  if (1 == isReference) {

    ctx->frame_num_curr++;

    /* Make sure we wrap around the maximum value for frame_num; see the spec from 2003, page 16. */
    if (ctx->frame_num_curr > ctx->frame_num_max) {
      ctx->frame_num_curr = 0;
    }
  }

  return 0;
}


/* ------------------------------------------------------- */

/*

  GENERAL INFO:

  This implementation generates a sequence of frame types: IDR,
  I, P and B, known as the GOP. The GOP is a repeating sequence
  of these frame types. For example IDR, P, B, B, P, B, B, etc.

  Each type has it's own period and timeline. For example we
  might want to generate an IDR every 12 frames. This means the
  timeline for the IDR goes from 0...12 (see note below when we
  grow this to 13). When we want an I frame every 4 frames, the
  I-frame timeline goes from [0...3], etc. There is a special
  situation when B frames are used. When B frames are used we
  grow the GOP-size with one extra frame to make sure the order
  of frames is correct; this is just to make sure the math works
  out.

  The code below starts with deciding if we should generate the
  most important frame (IDR). If not, we check if we need to
  generate a less important frame (I), then again (P). When we
  have to generate a B frame it overrules the P frame.  The IDR
  is more important than an I, and an I frame is more important
  than a P. The B frames overrule the P frames. This is the same
  order as we perform the checks in the code below.

  When we have to generate an I frame, there is an edge case. The
  timelines for all frames other than the IDR start -after- the
  IDR.  Therefore the first frame after the IDR has a timeline
  value of 0 for the I, P, B timelines. Due to how the modulo
  operator works, this would mean that we would need to generate
  an I frame directly after the IDR frame. The reason for this,
  is that we use 0 as the index/value for modulo: 0 % <something>
  gives 0. That's why we use the (idr_mod >= 2) below. Without
  this check we would insert an I frame directly after the IDR,
  e.g. (IDR, I, B, ...). By adding this check we get: IDR, P, B,
  B, ... I ... etc. See the "edge case" in notes column of the
  table below.

  TIMELINES:

  Let's dive a bit deeper into the timelines. We receive an
  `counter` which is a monotonically increasing number
  that represents the "encode" loop index. So if you want to
  encode 1000 frames, this goes from 0...999.  

  Based on this `counter` we generate the timeline for
  the GOP, i.e. the IDR period. When our IDR period is 6 we
  generate the following sequence 0, 1, 2, 3, 4, 5 and repeat
  that over and over (idr_mod). Then we use this idr timeline
  value as input for the other timelimes: intra_mod, ip_mod. This
  value is `idr_mod = counter % idr_period`. The code
  below uses the `b_toggle` which grows the timeline when B
  frames are used.

  OPEN GOP:

  To generate a P, I or B frame we use `idr_mod` as the local
  timeline value where `idr_mod = idr_period % dx`. The `idr_mod`
  value starts at 0 and is incremented until we reach the
  idr_period value. When we want to generate an open GOP,
  e.g. only one IDR at the start, you can set `idr_period =
  0`. When using an open gop we cannot use `idr_period % dx` as
  that would trigger an arithmic exception. Therefore we check if
  the `idr_period` is 0 (open gop) or not (closed).

  idr = 16
  intra = 8
  ip = 4
  
  ```
  |  dx | type | idr_mod | intra_mod | ip_mod | note      |
  |-----+------+---------+-----------+--------+-----------|
  |   0 | IDR  |     <0> |         / |      / |           |
  |   1 | P    |       1 |         0 |    <0> | edge case |
  |   2 | B    |       2 |         1 |      1 |           |
  |   3 | B    |       3 |         2 |      2 |           |
  |   4 | B    |       4 |         3 |      3 |           |
  |   5 | P    |       5 |         4 |    <0> |           |
  |   6 | B    |       6 |         5 |      1 |           |
  |   7 | B    |       7 |         6 |      2 |           |
  |   8 | B    |       8 |         7 |      3 |           |
  |   9 | I    |       9 |       <0> |      0 |           |
  |  10 | B    |      10 |         1 |      1 |           |
  |  11 | B    |      11 |         2 |      2 |           |
  |  12 | B    |      12 |         3 |      3 |           |
  |  13 | P    |      13 |         4 |    <0> |           |
  |  14 | B    |      14 |         5 |      1 |           |
  |  15 | B    |      15 |         6 |      2 |           |
  |  16 | B    |      16 |         7 |      3 |           |
  |  17 | IDR  |     <0> |         / |      / |           |
  |  18 | P    |       1 |         0 |    <0> | edge case |
  |  19 | B    |       2 |         1 |      1 |           |
  | ... | ...  |     ... |       ... |    ... |           |
  |     |      |         |           |        |           |
  ```

  REFERENCE:

  This function is based on the h264encode.c
  `encoding2display_order()` function. The values that this
  function generates are the same as this
  `encoding2display_order()` function for the following 
  values that I used while testing:

  ```

  | idr | intra | ip | notes                                                        |
  |-----+-------+----|--------------------------------------------------------------|
  |  12 |     6 |  6 |                                                              |
  |  24 |     6 |  6 |                                                              |
  |   8 |     4 |  1 |                                                              |
  |  24 |     6 |  2 |                                                              |
  |   6 |     3 |  1 |                                                              |
  |   6 |     2 |  1 |                                                              |
  |  12 |     2 |  1 |                                                              |
  |   2 |     2 |  2 |                                                              |
  |  12 |     6 |  1 |                                                              |
  |   0 |     6 |  1 | open gop                                                     |
  |   0 |     0 |  1 | open gop                                                     |
  |   0 |     3 |  1 | open gop                                                     | 
  |   10 |    0 |  6 | different from h264encode; we do insert IDR, h264encode not. |
  ```

 */
static int gop_get_picture_frame_type(va_gop* ctx, uint64_t counter, uint8_t* frameType) {

  uint64_t idr_mod = 0; 
  uint64_t intra_mod = 0;
  uint64_t ip_mod = 0; 
  uint8_t has_ip = 0;
  
  if (NULL == ctx) {
    TRAE("Cannot get frame type, given `va_gop*` is NULL.");
    return -1;
  }

  if (NULL == frameType) {
    TRAE("Cannot get frame type, givern `frameType` is NULL.");
    return -2;
  }
  
  has_ip = (ctx->settings.ip_period <= 1) ? 0 : 1;

  if (0 != ctx->settings.idr_period) {
    /* Closed GOP + taking B frames into account. */
    idr_mod = counter % (ctx->settings.idr_period + has_ip);
  }
  else {
    /* Open GOP */
    idr_mod = counter;
  }

  /* idr frame */
  if (0 == idr_mod) {
    *frameType = VA_GOP_FRAME_TYPE_IDR;
    return 0;
  }

  /* intra frame */
  if (ctx->settings.intra_period > 1) { 
    intra_mod = (idr_mod - has_ip) % ctx->settings.intra_period;
    if (0 == intra_mod && idr_mod >= 2)  {
      *frameType = VA_GOP_FRAME_TYPE_I;
      return 0;
    }
  }

  /* p frame */
  *frameType = VA_GOP_FRAME_TYPE_P;

  if (0 == has_ip) {
    return 0;
  }

  /* b frame */
  ip_mod = (idr_mod - has_ip) % ctx->settings.ip_period;
  if (0 != ip_mod) {
    *frameType = VA_GOP_FRAME_TYPE_B;
    return 0;
  }
  
  return 0;
}

/* ------------------------------------------------------- */
/*

  This function accepts the current `counter` (which is the loop
  index of your encoding loop), the current frame type and
  calculates the display order. For example when looking
  at the table below, when `counter = 1` you should feed source
  frame with index `6` to the encoder.

  B-frames will always refer to the previous counter value; the
  reason for this is that there will be one forward reference
  (e.g.  the P frame at `counter = 1`.

  Also note that the result value is an absolute value, e.g.
  it's not the same as the value we used for `pic_order_cnt_lsb`
  as that value is reset for each IDR (and a couple of other
  rules are applied). You can think about the `display_order` as the
  order relative to the main timeline and the `pic_order_cnt_lsb`
  as the display order relative to the IDR timeline.

  | counter | type | display index |
  |---------+------+---------------+
  |       0 | IDR  |             0 |
  |       1 | P    |             6 |
  |       2 | B    |             1 |
  |       3 | B    |             2 |
  |       4 | B    |             3 |
  |       5 | B    |             4 |
  |       6 | B    |             5 |
  |       7 | P    |            12 |
  |       8 | B    |             7 |
  |       9 | B    |             8 |
  |      10 | B    |             9 |
  |      11 | B    |            10 |
  |      12 | B    |            11 |
  |      13 | P    |            18 |

 */
static int gop_get_picture_display_order(
  va_gop* ctx,
  uint64_t counter,
  uint8_t frameType,
  uint64_t* displayFrameIndex
)
{
  uint32_t ip_dist = 0;
  
  if (NULL == ctx) {
    TRAE("Cannot get the display frame index as the given `va_gop*` is NULL.");
    return -1;
  }

  if (NULL == displayFrameIndex) {
    TRAE("Cannot get the display frame index as the given `displayFrameIndex*` is NULL.");
    return -2;
  }

  /* When we don't have B-frames, i.e. ip_period <= 1, then distance is 0. */
  ip_dist = (ctx->settings.ip_period <= 1) ? 0 : (ctx->settings.ip_period - 1);
  
  switch (frameType) {
    
    case VA_GOP_FRAME_TYPE_IDR: {
      *displayFrameIndex = counter;
      break;
    }
    
    case VA_GOP_FRAME_TYPE_I: {
      *displayFrameIndex = counter + ip_dist; 
      break;
    }
    
    case VA_GOP_FRAME_TYPE_P: {
      *displayFrameIndex = counter + ip_dist;
      break;
    }
    
    case VA_GOP_FRAME_TYPE_B: {
      *displayFrameIndex = counter - 1;
      break;
    }
    
    default: {
      TRAE("Cannot convert the given `counter` into a `displayFrameIndex` as it's unknown: %u", frameType);
      return -3;
    }
  }

  return 0;
}

/* ------------------------------------------------------- */

static const char* gop_frame_type_to_string(uint8_t type) {
  
  switch (type) {
    case VA_GOP_FRAME_TYPE_B:   { return "B";       }
    case VA_GOP_FRAME_TYPE_P:   { return "P";       }
    case VA_GOP_FRAME_TYPE_I:   { return "I";       }
    case VA_GOP_FRAME_TYPE_IDR: { return "IDR";     } 
    default:                    { return "UNKNOWN"; } 
  }
};

/* ------------------------------------------------------- */

int va_gop_print(va_gop* ctx) {

  if (NULL == ctx) {
    TRAE("Cannot print info about the `va_gop*` as it's NULL.");
    return -1;
  }

  TRAD("va_gop");
  TRAD("  cfg.idr_period: %u", ctx->settings.idr_period);
  TRAD("  cfg.intra_period: %u", ctx->settings.intra_period);
  TRAD("  cfg.ip_period: %u", ctx->settings.ip_period);
  TRAD("");

  return 0;
}

/* ------------------------------------------------------- */

int va_gop_picture_print(va_gop_picture* pic) {

  if (NULL == pic) {
    TRAE("Cannot print `va_gop_picture*` as it's NULL.");
    return -1;
  }

  TRAD("va_gop_picture");
  TRAD("  frame_type: %s", gop_frame_type_to_string(pic->frame_type));
  TRAD("  frame_num: % 3llu", pic->frame_num);
  TRAD("  display_order: % 3llu", pic->display_order);
  TRAD("  poc_lsb: % 3llu", pic->poc_lsb);
  TRAD("  poc_top_field: % 3llu", pic->poc_top_field);
  TRAD("  is_reference: %u", pic->is_reference);
  TRAD("  idr_id: %u", pic->idr_id);

  return 0;
}

int va_gop_picture_print_oneliner(va_gop_picture* pic) {
  
  if (NULL == pic) {
    TRAE("Cannot print the `va_gop_picture*` as it's NULL.");
    return -1;
  }

  TRAD(
    "va_gop_picture. "
    "frame_num: % 3llu, "
    "display_order: % 3llu, "
    "is_reference: %u, "
    "poc_lsb: % 3u, "
    "poc_top_field: % 3u, "
    "idr_id: % 2u, "
    "frame_type: % 4s, "
    ,
    pic->frame_num,
    pic->display_order,
    pic->is_reference,
    pic->poc_lsb,
    pic->poc_top_field,
    pic->idr_id,
    gop_frame_type_to_string(pic->frame_type)
  );
  
  return 0;
}

/* ------------------------------------------------------- */

/*

  GENERAL INFO:

    This function will allocate the `va_enc` instance which wraps
    the encoder functionality for VAAPI. We will make a
    connection with XORG, initialize VAAPI, setup some defaults,
    etc.
    
    Currently we are limiting the features we implement:
    
    - No support for B-frames atm; we focus on real-time
      streaming with WebRtc support which doesn't allow B-frames
      at the moment.
    
    - We use the constrained baseline profile.
    
    - No support for cropping. 

    - Only works when the backend can be used with packed headers.

    - Only works with YUV420.

    - We are not (yet) using `ref_list{0,1}_max`. Should we? https://github.com/intel/libva-utils/issues/267

  TODO:
  
    @todo: We might want to query the entry points to make sure 
           that we support an h264 encoder.

  REFERENCES:

    [0]: https://github.com/intel/libva-utils/blob/master/encode/h264encode.c "VA Utils repository"
 
 */
int va_enc_create(va_enc_settings* cfg, va_enc** ctx) {

  VAConfigAttrib supported_attribs[VAConfigAttribTypeMax] = {};
  VAConfigAttrib encoder_attribs[3] = {};
  VAStatus status = VA_STATUS_SUCCESS;
  va_enc* inst = NULL;
  uint32_t flags = 0;
  uint32_t i = 0;
  int r = 0;

  TRAE("@todo rename mapping functions in vaapi-utils.c; see vtbox-enc and api doc.");

  /* --------------------------------------- */
  /* VALIDATE                                */
  /* --------------------------------------- */
  
  if (NULL == cfg) {
    TRAE("Cannot create the `va_enc` instance as the given `va_enc_settings` is NULL.");
    r = -10;
    goto error;
  }

  if (NULL == ctx) {
    TRAE("Cannot create the `va_enc` instance as the given `va_enc**` is NULL.");
    r = -20;
    goto error;
  }

  if (NULL != (*ctx)) {
    TRAE("Cannot create the `va_enc` instance as the give *(va_enc**) is not NULL. Initialize the result to NULL.");
    r = -30;
    goto error;
  }

  r = gop_settings_validate(&cfg->gop);
  if (r < 0) {
    TRAE("Cannot create the `va_enc` instance as the gop settings are invalid.");
    r = -40;
    goto error;
  }

  if (1 != cfg->gop.ip_period) {
    TRAE("Cannot create the `va_enc` instance as the `ip_period` setting from `va_gop_settings` must be 1 as we don't support B-frames yet.");
    r = -45;
    goto error;
  }
  
  if (0 == cfg->image_width) {
    TRAE("Cannot create the `va_enc` instance as the `image_width` is not set.");
    r = -50;
    goto error;
  }

  if (0 == cfg->image_height) {
    TRAE("Cannot create the `va_enc` instance as the `image_height` is not set.");
    r = -60;
    goto error;
  }

  if (0 == cfg->image_format) {
    TRAE("Cannot create the `va_enc` instance as the `image_format` is not set.");
    r = -62;
    goto error;
  }

  /* @todo should we check this? */
  if (0  == cfg->num_ref_frames) {
    TRAE("Cannot create the `va_enc` instance as the given `num_ref_frames` is 0.");
    r = -70;
    goto error;
  }

  inst = calloc(1, sizeof(va_enc));
  if (NULL == inst) {
    TRAE("Cannot create the `va_enc` instance, we failed to allocate the `va_enc` instance. Out of memory?");
    r = -80;
    goto error;
  }

  /* --------------------------------------- */
  /* DEFAULT MEMBERS                         */
  /* --------------------------------------- */

  /* We keep a copy of the given settings + deep copy of callbacks. */
  memcpy(&inst->settings, cfg, sizeof(*cfg));

  inst->entropy_mode = VA_ENTROPY_MODE_CAVLC;
  inst->profile = VAProfileH264ConstrainedBaseline;
  inst->level_idc = 41; /* 50Mbit, 1920x1080@30fps, 1280x720@60fps, 4 ref frames, etc. */
  inst->width_mb_aligned = (cfg->image_width + 15) / 16 * 16;
  inst->height_mb_aligned = (cfg->image_height + 15) / 16 * 16;
  inst->width_in_mbs = inst->width_mb_aligned / 16;
  inst->height_in_mbs = inst->height_mb_aligned / 16;
  inst->short_ref_count = 0;

  TRAE("@todo rename va_util_map_image_format_to_sampling_format to va_util_imageformat_to_samplingformat");
  
  /* Convert the image format to VAAPI sampling and storage formats. */
  r = va_util_map_image_format_to_sampling_format(cfg->image_format, &inst->sampling_format);
  if (r < 0) {
    TRAE("Cannot create the `va_enc` instance. Failed to map the image format into the sampling format that VAAPI uses. ");
    r = -82;
    goto error;
  }

  TRAE("@todo rename va_util_map_image_format_to_storage_format to va_util_imageformat_to_storageformat");
  
  r = va_util_map_image_format_to_storage_format(cfg->image_format, &inst->storage_format);
  if (r < 0)  {
    TRAE("Cannot create the `va_enc` instance. Failed to map the image format into the storage format that VAAPI uses (NV12, YV12, etc.).");
    r = -84;
    goto error;
  }

  /* Validate the configs */
  if (inst->settings.image_width != inst->width_mb_aligned) {
    TRAE("Cannot create the `va_enc` instance. This current implementation doesn't support widths that aren't macroblock aligned. Use a width that is a multiple of 16.");
    r = -90;
    goto error;
  }

  if (inst->settings.image_height != inst->height_mb_aligned) {
    TRAE("Cannot create the `va_enc` instance. This current implementation doesn't support heights that aren't macroblock aligned. Use a height that is a multiple of 16.");
    r = -100;
    goto error;
  }

  /* Set the constraint flag that are necessary when geneating the SPS. See [this][0] reference. */
  switch (inst->profile) {
    
    case VAProfileH264Main: {
      inst->constraint_set_flag |= (1 << 1); /* Annex A.2.2 */
      break;
    }

    case VAProfileH264High: {
      inst->constraint_set_flag |= (1 << 3); /* Annex A.2.4 */
      break;
    }

    /* Default to constrained based line. */
    case VAProfileH264ConstrainedBaseline:
    default: {
      inst->profile = VAProfileH264ConstrainedBaseline;
      inst->constraint_set_flag |= (1 << 0 | 1 << 1); /* Annex A.2.1 & A.2.2 */
      inst->settings.gop.ip_period = 0; /* No B-frames */
      break;
    }
  }

  /* --------------------------------------- */
  /* INITIALIZE                              */
  /* --------------------------------------- */

  r = tra_golomb_writer_create(&inst->bitstream, 1024);
  if (r < 0) {
    TRAE("Cannot create the `va_enc` instance. Failed to create the `tra_golomb` context that we use to write the bitstream.");
    r = -110;
    goto error;
  }

  /* Get a display that represents a connection with XORG */
  inst->xorg_display = XOpenDisplay(NULL);
  if (NULL == inst->xorg_display) {;
    TRAE("Cannot create the `va_enc` instance. Failed to open the display: %s", strerror(errno));
    r = -120;
    goto error;
  }

  inst->va_display = vaGetDisplay(inst->xorg_display);
  if (NULL == inst->va_display) {
    TRAE("Cannot create the `va_enc` instance. Failed to get the display.");
    r = -130;
    goto error;
  }

  /* Initialize VA and get the minor/major version. */
  status = vaInitialize(inst->va_display, &inst->ver_major, &inst->ver_minor);
  if (VA_STATUS_SUCCESS != status) {
    TRAE("Cannot create the `va_enc` instance. Failed to initialize VA.");
    r = -140;
    goto error;
  }

  /* --------------------------------------- */
  /* CHECK CAPABILITIES                      */
  /* --------------------------------------- */

  /* Check what attributes are supported. */
  for (i = 0; i < VAConfigAttribTypeMax; ++i) {
    supported_attribs[i].type = i; /* From `h264encode.c` in the [va-utils repository][0]. */
  }
  
  status = vaGetConfigAttributes(
    inst->va_display,
    inst->profile,
    VAEntrypointEncSlice,
    supported_attribs,
    VAConfigAttribTypeMax
  );

  if (VA_STATUS_SUCCESS != status) {
    TRAE("Cannot create the `va_enc` instance. Failed to get the supported config attributes.");
    r = -150;
    goto error;
  }

  /* Currently, this implementation assumes that we have to generate packed headers. */
  if (supported_attribs[VAConfigAttribEncPackedHeaders].value == VA_ATTRIB_NOT_SUPPORTED) {
    TRAD("Cannot create the `va_enc` instance. Doesn't supports VAConfigAttribEncPackedHeaders");
    r = -160;
    goto error;
  }

  if (0 == (supported_attribs[VAConfigAttribRTFormat].value & VA_RT_FORMAT_YUV420)) {
    TRAE("Cannot create the `va_enc` intance. The encoder does not support YUV420. Currently we are limited to this format.");
    r = -180;
    goto error;
  }

  /* 
     @todo(roxlu): The current implementation only works when
     packed headers are supported. We do use this flag but don't
     handle the case when packed heades are not supported.
  */
  inst->is_using_packed_headers = 1;

  /* --------------------------------------- */
  /* CONFIGURE                               */
  /* --------------------------------------- */

  /* Setup the attributes we want to use. */
  encoder_attribs[0].type = VAConfigAttribRTFormat;
  encoder_attribs[0].value = inst->sampling_format; 
  encoder_attribs[1].type = VAConfigAttribRateControl;
  encoder_attribs[1].value = VA_RC_CQP;
  encoder_attribs[2].type = VAConfigAttribEncPackedHeaders;
  encoder_attribs[2].value = VA_ENC_PACKED_HEADER_NONE;

  flags = supported_attribs[VAConfigAttribEncPackedHeaders].value;
  if (flags != VA_ATTRIB_NOT_SUPPORTED) {

    TRAD("Packed headers are supported.");
        
    if (flags & VA_ENC_PACKED_HEADER_SEQUENCE) {
      TRAD("  Supports packed sequence headers.");
      encoder_attribs[2].value |= VA_ENC_PACKED_HEADER_SEQUENCE;
    }
    
    if (flags & VA_ENC_PACKED_HEADER_PICTURE) {
      TRAD("  Supports packed picture headers.");
      encoder_attribs[2].value |= VA_ENC_PACKED_HEADER_PICTURE;
    }
    
    if (flags & VA_ENC_PACKED_HEADER_SLICE) {
      TRAD("  Supports packed slice headers.");
      encoder_attribs[2].value |= VA_ENC_PACKED_HEADER_SLICE;
    }

    if (flags & VA_ENC_PACKED_HEADER_MISC) {
      TRAD("  Supports packed misc headers.");
      encoder_attribs[2].value |= VA_ENC_PACKED_HEADER_MISC;
    }
  }

  /* Set the maximum supported reference frames for list0 (bottom 16 bits) and list1 (top 16 bits). */
  flags = supported_attribs[VAConfigAttribEncMaxRefFrames].value;

  if (flags != VA_ATTRIB_NOT_SUPPORTED) {
    inst->ref_list0_max = flags & 0xFFFF;
    inst->ref_list1_max = (flags >> 16) & 0xFFFF;
    TRAE("@todo use ref_list0_max and ref_list1_max to unset unused reference frames.");
  }
  else {
    TRAE("@todo when we can't query the maximum number of reference frames; what values should we use?");
  }

  status = vaCreateConfig(
    inst->va_display,
    inst->profile,
    VAEntrypointEncSlice,
    encoder_attribs,
    3,
    &inst->config_id
  );

  if (VA_STATUS_SUCCESS != status) {
    TRAE("Cannot create the `va_enc` instance. Failed to create the config.");
    r = -190;
    goto error;
  }

  /* --------------------------------------- */
  /* CREATE: GOP, ENCODER, BUFFERS           */
  /* --------------------------------------- */

  r = va_gop_create(&cfg->gop, &inst->gop);
  if (r < 0) {
    TRAE("Cannot create the `va_enc` instance. Failed to create the `va_gop` that we use to generate the GOP, frame_num, etc.");
    r = -200;
    goto error;
  }
  
  r = enc_init_encode(inst);
  if (r < 0) {
    TRAE("Cannot create the `va_enc` instance. Failed to initialize the encoder.");
    r = -210;
    goto error;
  }

  /* ... and assign */
  *ctx = inst;

 error:

  /* When an error occured; destroy if necessary and unset. */
  if (r < 0) {
    
    if (NULL != inst) {
      
      r = va_enc_destroy(inst);
      if (r < 0) {
        TRAE("Failed to cleanly destroy the `va_enc` after an error occured while creating the object.");
      }
      
      inst = NULL;
    }

    if (NULL != ctx) {
      *ctx = NULL;
    }
  }
  
  return r;
}

/* ------------------------------------------------------- */

/*
  This function deallocates everything that was allocated in
  `va_enc_create()`. We explicitly touch every member and reset
  them.
 */
int va_enc_destroy(va_enc* ctx) {

  VAStatus status = VA_STATUS_SUCCESS;
  uint32_t i = 0;
  int ret = 0;
  int r = 0;

  if (NULL == ctx) {
    TRAE("Cannot destroy the encoder; given `ctx` is NULL.");
    return -1;
  }
  
  /* Free coded buffers. */
  if (NULL != ctx->coded_buffers) {
    
    for (i = 0; i < ctx->num_coded_buffers; ++i) {
      status = vaDestroyBuffer(ctx->va_display, ctx->coded_buffers[i]);
      if (VA_STATUS_SUCCESS != status) {
        TRAE("Failed to cleanly destroy a coded buffer.");
        ret -= 1;
      }
    }

    free(ctx->coded_buffers);
  }

  /* Free reference surfaces. */
  if (NULL != ctx->ref_surfaces) {
    status = vaDestroySurfaces(ctx->va_display, ctx->ref_surfaces, ctx->num_ref_surfaces);
    if (VA_STATUS_SUCCESS != status) {
      TRAE("Failed to cleanly destroy the reference surfaces.");
      ret -= 2;
    }
  }
  
  /* Free source surfaces. */
  if (NULL != ctx->src_surfaces) {
    status = vaDestroySurfaces(ctx->va_display, ctx->src_surfaces, ctx->num_src_surfaces);
    if (VA_STATUS_SUCCESS != status) {
      TRAE("Failed to cleanly destroy the source surfaces.");
      ret -= 3;
    }
  }

  /* Destroy context */
  if (0 != ctx->context_id) {
    status = vaDestroyContext(ctx->va_display, ctx->context_id);
    if (VA_STATUS_SUCCESS != status) {
      TRAE("Failed to cleanly destroy the context id.");
      ret -= 4;
    }
  }

  /* Destroy config */
  if (0 != ctx->config_id) {
    status = vaDestroyConfig(ctx->va_display, ctx->config_id);
    if (VA_STATUS_SUCCESS != status) {
      TRAE("Failed to cleanly destroy the config.");
      ret -= 5;
    }
  }

  /* Terminate VA */
  if (NULL != ctx->va_display) {
    status = vaTerminate(ctx->va_display);
    if (VA_STATUS_SUCCESS != status) {
      TRAE("Failed to cleanly terminate VA.");
      ret -= 6;
    }
  }

  /* Destroy `va_gop` */
  if (NULL != ctx->gop) {
    r = va_gop_destroy(ctx->gop);
    if (r < 0) {
      TRAE("Failed to cleanly destroy the `va_gop` instance.");
      ret -= 7;
    }
  }

  /* Destroy bit stream writer. */
  if (NULL != ctx->bitstream) {
    r = tra_golomb_writer_destroy(ctx->bitstream);
    if (r < 0) {
      TRAE("Failed to cleanly destroy the `tra_golomb`.");
      ret -= 8;
    }
  }

  if (NULL != ctx->xorg_display) {
    XCloseDisplay(ctx->xorg_display);
  }

  ctx->xorg_display = NULL;
  ctx->va_display = NULL;
  ctx->src_surfaces = NULL;
  ctx->ref_surfaces = NULL;
  ctx->coded_buffers = NULL;
  ctx->bitstream = NULL;

  ctx->config_id = 0;
  ctx->context_id = 0;
  ctx->profile = 0;
  ctx->ver_minor = 0;
  ctx->ver_major = 0;

  ctx->level_idc = 0;
  ctx->sampling_format = 0;
  ctx->storage_format = 0;
  ctx->width_mb_aligned = 0;
  ctx->height_mb_aligned = 0;
  ctx->width_in_mbs = 0;
  ctx->height_in_mbs = 0;
  ctx->entropy_mode = VA_ENTROPY_MODE_CAVLC;
  ctx->is_using_packed_headers = 0;
  ctx->constraint_set_flag = 0;

  ctx->num_coded_buffers = 0;
  ctx->num_src_surfaces = 0;
  ctx->num_ref_surfaces = 0;
  ctx->curr_ref_surface_index = 0;
  ctx->curr_coded_buffer_index = 0;
  ctx->short_ref_count = 0;
  ctx->ref_list0_max = 0;
  ctx->ref_list1_max = 0;

  free(ctx);
  ctx = NULL;
  
  return ret;
}

/* ------------------------------------------------------- */

/*

  GENERAL INFO:

    This function encodes the given data. We first map a source
    surface, then we map it's buffer and finally upload the given
    data.
    
    Once the source surface contains the given frame, we use the
    `va_gop` instance to determine what kind of slices/frame we
    have to create (IDR, I, P, B).
    
    The encode process starts with a call to `vaBeginPicture()`
    which must be called for each frame that we want to
    encode. When we're encoding the first frame or (if you
    prefer) before each IDR, we specify the Sequence Parameters
    (seq_param). For each picture we setup Picture Parameters
    (pic_param).
  
  TODO:

    @todo When something fails after we've called
    `vaBeginPicture()` we should call `vaEndPicture()`. To check
    if `vaBeginPicture()` was called we might need to add some
    state var. ... Or maybe I don't check this at all and simply
    EXIT (?).

 */
int va_enc_encode(
  va_enc* ctx,
  tra_sample* sample,
  uint32_t type,
  uint8_t* data
)
{
  VAStatus status = VA_STATUS_SUCCESS;
  tra_memory_image* src_image = NULL;
  uint8_t* input_ptr = NULL;
  VAImage input_image = {};
  uint32_t src_stride = 0;
  uint32_t dst_stride = 0;
  uint8_t* src_ptr = NULL;
  uint8_t* dst_ptr = NULL;
  uint32_t half_h = 0;
  uint32_t w = 0;
  uint32_t h = 0;
  uint32_t j = 0;
  int r = 0;

  /* --------------------------------------- */
  /* VALIDATE                                */
  /* --------------------------------------- */
  
  if (NULL == ctx) {
    TRAE("Cannot encode with `vaapi-enc` because the given `va_enc*` is NULL.");
    r = -10;
    goto error;
  }

  if (NULL == ctx->gop) {
    TRAE("Cannot encode with `vaapi-enc` because `va_enc::gop` is NULL. Not initialized?");
    r = -20;
    goto error;
  }

  if (NULL == sample) {
    TRAE("Cannot encode with `vaapi-enc`, the given `tra_sample*` is NULL.");
    r = -30;
    goto error;
  }
  
  if (NULL == data) {
    TRAE("Cannot encode with `vaapi-enc`, the given `data` is NULL.");
    r = -40;
    goto error;
  }

  if (TRA_MEMORY_TYPE_IMAGE != type) {
    TRAE("Cannot encode with `vaapi-enc`, the given input type is not supported.");
    r = -50;
    goto error;
  }

  /* Make sure the given image has a valid size. */
  src_image = (tra_memory_image*) data;
  w = src_image->image_width;
  h = src_image->image_height;

  if (w != ctx->settings.image_width) {
    TRAE("Cannot encode with `vaapi-enc` as the input image width (%u) is not what we would expect (%u).", w, ctx->settings.image_width);
    r = -54;
    goto error;
  }

  if (h != ctx->settings.image_height) {
    TRAE("Cannot encode with `vaapi-enc` as the input image height (%u) is not what we would expect (%u).", h, ctx->settings.image_height);
    r = -58;
    goto error;
  }

  /* --------------------------------------- */
  /* UPLOAD YUV                              */
  /* --------------------------------------- */
      
  status = vaDeriveImage(ctx->va_display, ctx->src_surfaces[ctx->curr_src_surface_index], &input_image);
  if (VA_STATUS_SUCCESS != status) {
    TRAE("Cannot encode with `vaapi-enc`. Failed to get image information.");
    r = -60;
    goto error;
  }

  if (ctx->storage_format != input_image.format.fourcc) {
    TRAE("Cannot encode with `vaapi-enc`, we expect the VAImage to have a format of NV12.");
    r = -70;
    goto error;
  }

  status = vaMapBuffer(ctx->va_display, input_image.buf, (void**) &input_ptr);
  if (VA_STATUS_SUCCESS != status) {
    TRAE("Cannot encode with `vaapi-enc`, failed to map the input image buffer.");
    r = -80;
    goto error;
  }

  /* Copy the Y plane. */
  dst_ptr = input_ptr + input_image.offsets[0];
  dst_stride = input_image.pitches[0];
  src_ptr = src_image->plane_data[0];
  src_stride = src_image->plane_strides[0];
  
  for (j = 0; j < h; ++j) {
    memcpy(dst_ptr, src_ptr, src_stride);
    dst_ptr = dst_ptr + dst_stride;
    src_ptr = src_ptr + src_stride;
  }
  
  /* Copy the UV plane. */
  half_h = h / 2;
  dst_ptr = input_ptr + input_image.offsets[1];
  dst_stride = input_image.pitches[1];
  src_ptr = src_image->plane_data[1];
  src_stride = src_image->plane_strides[1];

  for (j = 0; j < half_h; ++j) {
    memcpy(dst_ptr, src_ptr, src_stride);
    dst_ptr = dst_ptr + dst_stride;
    src_ptr = src_ptr + src_stride;
  }

  /* --------------------------------------- */
  /* ENCODE                                  */
  /* --------------------------------------- */

  /* Check what kind of slice/frame we have to create. */
  r = va_gop_get_next_picture(ctx->gop, &ctx->gop_pic);
  if (r < 0) {
    TRAE("Cannot encode with `vaapi-enc`. Failed to get the next GOP picture.");
    r = -90;
    goto error;
  }

  va_gop_picture_print_oneliner(&ctx->gop_pic);
  
  if (VA_GOP_FRAME_TYPE_B == ctx->gop_pic.frame_type) {
    TRAE("Cannot encode with `vaapi-enc`. Requested to create a B-frame which we don't support yet. Make sure to the `ip_period` to 1 for the `va_gop_settings` (exiting).");
    exit(EXIT_FAILURE);
  }
    
  status = vaBeginPicture(ctx->va_display, ctx->context_id, ctx->src_surfaces[ctx->curr_src_surface_index]);
  if (VA_STATUS_SUCCESS != status) {
    TRAE("Cannot encode with `vaapi-enc`. Failed to `vaBeginPicture`.");
    r = -100;
    goto error;
  }

  if (VA_GOP_FRAME_TYPE_IDR != ctx->gop_pic.frame_type) {
    
    r = enc_render_picture(ctx);
    if (r < 0) {
      TRAE("Cannot encode with `vaapi-enc`. Failed to render the current picture.");
      r = -110;
      goto error;
    }
  }
  else {

    /* Generate IDR + SPS + PPS  */
    r = enc_render_sequence(ctx);
    if (r < 0) {
      TRAE("Cannot encode with `vaapi-enc`. Failed to render the sequence.");
      r = -120;
      goto error;
    }

    r = enc_render_picture(ctx);
    if (r < 0) {
      TRAE("Cannot encode with `vaapi-enc`. Failed to render the picture.");
      r = -130;
      goto error;
    }

    /* When we're using packed headers, generate the SPS/PPS */
    if (1 == ctx->is_using_packed_headers) {
      
      r = enc_render_packed_sequence(ctx);
      if (r < 0) {
        TRAE("Cannot encode with `vaapi-enc`. Failed to render the packed sequence.");
        r = -140;
        goto error;
      }

      r = enc_render_packed_picture(ctx);
      if (r < 0) {
        TRAE("Cannot encode with `vaapi-enc`. Failed to render the packed picture.");
        r = -150;
        goto error;
      }
    }    
  }

  /* Render the slice and (maybe) packed slice. */
  r = enc_render_slice(ctx);
  if (r < 0) {
    TRAE("Cannot encode with `vaapi-enc`. Failed to render the slice.");
    r = -160;
    goto error;
  }

  if (1 == ctx->is_using_packed_headers) {
    r = enc_render_packed_slice(ctx);
    if (r < 0) {
      TRAE("Cannot encode with `vaapi-enc`. Failed to render the packed slice.");
      r = -170;
      goto error;
    }
  }

  status = vaEndPicture(ctx->va_display, ctx->context_id);
  if (VA_STATUS_SUCCESS != status) {
    TRAE("Cannot encode with `vaapi-enc`. Failed to `vaEndPicture`.");
    r = -180;
    goto error;
  }

  /* Receive the encoded data. */
  r = enc_save_coded_data(ctx);
  if (r < 0) {
    TRAE("Cannot encode with `vaapi-enc`. Failed to save the coded data.");
    r = -190;
    goto error;
  }

  /* 
     Now we need to save the current picture (gop_pic) into our
     reference frames array, but only when it's marked as a
     reference frame. This is done with
     `enc_update_reference_frames()`.
  */
  r = enc_update_reference_frames(ctx);
  if (r < 0) {
    TRAE("Cannot encode with `vaapi-enc`. Failed to save the current picture as reference frame.");
    r = -200;
    goto error;
  }

  /* Update our indices. */
  ctx->curr_src_surface_index++;
  if (ctx->curr_src_surface_index >= ctx->num_src_surfaces) {
    ctx->curr_src_surface_index = 0;
  }

  ctx->curr_coded_buffer_index++;
  if (ctx->curr_coded_buffer_index >= ctx->num_coded_buffers) {
    ctx->curr_coded_buffer_index = 0;
  }

  ctx->curr_ref_surface_index++;
  if (ctx->curr_ref_surface_index >= ctx->num_ref_surfaces) {
    ctx->curr_ref_surface_index = 0;
  }

 error:

  if (NULL != input_ptr &&
      NULL != ctx)
    {
      status = vaUnmapBuffer(ctx->va_display, input_image.buf);
      if (VA_STATUS_SUCCESS != status) {
        TRAE("Cannot encode with `vaapi-enc`. Failed to cleanly unmap the `VAImage` that we use as input.");
        r -= 7;
      }
    }

  /* When the width and height are set, we assume that `vaDeriveImage` has been called and we have to destroy it. */
  if (0 != input_image.width &&
      0 != input_image.height)
    {
      status = vaDestroyImage(ctx->va_display, input_image.image_id);
      if (VA_STATUS_SUCCESS != status) {
        TRAE("Cannot encode with `vaapi-enc`. Failed to cleanly destroy the `VAImage`. ");
        r -= 8;
      }
    }

  return r;
}

/* ------------------------------------------------------- */

int va_enc_flush(va_enc* ctx) {

  TRAE("@todo implement the va_enc_flush()!");

  return -10;
}

/* ------------------------------------------------------- */

/* 
   When we've delivered a raw YUV frame to the encoder and took
   all the required steps like creating the sequence, picture
   param, bit stream, we can use this function to finally
   extract/receive the encoded data.
*/
static int enc_save_coded_data(va_enc* ctx) {

  tra_encoder_callbacks* callbacks = NULL;
  tra_memory_h264 encoded_data = { 0 };
  VACodedBufferSegment* buf_list = NULL;
  VAStatus status = VA_STATUS_SUCCESS;
  int r = 0;

  if (NULL == ctx) {
    TRAE("Cannot save the encoded data. The given `va_enc*` is NULL.");
    r = -10;
    goto error;
  }

  if (NULL == ctx->settings.callbacks) {
    TRAE("Cannot save the encoded data. The `va_enc_settings.callbacks` member is NULL.");
    r = -20;
    goto error;
  }

  if (NULL == ctx->settings.callbacks->on_encoded_data) {
    TRAE("Cannot save the encoded data. The `tra_encoder_callbacks::on_encoded_data` member is NULL.");
    r = -30;
    goto error;
  }

  status = vaSyncSurface(ctx->va_display, ctx->src_surfaces[ctx->curr_src_surface_index]);
  if (VA_STATUS_SUCCESS != status) {
    TRAE("Cannot save the encoded data. Failed to synchronishe the surface.");
    r = -30;
    goto error;
  }

  status = vaMapBuffer(
    ctx->va_display,
    ctx->coded_buffers[ctx->curr_coded_buffer_index],
    (void**)(&buf_list)
  );

  if (VA_STATUS_SUCCESS != status) {
    TRAE("Cannot save the encoded data. Failed to map the buffer.");
    r = -40;
    goto error;
  }

  callbacks = ctx->settings.callbacks;
  
  while (NULL != buf_list) {

    encoded_data.data = buf_list->buf;
    encoded_data.size = buf_list->size;

    r = callbacks->on_encoded_data(
      TRA_MEMORY_TYPE_H264,
      &encoded_data,
      callbacks->user
    );

    if (r < 0) {
      TRAE("The `on_encoded_data()` callbacks returned an error.");
      /* fall through */
    }

    buf_list = (VACodedBufferSegment*) buf_list->next;
  }

  status = vaUnmapBuffer(
    ctx->va_display,
    ctx->coded_buffers[ctx->curr_coded_buffer_index]
  );

  if (VA_STATUS_SUCCESS != status) {
    TRAE("Failed to cleanly unmap the buffer while saving the coded data.");
    r = -50;
    goto error;
  }

 error:
  return r;
}

/* ------------------------------------------------------- */

/*
  GENERAL INFO:

  This function is called each time we start a new sequence
  (e.g. GOP). Note that this is not 100% the same as create the
  SPS as we have to store/set some other VAAPI relevant
  information too. The naming might be a bit confusing;
  especially when you consider that `enc_render_picture()` does
  not generate a PPS and must be called for each frame we want to
  encode.

  We setup the Sequence Parameters to be used for
  frame based encoding by setting the `frame_mbs_only_flag =
  1`. I've included the descriptions as they are (partly) found
  int he H264 2016 spec.

  chroma_format_idc: 

    Specifies the chroma sampling relative to the luma sampling
    as specified in clause 6.2 of the 2016 spec. The value of
    chroma_format_idc shall be in the range of 0 to 3,
    inclusive. When chroma_format_idc is not present, it shall be
    inferred to be equal to 1 (4:2:0). 0: monochrome, 1: 4:2:0,
    2: 4:2:2, 3: 4:4:4

  frame_mbs_only_flag: 

    A value of 0 specifies that coded pictures of the coded video
    sequence may either be coded fields or coded
    frames. frame_mbs_only_flag equal to 1 specifies that every
    coded picture of the coded video sequence is a coded frame
    containing only frame macroblocks.

  mb_adaptive_frame_field_flag: 

    A value equal to 0 specifies no switching between frame and
    field macroblocks within a
    picture. mb_adaptive_frame_field_flag equal to 1 specifies
    the possible use of switching between frame and field
    macroblocks within frames. When mb_adaptive_frame_field_flag
    is not present it shall be inferred to be equal to 0.

  direct_8x8_inference_flag:

    Specifies the method used in the derivation process for luma
    motion vectors for B_Skip, B_Direct_16x16 as specified in
    clause 8.4.1.2. When frame_mbs_only_flag is equal to 0,
    direct_8x8_inference_flag shall be equal to 1.

  log2_max_frame_num_minus4:

    Specifies the value of the variable _MaxFrameNum_ that is
    used in frame_num related derivations as follows:
    `MaxFrameNum = 2^(log2_max_frame_num_minus4 + 4)`. The
    frame_num is incremented for each reference frame and used in
    the slice header. The `va_gop*` functions calculate this
    value.

  seq_scaling_matrix_present_flag:

    Determines if the scaling values are presen.t
  
  pic_order_cnt_type:    

    Specifies the picture order count logic that we use; Our
    implementation uses type 0.

  log2_max_pic_order_cnt_lsb_minus4: 

    Specifies the value of the variable _MaxPicOrderCntLsb_ that
    is used in the decoder process for picture order count as
    specified in clause 8.2.1 as follows: `MaxPicOrderCntLsb =
    2^(log2_max_pic_order_cnt_lsb_minus4 + 4)`. The value
    of log2_max_pic_order_cnt_lsb_minus4 shall be in the 
    ragne of 0 to 12 inclusive.
    
  delta_pic_order_always_zero_flag:

    A value equal to 1 specifies that delta_pic_order_cnt[0] and
    delta_pic_order_cnt[1] are not present in the slice headers
    of the sequence and shall be inferred to be equal to
    0. delta_pic_order_always_zero_flag equal to 0 specifies that
    delta_pic_order_cnt[0] is present in the slice headers of the
    seqeuence and delta_pic_order_cnt[1] may be present in the
    slice headers of the sequence.

  TODO:

  @todo Currently I've hardcoded the bitrate value. I still have
        to look into bitrate control methods for VAAPI.

  @todo I've hardcoded the time_scale value to 900, similar to
        the value that is used in the h264encode.c example.

  @todo I've hardcoded the `num_units_in_ticks = 15`, similar to
        the value that is used in the h264encode.c example

  @todo Currently we don't support cropping, so we check this. At 
        some point we can add this. 

  @todo We are not destroying the buffer we've craeted for the
        SPS. The h264encode.c example doesn't do this either. Is 
        this a bug?

 */
static int enc_render_sequence(va_enc* ctx) {

  VAStatus status = VA_STATUS_SUCCESS;
  VABufferID sps_buf_id = 0;
  
  int r = 0;
  
  if (NULL == ctx) {
    TRAE("Cannot render the SPS as the given `va_enc*` is NULL.");
    return -1;
  }

  if (ctx->settings.gop.log2_max_pic_order_cnt_lsb < 4) {
    TRAE("Cannot render the SPS. The `log2_max_pic_order_cnt_lsb` value of the settings must be >= 4 and <= 16.");
    return -2;
  }

  if (ctx->settings.gop.log2_max_frame_num < 4) {
    TRAE("Cannot render the SPS. The `log2_max_frame_num` value of the settings must be >= 4 and <= 16.");
    return -3;
  }

  if (ctx->settings.image_width != ctx->width_mb_aligned) {
    TRAE("Cannot render the SPS. We do not support cropping (width).");
    return -4;
  }

  if (ctx->settings.image_height != ctx->height_mb_aligned) {
    TRAE("Cannot render the SPS. We do not support cropping (height).");
    return -5;
  }

  ctx->seq_param.seq_parameter_set_id = 0;
  ctx->seq_param.level_idc = ctx->level_idc;
  ctx->seq_param.intra_period = ctx->settings.gop.intra_period;
  ctx->seq_param.intra_idr_period = ctx->settings.gop.idr_period;
  ctx->seq_param.ip_period = ctx->settings.gop.ip_period;

  ctx->seq_param.bits_per_second = 700e3;
  ctx->seq_param.max_num_ref_frames = ctx->settings.num_ref_frames;  
  ctx->seq_param.picture_width_in_mbs = ctx->width_in_mbs;
  ctx->seq_param.picture_height_in_mbs = ctx->height_in_mbs;

  ctx->seq_param.seq_fields.bits.chroma_format_idc = 1;
  ctx->seq_param.seq_fields.bits.frame_mbs_only_flag = 1;
  ctx->seq_param.seq_fields.bits.mb_adaptive_frame_field_flag = 0;
  ctx->seq_param.seq_fields.bits.seq_scaling_matrix_present_flag = 0;
  ctx->seq_param.seq_fields.bits.direct_8x8_inference_flag = 1;
  ctx->seq_param.seq_fields.bits.log2_max_frame_num_minus4 = ctx->settings.gop.log2_max_frame_num - 4;
  ctx->seq_param.seq_fields.bits.pic_order_cnt_type = 0;
  ctx->seq_param.seq_fields.bits.log2_max_pic_order_cnt_lsb_minus4 = ctx->settings.gop.log2_max_pic_order_cnt_lsb - 4;
  ctx->seq_param.seq_fields.bits.delta_pic_order_always_zero_flag = 0;

  /* @todo Currently we only support picture order count type 0. */
  ctx->seq_param.num_ref_frames_in_pic_order_cnt_cycle = 0;
  ctx->seq_param.offset_for_non_ref_pic = 0;
  ctx->seq_param.offset_for_top_to_bottom_field = 0;
  
  /* @todo Currently we don't support cropping. */
  ctx->seq_param.frame_cropping_flag = 0;
  ctx->seq_param.frame_crop_left_offset = 0;
  ctx->seq_param.frame_crop_right_offset = 0;
  ctx->seq_param.frame_crop_top_offset = 0;
  ctx->seq_param.frame_crop_bottom_offset = 0;
  
  ctx->seq_param.num_units_in_tick = 15;
  ctx->seq_param.time_scale = 900;

  /* Create the buffer that represents the SPS */
  status = vaCreateBuffer(
    ctx->va_display,
    ctx->context_id,
    VAEncSequenceParameterBufferType,
    sizeof(ctx->seq_param),
    1,
    &ctx->seq_param,
    &sps_buf_id
  );

  if (VA_STATUS_SUCCESS != status) {
    TRAE("Cannot render the SPS. Failed to create the buffer for it.");
    return -6;
  }

  /* Send the buffer to the server. */
  status = vaRenderPicture(
    ctx->va_display,
    ctx->context_id,
    &sps_buf_id,
    1
  );

  if (VA_STATUS_SUCCESS != status) {
    TRAE("Cannot render the SPS. Failed to send the SPS to the server.");
    r = -7;
    goto error;
  }

  /* @todo remove this log message. Kept here to make sure I don't forget to look into this. */
  {
    static int did_log = 0;
    if (0 == did_log) {
      TRAE("@todo None of the implementations I saw free the sps buffer. Should we? See h264encode.c");
      did_log = 1;
    }
  }

 error:

  /* 
     @todo The documentation in `va.h` about `vaCreateBuffer()` says
     "The user must call vaDestroyBuffer()` to destroy a buffer." though
     the h264encode.c example doesn't destroy the SPS buffer.  Should
     we do destroy it or not?
  */

  return r;
}

/* ------------------------------------------------------- */

/*                                                         
                                                           
   For each picture that we want to encode we have to call this
   `enc_render_picture()` function. Initially I assumed that
   this would generate the PPS, but this is not the case. You
   call this function for every frame and we don't want to add a
   PPS for each frame. The `VAEncPictureParameterBufferH264` does
   however hold the information that is needed to create the PPS.

   When the encoder expects packed headers we have to provide two
   extra buffers and we have to provide them with the same call
   to `vaRenderPicture()`. The two other buffers are:

   - VAEncPackedHeaderParameterBuffer, with type `VAEncPackedHeaderPicture`.
   - VAEncPackedHeaderDataBuffer (holds the data).

  ENCODER RELATED FIELDS:

     Some of the important members of the
     `VAEncPictureParameterBufferH264` type:
     
     CurrPic (VAPictureH264): 

       We have to provide a scratch VA surface ID. After a chat
       with @ceyusa from GStreamer I understand that this is a
       surface into which the encoder writes data that is needs
       to generate a final coded buffer. The surface is buffer
       into which the encoder stores the representation of the
       input frame.  The raw video input frame (yuv) is provide
       via the call to `vaBeginPicture()`. The `h264encode.c`
       example, sets `CurrPic.picture_id` to the ID of a
       reference frame. The coded buffer becomes available after
       calling `vaEndPicture()` (@ceyusa).
     
     ReferenceFrames[16] (VAPictureH264): 

       Array which represents the Decoded Picture
       Buffer. Contains a list of decoded frames used as
       reference. These can later be used as a reference for P
       and B frames.
     
     coded_buf (VABufferID): 

       The buffer ID into which the encoded data will be stored.
     
     VAPictureH264:
     
       - VASurfaceID picture_id: 
       - frame_idx:  used as `frame_num` ? 
       - flags: e.g. top field, bottom field, short term, long term.
       - TopFieldOrderCnt
       - BottomFieldOrderCnt

   PICTURE PARAMETERS:   

    pic_init_qp: 
    
      The documentation says `pic_init_qp_minus26 + 26`, so if we
      want the `pic_init_qp_minus26` to hold a value of 0 we
      should use 26. This is used to control the bitrate.
    
    idr_pic_flag: 
    
      This must be set to 1 when the current frame is an IDR
      frame. When set, we will increment the `idr_pic_id` which is
      used to identify an IDR frame.
    
    reference_pic_flag:
    
      Is set to 1 when the current picture should be a reference
      picture.
    
    entropy_coding_mode_flag:
    
      Wether we use CABAC or CAVLC. This is stored in the context
      member `entropy_mode`.
    
    weighted_pred_flag: 
     
      When equal to 0, this specifies that weighted prediction
      shall not be applied to P and SP
      slices. `weighted_pred_flag` equal to 1 specifies that
      weighted prediction shall be applied to P and SP slices.
    
    weighted_bipred_idc:
    
      When equal to 0 this specifies that the default weighted
      prediction shall be applied to B slices. A value of 1
      specifies that _explicit_ weighted prediction shall be
      appliced to B slices. A value of 2, specifies that implicit
      weighted prediction shall be applied to B slices.
    
    constrained_intra_pred_flag:

      A value equal to 0 specifies that intra prediction allows
      usage of residual data and decoded samples of neighbouring
      macroblocks coded using **Inter** macroblock prediction
      modes for the prediction of macroblocks coded using
      **Intra** macroblock prediction modes. 
    
    transform_8x8_mode_flag:

      A value equal to 1 specifies that the 8x8 transform
      decoding process may be in use (see clause 8.5).
      transform_8x8_mode_flag equal to 0 specifies that the 8x8
      transform decoding process is not in use. When
      transform_8x8_mode_flag is not present, it shall be
      inferred to be 0. 
    
    deblocking_filter_control_present_flag:
  
      A value equal to 1 specifies that a set of syntax elements
      controlling the characteristics of the deblocking filter is
      present in the slice
      header. deblocking_filter_control_present_flag equal to 0
      specifies that the set of syntax elements controlling the
      characteristics of the deblocking filter is not present in
      the slice headers and their inferred values are in effect.
  
    redundant_pic_cnt_present_flag:

      A value equal to 0 specifies that the redundant_pic_cnt
      syntax element is not present in slice headers, coded slice
      data partition B NAL units, and coded slice data partition
      C NAL units that refer (either directly or by association
      with a corresponding coded slice data partition A NAL unit)
      to the picture parameter set.
      redundant_pic_cnt_present_flag equal to 1 specifies that
      the redundant_pic_cnt syntax element is present in all
      slice headers, coded slice data partition B NAL units, and
      coded slice data partition C NAL units that refer (either
      directly or by association with a corresponding coded slice
      data partition A NAL unit) to the picture parameter set.
    
    pic_order_present_flag (spec < 2016)

      A value equal to 1 specifies that the picture order count
      related syntax elements are present in the slice headers as
      specified in subclause 7.3.3. pic_order_present_flag equal
      to 0 specifies that the picture order count related syntax
      elements are not present in the slice headers
    
    pic_scaling_matrix_present_flag:

      A value equal to 1 specifies that the flags
      seq_scaling_list_present_flag[ i ] for i = 0..7 or i =
      0..11 are present. seq_scaling_matrix_present_flag equal to
      0 specifies that these flags are not present and the
      sequencelevel scaling list specified by Flat_4x4_16 shall
      be inferred for i = 0..5 and the sequence-level scaling
      list specified by Flat_8x8_16 shall be inferred for i =
      6..11. When seq_scaling_matrix_present_flag is not present,
      it shall be inferred to be equal to 0.

  TODO:

    @todo I'm currently not 100% sure about the use of the
          `CurrPic`. What should we provide and what logic do we
          need to use. See [this][0] implementation which might
          provide some more background.

    @todo The h264encode.c reference implementation uses always 0 
          for the {pic,sec}_parameter_set_id. I'm following the 
          same approach. 

    @todo I use two indices, `curr_ref_surface_index` and
          `curr_coded_buffer_index` and maybe it's cleaner to use
          something like `current_slot` like h264encode.c; though
          then the buffer sizes must be the same.
  
  REFERENCES:

    [0]: gstvah264enc.c "VAAPI implementation from GStreamer"

 */
static int enc_render_picture(va_enc* ctx) {

  VAStatus status = VA_STATUS_SUCCESS;
  VABufferID pic_param_buf = 0;
  uint32_t num = 0;
  uint32_t i = 0;
  int r = 0;

  /* Validate */
  if (NULL == ctx) {
    TRAE("Cannot render the PPS, gvien `va_enc` is NULL.");
    return -1;
  }

  if (ctx->gop_pic.frame_num > UINT16_MAX) {
    TRAE("The `frame_num` that was calculated by `va_gop` exceeds the value that we can store in `VAEncPictureParameterBufferH264`. ");
    return -2;
  }

  num = sizeof(ctx->pic_param.ReferenceFrames) / sizeof(ctx->pic_param.ReferenceFrames[0]);
  if (16 != num) {
    TRAE("We assume that the `pic_param` stores 16 reference frames. (exiting).");
    exit(EXIT_FAILURE);
  }

  /* Setup the picture params. */
  ctx->pic_param.CurrPic.picture_id = ctx->ref_surfaces[ctx->curr_ref_surface_index];
  ctx->pic_param.CurrPic.frame_idx = ctx->gop_pic.frame_num;
  ctx->pic_param.CurrPic.flags = 0;
  ctx->pic_param.CurrPic.TopFieldOrderCnt = ctx->gop_pic.poc_top_field;
  ctx->pic_param.CurrPic.BottomFieldOrderCnt = ctx->gop_pic.poc_top_field;

  ctx->pic_param.coded_buf = ctx->coded_buffers[ctx->curr_coded_buffer_index];
  ctx->pic_param.pic_parameter_set_id = 0;
  ctx->pic_param.seq_parameter_set_id = 0;
  ctx->pic_param.last_picture = 0;
  ctx->pic_param.frame_num = ctx->gop_pic.frame_num;
  ctx->pic_param.pic_init_qp = 26; /* This will result in a value of 0 in the bit stream, which is mapped to QP step 26 ^.^ */

  ctx->pic_param.num_ref_idx_l0_active_minus1 = 0;
  ctx->pic_param.num_ref_idx_l1_active_minus1 = 0;
  ctx->pic_param.chroma_qp_index_offset = 0;
  ctx->pic_param.second_chroma_qp_index_offset = 0;

  ctx->pic_param.pic_fields.bits.idr_pic_flag = (VA_GOP_FRAME_TYPE_IDR == ctx->gop_pic.frame_type) ? 1 : 0;
  ctx->pic_param.pic_fields.bits.reference_pic_flag = ctx->gop_pic.is_reference;
  ctx->pic_param.pic_fields.bits.entropy_coding_mode_flag = (VA_ENTROPY_MODE_CAVLC == ctx->entropy_mode) ? 0 : 1;
  ctx->pic_param.pic_fields.bits.weighted_pred_flag = 0;
  ctx->pic_param.pic_fields.bits.weighted_bipred_idc = 0;
  ctx->pic_param.pic_fields.bits.constrained_intra_pred_flag = 0;
  ctx->pic_param.pic_fields.bits.transform_8x8_mode_flag = 0;
  ctx->pic_param.pic_fields.bits.deblocking_filter_control_present_flag = 1;
  ctx->pic_param.pic_fields.bits.redundant_pic_cnt_present_flag = 0;
  ctx->pic_param.pic_fields.bits.pic_order_present_flag = 0;
  ctx->pic_param.pic_fields.bits.pic_scaling_matrix_present_flag = 0;
  
  /* Copy our reference frames. */
  memcpy(ctx->pic_param.ReferenceFrames, ctx->ref_frames, ctx->short_ref_count * sizeof(VAPictureH264));

  for (i = ctx->short_ref_count; i < 16; ++i) {
    ctx->pic_param.ReferenceFrames[i].picture_id = VA_INVALID_SURFACE;
    ctx->pic_param.ReferenceFrames[i].flags = VA_PICTURE_H264_INVALID;
  }

  /* Create the picture parameter buffer. */
  status = vaCreateBuffer(
    ctx->va_display,
    ctx->context_id,
    VAEncPictureParameterBufferType,
    sizeof(ctx->pic_param),
    1,
    &ctx->pic_param,
    &pic_param_buf
  );

  if (VA_STATUS_SUCCESS != status) {
    TRAE("Failed to create the buffer for the picture parameter.");
    return -3;
  }

  /* Send the picture parameter to the server. */
  status = vaRenderPicture(ctx->va_display, ctx->context_id, &pic_param_buf, 1);
  if (VA_STATUS_SUCCESS != status) {
    TRAE("Failed to send the picture parameter buffer to the encoder.");
    return -4;
  }

  return r;
}

/* ------------------------------------------------------- */

/*
  
  GENERAL INFO:

    This function will be called once a frame. Currently this
    implementation only supports frame based encoding (as apposed
    to field based).

  SLICE ATTRIBUTES:

    direct_spatial_mv_pref_flag:

      Specifies the method used in the decoding procerss to
      derive motion vectors and reference indices for inter
      prediction. The `va_enc_h264.h` says this should be 1
      for B-slices.

    num_ref_idx_active_override_flag:
   
      A value equal to 1 specifies that the syntax element
      `num_ref_idex_l0_active_minus1` is present for P, SP and B
      slices and that the syntax element
      `ref_idx_l1_active_minus1` is present for B slices. This is
      used to override the active index for this particular
      slice; otherwise the value from the PPS is used.

  TODO:

    @todo We might want to use a lookup table for the slice types.

    @todo Setup the reference lists for B-slices

 */
static int enc_render_slice(va_enc* ctx) {

  VAStatus status = VA_STATUS_SUCCESS;
  VABufferID slice_param_buf = 0;
  uint32_t num_refs = 0;
  uint32_t i = 0;
  int r = 0;
 
  if (NULL == ctx) {
    TRAE("Cannot render a slide, given `va_enc*` is NULL.");
    return -1;
  }

  ctx->slice_param.macroblock_address = 0;
  ctx->slice_param.num_macroblocks = (ctx->width_mb_aligned * ctx->height_mb_aligned) / (16 * 16);
  ctx->slice_param.pic_parameter_set_id = 0;
  ctx->slice_param.idr_pic_id = ctx->gop_pic.idr_id;
  ctx->slice_param.pic_order_cnt_lsb = ctx->gop_pic.poc_lsb;
  ctx->slice_param.num_ref_idx_active_override_flag = 0;
  ctx->slice_param.num_ref_idx_l0_active_minus1 = 0;
  ctx->slice_param.num_ref_idx_l1_active_minus1 = 0;
  ctx->slice_param.delta_pic_order_cnt_bottom = 0;
  ctx->slice_param.direct_spatial_mv_pred_flag = (VA_GOP_FRAME_TYPE_B == ctx->gop_pic.frame_type) ? 1 : 0;
  ctx->slice_param.luma_log2_weight_denom = 0;
  ctx->slice_param.chroma_log2_weight_denom = 0;
  ctx->slice_param.luma_weight_l0_flag = 0;
  ctx->slice_param.chroma_weight_l0_flag = 0;
  ctx->slice_param.luma_weight_l1_flag = 0;
  ctx->slice_param.chroma_weight_l1_flag = 0;
  ctx->slice_param.cabac_init_idc = 0;
  ctx->slice_param.slice_qp_delta = 0;
  ctx->slice_param.disable_deblocking_filter_idc = 0;
  ctx->slice_param.slice_alpha_c0_offset_div2 = 0;
  ctx->slice_param.slice_beta_offset_div2 = 0;

  /* Slice type, see "7.4.3 Slice header semantics" */
  switch (ctx->gop_pic.frame_type) {
    case VA_GOP_FRAME_TYPE_P: {
      ctx->slice_param.slice_type = 0;
      break;
    }
    case VA_GOP_FRAME_TYPE_I: {
      ctx->slice_param.slice_type = 2;
      break;
    }
    case VA_GOP_FRAME_TYPE_IDR: {
      ctx->slice_param.slice_type = 2;
      break;
    }
    case VA_GOP_FRAME_TYPE_B: {
      ctx->slice_param.slice_type = 1;
      break;
    }
    default: {
      TRAE("Cannot set the slice type; unhandled frame_type: %u.", ctx->gop_pic.frame_type);
      return -2;
    }
  }

  /* Copy the reference picture list for P slices. */
  if (VA_GOP_FRAME_TYPE_P == ctx->gop_pic.frame_type) {
    
    r = enc_update_reference_lists(ctx);
    if (r < 0) {
      TRAE("Failed to update the reference picture list.");
      return -3;
    }

    {
      static uint8_t did_log = 0;
      if (0 == did_log) {
        TRAE("@todo I think we should be OK by only copying the `short_ref_count` number of entries from the `ref_list0`.");
        did_log = 1;
      }
    }

    /*
      Make sure we clamp the amount of refs we copy, because
      maybe the `ref_list0_max` that we got from the driver is
      larger than 32 and we only keep 32.
    */
    num_refs = (ctx->ref_list0_max > 32) ? 32 : ctx->ref_list0_max;
    memcpy(ctx->slice_param.RefPicList0, ctx->ref_list0, num_refs * sizeof(VAPictureH264));

    /* Mark all other references as invalid. */
    for (i = ctx->ref_list0_max; i < 32; ++i) {
      ctx->slice_param.RefPicList0[i].picture_id = VA_INVALID_SURFACE;
      ctx->slice_param.RefPicList0[i].flags = VA_PICTURE_H264_INVALID;
    }
  }

  /* Create the slice buffer. */
  status = vaCreateBuffer(
    ctx->va_display,
    ctx->context_id,
    VAEncSliceParameterBufferType,
    sizeof(ctx->slice_param),
    1,
    &ctx->slice_param,
    &slice_param_buf
  );

  if (VA_STATUS_SUCCESS != status) {
    TRAE("Failed to create the slice parameter buffer.");
    return -4;
  }

  /* Send the buffer to the server. */
  status = vaRenderPicture(ctx->va_display, ctx->context_id, &slice_param_buf, 1);
  if (VA_STATUS_SUCCESS != status) {
    TRAE("Failed to render the slice parameter buffer.");
    return -5;
  }
  
 error:

  return r;
}

/* ------------------------------------------------------- */

static int enc_render_packed_sequence(va_enc* ctx) {
  
  VAEncPackedHeaderParameterBuffer para_buf = {};
  VAEncSequenceParameterBufferH264* sps = NULL;
  VABufferID para_buf_id = 0;
  VABufferID sps_buf_id = 0;
  VABufferID render_ids[2] = {};
  VAStatus status = VA_STATUS_SUCCESS;
  int r = 0;

  /* -------------------------------------------- */
  /* STEP 0: Validate                             */
  /* -------------------------------------------- */

  if (NULL == ctx) {
    TRAE("Cannot render the packed sequence, because the given `va_enc*` is NULL.");
    return -1;
  }

  if (VAProfileH264ConstrainedBaseline != ctx->profile) {
    TRAE("Cannot render the packed sequence, we only support the baseline profile for now!");
    return -2;
  }

  if (NULL == ctx->bitstream) {
    TRAE("Cannot render the packed sequence, the `va_enc::bitstream` member is NULL. Not initialized?");
    return -3;
  }

  sps = &ctx->seq_param;

  /* Currently we can only write an SPS that validates the following checks. */
  if (0 != sps->seq_fields.bits.pic_order_cnt_type) {
    TRAE("Cannot render the packed sequence, we only support pic_order_cnt_type 0.");
    return -5;
  }
  
  if (0 == sps->seq_fields.bits.frame_mbs_only_flag) {
    TRAE("Cannot render the packed sequence, we only support `frame_mbs_only_flag == 1`.");
    return -6;
  }

  if (0 != sps->frame_cropping_flag) {
    TRAE("Cannot render the packed sequence; we only support `frame_cropping_flag == 0`. ");
    return -7;
  }

  /* Make sure we start with a fresh bitstream; e.g. all bits are set to 0x00 */
  r = tra_golomb_writer_reset(ctx->bitstream);
  if (r < 0) {
    TRAE("Failed to reset the bitstream.");
    return -4;
  }

  /* -------------------------------------------- */
  /* STEP 1: Generate the SPS bitstream           */
  /* -------------------------------------------- */

  /* Add the annex-b prefix and nal header. */
  tra_h264_write_annexb_header(ctx->bitstream);
  tra_h264_write_nal_header(ctx->bitstream, TRA_NAL_REF_IDC_HIGH, 7); 

  /* Write the SPS. */
  tra_golomb_write_bits(ctx->bitstream, 66, 8);                                                /* profile_idc, 66 = baseline */
  tra_golomb_write_bit(ctx->bitstream, 1);                                                     /* constrained_set0_flag */ 
  tra_golomb_write_bit(ctx->bitstream, 1);                                                     /* constrained_set1_flag */
  tra_golomb_write_bit(ctx->bitstream, 0);                                                     /* constrained_set2_flag */
  tra_golomb_write_bit(ctx->bitstream, 0);                                                     /* constrained_set3_flag */
  tra_golomb_write_bits(ctx->bitstream, 0, 4);                                                 /* reserved_zero_bits */
  tra_golomb_write_bits(ctx->bitstream, sps->level_idc, 8);                                    /* level_idc */
  tra_golomb_write_ue(ctx->bitstream, sps->seq_parameter_set_id);                              /* seq_parameter_set_id */
  tra_golomb_write_ue(ctx->bitstream, sps->seq_fields.bits.log2_max_frame_num_minus4);         /* log2_max_frame_num_minus4 */
  tra_golomb_write_ue(ctx->bitstream, sps->seq_fields.bits.pic_order_cnt_type);                /* pic_order_cnt_type */
  tra_golomb_write_ue(ctx->bitstream, sps->seq_fields.bits.log2_max_pic_order_cnt_lsb_minus4); /* log2_max_pic_order_cnt_lsb_minus4 */
  tra_golomb_write_ue(ctx->bitstream, sps->max_num_ref_frames);                                /* num_ref_frames */
  tra_golomb_write_bit(ctx->bitstream, 0);                                                     /* gaps_in_frame_num_value_allowed_flag. */
  tra_golomb_write_ue(ctx->bitstream, sps->picture_width_in_mbs - 1);                          /* pic_width_in_mbs_minus1 */
  tra_golomb_write_ue(ctx->bitstream, sps->picture_height_in_mbs - 1);                         /* pic_height_in_mbs_minus1 */
  tra_golomb_write_bit(ctx->bitstream, sps->seq_fields.bits.frame_mbs_only_flag);              /* frame_mbs_only_flag */
  tra_golomb_write_bit(ctx->bitstream, sps->seq_fields.bits.direct_8x8_inference_flag);        /* direct_8x8_inference_flag */
  tra_golomb_write_bit(ctx->bitstream, sps->frame_cropping_flag);                              /* frame_cropping_flag */
  tra_golomb_write_bit(ctx->bitstream, 0);                                                     /* vui_parameters_present_flag */

  /* And the trailing bits to make sure we're on a byte boundary. */
  tra_h264_write_trailing_bits(ctx->bitstream);

  /* -------------------------------------------- */
  /* STEP 2: create parameter buffers             */
  /* -------------------------------------------- */

  para_buf.type = VAEncPackedHeaderSequence;
  para_buf.bit_length = ctx->bitstream->byte_offset * 8;
  para_buf.has_emulation_bytes = 0;

  status = vaCreateBuffer(
    ctx->va_display,
    ctx->context_id,
    VAEncPackedHeaderParameterBufferType,
    sizeof(para_buf),
    1,
    &para_buf,
    &para_buf_id
  );

  if (VA_STATUS_SUCCESS != status) {
    TRAE("Failed to create a buffer for the packed-header-parameter. ");
    r = -5;
    goto error;
  }

  status = vaCreateBuffer(
    ctx->va_display,
    ctx->context_id,
    VAEncPackedHeaderDataBufferType,
    ctx->bitstream->byte_offset,
    1, 
    ctx->bitstream->data,
    &sps_buf_id
  );

  if (VA_STATUS_SUCCESS != status) {
    TRAE("Failed to create the buffer for the SPS data.");
    r = -6;
    goto error;
  }

  /* -------------------------------------------- */
  /* STEP 3: render the buffers into the stream   */
  /* -------------------------------------------- */
  
  render_ids[0] = para_buf_id;
  render_ids[1] = sps_buf_id;
  
  status = vaRenderPicture(
    ctx->va_display,
    ctx->context_id,
    render_ids,
    2
  );

  if (VA_STATUS_SUCCESS != status) {
    TRAE("Failed to render the SPS header/data. (exiting).");
    r = -7;
    goto error;
  }

  //tra_golomb_save_to_file(ctx->bitstream, "./sps.raw");

 error:

  return r;
}

/* ------------------------------------------------------- */

/*

  GENERAL INFO:

    This function will generate the bitstream for the PPS.

  TODO:

    @todo The `h264encode.c` reference implementation writes a 
          couple of more attributes into the PPS than this function
          does.

 */
static int enc_render_packed_picture(va_enc* ctx) {

  VAEncPackedHeaderParameterBuffer para_buf = {};
  VAEncPictureParameterBufferH264* pps = NULL;
  VAStatus status = VA_STATUS_SUCCESS;
  VABufferID render_ids[2] = {};
  VABufferID para_id = 0;
  VABufferID data_id = 0;
  int r = 0;

  /* -------------------------------------------- */
  /* STEP 0: Validate                             */
  /* -------------------------------------------- */
  
  if (NULL == ctx) {
    TRAE("Cannot render the packed picture, because the given `va_enc*` is NULL.");
    return -1;
  }

  if (NULL == ctx->bitstream) {
    TRAE("Cannot render the packed picture, because the `ctx->bitstream` is NULL.");
    return -2;
  }
  
  r = tra_golomb_writer_reset(ctx->bitstream);
  if (r < 0) {
    TRAE("Cannot render the packed picture, failed to reset the bitstream.");
    return -3;
  }

  /* -------------------------------------------- */
  /* STEP 1: Generate the PPS bitstream           */
  /* -------------------------------------------- */

  pps = &ctx->pic_param;
  
  /* Write the annex-b prefix and nal header. */
  tra_h264_write_annexb_header(ctx->bitstream);
  tra_h264_write_nal_header(ctx->bitstream, TRA_NAL_REF_IDC_HIGH, 8); 

  /* Write the PPS */
  tra_golomb_write_ue(ctx->bitstream, pps->pic_parameter_set_id);                                    /* pic_parameter_set_id */
  tra_golomb_write_ue(ctx->bitstream, pps->seq_parameter_set_id);                                    /* seq_parameter_set_id */
  tra_golomb_write_bit(ctx->bitstream, ctx->entropy_mode);                                           /* entropy_coding_mode_flag, @todo currently this is hardcoded to 1, CABAC */
  tra_golomb_write_bit(ctx->bitstream, 0);                                                           /* pic_order_present_flag: no extra syntax elements. */
  tra_golomb_write_ue(ctx->bitstream, 0);                                                            /* num_slice_groups_minus1: not using slice groups. */
  tra_golomb_write_ue(ctx->bitstream, pps->num_ref_idx_l0_active_minus1);                            /* num_ref_idx_l0_active_minus1 */
  tra_golomb_write_ue(ctx->bitstream, pps->num_ref_idx_l1_active_minus1);                            /* num_ref_idx_l1_active_minus1 */
  tra_golomb_write_u(ctx->bitstream, pps->pic_fields.bits.weighted_pred_flag, 1);                    /* weighted_pred_flag */
  tra_golomb_write_u(ctx->bitstream, pps->pic_fields.bits.weighted_bipred_idc, 2);                   /* weighted_bipred_idc */
  tra_golomb_write_se(ctx->bitstream, pps->pic_init_qp - 26);                                        /* pic_init_qp_minus26 */
  tra_golomb_write_se(ctx->bitstream, 0);                                                            /* pic_init_qs_minus26 */
  tra_golomb_write_se(ctx->bitstream, 0);                                                            /* chroma_qp_index_offset */
  tra_golomb_write_bit(ctx->bitstream, pps->pic_fields.bits.deblocking_filter_control_present_flag); /* deblocking_filter_control_present_flag */
  tra_golomb_write_bit(ctx->bitstream, 0);                                                           /* constrained_intra_pred_flag */
  tra_golomb_write_bit(ctx->bitstream, 0);                                                           /* redundant_pic_cnt_present_flag */
 
  /* Add the trailing bits to make sure we're on a byte boundary. */
  tra_h264_write_trailing_bits(ctx->bitstream);

  //tra_golomb_save_to_file(ctx->bitstream, "pps.raw");

  /* ------------------------------------------------ */
  /* STEP 2: create parameter buffers (header + data) */
  /* ------------------------------------------------- */

  para_buf.type = VAEncPackedHeaderPicture;
  para_buf.bit_length = ctx->bitstream->byte_offset * 8;
  para_buf.has_emulation_bytes = 0;

  status = vaCreateBuffer(
    ctx->va_display,
    ctx->context_id,
    VAEncPackedHeaderParameterBufferType,
    sizeof(para_buf),
    1,
    &para_buf, &para_id
  );

  if (VA_STATUS_SUCCESS != status) {
    TRAE("Failed to create the buffer for the PPS header parameter.");
    r = -4;
    goto error;
  }

  status = vaCreateBuffer(
    ctx->va_display,
    ctx->context_id,
    VAEncPackedHeaderDataBufferType,
    ctx->bitstream->byte_offset,
    1,
    ctx->bitstream->data,
    &data_id
  );

  if (VA_STATUS_SUCCESS != status) {
    TRAE("Failed to create the buffer for the PPS.");
    r = -5;
    goto error;
  }

  render_ids[0] = para_id;
  render_ids[1] = data_id;

  status = vaRenderPicture(ctx->va_display, ctx->context_id, render_ids, 2);
  if (VA_STATUS_SUCCESS != status) {
    TRAE("Failed to render the PPS into the bitstream.");
    r = -6;
    goto error;
  }

 error:

  return r;
}

/* ------------------------------------------------------- */

/*

  GENERAL INFO:

    This function will generate the bitstream for the slice
    header. I've added the description of a couple of interesting
    slice header fields below. 

  TODO:

    @todo This function will currently only generate the correct
          slice header bitstream when we're not using
          B-frames/slices.

  FIELDS:
  
  frame_num:
   
    This is used as an identifier for pictures and shall be
    represented by `log2_max_frame_num_minus4 + 4` bits in the
    bitstream. The requirements for `frame_num` are described in
    detail in the spec. The `frame_num` is managed by `va_gop`;
    and only incremented after each reference frame.

  no_output_of_prior_pics_flag: 

    This specifies how the previously-decoded pictures in the
    decoded picture buffer are treated after decoding of an IDR
    picture (see spec for more details).

  long_term_reference_flag:

    A value equal to 0 specifies that the _MaxLongTermFrameIdx_
    variable is set equal to "no long-term frame indices" and
    that the IDR picture is marked as "use for short-term
    reference". `long_term_reference_flag` equal to 1 specifies
    that the _MaxLongTermFrameIdx_ variable is set equal to 0 and
    tha tthe current IDR picture is marked "used for long-term
    reference" and is assigned _LongTermFrameIdx_ equal to 0.

  adaptive_ref_pic_marking_mode_flag:

    Selects the reference picture marking mode of the currently
    decoded picture. See spec for more details, but a value of 0
    means we're using a sliding window, first-in/first-out
    mechonism.

  slice_qp_delta:

    Specifies the initial value for QPy to be used for all
    macroblocks in the slice until modified by the value of
    `mp_qp_delta` in the macroblock layer. The initial QPy
    quantisation parameter for the slice is computed as (see
    spec).


 */
static int enc_render_packed_slice(va_enc* ctx) {
  
  VAEncSliceParameterBufferH264* slice = NULL;
  VAEncPictureParameterBufferH264* pp = NULL;
  tra_golomb_writer* bs = NULL;
  int r = 0;

  /* -------------------------------------------- */
  /* STEP 0: Validate and reset                   */
  /* -------------------------------------------- */

  if (NULL == ctx) {
    TRAE("Cannot render the packed slice, because the given `va_enc*` is NULL.");
    return -1;
  }

  if (NULL == ctx->bitstream) {
    TRAE("Cannot render the packed slice, the the `bitstream` member is NULL. Forgot to initialize maybe?");
    return -2;
  }

  if (1 != ctx->seq_param.seq_fields.bits.frame_mbs_only_flag) {
    TRAE("Cannot render the packed slice; the `frame_mbs_only_flag` is set not 1; we only support `frame_mbs_only_flag = 1`. ");
    return -3;
  }

  if (0 != ctx->seq_param.seq_fields.bits.pic_order_cnt_type) {
    TRAE("Cannot render the packed slice; the pic_order_cnt_type is not 0. We only support type 0.");
    return -4;
  }

  /* ... we already check this in `va_enc_create()`; though this won't hurt. */
  if (ctx->settings.gop.log2_max_frame_num < 4) {
    TRAE("Cannot render the packed slice. The `log2_max_frame_num` setting is < 4. This should be between 4 and 16, inclusive.");
    return -5;
  }

  if (ctx->settings.gop.log2_max_pic_order_cnt_lsb < 4) {
    TRAE("Cannot render the packed slice. The `log2_max_pic_order_cnt` setting is < 4. This should be between 4 and 16, inclusive.");
    return -6;
  }

  if (0 != ctx->slice_param.num_ref_idx_active_override_flag) {
    TRAE("Cannot render the packed slice, `num_ref_idx_active_override_flag` must be 0; we don't support overrides yet.");
    return -7;
  }

  /* -------------------------------------------- */
  /* STEP 1: Generate the bitstream               */
  /* -------------------------------------------- */

  /* Make sure we start with a fresh bitstream; e.g. all bits are set to 0x00 */
  r = tra_golomb_writer_reset(ctx->bitstream);
  if (r < 0) {
    TRAE("Failed to reset the bitstream.");
    return -6;
  }

  /* Using shorter names to improve readability */
  slice = &ctx->slice_param;
  pp = &ctx->pic_param;
  bs= ctx->bitstream;

  /* Write the slice header. */
  tra_golomb_write_ue(bs, 0);                                                                    /* first_mb_in_slice */
  tra_golomb_write_ue(bs, slice->slice_type);                                                    /* slice_type */
  tra_golomb_write_ue(bs, slice->pic_parameter_set_id);                                          /* pic_parameter_set_id */
  tra_golomb_write_u(bs, ctx->gop_pic.frame_num, ctx->settings.gop.log2_max_frame_num);               /* frame_num */

  if (0 != pp->pic_fields.bits.idr_pic_flag) {
    tra_golomb_write_ue(bs, slice->idr_pic_id);                                                  /* idr_pic_idr */
  }
  
  tra_golomb_write_u(bs, pp->CurrPic.TopFieldOrderCnt, ctx->settings.gop.log2_max_pic_order_cnt_lsb); /* pic_order_cnt_lsb */

  if (1 == enc_is_slice_p(slice->slice_type)) {
    tra_golomb_write_bit(bs, slice->num_ref_idx_active_override_flag);                           /* num_ref_idx_active_override_flag */
    tra_golomb_write_bit(bs, 0);                                                                 /* ref_pic_list_reordering_flag_l0 */
  }

  /* dec_ref_pic_marking() */
  if (1 == pp->pic_fields.bits.reference_pic_flag) {             
    if (0 != pp->pic_fields.bits.idr_pic_flag) {
      tra_golomb_write_bit(bs, 0);                                                               /* no_output_of_prior_pics_flags */
      tra_golomb_write_bit(bs, 0);                                                               /* long_term_reference_flag */
    }
    else {
      tra_golomb_write_bit(bs, 0);                                                               /* adaptive_ref_pic_marking_mode_flag */
    }
  }

  /* entropy coding mode */
  if (pp->pic_fields.bits.entropy_coding_mode_flag &&
      1 != enc_is_slice_i(slice->slice_type))
    {
      tra_golomb_write_ue(bs, slice->cabac_init_idc);                                            /* cabac_init_idc */
    }

  tra_golomb_write_se(bs, slice->slice_qp_delta);                                                /* slice_qp_delta */

  if (1 == pp->pic_fields.bits.deblocking_filter_control_present_flag) {                             

    tra_golomb_write_ue(bs, slice->disable_deblocking_filter_idc);
    
    if (1 != slice->disable_deblocking_filter_idc) {
      tra_golomb_write_se(bs, slice->slice_alpha_c0_offset_div2);                                /* slice_alpha_c0_offset_div2: 2 */
      tra_golomb_write_se(bs, slice->slice_beta_offset_div2);                                    /* slice_beta_offset_div2: 2 */
    }
  }

 error:

  return r;
}

/* ------------------------------------------------------- */

/*
  Adds the given `VAPictureH264` to the `ref_frames` and applies
  a first-in/first-out logic: the newest picture is stored into
  index 0, the oldest at `va_enc::short_ref_count`. The goal of
  this function is to add the `VAPictureH264` (as stored in
  `pic_param) when the `gop_pic.is_reference = 1`. Note that this
  function uses the current picture, see `va_enc_encode()` where
  we call `va_gop_get_next_picture()`.

  The `ref_frames` are later copied into the
  `VAEncPictureParameterBufferH264::ReferenceFrames` member
  (pic_param). When we copy the reference frames there are some
  rules regarding the sort method, order and short/long term
  reference frames.

  The appraoch how we deal with reference frames is similar to
  the `h264encode.c` implementation from the [libva-utils][0]
  repository. We manage the reference frames as follows:
  
  - We store every `VAPictureH264` into `va_enc::ref_frames` when
    our _gop manager_ (va_gop), has marked the current
    `va_gop_picture` as a reference frame. This flag may be set
    by a call to `va_gop_get_next_picture()`. This function
    checks what kind of frame we should generate (IDR, I, P, B)
    and sets the `is_reference = 1` when the current picture
    should be used as reference frame.

  - After we have stored a reference frame, and we have this
    collection of reference frames, we use them to create the
    list0 and/or list1 according to the rules of the spec. This
    means that list0 (used by P-frames) is sorted by _PicNum_
    (`frame_num % max_num_frames`) (encode/decode order) and
    list1 (used by B-frames) by the picture order count. We
    create the reference lists with the function
    `enc_update_reference_lists()`.

  REFERENCES:

    [0]: https://github.com/intel/libva-utils/blob/master/encode/h264encode.c

*/
static int enc_update_reference_frames(va_enc* ctx) {

  uint32_t i = 0;
  
  if (NULL == ctx) {
    TRAE("Cannot store the given `VAPictureH264` as a reference frame as the given `va_enc*` is NULL.");
    return -1;
  }

  /* Current picture is not a reference picture so we can skip this. */
  if (0 == ctx->gop_pic.is_reference) {
    return 0;
  }

  /* Make sure the number of short refs does not exceed the max ref frames. */
  ctx->short_ref_count++;
  if (ctx->short_ref_count > ctx->settings.num_ref_frames) {
    ctx->short_ref_count = ctx->settings.num_ref_frames;
  }

  /* Move "older" frames down (fifo), then store the newest at index 0. */
  for (i = ctx->short_ref_count - 1; i > 0; i--) {
    ctx->ref_frames[i] = ctx->ref_frames[i - 1];
  }

  /* Store the current picture as a reference frame and make sure it is marked as such. */
  ctx->ref_frames[0] = ctx->pic_param.CurrPic;

  {
    static uint8_t did_log = 0;
    if (0 == did_log) {
      TRAE("@todo In `enc_update_reference_frames()` I'm setting the current picture flag; Maybe I should do this before calling the function?");
      did_log = 1;
    }
  }

  ctx->pic_param.CurrPic.flags = VA_PICTURE_H264_SHORT_TERM_REFERENCE;
  
  return 0;
}

/* ------------------------------------------------------- */

/*

  GENERAL INFO:

    This function will update the list0 and list1 reference lists
    for the current picture (gop_pic). The list0 holds the
    reference frames as used by P and B frames. List1 is only
    used by B-frames. Also note that we have to sort frames
    differently based on their type (P or B).

  TODO:

    @todo Add support for B frames. 

 */
static int enc_update_reference_lists(va_enc* ctx) {

  if (NULL ==  ctx) {
    TRAE("Cannot update the reference lists; given `va_enc*` is NULL.");
    return -1;
  }

  if (VA_GOP_FRAME_TYPE_B == ctx->gop_pic.frame_type) {
    TRAE("@todo we have to add support for B frames. We are not updating the list0 and list1 for B frames atm. (exiting).");
    exit(EXIT_FAILURE);
  }

  /* We only need to sort when the current actually uses reference frames. */
  if (VA_GOP_FRAME_TYPE_P != ctx->gop_pic.frame_type) {
    return 0;
  }

  memcpy(ctx->ref_list0, ctx->ref_frames, ctx->short_ref_count * sizeof(VAPictureH264));

  qsort(
    ctx->ref_list0,
    ctx->short_ref_count,
    sizeof(VAPictureH264),
    va_util_compare_h264picture_frame_num
  );

  return 0;
}
    
/* ------------------------------------------------------- */

/* 

   GENERAL INFO:

    This function will create the encoder context and initializes
    the required source and destination buffers. This function is
    called from `va_enc_create()` and partially based on
    setup_encode()` from the libva-utils repository.
     
   REFERENCES:

     [0]: https://www.yumpu.com/en/document/read/47074087/hardware-accelerated-h264-video-encoding-using-vaapi-on-the- "Hardware Accelerated H.264 Video Encoding using VAAPI on the Intel Atam Processor E6xx Series"
 
 */
static int enc_init_encode(va_enc* ctx) {
  
  VAStatus status = VA_STATUS_SUCCESS;
  VASurfaceID* surfaces = NULL;
  uint32_t coded_buffer_size = 0;
  int r = 0;
  int i = 0;
  int num = 0;

  /* --------------------------------------- */
  /* VALIDATE                                */
  /* --------------------------------------- */
  
  if (NULL == ctx) {
    TRAE("Cannot initialize the encoder; given `va_enc*` is NULL.");
    return -1;
  }
  
  if (NULL != ctx->src_surfaces) {
    TRAE("Cannot initialize the encode session; seems like we've already created the source surfaces.");
    return -2;
  }

  if (NULL == ctx->va_display) {
    TRAE("Cannot initialize the encoder; the `va_display` is NULL.");
    return -3;
  }

  if (0 == ctx->width_mb_aligned) {
    TRAE("Cannot initialize the encoder `width_mb_aligned` is 0.");
    return -4;
  }

  if (0 == ctx->height_mb_aligned) {
    TRAE("Cannot initialize the encoder `height_mb_aligned` is 0.");
    return -5;
  }

  /* --------------------------------------- */
  /* SURFACES                                */
  /* --------------------------------------- */

  /* The number of source and reference surfaces we create. */
  ctx->num_coded_buffers = 16;
  ctx->num_src_surfaces = 16;
  ctx->num_ref_surfaces = 16;
  ctx->curr_coded_buffer_index = 0;
  ctx->curr_ref_surface_index = 0;
  ctx->curr_coded_buffer_index = 0;
  
  /* Allocate the source surface array */
  ctx->src_surfaces = calloc(ctx->num_src_surfaces, sizeof(VASurfaceID));
  if (NULL == ctx->src_surfaces) {
    TRAE("Failed to allocate the source surfaces.");
    return -6;
  }
  
  status = vaCreateSurfaces(
    ctx->va_display,
    ctx->sampling_format,
    ctx->width_mb_aligned,
    ctx->height_mb_aligned,
    ctx->src_surfaces,
    ctx->num_src_surfaces,
    NULL,
    0
  );

  if (VA_STATUS_SUCCESS != status) {
    TRAE("Failed to create the source surfaces.");
    return -7;
  }

  /* Allocate the reference surface array. */
  ctx->ref_surfaces = calloc(ctx->num_ref_surfaces, sizeof(VASurfaceID));
  if (NULL == ctx->ref_surfaces) {
    TRAE("Failed to allocate the reference surfaces.");
    return -8;
  }

  status = vaCreateSurfaces(
    ctx->va_display,
    ctx->sampling_format,
    ctx->width_mb_aligned,
    ctx->height_mb_aligned,
    ctx->ref_surfaces,
    ctx->num_ref_surfaces,
    NULL,
    0
  );

  if (VA_STATUS_SUCCESS != status) {
    TRAE("Failed to create the reference surfaces.");
    return -9;
  }

  /* 
     When we create the context, we pass an array of surface IDs
     that we're going to use. These surface IDs act as a hint to
     the context. We allocate an array large enough to hold the
     `ref_surfaces` and `src_surfaces`.
  */
  surfaces = calloc(ctx->num_src_surfaces + ctx->num_ref_surfaces, sizeof(VASurfaceID));
  if (NULL == surfaces) {
    TRAE("Failed to allocate the array for our surface IDs hint for the context.");
    return -8;
  }

  memcpy(surfaces, ctx->src_surfaces, ctx->num_src_surfaces * sizeof(VASurfaceID));
  memcpy(surfaces + ctx->num_src_surfaces, ctx->ref_surfaces, ctx->num_ref_surfaces * sizeof(VASurfaceID));

  /* --------------------------------------- */
  /* CONTEXT                                 */
  /* --------------------------------------- */
  
  /* Create the context. */
  status = vaCreateContext(
    ctx->va_display,
    ctx->config_id,
    ctx->width_mb_aligned,
    ctx->height_mb_aligned,
    VA_PROGRESSIVE,
    surfaces,
    (ctx->num_src_surfaces + ctx->num_ref_surfaces),
    &ctx->context_id
  );

  if (VA_STATUS_SUCCESS != status) {
    TRAE("Failed to create the context.");
    r = -9;
    goto error;
  }

  /* --------------------------------------- */
  /* CODED BUFFERS                           */
  /* --------------------------------------- */
  
  /* Create the coded buffers. */
  ctx->coded_buffers = calloc(ctx->num_coded_buffers, sizeof(VABufferID));
  if (NULL == ctx->coded_buffers) {
    TRAE("Failed to allocate the buffer for the coded buffer.");
    r = -10;
    goto error;
  }

  /* 
     Here we calculate/approximate the size for our coded buffers. This
     is based on the `h264encode.c` example from the libva-utils repository
     and is also mentioned in [this article][0].
  */
  coded_buffer_size = (ctx->width_mb_aligned * ctx->height_mb_aligned * 400) / (16 * 16);
  TRAD("coded_buffer_size: %u", coded_buffer_size);

  for (i = 0; i < ctx->num_src_surfaces; ++i) {

    status = vaCreateBuffer(
      ctx->va_display,
      ctx->context_id,
      VAEncCodedBufferType,
      coded_buffer_size,
      1,
      NULL,
      ctx->coded_buffers + i
    );

    if (VA_STATUS_SUCCESS != status) {
      TRAE("Failed to create the buffer for the coded data.");
      r = -10;
      goto error;
    }
  }
  
 error:

  if (NULL != surfaces) {
    free(surfaces);
    surfaces = NULL;
  }

  return r;
}

/* ------------------------------------------------------- */

/* Returns 1 if the given slice type represents a P slice, otherwise 0. */
static uint8_t enc_is_slice_p(uint8_t sliceType) {
  return (sliceType == 0) ? 1 : 0;
}

/* ------------------------------------------------------- */

/* Returns 1 if the given slice type represents a B slice, otherwise 0. */
static uint8_t enc_is_slice_b(uint8_t sliceType) {
  return (sliceType == 1) ? 1 : 0;
}

/* ------------------------------------------------------- */

/* Returns 1 if the given slice type represents a I slice, otherwise 0. */
static uint8_t enc_is_slice_i(uint8_t sliceType) {
  return (sliceType == 2) ? 1 : 0;
}

/* ------------------------------------------------------- */

int va_enc_print(va_enc* ctx) {

  if (NULL == ctx) {
    TRAE("Cannot print the `va_enc` as it's NULL.");
    return -1;
  }

  TRAD("va_enc");
  TRAD("  gop: %p", ctx->gop);
  TRAD("  settings.image_width: %u", ctx->settings.image_width);
  TRAD("  settings.image_height: %u", ctx->settings.image_height);
  TRAD("  settings.gop.idr_period: %u", ctx->settings.gop.idr_period);
  TRAD("  settings.gop.intra_period: %u", ctx->settings.gop.intra_period);
  TRAD("  settings.gop.ip_period: %u", ctx->settings.gop.ip_period);
  TRAD("  settings.gop.log2_max_frame_num: %u", ctx->settings.gop.log2_max_frame_num);
  TRAD("  settings.gop.log2_max_pic_order_cnt_lsb: %u", ctx->settings.gop.log2_max_pic_order_cnt_lsb);
  TRAD("  xorg_display: %p", ctx->xorg_display);
  TRAD("  va_display: %p", ctx->va_display);
  TRAD("  config_id: %u", ctx->config_id);
  TRAD("  context_id: %u", ctx->context_id);
  TRAD("  profile: %s", enc_profile_to_string(ctx->profile));
  TRAD("  sampling_format: %s", enc_sampling_format_to_string(ctx->sampling_format));
  TRAD("  ver_minor: %d", ctx->ver_minor);
  TRAD("  ver_major: %d", ctx->ver_major);
  TRAD("  width_mb_aligned: %u", ctx->width_mb_aligned);
  TRAD("  height_mb_aligned: %u", ctx->height_mb_aligned);
  TRAD("  width_in_mbs: %u", ctx->width_in_mbs);
  TRAD("  height_in_mbs: %u", ctx->height_in_mbs);
  TRAD("  entropy_mode: %s", enc_entropymode_to_string(ctx->entropy_mode));
  TRAD("  is_using_packed_headers: %u", ctx->is_using_packed_headers);
  TRAD("  constraint_set_flag: %02x", ctx->constraint_set_flag);
  TRAD("  coded_buffers: %p", ctx->coded_buffers);
  TRAD("  src_surfaces: %p", ctx->src_surfaces);
  TRAD("  ref_surfaces: %p", ctx->ref_surfaces);
  TRAD("  num_src_surfaces: %u", ctx->num_src_surfaces);
  TRAD("  num_ref_surfaces: %u", ctx->num_ref_surfaces);
  TRAD("  ref_list0_max: %u", ctx->ref_list0_max);
  TRAD("  ref_list1_max: %u", ctx->ref_list1_max);
  TRAD("");

  return 0;
}

/* ------------------------------------------------------- */

static const char* enc_entropymode_to_string(uint8_t mode) {

  switch (mode) {
    case VA_ENTROPY_MODE_CABAC: { return "VA_ENTROPY_MODE_CABAC"; }
    case VA_ENTROPY_MODE_CAVLC: { return "VA_ENTROPY_MODE_CAVLC"; }
    default:                    { return "UNKNOWN";               }
  }
}

/* ------------------------------------------------------- */

static const char* enc_profile_to_string(VAProfile profile) {

  switch (profile) {
    case VAProfileH264High:                { return "VAProfileH264High";                }
    case VAProfileH264Main:                { return "VAProfileH264Main";                }
    case VAProfileH264ConstrainedBaseline: { return "VAProfileH264ConstrainedBaseline"; }
    default:                               { return "UNKNOWN";                          }
  }
}

/* ------------------------------------------------------- */

static const char* enc_sampling_format_to_string(uint32_t fmt) {

  switch (fmt) {
    case VA_RT_FORMAT_YUV420:    { return "VA_RT_FORMAT_YUV420";    }
    case VA_RT_FORMAT_YUV422:    { return "VA_RT_FORMAT_YUV422";    }
    case VA_RT_FORMAT_YUV444:    { return "VA_RT_FORMAT_YUV444";    }
    case VA_RT_FORMAT_YUV411:    { return "VA_RT_FORMAT_YUV411";    }
    case VA_RT_FORMAT_YUV400:    { return "VA_RT_FORMAT_YUV400";    }
    case VA_RT_FORMAT_YUV420_10: { return "VA_RT_FORMAT_YUV420_10"; }
    case VA_RT_FORMAT_YUV422_10: { return "VA_RT_FORMAT_YUV422_10"; }
    case VA_RT_FORMAT_YUV444_10: { return "VA_RT_FORMAT_YUV444_10"; }
    case VA_RT_FORMAT_YUV420_12: { return "VA_RT_FORMAT_YUV420_12"; }
    case VA_RT_FORMAT_YUV422_12: { return "VA_RT_FORMAT_YUV422_12"; }
    case VA_RT_FORMAT_YUV444_12: { return "VA_RT_FORMAT_YUV444_12"; }
    case VA_RT_FORMAT_RGB16:     { return "VA_RT_FORMAT_RGB16";     }
    case VA_RT_FORMAT_RGB32:     { return "VA_RT_FORMAT_RGB32";     }
    case VA_RT_FORMAT_RGBP:      { return "VA_RT_FORMAT_RGBP";      }
    case VA_RT_FORMAT_RGB32_10:  { return "VA_RT_FORMAT_RGB32_10";  }
    case VA_RT_FORMAT_PROTECTED: { return "VA_RT_FORMAT_PROTECTED"; }
    default:                     { return "UNKNOWN";                }
  }
}

/* ------------------------------------------------------- */

static int enc_print_image(VAImage* img) {

  uint8_t* fcc = NULL;
  int8_t* order = NULL;
    
  if (NULL == img) {
    TRAE("Cannot print the `VAImage` as the given image is NULL.");
    return -1;
  }

  fcc = (uint8_t*)&img->format.fourcc;
  order = img->component_order;

  TRAD("VAImage");
  TRAD("  image_id: %u", img->image_id);
  TRAD("  format.fourcc: %c%c%c%c", fcc[0], fcc[1], fcc[2], fcc[3]);
  TRAD("  format.byte_order: %u", img->format.byte_order);
  TRAD("  format.bits_per_pixel: %u", img->format.bits_per_pixel);
  TRAD("  format.depth: %u", img->format.depth);
  TRAD("  format.red_mask: %u", img->format.red_mask);
  TRAD("  format.green_mask: %u", img->format.green_mask);
  TRAD("  format.blue_mask: %u", img->format.blue_mask);
  TRAD("  format.alpha_mask: %u", img->format.alpha_mask);
  TRAD("  buf: %u", img->buf);
  TRAD("  width: %u", img->width);
  TRAD("  height: %u", img->height);
  TRAD("  data_size: %u", img->data_size);
  TRAD("  num_planes: %u", img->num_planes);
  TRAD("  pitches[0]: %u", img->pitches[0]);
  TRAD("  pitches[1]: %u", img->pitches[1]);
  TRAD("  pitches[2]: %u", img->pitches[2]);
  TRAD("  offsets[0]: %u", img->offsets[0]);
  TRAD("  offsets[1]: %u", img->offsets[1]);
  TRAD("  offsets[2]: %u", img->offsets[2]);
  TRAD("  num_palette_entries: %d", img->num_palette_entries);
  TRAD("  component_order: %d, %d, %d, %d", order[0], order[1], order[2], order[2]);

  return 0;
}

/* ------------------------------------------------------- */

static int enc_pring_seq_param(VAEncSequenceParameterBufferH264* sps) {

  if (NULL == sps) {
    TRAE("Cannot print the SPS, it's NULL.");
    return -1;
  }

  TRAD("VAEncSequenceParameterBufferH264");
  TRAD("  seq_parameter_set_id: %u", sps->seq_parameter_set_id);
  TRAD("  level_idc: %u", sps->level_idc);
  TRAD("  intra_period: %u", sps->intra_period);
  TRAD("  intra_idr_period: %u", sps->intra_idr_period);
  TRAD("  ip_period: %u", sps->ip_period);
  TRAD("  bits_per_second: %u", sps->bits_per_second);
  TRAD("  max_num_ref_frames: %u", sps->max_num_ref_frames);
  TRAD("  picture_width_in_mbs: %u", sps->picture_width_in_mbs);
  TRAD("  picture_height_in_mbs: %u", sps->picture_height_in_mbs);
  TRAD("  seq_fields: ");
  TRAD("    chroma_format_idc: %u", sps->seq_fields.bits.chroma_format_idc);
  TRAD("    frame_mbs_only_flag: %u", sps->seq_fields.bits.frame_mbs_only_flag);
  TRAD("    mb_adapative_frame_field_flag: %u", sps->seq_fields.bits.mb_adaptive_frame_field_flag);
  TRAD("    seq_scaling_matrix_present_flag: %u", sps->seq_fields.bits.seq_scaling_matrix_present_flag);
  TRAD("    direct_8x8_inference_flag: %u", sps->seq_fields.bits.direct_8x8_inference_flag);
  TRAD("    log2_max_frame_num_minus4: %u", sps->seq_fields.bits.log2_max_frame_num_minus4);
  TRAD("    pic_order_cnt_type: %u", sps->seq_fields.bits.pic_order_cnt_type);
  TRAD("    log2_max_pic_order_cnt_lsb_minus4: %u", sps->seq_fields.bits.log2_max_pic_order_cnt_lsb_minus4);
  TRAD("    delta_pic_order_always_zero_flag: $u", sps->seq_fields.bits.delta_pic_order_always_zero_flag);
  TRAD("  bit_depth_luma_minus8: %u", sps->bit_depth_luma_minus8);
  TRAD("  bit_depth_chroma_minus8: %u", sps->bit_depth_chroma_minus8);
  TRAD("  num_ref_frames_in_pic_order_cnt_cycle: %u", sps->num_ref_frames_in_pic_order_cnt_cycle);
  TRAD("  offset_for_non_ref_pic: %u", sps->offset_for_non_ref_pic);
  TRAD("  offset_for_top_to_bottom_field: %u", sps->offset_for_top_to_bottom_field);
  TRAD("  frame_cropping_flag: %u", sps->frame_cropping_flag);
  TRAD("  frame_crop_left_offset: %u", sps->frame_crop_left_offset);
  TRAD("  frame_crop_right_offset: %u", sps->frame_crop_right_offset);
  TRAD("  frame_crop_top_offset: %u", sps->frame_crop_top_offset);
  TRAD("  frame_crop_bottom_offset: %u", sps->frame_crop_bottom_offset);
  TRAD("  vui_parameters_present_flag: %u", sps->vui_parameters_present_flag);
  TRAD("     aspect_ratio_info_present_flag: %u", sps->vui_fields.bits.aspect_ratio_info_present_flag);
  TRAD("     timing_info_present_flag: %u", sps->vui_fields.bits.timing_info_present_flag);
  TRAD("     bitstream_restriction_flag: %u", sps->vui_fields.bits.bitstream_restriction_flag);
  TRAD("     log2_max_mv_length_horizontal: %u", sps->vui_fields.bits.log2_max_mv_length_horizontal);
  TRAD("     log2_max_mv_length_vertical: %u", sps->vui_fields.bits.log2_max_mv_length_vertical);
  TRAD("     fixed_frame_rate_flag: %u", sps->vui_fields.bits.fixed_frame_rate_flag);
  TRAD("     low_delay_hrd_flag: %u", sps->vui_fields.bits.low_delay_hrd_flag);
  TRAD("     motion_vectors_over_pic_boundaries_flag: %u", sps->vui_fields.bits.motion_vectors_over_pic_boundaries_flag);
  TRAD("  aspect_ratio_idc: %u", sps->aspect_ratio_idc);
  TRAD("  sar_width: %u", sps->sar_width);
  TRAD("  sar_height: %u", sps->sar_height);
  TRAD("  num_units_in_tick: %u", sps->num_units_in_tick);
  TRAD("  time_scale: %u", sps->time_scale);
  TRAD("");
    
  return 0;
}

/* ------------------------------------------------------- */

static int enc_print_pic_param(VAEncPictureParameterBufferH264* pps) {

  if (NULL == pps) {
    TRAE("Cannot print he picture parameter set; it's NULL.");
    return -1;
  }

  TRAD("VAEncPictureParameterBufferH264");
  TRAD("  CurrPic: %p", &pps->CurrPic);
  TRAD("  ReferencesFrames[16] (in PDB).");
  TRAD("  pic_parameter_set_id: %u", pps->pic_parameter_set_id);
  TRAD("  seq_parameter_set_id: %u", pps->seq_parameter_set_id);
  TRAD("  last_picture: %u", pps->last_picture);
  TRAD("  frame_num: %u", pps->frame_num);
  TRAD("  pic_init_qp: %u", pps->pic_init_qp);
  TRAD("  num_ref_idx_l0_active_minus1: %u", pps->num_ref_idx_l0_active_minus1);
  TRAD("  num_ref_idx_l1_active_minus1: %u", pps->num_ref_idx_l1_active_minus1);
  TRAD("  chroma_qp_index_offset: %u", pps->chroma_qp_index_offset);
  TRAD("  second_chroma_qp_index_offset: %u", pps->second_chroma_qp_index_offset);
  TRAD("  pic_fields:");
  TRAD("    idr_pic_flag: %u", pps->pic_fields.bits.idr_pic_flag);
  TRAD("    reference_pic_flag: %u", pps->pic_fields.bits.reference_pic_flag);
  TRAD("    entropy_coding_mode_flag: %u", pps->pic_fields.bits.entropy_coding_mode_flag);
  TRAD("    weighted_pred_flag: %u", pps->pic_fields.bits.weighted_pred_flag);
  TRAD("    constrained_intra_pred_flag: %u", pps->pic_fields.bits.constrained_intra_pred_flag);
  TRAD("    transform_8x8_mode_flag: %u", pps->pic_fields.bits.transform_8x8_mode_flag);
  TRAD("    deblocking_filter_control_present_flag: %u", pps->pic_fields.bits.deblocking_filter_control_present_flag);
  TRAD("    redundant_pic_cnt_present_flag: %u", pps->pic_fields.bits.redundant_pic_cnt_present_flag);
  TRAD("    pic_order_present_flag: %u", pps->pic_fields.bits.pic_order_present_flag);
  TRAD("    pic_scaling_matrix_present_flag: %u", pps->pic_fields.bits.pic_scaling_matrix_present_flag);
  TRAD("");

  return 0;
}

/* ------------------------------------------------------- */
