/* ------------------------------------------------------- */

#include <errno.h>
#include <stdlib.h>

#include <va/va_x11.h>
#include <va/va.h>

#include <tra/modules/vaapi/vaapi-utils.h>
#include <tra/modules/vaapi/vaapi-dec.h>
#include <tra/module.h>
#include <tra/types.h>
#include <tra/log.h>
#include <tra/avc.h>

/* ------------------------------------------------------- */

struct va_dec {

  /* General */
  Display* xorg_display;                  /* The connection with X11. */
  VADisplay* va_display;                  /* VA connection with X11 */
  VAConfigID config_id;                   /* A handle to the configuration that represents our decoding session; e.g. the pixel format. */
  VAContextID context_id;                 /* Handle that represents our connection with the decoder. */
  VAProfile profile;                      /* The decoder profile. */
  VAIQMatrixBufferH264 iq_matrix;         /* Inverse Quantization Matrix: for each picture that we want to decode we have to supply this too.  */
  tra_avc_reader* avc_reader;             /* The `tra_avc_reader` instance that we use to parse NALs. */
  int ver_major;                          /* Major version of VA; retrieved when we initialize VAAPI. */
  int ver_minor;                          /* Minor version of VA; retrieved when we initialize VAAPI. */
  tra_decoder_callbacks callbacks;        /* The callbacks info that we copy from the settings. */

  /* Image */
  uint32_t width;                         /* The width of the video; given via settings. */
  uint32_t height;                        /* The height of the video; given via the settings. */
  uint32_t width_mb_aligned;              /* Width aligned to the macroblock size fo 16; e.g. number of macroblocks that fit in the width. */
  uint32_t height_mb_aligned;             /* Height aligned to the macroblock size of 16; e.g. number of macroblocks that fit in the height. */
  uint32_t width_in_mbs;                  /* The number of macroblocks that fit in the (aligned) width. */
  uint32_t height_in_mbs;                 /* The number of macroblocks that fit in the (aligned) height. */

  /* Bitstream */
  VAPictureParameterBufferH264 pic_param; /* See `va_dec_decode() and `dec_decode_nal()`. We setup the `VAPictureParameterH264` for the current access unit; we reuse this member. */ 
  VASliceParameterBufferH264 slice_param; /* See `va_dec_decode() and `dec_decode_nal()`. We setup the `VASliceParameterBufferH264` for the current access unit; we reuse this member. */ 
  tra_avc_parsed_sps parsed_sps;           /* The `tra_avc_reader` keeps track of received SPS instances and sets this. The data is owned by the `tra_avc_reader`. */
  tra_avc_parsed_pps parsed_pps;           /* The `tra_avc_reader` keeps track of received PPS iunstance(s) and sets this. The data is owned by the `tra_avc_reader`. */
  tra_avc_parsed_slice parsed_slice;  
  
  /* Surfaces */
  VASurfaceID* dest_surfaces;             /* Array with the destination surfaces. When we start the decode process we tell VA what surface to use as destination buffer by calling `vaBeginPicture()` and passing `curr_dest_surface_index`. */
  uint32_t num_dest_surfaces;             /* The total number of destination surfaces. */
  uint32_t curr_dest_surface_index;       /* The current destination that we use; is the same for one decode call; is incremented at the end of `va_dec_decode()`, we do this in `dec_decode_nal()`. */

  /* Reference frames. */
  uint32_t short_ref_count;               /* The current number of reference frames that we store in `ref_frames`. This is increment for each nal for which `nal_ref_idc != 0` and will not exceed `sps.max_num_ref_frames`. This is used to determine the size of the first-in/first-out reference lists. */
  uint32_t ref_list0_max;                 /* The maximum number of references in list0 (per slice, so normally 32). */
  uint32_t ref_list1_max;                 /* The maximum number of references in list1 (per slice, so normally 32). */
  VAPictureH264 ref_frames[16];           /* The reference frames that we keep track of. Whenever `nal_ref_idc` is set for a nal, we store the frame in this array. This array is then used to create the reference frames for a picture- and slice-param. */
  VAPictureH264 ref_list0[32];            /* Depending on the current frame type (P or B) this list holds the reference frames sorted by either the decode order (frame_num for P-slices, short-term) or display order (B-slices, short-term). 32 because we can have up to 32 fields.*/
  VAPictureH264 ref_list1[32];            /* This list is use to store reference frames for B-slices. 32 because we can have up to 32 fields */
};
  
/* ------------------------------------------------------- */

static int dec_parse_nal(va_dec* ctx, uint8_t* nalStart, uint32_t nalSize, uint8_t* canDecode); /* Based on the type of nal we feed data to the decoder. `nalStart` should point to the nal unit byte. The `canDecode` is set to 1 when can feed the current `va_dec::slice` to the decoder. */
static int dec_decode_nal(va_dec* ctx, uint8_t* nalStart, uint32_t nalSize); /* Decodes the given slice. */
static int dec_begin_picture(va_dec* ctx);
static int dec_end_picture(va_dec* ctx);
static int dec_map_picture(va_dec* ctx); /* This will map the picture which allows us to "download" it. */
static int dec_create_pic_param(va_dec* ctx);
static int dec_create_slice_param(va_dec* ctx, uint8_t* nalStart, uint32_t nalSize);
static int dec_update_reference_frames(va_dec* ctx);
static int dec_update_reference_lists(va_dec* ctx);

/* ------------------------------------------------------- */

/*

  GENERAL INFO:

    This function will create the `va_dec` instance; we make a
    connection with X11, create a decoder config, initialize VA
    etc.

  TODO:

    @todo  I've hardcoded the profile; this could/should be passed via settings. 
    @todo  Should we extract/move the "Inverse Quanitzation Matrix" to the SPS/PPS? 
    @todo  I've hardcoded the sizes for `ref_lits{0,1}_max`; maybe I should get them from the context?
    @todo  We have hardcoded the pixel format; might want to make that configurable.
    @todo  I'm not yet calling `dec_update_reference_frameS()`. 

 */
