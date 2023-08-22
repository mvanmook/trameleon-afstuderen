/* ------------------------------------------------------- */

#include <cuda.h>
#include <tra/modules/nvidia/nvidia-enc.h>
#include <tra/modules/nvidia/nvidia-utils.h>
#include <tra/modules/nvidia/nvidia.h>
#include <tra/modules/nvidia/nvEncodeAPI.h>
#include <tra/modules/nvidia/cuviddec.h>
#include <tra/modules/nvidia/nvcuvid.h>
#include <tra/profiler.h>
#include <tra/registry.h>
#include <tra/module.h>
#include <tra/types.h>
#include <tra/log.h>
#include <tra/buffer.h>

/* ------------------------------------------------------- */

typedef struct tra_nvenc_registration tra_nvenc_registration;

/* ------------------------------------------------------- */

static tra_encoder_api encoder_api;

/* ------------------------------------------------------- */

typedef struct tra_nvenc {

  tra_encoder_settings settings;
  NV_ENCODE_API_FUNCTION_LIST funcs;       /* NVIDIA encoder functions, loaded through the call to `NvEncodeAPICreateInstance()`. */
  void* session;                           /* The encoder session. */
  uint32_t coded_width;                    /* The width of the input stream. */
  uint32_t coded_height;                   /* The height of the input stream */

  /* Output */
  NV_ENC_OUTPUT_PTR* output_buffers;       /* Array to bit stream output buffer pointers. */
  uint32_t output_count;                   /* Number of bit stream output buffers. */
  uint32_t output_index;                   /* The output index that we use to write the encoded bitstream into. */

  /* Input */
  uint32_t input_count;                    /* The number of preferred input buffers that should be used when feeding buffers into the encoder. These encoder variants (e.g. OpenGL, CUDA, etc.) use this number to allocate the buffers that they feed into the encoder. */
  NV_ENC_BUFFER_FORMAT input_format;       /* The pixel format that the encoder expects. */
  
} tra_nvenc;

/* ------------------------------------------------------- */

struct tra_nvenc_resources {
  tra_nvenc_registration* reg_array;       /* Array of registrations that the `tra_nvenc_resources` manages. */
  uint32_t reg_count;                      /* Number of elements in `resources` and `reg_array`. */
  uint32_t reg_index;                      /* The current index into `reg_array` that we can use as input when encoding. */
  tra_nvenc* encoder;                      /* The encoder to which we register resources. Is copied from the settings when we create an instance. */                            
};

/* ------------------------------------------------------- */

struct tra_nvenc_registration {
  uint32_t external_stride;                /* The pitch of the external resource; e.g. CUDA buffer pitch, etc. */
  void* external_ptr;                      /* The external pointer that was registered. This is used to find the `NV_ENC_REGISTERED_PTR` based on an external pointer (like a CUDA, CUdeviceptr). */
  NV_ENC_REGISTERED_PTR* registered_ptr;   /* The pointer that NVENC that we got when registering `external_ptr`. */
};

/* ------------------------------------------------------- */

static int tra_nvenc_print_inputformats(tra_nvenc* ctx, GUID encodeGuid);

/* ------------------------------------------------------- */

/*

  This will create a new encoder session, based on the [NVENC
  Programmers Guide][2]. We first load the API functions through
  `NvEncodeAPICreateInstance` and then create the session. 

  According to the [programmers guide][2], we first have to open
  an encode session using `nvEncOpenEncodeSessionEx()` and then
  initialize it using nvEncInitializeEncoder()`.  When we
  initialize the encoder we have to specify the initialization
  parameters. The documentation describes several steps you 
  can take to retrieve the capabilities it supports that you can 
  use when initializing the encoder.

  When you initialize the encoder, you can set the `presetGUID`
  on the initialization structure. You can also use
  `nvEncGetEncodePresetConfig()`, then make some changes to the
  preset and set the `encodeConfig` member of the init struct;
  you only have to do this when you want to customize the preset.
  See [my forum post][0].

  When we open the encoder session, we use
  `nvEncOpenEncodeSessionEx()` which expects a "Pointer to client
  device" for the `device` member of the
  `NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS`.  The documentation in
  the header (nvEncodeApi) is not really clear; It should be
  "Pointer to cuda context" or something as you have to pass a
  `CUcontext` pointer.  See [my forum post][1].

 REFERENCES:

   [0]: https://forums.developer.nvidia.com/t/setting-nv-enc-initialize-params-encodeconfig-to-null-results-in-nv-enc-err-invalid-device-error/209302 
   [1]: https://forums.developer.nvidia.com/t/nv-enc-open-encode-sessionex-params-device-is-not-a-cudevice-but-cucontext-update-docs-maybe/219910
   [2]: https://docs.nvidia.com/video-technologies/video-codec-sdk/nvenc-video-encoder-api-prog-guide/index.html "Programmers Guide"
   
 */
