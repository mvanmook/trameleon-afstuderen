/* ------------------------------------------------------- */

/* Used during development and debugging (fwrite, etc.) */
#include <stdio.h>
#include <errno.h>
#include <tra/modules/nvidia/cuviddec.h>
#include <tra/modules/nvidia/nvcuvid.h>
#include <tra/modules/nvidia/nvidia-utils.h>
#include <tra/modules/nvidia/nvidia-easy.h>
#include <tra/modules/nvidia/nvidia-dec.h>
#include <tra/modules/nvidia/nvidia.h>
#include <tra/profiler.h>
#include <tra/registry.h>
#include <tra/module.h>
#include <tra/types.h>
#include <tra/log.h>

/* ------------------------------------------------------- */

static tra_easy_api easy_decoder_api;
static tra_decoder_api decoder_api;

/* ------------------------------------------------------- */

struct tra_nvdec {
  tra_decoder_settings settings; /* Piecewise copied when creating the instaance. */
  CUvideoparser parser;
  CUvideodecoder decoder;

  /* Copying from device (GPU) to host (CPU). */
  uint32_t frame_format;  /* The pixel format of the output image, e.g. `cudaVideoSurfaceFormat_NV12`. */
  uint32_t frame_width;   /* The width of the output image. */
  uint32_t frame_height;  /* The height of the output image. */
  uint8_t* frame_ptr;     /* When we copy a decoded picture from device memory (GPU) into host memory (CPU) this is where we store the data. */
  size_t frame_size;      /* How many bytes we can and have stored in `frame_ptr`. */
};

/* ------------------------------------------------------- */

static int tra_nvdec_on_sequence_callback(void* user, CUVIDEOFORMAT* format);    /* Get's called synchronously from `cuvidParseVideoData()`. */
static int tra_nvdec_on_decode_callback(void* user, CUVIDPICPARAMS* pic);        /* Called when a picture is ready to be decoded; in decode order. */
static int tra_nvdec_on_display_callback(void* user, CUVIDPARSERDISPINFO* disp); /* Called when a picture is ready to be displayed; in display order. */

/* ------------------------------------------------------- */

static int tra_nvdec_output_to_host(tra_nvdec* ctx, CUVIDPARSERDISPINFO* info);
static int tra_nvdec_output_to_device(tra_nvdec* ctx, CUVIDPARSERDISPINFO* info);

/* ------------------------------------------------------- */

static const char* decoder_get_name();
static const char* decoder_get_author();
static int decoder_create(tra_decoder_settings* cfg, void* settings, tra_decoder_object** obj);
static int decoder_destroy(tra_decoder_object* obj);
static int decoder_decode(tra_decoder_object* obj, uint32_t type, void* data);

/* ------------------------------------------------------- */

static int easy_decoder_create(tra_easy* ez, tra_decoder_settings* cfg, void** dec);
static int easy_decoder_decode(void* dec, uint32_t type, void* data);
static int easy_decoder_destroy(void* dec);

/* ------------------------------------------------------- */

/*
  
  This will create the `tra_nvdec` instance and sets up the
  `CUvideoparser`. As documented in the [Programmers Guide][1],
  we create a `CUvideoparser` with the `ulMaxNumDecodeSurfaces`
  set to 1. Then, once our sequence callback is called we set the
  correct number of required decode surfaces. This correct number
  can be extracted from the sequence data.
 
 */