int va_dec_create(va_dec_settings* cfg, va_dec** ctx) {

  VAStatus status = VA_STATUS_SUCCESS;
  VAMessageCallback cb = NULL;
  VAConfigAttrib attrib = {};
  va_dec* inst = NULL;
  int r = 0;

  if (NULL == cfg) {
    TRAE("Cannot create the `va_dec`. Given `va_dec_settings*` is NULL.");
    r = -10;
    goto error;
  }

  if (0 == cfg->image_width) {
    TRAE("Cannot create the `va_dec`. The `image_width` of the `va_dec_settings` is 0. Specify the video width.");
    r = -20;
    goto error;
  }

  if (0 == cfg->image_height) {
    TRAE("Cannot create the `va_dec`. The `image_height` of the `va_dec_settings` is 0. Specify the video height.");
    r = -30;
    goto error;
  }

  if (NULL == ctx) {
    TRAE("Cannot create the `va_dec`, given `va_dec**` is NULL.");
    r = -50;
    goto error;
  }
  
  if (NULL != (*ctx)) {
    TRAE("Cannot create the `va_dec`, given `*(va_dec**)` is not NULL. Already initialized?");
    r = -60;
    goto error;
  }

  inst = calloc(1, sizeof(va_dec));
  if (NULL == inst) {
    TRAE("Failed to allocate the `va_dec` instance. Out of memory?");
    r = -70;
    goto error;
  }

  TRAE("@todo I've hardcoded the ref_list0_max and ref_list1_max which I probably want to retrieve from the decoder.");
  TRAE("@todo We're not yet calling `dec_update_reference_frames()`. ");

  /* --------------------------------------- */
  /* INITIALIZE                              */
  /* --------------------------------------- */

  inst->callbacks = *cfg->callbacks;
  inst->width = cfg->image_width;
  inst->height = cfg->image_height;
  inst->width_mb_aligned = (inst->width + 15) / (16 * 16);
  inst->height_mb_aligned = (inst->height + 15) / (16 * 16);
  inst->width_in_mbs = inst->width_mb_aligned / 16;
  inst->height_in_mbs = inst->height_mb_aligned / 16;
  inst->short_ref_count = 0;
  inst->ref_list0_max = 32;
  inst->ref_list1_max = 32;
  
  /* Get a display that represents a connection with XORG */
  inst->xorg_display = XOpenDisplay(NULL);
  if (NULL == inst->xorg_display) {;
    TRAE("Failed to open the display: %s", strerror(errno));
    r = -80;
    goto error;
  }

  inst->va_display = vaGetDisplay(inst->xorg_display);
  if (NULL == inst->va_display) {
    TRAE("Failed to get the display.");
    r = -90;
    goto error;
  }

  /* Initialize VA and get the minor/major version. */
  status = vaInitialize(inst->va_display, &inst->ver_major, &inst->ver_minor);
  if (VA_STATUS_SUCCESS != status) {
    TRAE("Failed to initialize VA.");
    r = -100;
    goto error;
  }

  cb = vaSetErrorCallback(inst->va_display, va_util_error_callback, NULL);
  if (NULL == cb) {
    TRAE("Failed to set the error callback.");
    r = -110;
    goto error;
  }

  cb = vaSetInfoCallback(inst->va_display, va_util_info_callback, NULL);
  if (NULL == cb) {
    TRAE("Failed to set the info callback.");
    r = -120;
    goto error;
  }

  /* Create the config. */
  attrib.type = VAConfigAttribRTFormat;
  attrib.value = VA_RT_FORMAT_YUV420;
  
  status = vaCreateConfig(
    inst->va_display,
    VAProfileH264ConstrainedBaseline,
    VAEntrypointVLD,
    &attrib,
    1,
    &inst->config_id
  );

  if (VA_STATUS_SUCCESS != status) {
    TRAE("Failed to create the config for the decoder.");
    r = -130;
    goto error;
  }

  /* Create the "dest" surfaces. */
  inst->num_dest_surfaces = 16;
  inst->curr_dest_surface_index = 0;
  inst->dest_surfaces = calloc(1, inst->num_dest_surfaces * sizeof(VASurfaceID));
  
  if (NULL == inst->dest_surfaces) {
    TRAE("Failed to allocate the dest surfaces.");
    r = -140;
    goto error;
  }

  status = vaCreateSurfaces(
    inst->va_display,
    VA_RT_FORMAT_YUV420,
    inst->width,
    inst->height,
    inst->dest_surfaces,
    inst->num_dest_surfaces,
    NULL,
    0
  );

  if (VA_STATUS_SUCCESS != status) {
    TRAE("Failed to create the `dest_surfaces`.");
    r = -150;
    goto error;
  }

  /* Create the context */
  status = vaCreateContext(
    inst->va_display,
    inst->config_id,
    inst->width,
    inst->height,
    VA_PROGRESSIVE,
    inst->dest_surfaces,
    inst->num_dest_surfaces,
    &inst->context_id
  );

  if (VA_STATUS_SUCCESS != status) {
    TRAE("Failed to create the context.");
    r = -160;
    goto error;
  }

  /* 
     Create the inverse quantization matrix. These values are
     used during the inverse DCT process. Currently we're using
     the default values, but we might want to get them from SPS
     (see `seq_scaling_matrix_present_flag`) and the PPS (see the
     `pic_scaling_matrix_present_flag`).
  */
  memset(inst->iq_matrix.ScalingList4x4, 0x10, sizeof(inst->iq_matrix.ScalingList4x4));
	memset(inst->iq_matrix.ScalingList8x8[0], 0x10, sizeof(inst->iq_matrix.ScalingList8x8[0]));
	memset(inst->iq_matrix.ScalingList8x8[1], 0x10, sizeof(inst->iq_matrix.ScalingList8x8[0]));

  r = tra_avc_reader_create(&inst->avc_reader);
  if (r < 0) {
    TRAE("Failed to create the `tra_avc_reader`.");
    r = -170;
    goto error;
  }
  
  *ctx = inst;

 error:

  if (r < 0) {
    if (NULL != inst) {
      va_dec_destroy(inst);
      inst = NULL;
    }

    if (NULL != ctx) {
      *ctx = NULL;
    }
  }
  
  return r;
}

/* ------------------------------------------------------- */

