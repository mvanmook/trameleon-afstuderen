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

  NVIDIA DECODER TEST
  ===================

  GENERAL INFO:

    This file creates an instance of `nvdec` and decodes some
    h264. This file was used during the development of the NVIDIA
    based decoder.

    This test creates a source filter that produces raw YUV420p
    frames which are then passed into an encoder using
    FFmpeg. You can use `ffmpeg -encoders -hide_banner` to get a
    list of available encoders; the name of the codec is passed into
    `avcodec_find_encoder_by_name`, e.g.

       ffmpeg -encoders -hide_banner | grep -i 264

    Once we have a raw H264 frame, we pass it into the NVIDIA
    decoder module from Trameleon. We receive decoded frames in
    our `on_decoded_data` callback. We store the first decoded
    frame into a .yuv file that can viewed using the YUView util.

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
#include <tra/registry.h>
#include <tra/module.h>
#include <tra/types.h>
#include <tra/core.h>
#include <tra/log.h>

/* ------------------------------------------------------- */

static int encode_frame(AVCodecContext* codecCtx, AVFrame* frame, AVPacket* pkt, tra_decoder* dec, FILE* outputFile);
static int on_decoded_data(uint32_t type, void* data, void* user);

/* ------------------------------------------------------- */

int main(int argc, const char* argv[]) {

  AVFilterContext* filter_sink = NULL;
  AVFilterInOut* filter_inputs = NULL;
  AVFilterInOut* filter_outputs = NULL;
  AVFilterGraph* graph = NULL;
  AVCodecContext* codec_ctx = NULL;
  const AVCodec* codec_inst = NULL;
  AVPacket* pkt = NULL;
  
  uint32_t image_width = 0;
  uint32_t image_height = 0;
  AVFrame* frame = NULL;
  FILE* fp = NULL;
  int r = 0;

  tra_nvdec_settings nv_settings = { 0 };
  tra_decoder_callbacks dec_callbacks = { 0 };
  tra_decoder_settings dec_settings = { 0 };
  tra_decoder* dec = NULL;
  tra_core_settings core_cfg = { 0 };
  tra_core* core = NULL;

  /* We need to create a cuda context otherwise we can't decode. */
  tra_cuda_context* cu_ctx = NULL;
  tra_cuda_device* cu_dev = NULL;
  tra_cuda_api* cu_api = NULL;

  /* We also store the generated h264 into this file; just to make sure the data that is created is correct. */
  fp = fopen("./test-module-nvidia-decoder.h264", "wb");
  if (NULL == fp) {
    TRAE("Failed to open the output .h264 file.");
    r = -5;
    goto error;
  }

  /* ------------------------------------------------------- */
  /* Setup the test source generator.                        */
  /* ------------------------------------------------------- */

  graph = avfilter_graph_alloc();
  if (NULL == graph) {
    TRAE("Failed to allocate the filter graph.");
    r = -10;
    goto error;
  }

  r = avfilter_graph_parse2(
    graph,
    "testsrc@roxlu=duration=1:size=1280x720,format=pix_fmts=yuv420p,buffersink@roxlu",
    &filter_inputs,
    &filter_outputs
  );

  if (r < 0) {
    TRAE("Failed to create the filter graph.");
    r = -20;
    goto error;
  }

  r = avfilter_graph_config(graph, NULL);
  if (r < 0) {
    TRAE("Failed to config the filter graph.");
    r = -30;
    goto error;
  }
               
  filter_sink = avfilter_graph_get_filter(graph, "buffersink@roxlu");
  if (NULL == filter_sink) {
    TRAE("Failed to get the buffersink from the graph.");
    r = -40;
    goto error;
  }

  image_width = av_buffersink_get_w(filter_sink);
  image_height = av_buffersink_get_h(filter_sink);

  frame = av_frame_alloc();
  if (NULL == frame) {
    TRAE("Failed to allocate the AVFrame.");
    r = -50;
    goto error;
  }

  /* ------------------------------------------------------- */
  /* Setup the encoder.                                      */
  /* ------------------------------------------------------- */

  codec_inst = avcodec_find_encoder_by_name("libx264");
  if (NULL == codec_inst) {
    TRAE("Failed to find the `libx264` encoder that we use to generate h264 for the decoder.");
    r = -60;
    goto error;
  }

  codec_ctx = avcodec_alloc_context3(codec_inst);
  if (NULL == codec_ctx) {
    TRAE("Failed to allocate the `AVCodecContext`.");
    r = -70;
    goto error;
  }

  codec_ctx->bit_rate = 700e3;
  codec_ctx->width = 1280;
  codec_ctx->height = 720;
  codec_ctx->gop_size = 10;
  codec_ctx->max_b_frames = 0;
  codec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
  codec_ctx->time_base = (AVRational){1, 25};
  codec_ctx->framerate = (AVRational){25, 1};

  r = avcodec_open2(codec_ctx, codec_inst, NULL);
  if (r < 0) {
    TRAE("Failed to open the encoder.");
    r = -80;
    goto error;
  }

  /* ------------------------------------------------------- */
  /* Setup trameleon.                                        */
  /* ------------------------------------------------------- */

  r = tra_core_create(&core_cfg, &core);
  if (r < 0) {
    TRAE("Failed to create the core context.");
    r = -82;
    goto error;
  }

  /* Create the cuda context */
  r = tra_core_api_get(core, "cuda", (void**) &cu_api);
  if (r < 0) {
    TRAE("Failed to get the CUDA api.");
    r = -83;
    goto error;
  }

  r = cu_api->init();
  if (r < 0) {
    TRAE("Failed to initialize the CUDA api.");
    r = -84;
    goto error;
  }

  cu_api->device_list();

  /* Use the default device. */
  r = cu_api->device_create(0, &cu_dev);
  if (r < 0) {
    TRAE("Failed to create a cuda device.");
    r = -85;
    goto error;
  }

  r = cu_api->context_create(cu_dev, 0, &cu_ctx);
  if (r < 0) {
    TRAE("Failed to create the cuda context.");
    r = -86;
    goto error;
  }

  r = cu_api->device_get_handle(cu_dev, &nv_settings.cuda_device_handle);
  if (r < 0) {
    TRAE("Failed to get the cuda device handle.");
    r = -87;
    goto error;
  }

  r = cu_api->context_get_handle(cu_ctx, &nv_settings.cuda_context_handle);
  if (r < 0) {
    TRAE("Failed to get the cuda context handle.");
    r = -88;
    goto error;
  }

  /* Create the nvidia decoder context. */
  dec_settings.callbacks.on_decoded_data = on_decoded_data;
  dec_settings.callbacks.user = NULL;
  dec_settings.image_width = codec_ctx->width;
  dec_settings.image_height = codec_ctx->height;
  dec_settings.output_type = TRA_MEMORY_TYPE_IMAGE;

  r = tra_core_decoder_create(core, "nvdec", &dec_settings, &nv_settings, &dec);
  if (r < 0) {
    TRAE("Failed to create `nvdec`.");
    r = -83;
    goto error;
  }

  /* -------------------------------------------------------------- */
  /* Extract raw frames, encode them and feed them into the decoder */
  /* -------------------------------------------------------------- */

  pkt = av_packet_alloc();
  if (NULL == pkt) {
    TRAE("Failed to allocate the `AVPacket` that we need to receive the encoded data.");
    r = -90;
    goto error;
  }
  
  while (1) {
    
    r = av_buffersink_get_frame(filter_sink, frame);
    if (AVERROR(EAGAIN) == r) {
      break;
    }

    if (AVERROR_EOF == r) {
      TRAI("Ready with encoding.");
      break;
    }

    r = encode_frame(codec_ctx, frame, pkt, dec, fp);
    if (r < 0) {
      TRAE("Failed to encode a frame.");
      r = -100;
      goto error;
    }
  }

  /* Flush encoder */
  r = encode_frame(codec_ctx, NULL, pkt, dec, fp);
  if (r < 0) {
    TRAE("Failed to flush the encoder.");
    r = -110;
    goto error;
  }

 error:

  if (NULL != graph) {
    avfilter_graph_free(&graph);
    graph = NULL;
  }

  if (NULL != frame) {
    av_frame_free(&frame);
    frame = NULL;
  }

  if (NULL != pkt) {
    av_packet_free(&pkt);
    pkt = NULL;
  }

  if (NULL != codec_ctx) {
    avcodec_free_context(&codec_ctx);
    codec_ctx = NULL;
  }

  if (NULL != cu_dev
      && NULL != cu_api)
    {
      cu_api->device_destroy(cu_dev);
      cu_dev = NULL;
    }

  if (NULL != cu_ctx
      && NULL != cu_api)
    {
      cu_api->context_destroy(cu_ctx);
      cu_ctx = NULL;
    }

  if (NULL != dec) {
    tra_decoder_destroy(dec);
    dec = NULL;
  }

  if (NULL != core) {
    tra_core_destroy(core);
    core = NULL;
  }

  if (NULL != fp) {
    fclose(fp);
    fp = NULL;
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
  This function will be called when the decoder has a frame that
  can be displayed. For now we store the first decoded frame into a
  .yuv file that can be easily opened by the YUView util. We use
  the file naming convention that YUView uses for NV12.

     something_<width>x<height>_yuv420pUVI.yuv

 */
static int on_decoded_data(
  uint32_t type,
  void* data,
  void* user
)
{
  
  static int wrote_to_file = 0;
  tra_memory_image* img = NULL;
  char fname[256] = { 0 };
  uint32_t done = 0;
  uint32_t i = 0;
  uint32_t j = 0;
  FILE* fp = NULL;  
  int r = 0;

  if (NULL == data) {
    TRAE("Our `on_decoded_data` was called but the received data is NULL.");
    r = -10;
    goto error;
  }

  if (TRA_MEMORY_TYPE_IMAGE != type) {
    TRAE("Unhandled decoded image type.");
    r = -20;
    goto error;
  }

  /* We only write the first frame. */
  if (0 != wrote_to_file) {
    r = 0;
    goto error;
  }

  img = (tra_memory_image*) data;
  if (0 == img->image_width) {
    TRAE("Cannot handle decoded image; width not set.");
    r = -30;
    goto error;
  }

  if (0 == img->image_height) {
    TRAE("Cannot handle decoded image; height not set.");
    r = -40;
    goto error;
  }

  if (TRA_IMAGE_FORMAT_NV12 != img->image_format) {
    TRAE("Cannot handle the decoded image; we only support nv12 for now.");
    r = -50;
    goto error;
  }

  r = snprintf(fname, sizeof(fname), "nvidia_%ux%u_yuv420pUVI.yuv", img->image_width, img->image_height);
  if (r < 0) {
    TRAE("Failed to create the destination filename.");
    r = -60;
    goto error;
  }

  /* Create a file with a name that can be automatically parsed by YUView */
  fp = fopen(fname, "wb");
  if (NULL == fp) {
    TRAE("Failed to open test output file.");
    r = -20;
    goto error;
  }

  /* Y-plane */
  for (j = 0; j < img->plane_heights[0]; ++j) {

    done = fwrite(
      img->plane_data[0] + (j * img->plane_strides[0]),
      img->image_width,
      1,
      fp
    );
    
    if (1 != done) {
      TRAE("Failed to write the decoded frame.");
      r = -70;
      goto error;
    }
  }

  /* UV plane */
  for (j = 0; j < img->plane_heights[1]; ++j) {

    done = fwrite(
      img->plane_data[1] + (j * img->plane_strides[1]),
      img->image_width,
      1,
      fp
    );
    
    if (1 != done) {
      TRAE("Failed to write the decoded frame.");
      r = -70;
      goto error;
    }
  }

  fclose(fp);

  /* Only write once. */
  TRAI("Stored the decoded data into: `%s`.", fname);
  wrote_to_file = 1;

 error:
  return r;
}

/* ------------------------------------------------------- */