int tra_nvdec_create(
  tra_decoder_settings* cfg,
  tra_nvdec_settings* settings,
  tra_nvdec** ctx
)
{
  TRAP_TIMER_BEGIN(prof_create, "tra_nvdec_create");
  
  CUVIDPARSERPARAMS parser_params = { 0 };
  CUresult result = CUDA_SUCCESS;
  tra_nvdec* inst = NULL;
  int ret = 0;
  int r = 0;

  TRAI("Creating nvdec.");

  if (NULL == cfg) {
    TRAE("Cannot create the `nvdec` as the given `tra_decoder_settings` is NULL.");
    r = -10;
    goto error;
  }

  if (NULL == cfg->callbacks.on_decoded_data) {
    TRAE("Cannot create the `nvdec` as the given `tra_decoder_settings::callbacks::on_decoded_data` member is not set.");
    r = -20;
    goto error;
  }

  /*
    This decoder can currently output to either a CPU image which
    is represented by a `tra_memory_image` or keep the decoded
    image in GPU memory. When kept on the GPU the image is
    represented by a `tra_memory_cuda`.
  */
  if (TRA_MEMORY_TYPE_IMAGE != cfg->output_type
      && TRA_MEMORY_TYPE_CUDA != cfg->output_type)
    {
      TRAE("Cannot create the `nvdec` as the requested output type (%s) is not supported.", tra_memorytype_to_string(cfg->output_type));
      r = -70;
      goto error;
    }

  if (NULL == ctx) {
    TRAE("Cannot create the `nvdec` as the given `tra_nvdec**` is NULL.");
    r = -30;
    goto error;
  }

  if (NULL == settings) {
    TRAE("Cannot create the `nvdec` as the given `tra_nvdec_settings` is NULL.");
    r = -40;
    goto error;
  }

  if (NULL == settings->cuda_context_handle) {
    TRAE("Cannot create the `nvdec` as the given `tra_nvdec_settings::cuda_context_handle` is NULL.");
    r = -50;
    goto error;
  }

  if (NULL == settings->cuda_device_handle) {
    TRAE("Cannot create the `nvdec` as the given `tra_nvdec_settings::cuda_device_handle` is NULL.");
    r = -60;
    goto error;
  }

  inst = calloc(1, sizeof(tra_nvdec));
  if (NULL == inst) {
    TRAE("Cannot create the `nvdec` as we failed to allocate our context. Out of memory?");
    r = -40;
    goto error;
  }

  /* Create the parser which managed things like the Decode Picture Buffer */
  parser_params.CodecType = cudaVideoCodec_H264; /* @todo */
  parser_params.ulMaxNumDecodeSurfaces = 1; /* See note above this function. */
  parser_params.ulClockRate = 0; /* 0 = default = 10.000.000 hz */
  parser_params.ulErrorThreshold = 100; /* Error threshold for calling `pfnDecodePicture` event is picture is fully corrupted. */
  parser_params.ulMaxDisplayDelay = 0; /* 0 = low latency. */
  parser_params.pUserData = inst ;
  parser_params.pfnSequenceCallback = tra_nvdec_on_sequence_callback;
  parser_params.pfnDecodePicture = tra_nvdec_on_decode_callback;
  parser_params.pfnDisplayPicture = tra_nvdec_on_display_callback;

  TRAE("@todo see NvDecoder.cpp where they lock the `cuvidCreateVideoParser()` call. Why?");
  
  result = cuvidCreateVideoParser(&inst->parser, &parser_params);
  if (CUDA_SUCCESS != result) {
    TRAE("Failed to create the `CUvideoparser`.");
    r = -50;
    goto error;
  }

  /* Setup */
  inst->settings = *cfg;

  /* Assign the result. */
  *ctx = inst;

 error:

  if (r < 0) {
    
    if (NULL != inst) {
      
      ret = tra_nvdec_destroy(inst);
      if (ret < 0) {
        TRAE("After we failed to create the `nvdec` instance we also failed to cleanly destroy the instance.");
      }

      inst = NULL;
    }

    if (NULL != ctx) {
      *ctx = NULL;
    }
  }

  TRAP_TIMER_END(prof_create, "tra_nvdec_create");
  
  return r;
}

/* ------------------------------------------------------- */

int tra_nvdec_destroy(tra_nvdec* ctx) {

  CUresult ret = CUDA_SUCCESS;
  int result = 0;
  int r = 0;

  if (NULL == ctx) {
    TRAE("Cannot destroy the `tra_nvdec` as the given `tra_nvdec*` is NULL.");
    result -= 10;
    goto error;
  }

  /* Destroy parser */
  if (NULL != ctx->parser) {
    ret = cuvidDestroyVideoParser(ctx->parser);
    if (CUDA_SUCCESS != ret) {
      TRAE("Failed to cleanly destroy the `CUvideoparser`.");
      result -= 20;
    }
  }

  /* Destroy decoder */
  if (NULL != ctx->decoder) {
    ret = cuvidDestroyDecoder(ctx->decoder);
    if (CUDA_SUCCESS != ret) {
      TRAE("Failed to cleanly destroy the NVDEC decoder; cuvidDestroyDecoder() returned an error.");
      result -= 30;
    }
  }

  /* Deallocate the buffer into which we copy device -> host decoded frames. */
  if (NULL != ctx->frame_ptr) {
    ret = cuMemFreeHost(ctx->frame_ptr);
    if (CUDA_SUCCESS != ret) {
      TRAE("Failed to cleanly free the frame buffer.");
      result -= 40;
    }
  }

  ctx->frame_format = 0;
  ctx->frame_width = 0;
  ctx->frame_height = 0;
  ctx->frame_size = 0;
  ctx->frame_ptr = NULL;
  ctx->parser = NULL;
  ctx->decoder = NULL;

  free(ctx);
  ctx = NULL;

 error:
  return result;
}