int tra_nvenc_create(
  tra_encoder_settings* cfg,
  tra_nvenc_settings* settings,
  tra_nvenc** ctx
)
{
  TRAP_TIMER_BEGIN(prof, "tra_nvenc_create");
  
  NV_ENC_BUFFER_FORMAT buffer_format = NV_ENC_BUFFER_FORMAT_UNDEFINED;
  NV_ENC_CREATE_BITSTREAM_BUFFER create_buffer_info = { 0 };
  NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS session_cfg = { 0 };
  NV_ENC_INITIALIZE_PARAMS init_cfg = { 0 };
  NV_ENC_PRESET_CONFIG preset_cfg = { 0 };
  NV_ENC_CONFIG enc_cfg = { 0 };
  NVENCSTATUS status = NV_ENC_SUCCESS;
  CUresult result = CUDA_SUCCESS;

  tra_nvenc* inst = NULL;
  uint32_t i = 0;
  int r = 0;

  if (NULL == cfg) {
    TRAE("Cannot create the NVIDIA encoder, the given `tra_encoder_settings` is NULL.");
    r = -10;
    goto error;
  }

  if (0 == cfg->image_width) {
    TRAE("Cannot create the NVIDIA encoder, the given `tra_encoder_settings.image_width` is 0.");
    r = -20;
    goto error;
  }

  if (0 == cfg->image_height) {
    TRAE("Cannot create the NVIDIA encoder, the given `tra_encoder_settings.image_height` is 0.");
    r = -30;
    goto error;
  }

  if (0 == cfg->fps_num) {
    TRAE("Cannot create the NVIDIA encoder, the given `tra_encoder_settings.fps_num` is 0.");
    r = -40;
    goto error;
  }

  if (0 == cfg->fps_den) {
    TRAE("Cannot create the NVIDIA encoder, the given `tra_encoder_settings.fps_den` is 0.");
    r = -50;
    goto error;
  }

  if (NULL == settings) {
    TRAE("Cannot create the NVIDIA encoder, the given `tra_nvenc_settings` is NULL.");
    r = -60;
    goto error;
  }

  if (NULL == settings->cuda_context_handle) {
    TRAE("Cannot create the NVIDIA encoder, the `tra_nvenc_settings.cuda_context_handle` is NULL.");
    r = -70;
    goto error;
  }

  if (NULL == settings->cuda_device_handle) {
    TRAE("Cannot create the NVIDIA encoder, the `tra_nvenc_settings.cuda_device_handle` is NULL.");
    r = -80;
    goto error;
  }

  if (NV_ENC_DEVICE_TYPE_CUDA != settings->device_type
      && NV_ENC_DEVICE_TYPE_OPENGL != settings->device_type)
    {
      TRAE("Cannot create the NVIDIA encoder, the given device type is not supported. ");
      r = -90;
      goto error;
    }

  if (NULL == cfg->callbacks.on_encoded_data) {
    TRAE("Cannot create te NVIDIA encoder, the given `tra_encoder_settings.callbacks.on_encoded_data` is NULL. Set this to a function that receives encoded data.");
    r = -100;
    goto error;
  }

  if (NULL == cfg->callbacks.on_flushed) {
    TRAE("Cannot create the NVIDIA encoder, the given `tra_encoder_settings.callbacks.on_flushed` is NULL. Set this to the function which is called when we've flushed the encoder.");
    r = -105;
    goto error;
  }

  if (NULL == ctx) {
    TRAE("Cannot create the NVIDIA encoder, the given result pointer is NULL.");
    r = -110;
    goto error;
  }

  if (NULL != *ctx) {
    TRAE("Cannot create the NVIDIA encoder, it seems that the result pointer is not NULL. Already created? Or not initialized to NULL?");
    r = -120;
    goto error;
  }

  /* Convert input `image_format` into a `NV_ENC_BUFFER_FORMAT`.  */
  r = tra_nv_imageformat_to_bufferformat(cfg->image_format, &buffer_format);
  if (r < 0) {
    TRAE("Cannot create the NVIDIA encoder, the given `image_format` is not supported.");
    r = -130;
    goto error;
  }

  inst = calloc(1, sizeof(tra_nvenc));
  if (NULL == inst) {
    TRAE("Failed to allocate `tra_nvenc`. Out of memory?");
    r = -140;
    goto error;
  }
  
  inst->settings = *cfg;
  inst->funcs.version = NV_ENCODE_API_FUNCTION_LIST_VER;
  inst->funcs.reserved = 0;

  /* 
     The `input_format` and `coded_{width,height}` are used when
     we send a buffer to the endoder using nvEncEncodePicture()`.
  */
  inst->input_format = buffer_format;
  inst->coded_width = cfg->image_width;
  inst->coded_height = cfg->image_height;

  /* Load the functions. */
  status = NvEncodeAPICreateInstance(&inst->funcs);
  if (NV_ENC_SUCCESS != status) {
    TRAE("Failed to load the encoder functions.");
    r = -150;
    goto error;
  }

  /* Create encoder session. */
  session_cfg.version = NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS_VER;
  session_cfg.deviceType = settings->device_type;
  session_cfg.device = *((CUcontext*)settings->cuda_context_handle); 
  session_cfg.apiVersion = NVENCAPI_VERSION;

  status = inst->funcs.nvEncOpenEncodeSessionEx(&session_cfg, &inst->session);
  if (NV_ENC_SUCCESS != status) {
    TRAE("Failed to create the encoder session: %s", tra_nv_status_to_string(status));
    r = -160;
    goto error;
  }

  if (NULL == inst->session) {
    TRAE("Failed to create the encoder session; the created session is NULL.");
    r = -170;
    goto error;
  }

  tra_nvenc_print_inputformats(inst, NV_ENC_CODEC_H264_GUID);

  /* Create preset config */
  preset_cfg.version = NV_ENC_PRESET_CONFIG_VER;
  preset_cfg.presetCfg.version = NV_ENC_CONFIG_VER;

  status = inst->funcs.nvEncGetEncodePresetConfigEx(
    inst->session,
    NV_ENC_CODEC_H264_GUID,
    NV_ENC_PRESET_P3_GUID,
    NV_ENC_TUNING_INFO_ULTRA_LOW_LATENCY,
    &preset_cfg
  );

  if (NV_ENC_SUCCESS != status) {
    TRAE("Failed to get the encoder preset config.");
    r = -180;
    goto error;
  }

  /* Setup the initialization settings, based on `NvEncoder::CreateDefaultEncoderParams()`. */
  init_cfg.version = NV_ENC_INITIALIZE_PARAMS_VER;
  init_cfg.encodeGUID = NV_ENC_CODEC_H264_GUID;
  init_cfg.presetGUID = NV_ENC_PRESET_P3_GUID; /* @todo make this settings for the user. */
  init_cfg.encodeWidth = inst->coded_width; 
  init_cfg.encodeHeight = inst->coded_height;
  init_cfg.darWidth = inst->coded_width; 
  init_cfg.darHeight = inst->coded_height; 
  init_cfg.frameRateNum = cfg->fps_num;
  init_cfg.frameRateDen = cfg->fps_den; 
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
  init_cfg.maxEncodeWidth = inst->coded_width; 
  init_cfg.maxEncodeHeight = inst->coded_height; 
  init_cfg.bufferFormat = inst->input_format; /* See nvEncodeAPI.h: should only be set when D3D12 interface type is used. */
  init_cfg.tuningInfo = NV_ENC_TUNING_INFO_ULTRA_LOW_LATENCY; /* @todo */

  tra_nv_print_initparams(&init_cfg);

  status = inst->funcs.nvEncInitializeEncoder(inst->session, &init_cfg);
  if (NV_ENC_SUCCESS != status) {
    TRAE("Failed to initialize the encoder: %s", tra_nv_status_to_string(status));
    r = -190;
    goto error;
  }

  /* Determine the size of the input and output buffers; based on `NvEncoder.cpp`. */
  inst->output_count = init_cfg.encodeConfig->frameIntervalP + init_cfg.encodeConfig->rcParams.lookaheadDepth;
  inst->input_count = inst->output_count;
  
  if (0 == inst->output_count) {
    TRAE("The calculated number of bitstream buffers we should allocate is 0.");
    r = -200;
    goto error;
  }

  /* Create the buffers for the output bitstream */
  inst->output_buffers = malloc(inst->output_count * (sizeof(NV_ENC_OUTPUT_PTR)));
  if (NULL == inst->output_buffers) {
    TRAE("Failed to allocate the array for our output buffers.");
    r = -210;
    goto error;
  }

  for (i = 0; i < inst->output_count; ++i) {

    create_buffer_info.version = NV_ENC_CREATE_BITSTREAM_BUFFER_VER;
    
    status = inst->funcs.nvEncCreateBitstreamBuffer(inst->session, &create_buffer_info);
    if (NV_ENC_SUCCESS != status) {
      TRAE("Failed to create a bitstream buffer.");
      r = -220;
      goto error;
    }

    if (NULL == create_buffer_info.bitstreamBuffer) {
      TRAE("The bitstream buffer that was created is NULL.");
      r = -230;
      goto error;
    }

    inst->output_buffers[i] = create_buffer_info.bitstreamBuffer;
  }

  /* Finally assign the result. */
  *ctx = inst;
  
 error:

  /* When an error occurs we have to cleanup the encoder. */
  if (r < 0) {

    if (NULL != inst) {
      
      r = tra_nvenc_destroy(inst);
      if (r < 0) {
        TRAE("After we've failed to create an encoder instance, we also failed to cleanly destroy it.");
      }

      inst = NULL;
    }

    if (NULL != ctx) {
      *ctx = NULL;
    }
  }

  TRAP_TIMER_END(prof, "tra_nvenc_create");

  return r;
}

