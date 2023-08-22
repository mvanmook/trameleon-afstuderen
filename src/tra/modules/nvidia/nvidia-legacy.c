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

  NVIDIA MODULE
  =============

  GENERAL INFO:

    This file contains the (currently) experimental
    implementation of the nvidia. During the implementation of
    this module I'll describe things that I should look into in
    the `research-nvidia.md` file of the documentation
    repository.

  REFERENCES:

    [0]: https://gist.github.com/roxlu/a06840d580a21683990d03ea5fec25ea "NvDecoder.cpp"
    [1]: https://gist.github.com/roxlu/db403566fae167a90b43458db5d98aa3 "VideoDecoderCuda.cpp"
    [2]: https://gist.github.com/roxlu/549874bbbf8e6e53d39ecd15d14e0778 "nvdec.cpp"
    [3]: https://gist.github.com/roxlu/549874bbbf8e6e53d39ecd15d14e0778 "plugin_cuda_codec_h264.cxx"
    [4]: https://gist.github.com/roxlu/163379ef27c8c27832351d42fe0da08b "cuviddec.c"
    [5]: https://gist.github.com/roxlu/474802a3985de3404c0e452ac2a96f32 "memory.c"
    [6]: https://docs.nvidia.com/video-technologies/video-codec-sdk/pdf/NVENC_VideoEncoder_API_ProgGuide.pdf "NVENC Programmers Guide"

 */

/* ------------------------------------------------------- */

#include <tra/module.h>
#include <tra/modules/nvidia/nvidia.h>
#include <tra/registry.h>
#include <tra/modules/nvidia/nvcuvid.h>
#include <tra/modules/nvidia/cuviddec.h>
#include <tra/modules/nvidia/nvEncodeAPI.h>
#include <tra/types.h>
#include <tra/log.h>
#include <cuda.h>
#include <stdio.h>

/* ------------------------------------------------------- */

static tra_cuda_api cuda_api;
static tra_decoder_api nvdec_api;
static tra_encoder_api nvenc_api;

/* ------------------------------------------------------- */

typedef struct nvdec_context {
  tra_decoder_callbacks callbacks;
  CUvideoparser video_parser;
  CUvideodecoder video_decoder;
  uint32_t coded_width; /* @todo: added this while experimenting with mapping of video frames, we might not need this. */
  uint32_t coded_height; /* @todo: added this while experimenting with mapping of video frames, we might not need this. */
  uint32_t chroma_height; /* @todo: based on [this][0] implementation and used when copying from gpu to cpu. When we do an on-device copy, I don't think we need this. */
  uint32_t luma_height;  /* @todo based on [this][0] implementation and used when copying from gpu to cpu. When do do an on-device copy, I don't think we need this. */
  uint8_t* dst_buffer; /* @todo currently I'm copying the decoded frames to CPU. This is the buffer into which we write the decoded data. */
  uint32_t dst_nbytes; /* @todo currently we copy the decoded yuv into `dst_buffer`; this is the number of bytes we allocated. */
} nvdec_context;

static const char* nvdec_get_name();
static const char* nvdec_get_author();
static int nvdec_create(tra_decoder_callbacks* callbacks, rx_dict* cfg, void* settings, tra_decoder_object** inst);
static int nvdec_destroy(tra_decoder_object* inst);
static int nvdec_decode(tra_decoder_object* inst, tra_encoded_data* data);
static int nvdec_sequence_callback(void* user, CUVIDEOFORMAT* fmt);
static int nvdec_decode_callback(void* user, CUVIDPICPARAMS* params);
static int nvdec_display_callback(void* user, CUVIDPARSERDISPINFO* info);
static int nvdec_print_videoformat(CUVIDEOFORMAT *fmt);
static const char* nvdec_cudavideocodec_to_string(cudaVideoCodec codec);
static const char* nvdec_cudachromaformat_to_string(cudaVideoChromaFormat chroma);

/* ------------------------------------------------------- */

typedef struct nvenc_context {

  tra_encoder_callbacks callbacks;
  NV_ENCODE_API_FUNCTION_LIST funcs; /* NVIDIA encoder functions, loaded through the call to `NvEncodeAPICreateInstance()`. */
  void* session; /* The encoder session. */
  uint32_t coded_width; /* The width of the input stream. */
  uint32_t coded_height; /* The height of the input stream */
  uint32_t chroma_height; /* @todo currently we only support NV12 as input. The height of the chroma plane of the input stream. */
  uint32_t luma_height; /* @todo currently we only support NV12 as input. The height of the luma plane of the input stream; */

  /* Output */
  NV_ENC_OUTPUT_PTR* output_buffers; /* Array to bit stream output buffers. */
  uint32_t num_output_buffers; /* Number of bit stream output buffers. */

  /* Input */
  CUdeviceptr* input_buffers; /* @todo currently using CUDA to allocate input buffers. */
  uint32_t num_input_buffers;
  NV_ENC_BUFFER_FORMAT input_format; /* @todo currently we only support NV12 as input. Although the documentation says we should only set the input format for D3D12 interface type is used, we still use it's required to calculate our buffer sizes. */
  uint32_t input_pitch; /* @todo this is specific to the situation where we use cuda based buffers; this is set to the pitch of the input buffers. */
  uint32_t input_index; /* @todo only for CPU > GPU transfer which we're currently testing. This is the index into the `input_buffer` that we're going to copy data into. Every time we encode a frame we icrement this up to `num_input_buffer`. */
  NV_ENC_REGISTERED_PTR* input_regs; /* @todo When we use externally allocated buffers we have to register them; for each registered buffer we store a handle in this array. */
  uint32_t num_input_regs; /* @todo this will be the same size as `num_input_buffers` though it might be cleaner to keep them separate. */
  
} nvenc_context;

static const char* nvenc_get_name();
static const char* nvenc_get_author();
static int nvenc_create(tra_encoder_callbacks* callbacks, rx_dict* cfg, void* settings, tra_encoder_object** inst);
static int nvenc_destroy(tra_encoder_object* inst);
static int nvenc_encode(tra_encoder_object* inst, tra_video_frame* frame);
static int nvenc_print_initparams(NV_ENC_INITIALIZE_PARAMS* cfg);
static int nvenc_print_inputformats(nvenc_context* ctx, GUID encodeGuid);
static const char* nvenc_status_to_string(NVENCSTATUS status);
static const char* nvenc_codecguid_to_string(GUID* guid);
static const char* nvenc_presetguid_to_string(GUID* guid);
static const char* nvenc_profileguid_to_string(GUID* guid);
static const char* nvenc_framefieldmode_to_string(NV_ENC_PARAMS_FRAME_FIELD_MODE mode);
static const char* nvenc_mvprecision_to_string(NV_ENC_MV_PRECISION prec);
static const char* nvenc_rcmode_to_string(NV_ENC_PARAMS_RC_MODE mode);
static const char* nvenc_tuninginfo_to_string(NV_ENC_TUNING_INFO info);
static const char* nvenc_bufferformat_to_string(NV_ENC_BUFFER_FORMAT fmt);