int va_dec_destroy(va_dec* ctx) {

  VAStatus status = VA_STATUS_SUCCESS;
  int result = 0;
  int r = 0;
  
  if (NULL == ctx) {
    TRAE("Cannot destroy the `app*` as it's NULL.");
    return -1;
  }

  /* Destroy the dest surfaces. */
  if (NULL != ctx->va_display
      && NULL != ctx->dest_surfaces)
    {
      status = vaDestroySurfaces(ctx->va_display, ctx->dest_surfaces, ctx->num_dest_surfaces);
      if (VA_STATUS_SUCCESS != status) {
        TRAE("Failed to cleanly destroy the destination surfaces.");
        result -= 2;
      }
    }
  
  /* Destroy context */
  if (NULL != ctx->va_display
      && 0 != ctx->context_id)
    {
      status = vaDestroyContext(ctx->va_display, ctx->context_id);
      if (VA_STATUS_SUCCESS != status) {
        TRAE("Failed to cleanly destroy the context id.");
        result -= 4;
      }
    }

  /* Destroy config */
  if (NULL != ctx->va_display
      && 0 != ctx->config_id)
    {
      status = vaDestroyConfig(ctx->va_display, ctx->config_id);
      if (VA_STATUS_SUCCESS != status) {
        TRAE("Failed to cleanly destroy the config.");
        result -= 6;
    }
  }

  /* Terminate VA */
  if (NULL != ctx->va_display) {
    status = vaTerminate(ctx->va_display);
    if (VA_STATUS_SUCCESS != status) {
      TRAE("Failed to cleanly terminate VA.");
      result -= 6;
    }
  }

  if (NULL != ctx->xorg_display) {
    XCloseDisplay(ctx->xorg_display);
  }

  if (NULL != ctx->avc_reader) {
    r = tra_avc_reader_destroy(ctx->avc_reader);
    if (r < 0) {
      TRAE("Failed to cleanly destroy the `tra_avc_reader`.");
      result -= 8;
    }
  }

  ctx->xorg_display = NULL;
  ctx->va_display = NULL;
  ctx->avc_reader = NULL;
  ctx->config_id = 0;
  ctx->context_id = 0;
  ctx->profile = 0;
  ctx->ver_minor = 0;
  ctx->ver_major = 0;

  ctx->width = 0;
  ctx->height = 0;
  ctx->width_mb_aligned = 0;
  ctx->height_mb_aligned = 0;
  ctx->width_mb_aligned = 0;
  ctx->height_mb_aligned = 0;

  ctx->dest_surfaces = NULL;
  ctx->num_dest_surfaces = 0;
  ctx->curr_dest_surface_index = 0;

  ctx->short_ref_count = 0;
  ctx->ref_list0_max = 0;
  ctx->ref_list1_max = 0;

  free(ctx);
  ctx = NULL;

  return result;
}

/* ------------------------------------------------------- */

/*
  
  GENERAL INFO:

    This function will decode the given data. Currently we expect
    the data to use annex-b H264. This function will parse the
    given data and makes sure the decoder receives the correct
    data. You can pass multiple nals in one call; though in most
    cases it makes sense to pass e.g. [SPS, PPS, IDR], [P], [I],
    [B] access units seperately.

    Currently we support the `TRA_MEMORY_TYPE_H264` input
    type, but in the near future we will support device memory
    too. When the type is `TRA_MEMORY_TYPE_H264`, we expect a
    `tra_memory_h264` to be given as the `data`. The
    `size` member is the total size of the included data.

    This function loops over all the nals in the given data and
    passes each nal into `dec_parse_nal()` which will extract the
    required information from the SPS, PPS and slices that we
    need to decode. This function sets the parameter `canDecode`
    to 1, when we have a valid SPS, PPS and the current nal is a
    slice. When `canDecode` is 1, we call `dec_decode_nal()`.

 */
int va_dec_decode(va_dec* ctx, uint32_t type, void* data) {

  tra_memory_h264* host_mem = NULL;
  uint8_t* data_ptr = NULL;
  uint8_t* nal_start = NULL;
  uint32_t nal_size = 0;
  uint32_t nbytes_parsed = 0;
  uint32_t nbytes_left = 0;
  uint32_t nbytes_header = 0; /* The size of the annex-b header. */
  uint8_t can_decode = 0;     /* Is set to 1 when we can decode the current nal: e.g. when we have a SPS, PPS and the current nal is a slice. */
  int r = 0;
  
  if (NULL == ctx) {
    TRAE("Cannot decode as the given `va_dec*` is NULL.");
    r = -10;
    goto error;
  }

  if (TRA_MEMORY_TYPE_H264 != type) {
    TRAE("Cannot decode as the `type` is not TRA_MEMORY_TYPE_H264. We don't support other types yet.");
    r = -20;
    goto error;
  }

  if (NULL == data) {
    TRAE("Cannot decode as the given `data` is NULL.");
    r = -30;
    goto error;
  }

  host_mem = (tra_memory_h264*) data;
  if (NULL == host_mem->data) {
    TRAE("Cannot decode as the `data` member of the `tra_memory_h264` is NULL.");
    r = -40;
    goto error;
  }

  if (0 == host_mem->size) {
    TRAE("Cannot decode as the `size` member of the `tra_memory_h264` is 0.");
    r = -50;
    goto error;
  }
  
  /* As long as we have data to parse, find the next NAL unit. */
  nbytes_left = host_mem->size;
  data_ptr = host_mem->data;
  
  while (nbytes_left > 0) {

    nal_start = NULL;
    nal_size = 0;
    
    r = tra_nal_find(data_ptr, nbytes_left, &nal_start, &nal_size);
    if (r < 0) {
      TRAE("Cannot decode, no nal found.");
      r = -60;
      goto error;
    }

    if (nal_start <= data_ptr) {
      TRAE("The found `nal_start` is wrong; it's position outside the current scope.");
      r = -70;
      goto error;
    }

    nbytes_header = nal_start - data_ptr;
    if (3 != nbytes_header
        && 4 != nbytes_header)
      {
        TRAE("The annex-b header that we found is not 3 or 4 bytes. Did something go wrong while parsing the nal?");
        r = -80;
        goto error;
      }

    if (nbytes_left < nbytes_header) {
      TRAE("The found nal header size is larger than the available bytes? Something went wrong.");
      r = -90;
      goto error;
    }

    nbytes_left -= nbytes_header;

    if (nbytes_left < nal_size) {
      TRAE("The size of the nal is larger than the available bytes. Something went wrong.");
      r = -100;
      goto error;
    }

    nbytes_left -= nal_size;
    data_ptr += nbytes_header;
    data_ptr += nal_size;

    /* Parse the nal. */
    r = dec_parse_nal(ctx, nal_start, nal_size, &can_decode);
    if (r < 0) {
      TRAE("Failed to parse a nal.");
      r = -110;
      goto error;
    }

    if (0 == can_decode) {
      continue;
    }

    /* Decode the nal: at this point we have a slice and previously parsed a SPS and PPS. */
    r = dec_decode_nal(ctx, nal_start, nal_size);
    if (r < 0) {
      TRAE("Failed to decode the current nal.");
      r = -120;
      goto error;
    }
  }    

 error:
  return r;
}