/* ------------------------------------------------------- */

int tra_nvenc_destroy(tra_nvenc* ctx) {

  TRAP_TIMER_BEGIN(prof, "tra_nvenc_destroy");

  NVENCSTATUS status = NV_ENC_SUCCESS;
  uint32_t i = 0;
  int result = 0;
  
  if (NULL == ctx) {
    TRAE("Cannot destroy the `tra_nvenc` because the given `tra_nvenc*` is NULL.");
    return -1;
  }

  /* Destroy the bitstream buffers that are used to hold the output stream. */
  if (NULL != ctx->output_buffers) {
    for (i = 0; i < ctx->output_count; ++i) {
      status = ctx->funcs.nvEncDestroyBitstreamBuffer(ctx->session, ctx->output_buffers[i]);
      if (NV_ENC_SUCCESS != status) {
        TRAE("Failed to cleanly destroy a bitstream buffer (%u): %s.", i, tra_nv_status_to_string(status));
        result -= 5;
      }
    }
  }
  
  /* Destroy the encoder session. */
  if (NULL != ctx->session) {
    if (NULL != ctx->funcs.nvEncDestroyEncoder) {
      status = ctx->funcs.nvEncDestroyEncoder(ctx->session);
      if (NV_ENC_SUCCESS != status) {
        TRAE("Failed to cleanly destroy the encoder session.");
        result -= 20;
      }
    }
  }

  TRAE("@todo flush encoder.");

  ctx->coded_width = 0;
  ctx->coded_height = 0;
  ctx->session = NULL;
  ctx->output_buffers = NULL;
  ctx->output_count = 0;
  ctx->output_index = 0;
  ctx->input_count = 0;
  ctx->input_format = NV_ENC_BUFFER_FORMAT_UNDEFINED;
  
  free(ctx);
  ctx = NULL;

  TRAP_TIMER_END(prof, "tra_nvenc_destroy");
  
  return result;
}