/* ------------------------------------------------------- */

static int cuda_device_list(); /* Prints a list with available NVIDIA devices. */
static int cuda_device_get(int num, CUdevice* device);
static int cuda_context_create(CUdevice device, int flags, CUcontext* result);
static int cuda_context_destroy(CUcontext ctx);

/* ------------------------------------------------------- */

static int cuda_device_list() {

  const char* err_str = NULL;
  CUresult result = CUDA_SUCCESS;
  CUdevice device = 0;
  char name[256] = { 0 };
  int num_devices = 0;
  int i = 0;

  result = cuDeviceGetCount(&num_devices);
  if (CUDA_SUCCESS != result) {
    cuGetErrorString(result, &err_str);
    TRAE("Cannot list CUDA devices, failed to get the number of cuda devices: %s.", err_str);
    return -1;
  }

  for (i = 0; i < num_devices; ++i) {

    result = cuDeviceGet(&device, i);
    if (CUDA_SUCCESS != result) {
      cuGetErrorString(result, &err_str);
      TRAE("Cannot list CUDA devices, failed to access a CUDA device for index %d: %s.", i, err_str);
      continue;
    }

    result = cuDeviceGetName(name, sizeof(name), device);
    if (CUDA_SUCCESS != result) {
      cuGetErrorString(result, &err_str);
      TRAE("Failed list CUDA devices, failed to get the CUDA device name for index %d: %s.", i, err_str);
      continue;
    }

    TRAD("Cuda device %d: %s", i, name);
  }
  
  return 0;
}

static int cuda_device_get(int num, CUdevice* device) {
  
  CUresult result = CUDA_SUCCESS;
  const char* err_str = NULL;
  
  if (NULL == device) {
    TRAE("Cannot get device, given `CUdevice*` is NULL.");
    return -1;
  }

  result = cuDeviceGet(device, num);
  if (CUDA_SUCCESS != result) {
    cuGetErrorString(result, &err_str);
    TRAE("Failed to access a CUDA device for index %d: %s.", num, err_str);
    return -2;
  }
  
  return 0;
}

static int cuda_context_create(CUdevice device, int flags, CUcontext* ctx) {
  
  CUresult result = CUDA_SUCCESS;
  const char* err_str = NULL;
  
  result = cuCtxCreate(ctx, flags, device);
  if (CUDA_SUCCESS != result) {
    cuGetErrorString(result, &err_str);
    TRAE("Failed to create the CUDA context: %s", err_str);
    return -1;
  }

  TRAD("CREATED: %p", *ctx);
  
  return 0;
}

static int cuda_context_destroy(CUcontext ctx) {

  CUresult result = CUDA_SUCCESS;
  const char* err_str = NULL;
  int r = 0;

  if (NULL == ctx) {
    TRAE("Cannot destroy the CUDA context, the given context is NULL.");
    return -1;
  }
  
  result = cuCtxDestroy(ctx);
  if (CUDA_SUCCESS != result) {
    cuGetErrorString(result, &err_str);
    TRAE("Failed to cleanly destroy the CUDA context: %s", err_str);
    return -2;
  }

  return r;
}

/* ------------------------------------------------------- */

static const char* nvdec_get_name() {
  return "nvdec";
}

static const char* nvdec_get_author() {
  return "roxlu";
}

/* 

   Create the NVIDIA decoder instance, based on the NVIDIA Video
   Decoder API Programming Guide.  Important to be aware of is
   the `ulMaxNumDecodeSurfaces` which is the number of surfaces
   in the decode picture buffers (DPB). As we don't know this
   value at this point, we set it to 1 and update it when our
   `pfnSequenceCallback` is called; this strategy is also
   described in the NVIDIA Video Decoder API Programming Guide.

 */
static int nvdec_create(
  tra_decoder_callbacks* callbacks,
  rx_dict* cfg,
  void* settings,
  tra_decoder_object** inst
)
{

  CUVIDPARSERPARAMS parser_cfg = {};
  CUresult result = CUDA_SUCCESS;
  const char* err_str = NULL;
  nvdec_context* ctx = NULL;
  int r = 0;
  
  TRAI("Create nvdecoder api.");

  if (NULL == callbacks) {
    TRAE("Cannot create the NVIDIA decoder, the given `tra_decoder_callbacks` is NULL.");
    return -1;
  }

  if (NULL == callbacks->on_decoded_data) {
    TRAE("The `tra_decoder_callbacks::on_decoded_data()` is NULL. Set it to a function that receives decoded frames.");
    return -2;
  }
  
  /* Create the instance that keeps track of our state. */
  ctx = calloc(1, sizeof(nvdec_context));
  if (NULL == ctx) {
    TRAE("Failed to allocate our internal `nvdec_context`.");
    return -3;
  }

  ctx->callbacks.on_decoded_data = callbacks->on_decoded_data;
  ctx->callbacks.user = callbacks->user;

  parser_cfg.CodecType = cudaVideoCodec_H264;                    /* See `cuviddec.h` */
  parser_cfg.ulMaxNumDecodeSurfaces = 1;
  parser_cfg.ulClockRate = 0;                                    /* 0 = default, 10000000Hz */
  parser_cfg.ulErrorThreshold = 100;                             /* 100 = always try `pfnDecodePicture` even if pciture bitstream is fully corrupted. */
  parser_cfg.ulMaxDisplayDelay = 0;                              /* 0 = no delay, max display queue delay. */
  parser_cfg.bAnnexb = 1;                                        /* 1 = Use annex-b input stream. */
  parser_cfg.pUserData = ctx;                                    /* Passed into the callbacks. */
  parser_cfg.pfnSequenceCallback = nvdec_sequence_callback;      /* Called before decoder frames and/or whenever there is a format change. */
  parser_cfg.pfnDecodePicture = nvdec_decode_callback;           /* Called when a picture is ready to be decoded (decode order). */
  parser_cfg.pfnDisplayPicture = nvdec_display_callback;         /* Called when a picture is ready to be displayed (display order). */
  parser_cfg.pfnGetOperatingPoint = NULL;

  result = cuvidCreateVideoParser(&ctx->video_parser, &parser_cfg);
  if (CUDA_SUCCESS != result) {
    cuGetErrorString(result, &err_str);
    TRAE("Failed to create the video parser: %s.", err_str);
    r = -2;
    goto error;
  }

  /* Finally set the result. */
  *inst = (tra_decoder_object*) ctx;

 error:

  /* When an error occurs we have to cleanup the decoder. */
  if (r < 0) {

    if (NULL != ctx) {
      
      r = nvdec_destroy((tra_decoder_object*) ctx);
      if (r < 0) {
        TRAE("After we've failed to create an decoder instance, we also failed to cleanly destroy it.");
      }

      ctx = NULL;
    }

    if (NULL != inst) {
      *inst = NULL;
    }
  }

  return r;
}