/* ------------------------------------------------------- */

/*
  Based on the type of nal we feed data to the
  decoder. `nalStart` should point to the nal unit byte. The
  `canDecode` is set to 1 when we've parsed a SPS, PPS and the
  current nal is a slice.
*/
static int dec_parse_nal(va_dec* ctx, uint8_t* nalStart, uint32_t nalSize, uint8_t* canDecode) {

  uint8_t nal_unit_type = 0;
  uint8_t is_slice = 0;
  int r = 0;
  
  if (NULL == ctx) {
    TRAE("Cannot parse nal, given `va_dec*` is NULL.");
    return -1;
  }

  if (NULL == nalStart) {
    TRAE("Cannot parse nal, given `nalStart` is NULL.");
    return -2;
  }

  if (0 == nalSize) {
    TRAE("Cannot parse nal, given `nalSize` is NULL.");
    return -3;
  }

  if (NULL == canDecode) {
    TRAE("Cannot set the `canDecode` result as the given pointer is NULL.");
    return -4;
  }

  *canDecode = 0;
  nal_unit_type = nalStart[0] & 0x1F;
  
  switch (nal_unit_type) {

    /* SLICE */
    case TRA_NAL_TYPE_CODED_SLICE_NON_IDR: 
    case TRA_NAL_TYPE_CODED_SLICE_IDR: {
      
      r = tra_avc_parse_slice(ctx->avc_reader, nalStart, nalSize, &ctx->parsed_slice);
      if (r < 0) {
        TRAE("Failed to parse the slice.");
        return -4;
      }

      is_slice = 1;

      break;
    }

    /* SPS */
    case TRA_NAL_TYPE_SPS: {

      r = tra_avc_parse_sps(ctx->avc_reader, nalStart, nalSize, &ctx->parsed_sps);
      if (r < 0) {
        TRAE("Failed to parse the SPS.");
        return -4;
      }
      
      break;
    }

    /* PPS */
    case TRA_NAL_TYPE_PPS: {

      r = tra_avc_parse_pps(ctx->avc_reader, nalStart, nalSize, &ctx->parsed_pps);
      if (r < 0) {
        TRAE("Failed to parse the PPS.");
        return -4;
      }
      
      break;
    }
    
    default: {
      TRAE("Unhandled NAL unit type: %u.", nal_unit_type);
      return -4;
    }
  }

  /* When we have a SPS, PPS and SLICE we can decode. */
  if (NULL == ctx->parsed_pps.pps || NULL == ctx->parsed_sps.sps) {
    return 0;
  }
  
  if (0 == is_slice) {
    return 0;
  }

  /* Decode! */
  *canDecode = 1;

  return 0;
}

/* ------------------------------------------------------- */

/*

  GENERAL INFO: 

    This function will setup all the required parameters for
    VAAPI. This function should only be called when
    `dec_parse_nal()` has set the `canDecode` parameter to 1.
    This means that all the data we need has been parsed: SPS,
    PPS and the current nal (nalStart) is a slice.
    
    The decode process starts by calling `vaBeginPicture()`, then
    we create a picture param that describes the picture that we
    want to decode. This involves setting up the
    `VAPictureParameterBufferH264`, and
    `VASliceParameterBufferH264`. The
    `VASliceParameterBufferH264` contains information about the
    slice. It contains attributes like the bit offset in the
    stream, the macroblock offset, slice_type, etc. The slice
    parameters will also get a list with reference frames when it
    may reference other pictures. The slice is sent to the server
    together with a `VASliceDataBufferType` which contains the
    actual nal data.

    When we've sent all the buffers we call `vaEndPicture()` which 
    kicks off the decode process. After this we can synchronise and
    map the data to receive decoded data. 

    At the end of the decode process we update the reference
    frame. See `dec_update_reference_frames` for more info. Then
    we increment the destination surface index.
    
    To summarize;
    
      - vaBeginPicture();
      - va{Create,Render}Buffer: 
        - VAIQMatrixBufferType
        - VAPictureParameterBufferType
        - VASliceParameterBufferType
        - VASliceDataBufferType
      - vaEndPicture();
  
  TODO:

    @todo   I'm not sure how we should handle an error case -after- 
            `dec_begin_picture()` succeeded. Do we always have to call
            `dec_end_picture()` (?).

    @todo   Figure out when we should map/sync a frame. 

 */
static int dec_decode_nal(va_dec* ctx, uint8_t* nalStart, uint32_t nalSize) {

  int r = 0;
  
  if (NULL == ctx) {
    TRAE("Cannot decode the given nal as the given `va_dec` is NULL.");
    return -1;
  }

  if (NULL == nalStart) {
    TRAE("Cannot decode the given nal as it's NULL.");
    return -2;
  }

  if (0 == nalSize) {
    TRAE("Cannot decode the given nal as the size is 0.");
    return -3;
  }

  if (NULL == ctx->parsed_sps.sps) {
    TRAE("Cannot decode the given nal as we don't have a valid SPS yet.");
    return -4;
  }

  if (NULL == ctx->parsed_pps.pps) {
    TRAE("Cannot decode the given nal as we don't have a valid PPS yet.");
    return -5;
  }

  r = dec_begin_picture(ctx);
  if (r < 0) {
    TRAE("Failed to begin the decode process.");
    return -6;
  }

  r = dec_create_pic_param(ctx);
  if (r < 0) {
    TRAE("Failed to create the picture param.");
    return -7;
  }

  r = dec_create_slice_param(ctx, nalStart, nalSize);
  if (r < 0) {
    TRAE("Failed to create the slice param.");
    return -8;
  }

  r = dec_end_picture(ctx);
  if (r < 0) {
    TRAE("Failed to end the decode process.");
    return -9;
  }

  /* This is where we could map the current picture */
  r = dec_map_picture(ctx);
  if (r < 0) {
    TRAE("Failed to map the picture which holds the decoded image.");
    return -10;
  }

  r = dec_update_reference_frames(ctx);
  if (r < 0) {
    TRAE("Failed to update the reference frames.");
    return -11;
  }

  /* Make sure the `curr_dest_surface_index` stays in range. */
  ctx->curr_dest_surface_index++;
  if (ctx->curr_dest_surface_index >= ctx->num_dest_surfaces) {
    ctx->curr_dest_surface_index = 0;
  }

  return r;
}

