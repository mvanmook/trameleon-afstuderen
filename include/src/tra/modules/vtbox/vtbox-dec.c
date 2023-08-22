/* ------------------------------------------------------- */

#include <VideoToolbox/VideoToolbox.h>
#include <tra/modules/vtbox/vtbox-dec.h>
#include <tra/modules/vtbox/vtbox-utils.h>
#include <tra/module.h>
#include <tra/types.h>
#include <tra/avc.h>
#include <tra/log.h>

/* ------------------------------------------------------- */

typedef struct vt_dec             vt_dec;
typedef struct vt_dec_create_info vt_dec_create_info;

/* ------------------------------------------------------- */

struct vt_dec {
  tra_decoder_settings settings;
  CMVideoFormatDescriptionRef format_description; /* The struct which describes the encoded bitstream; e.g. the width, height and pixel format. This is created from the SPS and PPS. We need to keep track of the current format description when we create CMSampleBuffers that we feed into the decoder. */
  VTDecompressionSessionRef session;
};

/* ------------------------------------------------------- */

/* 
   The `vt_dec_create_info` is used to create the decoder session. 
   See `vtbox_create_decoder_session()` for more info. 
*/
struct vt_dec_create_info {
  uint8_t* sps_data;
  uint32_t sps_size;
  uint8_t* pps_data;
  uint32_t pps_size;
};

/* ------------------------------------------------------- */

static int vtbox_validate_create_info(vt_dec_create_info* info);
static int vtbox_create_decoder_session(vt_dec* ctx, vt_dec_create_info* info); /* When we receive a SPS/PPS in `vt_dec_decode()` we will create a decoder session. */
static int vtbox_create_decoder_config(vt_dec* ctx, CFMutableDictionaryRef* dictOut); /* Creates the CFMutableDictionary that is used for the `videoDecoderSpecification` argument. */
static int vtbox_create_format_config(vt_dec* ctx, vt_dec_create_info* info); /* This create the `CMVideoFormatDescriptionRef` from the SPS and PPS which are set in the create info. This format description is used when we start feeding coded data to the decoder. */
static int vtbox_create_image_config(vt_dec* ctx, CFMutableDictionaryRef* dictOut);
static int vtbox_create_sample_buffer(vt_dec* ctx, uint8_t* data, uint32_t nbytes, CMSampleBufferRef* sampleOut);
static void vtbox_on_decoded_frame(void *user, void *ref, OSStatus status, VTDecodeInfoFlags flags, CVImageBufferRef buffer, CMTime pts, CMTime duration); /* Is called by the decoder w/e it has a decoded frame. */

/* ------------------------------------------------------- */

