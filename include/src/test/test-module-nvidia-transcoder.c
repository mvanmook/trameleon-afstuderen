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

  NVIDIA TRANSCODER TEST
  ======================

  GENERAL INFO:

    This file is used during the development of the NVIDIA module
    for Trameleon. We generate raw H264 frames using FFmpeg, then
    decode this using the NVIDIA decoder. We make sure that
    decoded frames stay on the GPU. Then we hand over the decoded
    frame to the NVIDIA based encoder to perform on-device
    transcoding. This test is based on the NVIDIA decoder
    example.
    
    We create a FFmpeg, source filter that produces raw YUV420p
    frames which are then passed into an encoder using
    FFmpeg. You can use `ffmpeg -encoders -hide_banner` to get a
    list of available encoders; the name of the codec is passed into
    `avcodec_find_encoder_by_name`, e.g.

       ffmpeg -encoders -hide_banner | grep -i 264

    Once we have a raw H264 frame, we pass it into the NVIDIA
    decoder module from Trameleon. We receive decoded frames in
    our `on_decoded_data` callback. The decoded frame will still
    be stored on the GPU. We feed this decoded YUV frame into the
    encoder.

  REFERENCES:

   [0]: https://github.com/FFmpeg/FFmpeg/blob/master/doc/examples/encode_video.c "Encoder example"
   
 */

/* ------------------------------------------------------- */

#include <stdlib.h>

#include <libavcodec/avcodec.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/avfilter.h>
#include <libavutil/frame.h>

#include <tra/modules/nvidia/nvidia-cuda.h>
#include <tra/modules/nvidia/nvidia-dec.h>
#include <tra/modules/nvidia/nvidia-enc.h>
#include <tra/registry.h>
#include <tra/module.h>
#include <tra/types.h>
#include <tra/core.h>
#include <tra/log.h>

/* ------------------------------------------------------- */

typedef struct app {

  /* Core */
  tra_core_settings core_cfg;
  tra_core* core;

  /* Decoder */
  tra_nvdec_settings nvdec_settings;
  tra_decoder_settings dec_settings;
  tra_decoder* dec; 

  /* Encoder */
  tra_nvenc_settings nvenc_settings;
  tra_encoder_settings enc_settings;
  tra_sample enc_sample;
  tra_encoder* enc;
  
  /* We need to create a cuda context otherwise we can't decode. */
  tra_cuda_context* cu_ctx;
  tra_cuda_device* cu_dev;
  tra_cuda_api* cu_api;

  /* FFmpeg */
  AVFilterContext* filter_sink;
  AVFilterInOut* filter_inputs;
  AVFilterInOut* filter_outputs;
  AVFilterGraph* graph;
  AVCodecContext* codec_ctx;
  const AVCodec* codec_inst;
  AVPacket* pkt;

  /* General */
  uint32_t image_width;
  uint32_t image_height;
  AVFrame* frame;
  int is_ready; /* Set to 1 when the filter graph is finished. */
  FILE* gen_fp; /* We write the generated .h264 to this file. */
  FILE* trans_fp; /* we store the transcoded data into this file. */

} app;
  

/* ------------------------------------------------------- */

static int app_create(app** ctx);
static int app_destroy(app* ctx);
static int app_encode(app* ctx);

/* ------------------------------------------------------- */

static int encode_frame(AVCodecContext* codecCtx, AVFrame* frame, AVPacket* pkt, tra_decoder* dec, FILE* outputFile);
static int on_decoded_data(uint32_t type, void* data, void* user);
static int on_encoded_data(uint32_t type, void* data, void* user);

/* ------------------------------------------------------- */

int main(int argc, const char* argv[]) {

  app* ctx = NULL;
  int r = 0;

  r = app_create(&ctx);
  if (r < 0) {
    TRAE("Failed to create the application.");
    r = -10;
    goto error;
  }

  while (0 == ctx->is_ready) {
    
    r = app_encode(ctx);
    if (r < 0) {
      TRAE("Failed to encode.");
      r = -20;
      goto error;
    }
  }

 error:

  if (NULL != ctx) {
    r = app_destroy(ctx);
    if (r < 0) {
      TRAE("Failed to cleanly destroy the app.");
    }
  }
  
  if (r < 0) {
    return EXIT_FAILURE;
  }
  

  return EXIT_SUCCESS;
}