static int nvdec_destroy(tra_decoder_object* inst) {

  TRAE("@todo flush frames.");
  
  int r = 0;
  int ret = 0;
  nvdec_context* ctx = NULL;
  CUresult result = CUDA_SUCCESS;

  if (NULL == inst) {
    TRAE("Cannot destroy the `nvdec_context` because the given `tra_decoder_object*` is NULL.");
    return -1;
  }

  ctx = (nvdec_context*) inst;

  if (NULL != ctx->video_parser) {
    result = cuvidDestroyVideoParser(ctx->video_parser);
    if (CUDA_SUCCESS != result) {
      TRAE("Failed to cleanly destroy the video parser.");
      ret -= 1;
    }
  }

  if (NULL != ctx->video_decoder) {
    result = cuvidDestroyDecoder(ctx->video_decoder);
    if (CUDA_SUCCESS != result) {
      TRAE("Failed to cleanly destroy the video decoder.");
      ret -= 2;
    }
  }
  
  if (NULL != ctx->dst_buffer) {
    result = cuMemFreeHost(ctx->dst_buffer);
    if (CUDA_SUCCESS != result) {
      TRAE("Failed to cleanly free the destination buffer.");
      ret -= 3;
    }
  }

  ctx->video_parser = NULL;
  ctx->video_decoder = NULL;
  ctx->coded_width = 0;
  ctx->coded_height = 0;
  ctx->chroma_height = 0;
  ctx->luma_height = 0;
  ctx->dst_buffer = 0;
  ctx->dst_nbytes = 0;

  free(ctx);
  ctx = NULL;
  
  return ret;
}

/*
  Decode the given buffer; currently we expect that the buffer
  contains H264 annex-b.
 */
static int nvdec_decode(tra_decoder_object* inst, tra_encoded_data* data) {

  CUVIDSOURCEDATAPACKET pkt = {};
  CUresult result = CUDA_SUCCESS;
  nvdec_context* ctx = NULL;
  
  if (NULL == inst) {
    TRAE("Cannot decode as the given `tra_decoder_object*` is NULL.");
    return -1;
  }

  if (NULL == data) {
    TRAE("Cannot decode as the given `tra_encoded_data*` is NULL.");
    return -2;
  }

  pkt.flags = 0;
  pkt.payload_size = data->nbytes;
  pkt.payload = data->data;
  pkt.timestamp = 0;

  ctx = (nvdec_context*) inst;

  result = cuvidParseVideoData(ctx->video_parser, &pkt);
  if (CUDA_SUCCESS != result) {
    TRAE("Failed to parse the given video data.");
    return -3;
  }

  return 0;
}

/* ------------------------------------------------------- */

/*

  See chapter "4.1 Video Parser" of the NVIDIA VIDEO CODEC SDK -
  DECODER"`, which explains that we should wait with creating the
  decoder object until we know the minimum number of surfaces we
  should allocate in the Decode Picture Buffer (DPB). 

  Interesting fields:

  - ulIntraDecodeOnly: can be used to optimize memory usage.
  - ulMaxWidth: used when reconfiguring.
  - ulMaxHeight: used when reconfiguring.
  - OutputFormat: @todo see what the most optimal output format is.
  - ulNumOutputSurfaces: number of output surfaces; optimize next steps.

  @return 

    0   = failed
    1   = succeeded
    > 1 = number of surfaces to allocaten the Decode Picture Buffer.

 */
static int nvdec_sequence_callback(void* user, CUVIDEOFORMAT* fmt) {

  cudaVideoDeinterlaceMode interlace_mode = cudaVideoDeinterlaceMode_Weave;
  CUVIDDECODECREATEINFO cfg = { };
  CUresult result = CUDA_SUCCESS;
  nvdec_context* ctx = NULL;

  if (NULL == fmt) {
    TRAE("Cannot handle the sequence callback as the given `CUVIDEOFORMAT*` is NULL.");
    return 0;
  }

  nvdec_print_videoformat(fmt);

  if (NULL == user) {
    TRAE("Cannot handle the sequence callbacks as the given `void*` for the user data is NULL.");
    return 0;
  }

  ctx = (nvdec_context*) user;
  
  if (NULL != ctx->video_decoder) {
    TRAE("We already created the video decoder. We might arrive here when the stream changed. @todo handle this situation.");
    return 0;
  }

  ctx->coded_width = fmt->coded_width;
  ctx->coded_height = fmt->coded_height;

  /* Calculate the height of the Y and UV */
  switch (fmt->chroma_format) {
    
    case cudaVideoChromaFormat_420: {
      ctx->luma_height = fmt->coded_height;
      ctx->chroma_height = fmt->coded_height * 0.5;
      break;
    }
    
    default: {
      TRAE("Unhandled chroma format: %s.", nvdec_cudachromaformat_to_string(fmt->chroma_format));
      return 0;
    }
  }

  /* Allocate the buffer where we store the decoded NV12 frames. */
  ctx->dst_nbytes = (ctx->coded_width * ctx->luma_height) + (ctx->coded_width * ctx->chroma_height);

  result = cuMemAllocHost((void**) &ctx->dst_buffer, ctx->dst_nbytes);
  if (CUDA_SUCCESS != result) {
    TRAE("Failed to allcoate a buffer for the decoded picture; we need %u bytes.", ctx->dst_nbytes);
    return 0;
  }

  if (NULL == ctx->dst_buffer) {
    TRAE("Failed to allocate the destination buffer for decoded frames.");
    return 0;
  }

  if (0 == fmt->progressive_sequence) {
    interlace_mode = cudaVideoDeinterlaceMode_Adaptive;
  }

  cfg.ulWidth = fmt->coded_width;
  cfg.ulHeight = fmt->coded_height;
  cfg.ulNumDecodeSurfaces = fmt->min_num_decode_surfaces;
  cfg.CodecType = fmt->codec;
  cfg.ChromaFormat = fmt->chroma_format;
  cfg.ulCreationFlags = cudaVideoCreate_Default;
  cfg.bitDepthMinus8 = fmt->bit_depth_luma_minus8;
  cfg.ulIntraDecodeOnly = 0;
  cfg.ulMaxWidth = fmt->coded_width;
  cfg.ulMaxHeight = fmt->coded_height;
  cfg.display_area.left = fmt->display_area.left;
  cfg.display_area.top = fmt->display_area.top;
  cfg.display_area.right = fmt->display_area.right;
  cfg.display_area.bottom = fmt->display_area.bottom;
  cfg.OutputFormat = cudaVideoSurfaceFormat_NV12;
  cfg.DeinterlaceMode = interlace_mode;
  cfg.ulTargetWidth = fmt->coded_width;
  cfg.ulTargetHeight = fmt->coded_height;
  cfg.ulNumOutputSurfaces = 2;
  cfg.target_rect.left = 0;
  cfg.target_rect.top = 0;
  cfg.target_rect.right = fmt->coded_width;
  cfg.target_rect.bottom = fmt->coded_height;

  result = cuvidCreateDecoder(&ctx->video_decoder, &cfg);
  if (CUDA_SUCCESS != result) {
    TRAE("Failed to create the video decoder.");
    return 0;
  }

  return cfg.ulNumDecodeSurfaces;
}

