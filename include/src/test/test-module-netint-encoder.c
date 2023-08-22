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

  NETINT ENCODER TEST
  ===================

  GENERAL INFO:

    This file creates an instance of the `nienc` and encodes raw
    NV12 data. The NV12 data is generated using the FFmpeg
    testsrc filter. This file was created during the development
    of the Trameleon library and is used to test the `netint`
    module.

*/

/* ------------------------------------------------------- */

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#include <libavfilter/buffersink.h>
#include <libavfilter/avfilter.h>
#include <libavutil/frame.h>

#include <tra/log.h>
#include <tra/module.h>
#include <tra/core.h>
#include <tra/types.h>

/* ------------------------------------------------------- */

static int on_encoded_data(uint32_t type, void* data, void* user);

/* ------------------------------------------------------- */

int main(int argc, const char* argv[]) {

  AVFilterContext* filter_sink = NULL;
  AVFilterInOut* filter_inputs = NULL;
  AVFilterInOut* filter_outputs = NULL;
  AVFilterGraph* graph = NULL;
  uint32_t image_width = 0;
  uint32_t image_height = 0;
  AVFrame* frame = NULL;
  FILE* fp = NULL;
  int r = 0;

  tra_core_settings core_cfg = { 0 };
  tra_core* core = NULL;

  tra_encoder_settings enc_opt = { 0 };
  tra_encoder* enc = NULL;
  tra_sample sample = { 0 };
  tra_memory_image img = { 0 };
  
  TRAD("NETINT Encoder Test");

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
    "testsrc@roxlu=duration=10:size=1280x720,format=pix_fmts=yuv420p, buffersink@roxlu",
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
  
  TRAD("Generating %u x %u yuv frames.", image_width, image_height);


  /* ------------------------------------------------------- */
  /* Setup the configs, output and encoder                   */
  /* ------------------------------------------------------- */

  fp = fopen("./test-module-netint-encoder.h264", "wb");
  if (NULL == fp) {
    TRAE("Failed to generate the output file.");
    r = -55;
    goto error;
  }
  
  r = tra_core_create(&core_cfg, &core);
  if (r < 0) {
    r = -60;
    goto error;
  }

  enc_opt.image_width = image_width;
  enc_opt.image_height = image_height;
  enc_opt.input_format = TRA_IMAGE_FORMAT_I420;
  enc_opt.output_format = TRA_MEMORY_TYPE_H264;
  enc_opt.fps_num = 25;
  enc_opt.fps_den = 1;
  enc_opt.bitrate = 700e3;
  enc_opt.callbacks.on_encoded_data = on_encoded_data;
  enc_opt.callbacks.user = fp;

  r = tra_core_encoder_create(core, "nienc", &enc_opt, NULL, &enc);
  
  if (r < 0) {
    r = -80;
    goto error;
  }

  /* ------------------------------------------------------- */
  /* Encode                                                  */
  /* ------------------------------------------------------- */

  TRAD("Encode %lld", sample.pts);

  while (1) {
    
    r = av_buffersink_get_frame(filter_sink, frame);
    if (AVERROR(EAGAIN) == r) {
      break;
    }

    if (AVERROR_EOF == r) {
      TRAI("Ready with encoding.");
      r = 0;
      break;
    }

    /* Setup the data we pass into the encoder. */
    img.image_width = frame->width;
    img.image_height = frame->height;
    img.image_format = enc_opt.input_format;

    img.plane_count = 2;
    
    img.plane_data[0] = frame->data[0];
    img.plane_data[1] = frame->data[1];
    img.plane_data[2] = frame->data[2];

    img.plane_strides[0] = frame->linesize[0];
    img.plane_strides[1] = frame->linesize[1];
    img.plane_strides[2] = frame->linesize[2];

    img.plane_heights[0] = frame->height;
    img.plane_heights[1] = frame->height / 2;
    img.plane_heights[2] = frame->height / 2;

    sample.pts = frame->pts;

    r = tra_encoder_encode(enc, &sample, TRA_MEMORY_TYPE_IMAGE, &img);
    if (r < 0) {
      TRAE("Failed to encode.");
      break;
    }
  }

    r = tra_encoder_flush(enc);
    if (r < 0) {
        TRAE("Failed to encode.");
        goto error;
    }

 error:

  if (NULL != enc) {
    tra_encoder_destroy(enc);
    enc = NULL;
  }
  
  if (NULL != core) {
    tra_core_destroy(core);
    core = NULL;
  }

  if (NULL != graph) {
    avfilter_graph_free(&graph);
    graph = NULL;
  }

  if (NULL != frame) {
    av_frame_free(&frame);
    frame = NULL;
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

    tra_memory_h264* encoded = (tra_memory_h264*) data;
  FILE* fp = (FILE*) user;
  int r = 0;
  
  if (TRA_MEMORY_TYPE_H264 != type) {
    TRAE("Cannot handle encoded data, unsupported type: %u. (exiting)", type);
    exit(EXIT_FAILURE);
  }

  if (NULL == encoded) {
    TRAE("Cannot handle encoded data, `data` is NULL. (exiting).");
    exit(EXIT_FAILURE);
  }

  if (NULL == fp) {
    TRAE("Cannot handle encoded data, `user` is NULL. (exiting).");
    exit(EXIT_FAILURE);
  }

  if (0 == encoded->size) {
    TRAE("Cannot handle encoded data, the `size` is 0. (exiting).") ;
    exit(EXIT_FAILURE);
  }

  r = fwrite(encoded->data, encoded->size, 1, fp);
  if (1 != r) {
    TRAE("Cannot handle encoded data. Failed to write the encoded data. (exiting)");
    exit(EXIT_FAILURE);
  }
  
  TRAD("Got encoded data, type: %u, data: %p, bytes: %p, size: %u", type, data, encoded->data, encoded->size);

  return 0;
}

/* ------------------------------------------------------- */
