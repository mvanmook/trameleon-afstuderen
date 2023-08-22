/* ------------------------------------------------------- */

#include <VideoToolbox/VideoToolbox.h>
#include <tra/modules/vtbox/vtbox-enc.h>
#include <tra/modules/vtbox/vtbox-utils.h>
#include <tra/module.h>
#include <tra/types.h>
#include <tra/log.h>
#include <tra/buffer.h>

/* ------------------------------------------------------- */

struct vt_enc {
  tra_encoder_settings settings;
  VTCompressionSessionRef session;
  tra_buffer* params_buffer; /* Buffer that we use to store the SPS and PPS that we extract using `CMVideoFormatDescriptionGetH264ParameterSetAtIndex()`. */
};

/* ------------------------------------------------------- */

static int vtbox_validate_settings(tra_encoder_settings* cfg);
static int vtbox_create_encoder_params(vt_enc* ctx, CFMutableDictionaryRef* params);
static int vtbox_create_image_buffer_params(vt_enc* ctx, CFMutableDictionaryRef* params);
static int vtbox_create_pixel_buffer(vt_enc* ctx, CVPixelBufferRef* pixelBuffer);
static int vtbox_set_allow_frame_reordering(vt_enc* ctx, uint8_t isAllowed);
static int vtbox_is_key_frame(vt_enc* ctx, CMSampleBufferRef buffer, uint8_t* isKeyFrame);
static int vtbox_extract_parameter_sets(vt_enc* ctx, CMSampleBufferRef buffer);
static void vtbox_on_encoded_frame(void* callbackUserData, void* frameUserData, OSStatus status, VTEncodeInfoFlags flags, CMSampleBufferRef sampleBuffer);

/* ------------------------------------------------------- */

int vt_enc_create(tra_encoder_settings* cfg, vt_enc** ctx) {

  CFMutableDictionaryRef enc_params = NULL;  /* Encoder specification dictionary */
  CFMutableDictionaryRef img_params = NULL;  /* Source image buffer attributes. */
  OSStatus status = noErr;
  vt_enc* inst = NULL;
  int r = 0;

  TRAE("@todo We should create a `tra_image_setup_for_image_format()` function or similar which sets some defaults based on the given image format; like plane_strides, plane_heights, etc. ");
  TRAE("@todo describe how to return a value from a destroy function. Use `r` for function returns and use `result` as  the final result.");

  if (NULL == cfg) {
    TRAE("Cannot create the `vt_enc` as the given `tra_encoder_settings*` is NULL.");
    r = -10;
    goto error;
  }

  r = vtbox_validate_settings(cfg);
  if (r < 0) {
    TRAE("Cannot create `vt_enc` as the given settings are invalid.");
    r = -20;
    goto error;
  }

  inst = calloc(1, sizeof(vt_enc));
  if (NULL == inst) {
    TRAE("Cannot create the `vt_enc` we failed to allocate the `vt_enc` instance.");
    r = -30;
    goto error;
  }

  inst->settings = *cfg;

  r = tra_buffer_create(32, &inst->params_buffer);
  if (r < 0) {
    TRAE("Cannot create the `vt_enc` as we failed to create the buffer for the parameter sets. ");
    r = -40;
    goto error;
  }

  /* Create the dictionary with the encoder configuration */
  r = vtbox_create_encoder_params(inst, &enc_params);
  if (r < 0) {
    TRAE("Cannot create the `vt_enc` as we failed to create the encoder params.");
    r = -50;
    goto error;
  }

  /* Create the dictionary with the image buffer params; VT uses this to create an optimised buffer pool. */
  r = vtbox_create_image_buffer_params(inst, &img_params);
  if (r < 0) {
    TRAE("Cannot create the `vt_enc` as we failed to create the image buffer attributes.");
    r = -60;
    goto error;
  }
  
  status = VTCompressionSessionCreate(
    NULL,                   /* The allocator for the session, NULL means that we use the default. */
    cfg->image_width,       /* The width of the input pictures. */
    cfg->image_height,      /* The  height of the input pictures. */
    kCMVideoCodecType_H264, /* The codec type */
    enc_params,             /* Encoder specifications */
    img_params,             /* Image buffer attributes; used to create a pixel buffer pool. */
    NULL,                   /* Allocator for compressed data; NULL for default. */
    vtbox_on_encoded_frame, /* Output callback; can be called asynchronously from a different thread. */
    inst,                   /* User data for callback. */
    &inst->session          /* The created session. */
  );

  if (noErr != status) {
    TRAE("Cannot create the `vt_enc`, failed to create the compression session: %d", status);
    r = -70;
    goto error;
  }

  /* Disable B-frames */
  r = vtbox_set_allow_frame_reordering(inst, 0);
  if (r < 0) {
    TRAE("Cannot create the `vt_enc`, failed to disable frame reordering.");
    r = -80;
    goto error;
  }

  status = VTCompressionSessionPrepareToEncodeFrames(inst->session);
  if (noErr != status) {
    TRAE("Cannot create the `vt_enc` as we failed to prepare the session to start encoding.");
    r = -90;
    goto error;
  }

  *ctx = inst;

 error:

  if (r < 0) {
    
    if (NULL != inst) {
      vt_enc_destroy(inst);
      inst = NULL;
    }

    if (NULL != ctx) {
      *ctx = NULL;
    }

  }

  if (NULL != enc_params) {
    CFRelease(enc_params);
    enc_params = NULL;
  }

  if (NULL != img_params) {
    CFRelease(img_params);
    img_params = NULL;
  }

  return r;
}