/* ------------------------------------------------------- */

/*
  We've created a separate encode function as we have to call it
  from the filter graph loop but also to flush the encoder data
  once the filter graph has been run. If we don't flush the
  encoder you might never receive any encoded data. We flush the
  encoder by passing NULL for the `AVFrame`.
 */
static int encode_frame(
  AVCodecContext* codecCtx,
  AVFrame* frame,
  AVPacket* pkt,
  tra_decoder* dec, 
  FILE* outputFile
)
{
  tra_memory_h264 host_mem = { 0 };
  int r = 0;
  
  if (NULL == codecCtx) {
    TRAE("Cannot encode the frame, given `AVCodecContext*` is NULL.");
    return -10;
  }

  if (NULL == pkt) {
    TRAE("Cannot encode as the given `AVPacket*` is NULL.");
    return -20;
  }

  r = avcodec_send_frame(codecCtx, frame);
  if (r < 0) {
    TRAE("Failed to send a frame into the encoder.");
    return -30;
  }

  if (NULL != frame) {
    TRAD("Got frame: %llu, width: %u, height: %u.", frame->pts, frame->width, frame->height);
  }

  if (NULL == dec) {
    TRAE("Cannot decode as the given decoder instanced is NULL.");
    return -40;
  }

  while (r >= 0) {

    r = avcodec_receive_packet(codecCtx, pkt);
    if (AVERROR(EAGAIN) == r) {
      TRAI("Encoder is not ready yet.");
      r = 0;
      break;
    }

    if (AVERROR_EOF == r) {
      TRAI("Encoder returned OEF");
      r = 0;
      break;
    }

    /* Use our decoder to decode the packet. */
    if (0 != pkt->size
        || NULL != pkt->data)
      {
        host_mem.size = pkt->size;
        host_mem.data = pkt->data;
        
        r = tra_decoder_decode(dec, TRA_MEMORY_TYPE_H264, &host_mem);
        if (r < 0) {
          TRAE("Failed to decode the encoded h264 packet.");
          return -50;
        }
      }

    if (pkt->size > 0
        && NULL != pkt->data
        && NULL != outputFile)
      {
        r = fwrite(pkt->data, pkt->size, 1, outputFile);
        if (1 != r) {
          TRAE("Failed to write the encoded data (%d).", r);
        }
      }
    
    av_packet_unref(pkt);
  }

  return r;
}

/* ------------------------------------------------------- */

/*

  This function will be called when the decoder has a decoded
  frame. Because we've created the decoder with the
  `TRA_MEMORY_TYPE_CUDA` output type, the decoded image will stay
  on the GPU. Here, we pass the decoded image to the encoder.
  
 */
static int on_decoded_data(
  uint32_t type,
  void* data,
  void* user
)
{
  app* ctx = NULL;
  int r = 0;

  if (TRA_MEMORY_TYPE_CUDA != type) {
    TRAE("We expect the decoded data to be represented by `TRA_MEMORY_TYPE_CUDA`.");
    r = -10;
    goto error;
  }

  ctx = (app*) user;
  if (NULL == ctx) {
    TRAE("Failed to case the user pointer into `app*`.");
    r = -20;
    goto error;
  }

  r = tra_encoder_encode(ctx->enc, &ctx->enc_sample, TRA_MEMORY_TYPE_CUDA, data);
  if (r < 0) {
    TRAE("Failed to encode an input image from device memory.");
    r = -30;
    goto error;
  }
  
 error:
  return r;
}

/* ------------------------------------------------------- */