static int nvdec_decode_callback(void* user, CUVIDPICPARAMS* pic) {

  nvdec_context* ctx = NULL;
  CUresult result = CUDA_SUCCESS;

  if (NULL == user) {
    TRAE("Cannot decode as the given `void*` for the user data is NULL.");
    return 0;
  }

  if (NULL == pic) {
    TRAE("Cannot decode as the given `CUVIDPICPARAMS*` is NULL.");
    return 0;
  }

  ctx = (nvdec_context*) user;

  if (NULL == ctx->video_decoder) {
    TRAE("Cannot decode as the `nvdec_context::video_decoder` is NULL. Already shutdown while decoding was in flight?");
    return 0;
  }

  result = cuvidDecodePicture(ctx->video_decoder, pic);
  if (CUDA_SUCCESS != result) {
    TRAE("Failed to decode a picture.");
    return 0;
  }

  return 1; /* 0 = fail, 1 = succeeded */
}

/* 

   This function is called whenever the decoder has a picture
   that we can either display or pass into a next step of a
   transcoding pipeline.
   
   HOW TO NICELY HANDLE ERRORS:

     Before we can copy the decoded picture we have to map it
     using `cuvidMapVideoFrame()`. When we successfully mapped a
     picture we have to make to unmap it too.  After mapping,
     several things can go wrong, like copying the data or
     allocating space for the destination. To nicely 
     unmap different solutions are used. 

     [This][0] implementation from NVIDIA uses exceptions
     although it seems like they don't unmap the frame for each
     time they might throw an error; I'm not sure if they unmap a
     frame somewhere else, but the code is not very clear.

     [This][1] solution from QT explicitly unmaps the frame for
     each error case, which results in a lot of duplicated, but
     tbh readable code.

     [This][2] uses a goto and a boolean to unmap; though the
     nested branches make the code less readable. They use a
     similar approach as the [FFmpeg][4] implementation.

     I like [the FFmpeg][4] approach best, they use the mapped
     pointer to check if the frame was mapped; which means one
     less state to keep track of which is done in [this][2]
     implementation.

   TODO:

   - The documentation uses an example where it passes an 
     `unsigned long long` for the device pointer; the 
     `cuvidMapVideoFrame()` defines `unsigned int*`...
     Then we also have [this][2] implementation which 
     uses a `CUdeviceptr`. Nice :)

   @return 
   
     0    = fail
     >= 1 = succeeded

 */