/* ------------------------------------------------------- */

/* 

   This function will deallocate the compression session. This
   will flush any outstanding frames before cleaning up.

 */
int vt_enc_destroy(vt_enc* ctx) {

  OSStatus status = noErr;
  int result = 0;
  int r = 0;

  if (NULL == ctx) {
    TRAE("Cannot destroy the `vt_enc` as it's NULL.");
    return -1;
  }

  /* Tear down the encoder session. */
  if (NULL != ctx->session) {

    status = VTCompressionSessionCompleteFrames(ctx->session, kCMTimeInvalid);
    if (noErr != status) {
      TRAE("Cannot cleanly destroy the compression session as we failed to complete any outstanding frames.");
      result -= 20;
    }

    VTCompressionSessionInvalidate(ctx->session);
    CFRelease(ctx->session);
  }

  /* Cleanup the buffer that we use to store the parameters sets (SPS, PPS) */
  if (NULL != ctx->params_buffer) {
    r = tra_buffer_destroy(ctx->params_buffer);
    if (r < 0) {
      TRAE("Cannot cleanly destroy the buffer that we use to store the parameter sets.");
      result -= 30;
    }
  }

  ctx->session = NULL;
  ctx->params_buffer = NULL;

  free(ctx);
  ctx = NULL;

  return result;
}

/* ------------------------------------------------------- */

/*

  GENERAL INFO:

    Feed a raw image into the encoder. Because we setup the
    compression session to use it's own internal pixel buffer
    pool (which is advised and probably gives us the best
    performance), we use the
    `VTCompressionSessionGetPixelBufferPool()` function to
    retrieve a pool. We use this pool to provide the data that is
    passed into this function.

  REFERENCES:

    [0]: https://developer.apple.com/documentation/videotoolbox/1428293-vtcompressionsessiongetpixelbuff?language=objc "VTCompressionSessionGetPixelBufferPool"

 */
