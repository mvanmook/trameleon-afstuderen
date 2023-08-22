/* ------------------------------------------------------- */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>

#include <ni_device_api.h>
#include <ni_rsrc_api.h>
#include <ni_util.h>

#include <tra/modules/netint/netint-utils.h>
#include <tra/modules/netint/netint-enc.h>
#include <tra/module.h>
#include <tra/types.h>
#include <tra/log.h>

/* ------------------------------------------------------- */

typedef struct netint_enc netint_enc;

/* ------------------------------------------------------- */

struct netint_enc {
  tra_encoder_settings settings;
  ni_device_context_t* device;
  ni_session_context_t session;
  ni_encoder_params_t params;
  uint8_t is_start_of_stream; /* Is set to 1 when we just started the encode process and we haven't sent anything to the encoder yet. */
};

/* ------------------------------------------------------- */

static int netint_enc_save_coded_data(netint_enc* ctx); /* Downloads/reads the encoded data. */

/* ------------------------------------------------------- */

/*
  GENERAL INFO

    We first create a session with
    `ni_device_session_context_init()`. Next we set the codec
    format and device id. The SDK provides a way to select a
    device based on some heuristic. We use a hardcoded device id.

  TODO:

    @todo The `ni_device_test.c` examples calls
          `ni_encoder_init_default_params()` before doing
          anything else. The documentation doesn't mention this;
          not sure if this is required.

    @todo The api also provicdes `ni_encoder_params_check()`
          which we can call to make sure the current params are
          correct. Maybe I should add it.

    @todo Figure out what the most optimized image format
          is. Most encoders prefer NV12, but not sure what NETINT
          prefers.

 */
int netint_enc_create(tra_encoder_settings* cfg, netint_enc** ctx) {

  unsigned long load = 0;
  netint_enc* inst = NULL;
  ni_retcode_t code = 0;
  int r = 0;

  if (NULL == cfg) {
    TRAE("Cannot create the `netint_enc` instance as the given `tra_encoder_settings*` are NULL.");
    r = -10;
    goto error;
  }
  
  if (NULL == ctx) {
    TRAE("Cannot create the `netint_enc` instance as the given destination is NULL.");
    r = -20;
    goto error;
  }

  if (NULL != (*ctx)) {
    TRAE("Cannot create the `netint_enc` as the given destination is not NULL. Already initialized? Or not initialized to NULL?");
    r = -30;
    goto error;
  }

  if (0 == cfg->image_width) {
    TRAE("Cannot create the `netint_enc` instance as the `image_width` is 0.");
    r = -40;
    goto error;
  }

  if (0 == cfg->image_height) {
    TRAE("Cannot create the `netint_enc` instance as the `image_height` is 0.");
    r = -50;
    goto error;
  }

  if (TRA_IMAGE_FORMAT_I420 != cfg->image_format) {
    TRAE("Cannot create the `netint_enc` instance as the `image_format` is not `TRA_IMAGE_FORMAT_I420` ");
    r = -60;
    goto error;
  }

  if (0 == cfg->fps_num) {
    TRAE("Cannot create the `netint_enc` instance as the `fps_num` is 0.");
    r = -70;
    goto error;
  }

  if (0 == cfg->fps_den) {
    TRAE("Camnnot create the `netint_enc` instance as the `fps_den` is 0.");
    r = -80;
    goto error;
  }

  inst = calloc(1, sizeof(netint_enc));
  if (NULL == inst) {
    TRAE("Cannot create the `netint_enc`. Failed to allocate.");
    r = -90;
    goto error;
  }

  inst->settings = *cfg;
  inst->is_start_of_stream = 1;

  code = ni_encoder_init_default_params(
    &inst->params,
    cfg->fps_num,
    cfg->fps_den,
    200000,
    cfg->image_width,
    cfg->image_height
  );
  
  if (NI_RETCODE_SUCCESS != code) {
    TRAE("Cannot create the `netint_enc`, failed to initialize the encoder parameters.");
    r = -100;
    goto error;
  }

#if 0
  /* Use custom resource management logic. */
  inst->device = ni_rsrc_allocate_direct(
    NI_DEVICE_TYPE_ENCODER,
    0,
    EN_H264,
    cfg->image_width,
    cfg->image_height,
    cfg->fps_num,
    &load
  );
#endif

#if 1
  /* Use NETINTs own resource management logic. */
  inst->device = ni_rsrc_allocate_auto(
    NI_DEVICE_TYPE_ENCODER,
    EN_ALLOC_LEAST_LOAD,
    EN_H264,
    cfg->image_width,
    cfg->image_height,
    cfg->fps_num,
    &load
  );
#endif
  
  if (NULL == inst->device) {
    TRAE("Failed to allocate the resources for a device context.");
    r = -110;
    goto error;
  }
  
  /* @todo Should we call `ni_encoder_params_check()` at this point? */
  
  ni_device_session_context_init(&inst->session);
  
  inst->session.keep_alive_timeout = 10;                     /* Heartbeat timeout that is used to keep the connection between the libxcoder and firmware open. */
  inst->session.codec_format = NI_CODEC_FORMAT_H264;         /* The codec we want to use; h264 or h265. */
  inst->session.hw_id = 0;                                   /* The device to use. Use `ni_rsrc_mon` to show a list of indices for each device. */
  inst->session.src_bit_depth = 8;                           /* 8 or 10 bits/pixel formats, defaults to 8. */
  inst->session.src_endian = NI_FRAME_LITTLE_ENDIAN;         /* The endianness of our source data. */
  inst->session.bit_depth_factor = 1;                        /* For YUV buffer allocation. */
  inst->session.p_session_config = &inst->params;            /* See section 6.5.3 of the [integration guide][0] */
  inst->session.session_id = NI_INVALID_SESSION_ID;
  
  code = ni_device_session_open(&inst->session, NI_DEVICE_TYPE_ENCODER);
  if (NI_RETCODE_SUCCESS != code) {
    TRAE("Cannot create the `netint_enc`, the call to `ni_device_session_open()` failed with %s.", netint_retcode_to_string(code));
    r = -4;
    goto error;
  }

  netint_print_sessioncontext(&inst->session);
  netint_print_encoderparams(&inst->params);
  
  /* Set the log level ; used during development. */
  ni_log_set_level(NI_LOG_INFO);

  *ctx = inst;

 error:

  if (r < 0) {

    if (NULL != inst) {
      netint_enc_destroy(inst);
      inst = NULL;
    }
    
    if (NULL != ctx) {
      *ctx = NULL;
    }
  }

  return r;
}

