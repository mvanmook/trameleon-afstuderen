/* ------------------------------------------------------- */

#include <libavcodec/avcodec.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/avfilter.h>
#include <libavutil/frame.h>
#include <libavutil/pixdesc.h>
#include <dev/generator.h>
#include <tra/types.h>
#include <tra/log.h>

/* ------------------------------------------------------- */

static int encode_frame(dev_generator* ctx);

/* ------------------------------------------------------- */

struct dev_generator {

  /* FFmpeg */
  AVFilterContext* filter_sink;
  AVFilterInOut* filter_inputs;
  AVFilterInOut* filter_outputs;
  AVFilterGraph* graph;
  AVCodecContext* codec_ctx;
  const AVCodec* codec_inst;
  AVPacket* pkt;
  AVFrame* frame;

  /* General */
  dev_generator_settings settings;
  int is_ready;
};

/* ------------------------------------------------------- */

/*
  This creates a filter which generates YUV data in the NV12
  format.  You can use the `YUV420p` by passing
  `pix_fmts=yuv420p` instead of `pix_fmts=nv12`, though make sure
  to also change the `tra_memory_image.image_format` type that we pass
  into the `on_frame` callback.
*/
int dev_generator_create(
  dev_generator_settings* cfg,
  dev_generator** ctx
)
{
  char filter_str[256] = { 0 };
  dev_generator* inst = NULL;
  int r = 0;

  if (NULL == cfg) {
    TRAE("Cannot create the generator as the given settings is NULL.");
    r = -10;
    goto error;
  }

  if (0 == cfg->image_width) {
    TRAE("`image_width` is not set.");
    r = -15;
    goto error;
  }

  if (0 == cfg->image_height) {
    TRAE("`image_height` is not set.");
    r = -17;
    goto error;
  }

  if (0 == cfg->duration) {
    TRAE("`duration` not set.");
    r = -18;
    goto error;
  }

  if (NULL == cfg->on_encoded_data) {
    TRAE("`on_encoded_data` is not set.");
    r = -18;
    goto error;
  }

  if (NULL == ctx) {
    TRAE("Cannot create the generator as the given `dev_generator**` are NULL.");
    r = -20;
    goto error;
  }

  if (NULL != *ctx) {
    TRAE("Cannot create the generator as the `*(dev_generator**)` is not NULL. Already initialized?");
    r = -30;
    goto error;
  }

  inst = calloc(1, sizeof(dev_generator));
  if (NULL == inst) {
    TRAE("Failed to allocate the `dev_generator`.");
    r = -40;
    goto error;
  }

  inst->graph = avfilter_graph_alloc();
  if (NULL == inst) {
    TRAE("Failed to allocate the filter graph.");
    r = -50;
    goto error;
  }

  /* Generate the filter string. */
  r = snprintf(
    filter_str,
    sizeof(filter_str),
    "testsrc@roxlu=duration=%u:size=%ux%u,format=pix_fmts=nv12,buffersink@roxlu", 
    cfg->duration,
    cfg->image_width,
    cfg->image_height
  );

  if (r < 0) {
    TRAE("Failed to generate the filter graph string.");
    r = -60;
    goto error;
  }
  
  r = avfilter_graph_parse2(
    inst->graph,
    filter_str,
    &inst->filter_inputs,
    &inst->filter_outputs
  );

  TRAD("Filter graph: %s.", filter_str);

  if (r < 0) {
    TRAE("Failed to create the filter graph.");
    r = -60;
    goto error;
  }

  r = avfilter_graph_config(inst->graph, NULL);
  if (r < 0) {
    TRAE("Failed to config the filter graph.");
    r = -70;
    goto error;
  }
               
  inst->filter_sink = avfilter_graph_get_filter(inst->graph, "buffersink@roxlu");
  if (NULL == inst->filter_sink) {
    TRAE("Failed to get the buffersink from the graph.");
    r = -80;
    goto error;
  }

  inst->frame = av_frame_alloc();
  if (NULL == inst->frame) {
    TRAE("Failed to allocate the AVFrame.");
    r = -90;
    goto error;
  }

  /* ------------------------------------------------------- */
  /* Setup the FFmpeg encoder.                               */
  /* ------------------------------------------------------- */

  inst->codec_inst = avcodec_find_encoder_by_name("libx264");
  if (NULL == inst->codec_inst) {
    TRAE("Failed to find the `libx264` encoder that we use to generate h264 for the decoder.");
    r = -100;
    goto error;
  }

  inst->codec_ctx = avcodec_alloc_context3(inst->codec_inst);
  if (NULL == inst->codec_ctx) {
    TRAE("Failed to allocate the `AVCodecContext`.");
    r = -110;
    goto error;
  }

  inst->codec_ctx->bit_rate = 700e3;
  inst->codec_ctx->width = cfg->image_width;
  inst->codec_ctx->height = cfg->image_height;
  inst->codec_ctx->gop_size = 10;
  inst->codec_ctx->max_b_frames = 0;
  inst->codec_ctx->pix_fmt = AV_PIX_FMT_NV12;
  inst->codec_ctx->time_base = (AVRational){1, 25};
  inst->codec_ctx->framerate = (AVRational){25, 1};

  r = avcodec_open2(inst->codec_ctx, inst->codec_inst, NULL);
  if (r < 0) {
    TRAE("Failed to open the encoder.");
    r = -120;
    goto error;
  }

  inst->pkt = av_packet_alloc();
  if (NULL == inst->pkt) {
    TRAE("Failed to allocate the `AVPacket` that we need to receive the encoded data.");
    r = -130;
    goto error;
  }

  /* Finally set the result. */
  inst->settings = *cfg;
  inst->is_ready = 0;
  
  *ctx = inst;

 error:
  return r;
}