/*
  This function is called when the encoder has encoded data. We
  will receive the encoded data as a `tra_memory_h264` pointer,
  which means the encoded data is in CPU memory.
*/
static int on_encoded_data(uint32_t type, void* data, void* user) {

  tra_memory_h264* encoded = (tra_memory_h264*) data;
  app* ctx = NULL;
  int r = 0;

  if (TRA_MEMORY_TYPE_H264 != type) {
    TRAE("Cannot handle encoded data, unsupported type: %u. (exiting)", type);
    exit(EXIT_FAILURE);
  }

  if (NULL == encoded) {
    TRAE("Cannot handle encoded data, `data` is NULL. (exiting).");
    exit(EXIT_FAILURE);
  }

  if (0 == encoded->size) {
    TRAE("Cannot handle encoded data, the `size` is 0. (exiting).") ;
    exit(EXIT_FAILURE);
  }

  if (NULL == encoded->data) {
    TRAE("Cannot handle the encoded data as the `data` is NULL. (exiting).");
    exit(EXIT_FAILURE);
  }

  ctx = (app*) user;
  if (NULL == ctx) {
    TRAE("Got encoded data but the `user` pointer is NULL. (exiting).");
    exit(EXIT_FAILURE);
  }

  if (NULL == ctx->trans_fp) {
    TRAE("Got encoded data but the `trans_fp` FILE* is NULL. (exiting).");
    exit(EXIT_FAILURE);
  }

  r = fwrite(encoded->data, encoded->size, 1, ctx->trans_fp);
  if (1 != r) {
    TRAE("Cannot handle encoded data. Failed to write the encoded data. (exiting)");
    exit(EXIT_FAILURE);
  }

  TRAD("Got encoded data, type: %u, data: %p, bytes: %p, size: %u", type, data, encoded->data, encoded->size);
  
 error:
  return r;
}

/* ------------------------------------------------------- */