int vt_dec_create(tra_decoder_settings* cfg, vt_dec** ctx) {

  vt_dec* inst = NULL;
  int r = 0;

  if (NULL == cfg) {
    TRAE("Cannot create the `vt_dec` instance as the given `tra_decoder_settings` are NULL.");
    r = -1;
    goto error;
  }

  if (NULL == ctx) {
    TRAE("Cannot create the `vt_dec` instance as the given output argument is NULL.");
    r = -2;
    goto error;
  }

  if (0 == cfg->image_width) {
    TRAE("Cannot create the `vt_dec` as the given `tra_decoder_settings::image_width` is 0.");
    r = -3;
    goto error;
  }

  if (0 == cfg->image_height) {
    TRAE("Cannot create the `vt_dec` as the given `tra_decoder_settings::image_height` is 0.");
    r = -4;
    goto error;
  }

  TRAE("@todo We don't really need the width/height; though other implementations might need it; e.g. the vaapi or windows foundation.");

  inst = calloc(1, sizeof(vt_dec));
  if (NULL == inst) {
    TRAE("Cannot create the `vt_dec` instance; we failed to allocate it.");
    r = -2;
    goto error;
  }

  inst->settings = *cfg;

  *ctx = inst;

 error:

  if (r < 0) {
    
    if (NULL != inst) {
      vt_dec_destroy(inst);
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

  This function will deallocate the given `vt_dec*` and tears
  down the decompression session when it has been created. We
  will first flush any frames which are in the decoder
  buffers. Calling this function might trigger the decoded frame
  callback.

 */
int vt_dec_destroy(vt_dec* ctx) {

  OSStatus status = noErr;
  int result = 0;
  
  if (NULL == ctx) {
    TRAE("Cannot destroy the `vt_dec` as it's NULL.");
    return -1;
  }

  /* Tear down the compression session. */
  if (NULL != ctx->session) {

    status = VTDecompressionSessionWaitForAsynchronousFrames(ctx->session);
    if (noErr != status) {
      TRAE("Cannot cleanly destroy the `vt_dec`, we failed to wait for the asynchronous frames.");
      result -= 2;
    }

    status = VTDecompressionSessionFinishDelayedFrames(ctx->session);
    if (noErr != status) {
      TRAE("Cannot cleanly destroy the `vt_dec`, we failed to finish the delayed frames.");
      result -= 3;
    }

    VTDecompressionSessionInvalidate(ctx->session);
    CFRelease(ctx->session);
  }

  if (NULL != ctx->format_description) {
    CFRelease(ctx->format_description);
  }

  ctx->session = NULL;
  ctx->format_description = NULL;

  free(ctx);
  ctx = NULL;
  
  return result;
}

/* ------------------------------------------------------- */

int vt_dec_decode(vt_dec* ctx, uint32_t type, void* data) {

  tra_encoded_host_memory* host_mem = NULL;
  CMSampleBufferRef sample_buffer = NULL;
  vt_dec_create_info create_info = { 0 } ;
  VTDecodeFrameFlags flags_decode = 0;
  VTDecodeInfoFlags flags_out = 0;
  uint32_t parsed_nbytes = 0;
  OSStatus status = noErr;
  uint8_t* nal_start = NULL;
  uint32_t nal_size = 0;
  uint8_t nal_type = 0;
  uint8_t* data_ptr = NULL;
  uint32_t data_size = 0;
  int r = 0;

  if (NULL == ctx) {
    TRAE("Cannot decode as the given `vt_dec*` is NULL.");
    r = -10;
    goto error;
  }

  if (TRA_MEMORY_TYPE_HOST_H264 != type) {
    TRAE("Cannot decode as the given `type` is not supported.");
    r = -20;
    goto error;
  }

  if (NULL == data) {
    TRAE("Cannot decode as the given `data` is NULL.");
    r = -30;
    goto error;
  }

  host_mem = (tra_encoded_host_memory*) data;
  if (NULL == host_mem->data) {
    TRAE("Cannot decode as the `data` member of the `tra_encoded_host_memory` is NULL.");
    r = -40;
    goto error;
  }

  if (0 == host_mem->size) {
    TRAE("Cannot decode as the `size` member of the `tra_encoded_host_memory` is 0.");
    r = -50;
    goto error;
  }
  
  data_ptr = host_mem->data;
  data_size = host_mem->size;

  /* Check if there is a SPS/PPS and we might need to reconfigure the decoder. */
  while (parsed_nbytes < data_size) {

    r = tra_nal_find(data_ptr + parsed_nbytes, data_size - parsed_nbytes, &nal_start, &nal_size);
    if (r < 0) {
      TRAE("Cannot decode, failed to find a nal.");
      r = -40;
      goto error;
    }

    nal_type = nal_start[0] & 0x1F;

    switch(nal_type) {
      
      default: {
        break;
      }
      
      case TRA_NAL_TYPE_SPS: {
        create_info.sps_data = nal_start;
        create_info.sps_size = nal_size;
        break;
      }
      
      case TRA_NAL_TYPE_PPS: {
        create_info.pps_data = nal_start;
        create_info.pps_size = nal_size;
        break;
      }
    }
    
    parsed_nbytes += nal_size + 4; /* + 4 for the annex-b */
  }

  /* Create session when required and possible (we need sps and pps). */
  if (NULL != create_info.sps_data
      && create_info.sps_size > 0
      && NULL != create_info.pps_data
      && create_info.pps_size > 0)
    {
      r = vtbox_create_decoder_session(ctx, &create_info);
      if (r < 0) {
        TRAE("Failed to create the decoder session after extracting the SPS and PPS.");
        r = -4;
        goto error;
      }
    }

  if (NULL == ctx->session) {
    TRAE("Cannot decode, we couldn't create a decoder session.");
    r = -5;
    goto error;
  }

  r = vtbox_create_sample_buffer(ctx, data_ptr, data_size, &sample_buffer);
  if (r < 0) {
    TRAE("Cannot decode, failed to create the sample buffer.");
    r = -6;
    goto error;
  }

  status = VTDecompressionSessionDecodeFrame(
    ctx->session,
    sample_buffer,
    flags_decode,
    ctx,
    &flags_out
  );

  if (noErr != status) {
    TRAE("Cannot decoce, the call to `VTDecompressionSessionDecodeFrame()` failed.");
    r = -7;
    goto error;
  }
     
 error:

  if (NULL != sample_buffer) {
    CFRelease(sample_buffer);
    sample_buffer = NULL;
  }
  
  return r;
}

/* ------------------------------------------------------- */

/* 

   When we receive a SPS/PPS in `vt_dec_decode()` we will create
   a decoder session. First we use the SPS/PPS that are set in
   the given `vt_dec_create_info`. 

   Apple provides a function
   `CMVideoFormatDescriptionCreateFromH264ParameterSets()` that
   we use to create the video format description.

   Then we create the configuration for the decoder which is (at
   the moment) nothing more then a config which makes sure that
   we use the hardware decoder.

   When we have the video format description and decoder config,
   we create the config that holds the specifications for the
   resulting (decoded) images.

*/
static int vtbox_create_decoder_session(vt_dec* ctx, vt_dec_create_info* createInfo) {

  VTDecompressionOutputCallbackRecord callback_config = { 0 };
  CFMutableDictionaryRef decoder_config = NULL;
  CFMutableDictionaryRef image_config = NULL;
  OSStatus status = noErr;
  int r = 0;
  
  if (NULL == ctx) {
    TRAE("Cannot create the decoder session as the given `vt_dec*` is NULL.");
    r = -1;
    goto error;
  }

  if (NULL != ctx->session) {
    TRAE("Cannot create the decoder session as it's not NULL. Already created?");
    r = -2;
    goto error;
  }

  {
    static uint8_t did_log = 0;
    if (0 == did_log) {
      TRAE("@todo Currently I'm not releasing the `format_description` in `vtbox_create_decoder_session`. We should do this when we encounter a stream change.");
      did_log = 1;
    }
  }
  
  r = vtbox_validate_create_info(createInfo);
  if (r < 0) {
    TRAE("Cannot create the decoder as the given `vt_dec_create_info` is invalid.");
    r = -3;
    goto error;
  }

  /* Create video format description (used when creating buffers that we feed into the decoder). */
  r = vtbox_create_format_config(ctx, createInfo);
  if (r < 0) {
    TRAE("Cannot create the decoder; failed to create the `CMVideoFormatDescriptionRef`.");
    r = -4;
    goto error;
  }

  /* Create decoder config (make sure we use a hardware decoder).  */
  r = vtbox_create_decoder_config(ctx, &decoder_config);
  if (r < 0) {
    TRAE("Failed to create the decoder config.");
    r = -5;
    goto error;
  }

  /* Create the destination image config. */
  r = vtbox_create_image_config(ctx, &image_config);
  if (r < 0) {
    TRAE("Failed to create the image config.");
    r = -6;
    goto error;
  }

  /* Create the callback config. */
  callback_config.decompressionOutputCallback = vtbox_on_decoded_frame;
  callback_config.decompressionOutputRefCon = ctx;

  status = VTDecompressionSessionCreate(
    NULL,                    /* Allocator */
    ctx->format_description, /* videoFormatDescription */
    decoder_config,          /* videoDecoderSpecification */
    image_config,            /* destinationImageBufferAttributes */
    &callback_config,        /* outputCallback */
    &ctx->session            /* decompressionSessionOut */
  );

  if (noErr != status) {
    TRAE("Failed to create the decompression session.");
    r = -7;
    goto error;
  }

 error:

  if (NULL != decoder_config) {
    CFRelease(decoder_config);
    decoder_config = NULL;
  }

  if (NULL != image_config) {
    CFRelease(image_config);
    image_config = NULL;
  }

  if (r < 0) {
    if (NULL != ctx->format_description) {
      CFRelease(ctx->format_description);
      ctx->format_description = NULL;
    }
  }

  return r;
}
/* ------------------------------------------------------- */

/* 
   Prepares the format desciption that is used when we create the
   decoder session and when we feed samples into the
   decoder. This creates the `CMVideoFormatDescriptionRef` from
   the SPS and PPS which are set in the create info. This format
   description is used when we start feeding coded data to the
   decoder.

   This function sets the `format_description` member.

*/
static int vtbox_create_format_config(vt_dec* ctx, vt_dec_create_info* info) {

  const uint8_t* param_set_pointers[2] = { 0 };
  size_t param_set_sizes[2] = { 0 };
  OSStatus status = noErr;
  int r = 0;

  r = vtbox_validate_create_info(info);
  if (r < 0) {
    TRAE("Cannot create the `CMVideoFormatDescriptionRef` as the given `vt_dec_create_info` is invalid.");
    r = -1;
    goto error;
  }
  
  /* Create format description: used when creating sample buffers that we feed into the decoder. */
  if (NULL != ctx->format_description) {
    TRAE("Cannot create the decoder session as our `format_description` has been created already.");
    r = -2;
    goto error;
  }
  
  param_set_pointers[0] = info->sps_data;
  param_set_pointers[1] = info->pps_data;
  param_set_sizes[0] = info->sps_size;
  param_set_sizes[1] = info->pps_size;

  status = CMVideoFormatDescriptionCreateFromH264ParameterSets(
    NULL,                    /* Allocator */
    2,                       /* parameterSetCount */
    param_set_pointers,      /* parameterSetPointers */
    param_set_sizes,         /* parameterSetSizes */
    4,                       /* NALUnitHeaderLength */
    &ctx->format_description /* formatDescriptionOut */
  );

  if (noErr != status) {
    TRAE("Cannot create the `CMVideoFormatDescriptionRef`, failed to create it from the picture parameter sets.");
    r = -3;
    goto error;
  }

 error:
  return r;
}

/* ------------------------------------------------------- */

/* 
   Prepares a dictionary that is used when creating the decoder
   session. Creates the CFMutableDictionary that is used for the
   `videoDecoderSpecification` argument. The resulting dictionary
   is set to the given `dictConfig`. Make sure to call
   `CFRelease()` on the result.
*/
static int vtbox_create_decoder_config(vt_dec* ctx, CFMutableDictionaryRef* dictOut) {

  CFMutableDictionaryRef inst = NULL;
  int r = 0;
  
  if (NULL == ctx) {
    TRAE("Cannot create the decoder config as the given `vt_dec` is NULL.");
    r = -1;
    goto error;
  }

  /* Create the dictionary */
  inst = CFDictionaryCreateMutable(
    kCFAllocatorDefault,
    0,
    &kCFTypeDictionaryKeyCallBacks,
    &kCFTypeDictionaryValueCallBacks
  );

  if (NULL == inst) {
    TRAE("Cannot create the decoder config as we failed to allocate the mutable dictionary.");
    r = -2;
    goto error;
  }

  /* 
     Make sure we use the hardware decoder. With "require" we
     assure that we only work when a hardware decoder is
     available. 
  */
  CFDictionarySetValue(
    inst,
    kVTVideoDecoderSpecification_RequireHardwareAcceleratedVideoDecoder,
    kCFBooleanTrue
  );
  
  *dictOut = inst;

 error:

  if (r < 0) {
    
    if (NULL != inst) {
      CFRelease(inst);
      inst = NULL;
    }

    if (NULL != dictOut) {
      *dictOut = NULL;
    }
  }

  return r;
}

/* ------------------------------------------------------- */

/*

  GENERAL INFO: 

    Prepares the `destinationImageBufferAttributes` for the decoder
    session. We can only create this -after- we've created the
    `CMVideoFormatDescipription` from the picture parameter sets.
    We specificy the width, height and pixel format that we would
    like to use.
    
    You MUST call `CFRelease()` on the `dictOut` when you're ready.

  TODO:

    @todo Currently we request images that are compatible with
    OpenGL; I'm not 100% sure if we really want this as GL is
    decprecated on Mac.

 */
static int vtbox_create_image_config(vt_dec* ctx, CFMutableDictionaryRef* dictOut) {

  CFMutableDictionaryRef dict = NULL;
  CMVideoDimensions dimensions = { 0 };
  CFNumberRef fmt_ref = NULL;
  CFNumberRef height_ref = NULL;
  CFNumberRef width_ref = NULL;
  OSType fmt = 0;
  int r = 0;

  /* Validate */
  if (NULL == ctx) {
    TRAE("Cannot create the image config (`destinationImageBufferAttributes`) as the given `vt_dec*` is NULL.");
    r = -1;
    goto error;
  }

  if (NULL == ctx->format_description) {
    TRAE("Cannot create the image config (`destinationImageBufferAttributes`) as the given `vt_dec::format_description` is NULL. We need this to extract e.g. the width and height.");
    r = -2;
    goto error;
  }

  if (NULL == dictOut) {
    TRAE("Cannot create the image config (`destionationImageBufferAttributes`) as the givn `CFMutableDictionaryRef*` is NULL.");
    r = -3;
    goto error;
  }

  if (NULL == ctx->format_description) {
    TRAE("Cannot create the image config as the `format_description` member of the `vt_dec` is NULL.");
    r = -4;
    goto error;
  }

  /* Create the dictionary */
  fmt = kCVPixelFormatType_420YpCbCr8BiPlanarFullRange; 
  dimensions = CMVideoFormatDescriptionGetDimensions(ctx->format_description);
  width_ref = CFNumberCreate(NULL, kCFNumberSInt32Type, &dimensions.width);
  height_ref = CFNumberCreate(NULL, kCFNumberSInt32Type, &dimensions.height);
  fmt_ref = CFNumberCreate(NULL, kCFNumberSInt32Type, &fmt);
  
  if (NULL == width_ref) {
    TRAE("Cannot create the image config as we failed to create a `CFNumberRef` for the width.");
    r = -5;
    goto error;
  }

  if (NULL == height_ref) {
    TRAE("Cannot create the image config as we failed to create the `CFNumberRef` for the height.");
    r = -6;
    goto error;
  }

  if(NULL == fmt_ref) {
    TRAE("Cannot create the image config as we failed to create the `CFNumberRef` for the pixel format.");
    r = -7;
    goto error;
  }

  dict = CFDictionaryCreateMutable(
    kCFAllocatorDefault,
    4,
    &kCFTypeDictionaryKeyCallBacks,
    &kCFTypeDictionaryValueCallBacks
  );

  /* Set the values. */
  CFDictionarySetValue(dict, kCVPixelBufferPixelFormatTypeKey, fmt_ref);
  CFDictionarySetValue(dict, kCVPixelBufferWidthKey, width_ref);
  CFDictionarySetValue(dict, kCVPixelBufferHeightKey, height_ref);
  CFDictionarySetValue(dict, kCVPixelBufferOpenGLCompatibilityKey, kCFBooleanTrue);

  /* ... finally assign to the out argument. */
  *dictOut = dict;

 error:

  if (NULL != width_ref) {
    CFRelease(width_ref);
    width_ref = NULL;
  }

  if (NULL != height_ref) {
    CFRelease(height_ref);
    height_ref = NULL;
  }

  if (NULL != fmt_ref) {
    CFRelease(fmt_ref);
    fmt_ref = NULL;
  }

  /* Release when an error occured. */
  if (r < 0) {
    
    if (NULL != dict) {
      CFRelease(dict);
      dict = NULL;
    }

    if (NULL != dictOut) {
      *dictOut = NULL;
    }
  }

  return r;
}

/* ------------------------------------------------------- */
  
/* 
   This function checks if all the fields of the
   `vt_dec_create_info` have been set (and are valid). This is
   used when we create the decompression session and/or the
   format description.
 */
static int vtbox_validate_create_info(vt_dec_create_info* info) {

  if (NULL == info) {
    TRAE("Cannot validate the create info, given `vt_dec_create_info*` is NULL.");
    return -1;
  }
  
  if (NULL == info->sps_data) {
    TRAE("Cannot validate the create info as the given `vt_dec_create_info::sps_data` is NULL.");
    return -2;
  }

  if (0 == info->sps_size) {
    TRAE("Cannot validate the create info as the given `vt_dec_create_info::sps_size` is 0.");
    return -3;
  }

  if (0 == info->pps_data) {
    TRAE("Cannot validate the create info as the given `vt_dec_create_info::pps_data` is NULL.");
    return -4;
  }

  if (0 == info->pps_size) {
    TRAE("Cannot validate the create info as the given `vt_dec_create_info::pps_size` is 0.");
    return -5;
  }

  return 0;
}

/* ------------------------------------------------------- */

/*
  GENERAL INFO:

    This function will create a sample buffer for the given nal
    data.  According to [this][0] article we must deliver AVCC size
    based nals. As we expect the data to be in annex-b, we will
    rewrite the headers in this function.

  REFERENCES:

    [0]: https://mobisoftinfotech.com/resources/mguide/h264-encode-decode-using-videotoolbox/ 
 */
static int vtbox_create_sample_buffer(
  vt_dec* ctx,
  uint8_t* data,
  uint32_t nbytes,
  CMSampleBufferRef* sampleOut
)
{
  CMBlockBufferRef block_ref = NULL;
  CMSampleBufferRef sample_ref = NULL;
  CMSampleTimingInfo timing = { 0 };
  uint32_t parsed_nbytes = 0;
  uint8_t* nal_size_ptr = NULL; 
  uint8_t* nal_header = NULL; /* Points to the first byte of the annex-b header. */
  uint8_t* nal_start = NULL;
  uint32_t nal_size = 0;
  OSStatus status = noErr;
  int r = 0;
  
  if (NULL == ctx) {
    TRAE("Cannot create sample buffer, given `vt_dec*` is NULL.");
    r = -1;
    goto error;
  }
  
  if (NULL == data) {
    TRAE("Cannot create sample buffer, given `data` is NULL.");
    r = -2;
    goto error;
  }

  if (0 == nbytes) {
    TRAE("Cannot create sample buffer, given `nbytes` is 0.");
    r = -3;
    goto error;
  }

  if (NULL == sampleOut) {
    TRAE("Cannot create sample buffer, given `sampleOut` is NULL.");
    r = -4;
    goto error;
  }

  /* Change from annex-b to avcc size based headers. */
  while (parsed_nbytes < nbytes) {


    r = tra_nal_find(data + parsed_nbytes, nbytes - parsed_nbytes, &nal_start, &nal_size);
    if (r < 0) {
      TRAE("Failed to find a nal.");
      r = -5;
      goto error;
    }

    nal_header = nal_start - 4;

    /* Just a safety check so we don't read out of bounds. */
    if (nal_header < data) {
      TRAE("Cannot create sample buffer; we failed to access the nal header.");
      r = -6;
      goto error;
    }

    nal_size_ptr = (uint8_t*) &nal_size;
    
    nal_header[0] = nal_size_ptr[3];
    nal_header[1] = nal_size_ptr[2];
    nal_header[2] = nal_size_ptr[1];
    nal_header[3] = nal_size_ptr[0];

    parsed_nbytes += nal_size + 4;
  }

  /* Create the block buffer. */
  status = CMBlockBufferCreateWithMemoryBlock(
    NULL,             /* structureAllocator: allocates the CMBlockBuffer object; NULL uses default allocator. */
    data,             /* memoryBlock: the data we want our CMBlockBuffer to represent. */
    nbytes,           /* blockLength: the number of bytes in `data`. */
    kCFAllocatorNull, /* blockAllocator: as `data` is non-NULL, this will be used to deallocate the data. This means the block is not deallocated when we deallocate the CMBlockBuffer. */
    NULL,             /* customBlockSource: when non-null this will be used to free the block buffer. */
    0,                /* offsetToData */
    nbytes,           /* dataLength: the number of relevant bytes in `data`. */
    0,                /* flags: feature and control flags. */
    &block_ref        /* blockBufferOut: the created `CMBlockBufferRef`.  */
  );

  if (noErr != status) {
    TRAE("Failed to create the CMBlockBuffer that wraps the coded data. ");
    r = -7;
    goto error;
  }

  {
    static uint8_t did_log = 0;
    if (0 == did_log) {
      TRAE("@todo Do we have to provide timing info for the CMSampleBufffer? FFmpeg doesn't provide any timings.");
      did_log = 1;
    }
  }
  
  /* @todo currently we're feeding an "empty" timing struct. */
  timing.duration = CMTimeMake(0, 0);
  timing.presentationTimeStamp = CMTimeMake(0,0);
  timing.decodeTimeStamp = kCMTimeInvalid;

  /* Now create a sample buffer that wraps the block buffer. */
  status = CMSampleBufferCreate(
    kCFAllocatorDefault,     /* allocator; now we have to pass kCFAllocatorDefault to use the default allocater. */
    block_ref,               /* dataBuffer: the block buffer that holds the media. */
    TRUE,                    /* dataReady: wether or not the block buffer contains valid media. */
    NULL,                    /* makeDataReadyCallback */
    NULL,                    /* makeDataReadyRefcon */
    ctx->format_description, /* formatDescription: describes media's format. */
    1,                       /* numSamples: number of samples in `block_ref`  */
    1,                       /* numSampleTimingEntries */
    &timing,                 /* sampleTimingArray */
    0,                       /* numSampleSizeEntries: number of entries in `sampleSizeArray`. */
    NULL,                    /* sampleSizeArray */
    &sample_ref              /* sampleBufferOut */
  );

  if (noErr != status) {
    TRAE("Failed to create the sample buffer.");
    r = -8;
    goto error;
  }

  /* And assign the output */
  *sampleOut = sample_ref;

 error:

  return r;
}

/* ------------------------------------------------------- */

static void vtbox_on_decoded_frame(
  void *user,
  void *ref,
  OSStatus status,
  VTDecodeInfoFlags flags,
  CVImageBufferRef buffer,
  CMTime pts,
  CMTime duration
)
{
  OSType pix_fmt = CVPixelBufferGetPixelFormatType(buffer);
  bool is_planar = CVPixelBufferIsPlanar(buffer);
  tra_memory_image decoded_image = { 0 };
  vt_dec* ctx = NULL;
  uint32_t i = 0;
  int r = 0;

  /* Validate */
  if (NULL == user) {
    TRAE("Received a decoded frame, but our `user` pointers is NULL.");
    r = -10;
    goto error;
  }

  if (NULL == buffer) {
    TRAE("Decoded frame callback was called but the `buffer` is NULL.");
    r = -20;
    goto error;
  }

  ctx = (vt_dec*) user;
  if (NULL == ctx->settings.callbacks.on_decoded_data) {
    TRAE("Receive a decoded frame but the `on_decoded_data` callback hasn't been set.");
    r = -30;
    goto error;
  }

  /* Setup the result image */
  decoded_image.image_width = CVPixelBufferGetWidth(buffer);
  decoded_image.image_height = CVPixelBufferGetHeight(buffer);
  decoded_image.plane_count = CVPixelBufferGetPlaneCount(buffer);

  r = vtbox_cvpixelformat_to_imageformat(pix_fmt, &decoded_image.image_format);
  if (r < 0) {
    TRAE("Failed to convert the `CVPixelFormat` into a `TRA_IMAGE_FORAMT_`.");
    r = -10;
    goto error;
  }

  /* Lock the pixel buffer and setup the `tra_memory_image` that we pass to the client. */
  CVPixelBufferLockBaseAddress(buffer, kCVPixelBufferLock_ReadOnly);
  {
    for (i = 0; i < decoded_image.plane_count; ++i) {
      decoded_image.plane_data[i] = CVPixelBufferGetBaseAddressOfPlane(buffer, i);
      decoded_image.plane_strides[i] = CVPixelBufferGetBytesPerRowOfPlane(buffer, i);
      decoded_image.plane_heights[i] = CVPixelBufferGetHeightOfPlane(buffer, i);
    }

    /* We have to call the clietn while the base address has been locked. */
    r = ctx->settings.callbacks.on_decoded_data(
      TRA_MEMORY_TYPE_IMAGE,
      &decoded_image,
      ctx->settings.callbacks.user
    );

    if (r < 0) {
      TRAE("The client failed to handle decoded data.");
      /* fall through */
    }

  }
  CVPixelBufferUnlockBaseAddress(buffer, kCVPixelBufferLock_ReadOnly);  

 error:
  
  if (r < 0) {
    TRAE("An error occured while handling a decoded frame. ");
  }
}

/* ------------------------------------------------------- */