int vt_enc_encode(
  vt_enc* ctx,
  tra_sample* sample,
  uint32_t type,
  void* data
)
{
  OSStatus os_status = noErr;
  CVReturn cv_status = kCVReturnSuccess;

  CVPixelBufferRef pix_buf = NULL;
  uint32_t image_format = 0;
  OSType pix_format = 0;
  
  tra_image* src_image = NULL;
  uint8_t* src_ptr = NULL;
  uint32_t src_stride = 0;
  uint32_t src_height = 0;
  uint32_t plane_stride = 0;
  uint32_t plane_height = 0;
  uint8_t* plane_ptr = NULL;
  size_t plane_count = 0;
  uint32_t w = 0;
  uint32_t h = 0;
  uint32_t i = 0;
  uint32_t j = 0;
  int r = 0;

  /* @todo Currently I've hardcoded the framerate here! We might want to make this part of the `tra_encoder_settings`; e.g. fps_num and fps_den. See `research-api.org`. */
  CMTime duration = CMTimeMake(25, 1);
  CMTime pts = { 0 };

  /* --------------------------------------- */
  /* VALIDATE                                */
  /* --------------------------------------- */

  if (NULL == ctx) {
    TRAE("Cannot encode as the given `vt_enc*` is NULL.");
    r = -10;
    goto error;
  }
  
  if (NULL == ctx->session) {
    TRAE("Cannot encode, the given `vt_enc::session` is NULL.");
    r = -20;
    goto error;
  }

  if (NULL == sample) {
    TRAE("Cannot encode as the given `tra_sample*` is NULL.");
    r = -30;
    goto error;
  }

  if (TRA_MEMORY_TYPE_HOST_IMAGE != type) {
    TRAE("Cannot encode as the given input type is not `TRA_MEMORY_TYPE_HOST_IMAGE`. Currently we only support this type.");
    r = -40;
    goto error;
  }

  if (NULL == data) {
    TRAE("Cannot encode as the given `data` is NULL.");
    r = -50;
    goto error;
  }

  /* Make sure the given image is valid. */
  src_image = (tra_image*) data;
  w = src_image->image_width;
  h = src_image->image_height;
  
  if (w != ctx->settings.image_width) {
    TRAE("Cannot encode with `vtboxenc` as the input image width (%u) is not what we would expect (%u).", w, ctx->settings.image_width);
    r = -60;
    goto error;
  }

  if (h != ctx->settings.image_height) {
    TRAE("Cannot encode with `vtboxenc` as the iunput image height (%u) is not what we would expect (%u).", h, ctx->settings.image_height);
    r = -70;
    goto error;
  }

  if (ctx->settings.image_format != src_image->image_format) {
    TRAE("Cannot encode with `vtboxenc` as the given input image has a different image format (%s) than what we would expect (%s)",  tra_imageformat_to_string(src_image->image_format), tra_imageformat_to_string(ctx->settings.image_format));
    r = -80;
    goto error;
  }

  /* --------------------------------------- */
  /* UPLOAD YUV                              */
  /* --------------------------------------- */

  r = vtbox_create_pixel_buffer(ctx, &pix_buf);
  if (r < 0) {
    TRAE("Cannot encode, failed to create a pixel buffer.");
    r = -80;
    goto error;
  }

  plane_count = CVPixelBufferGetPlaneCount(pix_buf);
  if (plane_count != src_image->plane_count) {
    TRAE("The plane count of the `CVPixelBuffer` (%zu) is different than the given image (%u); we expect them to be the same.", plane_count, src_image->plane_count);
    r = -85;
    goto error;
  }

  /* Make sure the image format of the CVPixelBuffer matches the format of the given image. */
  pix_format = CVPixelBufferGetPixelFormatType(pix_buf);

  r = vtbox_cvpixelformat_to_imageformat(pix_format, &image_format);
  if (r < 0) {
    TRAE("Cannot encode, we failed to convert the CVPixelBufferFormatType into a Trameleon image format.");
    r = -90;
    goto error;
  }

  if (image_format != src_image->image_format) {
    TRAE("Cannot encode, the pixel format type of the CVPixelBuffer is different than the received image (%s). They should match.", tra_imageformat_to_string(src_image->image_format));
    r = -100;
    goto error;
  }
  
  /* Copy the input data */
  cv_status = CVPixelBufferLockBaseAddress(pix_buf, 0);
  if (kCVReturnSuccess != cv_status) {
    TRAE("Cannot encode, failed to lock the base address of the pixel buffer that we use to feed raw yuv into the encoder.");
    r = -5;
    goto error;
  }

  for (i = 0; i < src_image->plane_count; ++i) {
  
    plane_ptr = (uint8_t*) CVPixelBufferGetBaseAddressOfPlane(pix_buf, i);
    if (NULL == plane_ptr) {
      TRAE("Failed to get the base ptr of plane %u.", i);
      r = -6;
      goto error;
    }

    plane_stride = CVPixelBufferGetBytesPerRowOfPlane(pix_buf, i);
    if (0 == plane_stride) {
      TRAE("Failed to get the stride for plane %u.", i);
      r = -7;
      goto error;
    }

    plane_height = CVPixelBufferGetHeightOfPlane(pix_buf, i);
    if (0 == plane_height) {
      TRAE("Failed to get the height for plane %u.", i);
      r = -8;
      goto error;
    }

    src_ptr = src_image->plane_data[i];
    src_stride = src_image->plane_strides[i];
    src_height = src_image->plane_heights[i];

    if (src_height != plane_height) {
      TRAE("Cannot encode, the plane height of the given `tra_image` should match the height of the CVPixelBuffer.");
      r = -90;
      goto error;
    }
    
    for (j = 0; j < plane_height; ++j) {
      memcpy(plane_ptr, src_ptr, plane_stride);
      plane_ptr = plane_ptr + plane_stride;
      src_ptr = src_ptr + src_stride;
    }
  }

  cv_status = CVPixelBufferUnlockBaseAddress(pix_buf, 0);
  if (kCVReturnSuccess != cv_status) {
    TRAW("Something might go wrong while encoding. We locked a CVPixelBuffer but failed to unlock it.");
    /* fall through; never encountered this issue. */
  }

  pts = CMTimeMultiply(duration, sample->pts);

  os_status = VTCompressionSessionEncodeFrame(
    ctx->session,
    pix_buf,
    pts,        /* `presentationTimeStamp`: must be greater than the previous one. */
    duration,   /* duration of this frame. */
    NULL,       /* `frameProperties`: additional settings that can be used during encoding. */
    pix_buf,    /* `sourceFrameRefCon`: The reference value for the frame, which is passed to the output callback. */
    NULL        /* `infoFlagsOut`: A pointer to `VTEncodeInfoFlags` to receive information about the encode operation. */
  );

  {
    static uint8_t did_log = 0;
    if (0 == did_log) {
      TRAE("@todo do we need to release the pix_buf here?");
      did_log = 1;
    }
  }
  
 error:

  return r;
}