static int nvdec_display_callback(void* user, CUVIDPARSERDISPINFO* info) {

  tra_video_frame video_frame = {};
  CUVIDGETDECODESTATUS status_cfg = {};
  CUDA_MEMCPY2D copy_info = {};
  CUresult result = CUDA_SUCCESS;
  const char* err_str = NULL;
  nvdec_context* ctx = NULL;
  int r = 0; 

  CUVIDPROCPARAMS src_params = {};
  uint32_t src_pitch = 0;
  CUdeviceptr src_ptr = 0;

  if (NULL == user) {
    TRAE("Cannot handle the display callback as the `void*` user pointer is NULL.");
    return 0;
  }

  if (NULL == info) {
    TRAE("Cannot handle the display callback as the `CUVIDPARSERDISPINFO*` is NULL.");
    return 0;
  }

  ctx = (nvdec_context*) user;
  
  if (NULL == ctx->video_decoder) {
    TRAE("Cannot handle the display callback. The `video_decoder` member of our `nvdec_context` is NULL. Already shutdown?");
    return 0;
  }

  if (0 == ctx->coded_width) {
    TRAE("Cannot handle display callback, `coded_width` is 0.");
    return 0;
  }
  
  if (0 == ctx->coded_height) {
    TRAE("Cannot handle display callback, `coded_height` is 0.");
    return 0;
  }

  if (NULL == ctx->dst_buffer) {
    TRAE("Cannot handle display callback, `dst_buffer` is NULL.");
    return 0;
  }

  if (NULL == ctx->callbacks.on_decoded_data) {
    TRAE("No `on_decoded_data` callback set; makes no sense to decode.");
    return 0;
  }

  /* Based on [this][0] and [this][1] implementation. */
  src_params.progressive_frame = info->progressive_frame;
  src_params.second_field = info->repeat_first_field + 1;
  src_params.unpaired_field = info->repeat_first_field < 0;
  src_params.top_field_first = info->top_field_first;
  //src_params.output_stream = ctx->stream; /* @todo do we actually need this ... */

  /* Map the video frame. */
  result = cuvidMapVideoFrame(
    ctx->video_decoder,
    info->picture_index,
    &src_ptr,
    &src_pitch,
    &src_params
  );
    
  if (CUDA_SUCCESS != result) {
    cuGetErrorString(result, &err_str);
    TRAE("Failed to map a decoded picture for index %d: %s.", info->picture_index, err_str);
    r = 0;
    goto error;
  }

  result = cuvidGetDecodeStatus(
    ctx->video_decoder,
    info->picture_index,
    &status_cfg
  );

  if (CUDA_SUCCESS != result) {
    TRAE("Failed to get the decode status.");
    r = 0;
    goto error;
  }

  if (status_cfg.decodeStatus == cuvidDecodeStatus_Error
      || status_cfg.decodeStatus == cuvidDecodeStatus_Error_Concealed)
    {
      TRAE("There was a decode error.");
      r = 0;
      goto error;
    }

  /* Y-plane */
  copy_info.srcMemoryType = CU_MEMORYTYPE_DEVICE;
  copy_info.srcDevice = src_ptr;
  copy_info.srcPitch = src_pitch;
  copy_info.dstMemoryType = CU_MEMORYTYPE_HOST;
  copy_info.dstHost = ctx->dst_buffer;
  copy_info.dstDevice = (CUdeviceptr) ctx->dst_buffer;
  copy_info.dstPitch = ctx->coded_width;
  copy_info.WidthInBytes = ctx->coded_width;
  copy_info.Height = ctx->luma_height;

  result = cuMemcpy2D(&copy_info); 
  if (CUDA_SUCCESS != result) {
    TRAE("Failed to copy the Y-plane.");
    r = 0;
    goto error;
  }

  /* UV-plane */
  copy_info.srcDevice = (CUdeviceptr)((uint8_t*) src_ptr + src_pitch * ctx->luma_height);
  copy_info.dstHost = (CUdeviceptr)((uint8_t*) ctx->dst_buffer + ctx->coded_width * ctx->luma_height);
  copy_info.Height = ctx->chroma_height;

  result = cuMemcpy2D(&copy_info); 
  if (CUDA_SUCCESS != result) {
    TRAE("Failed to copy the UV-plane.");
    r = 0;
    goto error;
  }

  /* Notify the client that we've go ta decoded frame. */
  video_frame.data = ctx->dst_buffer;
  video_frame.nbytes = ctx->dst_nbytes;

  r = ctx->callbacks.on_decoded_data(&video_frame, ctx->callbacks.user);
  if (r < 0) {
    TRAE("The client failed to handle the decoded data.");
    /* @todo should we handle this case differently? */
  }

 error:

    /* When ready we can unmap the frame again. */
    if (0 != src_ptr) {
      result = cuvidUnmapVideoFrame(ctx->video_decoder, src_ptr);
      if (CUDA_SUCCESS != result) {
        cuGetErrorString(result, &err_str);
        TRAE("Failed to unmap a video frame we've just mapped: %s", err_str);
      }
    }
    
  return r;
}

/* ------------------------------------------------------- */

static const char* nvenc_get_name() {
  return "nvenc";
}
  
static const char* nvenc_get_author() {
  return "roxlu";
}

/*

  This will create a new encoder session, based on the [NVENC
  Programmers Guide][6]. We first load the API functions through
  `NvEncodeAPICreateInstance` and then create the session. 

  According to the [programmers guide][6], we first have to open
  an encode session using `nvEncOpenEncodeSessionEx()` and then
  initialize it using nvEncInitializeEncoder()` .  When we
  initialize the encoder we have to specify the initialization
  parameters. The documentation describes several steps you 
  can take to retrieve the capabilities it support that you can 
  use when initializing the encoder.

  When you initialize the encoder, you can set the `presetGUID`
  on the initialization structure. You can also use
  `nvEncGetEncodePresetConfig()`, then make some changes to the
  preset and set the `encodeConfig` member of the init struct;
  you only have to do this when you want to customize the preset.

  See my forum post:
  https://forums.developer.nvidia.com/t/setting-nv-enc-initialize-params-encodeconfig-to-null-results-in-nv-enc-err-invalid-device-error/209302


 */