/* ------------------------------------------------------- */

/*

  This function is the "start" of an encode loop. We specifcy
  into which destination surface the decoded data should be
  written by passing it into `vaBeginPicture()`.  The surface
  into which we "render" can later be used as reference
  surface. This `va_dec::dest_surfaces` array contains
  `VAPictureH264` structs into which we render; after each decode
  loop we store the current surface as a reference when the
  current nal is a reference frame.

 */
static int dec_begin_picture(va_dec* ctx) {
  
  VAStatus status = VA_STATUS_SUCCESS;
  
  if (NULL == ctx) {
    TRAE("Cannot begin picture; given `va_dec*` is NULL.");
    return -1;
  }

  status = vaBeginPicture(ctx->va_display, ctx->context_id, ctx->dest_surfaces[ctx->curr_dest_surface_index]);
  if (VA_STATUS_SUCCESS != status) {
    TRAE("Failed to begin picture.");
    return -2;
  }

  return 0;
}

/* ------------------------------------------------------- */

static int dec_end_picture(va_dec* ctx) {

  VAStatus status = VA_STATUS_SUCCESS;
  int r = 0;

  if (NULL == ctx) {
    TRAE("Cannot end the picture; given `va_dec*` is NULL.");
    return -1;
  }

  status = vaEndPicture(ctx->va_display, ctx->context_id);
  if (VA_STATUS_SUCCESS != status) {
    TRAE("Failed to end picture.");
    return -2;
  }

  return 0;
}

/* ------------------------------------------------------- */