/* ------------------------------------------------------- */

static int vtbox_validate_settings(tra_encoder_settings* cfg) {
  
  if (NULL == cfg) {
    TRAE("Cannot validate the given settings as they are NULL.");
    return -10;
  }

  if (0 == cfg->image_width) {
    TRAE("`tra_encoder_settings::image_width` is 0.");
    return -20;
  }

  if (0 == cfg->image_height) {
    TRAE("`tra_encoder_settings::image_height` is 0.");
    return -30;
  }

  if (0 == cfg->image_format) {
    TRAE("`tra_encoder_settings::image_format` is 0.");
    return -40;
  }

  if (NULL == cfg->callbacks.on_encoded_data) {
    TRAE("`tra_encoder_settings::callbacks::on_encoded_data` is NULL.");
    return -50;
  }

  return 0;
}

/* ------------------------------------------------------- */

/*

  GENERAL INFO:

    Create the encoder specification dictionary to configure the
    encoder.  Currently we're not setting much. We make sure that
    hardware acceleration is required.

  REFERENCES:

    [0] VTCompressionSessionProperties.h

*/
static int vtbox_create_encoder_params(vt_enc* ctx, CFMutableDictionaryRef* params) {

  CFMutableDictionaryRef dict = NULL;
  int r = 0;

  if (NULL == ctx) {
    TRAE("Cannot create the encoder params ass the given `vt_enc*` is NULL.");
    r = -1;
    goto error;
  }
  
  if (NULL == params) {
    TRAE("Cannot create the encoder params as the given `params` is NULL.");
    r = -2;
    goto error;
  }

  if (NULL != (*params)) {
    TRAE("Cannot create the encoder params as the given output dictionary is not NULL. Already created?");
    r = -3;
    goto error;
  }

  dict = CFDictionaryCreateMutable(
    kCFAllocatorDefault,
    3,
    &kCFTypeDictionaryKeyCallBacks,
    &kCFTypeDictionaryValueCallBacks
  );


  if (NULL == dict) {
    TRAE("Cannot create the encoder params, we failed to create the dictionary for the encoder params.");
    r = -2;
    goto error;
  }

  /* Make sure that hardware encoding is used. */
  CFDictionaryAddValue(
    dict,
    kVTVideoEncoderSpecification_RequireHardwareAcceleratedVideoEncoder,
    kCFBooleanTrue
  );

  *params = dict;

 error:

  if (r < 0) {
    
    if (NULL != dict) {
      CFRelease(dict);
      dict = NULL;
    }

    if (NULL != params) {
      *params = NULL;
    }
  }

  return r;
}