/* ------------------------------------------------------- */

int netint_enc_destroy(netint_enc* ctx) {

  ni_retcode_t code = NI_RETCODE_SUCCESS;
  int result = 0;

  TRAE("@todo Correctly implement buffers!");
  
  if (NULL == ctx) {
    TRAE("Cannot destroy the `netint_enc` as it's NULL.");
    return -1;
  }

  /* Close session */
  if (NI_INVALID_SESSION_ID != ctx->session.session_id) {
    code = ni_device_session_close(&ctx->session, 0, NI_DEVICE_TYPE_ENCODER);
    if (NI_RETCODE_SUCCESS != code) {
      TRAE("Failed to cleanly destroy the `netint_enc` session instance, failed with: %s. ", netint_retcode_to_string(code));
      result -= 1;
    }
  }

  if (NULL != ctx->device) {
    ni_rsrc_free_device_context(ctx->device); 
  }

  ctx->device = NULL;
  ctx->session.session_id = NI_INVALID_SESSION_ID;
  
  free(ctx);
  ctx = NULL;

  return result;
}

/* ------------------------------------------------------- */

/*
  GENERAL INFO:

    See the [integration guide][0], section 6.5.2 about how to prepare
    the input data. This needs to be yuv420p. The width needs to be a 
    multiple of 32 and the height 16 for H264.

  REFERENCES

    [0]: libxcoder_API_Integration_guideT408_T432_FW2.6.pdf

 */
