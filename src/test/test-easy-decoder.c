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

  EASY DECODER DEVELOPMENT TEST
  =============================

  GENERAL INFO:

    I've created this test while implementing the easy decoder
    application.  See `tra/modules/easy/easy-decoder.c` for the
    source that implements some of the boiler plate code that you
    would normally implement when directly using the decoders,
    encoders, etc. These are things like creating a CUDA context
    when working with the NVIDIA NVDEC/NVENC. By using the easy
    API, you don't have to implement that.

    We use FFmpeg to read a .h264 file. Make sure to create one
    using the command below:x

  GENERATE TEST FILE:

     ffmpeg -f lavfi -i testsrc=duration=10:size=1280x720:rate=30 -c:v libx264 -profile:v baseline -pix_fmt nv12 -level:v 1.2 -preset veryfast -an -bsf h264_mp4toannexb timer-baseline-1280x720.h264
  
 */
/* ------------------------------------------------------- */

#include <stdlib.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <tra/easy.h>
#include <tra/log.h>

/* ------------------------------------------------------- */

typedef struct app app;

/* ------------------------------------------------------- */

struct app {
  AVFormatContext* fmt_ctx;
  AVPacket* pkt;
  tra_easy_settings easy_cfg;
  tra_easy* easy_ctx; 
  FILE* output_fp;        /* The file into which we save the decoded output file. */
  uint8_t output_written; /* Set to `1` when we've stored an YUV output file. */
};

/* ------------------------------------------------------- */

static int app_init(app* ctx, const char* inputFilename, const char* outputFilename);
static int app_execute(app* ctx);
static int app_shutdown(app* ctx);
static int app_on_decoded(uint32_t type, void* data, void* user);

/* ------------------------------------------------------- */