/* ------------------------------------------------------- */

int tra_nvdec_decode(
  tra_nvdec* ctx,
  uint32_t type,
  void* data
)
{

  TRAP_TIMER_BEGIN(prof_dec, "tra_nvdec_decode");
  
  tra_memory_h264* host_mem = NULL;
  CUVIDSOURCEDATAPACKET pkt = { 0 };
  CUresult result = CUDA_SUCCESS;
  int r = 0;

  if (NULL == ctx) {
    TRAE("Cannot decode as the given `tra_nvdec*` is NULL.");
    r = -10;
    goto error;
  }

  if (TRA_MEMORY_TYPE_H264 != type) {
    TRAE("Cannot decode as the given type (%s) is not supported.", tra_memorytype_to_string(type));
    r = -20;
    goto error;
  }

  if (NULL == data) {
    TRAE("Cannot decode the given data as it's NULL.");
    r = -30;
    goto error;
  }

  host_mem = (tra_memory_h264*) data;
  if (NULL == host_mem->data) {
    TRAE("Cannot decode as the `data` member of the gvien `tra_memory_h264` is NULL.");
    r = -40;
    goto error;
  }

  if (0 == host_mem->size) {
    TRAE("Cannot decode as the `size` member of the given `tra_memory_h264` is 0.");
    r = -50;
    goto error;
  }

  /*
    Pass the given data into the video parser. When the video
    parser detects a SPS it will call the
    `tra_nvdec_on_sequence_callback`. The first time we receive this
    callback we will return the requested size for the decode
    picture buffer.
  */
  pkt.flags = CUVID_PKT_TIMESTAMP; /* @todo this is also used to indicate an end of stream! See `nvcuvid.h`. */
  pkt.payload_size = host_mem->size;
  pkt.payload = host_mem->data;
  pkt.timestamp = 0; /* @todo should we enfore a timestamp? */

  /* @todo see NvDecoder.cpp where the call to `cuvidParseVideoData` is sync'd. */
  result = cuvidParseVideoData(ctx->parser, &pkt);
  if (CUDA_SUCCESS != result) {
    TRAE("Failed to hand over the video data to the video parser.");
    r = -60;
    goto error;
  }

 error:
  
  TRAP_TIMER_END(prof_dec, "tra_nvdec_decode");
  
  return r;
}

/* ------------------------------------------------------- */