static int nvenc_create(
  tra_encoder_callbacks* callbacks,
  rx_dict* cfg,
  void* settings,
  tra_encoder_object** inst
)
{

  CUdevice tmp_cu_dev = 0;
  CUcontext tmp_cu_ctx = NULL;
  CUresult tmp_result = CUDA_SUCCESS;
  
  NV_ENC_CREATE_BITSTREAM_BUFFER create_buffer_info = {};
  NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS session_cfg = {};
  NV_ENC_INITIALIZE_PARAMS init_cfg = {};
  NV_ENC_CONFIG enc_cfg = {};
  NV_ENC_PRESET_CONFIG preset_cfg = {};
  NV_ENC_REGISTER_RESOURCE input_reg = {} ;
  NVENCSTATUS status = NV_ENC_SUCCESS;
  CUresult result = CUDA_SUCCESS;
  tra_nvenc_settings* enc_settings = NULL;
  nvenc_context* ctx = NULL;
  CUdeviceptr input_ptr = NULL;
  uint32_t input_pitch = 0;
  uint32_t i = 0;
  int r = 0;

  TRAI("Create nvenc api.");
  
  if (NULL == callbacks) {
    TRAE("Cannot create the NVIDIA encoder, the given `tra_encoder_callbacks` is NULL.");
    return -1;
  }

  if (NULL == callbacks->on_encoded_data) {
    TRAE("Cannot create te NVIDIA encoder, teh given `tra_encoder_callbacks::on_encoded_data` is NULL. Set this to a function that receives encoded data.");
    return -2;
  }

  enc_settings = (tra_nvenc_settings*) settings;
  if (NULL == enc_settings) {
    TRAE("Cannot create the NVIDIA encoder, no `tra_nvenc_settings` given.");
    return -3;
  }

  if (NULL == enc_settings->cuda_ctx) {
    TRAE("Cannot create the NVIDIA encoder, no `tra_nvenc_settings::cuda_ctx` set.");
    return -5;
  }

  ctx = calloc(1, sizeof(nvenc_context));
  if (NULL == ctx) {
    TRAE("Failed to allocate our internal `nvenc_context`. ");
    return -5;
  }

  ctx->callbacks.on_encoded_data = callbacks->on_encoded_data;
  ctx->callbacks.user = callbacks->user;
  ctx->funcs.version = NV_ENCODE_API_FUNCTION_LIST_VER;
  ctx->funcs.reserved = 0;

  /* 
     @todo: we've hardcoded these values atm; they are used to
     allocate the input buffers. We use a similar approach as
     `NvEncoderCuda::AllocateInputBuffers()` where we allocate
     the buffers based on the input format; currently we only
     support NV12.
  */
  ctx->input_format = NV_ENC_BUFFER_FORMAT_NV12; /* @todo currently hardcoded; we use the buffer format to calculate buffer sizes when we allocate our input buffers. This is similar to the cuda examples. */
  ctx->input_index = 0;
  ctx->coded_width = 1280;
  ctx->coded_height = 720;

  switch (ctx->input_format) {
    case NV_ENC_BUFFER_FORMAT_NV12: {
      ctx->luma_height = ctx->coded_height;
      ctx->chroma_height = ctx->coded_height * 0.5;
      break;
    }
    default: {
      TRAE("Unsupported buffer format.");
      r = -6;
      goto error;
    }
  }

  /* Load the functions. */
  status = NvEncodeAPICreateInstance(&ctx->funcs);
  if (NV_ENC_SUCCESS != status) {
    TRAE("Failed to load the encoder functions.");
    r = -6;
    goto error;
  }

  /* @todo remove */
  tmp_result = cuDeviceGet(&tmp_cu_dev, 0); if (CUDA_SUCCESS != tmp_result) { TRAE("Failed to create device."); }
  tmp_result = cuCtxCreate(&tmp_cu_ctx, 0, tmp_cu_dev); if (CUDA_SUCCESS != tmp_result) { TRAE("Failed to create context."); }

  /* Create encoder session. */
  session_cfg.version = NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS_VER;
  session_cfg.deviceType = NV_ENC_DEVICE_TYPE_CUDA;
  session_cfg.device = *enc_settings->cuda_ctx;
  session_cfg.device = tmp_cu_ctx; /* @todo testing. */
  session_cfg.apiVersion = NVENCAPI_VERSION;

  status = ctx->funcs.nvEncOpenEncodeSessionEx(&session_cfg, &ctx->session);
  if (NV_ENC_SUCCESS != status) {
    TRAE("Failed to create the encoder session.");
    r = -7;
    goto error;
  }

  if (NULL == ctx->session) {
    TRAE("Failed to create the encoder session; the created session is NULL.");
    r = -8;
    goto error;
  }

  nvenc_print_inputformats(ctx, NV_ENC_CODEC_H264_GUID);
  
  TRAD("|> FEEDING: %p", *enc_settings->cuda_ctx);

  preset_cfg.version = NV_ENC_PRESET_CONFIG_VER;
  preset_cfg.presetCfg.version = NV_ENC_CONFIG_VER;

  status = ctx->funcs.nvEncGetEncodePresetConfigEx(
    ctx->session,
    NV_ENC_CODEC_H264_GUID,
    NV_ENC_PRESET_P3_GUID,
    NV_ENC_TUNING_INFO_ULTRA_LOW_LATENCY,
    &preset_cfg
  );

  if (NV_ENC_SUCCESS != status) {
    TRAE("Failed to get the encoder preset config.");
    r = -9;
    goto error;
  }
  
  /* Setup the initialization settings, based on `NvEncoder::CreateDefaultEncoderParams()`. */
  init_cfg.version = NV_ENC_INITIALIZE_PARAMS_VER;
  init_cfg.encodeGUID = NV_ENC_CODEC_H264_GUID;
  init_cfg.presetGUID = NV_ENC_PRESET_P3_GUID; /* @todo make this settings for the user. */
  init_cfg.encodeWidth = 1280; /* @todo */
  init_cfg.encodeHeight = 720; /* @todo */
  init_cfg.darWidth = 1280; /* @todo */
  init_cfg.darHeight = 720; /* @todo */
  init_cfg.frameRateNum = 25; /* @todo */
  init_cfg.frameRateDen = 1; /* @todo */
  init_cfg.enableEncodeAsync = 0; /* Asynchronous mode is only supported on Windows. */
  init_cfg.enablePTD = 1; /* Picture Type Decision, see `CreateDefaultEncoderParams()` in `NvEncoder.cpp`  */
  init_cfg.reportSliceOffsets = 0;
  init_cfg.enableSubFrameWrite = 0;
  init_cfg.enableExternalMEHints = 0;
  init_cfg.enableMEOnlyMode = 0;
  init_cfg.enableWeightedPrediction = 0;
  init_cfg.enableOutputInVidmem = 0;
  init_cfg.reservedBitFields = 0;
  init_cfg.privDataSize = 0;
  init_cfg.privData = NULL;
  init_cfg.encodeConfig = &preset_cfg.presetCfg; /* @todo: Although the documentation says you can use `NULL` here, we receive an `NV_ENC_ERR_INVALID_DEVICE` when we don't supply a config.  */
  init_cfg.maxEncodeWidth = 1280; /* @todo */
  init_cfg.maxEncodeHeight = 720; /* @todo */
  init_cfg.bufferFormat = ctx->input_format; /* See nvEncodeAPI.h: should only be set when D3D12 interface type is used. */
  init_cfg.tuningInfo = NV_ENC_TUNING_INFO_ULTRA_LOW_LATENCY; /* @todo */

  nvenc_print_initparams(&init_cfg);

  status = ctx->funcs.nvEncInitializeEncoder(ctx->session, &init_cfg);
  if (NV_ENC_SUCCESS != status) {
    TRAE("Failed to initialize the encoder: %s", nvenc_status_to_string(status));
    r = -9;
    goto error;
  }

  /* Create the buffers for the output. */
  ctx->num_output_buffers = init_cfg.encodeConfig->frameIntervalP + init_cfg.encodeConfig->rcParams.lookaheadDepth;
  ctx->num_input_buffers = ctx->num_output_buffers;
  
  if (0 == ctx->num_output_buffers) {
    TRAE("The calculated number of bitstream buffers we should allocate is 0.");
    r = -10;
    goto error;
  }

  TRAD("Number of bitbuffers to allocate: %u", ctx->num_output_buffers);
  
  ctx->output_buffers = malloc(ctx->num_output_buffers * (sizeof(NV_ENC_OUTPUT_PTR)));
  if (NULL == ctx->output_buffers) {
    TRAE("Failed to allocate the array for our output buffers.");
    r = -11;
    goto error;
  }
  
  for (i = 0; i < ctx->num_output_buffers; ++i) {

    create_buffer_info.version = NV_ENC_CREATE_BITSTREAM_BUFFER_VER;
    
    status = ctx->funcs.nvEncCreateBitstreamBuffer(ctx->session, &create_buffer_info);
    if (NV_ENC_SUCCESS != status) {
      TRAE("Failed to create a bitstream buffer.");
      r = -12;
      goto error;
    }

    if (NULL == create_buffer_info.bitstreamBuffer) {
      TRAE("The bitstream buffer that was created is NULL.");
      r = -13;
      goto error;
    }

    ctx->output_buffers[i] = create_buffer_info.bitstreamBuffer;
  }

  /* Create the input buffers */
  ctx->input_buffers = calloc(1, ctx->num_input_buffers * sizeof(CUdeviceptr));
  if (NULL == ctx->input_buffers) {
    TRAE("Failed to allocate the input buffers.");
    r = -14;
    goto error;
  }

  /* Based on NvEncoderCuda.cpp from the Samples. */
  for (i = 0; i < ctx->num_input_buffers; ++i) {
    
    result = cuMemAllocPitch(
      &input_ptr,
      &ctx->input_pitch,
      ctx->coded_width, /* @todo this will only work for NV12 and similar */
      ctx->luma_height + ctx->chroma_height,
      16
    );

    if (CUDA_SUCCESS != result) {
      TRAE("Failed to allocate an input buffer.");
      r = -15;
      goto error;
    }

    ctx->input_buffers[i] = input_ptr;
  }

  /* 
     Register the input buffers; 

     @todo this is only required when we use externally allocated buffers.

  */
  ctx->num_input_regs = ctx->num_input_buffers;

  ctx->input_regs = calloc(1, ctx->num_input_regs * sizeof(NV_ENC_REGISTERED_PTR));
  if (NULL == ctx->input_regs) {
    TRAE("Failed to allocate the buffer for the input registrations.");
    r = -16;
    goto error;
  }
  
  for (i = 0; i < ctx->num_input_regs; ++i) {

    input_reg.version = NV_ENC_REGISTER_RESOURCE_VER;
    input_reg.resourceType = NV_ENC_INPUT_RESOURCE_TYPE_CUDADEVICEPTR; /* @todo currently we are testing with cuda encode/decode */
    input_reg.width = ctx->coded_width;
    input_reg.height = ctx->coded_height;
    input_reg.pitch = ctx->input_pitch;
    input_reg.subResourceIndex = 0;
    input_reg.resourceToRegister = (void*) ctx->input_buffers[i];
    input_reg.bufferFormat = ctx->input_format;
    input_reg.bufferUsage = NV_ENC_INPUT_IMAGE;
    input_reg.pInputFencePoint = NULL;
    input_reg.pOutputFencePoint = NULL;

    status = ctx->funcs.nvEncRegisterResource(ctx->session, &input_reg);
    if (NV_ENC_SUCCESS != status) {
      TRAE("Failed to register an input resource.");
      r = -17;
      goto error;
    }

    if (NULL == input_reg.registeredResource) {
      TRAE("Failed to retrieve a pointer to the registered resourcde.");
      r = -18;
      goto error;
    }
    
    ctx->input_regs[i] = input_reg.registeredResource;
  }
   
  /* Finally assign the result. */
  *inst = (tra_encoder_object*) ctx;

 error:

  /* When an error occurs we have to cleanup the encoder. */
  if (r < 0) {

    if (NULL != ctx) {
      
      r = nvenc_destroy((tra_encoder_object*) ctx);
      if (r < 0) {
        TRAE("After we've failed to create an encoder instance, we also failed to cleanly destroy it.");
      }

      ctx = NULL;
    }

    if (NULL != inst) {
      *inst = NULL;
    }
  }
  
  return r;
}
  