int netint_enc_encode(
  netint_enc* ctx,
  tra_sample* sample, 
  uint32_t type,
  void* data
)
{
  int dst_height_aligned[NI_MAX_NUM_DATA_POINTERS] = {0};
  int dst_stride[NI_MAX_NUM_DATA_POINTERS] = {0};
  uint8_t* src_ptr[NI_MAX_NUM_DATA_POINTERS] = {};
  int src_stride[NI_MAX_NUM_DATA_POINTERS] = {};
  int src_height[NI_MAX_NUM_DATA_POINTERS] = {};
  ni_retcode_t code = NI_RETCODE_SUCCESS;
  ni_session_data_io_t data_io = {};
  ni_packet_t* packet = NULL;
  ni_frame_t* frame = NULL;
  tra_image* src_image = NULL;
  int nwritten = 0;
  uint32_t i = 0;
  int r = 0;
  
  if (NULL == ctx) {
    TRAE("Cannot encode, given `netint_enc*` is NULL.");
    r = -10;
    goto error;
  }

  if (NULL == sample) {
    TRAE("Cannot encode, the given `tra_sample*` is NULL.");
    r = -20;
    goto error;
  }

  if (TRA_MEMORY_TYPE_IMAGE != type) {
    TRAE("Cannot encode because the input type is not supported.");
    r = -30;
    goto error;
  }

  if (NULL == data) {
    TRAE("Cannot encode, given `data` is NULL.");
    r = -40;
    goto error;
  }

  src_image = (tra_image*) data;
  if (src_image->image_width != ctx->settings.image_width) {
    TRAE("Given input `image_width` (%u)  differs from the initial settings (%u).", src_image->image_width, ctx->settings.image_width);
    r = -50;
    goto error;  
  }

  if (src_image->image_height != ctx->settings.image_height) {
    TRAE("Given input `image_height` (%u)  differs from the initial settings (%u).", src_image->image_height, ctx->settings.image_height);
    r = -60;
    goto error;  
  }

  if (0 == ctx->settings.image_width) {
    TRAE("Cannot encode the given frame as the instance `image_width` setting is 0.");
    r = -70;
    goto error;
  }

  if (0 == ctx->settings.image_height) {
    TRAE("Cannot encode the given frame as the instance `image_heigh` setting is 0.");
    r = -80;
    goto error;
  }

  frame = &data_io.data.frame;
  packet = &data_io.data.packet;

  frame->force_key_frame = 0;
  frame->start_of_stream = ctx->is_start_of_stream;
  frame->end_of_stream = 0;
  frame->video_width = ctx->settings.image_width;
  frame->video_height = ctx->settings.image_height;
  frame->extra_data_len = NI_APP_ENC_FRAME_META_DATA_SIZE;

  /* 
     Check the stride and heights of the YUV420p hardware
     destination that we're going to sent to the encoder. This is
     just a little helper which makes sure the heights are
     multiples of the supported values (8, 16, 32).
  */
  ni_get_hw_yuv420p_dim(
    ctx->settings.image_width,
    ctx->settings.image_height,
    ctx->session.bit_depth_factor,
    (NI_CODEC_FORMAT_H264 == ctx->session.codec_format) ? 1 : 0,
    dst_stride,
    dst_height_aligned
  );

  /* Print some debug info. */
  for (i = 0; i < NI_MAX_NUM_DATA_POINTERS; ++i) {
    TRAD("dst_height_aligned[%u] = %d, dst_stride[%u] = %d", i, dst_height_aligned[i], i, dst_stride[i]);
  }

  ni_frame_buffer_alloc_v3(
    frame,
    ctx->settings.image_width,
    ctx->settings.image_height,
    dst_stride,
    (NI_CODEC_FORMAT_H264 == ctx->session.codec_format) ? 1 : 0,
    frame->extra_data_len, /* @todo */
    ctx->session.bit_depth_factor
  );

  if (NULL == frame->p_data[0]) {
    TRAE("Failed to allocate YUV buffer.");
    r = -90;
    goto error;
  }

  /* Prepare the source stride and offset info. */
#if 0  
  src_stride[0] = ctx->settings.image_width * ctx->session.bit_depth_factor; /* @todo Describe this usage! See ni_device_test.c, encoder_send_data() */
  src_stride[1] = (ctx->settings.image_width * ctx->session.bit_depth_factor) / 2;
  src_stride[2] = (ctx->settings.image_width * ctx->session.bit_depth_factor) / 2;
  src_height[0] = ctx->settings.image_height;
  src_height[1] = ctx->settings.image_height / 2;
  src_height[2] = ctx->settings.image_height / 2;
  src_ptr[0] = data;
  src_ptr[1] = data + (src_stride[0] * src_height[0]);
  src_ptr[2] = data + (src_stride[0] * src_height[0]) + (src_stride[1] * src_height[1]);
#endif

  /* @todo See ni_device_test.c where the `bit_depth_factor` is used which is required when we're dealing with non-8bit input. */
  
  /* Map the input image to the parameters that `ni_copy_hw_yuv420p` accepts. */
  src_ptr[0] = src_image->plane_data[0];
  src_ptr[1] = src_image->plane_data[1];
  src_ptr[2] = src_image->plane_data[2];

  src_stride[0] = src_image->plane_strides[0];
  src_stride[1] = src_image->plane_strides[1];
  src_stride[2] = src_image->plane_strides[2];

  src_height[0] = src_image->plane_heights[0];
  src_height[1] = src_image->plane_heights[1];
  src_height[2] = src_image->plane_heights[2];
  
  ni_copy_hw_yuv420p(
    (uint8_t**) (frame->p_data),
    src_ptr,
    ctx->settings.image_width,
    ctx->settings.image_height,
    ctx->session.bit_depth_factor,
    dst_stride,
    dst_height_aligned,
    src_stride,
    src_height
  );

  nwritten = ni_device_session_write(&ctx->session, &data_io, NI_DEVICE_TYPE_ENCODER);
  if (nwritten < 0) {
    TRAE("Failed to write YUV data to the encoder: %s.", netint_retcode_to_string(nwritten));
    TRAE("@todo aren't we leadking the buffer here?");
    r = -100;
    goto error;
  }
  
  r = netint_enc_save_coded_data(ctx);
  if (r < 0) {
    TRAE("Failed to save the coded data.");
    r = -110;
    goto error;
  }

  /* @todo Should we set this here or leave it to the user? */
  ctx->is_start_of_stream = 0;

 error:
  return r;
}