/*

  GENERAL INFO:
  
    Get's called synchronously from `cuvidParseVideoData()`. This
    function should return the number decode surfaces for the
    video parser to use. The documentation also states that it's
    optimal to only create the decoder once you know the minimum
    number of decoder surfaces.  This number is indicated by
    `CUVIDEOFORMAT::min_num_decode_surfaces`. By returning a
    value > 1, from this function will make sure that the parser
    will allocate the returned number of surfaces.

  TODO:

    @todo fix frame_size calculation for other formats:
    
    We probably want to implement a generic function that gives
    us the destination frame size; especially when we want to
    copy the device memory (GPU) to host memory (CPU). See
    [this][0] implemention from NVIDA; though we might want to
    create one general function in Trameleon too.

  REFERENCES:

    [0]: https://github.com/NVIDIA/video-sdk-samples/blob/master/Samples/NvCodec/NvEncoder/NvEncoder.cpp#L808 "GetFrameSize"

  RETURN:
  
    0: fail
    1: succeeded
    > 1: override decode picture buffer size.
    
*/
static int tra_nvdec_on_sequence_callback(void* user, CUVIDEOFORMAT* format) {

  TRAI("Got sequence info.");

  TRAE("@todo add support for cropping, see NvDecoder.cpp::handleVideoSequence");
  CUVIDDECODECREATEINFO params = { 0 };
  CUresult result = CUDA_SUCCESS;
  uint32_t num_surfaces = 0;
  tra_nvdec* dec = NULL;
  int r = 0;

  /* ----------------------------------------------- */
  /* Validate input                                  */
  /* ----------------------------------------------- */
  if (NULL == user) {
    TRAE("Cannot handle the sequence callback as the given `user` is NULL.");
    r = -10;
    goto error;
  }

  if (NULL == format) {
    TRAE("Cannot handle the sequence callback as the given `format` is NULL.");
    r = -20;
    goto error;
  }

  dec = (tra_nvdec*) user;
  if (NULL != dec->decoder) {
    TRAE("Our `tra_nvdec_on_sequence_callback` was called, though we've already created the decoder. This is only a issue when the SPS actually changes (e.g. new resolution). ");
    r = -30;
    goto error;
  }

  /*
    @todo when we allow other formats we have to make sure that
    the `dec->frame_size` calculation is also updated to make
    sure it can hold a decoded frame.
  */
  dec->frame_format = params.OutputFormat;

  if (cudaVideoSurfaceFormat_NV12 != dec->frame_format) {
    TRAE("Currently we only support the NV12 output format.");
    r = -35;
    goto error;
  }

  tra_nv_print_videoformat(format);

  /* ----------------------------------------------- */
  /* Setup decoder params.                           */
  /* ----------------------------------------------- */
  
  params.CodecType = format->codec;
  params.ChromaFormat = format->chroma_format;
  params.OutputFormat = (0 != format->bit_depth_luma_minus8) ? cudaVideoSurfaceFormat_P016 : cudaVideoSurfaceFormat_NV12;
  params.bitDepthMinus8 =  format->bit_depth_luma_minus8;
  params.DeinterlaceMode = cudaVideoDeinterlaceMode_Weave;
  params.ulNumOutputSurfaces = 2; /* The amount of simultaneously mapped output surfaces (e.g. frames which can be displayed). . */
  params.ulCreationFlags = cudaVideoCreate_PreferCUVID; /* JPEG decoded by CUDA, video by NVDEC hardware. */
  params.ulNumDecodeSurfaces = format->min_num_decode_surfaces;
  params.vidLock = NULL; /* @todo look into locking. */
  params.ulIntraDecodeOnly = 0;

  /* Resolution */
  params.ulWidth = format->coded_width;
  params.ulHeight = format->coded_height;
  params.ulMaxWidth = format->coded_width; /* @todo we don't support changing resolutions. */
  params.ulMaxHeight = format->coded_height; /* @todo we don't support chaning resulutions. */

  /* @todo we should change this once we take cropping into account. */
  params.ulTargetWidth = format->coded_width;
  params.ulTargetHeight = format->coded_height;

  /* ----------------------------------------------- */
  /* Create decoder instance.                        */
  /* ----------------------------------------------- */

  TRAE("@todo Should we call `cuCtxPushCurrent()` here?");

  tra_nv_print_decodecreateinfo(&params);

  result = cuvidCreateDecoder(&dec->decoder, &params);
  if (CUDA_SUCCESS != result) {
    TRAE("Failed to create the decoder instance.");
    tra_nv_print_result(result);
    r = -40;
    goto error;
  }

  /* Store the destination sizes. This is used to allocate the buffer when we're doing a device to host copy. */
  dec->frame_width = format->coded_width;
  dec->frame_height = format->coded_height;

 error:

  /* When an error occured, we must return 0 to conform the NVDEC callback API. */
  if (r < 0) {
    return 0;
  }

  /* Otherwise return the requested number of surfaces.*/
  if (NULL != format) {
    return format->min_num_decode_surfaces;
  }

  TRAE("We should not arrive here.");
    
  return r;
}

/* ------------------------------------------------------- */

/*
  Called when a picture is ready to be decoded; in decode order.

  @return (integer):
  
    0: fail
    >= 1: succeeded
    
*/
static int tra_nvdec_on_decode_callback(void* user, CUVIDPICPARAMS* pic) {

  TRAP_TIMER_BEGIN(prof_cb, "tra_nvdec_on_decode_callback");
  
  CUresult result = CUDA_SUCCESS;
  tra_nvdec* dec = NULL;
  int r = 0;

  if (NULL == user) {
    TRAE("We can decode a picture but the received `void*`  for the `user` argument is NULL. This should be a pointer to our `tra_nvdec`.");
    r = -10;
    goto error;
  }

  if (NULL == pic) {
    TRAE("Cannot decode a picture as the given `CUVIDPICPARAMS*` is NULL.");
    r = -20;
    goto error;
  }

  dec = (tra_nvdec*) user;
  
  if (NULL == dec->decoder) {
    TRAE("We can decode a picture but our `decoder` member is NULL.");
    r = -30;
    goto error;
  }

  result = cuvidDecodePicture(dec->decoder, pic);
  if (CUDA_SUCCESS != result) {
    TRAE("Failed to decode a picture.");
    tra_nv_print_result(result);
    r = -40;
    goto error;
  }

 error:

  TRAP_TIMER_END(prof_cb, "tra_nvdec_on_decode_callback");
  
  /* When an error occured, we must return 0 to conform the NVDEC callback API. */
  if (r < 0) {
    return 0;
  }

  /* All good ... */
  return 1;
}

/* ------------------------------------------------------- */