/* ------------------------------------------------------- */

/*
  GENERAL INFO:
  
    This function will flush any queued data in the encoder. This
    might result in multiple calls to the callback that receives
    the encoded data.

    Note that we could have created a separate function that
    handles the call to `nvEncEncodePicture()` and also extracts
    the bitstream data. Though I currently I think it's better
    keep all the flush logic in this one function for clarity.

  REFERENCES:

    [0]: https://docs.nvidia.com/video-technologies/video-codec-sdk/nvenc-video-encoder-api-prog-guide/index.html#notifying-the-end-of-input-stream
    
 */
int tra_nvenc_flush(tra_nvenc* ctx) {

  NV_ENC_LOCK_BITSTREAM lock_bitstream = { 0 };
  tra_memory_h264 encoded_data = { 0 };
  NVENCSTATUS status = NV_ENC_SUCCESS;
  NV_ENC_PIC_PARAMS pic = { 0 };
  uint32_t index = 0;
  int is_locked = 0;
  int r = 0;

  /* ----------------------------------------------- */
  /* Validate                                        */
  /* ----------------------------------------------- */

  if (NULL == ctx) {
    TRAE("Cannot flush the encoder as the given `tra_nvenc*` is NULL.");
    r = -10;
    goto error;
  }

  if (NULL == ctx->session) {
    TRAE("Cannot flush the encoder as the `session` member of `tra_nvenc` is NULL.");
    r = -20;
    goto error;
  }

  if (NULL == ctx->settings.callbacks.on_flushed) {
    TRAE("Cannot flush the encoder as the `on_flushed` callback hasn't been set.");
    r = -30;
    goto error;
  }

  /* ----------------------------------------------- */
  /* Flush                                           */
  /* ----------------------------------------------- */

  /*
    To flush the encoder we have to make sure that all members of
    the `NV_ENC_PIC_PARAMS` have been set to zero and the
    `encodePicFlags` is set to `NV_ENC_PIC_FLAG_EOS` as described
    [here][0]. Though apparently the `version` has to be set too.
  */
  pic.version = NV_ENC_PIC_PARAMS_VER;
  pic.encodePicFlags = NV_ENC_PIC_FLAG_EOS;

  status = ctx->funcs.nvEncEncodePicture(ctx->session, &pic);
  if (NV_ENC_SUCCESS != status) {
    TRAE("Failed to trigger the flush: %s", tra_nv_status_to_string(status));
    r = -40;
    goto error;
  }

  /* ----------------------------------------------- */
  /* Extract bitstream data                          */
  /* ----------------------------------------------- */

  /* Select output buffer.  */
  index = ctx->output_index;
  ctx->output_index = (index + 1) % ctx->output_count;
  
  lock_bitstream.version = NV_ENC_LOCK_BITSTREAM_VER;
  lock_bitstream.outputBitstream = ctx->output_buffers[index];
  lock_bitstream.doNotWait = 1;

  /* Lock the bitstream while handing it over to the user. */
  status = ctx->funcs.nvEncLockBitstream(ctx->session, &lock_bitstream);
  if (NV_ENC_SUCCESS != status) {
    TRAE("Failed to lock the bitstream for flushing. ");
    r = -50;
    goto error;
  }
  
  is_locked = 1;

  /* Now that the bitstream is locked, the user can handle the bitstream. */
  encoded_data.data = (uint8_t*) lock_bitstream.bitstreamBufferPtr;
  encoded_data.size = lock_bitstream.bitstreamSizeInBytes;

  r = ctx->settings.callbacks.on_encoded_data(
    TRA_MEMORY_TYPE_H264,
    &encoded_data,
    ctx->settings.callbacks.user
  );
  
  if (r < 0) {
    TRAE("The handler failed to handle the encoded data nicely.");
    r = -60;
    goto error;
  }

  /* Let the user know that we've flushed the encoder. */
  r = ctx->settings.callbacks.on_flushed(ctx->settings.callbacks.user);
  if (r < 0) {
    TRAE("The `on_flushed` callback function returned an error.");
    r = -70;
    goto error;
  }
   
 error:

  if (1 == is_locked) {

    status = ctx->funcs.nvEncUnlockBitstream(ctx->session, lock_bitstream.outputBitstream);
    if (NV_ENC_SUCCESS != status) {
      TRAE("Failed to unlock the bitstream.");
      r -= 80;
    }
 
    is_locked = 0;
  }

  return r;
}

/* ------------------------------------------------------- */