/* ------------------------------------------------------- */

/* 

   This creates the dictionary that contains information about
   the source images that we're encoding. This is passed into the
   `VTCompressionSessionCreate()` function. By passing this
   parameter into the create function, we ask the encoder to
   allocate the buffer pool in an optimised way. The
   documentation says "Using pixel buffers not allocated by
   VideoToolbox increases the chance that you'll have to copy
   image data.".

 */
static int vtbox_create_image_buffer_params(vt_enc* ctx, CFMutableDictionaryRef* params) {

  CFMutableDictionaryRef dict = NULL;
  CFNumberRef width_ref = NULL;
  CFNumberRef height_ref = NULL;
  int r = 0;
  
  if (NULL == ctx) {
    TRAE("Cannot create the image buffer params as the given `vt_enc*` is NULL.");
    r = -1;
    goto error;
  }
  
  if (NULL == params) {
    TRAE("Cannot create the image buffer params as the given `params` is NULL.");
    r = -2;
    goto error;
  }

  if (NULL != (*params)) {
    TRAE("Cannot create the image buffer params as the given `*params` is not NULL. Already created?");
    r = -3;
    goto error;
  }

  r = vtbox_validate_settings(&ctx->settings);
  if (r < 0) {
    TRAE("Cannot create the image buffer params as the `settings` member of `vt_enc` is invalid.");
    r = -4;
    goto error;
  }

  dict = CFDictionaryCreateMutable(
    kCFAllocatorDefault,
    2,
    &kCFTypeDictionaryKeyCallBacks,
    &kCFTypeDictionaryValueCallBacks
  );

  if (NULL == dict) {
    TRAE("Cannot create the image buffer params as we failed to create the dictionary.");
    r = -5;
    goto error;
  }

  width_ref = CFNumberCreate(NULL, kCFNumberSInt32Type, &ctx->settings.image_width);
  if (NULL == width_ref) {
    TRAE("Cannot create the image buffer params as we failed to create a number ref for the width.");
    r = -4;
    goto error;
  }
  
  height_ref = CFNumberCreate(NULL, kCFNumberSInt32Type, &ctx->settings.image_height);
  if (NULL == height_ref) {
    TRAE("Cannot create the image buffer params as we failed to create a number ref for the height.");
    r = -5;
    goto error;
  }

  CFDictionaryAddValue(dict, kCVPixelBufferWidthKey, width_ref);
  CFDictionaryAddValue(dict, kCVPixelBufferHeightKey, height_ref);

  /* Finally assign. */
  *params = dict;

 error:

  if (r < 0) {
    if (NULL != dict) {
      CFRelease(dict);
      dict = NULL;
    }

    if (NULL != params) {
      *params = NULL;
    }
  }

  if (NULL != width_ref) {
    CFRelease(width_ref);
    width_ref = NULL;
  }
  
  if (NULL != height_ref) {
    CFRelease(height_ref);
    height_ref = NULL;
  }
  
  return r;
}

/* ------------------------------------------------------- */