int main(int argc, const char* argv[]) {

  app ctx = { 0 };
  int r = 0;

  TRAD("Easy Decoder");
  
  r = app_init(&ctx, "timer-baseline-1280x720.h264", "timer-baseline_1280x720_yuv420pUVI.yuv");
  if (r < 0) {
    TRAE("Failed to initialize the application.");
    r = -10;
    goto error;
  }

  r = app_execute(&ctx);
  if (r < 0) {
    TRAE("Failed to execute the application.");
    r = -20;
    goto error;
  }
  
 error:

  r = app_shutdown(&ctx);
  if (r < 0) {
    TRAE("Failed to cleanly shutdown the application.");
    r = -20;
  }

  if (r < 0) {
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}

/* ------------------------------------------------------- */

static int app_init(app* ctx, const char* inputFilename, const char* outputFilename) {

  int r = 0;

  /* -------------------------------------- */
  /* Validate                               */
  /* -------------------------------------- */
  if (NULL == ctx) {
    TRAE("Cannot init the application as it's NULL.");
    r = -10;
    goto error;
  }

  if (NULL == inputFilename) {
    TRAE("Cannot init the application as the `intputFilename` is NULL.");
    r = -20;
    goto error;
  }

  if (NULL == outputFilename) {
    TRAE("Cannot init the application as the `outputFilename` is NULL.");
    r = -30;
    goto error;
  }

  /* -------------------------------------- */
  /* Setup FFmpeg helpers                   */
  /* -------------------------------------- */
  r = avformat_open_input(&ctx->fmt_ctx, inputFilename, NULL, NULL);
  if (r < 0) {
    TRAE("Failed to open the input file: `%s`.", inputFilename);
    r = -30;
    goto error;
  }

  ctx->pkt = av_packet_alloc();
  if (NULL == ctx->pkt) {
    TRAE("Failed to allocate a `AVPacket`.");
    r = -40;
    goto error;
  }

  /* -------------------------------------- */
  /* Create Trameleon Application           */
  /* -------------------------------------- */
  ctx->easy_cfg.type = TRA_EASY_APPLICATION_TYPE_DECODER;

  r = tra_easy_create(&ctx->easy_cfg, &ctx->easy_ctx);
  if (r < 0) {
    TRAE("Failed to create the easy instance.");
    r = -50;
    goto error;
  }
  
  tra_easy_set_opt(ctx->easy_ctx, TRA_EOPT_INPUT_SIZE, 1280, 720);
  tra_easy_set_opt(ctx->easy_ctx, TRA_EOPT_OUTPUT_TYPE, TRA_MEMORY_TYPE_IMAGE);
  tra_easy_set_opt(ctx->easy_ctx, TRA_EOPT_DECODED_CALLBACK, app_on_decoded);
  tra_easy_set_opt(ctx->easy_ctx, TRA_EOPT_DECODED_USER, ctx);

  r = tra_easy_init(ctx->easy_ctx);
  if (r < 0) {
    TRAE("Failed to initialize the easy decoder.");
    r = -60;
    goto error;
  }

  /* -------------------------------------- */
  /* Output file                            */
  /* -------------------------------------- */
  ctx->output_fp = fopen(outputFilename, "wb");
  if (NULL == ctx->output_fp) {
    TRAE("Failed to open the output file: `%s`.", outputFilename);
    r = -70;
    goto error;
  }

 error:
  
  if (r < 0) {
    if (NULL != ctx) {
      app_shutdown(ctx);
    }
  }

  return r;
}

/* ------------------------------------------------------- */

static int app_shutdown(app* ctx) {

  int result = 0;
  int r = 0;

  if (NULL == ctx) {
    TRAE("Cannot shutdown the application as it's NULL.");
    return -10;
  }

  if (NULL != ctx->fmt_ctx) {
    avformat_close_input(&ctx->fmt_ctx);
  }

  if (NULL != ctx->pkt) {
    av_packet_free(&ctx->pkt);
  }

  if (NULL != ctx->easy_ctx) {
    r = tra_easy_destroy(ctx->easy_ctx);
    if (r < 0) {
      TRAE("Failed to cleanly destroy the easy handle.");
    }
  }

  if (NULL != ctx->output_fp) {
    fclose(ctx->output_fp);
  }

  ctx->easy_ctx = NULL;
  ctx->fmt_ctx = NULL;
  ctx->pkt = NULL;
  ctx->output_fp = NULL;
  ctx->output_written = 0;

  return result;
}

/* ------------------------------------------------------- */

static int app_execute(app* ctx) {

  tra_memory_h264 mem = { 0 };
  int r = 0;
  
  if (NULL == ctx) {
    TRAE("Cannot execute the application as it's NULL.");
    r = -10;
    goto error;
  }

  if (NULL == ctx->fmt_ctx) {
    TRAE("Cannot execute the application as the `AVFormatContext` is NULL.");
    r = -20;
    goto error;
  }

  if (NULL == ctx->pkt) {
    TRAE("Cannot execute the application as the `AVPacket*` is NULL.");
    r = -30;
    goto error;
  }

  if (NULL == ctx->easy_ctx) {
    TRAE("Cannot execute the application as the `easy_ctx` is NULL.");
    r = -40;
    goto error;
  }

  while (av_read_frame(ctx->fmt_ctx, ctx->pkt) >= 0) {

    mem.data = ctx->pkt->data;
    mem.size = ctx->pkt->size;

    r = tra_easy_decode(ctx->easy_ctx, TRA_MEMORY_TYPE_H264, &mem);
    if (r < 0) {
      TRAE("Failed to decode.");
      r = -50;
      goto error;
    }
  }
  
 error:
  return r;
}

/* ------------------------------------------------------- */

static int app_on_decoded(uint32_t type, void* data, void* user) {

  tra_memory_image* mem = NULL;
  app* ctx = NULL;
  uint32_t dx = 0;
  uint32_t j = 0;
  int result = 0;
  int r = 0;

  if (TRA_MEMORY_TYPE_IMAGE != type) {
    TRAE("Decoded H264, but the output type is not `TRA_MEMORY_TYPE_IMAGE`. We only support the _IMAGE type for now.");
    r = -10;
    goto error;
  }

  ctx = (app*) user;
  if (NULL == ctx) {
    TRAE("Decoded H264, but cannot store the output as the received user ptr is NULL.");
    r = -30;
    goto error;
  }

  /* Already created the output file? We only store the first decoded image. */
  if (1 == ctx->output_written) {
    r = 0;
    goto error;
  }

  mem = (tra_memory_image*) data;
  if (NULL == mem) {
    TRAE("Deecoded H264, but the received image is NULL.");
    r = -20;
    goto error;
  }

  tra_memoryimage_print(mem);

  if (NULL == mem->plane_data[0]) {
    TRAE("Decoded H264, but the luma plane is NULL.");
    r = -30;
    goto error;
  }

  if (NULL == mem->plane_data[1]) {
    TRAE("Decoded H264, but the chroma plane is NULL.");
    r = -40;
    goto error;
  }
  
  /* Write Y-plane */
  for (j = 0; j < mem->plane_heights[0]; ++j) {

    result = fwrite(
      mem->plane_data[0] + (j * mem->plane_strides[0]),
      mem->image_width,
      1,
      ctx->output_fp
    );

    if (1 != result) {
      TRAE("Failed to write a line of Y-data.");
      r = -40;
      goto error;
    }
  }

  /* Write UV plane */
  for (j = 0; j < mem->plane_heights[1]; ++j) {

    result = fwrite(
      mem->plane_data[1] + (j * mem->plane_strides[1]),
      mem->image_width,
      1,
      ctx->output_fp
    );

     if (1 != result) {
       TRAE("Failed to write a line of UV data.");
       r = -50;
       goto error;
     }
  }

  fclose(ctx->output_fp);
  ctx->output_fp = NULL;
  ctx->output_written = 1;
  
  r = 0;
  
 error:
  return r;
}

/* ------------------------------------------------------- */