/*

  GENERAL INFO:
  
    Called when a picture is ready to be displayed; in display
    order. We check what output type the user requested and
    either map the decoded picture into CPU or keep it on the
    device. When the user wants to keep the decoded picture on
    the device we most likely want to send the decoded picture
    into an encoder to perform transcoding. Another use case
    would be to decode into a texture and render the decoded
    picture.

  @return (integer):
  
    0: fail
    >= 1: succeeded

*/

static int tra_nvdec_on_display_callback(void* user, CUVIDPARSERDISPINFO* info) {

  TRAP_TIMER_BEGIN(prof_cb, "tra_nvdec_on_display_callback");
  
  tra_nvdec* dec = NULL;
  int r = 0;
  
  /* ----------------------------------------------- */
  /* Validate input and state.                       */
  /* ----------------------------------------------- */

  if (NULL == user) {
    TRAE("Cannot handle the picture as the given `void*` that represents our user data is NULL.");
    r = -10;
    goto error;
  }    

  if (NULL == info) {
    TRAE("Cannot handle the picture as the received `CUVIDPARSERDISPINFO*` is NULL.");
    r = -20;
    goto error;
  }
  
  dec = (tra_nvdec*) user;
  
  if (NULL == dec->decoder) {
    TRAE("Cannot handle a decoded picture as our `tra_nvdec::decoder` member is NULL.");
    r = -30;
    goto error;
  }

  switch (dec->settings.output_type) {
    
    case TRA_MEMORY_TYPE_IMAGE: {
      r = tra_nvdec_output_to_host(dec, info);
      break;
    }
    
    case TRA_MEMORY_TYPE_CUDA: {
      r = tra_nvdec_output_to_device(dec, info);
      break;
    }
    
    default: {
      TRAE("Unhandled output type: %s.", tra_memorytype_to_string(dec->settings.output_type));
      r = -40;
      break;
    }
  }

 error:

  TRAP_TIMER_END(prof_cb, "tra_nvdec_on_display_callback");
    
  if (r < 0) {
    return 0;
  }
  
  return 1;
}

/* ------------------------------------------------------- */