static int vtbox_create_pixel_buffer(
  vt_enc* ctx,
  CVPixelBufferRef* pixelBuffer
)
{
  CVReturn status = kCVReturnSuccess;
  CVPixelBufferRef pix_buf = NULL;
  CVPixelBufferPoolRef pix_buf_pool = NULL;
  int r = 0;
  
  if (NULL == ctx) {
    TRAE("Cannot create a pixel buffer because the given `vt_enc*` is NULL.");
    r = -1;
    goto error;
  }

  if (NULL == pixelBuffer) {
    TRAE("Cannot create a pixel buffer because the given `pixelBuffer` is NULL.");
    r = -2;
    goto error;
  }
  
  /* We default to NULL. */
  *pixelBuffer = NULL;
   
  pix_buf_pool = VTCompressionSessionGetPixelBufferPool(ctx->session);
  if (NULL == pix_buf_pool) {
    TRAE("Cannot create a pixel buffer because we failed to retrieve a reference to the pixel buffer pool of the encoder session.");
    r = -3;
    goto error;
  }

  /* Create the pixel buffer in the of the encoder session. */
  status = CVPixelBufferPoolCreatePixelBuffer(
    NULL,
    pix_buf_pool,
    &pix_buf
  );

  if (kCVReturnSuccess != status) {
    TRAE("Cannot create a pixel buffer as we failed to create one in the pool.");
    r = -4;
    goto error;
  }

  /* Assign the result */
  *pixelBuffer = pix_buf;

 error:

  return r;
}

/* ------------------------------------------------------- */

/*
  This function sets the value for
  `kVTCompressionPropertyKey_AllowFrameReordering` that can be
  used to e.g. disable or enable B-frames.

  @param ctx (vt_enc*)
  @apram isAllowed (uint8_t) 1 = yes, 0 = no

 */
static int vtbox_set_allow_frame_reordering(vt_enc* ctx, uint8_t isAllowed) {

  OSStatus status = noErr;
  int r = 0;
  
  if (NULL == ctx) {
    TRAE("Cannot set the if frame reordering is allowed as the given `vt_enc*` is NULL.");
    r = -1;
    goto error;
  }

  if (NULL == ctx->session) {
    TRAE("Cannot set if frame reordering is allowed as the `vt_enc::session` member is NULL.");
    r = -2;
    goto error;
  }

  status = VTSessionSetProperty(
    ctx->session,
    kVTCompressionPropertyKey_AllowFrameReordering,
    (1 == isAllowed) ? kCFBooleanTrue : kCFBooleanFalse
  );

  if (noErr != status) {
    TRAE("Faield to set if reordering of frames is allowed or not.");
    r = -3;
    goto error;
  }

 error:
  return r;
}

/* ------------------------------------------------------- */

static int vtbox_is_key_frame(vt_enc* ctx, CMSampleBufferRef buffer, uint8_t* isKeyFrame) {

  CFArrayRef attachments = NULL;
  CFBooleanRef depends = NULL;
  CFDictionaryRef dict = NULL;
  int r = 0;
  
  if (NULL == buffer) {
    TRAE("Cannot check if the buffer holds a keyframe as it's NULL.");
    r = -1;
    goto error;
  }

  if (NULL == isKeyFrame) {
    TRAE("Cannot check if the buffer holds a keyframne as the given result `iskeyFrame` is NULL.");
    r = -2;
    goto error;
  }

  /* Default to false. */
  *isKeyFrame = 0;

  /* No attachments; so not a key frame. */
  attachments = CMSampleBufferGetSampleAttachmentsArray(buffer, false);
  if (NULL == attachments)  {
    r = 0;
    goto error;
  }
  
  dict = (CFDictionaryRef) CFArrayGetValueAtIndex(attachments, 0);
  if (NULL == dict) {
    TRAE("Cannot check if the buffer holds a keyframe, failed to get the dictionary from the sample buffer attachment array.");
    r = -3;
    goto error;
  }
  
  depends = (CFBooleanRef) CFDictionaryGetValue(dict, kCMSampleAttachmentKey_DependsOnOthers);
  if (NULL == depends) {
    TRAE("Cannot check if the buffer holds a keyframe, failed to get the `kCMSampleAttachmentKey_DependsOnOthers` key; cannot check if the given CMSampleBufferRef is a keyframe.");
    r = -4;
    goto error;
  }
  
  *isKeyFrame = (depends == kCFBooleanTrue) ? 0 : 1;
  
 error:
  return r;
}

/* ------------------------------------------------------- */