static int app_create(app** ctx) {

  app* inst = NULL;
  int r = 0;

  inst = calloc(1, sizeof(app));
  if (NULL == inst) {
    TRAE("Failed to allocate the application intance.");
    r = -10;
    goto error;
  }

  /*
    Open the file into which we store the H264 from the generator
    (FFmpeg filter graph). We will decode the generated H264 and
    then encode again.
  */
  inst->gen_fp = fopen("./test-module-nvidia-transcoder-generated.h264", "wb");
  if (NULL == inst->gen_fp) {
    TRAE("Failed to open the output .h264 file for the generated h264.");
    r = -20;
    goto error;
  }

  /* Open the file into which we store the H264 from the transcoder. */
  inst->trans_fp = fopen("./test-module-nvidia-transcoder-transcoded.h264", "wb");
  if (NULL == inst->trans_fp) {
    TRAE("Failed to open the output .h264 file for the transcoded data.");
    r = -30;
    goto error;
  }

  /* ------------------------------------------------------- */
  /* Setup the test source generator.                        */
  /* ------------------------------------------------------- */

  inst->graph = avfilter_graph_alloc();
  if (NULL == inst) {
    TRAE("Failed to allocate the filter graph.");
    r = -10;
    goto error;
  }

  r = avfilter_graph_parse2(
    inst->graph,
    "testsrc@roxlu=duration=10:size=1280x720,format=pix_fmts=yuv420p,buffersink@roxlu",
    &inst->filter_inputs,
    &inst->filter_outputs
  );

  if (r < 0) {
    TRAE("Failed to create the filter graph.");
    r = -20;
    goto error;
  }

  r = avfilter_graph_config(inst->graph, NULL);
  if (r < 0) {
    TRAE("Failed to config the filter graph.");
    r = -30;
    goto error;
  }
               
  inst->filter_sink = avfilter_graph_get_filter(inst->graph, "buffersink@roxlu");
  if (NULL == inst->filter_sink) {
    TRAE("Failed to get the buffersink from the graph.");
    r = -40;
    goto error;
  }

  inst->image_width = av_buffersink_get_w(inst->filter_sink);
  inst->image_height = av_buffersink_get_h(inst->filter_sink);

  inst->frame = av_frame_alloc();
  if (NULL == inst->frame) {
    TRAE("Failed to allocate the AVFrame.");
    r = -50;
    goto error;
  }

  /* ------------------------------------------------------- */
  /* Setup the FFmpeg encoder.                               */
  /* ------------------------------------------------------- */

  inst->codec_inst = avcodec_find_encoder_by_name("libx264");
  if (NULL == inst->codec_inst) {
    TRAE("Failed to find the `libx264` encoder that we use to generate h264 for the decoder.");
    r = -60;
    goto error;
  }

  inst->codec_ctx = avcodec_alloc_context3(inst->codec_inst);
  if (NULL == inst->codec_ctx) {
    TRAE("Failed to allocate the `AVCodecContext`.");
    r = -70;
    goto error;
  }

  inst->codec_ctx->bit_rate = 700e3;
  inst->codec_ctx->width = 1280;
  inst->codec_ctx->height = 720;
  inst->codec_ctx->gop_size = 10;
  inst->codec_ctx->max_b_frames = 0;
  inst->codec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
  inst->codec_ctx->time_base = (AVRational){1, 25};
  inst->codec_ctx->framerate = (AVRational){25, 1};

  r = avcodec_open2(inst->codec_ctx, inst->codec_inst, NULL);
  if (r < 0) {
    TRAE("Failed to open the encoder.");
    r = -80;
    goto error;
  }
  
  inst->pkt = av_packet_alloc();
  if (NULL == inst->pkt) {
    TRAE("Failed to allocate the `AVPacket` that we need to receive the encoded data.");
    r = -90;
    goto error;
  }

  /* ------------------------------------------------------- */
  /* Setup trameleon.                                        */
  /* ------------------------------------------------------- */

  r = tra_core_create(&inst->core_cfg, &inst->core);
  if (r < 0) {
    TRAE("Failed to create the core context.");
    r = -82;
    goto error;
  }

  /* Create the cuda context */
  r = tra_core_api_get(inst->core, "cuda", (void**) &inst->cu_api);
  if (r < 0) {
    TRAE("Failed to get the CUDA api.");
    r = -83;
    goto error;
  }

  r = inst->cu_api->init();
  if (r < 0) {
    TRAE("Failed to initialize the CUDA api.");
    r = -84;
    goto error;
  }

  inst->cu_api->device_list();

  /* Use the default device. */
  r = inst->cu_api->device_create(0, &inst->cu_dev);
  if (r < 0) {
    TRAE("Failed to create a cuda device.");
    r = -85;
    goto error;
  }

  r = inst->cu_api->context_create(inst->cu_dev, 0, &inst->cu_ctx);
  if (r < 0) {
    TRAE("Failed to create the cuda context.");
    r = -86;
    goto error;
  }

  r = inst->cu_api->device_get_handle(inst->cu_dev, &inst->nvdec_settings.cuda_device_handle);
  if (r < 0) {
    TRAE("Failed to get the cuda device handle.");
    r = -87;
    goto error;
  }

  r = inst->cu_api->context_get_handle(inst->cu_ctx, &inst->nvdec_settings.cuda_context_handle);
  if (r < 0) {
    TRAE("Failed to get the cuda context handle.");
    r = -88;
    goto error;
  }

  /* Create the nvidia decoder context. */
  inst->dec_settings.callbacks.on_decoded_data = on_decoded_data;
  inst->dec_settings.callbacks.user = inst;
  inst->dec_settings.image_width = inst->codec_ctx->width;
  inst->dec_settings.image_height = inst->codec_ctx->height;
  inst->dec_settings.output_type = TRA_MEMORY_TYPE_CUDA;

  r = tra_core_decoder_create(inst->core, "nvdec", &inst->dec_settings, &inst->nvdec_settings, &inst->dec);
  if (r < 0) {
    TRAE("Failed to create `nvdec`.");
    r = -83;
    goto error;
  }

  /* Create the nvidia encoder context. */
  inst->enc_settings.image_width = inst->codec_ctx->width;
  inst->enc_settings.image_height = inst->codec_ctx->height;
  inst->enc_settings.image_format = TRA_IMAGE_FORMAT_NV12;
  inst->enc_settings.fps_num = 25;
  inst->enc_settings.fps_den = 1;
  inst->enc_settings.callbacks.on_encoded_data = on_encoded_data;
  inst->enc_settings.callbacks.user = inst;

  inst->nvenc_settings.cuda_context_handle = inst->nvdec_settings.cuda_context_handle;
  inst->nvenc_settings.cuda_device_handle = inst->nvdec_settings.cuda_device_handle;

  r = tra_core_encoder_create(inst->core, "nvenccuda", &inst->enc_settings, &inst->nvenc_settings, &inst->enc);
  if (r < 0) {
    TRAE("Failed to cretae `nvenc`.");
    r = -90;
    goto error;
  }

  /* Setup some app state. */
  inst->is_ready = 0;
  
  /* Finally assign the result. */
  *ctx = inst;

 error:
  
  if (r < 0) {
    
    if (NULL != inst) {
      app_destroy(inst);
      inst = NULL;
    }

    if (NULL != ctx) {
      *ctx = NULL;
    }
  }

  return r;
}