static int tra_nvdec_output_to_host(tra_nvdec* ctx, CUVIDPARSERDISPINFO* info) {

  TRAP_TIMER_BEGIN(prof_cb, "tra_nvdec_output_to_host");
    
  tra_memory_image decoded_image = { 0 };
  CUVIDPROCPARAMS proc_params = { 0 };
  CUresult result = CUDA_SUCCESS;
  size_t required_size = 0;
  CUdeviceptr src_frame = 0;
  uint32_t src_stride = 0;
  int is_mapped = 0;
  int r = 0;

  if (NULL == ctx) {
    TRAE("Cannot output to host memory as the given `tra_nvdec*` instance is NULL.");
    r = -10;
    goto error;
  }

  if (NULL == info) {
    TRAE("Cannot output to host memory as the given `CUVIDPARSERDISPINFO*` is NULL.");
    r = -20;
    goto error;
  }
  
  /* @todo when we're going to support different memory transfers we should remove this check. */
  if (0 == ctx->frame_height) {
    TRAE("The `frame_height` is 0; this should be set in the sequence callback.");
    r = -30;
    goto error;
  }

  if (cudaVideoSurfaceFormat_NV12 != ctx->frame_format) {
    TRAE("The `frame_format` is not `cudaVideoSurfaceFormat_NV12`; currently this is the only supported format.");
    r = -40;
    goto error;
  }

  if (NULL == ctx->settings.callbacks.on_decoded_data) {
    TRAE("Cannot handle the decoded data as the `on_decoded_data` callback is not set.");
    r = -40;
    goto error;
  }
  
  /* ----------------------------------------------- */
  /* Map the decoded data.                           */
  /* ----------------------------------------------- */

  /* tra_nv_print_parserdispinfo(info); */ 

  /* Based on `NvDecoder.cpp` */
  proc_params.progressive_frame = info->progressive_frame;
  proc_params.second_field = info->repeat_first_field + 1;
  proc_params.top_field_first = info->top_field_first;
  proc_params.unpaired_field = info->repeat_first_field < 0 ? 1 : 0;
  proc_params.output_stream = NULL; /* @todo read up on how to use output streams. */
     
  result = cuvidMapVideoFrame(ctx->decoder, info->picture_index, &src_frame, &src_stride, &proc_params);
  if (CUDA_SUCCESS != result) {
    TRAE("Failed to map the video frame.");
    r = -40;
    goto error;
  }

  is_mapped = 1;

  {
    /* just making sure I'll fix this :) */
    static int logged = 0;
    if (0 == logged) {
      TRAE("@todo need to create a function that gives us the frame size based on pixel format.");
      logged = 1;
    }
  }

  /* ----------------------------------------------- */
  /* Alocate our destination buffer                  */
  /* ----------------------------------------------- */
 
  if (NULL == ctx->frame_ptr) {

    if (NULL != ctx->frame_ptr) {
      TRAE("It seems we've already allocated our destination buffer; currently we don't handle resizing of the destination buffer.");
      r = -55;
      goto error;
    }

    /* Calculate the nv12 frame size. */
    ctx->frame_size = (src_stride * ctx->frame_height * 3) / 2;
    if (0 == ctx->frame_size) {
      TRAE("Failed to calculate a valid `frame_size`; this means that the received format or pitch contains an invalid size.");
      r = -60;
      goto error;
    }

    result = cuMemAllocHost((void**) &ctx->frame_ptr, ctx->frame_size);
    if (CUDA_SUCCESS != result) {
      TRAE("Failed to allocate our desntination (CPU) buffer.");
      r = -70;
      goto error;
    }
  
    if (NULL == ctx->frame_ptr) {
      TRAE("Failed to allocate our destination (CPU) buffer; our `frame_ptr` is NULL.");
      r = -80;
      goto error;
    }
  }

  /* -------------------------------------------------- */
  /* Transfer from device (GPU) to host (CPU) memory.   */
  /* -------------------------------------------------- */
  
  result = cuMemcpyDtoH(ctx->frame_ptr, src_frame, ctx->frame_size);
  if (CUDA_SUCCESS != result) {
    TRAE("Failed to copy the decoded frame from device (GPU) into host (CPU) memory.");
    r = -50;
    goto error;
  }

  switch (ctx->frame_format) {
    
    case cudaVideoSurfaceFormat_NV12: {
      
      decoded_image.image_format = TRA_IMAGE_FORMAT_NV12;
      decoded_image.image_width = ctx->frame_width;
      decoded_image.image_height = ctx->frame_height;
      decoded_image.plane_count = 2;

      decoded_image.plane_strides[0] = src_stride;
      decoded_image.plane_strides[1] = src_stride;
      decoded_image.plane_strides[2] = 0;

      decoded_image.plane_heights[0] = ctx->frame_height;
      decoded_image.plane_heights[1] = ctx->frame_height / 2;
      decoded_image.plane_heights[2] = 0;
      
      decoded_image.plane_data[0] = ctx->frame_ptr;
      decoded_image.plane_data[1] = ctx->frame_ptr + (ctx->frame_height * src_stride);
      decoded_image.plane_data[2] = NULL;
      
      break;
    }
    
    default: {
      TRAE("Unhandled pixel format. Cannot handle the decoded picture.");
      r = -60;
      goto error;
    }
  }

  /* .. finally notify our user. */
  r = ctx->settings.callbacks.on_decoded_data(
    TRA_MEMORY_TYPE_IMAGE,
    &decoded_image,
    ctx->settings.callbacks.user
  );

  if (r < 0) {
    TRAE("The user failed to cleanly handle the decoded image.");
    r = -70;
    goto error;
  }

 error:

  if (1 == is_mapped) {
    
    result = cuvidUnmapVideoFrame(ctx->decoder, src_frame);
    if (CUDA_SUCCESS != result) {
      TRAE("Failed to unmap the video frame.");
      r = -60;
    }
  }

  TRAP_TIMER_END(prof_cb, "tra_nvdec_output_to_host");

  return r;
}

/* ------------------------------------------------------- */

/*
  
  This function is called when the user requested to decode video
  and keep the decoded data on the device (GPU). This is most
  likely the case when the user wants to transcode video. We
  decode the input video on the device and then use it to encode
  it into a different resolution and/or bitrate.
  
 */