/*

  This function will encode the input data (represented by
  `tra_nvenc_registration`), extract the encoded bitstream and
  calls the user callback.

  Before this encoder can encode we have to register the buffers
  that we want to encode. This is called registering the
  buffers. Once registered we use a registration point as source
  for the encoding process. The registration is like the glue
  between the data you want to encode, an encoder representation
  of that and the encoder. This registering of buffers is done by
  the different encoder variants (e.g. nvencgl, nvenchost,
  nvenccuda).

  We have created separate encoder variants that manage the
  registrations. For example we have `tra_nvenchost` which
  accepts `tra_memory_image` instances that reside in CPU memory.
  The `tra_nvenchost` will allocate a couple of device buffers
  into which these images are copied. These device buffers are
  registered with the encoder. When a user wants to encode a
  `tra_memory_image`, the `tra_nvenchost` will copy the image
  into the device buffer. To encode, we use the registration that
  represents the device buffer.
  
 */
int tra_nvenc_encode_with_registration(
  tra_nvenc* ctx,
  tra_sample* sample,
  tra_nvenc_registration* reg
)
{
  NV_ENC_LOCK_BITSTREAM lock_bitstream = { 0 };
  NV_ENC_MAP_INPUT_RESOURCE mapped = { 0 };
  tra_memory_h264 encoded_data = { 0 };
  NVENCSTATUS status = NV_ENC_SUCCESS;
  NV_ENC_PIC_PARAMS pic = { 0 };
  uint32_t index = 0;
  int is_mapped = 0;
  int is_locked = 0;
  int r = 0;

  /* ----------------------------------------------- */
  /* Validate                                        */
  /* ----------------------------------------------- */
  
  if (NULL == ctx) {
    TRAE("Cannot encode as the given `tra_nvenc` is NULL.");
    r = -10;
    goto error;
  }

  if (NULL == reg) {
    TRAE("Cannot encode as the given `tra_nvenc_registration*` is NULL.");
    r = -20;
    goto error;
  }

  if (NULL == reg->registered_ptr) {
    TRAE("Cannot encode as the given `tra_nvenc_registration::registered_ptr` is NULL.");
    r = -30;
    goto error;
  }

  if (0 == reg->external_stride) {
    TRAE("Cannot encode as the `tra_nvenc_registration::external_stride is not set.");
    r = -40;
    goto error;
  }

  if (0 == ctx->coded_width) {
    TRAE("Cannot encode as the `coded_width` is 0.");
    r = -50;
    goto error;
  }

  if (0 == ctx->coded_height) {
    TRAE("Cannot encode as the `coded_height` is 0.");
    r = -60;
    goto error;
  }

  if (NULL == ctx->output_buffers) {
    TRAE("Cannot encode as our output buffers haven't been allocated.");
    r = -70;
    goto error;
  }

  if (0 == ctx->output_count) {
    TRAE("Cannot encode as the number of output buffers is 0.");
    r = -80;
    goto error;
  }

  if (NULL == ctx->settings.callbacks.on_encoded_data) {
    TRAE("Cannot encode as the `on_encoded_data` callback hasn't been set.");
    r = -90;
    goto error;
  }

  /* ----------------------------------------------- */
  /* Map input buffer                                */
  /* ----------------------------------------------- */

  /* Get and advance our output buffer index where we store the bitstream into. */
  index = ctx->output_index;
  ctx->output_index = (index + 1) % ctx->output_count;

  /* Map the registered input buffer.  */
  mapped.version = NV_ENC_MAP_INPUT_RESOURCE_VER;
  mapped.subResourceIndex = 0; /* deprecated */
  mapped.inputResource = NULL; /* deprecated */
  mapped.registeredResource = reg->registered_ptr;
  mapped.mappedResource = NULL; /* [out] */

  if (NULL == mapped.registeredResource) {
    TRAE("Cannot map the resource as it's NULL.");
    r = -100;
    goto error;
  }

  status = ctx->funcs.nvEncMapInputResource(ctx->session, &mapped);
  if (NV_ENC_SUCCESS != status) {
    TRAE("Failed to map an input resource.");
    r = -110;
    goto error;
  }

  is_mapped = 1;

  /* ----------------------------------------------- */
  /* Encode                                          */
  /* ----------------------------------------------- */

  /* Once we have a mapped resource we can encode it using a NV_ENC_PIC_PARAMS. */
  pic.version = NV_ENC_PIC_PARAMS_VER;
  pic.inputWidth = ctx->coded_width; /* @todo Should we use the `src_image->image_width here? What is the exact meaning of this field. */
  pic.inputHeight = ctx->coded_height; /* @todo see `inputWidth`. */
  pic.inputPitch = reg->external_stride;
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
    r = -120;
    goto error;
  }

  /* ----------------------------------------------- */
  /* Extract bitstream                               */
  /* ----------------------------------------------- */

  lock_bitstream.version = NV_ENC_LOCK_BITSTREAM_VER;
  lock_bitstream.outputBitstream = ctx->output_buffers[index];
  lock_bitstream.doNotWait = 1;

  /* Lock the bitstream while handing it over to the user. */
  status = ctx->funcs.nvEncLockBitstream(ctx->session, &lock_bitstream);
  if (NV_ENC_SUCCESS != status) {
    TRAE("Failed to lock the bitstream. ");
    r = -130;
    goto error;
  }
  
  is_locked = 1;

  /* Now that the bitstream is locked, the user can handle it. */
  encoded_data.data = (uint8_t*) lock_bitstream.bitstreamBufferPtr;
  encoded_data.size = lock_bitstream.bitstreamSizeInBytes;

  r = ctx->settings.callbacks.on_encoded_data(
    TRA_MEMORY_TYPE_H264,
    &encoded_data,
    ctx->settings.callbacks.user
  );
  
  if (r < 0) {
    TRAE("The handler failed to handle the encoded data nicely.");
    r = -140;
    goto error;
  }

 error:

  /* Unlock the bitstream when required. */
  if (1 == is_locked) {

    status = ctx->funcs.nvEncUnlockBitstream(ctx->session, lock_bitstream.outputBitstream);
    if (NV_ENC_SUCCESS != status) {
      TRAE("Failed to unlock the bitstream.");
      r -= 150;
    }
 
    is_locked = 0;
  }

  /* Unmap the input resource when required. */
  if (1 == is_mapped) {
  
    status = ctx->funcs.nvEncUnmapInputResource(ctx->session, mapped.mappedResource);
    if (NV_ENC_SUCCESS != status) {
      TRAE("Failed to unmap an input resource.");
      r -= 160;
    }

    is_mapped = 0;
  }
 
  return r;
}