/*

  GENERAL INFO:
  
    This function will setup the h264 picture parameters which
    include both data from the NAL (to check if it's a reference
    frame), SPS and PPS. This code is partly based on
    `vaapi_h264.c` from FFmpeg and we used the [yangrtc][1] 
    while looking into this.

    We set up our `VAPictureParameterBufferH264` and the
    `VAIQMatrixBufferType` which contains the inverse
    quantization matrix (IQM). VAAPI does not have a "correct"
    IQM matrix by default; when we don't set it the resulting
    output will be invalid.

    When we setup the `VAPictureParameterBufferH264` we have to
    get some info from the nal that we're currently handling.
    This nal is the nal of a slice. See below where we map a
    local `tra_nal*` to `va_dec.parsed_slice.nal`. The `tra_nal`
    type is just a tiny struct that represents the nal header
    which is stored before a SPS, PPS, SLICE, etc.

  TODO:

    @todo Add support for other then picture order count types = 0.
    @todo Add support for slice based decoding.
    @todo Add support for scaling lists that we have to extract from SPS/PPS.

  REFERENCES:

    [0]: https://github.com/FFmpeg/FFmpeg/blob/master/libavcodec/vaapi_h264.c
    [1]: https://github.com/yangrtc/yangrtc/blob/main/libmetartc4/src/yangdecoder/pc/YangVideoDecoderIntel.cpp

*/
static int dec_create_pic_param(va_dec* ctx) {

  VAPictureParameterBufferH264* pp = NULL;
  VAStatus status = VA_STATUS_SUCCESS;
  VABufferID pic_param_buf = 0;
  VABufferID iq_matrix_buf = 0;
  VABufferID buf_ids[2] = {};
  VAPictureH264* pic = NULL;
  tra_slice* slice = NULL;
  tra_nal* nal = NULL;
  tra_sps* sps = NULL;
  tra_pps* pps = NULL;
  uint32_t num = 0;
  uint32_t i = 0;
  int r = 0;

  if (NULL == ctx) {
    TRAE("Cannot create the pic param as the given `va_dec*` is NULL.");
    return -1;
  }

  if (NULL == ctx->parsed_sps.sps) {
    TRAE("Cannot create the pic param as the given `ctx->parsed_sps.sps*` is NULL.");
    return -2;
  }

  if (NULL == ctx->parsed_pps.pps) {
    TRAE("Cannot create the pic param as the given `ctx->parsed_pps.pps*` is NULL.");
    return -3;
  }

  /* Some shorter vars. */
  pp = &ctx->pic_param;
  slice = &ctx->parsed_slice.slice;
  nal = &ctx->parsed_slice.nal;
  sps = ctx->parsed_sps.sps;
  pps = ctx->parsed_pps.pps;

  /* Make sure that the current `nal` is a slice. */
  switch (nal->nal_unit_type) {
    
    case TRA_NAL_TYPE_CODED_SLICE_NON_IDR:
    case TRA_NAL_TYPE_CODED_SLICE_DATA_PARTITION_A:
    case TRA_NAL_TYPE_CODED_SLICE_DATA_PARTITION_B:
    case TRA_NAL_TYPE_CODED_SLICE_DATA_PARTITION_C:
    case TRA_NAL_TYPE_CODED_SLICE_IDR: {
      break;
    }
    
    default: {
      TRAE("The given nal type is not a coded slice; we can only create a picture param when we're handling a slice. ");
      return -4;
    }
  }
  
  /* Setup CurrPic */
  pic = &pp->CurrPic;
  pic->picture_id = ctx->dest_surfaces[ctx->curr_dest_surface_index];
  pic->frame_idx = slice->frame_num;
  pic->flags = 0;
  pic->TopFieldOrderCnt = slice->pic_order_cnt_lsb; 
  pic->BottomFieldOrderCnt = slice->pic_order_cnt_lsb;

  /* Setup picture params. */
  pp->picture_width_in_mbs_minus1 = sps->pic_width_in_mbs_minus1;
  pp->picture_height_in_mbs_minus1 = sps->pic_height_in_map_units_minus1;
  pp->bit_depth_luma_minus8 = 0; /* not implemented */
  pp->bit_depth_chroma_minus8 = 0; /* not implemented */
  pp->num_ref_frames = sps->max_num_ref_frames;
  pp->frame_num = slice->frame_num;

  pp->seq_fields.bits.chroma_format_idc = sps->chroma_format_idc;
  pp->seq_fields.bits.residual_colour_transform_flag = 0; /* not implemented */
  pp->seq_fields.bits.gaps_in_frame_num_value_allowed_flag = sps->gaps_in_frame_num_value_allowed_flag;
  pp->seq_fields.bits.frame_mbs_only_flag = sps->frame_mbs_only_flag;
  pp->seq_fields.bits.mb_adaptive_frame_field_flag = sps->mb_adaptive_frame_field_flag;
  pp->seq_fields.bits.direct_8x8_inference_flag = sps->direct_8x8_inference_flag;
  pp->seq_fields.bits.MinLumaBiPredSize8x8 = (sps->level_idc >= 31); /* A.3.3.2, copied from vaapi_h264.c */
  pp->seq_fields.bits.log2_max_frame_num_minus4 = sps->log2_max_frame_num_minus4;
  pp->seq_fields.bits.pic_order_cnt_type = sps->pic_order_cnt_type; /* We only support picture order count 0. */
  pp->seq_fields.bits.log2_max_pic_order_cnt_lsb_minus4 = sps->log2_max_pic_order_cnt_lsb_minus4;
  pp->seq_fields.bits.delta_pic_order_always_zero_flag = 0; /* not implemented, pic order cnt type = 1 */

  pp->num_slice_groups_minus1 = 0; /* FMO is not supported */
  pp->slice_group_map_type = 0; /* FMO is not supported */
  pp->slice_group_change_rate_minus1 = 0; /* FMO is not supported */
  pp->pic_init_qp_minus26 = pps->pic_init_qp_minus26;
  pp->pic_init_qs_minus26 = pps->pic_init_qs_minus26;
  pp->chroma_qp_index_offset = pps->chroma_qp_index_offset;

  pp->pic_fields.bits.entropy_coding_mode_flag = pps->entropy_coding_mode_flag;
  pp->pic_fields.bits.weighted_pred_flag = pps->weighted_pred_flag;
  pp->pic_fields.bits.weighted_bipred_idc = pps->weighted_bipred_idc;
  pp->pic_fields.bits.transform_8x8_mode_flag = 0; /* not implemented */
  pp->pic_fields.bits.field_pic_flag = 0; /* @todo: currently we don't support slice based decoding; only frame based.  */
  pp->pic_fields.bits.constrained_intra_pred_flag = pps->constrained_intra_pred_flag;
  pp->pic_fields.bits.pic_order_present_flag = pps->bottom_field_pic_order_in_frame_present_flag;
  pp->pic_fields.bits.deblocking_filter_control_present_flag = pps->deblocking_filter_control_present_flag;
  pp->pic_fields.bits.redundant_pic_cnt_present_flag = pps->redundant_pic_cnt_present_flag;
  pp->pic_fields.bits.reference_pic_flag = (0 == nal->nal_ref_idc) ? 0 : 1; 

  /* Copy our short reference pictures. */
  num = sizeof(pp->ReferenceFrames) / sizeof(pp->ReferenceFrames[0]);
  memcpy(pp->ReferenceFrames, ctx->ref_frames, ctx->short_ref_count * sizeof(VAPictureH264));

  /* Unset all the other reference pictures. */
  for (i = ctx->short_ref_count; i < num; ++i) {
    pp->ReferenceFrames[i].picture_id = VA_INVALID_SURFACE;
    pp->ReferenceFrames[i].flags = VA_PICTURE_H264_INVALID;
  }

  /* Create the buffer for the iq-matrix */
  status = vaCreateBuffer(
    ctx->va_display,
    ctx->context_id,
    VAIQMatrixBufferType,
    sizeof(ctx->iq_matrix),
    1,
    &ctx->iq_matrix,
    &iq_matrix_buf
  );

  if (VA_STATUS_SUCCESS != status) {
    TRAE("Failed to create the buffer for the iq-matrix.");
    r = -6;
    goto error;
  }

  /* Create the buffer for the pic param. */
  status = vaCreateBuffer(
    ctx->va_display,
    ctx->context_id,
    VAPictureParameterBufferType,
    sizeof(ctx->pic_param),
    1,
    &ctx->pic_param,
    &pic_param_buf
  );
  
  if (VA_STATUS_SUCCESS != status) {
    TRAE("Failed to create the buffer for our picture parameters.");
    r = -6;
    goto error;
  }

  /* Send the buffers to the server. */
  buf_ids[0] = pic_param_buf;
  buf_ids[1] = iq_matrix_buf;
  
  status = vaRenderPicture(ctx->va_display, ctx->context_id, buf_ids, 2);
  if (VA_STATUS_SUCCESS != status) {
    TRAE("Failed to render the picture parameter buffer and iq-matrix buffer.");
    r = -7;
    goto error;
  }

 error:
  return r;
}

/* ------------------------------------------------------- */