static int tra_nvdec_output_to_device(tra_nvdec* ctx, CUVIDPARSERDISPINFO* info) {

  TRAP_TIMER_BEGIN(prof_cb, "tra_nvdec_output_to_device");

  CUVIDPROCPARAMS proc_params = { 0 };
  tra_memory_cuda cuda_mem = { 0 };
  CUresult result = CUDA_SUCCESS;
  int is_mapped = 0;
  int r = 0;

  TRAI("device output!");

  /* ----------------------------------------------- */
  /* Validate the input                              */
  /* ----------------------------------------------- */

  if (NULL == ctx) {
    TRAE("Cannot handle the decoded frame as the given `tra_nvdec*` is NULL.");
    r = -10;
    goto error;
  }
  
  if (NULL == ctx->settings.callbacks.on_decoded_data) {
    TRAE("Cannot handle the decoded frame as the `on_decoded_data` callback hasn't been set.");
    r = -20;
    goto error;
  }

  if (NULL == info) {
    TRAE("Cannot handle the decoded frame as the given `CUVIDPARSERDISPINFO*` is NULL.");
    r = -30;
    goto error;
  }

  /* ----------------------------------------------- */
  /* Map the decoded data.                           */
  /* ----------------------------------------------- */

  /* Based on `NvDecoder.cpp` */
  proc_params.progressive_frame = info->progressive_frame;
  proc_params.second_field = info->repeat_first_field + 1;
  proc_params.top_field_first = info->top_field_first;
  proc_params.unpaired_field = info->repeat_first_field < 0 ? 1 : 0;
  proc_params.output_stream = NULL; /* @todo read up on how to use output streams. */
     
  result = cuvidMapVideoFrame(ctx->decoder, info->picture_index, &cuda_mem.ptr, &cuda_mem.stride, &proc_params);
  if (CUDA_SUCCESS != result) {
    TRAE("Failed to map the video frame.");
    r = -40;
    goto error;
  }

  is_mapped = 1;

  /* snori: remove this; we're not wrapping the memory anymore. */
  //dec_mem.type = TRA_DEVICE_MEMORY_TYPE_CUDA;
  //dec_mem.data = &cuda_mem;

  /* Pass the decoded data to the user so it can e.g. transcode it. */
  r = ctx->settings.callbacks.on_decoded_data(
    TRA_MEMORY_TYPE_CUDA,
    &cuda_mem,
    ctx->settings.callbacks.user
  );

  if (r < 0) {
    TRAE("The user failed to handle the decoded data.");
    r = -50;
    goto error;
  }
  
 error:

  if (1 == is_mapped) {
    
    result = cuvidUnmapVideoFrame(ctx->decoder, cuda_mem.ptr);
    if (CUDA_SUCCESS != result) {
      TRAE("Failed to unmap the video frame.");
      r = -60;
    }
  }

  TRAP_TIMER_END(prof_cb, "tra_nvdec_output_to_device");
  
  return r;
}

/* ------------------------------------------------------- */

static const char* decoder_get_name() {
  return "nvdec";
}

/* ------------------------------------------------------- */

static const char* decoder_get_author() {
  return "roxlu";
}

/* ------------------------------------------------------- */

static int decoder_create(
  tra_decoder_settings* cfg,
  void* settings,
  tra_decoder_object** obj
)
{
  tra_nvdec* inst = NULL;
  int result = 0;
  int r = 0;

  if (NULL == obj) {
    TRAE("Cannot create the `nvdec` instance as the given `obj` is NULL.");
    r = -10;
    goto error;
  }

  if (NULL != (*obj)) {
    TRAE("Cannot create the `nvdec` instance as the given `*obj` is not NULL. Already created?");
    r = -20;
    goto error;
  }

  r = tra_nvdec_create(cfg, settings, &inst);
  if (r < 0) {
    TRAE("Cannot create the `nvdec` instance as we failed to creat the `tra_nvdec` context.");
    r = -30;
    goto error;
  }

  *obj = (tra_decoder_object*) inst;
  
 error:

  if (r < 0) {
    
    if (NULL != inst) {
      
      result = decoder_destroy((tra_decoder_object*) inst);
      if (result < 0) {
        TRAE("After we failed to create the `nvdec` instance we also failed to cleanly destroy it.");
      }

      inst = NULL;
    }

    if (NULL != obj) {
      *obj = NULL;
    }
  }
  
  return r;
}

/* ------------------------------------------------------- */

static int decoder_destroy(tra_decoder_object* obj) {

  int result = 0;
  int r = 0;

  TRAE("@todo destroy decoder ");

  return result;
}

/* ------------------------------------------------------- */

static int decoder_decode(
  tra_decoder_object* obj,
  uint32_t type,
  void* data
)
{
  int r = 0;

  if (NULL == obj) {
    TRAE("Cannot decode as the given `tra_decoder_object*` is NULL.");
    r = -10;
    goto error;
  }

  r = tra_nvdec_decode((tra_nvdec*) obj, type, data);
  if (r < 0) {
    TRAE("Cannot decode with `nvdec`, something went wrog while trying to call `tra_nvdec_decode()`. ");
    r = -50;
    goto error;
  }
  
 error:
  return r;
}

/* ------------------------------------------------------- */

