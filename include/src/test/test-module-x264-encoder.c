/* ------------------------------------------------------- */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/frame.h>

#include <tra/registry.h>
#include <tra/module.h>
#include <tra/types.h>
#include <tra/core.h>
#include <tra/dict.h>
#include <tra/log.h>

/* ------------------------------------------------------- */

static int on_encoded_data(uint32_t type, void* data, void* user);

/* ------------------------------------------------------- */

int main(int argc, const char* argv[]) {

  FILE* fp = NULL; 
  AVFilterContext* filter_sink = NULL;
  AVFilterGraph* graph = NULL;
  AVFilterInOut* filter_inputs = NULL;
  AVFilterInOut* filter_outputs = NULL;
  AVFrame* frame = NULL;
  uint32_t image_width = 0;
  uint32_t image_height = 0;

  tra_encoder_settings enc_opt;
  tra_encoder* enc = NULL;
  tra_memory_image img = { 0 };
  tra_sample sample = { 0 };
  
  tra_core_settings core_cfg = { };
  tra_core* core = NULL;
  int status = 0;
  int r = 0;
  
  TRAD("X264 Encoder Test");

  /* ------------------------------------------------------------ */
  /* Create a filter graph to generate YUV data.                   */
  /* ------------------------------------------------------------ */

  graph = avfilter_graph_alloc();
  if (NULL == graph) {
    TRAE("Failed to create the filter graph.");
    r = -10;
    goto error;
  }

  /* @important: Do not change the pixel format; is used below too. */
  r = avfilter_graph_parse2(
    graph,
    "testsrc@roxlu=duration=20:size=1280x720,format=pix_fmts=nv12,buffersink@roxlu",
    &filter_inputs,
    &filter_outputs
  );
  
  if (r < 0) {
    TRAE("Failed to parse the filter graph.");
    r = -20;
    goto error;
  }

  r = avfilter_graph_config(graph, NULL);
  if (r < 0) {
    TRAE("Failed to configure the filter graph.");
    r = -30;
    goto error;
  }

  filter_sink = avfilter_graph_get_filter(graph, "buffersink@roxlu");
  if (NULL == filter_sink) {
    TRAE("Failed to get the filter sink context.");
    r = -40;
    goto error;
  }

  image_width = av_buffersink_get_w(filter_sink);
  image_height = av_buffersink_get_h(filter_sink);

  TRAD(">> %u x %u", image_width, image_height);

  frame = av_frame_alloc();
  if (NULL == frame) {
    TRAE("Failed to allocate the AVFrame.");
    r = -50;
    goto error;
  }

  /* ------------------------------------------------------------ */
  /* Create the output file                                       */
  /* ------------------------------------------------------------ */

  fp = fopen("./test-module-x264.h264", "wb");
  if (NULL == fp) {
    TRAE("Failed to generate the output file.");
    r = -60;
    goto error;
  }

  /* ------------------------------------------------------------ */
  /* Create the encoder                                           */
  /* ------------------------------------------------------------ */

  r = tra_core_create(&core_cfg, &core);
  if (r < 0) {
    r = -70;
    goto error;
  }

  enc_opt.image_width = image_width;
  enc_opt.image_height = image_height;
  enc_opt.image_format = TRA_IMAGE_FORMAT_NV12;

  enc_opt.callbacks.on_encoded_data = on_encoded_data;
  enc_opt.callbacks.user = fp;

  r = tra_core_encoder_create(core, "x264enc", &enc_opt, NULL, &enc);
  if (r < 0) {
    r = -90;
    goto error;
  }

  /* ------------------------------------------------------------ */
  /* Encode                                                       */
  /* ------------------------------------------------------------ */

  img.image_width = image_width;
  img.image_height = image_height;
  img.image_format = TRA_IMAGE_FORMAT_NV12;
  img.plane_count = 2;
  
  while(1) {
    
    r = av_buffersink_get_frame(filter_sink, frame);
    if (AVERROR(EAGAIN) == r) {
      break;
    }
    
    if(AVERROR_EOF == r) {
      TRAD("Ready with encoding.");
      break;
    }

    TRAD("Encoding another frame ... %lld", frame->pts);

    /* Setup the data we pass into the encoder. */
    img.image_width = frame->width;
    img.image_height = frame->height;
    
    img.plane_data[0] = frame->data[0];
    img.plane_data[1] = frame->data[1];
    img.plane_data[2] = NULL;
    
    img.plane_strides[0] = frame->linesize[0];
    img.plane_strides[1] = frame->linesize[1];
    img.plane_strides[2] = 0;

    sample.pts = frame->pts;

    r = tra_encoder_encode(enc, &sample, TRA_MEMORY_TYPE_IMAGE, &img);
    if (r < 0) {
      TRAE("Failed to encode.");
      break;
    }
  }
  
 error:

  if (NULL != enc) {
    status = tra_encoder_destroy(enc);
    if (status < 0) {
      TRAE("Failed to cleanly destroy the encoder.");
      r = -1;
    }
  }

  if (NULL != core) {
    tra_core_destroy(core);
    core = NULL;
  }

  if (NULL != frame) {
    av_frame_free(&frame);
    frame = NULL;
  }

  if (NULL != graph) {
    avfilter_graph_free(&graph);
    graph = NULL;
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

static int on_encoded_data(uint32_t type, void* data, void* user) {

  tra_encoded_host_memory* encoded = (tra_encoded_host_memory*) data;
  FILE* fp = (FILE*) user;
  int r = 0;

  if (TRA_MEMORY_TYPE_HOST_H264 != type) {
    TRAE("Received encoded data, but we don't handle this specific type: %u. (exiting).", type);
    exit(EXIT_FAILURE);
  }

  if (NULL == fp) {
    TRAE("Received encoded data, but we can't handle this as the `user` pointer is NULL. This should point to a FILE. (exiting).");
    exit(EXIT_FAILURE);
  }

  if (NULL == encoded) {
    TRAE("Received encoded data, but we can't handle this as the `data` pointer is NULL. (exiting).");
    exit(EXIT_FAILURE);
  }

  r = fwrite(encoded->data, encoded->size, 1, fp);
  if (1 != r) {
    TRAE("Failed to write the encoded data. (exiting)");
    exit(EXIT_FAILURE);
  }
  
  TRAD("Got encoded data, type: %u, data: %p, bytes: %p", type, data, encoded->data);

  return 0;
};

/* ------------------------------------------------------- */