static int nvenc_destroy(tra_encoder_object* inst) {

  NVENCSTATUS status = NV_ENC_SUCCESS;
  nvenc_context* ctx = NULL;
  int ret = 0;
  
  if (NULL == inst) {
    TRAE("Cannot destroy the `nvenc_context` because the given `tra_encoder_object*` is NULL.");
    return -1;
  }

  ctx = (nvenc_context*) inst;

  /* Destroy the encoder session. */
  if (NULL != ctx->session) {
    if (NULL != ctx->funcs.nvEncDestroyEncoder) {
      status = ctx->funcs.nvEncDestroyEncoder(ctx->session);
      if (NV_ENC_SUCCESS != status) {
        TRAE("Failed to cleanly destroy the encoder session.");
        ret -= 2;
      }
    }
  }

  TRAE("@todo destroy input buffers.");
  TRAE("@todo destroy output buffers.");
  TRAE("@todo destroy input resource regs.");

  free(ctx);
  ctx = NULL;
  
  return ret;
}
  
static int nvenc_encode(tra_encoder_object* inst, tra_video_frame* frame) {

  NV_ENC_LOCK_BITSTREAM lock_bitstream = {};
  NV_ENC_MAP_INPUT_RESOURCE mapped = {};
  tra_encoded_data encoded_data = {};
  NV_ENC_PIC_PARAMS pic = {};
  NVENCSTATUS status = NV_ENC_SUCCESS;
  CUresult result = CUDA_SUCCESS;
  nvenc_context* ctx = NULL;
  CUDA_MEMCPY2D mem = {};
  uint32_t index = 0;
  int r = 0;
  
  if (NULL == inst) {
    TRAE("Cannot encode as the given `tra_encoder_object` is NULL.");
    return -1;
  }

  if (NULL == frame) {
    TRAE("Cannot encode as the given `tra_vide_frame*` is NULL.");
    return -2;
  }

  ctx = (nvenc_context*) inst;
  
  if (NULL == ctx->session) {
    TRAE("Cannot encode as the encoder session is NULL.");
    return -3;
  }

  if (NULL == ctx->callbacks.on_encoded_data) {
    TRAE("Cannot encode data as our `on_encoded_data()` callback is not set.");
    return -4;
  }

  /* Get and update the input buffer index. */
  index = ctx->input_index;
  ctx->input_index = (ctx->input_index + 1) % ctx->num_input_buffers;

  /* 
     Copy the LUMA data into he buffer. 
  
     @todo:

     - this is based on `NvEncoderCuda.cpp::CopyToDeviceFrame()`. 
     - It seems to be an "aligned" copy; I've to research what kind of copies we can make.
     - Currently the transfer is fully hardcoded; we have to abstract this.
     - Look into asynchronous copies.
     - Handle different chroma formats.
     - Do we need to push/pop the context?

  */
  mem.srcMemoryType = CU_MEMORYTYPE_HOST; 
  mem.srcPitch = ctx->coded_width; 
  mem.srcDevice = NULL; 
  mem.srcHost = frame->data;
  mem.dstMemoryType = CU_MEMORYTYPE_DEVICE; /* We copy from CPU to GPU */
  mem.dstDevice = ctx->input_buffers[index]; /* The pointer to the destination buffer. */
  mem.dstPitch = ctx->input_pitch; 
  mem.WidthInBytes = ctx->coded_width; 
  mem.Height = ctx->luma_height;

  result = cuMemcpy2D(&mem);
  if (CUDA_SUCCESS != result) {
    TRAE("Failed to copy the luma data into the buffer.");
    return -4;
  }

  /* Copy the CHROMA data ito the buffer. */
  mem.srcHost = frame->data + (ctx->coded_width * ctx->coded_height);
  mem.dstDevice = (CUdeviceptr) ((uint8_t*)ctx->input_buffers[index]) + ctx->coded_height * ctx->input_pitch; /* @todo */
  mem.dstPitch = ctx->input_pitch;
  mem.WidthInBytes = ctx->coded_width;
  mem.Height = ctx->chroma_height;

  result = cuMemcpy2D(&mem);
  if (CUDA_SUCCESS != result) {
    TRAE("Failed to copy the chroma data into the buffer.");
    return -5;
  }

  /* 
     At this point we've copied the given data to the input
     buffer. Now, we have to map the input resource. The input
     resource is used as a "link" between an input handle and the
     input buffer.
  */
  mapped.version = NV_ENC_MAP_INPUT_RESOURCE_VER;
  mapped.subResourceIndex = 0; /* deprecated */
  mapped.inputResource = NULL; /* deprecated */
  mapped.registeredResource = ctx->input_regs[index];
  mapped.mappedResource = NULL; /* [out] */

  if (NULL == mapped.registeredResource) {
    TRAE("Cannot map the resource as it's NULL.");
    return -6;
  }

  TRAD("Mapping input resource: %u, %p", index, mapped.registeredResource);
  
  status = ctx->funcs.nvEncMapInputResource(ctx->session, &mapped);
  if (NV_ENC_SUCCESS != status) {
    TRAE("Failed to map an input resource.");
    return -7;
  }

  TRAD("mapped: %p", mapped.mappedResource);

  /* Once we have a mapped resource we can encode it using a NV_ENC_PIC_PARAMS. */
  pic.version = NV_ENC_PIC_PARAMS_VER;
  pic.inputWidth = ctx->coded_width;
  pic.inputHeight = ctx->coded_height;
  pic.inputPitch = ctx->input_pitch;
  pic.encodePicFlags = 0; /* @todo */
  pic.frameIdx = 0; /* @todo */
  pic.inputTimeStamp = 0; /* @todo */
  pic.inputDuration = 0; /* @todo */
  pic.inputBuffer = mapped.mappedResource; /* `NV_ENC_INPUT_PTR`. */
  pic.outputBitstream = ctx->output_buffers[index];
  pic.completionEvent = NULL;
  pic.bufferFmt = ctx->input_format;
  pic.pictureStruct = NV_ENC_PIC_STRUCT_FRAME;
  pic.pictureType = 0; /* @todo */
  
  status = ctx->funcs.nvEncEncodePicture(ctx->session, &pic);
  if (NV_ENC_SUCCESS != status) {
    TRAE("Failed to encode a picture.");
    /* @todo use goto, for now fall through so we can unmap. */
  }

  lock_bitstream.version = NV_ENC_LOCK_BITSTREAM_VER;
  lock_bitstream.outputBitstream = ctx->output_buffers[index];
  lock_bitstream.doNotWait = 1;

  status = ctx->funcs.nvEncLockBitstream(ctx->session, &lock_bitstream);
  if (NV_ENC_SUCCESS != status) {
    /* @todo use goto, for now fall through so we can unmap */
    TRAE("Failed to lock the bitstream. ");
  }

  status = ctx->funcs.nvEncUnlockBitstream(ctx->session, lock_bitstream.outputBitstream);
  if (NV_ENC_SUCCESS != status) {
    TRAE("Failed to unlock the bitstream.");
    /* @todo use goto, for now fall through ... */
  }

  encoded_data.data = (uint8_t*) lock_bitstream.bitstreamBufferPtr;
  encoded_data.nbytes = lock_bitstream.bitstreamSizeInBytes;

  r = ctx->callbacks.on_encoded_data(&encoded_data, ctx->callbacks.user);
  if (r < 0) {
    TRAE("The handler failed to handle the encoded data nicely.");
    /* ... don't do anything at this point ... */
  }
  
  {
    uint8_t* ptr = (uint8_t*) lock_bitstream.bitstreamBufferPtr;
    TRAD("|> %02x %02x %02x %02x",
        ptr[0], ptr[1], ptr[2], ptr[3]
    );
  }
  
  status = ctx->funcs.nvEncUnmapInputResource(ctx->session, mapped.mappedResource);
  if (NV_ENC_SUCCESS != status) {
    TRAE("Failed to unmap an input resource.");
    return -7;
  }

  TRAD("Lets encode ... %u bytes", frame->nbytes);
  
  return 0;
}