static int dec_create_slice_param(va_dec* ctx, uint8_t* nalStart, uint32_t nalSize) {

  VASliceParameterBufferH264* sp = NULL;
  VAStatus status = VA_STATUS_SUCCESS;
  VABufferID render_ids[2] = {};
  VABufferID param_buf = 0;
  VABufferID data_buf = 0;
  tra_slice* slice = NULL;
  tra_sps* sps = NULL;
  tra_pps* pps = NULL;
  tra_nal* nal = NULL;
  uint32_t num_refs = 0;
  uint32_t i = 0;
  int r = 0;

  if (NULL == ctx) {
    TRAE("Cannot create the slice param, given `va_dec` is NULL.");
    return -1;
  }

  if (NULL == nalStart) {
    TRAE("Cannot create the slice param as the given `nalStart` is NULL. This should point to the nal_unit header.");
    return -2;
  }

  if (0 == nalSize) {
    TRAE("Cannot create the slice param as the given `nalSize` is 0. ");
    return -3;
  }

  /* Aliases for better readability */
  sps = ctx->parsed_sps.sps;
  pps = ctx->parsed_pps.pps;
  nal = &ctx->parsed_slice.nal;
  slice = &ctx->parsed_slice.slice;
  sp = &ctx->slice_param;
  
  if (NULL == sps) {
    TRAE("Cannot create the slice param, given `sps` is NULL.");
    return -2;
  }

  if (NULL == pps) {
    TRAE("Cannot create the slice param, given `pps` is NULL.");
    return -3;
  }

  if (NULL == nal) {
    TRAE("Cannot create the slice param, given `nal` is NULL.");
    return -4;
  }

  if (NULL == slice) {
    TRAE("Cannot create the slice param, given `slice` is NULL.");
    return -5;
  }

  sp->slice_data_size = nalSize;
  sp->slice_data_offset = 0;
  sp->slice_data_flag = 0;
  sp->slice_data_bit_offset = ctx->parsed_slice.data_bit_offset;
  sp->first_mb_in_slice = slice->first_mb_in_slice;
  sp->slice_type = slice->slice_type;
  sp->direct_spatial_mv_pred_flag = 0; /* Not supported. */
  sp->num_ref_idx_l0_active_minus1 = slice->num_ref_idx_l0_active_minus1;
  sp->num_ref_idx_l1_active_minus1 = slice->num_ref_idx_l1_active_minus1;
  sp->cabac_init_idc = slice->cabac_init_idc; 
  sp->slice_qp_delta = slice->slice_qp_delta;
  sp->disable_deblocking_filter_idc = slice->disable_deblocking_filter_idc;
  sp->slice_alpha_c0_offset_div2 = slice->slice_alpha_c0_offset_div2;
  sp->slice_beta_offset_div2 = slice->slice_beta_offset_div2;
  sp->luma_log2_weight_denom = 0; /* Not supported. */
  sp->chroma_log2_weight_denom = 0; /* Not supported. */

  {
    static uint8_t did_log = 0;
    if (0 == did_log) {
      TRAE("@todo I have to implement the reference lists and set them on the `VASliceParameterBufferH264` ");
      TRAE("@todo Currently only IDRs are decoded correctly ");
      did_log = 1;
    }
  }

  TRAE("@todo I've hardcoded unsetting of the reference pic list as I'm not sure how to deal with this yet. ");
  
  for (i = 0; i < 32; ++i) {

    sp->RefPicList0[i].flags = VA_PICTURE_H264_INVALID;
    sp->RefPicList0[i].picture_id = VA_INVALID_SURFACE;
    sp->RefPicList0[i].frame_idx = 0;
    sp->RefPicList0[i].TopFieldOrderCnt = 0;
    sp->RefPicList0[i].BottomFieldOrderCnt = 0;
    
    sp->RefPicList1[i].flags = VA_PICTURE_H264_INVALID;
    sp->RefPicList1[i].picture_id = VA_INVALID_SURFACE;
    sp->RefPicList1[i].frame_idx = 0;
    sp->RefPicList1[i].TopFieldOrderCnt = 0;
    sp->RefPicList1[i].BottomFieldOrderCnt = 0;
  }

  /* Update the list0 when this slice reference other frames. */
  if (TRA_SLICE_TYPE_P == sp->slice_type) {

    TRAD("Updating ref_list0 as the current slice is a reference slice.");

    r = dec_update_reference_lists(ctx);
    if (r < 0) {
      TRAE("Failed to update the reference lists.");
      return -6;
    }
    
    {
      static uint8_t did_log = 0;
      if (0 == did_log) {
        TRAE("@todo I think we should be OK by only copying the `short_ref_count` number of entries from the `ref_list0`.");
        did_log = 1;
      }
    }

    /* Make sure we clamp the amount of reference frames. */
    num_refs = (ctx->ref_list0_max > 32) ? 32 : ctx->ref_list0_max;
    memcpy(sp->RefPicList0, ctx->ref_list0, num_refs * sizeof(VAPictureH264));

    /* Mark all other potential references as invalid. */
    for (i = ctx->ref_list0_max; i < 32; ++i) {
      sp->RefPicList0[i].picture_id = VA_INVALID_SURFACE;
      sp->RefPicList0[i].flags = VA_PICTURE_H264_INVALID;
    }
  }

  // vaapi_print_sliceparameterbufferh264(sp);
  
  /* Create the buffer for the slice parameters. */
  status = vaCreateBuffer(
    ctx->va_display,
    ctx->context_id,
    VASliceParameterBufferType,
    sizeof(ctx->slice_param),
    1,
    &ctx->slice_param,
    &param_buf
  );

  if (VA_STATUS_SUCCESS != status) {
    TRAE("Failed to create the buffer for the slice parameter.");
    return -7;
  }

  /* Create the buffer for the slice data */
  status = vaCreateBuffer(
    ctx->va_display,
    ctx->context_id,
    VASliceDataBufferType,
    nalSize,
    1,
    nalStart,
    &data_buf
  );

  if (VA_STATUS_SUCCESS != status) {
    TRAE("Failed to create the buffer for the slice data.");
    return -8;
  }

  /* Send the buffers to the server. */
  render_ids[0] = param_buf;
  render_ids[1] = data_buf;

  status = vaRenderPicture(ctx->va_display, ctx->context_id, render_ids, 2);
  if (VA_STATUS_SUCCESS != status) {
    TRAE("Failed to render the slice param and data.");
    return -9;
  }

  return r;
}

/* ------------------------------------------------------- */

/* 
   See `vaapi.c` where I have a similar function that updates the
   reference frames. The `app->ref_frames` array is where we
   store reference frames during the decode process. Whenever a
   new reference frame is handled (e.g. we check the
   `nal_ref_idc` value, we store the frame into our array in a
   first-in / first-out fashion.

   Then later, we use this array to set the reference frames for
   the `VAPictureParameterBufferH264::ReferenceFrames`. There
   are several things we have to take care of:

   - We only store up to `max_num_ref_frames` as set in the SPS.
   - When the current frame is an IDR we reset the `short_ref_count`.

   TODO:

     @todo Should we pass the `sps` too as it contains the
           max_num_ref_frames; Currently we fetch this value from
           the VAPictureParameterBufferH264, which contains a
           copy ...

 */