/* ------------------------------------------------------- */

int dev_generator_destroy(dev_generator* ctx) {

  int result = 0;
  int r = 0;

  if (NULL == ctx) {
    TRAE("Cannot destroy the generator; given `ctx` is NULL.");
    result = -10;
    goto error;
  }

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

 error:
  return r;
}

/* ------------------------------------------------------- */

int dev_generator_update(dev_generator* ctx) {

  int r = 0;

  if (NULL == ctx) {
    TRAE("Cannot update as the given `dev_generator*` is NULL.");
    r = -10;
    goto error;
  }

  if (1 == ctx->is_ready) {
    TRAD("We're ready with encoding; no need to call `dev_generator_update()`.");
    r = -15;
    goto error;
  }

  if (NULL == ctx->codec_ctx) {
    TRAE("Cannot update as the given `dev_generator::codec_ctx` is NULL.");
    r = -20;
    goto error;
  }

  if (NULL == ctx->pkt) {
    TRAE("Cannot update as the given `dev_generator::pkt` is NULL.");
    r = -30;
    goto error;
  }

  if (NULL == ctx->frame) {
    TRAE("Cannot update as the given `dev_generator::frame` in NULL.");
    r = -40;
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
    ctx->is_ready = 1;
  }

  /* Encode */
  r = encode_frame(ctx);
  if (r < 0) {
    TRAE("Failed to encode a frame.");
    r = -100;
    goto error;
  }

 error:
  return r;
}

/* ------------------------------------------------------- */

static int encode_frame(dev_generator* ctx) {

  tra_memory_image host_image = { 0 };
  tra_memory_h264 host_h264 = { 0 };
  AVFrame* frame = NULL;
  int r = 0;

  if (NULL == ctx) {
    TRAE("Cannot encode; given `dev_generator*` is NULL.");
    r = -10;
    goto error;
  }

  if (NULL == ctx->pkt) {
    TRAE("Cannot encode, `pkt` is NULL.");
    r = -20;
    goto error;
  }

  if (NULL == ctx->frame) {
    TRAE("Cannot encode `frame` is NULL.");
    r = -30;
    goto error;
  }

  if (NULL == ctx->codec_ctx) {
    TRAE("Cannot encode `coded_ctx` is NULL.");
    r = -40;
    goto error;
  }

  /* Only set the frame when we're not ready. Using a NULL for frame flushes the encoder. */
  if (0 == ctx->is_ready) {
    frame = ctx->frame;
  }

  r = avcodec_send_frame(ctx->codec_ctx, frame);
  if (r < 0) {
    TRAE("Failed to send a frame into the encoder.");
    r = -30;
    goto error;
  }

  /* Notify the user we have a NV12 frame. */
  if (NULL != frame
      && ctx->settings.on_frame)
    {

      if (AV_PIX_FMT_NV12 != frame->format) {
        TRAE("Cannot hand over the generated frame as the pixel format is not TRA_IMAGE_FORMAT_NV12");
        r = -50;
        goto error;
      }
      
      host_image.image_format = TRA_IMAGE_FORMAT_NV12;
      host_image.image_width = ctx->settings.image_width;
      host_image.image_height = ctx->settings.image_height;
    
      host_image.plane_data[0] = frame->data[0];
      host_image.plane_data[1] = frame->data[1];
      host_image.plane_data[2] = frame->data[2];
      host_image.plane_count = 2;

      host_image.plane_strides[0] = frame->linesize[0];
      host_image.plane_strides[1] = frame->linesize[1];
      host_image.plane_strides[2] = frame->linesize[2];

      host_image.plane_heights[0] = frame->height;
      host_image.plane_heights[1] = frame->height / 2;
      host_image.plane_heights[2] = 0;

      r = ctx->settings.on_frame(
        TRA_MEMORY_TYPE_IMAGE,
        &host_image,
        ctx->settings.user
      );

      if (r < 0) {
        TRAE("The user failed to handle the generated image.");
        r = -40;
        goto error;
      }
  }
  
  while (r >= 0) {

    r = avcodec_receive_packet(ctx->codec_ctx, ctx->pkt);
    if (AVERROR(EAGAIN) == r) {
      r = 0;
      break;
    }

    if (AVERROR_EOF == r) {
      TRAI("Encoder returned OEF");
      r = 0;
      break;
    }

    if (0 != ctx->pkt->size
        || NULL != ctx->pkt->data)
      {

        /* Check if we're dealing with a key frame. */
        host_h264.flags = TRA_MEMORY_FLAG_NONE;
        
        if (AV_PKT_FLAG_KEY == ctx->pkt->flags & AV_PKT_FLAG_KEY) {
          host_h264.flags |= TRA_MEMORY_FLAG_IS_KEY_FRAME;
        }
        
        host_h264.size = ctx->pkt->size;
        host_h264.data = ctx->pkt->data;

        /* Pass the encoded data to the user. */
        ctx->settings.on_encoded_data(
          TRA_MEMORY_TYPE_H264,
          &host_h264,
          ctx->settings.user
        );
      }

    av_packet_unref(ctx->pkt);
  }

  /* When we arrive here everything is good ... */
  r = 0;

 error:
  return r;
}

/* ------------------------------------------------------- */

/* Returns 1 when ready, 0 when not ready and < 0 in case of an error. */
int dev_generator_is_ready(dev_generator* ctx) {

  if (NULL == ctx) {
    TRAE("Cannot check if the generator is ready as the given `dev_generator` is NULL.");
    return -10;
  }

  return ctx->is_ready;
}

/* ------------------------------------------------------- */