/* ------------------------------------------------------- */

int tra_load(tra_registry* reg) {

  TRAI("Registering the NVIDIA module.");

  int r = 0;

  if (NULL == reg) {
    TRAE("Cannot register the NVIDIA plugins, given `tra_registry` is NULL.");
    return -1;
  }
  
  r = tra_registry_add_api(reg, "cuda", &cuda_api);
  if (r < 0) {
    TRAE("Failed to register the `cuda` api.");
    return -2;
  }

  r = tra_registry_add_decoder_api(reg, &nvdec_api);
  if (r < 0) {
    TRAE("Failed to register the `nvdec` api.");
    return -3;
  }

  r = tra_registry_add_encoder_api(reg, &nvenc_api);
  if (r < 0) {
    TRAE("Failed to register the `nvenc` api.");
    return -4;
  }
  
  return 0;
}

/* ------------------------------------------------------- */

static tra_cuda_api cuda_api = {
  .device_list = cuda_device_list,
  .device_get = cuda_device_get,
  .context_create = cuda_context_create,
  .context_destroy = cuda_context_destroy,
};

static tra_decoder_api nvdec_api = {
  .get_name = nvdec_get_name,
  .get_author = nvdec_get_author,
  .create = nvdec_create,
  .destroy = nvdec_destroy,
  .decode = nvdec_decode,
};

static tra_encoder_api nvenc_api = {
  .get_name = nvenc_get_name,
  .get_author = nvenc_get_author,
  .create = nvenc_create,
  .destroy = nvenc_destroy,
  .encode = nvenc_encode,
};

/* ------------------------------------------------------- */