/*
  This function creates an instance of the `nvdec` decoder. This
  function is used when we use the easy layer of Trameleon. This
  easy layer will make sure that certain boiler plate or other
  "cumbersome" work will be done for the user.

  For example, this function uses the `tra_easy_nvapp` API to
  setup CUDA: create a context, select and create a device
  context, etc. When you don't use the easy layer you would have
  to do this yourself. 

 */
static int easy_decoder_create(tra_easy* ez, tra_decoder_settings* cfg, void** dec) {

  tra_easy_nvapp_settings nvapp_cfg = { 0 };
  tra_nvdec_settings nvdec_cfg = { 0 };
  tra_easy_nvapp* nvapp = NULL;
  tra_nvdec* inst = NULL;
  int r = 0;

  if (NULL == ez) {
    TRAE("Cannot create the easy nvdec instance as the given `tra_easy*` is NULL.");
    r = -20;
    goto error;
  }

  if (NULL == cfg) {
    TRAE("Cannot create the easy nvdec instance as the given `tra_decoder_settings*` is NULL.");
    r = -30;
    goto error;
  }
  
  if (NULL == dec) {
    TRAE("Cannot create the easy nvdec instance as the result argument is NULL.");
    r = -30;
    goto error;
  }

  if (NULL != *dec) {
    TRAE("Cannot create the easy nvdec instance as the given `*dec` is not NULL. Did you create it already?");
    r = -40;
    goto error;
  }

  /* Make sure an `tra_easy_nvapp` has been created (singleton) */
  nvapp_cfg.easy_ctx = ez;

  r = tra_easy_nvapp_ensure(&nvapp_cfg, &nvapp);
  if (r < 0) {
    TRAE("Cannot create the easy nvdec instance. Failed to ensure the `tra_easy_nvapp`");
    r = -50;
    goto error;
  }

  if (NULL == nvapp) {
    TRAE("Cannot create the easy nvdec instance as we failed to ensure the `tra_easy_nvapp`.");
    r = -60;
    goto error;
  }

  /* Setup decoder config. */
  nvdec_cfg.cuda_context_handle = nvapp->cuda_context_handle;
  nvdec_cfg.cuda_device_handle = nvapp->cuda_context_handle;

  r = tra_nvdec_create(cfg, &nvdec_cfg, &inst);
  if (r < 0) {
    TRAE("Cannot create the easy nvdec instance as we failed to create the `nvdec` instance. ");
    r = -70;
    goto error;
  }

  /* Assign result */
  *dec = inst;

 error:

  /* When an error occured we have to cleanup. */
  if (r < 0) {

    if (NULL != inst) {
      r = tra_nvdec_destroy(inst);
      if (r < 0) {
        TRAE("After an error occured while trying to create the easy NVIDIA decoder, we failed to cleanly destroy it.");
      }
    }

    if (NULL != dec) {
      *dec = NULL;
    }
  }
  
  return r;
}

/* ------------------------------------------------------- */
    
static int easy_decoder_decode(void* dec, uint32_t type, void* data) {
  return tra_nvdec_decode(dec, type, data);
}

/* ------------------------------------------------------- */

static int easy_decoder_destroy(void* dec) {
  return tra_nvdec_destroy(dec);
}
/* ------------------------------------------------------- */

int tra_load(tra_registry* reg) {

  int r = 0;
  
  if (NULL == reg) {
    TRAE("Cannot load the `nvdec` module as the given `tra_registry` is NULL.");
    return -10;
  }
  
  r = tra_registry_add_decoder_api(reg, &decoder_api);
  if (r < 0) {
    TRAE("Failed to register the `nvdec` decoder.");
    return -20;
  }

  r = tra_registry_add_easy_api(reg, &easy_decoder_api);
  if (r < 0) {
    TRAE("Failed to register the `nvdec` easy decoder.");
    return -30;
  }
  
  return 0;
}

/* ------------------------------------------------------- */

static tra_decoder_api decoder_api = {
  .get_name = decoder_get_name,
  .get_author = decoder_get_author,
  .create = decoder_create,
  .destroy = decoder_destroy,
  .decode = decoder_decode,
};

/* ------------------------------------------------------- */

static tra_easy_api easy_decoder_api = {
  .get_name = decoder_get_name,
  .get_author = decoder_get_author,
  .encoder_create = NULL,
  .encoder_encode = NULL,
  .encoder_flush = NULL,
  .encoder_destroy = NULL,
  .decoder_create = easy_decoder_create,
  .decoder_decode = easy_decoder_decode,
  .decoder_destroy = easy_decoder_destroy,
};
  
/* ------------------------------------------------------- */