/* ------------------------------------------------------- */

static int app_destroy(app* ctx) {

  int result = 0;
  int r = 0;
    
  if (NULL == ctx) {
    TRAE("Cannot destroy the app as it's NULL.");
    return -10;
  }

  /* Destroy FFmpeg resources */
  if (NULL != ctx->graph) {
    avfilter_graph_free(&ctx->graph);
    ctx->graph = NULL;
  }

  if (NULL != ctx->frame) {
    av_frame_free(&ctx->frame);
    ctx->frame = NULL;
  }

  if (NULL != ctx->pkt) {
    av_packet_free(&ctx->pkt);
    ctx->pkt = NULL;
  }

  if (NULL != ctx->codec_ctx) {
    avcodec_free_context(&ctx->codec_ctx);
    ctx->codec_ctx = NULL;
  }
  
  /* Destroy encoder */
  if (NULL != ctx->enc) {
    r = tra_encoder_destroy(ctx->enc);
    if (r < 0) {
      TRAE("Failed to cleanly destroy the encoder.");
      result -= 40;
    }
  }
  
  /* Destroy CUDA */
  if (NULL != ctx->cu_dev
      && NULL != ctx->cu_api)
    {
      ctx->cu_api->device_destroy(ctx->cu_dev);
      ctx->cu_dev = NULL;
    }

  if (NULL != ctx->cu_ctx
      && NULL != ctx->cu_api)
    {
      ctx->cu_api->context_destroy(ctx->cu_ctx);
      ctx->cu_ctx = NULL;
    }

  /* Destroy decoder */
  if (NULL != ctx->dec) {
    tra_decoder_destroy(ctx->dec);
    ctx->dec = NULL;
  }
  
  if (NULL != ctx->core) {
    tra_core_destroy(ctx->core);
    ctx->core = NULL;
  }
  
  if (NULL != ctx->gen_fp) {
    fclose(ctx->gen_fp);
    ctx->gen_fp = NULL;
  }

  if (NULL != ctx->trans_fp) {
    fclose(ctx->trans_fp);
    ctx->trans_fp = NULL;
  }

  return result;
}

/* ------------------------------------------------------- */

static int app_encode(app* ctx) {

  int r = 0;

  if (NULL == ctx) {
    TRAE("Cannot encode as the given `app*` is NULL.");
    r = -10;
    goto error;
  }

  /* When EGAIN is triggered it means we need to feed a bit more data. */
  r = av_buffersink_get_frame(ctx->filter_sink, ctx->frame);
  if (AVERROR(EAGAIN) == r) {
    r = 0;
    goto error;
  }
  
  /* When the filter graph has finished flush the encoder. */
  if (AVERROR_EOF == r) {
    
    TRAI("Ready with encoding.");

    /* Flush the encoder. */
    r = encode_frame(ctx->codec_ctx, NULL, ctx->pkt, ctx->dec, ctx->gen_fp);
    if (r < 0) {
      TRAE("Failed to flush the encoder.");
      r = -110;
      goto error;
    }

    ctx->is_ready = 1;
    r = 0;
    goto error;
  }

  /* Encode */
  r = encode_frame(ctx->codec_ctx, ctx->frame, ctx->pkt, ctx->dec, ctx->gen_fp);
  if (r < 0) {
    TRAE("Failed to encode a frame.");
    r = -100;
    goto error;
  }

 error:
  return r;
}

/* ------------------------------------------------------- */