static int dec_update_reference_frames(va_dec* ctx) {

  VAPictureParameterBufferH264* pp = NULL;
  uint8_t is_ref = 0;
  uint32_t i = 0;
  
  if (NULL == ctx) {
    TRAE("Cannot update the reference frames as the given `va_dec*` is NULL.");
    return -1;
  }

  pp = &ctx->pic_param;
  
  /* When the current picture is not a reference we don't add it. */
  is_ref = pp->pic_fields.bits.reference_pic_flag;
  if (0 == is_ref) {
    return 0;
  }

  /* Make sure the total number of reference frames never exceeds the maximum. */
  ctx->short_ref_count++;
  if (ctx->short_ref_count > pp->num_ref_frames) {
    ctx->short_ref_count = 0;
  }

  /* Apply the order count type 0 sliding window, first-in / first-out */
  for (i = ctx->short_ref_count; i > 0; i--) {
    ctx->ref_frames[i] = ctx->ref_frames[i - 1];
  }

  /* Store the current picture at index 0. */
  ctx->ref_frames[0] = pp->CurrPic;
  
  pp->CurrPic.flags = VA_PICTURE_H264_SHORT_TERM_REFERENCE;
  
  return 0;
}

/* ------------------------------------------------------- */

/*

  This function updates the reference list0 and list1. The list0
  is used by P and B frames. The list1 is only used by B
  frames. Currently we don't support B frames and only implement
  the list0 functionality. The list0 and list1 are copied into
  the `VASliceParameterBufferH264.RefPicList{0,1}`.

 */
static int dec_update_reference_lists(va_dec* ctx) {

  VASliceParameterBufferH264* sp = NULL;
  
  if (NULL == ctx) {
    TRAE("Cannot update the reference lists as the given `va_dec*` is NULL.");
    return -1;
  }

  sp = &ctx->slice_param;

  /* We need to check the current slice type. */
  if (TRA_SLICE_TYPE_P != sp->slice_type) {
    TRAE("The current slice is not a P slice. We only update the reference lists for P slices.");
    return -2;
  }
  
  /* For P-frames, copy the ref_frames into list0. */
  memcpy(ctx->ref_list0, ctx->ref_frames, ctx->short_ref_count * sizeof(VAPictureH264));

  /* list0 is sorted on `frame_num`. */
  qsort(
    ctx->ref_list0,
    ctx->short_ref_count,
    sizeof(VAPictureH264),
    va_util_compare_h264picture_frame_num
  );
  
  return 0;
}

/* ------------------------------------------------------- */

/* This will map the picture which allows us to "download" it. */

static int dec_map_picture(va_dec* ctx) {

  tra_memory_image decoded_image = { 0 };
  VAStatus status = VA_STATUS_SUCCESS;
  VAImage surface_image = {};
  uint8_t* surface_ptr = NULL;
  uint8_t did_image = 0;
  uint8_t did_map = 0;
  uint32_t i = 0;
  int r = 0;

  if (NULL == ctx) {
    TRAE("Cannot map the picture as the given `va_dec` is NULL.");
    r = -10;
    goto error;
  }

  if (NULL == ctx->callbacks.on_decoded_data) {
    TRAE("Cannot map the picture as the `on_decoded_data` callback is not set.");
    r = -20;
    goto error;
  }

  status = vaSyncSurface(ctx->va_display, ctx->dest_surfaces[ctx->curr_dest_surface_index]);
  if (VA_STATUS_SUCCESS != status) {
    TRAE("Failed to sync the surface.");
    r = -30;
    goto error;
  }

  status = vaDeriveImage(ctx->va_display, ctx->dest_surfaces[ctx->curr_dest_surface_index], &surface_image);
  if (VA_STATUS_SUCCESS != status) {
    TRAE("Failed to derive the image for the current surface.");
    r = -40;
    goto error;
  }

  did_image = 1;

  status = vaMapBuffer(ctx->va_display, surface_image.buf, (void**)&surface_ptr);
  if (VA_STATUS_SUCCESS != status) {
    TRAE("Failed to map the image buffer.");
    r = -50;
    goto error;
  }

  did_map = 1;

  if (NULL == surface_ptr) {
    TRAE("The surface pointer is NULL.");
    r = -60;
    goto error;
  }

  /* Get the image format. */
  r = va_util_map_storage_format_to_image_format(
    surface_image.format.fourcc,
    &decoded_image.image_format
  );

  if (r < 0) {
    TRAE("Cannot convert the VAAPI fourcc into a image format.");
    r = -70;
    goto error;
  }
  
  /*
  if (ctx->storage_format != surface_image.format.fourcc) {
    TRAE("Unhandled image format.");
    r = -6; 
    goto error;
  }
  */

  TRAD("This is where we should call the user!");

  decoded_image.image_width = surface_image.width;
  decoded_image.image_height = surface_image.height;
  decoded_image.plane_count = surface_image.num_planes;

  for (i = 0; i < 3; ++i) {
    decoded_image.plane_data[i] = surface_ptr + surface_image.offsets[i];
    decoded_image.plane_strides[i] = surface_image.pitches[i];
  }

  /* When we have 2 planes, VAAPI wil set the 3rd to the same values as the 2nd one. We don't do that as it's confusing. */
  for (i = decoded_image.plane_count; i < TRA_MAX_IMAGE_PLANES; ++i) {
    decoded_image.plane_data[i] = NULL;
    decoded_image.plane_strides[i] = 0;
  }

  r = ctx->callbacks.on_decoded_data(
    TRA_MEMORY_TYPE_IMAGE,
    &decoded_image,
    ctx->callbacks.user
  );

  if (r < 0) {
    TRAE("The client failed to handle the decoded image.");
    r = -80;
    goto error;
  }

 error:

  if (1 == did_map) {
    status = vaUnmapBuffer(ctx->va_display, surface_image.buf);
    if (VA_STATUS_SUCCESS != status) {
      TRAE("Failed to cleanly unmap a buffer.");
      r -= 9;
    }
  }

  if (1 == did_image) {
    status = vaDestroyImage(ctx->va_display, surface_image.image_id);
    if (VA_STATUS_SUCCESS != status) {
      TRAE("Failed to cleanly destroy the image.");
      r -= 10;
    }
  }

  return r;
}

/* ------------------------------------------------------- */