/* ------------------------------------------------------- */

int tra_nvenc_get_input_buffer_count(tra_nvenc* ctx, uint32_t* count) {
  
  if (NULL == ctx) {
    TRAE("Cannot get the input buffer count as the given `tra_nvenc*` is NULL.");
    return -10;
  }

  if (NULL == count) {
    TRAE("Cannot get the input buffer count as the given output `count` argument is NULL.");
    return -20;
  }

  *count = ctx->input_count;

  return 0;
}

/* ------------------------------------------------------- */

int tra_nvenc_resources_create(
  tra_nvenc_resources_settings* cfg,  
  tra_nvenc_resources** ctx
)
{
  tra_nvenc_resources* inst = NULL;
  int r = 0;
  
  if (NULL == cfg) {
    TRAE("Cannot create the `tra_nvenc_resources` as the given settings is NULL.");
    r = -10;
    goto error;
  }

  if (NULL == cfg->enc) {
    TRAE("Cannot create the `tra_nvenc_resources` as the given `tra_nvenc_resources_settings::enc` is NULL. We need the encoder to register (external) resources with. ");
    r = -20;
    goto error;
  }

  if (NULL == ctx) {
    TRAE("Cannot create the `tra_nvenc_resources` as the given `tra_nvenc_resources**` is NULL.");
    r = -30;
    goto error;
  }

  if (NULL != *ctx) {
    TRAE("Cannot create the `tra_nvenc_resources` as the given `*tra_nvenc_resources**` is not NULL. Did you already create it?");
    r = -40;
    goto error;
  }

  inst = calloc(1, sizeof(tra_nvenc_resources));
  if (NULL == inst) {
    TRAE("Cannot create the `tra_nvenc_resources` instance as we failed to allocate it. Out of memory?");
    r = -50;
    goto error;
  }

  /* Setup */
  inst->reg_array = NULL;
  inst->reg_index = 0;
  inst->reg_count = 0;
  inst->encoder = cfg->enc;

  /* Finally assign it. */
  *ctx = inst;

 error:

  if (r < 0) {

    if (NULL != inst) {
      tra_nvenc_resources_destroy(inst);
      inst = NULL;
    }

    if (NULL != ctx) {
      *ctx = NULL;
    }
  }
  
  return r;
}

/* ------------------------------------------------------- */

int tra_nvenc_resources_destroy(tra_nvenc_resources* ctx) {

  tra_nvenc_registration* reg_ptr = NULL;
  NVENCSTATUS status = NV_ENC_SUCCESS;
  tra_nvenc* enc = NULL;
  uint32_t i = 0;
  int result = 0;
  int r = 0;

  if (NULL == ctx) {
    TRAE("Cannot cleanly destroy the `tra_nvenc_resources*` as it's NULL.");
    result -= 10;
    goto error;
  }

  enc = ctx->encoder;
  if (NULL == enc) {
    TRAE("Cannot cleanly destroy the `tra_nvenc_resources` as its encoder member is NULL.");
    result = -20;
    goto error;
  }

  if (NULL == enc->session) {
    TRAE("Cannot cleanly destroy the `tra_nvenc_resources` as the encoder session is NULL. Did you already destroy the encoder instance?");
    result = -30;
    goto error;
  }

  for (i = 0; i < ctx->reg_count; ++i) {

    reg_ptr = ctx->reg_array + i;

    status = enc->funcs.nvEncUnregisterResource(enc->session, reg_ptr->registered_ptr);
    if (NV_ENC_SUCCESS != status) {
      TRAE("Failed to unregister the resource (%u).", i);
      result -= 40;
      continue;
    }
  }

  if (NULL != ctx->reg_array) {
    free(ctx->reg_array);
  }
  
  ctx->reg_array = NULL;
  ctx->reg_count = 0;
  ctx->reg_index = 0;
  ctx->encoder = NULL;

  free(ctx);
  ctx = NULL;

 error:
  return result;
}

/* ------------------------------------------------------- */