/* 

   GENERAL INFO:

     This function uses the
     `CMVideoFormatDescriptionGetH264ParameterSetAtIndex()`
     function to create a buffer with one or more SPS and PPS
     instances. To extract these parameter sets we have to first
     check how many there are stored and then extract the mone by
     one. To get the number of parameter sets we use the same
     function.
   
   REFERENCES:

     [0]: https://developer.apple.com/documentation/coremedia/1489529-cmvideoformatdescriptiongeth264p?language=objc

 */
static int vtbox_extract_parameter_sets(vt_enc* ctx, CMSampleBufferRef buffer) {

  CMFormatDescriptionRef format_ref = NULL;
  uint8_t annexb_header[] = { 0x00, 0x00, 0x00, 0x01 }; /* @todo remove; only used to write a annexb header during dev. */
  const uint8_t* param_ptr = NULL;
  int nal_header_length = 0;
  OSStatus status = noErr;
  size_t param_count = 0;
  size_t param_size = 0;
  size_t i = 0;
  int r = 0;

  if (NULL == ctx) {
    TRAE("Cannot extract parameter sets as the given `vt_enc*` is NULL.");
    r = -10;
    goto error;
  }

  if (NULL == buffer) {
    TRAE("Cannot extract parameter sets as the given `CMSampleBufferRef` is NULL.5~");
    r = -20;
    goto error;
  }

  r = tra_buffer_reset(ctx->params_buffer);
  if (r < 0) {
    TRAE("Cannot extract the parameter sets nas we failed to reset the buffer into which we store them.");
    r = -30;
    goto error;
  }

  format_ref = CMSampleBufferGetFormatDescription(buffer);
  if (NULL == format_ref) {
    TRAE("Cannot extract parameter sets; we failed to get the format description from the sample buffer.");
    r = -40;
    goto error;
  }

  /* Step 1: Check how many parameter sets we have to extract. */
  status = CMVideoFormatDescriptionGetH264ParameterSetAtIndex(
    format_ref,
    0,
    NULL,
    NULL,
    &param_count,
    &nal_header_length
  );

  if (noErr != status) {
    TRAE("Cannot extract the parameter sets; we failed to extract the number of available parameter sets.");
    r = -50;
    goto error;
  }

  if (0 == param_count) {
    TRAE("Cannot extra the parameter sets; the parameter count is zero.");
    r = -60;
    goto error;
  }

  if (4 != nal_header_length) {
    TRAE("Cannot extract the parameter sets; we expect the nal length to be 4. We got: %lu for the sps.", nal_header_length);
    r = -70;
    goto error;
  }

  /* Step 2: Extract each of the parameter sets and append annex-b headers. */
  for (i = 0; i < param_count; ++i) {
    
    status = CMVideoFormatDescriptionGetH264ParameterSetAtIndex(
      format_ref,
      i,
      &param_ptr,
      &param_size,
      NULL,
      NULL
    );
  
    if (noErr != status) {
      TRAE("Cannot extract the parameter sets; failed to retrieve the SPS.");
      r = -80;
      goto error;
    }

    if (0 == param_size) {
      TRAE("Cannot extract the parameter set, the returned size is zero.");
      r = -90;
      goto error;
    }

    /* First append the annex-b */
    r = tra_buffer_append_bytes(ctx->params_buffer, 4, annexb_header);
    if (r < 0) {
      TRAE("Cannot extract the parameter sets; failed to append the annex-b header to the buffer.");
      r = -100;
      goto error;
    }

    /* Append the parameter set. */
    r = tra_buffer_append_bytes(ctx->params_buffer, param_size, param_ptr);
    if (r < 0) {
      TRAE("Cannot extract the parameter sets; failed to append the parameter set.");
      r = -110;
      goto error;
    }
  }
  
 error:
  
  if (r < 0) {

    /* When an error occured we reset the buffer. */
    if (NULL != ctx
        && NULL != ctx->params_buffer)
      {
        tra_buffer_reset(ctx->params_buffer);
      }
  }
  
  return r;
}

/* ------------------------------------------------------- */