/* ------------------------------------------------------- */

/* Downloads/reads the encoded data. */
static int netint_enc_save_coded_data(netint_enc* ctx) {

  tra_encoded_host_memory encoded_data = { 0 };
  tra_encoder_callbacks* callbacks = NULL;
  ni_retcode_t code = NI_RETCODE_SUCCESS;
  ni_session_data_io_t data_io = {};
  int packet_nbytes = NI_MAX_TX_SZ;
  ni_packet_t* packet = NULL;
  int nread = 0;
  int r = 0;

  if (NULL == ctx) {
    TRAE("Cannot get the coded data as the given `netint_enc*` is NULL.");
    r = -10;
    goto error;
  }

  callbacks = &ctx->settings.callbacks;

  if (NULL == callbacks->on_encoded_data) {
    TRAE("Cannot handle encoded data, `tra_encoder_calblacks::on_encoded_data` is NULL.");
    r = -20;
    goto error;
  }

  {
    /* Just log this as I don't want to forget ^.^ */
    static uint8_t did_log = 0;
    if (0 == did_log) {
      TRAE("@todo we should name all functions that download bitstream data similar throughout the implementations.");
      TRAE("@todo in the save_coded_data function we have to check how to deal with buffer allocation/deallocations.");
      did_log = 1;
    }
  }

  packet = &data_io.data.packet;

  code = ni_packet_buffer_alloc(packet, packet_nbytes);
  if (NI_RETCODE_SUCCESS != code) {
    TRAE("Failed to allocate the packet buffer to receive the encoded data.");
    r = -30;
    goto error;
  }

  nread = ni_device_session_read(&ctx->session, &data_io, NI_DEVICE_TYPE_ENCODER);
  if (nread < 0) {
    TRAE("Failed to read data from the encoder: %s.", netint_retcode_to_string(nread));
    r = -40;
    goto error;
  }

  if (0 == nread) {
    TRAD("Didn't read anything.");
    r = 0;
    goto error;
  }

  /* See `ni_device_test.c`, handling this situation in the same way as in the reference implementation. */
  if (nread < NI_FW_ENC_BITSTREAM_META_DATA_SIZE) {
    TRAE("Didn't receive enough data: %d", nread);
    r = -50;
    goto error;
  }

  /* Jump over hardware meta data first.  */
  encoded_data.data = packet->p_data + NI_FW_ENC_BITSTREAM_META_DATA_SIZE;
  encoded_data.size = nread - NI_FW_ENC_BITSTREAM_META_DATA_SIZE;

  r = callbacks->on_encoded_data(
    TRA_MEMORY_TYPE_HOST_H264,
    &encoded_data,
    callbacks->user
  );

  if (r < 0) {
    TRAE("`on_encoded_data` returned an error. This happens when the user cannot handle the encoded data.");
    r = -60;
    goto error;
  }

 error:

  if (r < 0) {
    TRAE("Failed to read encoded data; @todo how to manage buffers here...");
  }

  return r;
}

/* ------------------------------------------------------- */