/*
  
  REGISTERING EXTERNAL RESOURCES:
  
    Every external resource that we want to encode needs to be
    registered with the encoder. An external resource can be a
    OpenGL texture, CUDA buffer, etc. The image below shows how
    different external inputs are registered using
    `nvEncRegisterResource()`.


      ┌────────┐
      │        │
      │  cuda  ├──┐
      │        │  │
      └────────┘  │                                    NV_ENC_REGISTER_RESOURCE
                  │
      ┌────────┐  │    ┌─────────────────────────┐     ┌────────────────────┐
      │        │  │    │                         │     │                    │
      │ opengl ├──┼───►│ nvEncRegisterResource() ├────►│ registeredResource │
      │        │  │    │                         │     │                    │
      └────────┘  │    └─────────────────────────┘     └────────────────────┘
                  │
      ┌────────┐  │
      │        │  │
      │ d3d12  ├──┘
      │        │
      └────────┘

    
 */
int tra_nvenc_resources_ensure_registration(
  tra_nvenc_resources* ctx,
  tra_nvenc_resource* resourceToEnsure,
  tra_nvenc_registration** resourceRegistration
)
{
  NV_ENC_REGISTER_RESOURCE input_reg = { 0 };
  NVENCSTATUS status = NV_ENC_SUCCESS;
  CUresult result = CUDA_SUCCESS;

  tra_nvenc_registration* reg = NULL;
  tra_nvenc* enc = NULL;
  uint32_t i = 0;

  int r = 0;

  if (NULL == ctx) {
    TRAE("Cannot ensure that the given `tra_nvenc_resource*` is registered as the `tra_nvenc_resources` context is NULL.");
    r = -10;
    goto error;
  }

  if (NULL == ctx->encoder) {
    TRAE("Cannot ensure that the given `tra_nvenc_resource*` is registered as the `tra_nvenc_resources::enc` member is NULL.");
    r = -20;
    goto error;
  }

  enc = ctx->encoder;

  if (0 == enc->coded_width) {
    TRAE("Cannot ensure that the given `tra_nvenc_resource*` is registered as the `coded_width` of the encoder context is not set.");
    r = -30;
    goto error;
  }

  if (0 == enc->coded_height) {
    TRAE("Cannot ensure that the given `tra_nvenc_resource*` is registered as the `coded_height` of the encoder context is not set.");
    r = -40;
    goto error;
  }

  if (0 == enc->input_format) {
    TRAE("Cannot ensure that the given `tr_nvenc_resource*` is registered as the `input_format` of the encoder context is not set.");
    r = -50;
    goto error;
  }

  if (NULL == resourceToEnsure) {
    TRAE("Cannot ensure the registration of the given `tra_nvenc_resource*` as it's NULL.");
    r = -60;
    goto error;
  }

  if (NULL == resourceToEnsure->resource_ptr) {
    TRAE("Cannot ensure the registration of the given `tra_nvenc_resource*` as the `resource_ptr` is NULL.");
    r = -70;
    goto error;
  }

  if (0 == resourceToEnsure->resource_stride) {
    TRAE("Cannot ensure the registration of the given `tra_nvenc_resource*` as the `resource_stride` is not set.");
    r = -80;
    goto error;
  }

  if (0 == resourceToEnsure->resource_type) {
    TRAE("Cannot ensure the registration of the given `tra_nvenc_resource*` as the `resource_type` is not set.");
    r = -90;
    goto error;
  }

  if (NV_ENC_INPUT_RESOURCE_TYPE_CUDADEVICEPTR != resourceToEnsure->resource_type
      && NV_ENC_INPUT_RESOURCE_TYPE_OPENGL_TEX != resourceToEnsure->resource_type)
    {
      TRAE("Cannot ensure the registration of the given `tra_nvenc_resource*` as the `resource_type` is not supported yet.");
      r = -100;
      goto error;
    }

  if (NULL == resourceRegistration) {
    TRAE("Cannot ensure the registration for the given `tra_nvenc_resource*` as the output argument to which we assign the created registration is NULL.");
    r = -110;
    goto error;
  }

  /*
    Check if we've aleady registered this resource. When we find
    a registrations for the external resource we the output result
    (registeredResource) and return 0.
  */
  for (i = 0; i < ctx->reg_count; ++i) {

    reg = ctx->reg_array + i;
    
    if (reg->external_ptr == resourceToEnsure->resource_ptr) {
      *resourceRegistration = reg;
      r = 0;
      goto error;
    }
  }

  /*
    When we arrive here it means the given `tra_nvenc_resource*`
    hasn't been registered yet. Therefore we register the
    resource after which we set the output result
    (registeredResource).
   */
  if (NULL != ctx->reg_array) {

    reg = realloc(ctx->reg_array, (ctx->reg_count + 1) * sizeof(tra_nvenc_registration));
    if (NULL == reg) {
      TRAE("Failed to grow our reg_array array. Out of mem?");
      r = -120;
      goto error;
    }
    
    ctx->reg_array = reg;
    ctx->reg_count = ctx->reg_count + 1;
  }
  else {

    reg = calloc(1, sizeof(tra_nvenc_registration));
    if (NULL == reg) {
      TRAE("Cannot ensure the registration as we failed to allocate our `reg_array` array.");
      r = -130;
      goto error;
    }

    ctx->reg_array = reg;
    ctx->reg_count = 1;
  }

  /* Register the given resource */
  input_reg.version = NV_ENC_REGISTER_RESOURCE_VER;
  input_reg.resourceType = resourceToEnsure->resource_type; 
  input_reg.resourceToRegister = (void*) resourceToEnsure->resource_ptr;
  input_reg.width = enc->coded_width;
  input_reg.height = enc->coded_height;
  input_reg.pitch = resourceToEnsure->resource_stride;
  input_reg.subResourceIndex = 0;
  input_reg.bufferFormat = enc->input_format;
  input_reg.bufferUsage = NV_ENC_INPUT_IMAGE;
  input_reg.pInputFencePoint = NULL;
  input_reg.pOutputFencePoint = NULL;

  status = enc->funcs.nvEncRegisterResource(enc->session, &input_reg);
  if (NV_ENC_SUCCESS != status) {
    TRAE("Failed to register an input resource: %s", tra_nv_status_to_string(status));
    r = -140;
    goto error;
  }

  if (NULL == input_reg.registeredResource) {
    TRAE("Failed to retrieve a pointer to the registered resourcde.");
    r = -150;
    goto error;
  }

  /* Make sure the newly created registration has a reference to the mapping. */
  reg = ctx->reg_array + (ctx->reg_count - 1);

  reg->external_stride = resourceToEnsure->resource_stride;
  reg->external_ptr = resourceToEnsure->resource_ptr;
  reg->registered_ptr = input_reg.registeredResource;

  /* Assign the output result. */
  *resourceRegistration = reg;

 error:
  return r;
}