static void vtbox_on_encoded_frame(
  void* callbackUserData,
  void* frameUserData,
  OSStatus status,
  VTEncodeInfoFlags flags,
  CMSampleBufferRef sampleBuffer
)
{
  tra_encoded_host_memory encoded_data = { 0 };
  tra_encoder_callbacks* callbacks = NULL;
  CVPixelBufferRef pix_buf = (CVPixelBufferRef) frameUserData;
  vt_enc* ctx = (vt_enc*) callbackUserData;
  CMBlockBufferRef block_ref = NULL;
  Boolean is_contiguous = false;
  uint8_t is_key_frame = 0;

  uint8_t* block_ptr = NULL;
  size_t parsed_nbytes = 0;
  size_t block_nbytes = 0;

  uint8_t* nal_ptr = NULL;
  uint32_t nal_size = 0;
  uint8_t* nal_size_ptr = (uint8_t*)&nal_size;
  int r = 0;

  if (noErr != status) {
    TRAE("Cannot handle encoded frame, status holds an error value. Something went wrong while encoding a frame.");
    r = -10;
    goto error;
  }

  if (NULL == sampleBuffer) {
    TRAE("Cannot handle encoded frame, `sampleBuffer` is NULL.");
    r = -20;
    goto error;
  }

  if (NULL == ctx) {
    TRAE("Cannot handle encoded frame, `callbackUserData` is not an instance of `vt_enc`.");
    r = -30;
    goto error;
  }

  callbacks = &ctx->settings.callbacks;
  if (NULL == callbacks->on_encoded_data) {
    TRAE("Cannot handle encoded frame, `tra_encoder_callbacks::on_encoded_data` is NULL.");
    r = -50;
    goto error;
  }

  block_ref = CMSampleBufferGetDataBuffer(sampleBuffer);
  if (NULL == block_ref) {
    TRAE("Cannot handle encoded frame, failed to get the CMBlockBufferRef from the CMSampleBufferRef");
    r = -60;
    goto error;
  }

  is_contiguous = CMBlockBufferIsRangeContiguous(block_ref, 0, 0);
  if (false == is_contiguous) {
    TRAE("Cannot handle encoded frame, we expect the block buffer to be contiguous.");
    r = -70;
    goto error;
  }

  status = CMBlockBufferGetDataPointer(block_ref, 0, NULL, &block_nbytes, (char**)&block_ptr);
  if (noErr != status) {
    TRAE("Cannot handle encoded frame, failed to get a pointer to the buffer data.");
    r = -80;
    goto error;
  }

  r = vtbox_is_key_frame(ctx, sampleBuffer, &is_key_frame);
  if (r < 0) {
    TRAE("Cannot handle encode frame, failed to check if the `CMSampleBufferRef` contains a key frame.");
    r = -90;
    goto error;
  }

  if (1 == is_key_frame) {
    
    r = vtbox_extract_parameter_sets(ctx, sampleBuffer);
    if (r < 0) {
      TRAE("Cannot handle encoded frame, failed to extract the parameter sets.");
      r = -100;
      goto error;
    }

    /* Notify the client and pass the SPS/PPS */
    encoded_data.data = ctx->params_buffer->data;
    encoded_data.size = ctx->params_buffer->size;

    callbacks->on_encoded_data(
      TRA_MEMORY_TYPE_HOST_H264,
      &encoded_data,
      callbacks->user
    );
  }

  /* Convert from AVCC to annex-b and call the client */
  nal_ptr = block_ptr;
  
  while (parsed_nbytes < block_nbytes) {

    nal_size_ptr[3] = nal_ptr[0];
    nal_size_ptr[2] = nal_ptr[1];
    nal_size_ptr[1] = nal_ptr[2];
    nal_size_ptr[0] = nal_ptr[3];

    nal_ptr[0] = 0x00;
    nal_ptr[1] = 0x00;
    nal_ptr[2] = 0x00;
    nal_ptr[3] = 0x01;

    encoded_data.data = nal_ptr;
    encoded_data.size = nal_size + 4; 

    callbacks->on_encoded_data(
      TRA_MEMORY_TYPE_HOST_H264,
      &encoded_data,
      callbacks->user
    );
    
    nal_ptr += nal_size + 4;
    parsed_nbytes += nal_size + 4;
  }

 error:

  if (NULL != pix_buf) {
    CFRelease(pix_buf);
    pix_buf = NULL;
  }
}

/* ------------------------------------------------------- */