/* ------------------------------------------------------- */

/*
  
  When we want to encode an input buffer, we first have to
  register the buffer. The registration is done once for each
  external buffer. To register you use the
  `tra_nvenc_resources_ensure_registration()` function.  Once
  they have been registered you have to retrieve a registration
  that can be used to encode the current input buffer. This
  function will give you the `tra_nvenc_registration` that you
  can use to encode an input buffer.
  
 */
int tra_nvenc_resources_get_usable_registration(
  tra_nvenc_resources* ctx,
  tra_nvenc_registration** usableRegistration,
  void** registeredResourcePtr
)
{
  tra_nvenc_registration* reg = NULL;
  int r = 0;
  
  if (NULL == ctx) {
    TRAE("Cannot get a usable registration as the given `tra_nvenc_resources` given is NULL.");
    r = -10;
    goto error;
  }

  if (NULL == usableRegistration) {
    TRAE("Cannot get a usable registration as the given output `tra_nvenc_registration**` is NULL.");
    r = -20;
    goto error;
  }
  
  if (NULL == registeredResourcePtr) {
    TRAE("Cannot get a usable registration as the given output pointer that will be set to the registered external pointer is NULL.");
    r = -30;
    goto error;
  }

  if (NULL == ctx->reg_array) {
    TRAE("Cannot get a usable registration as there no reg_array yet. Did you use `tra_nvenc_resource_ensure_registration()` to register a resource?");
    r = -40;
    goto error;
  }

  /* Get the next registration. */
  reg = ctx->reg_array + ctx->reg_index;
  if (NULL == reg->external_ptr) {
    TRAE("The external pointer of the registration is NULL. This should not happen.");
    r = -50;
    goto error;
  }

  *usableRegistration = reg;
  *registeredResourcePtr = reg->external_ptr;

  /*
    And advance the reg_index so that a next call to
    this function will retrieve the next usable entry.
  */
  ctx->reg_index = (ctx->reg_index + 1) % ctx->reg_count;

 error:
  return r;
}

/* ------------------------------------------------------- */

static int tra_nvenc_print_inputformats(tra_nvenc* ctx, GUID encodeGuid) {

  NV_ENC_BUFFER_FORMAT* formats = NULL;
  NVENCSTATUS status = NV_ENC_SUCCESS;
  uint32_t num_set = 0;
  uint32_t num = 0;
  uint32_t i = 0;
  int r = 0;
  
  if (NULL == ctx) {
    TRAE("Cannot print the input formats as the given `nvenc_ctx` is NULL.");
    r = -10;
    goto error;
  }

  if (NULL == ctx->session) {
    TRAE("Cannot print the input formats as the encoder session is NULL.");
    r = -20;
    goto error;
  }

  if (NULL == ctx->funcs.nvEncGetInputFormatCount) {
    TRAE("Cannot print the input formats as the `nvEncGetInputFormatCount()` is NULL.");
    r = -30;
    goto error;
  }

  status = ctx->funcs.nvEncGetInputFormatCount(ctx->session, encodeGuid, &num);
  if (NV_ENC_SUCCESS != status) {
    TRAE("Failed to get the number of input formats.");
    r = -40;
    goto error;
  }

  if (0 == num) {
    TRAE("Failed to retrieve the number of input formats; or no input formats found.");
    r = -50;
    goto error;
  }

  formats = malloc(num * sizeof(NV_ENC_BUFFER_FORMAT));
  if (NULL == formats) {
    TRAE("Failed to allocate the array to store the supported input formats.");
    r = -60;
    goto error;
  }

  status = ctx->funcs.nvEncGetInputFormats(ctx->session, encodeGuid, formats, num, &num_set);
  if (NV_ENC_SUCCESS != status) {
    TRAE("Failed to get the supported input formats.");
    r = -70;
    goto error;
  }

  for (i = 0; i < num_set; ++i) {
    TRAD("Supported input format: %s", tra_nv_bufferformat_to_string(formats[i]));
  }

 error:

  if (NULL != formats) {
    free(formats);
    formats = NULL;
  }
    
  return r;
}

/* ------------------------------------------------------- */
