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

  GENERAL INFO:
  
    I've created this test while working on the NVIDIA based
    converter. It uses a FFmpeg based generator which will create
    YUV (NV12) buffers that we convert.
  
 */
/* ------------------------------------------------------- */

#include <stdio.h>
#include <stdlib.h>
#include <tra/modules/nvidia/nvidia-converter.h>
#include <dev/generator.h>
#include <tra/module.h>
#include <tra/types.h>
#include <tra/core.h>
#include <tra/log.h>

/* ------------------------------------------------------- */

typedef struct app app;

/* ------------------------------------------------------- */

struct app {
  uint32_t image_width; /* The width of the image that the generator creates. */
  uint32_t image_height; /* The height of the image that the generator creates. */
  dev_generator_settings gen_cfg;
  dev_generator* gen;
  tra_core_settings core_cfg;
  tra_core* core;
  tra_converter_settings converter_cfg;
  tra_converter* converter;
};

/* ------------------------------------------------------- */

int app_init(app* ctx);
int app_shutdown(app* ctx);

/* ------------------------------------------------------- */

static int on_frame(uint32_t type, void* data, void* user);
static int on_encoded_data(uint32_t type, void* data, void* user);
static int on_converted(uint32_t type, void* data, void* user);

/* ------------------------------------------------------- */

int main(int argc, const char* argv[]) {

  app ctx = { 0 };
  int r = 0;

  TRAI("NVIDIA CONVERTER TEST");

  r = app_init(&ctx);
  if (r < 0) {
    r = -10;
    goto error;
  }

  while (0 == dev_generator_is_ready(ctx.gen)) {
    dev_generator_update(ctx.gen);
  }

  r = app_shutdown(&ctx);
  if (r < 0) {
    r = -20;
    goto error;
  }
  
 error:
  
  if (r < 0) {
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}

/* ------------------------------------------------------- */

int app_init(app* ctx) {

  int r = 0;

  if (NULL == ctx) {
    TRAE("Cannot initialize the `app` as it's NULL.");
    r = -10;
    goto error;
  }

  if (NULL != ctx->core) {
    TRAE("Cannot initialize the `app` as the `core` member has already been created.");
    r = -20;
    goto error;
  }

  ctx->image_width = 1920;
  ctx->image_height = 1080;

  /* Create generator. */
  ctx->gen_cfg.image_width = ctx->image_width;
  ctx->gen_cfg.image_height = ctx->image_height;
  ctx->gen_cfg.duration = 1;
  ctx->gen_cfg.user = ctx;
  ctx->gen_cfg.on_frame = on_frame;
  ctx->gen_cfg.on_encoded_data = on_encoded_data;

  r = dev_generator_create(&ctx->gen_cfg, &ctx->gen);
  if (r < 0) {
    TRAE("Failed to create the generator.");
    r = -30;
    goto error;
  }

  /* Create core. */
  r = tra_core_create(&ctx->core_cfg, &ctx->core);
  if (r < 0) {
    TRAE("Failed to initialize Trameleon.");
    r = -40;
    goto error;
  }

  /* Create converter. */
  ctx->converter_cfg.callbacks.on_converted = on_converted;
  ctx->converter_cfg.callbacks.user = ctx;
  ctx->converter_cfg.input_type = TRA_MEMORY_TYPE_IMAGE;
  ctx->converter_cfg.input_format = TRA_IMAGE_FORMAT_NV12;
  ctx->converter_cfg.input_width = ctx->image_width;
  ctx->converter_cfg.input_height = ctx->image_height;
  ctx->converter_cfg.output_width = ctx->image_width / 2;
  ctx->converter_cfg.output_height = ctx->image_height / 2;
  ctx->converter_cfg.output_type = TRA_MEMORY_TYPE_IMAGE;
  ctx->converter_cfg.output_format = TRA_IMAGE_FORMAT_NV12;

  /*
    We could also have used `output_type =
    TRA_MEMORY_TYPE_DEVICE_IMAGE` if we would have liked to keep
    the converted buffer on GPU; this is most likely what you want
    to do when you're transcoding.
  */
    
  r = tra_core_converter_create(ctx->core, "nvconverter", &ctx->converter_cfg, NULL, &ctx->converter);
  if (r < 0) {
    TRAE("Failed to create the converter.");
    r = -50;
    goto error;
  }

 error:

  return r;
}

/* ------------------------------------------------------- */

int app_shutdown(app* ctx) {

  int result = 0;
  int r = 0;

  if (NULL == ctx) {
    TRAE("Cannot shutdown the app as it's NULL.");
    return -10;
  }

  if (NULL != ctx->converter) {
    r = tra_converter_destroy(ctx->converter);
    if (r < 0) {
      TRAE("Failed to cleanly destroy the converter.");
      result -= 10;
    }
  }

  if (NULL != ctx->core) {
    r = tra_core_destroy(ctx->core);
    if (r < 0) {
      TRAE("Failed to cleanly destroy the core.");
      result -= 20;
    }
  }

  if (NULL != ctx->gen) {
    r = dev_generator_destroy(ctx->gen);
    if (r < 0) {
      TRAE("Failed to cleanly destroy te generator.");
      result -= 30;
    }
  }
  
  ctx->gen = NULL;
  ctx->converter = NULL;
  ctx->core = NULL;
  ctx->image_width = 0;
  ctx->image_height = 0;

  return result;
}

/* ------------------------------------------------------- */

static int on_frame(uint32_t type, void* data, void* user) {

  app* ctx = NULL;
  int r = 0;

  /* Validate */
  if (TRA_MEMORY_TYPE_IMAGE != type) {
    TRAE("Cannot handle the frame as it's not a host image.");
    r = -10;
    goto error;
  }

  if (NULL == data) {
    TRAE("Cannot handle the frame as the data is NULL.");
    r = -20;
    goto error;
  }

  if (NULL == user) {
    TRAE("Cannot handle the frame as the `user` pointer is NULL.");
    r = -30;
    goto error;
  }

  /* Pass the host image to the converter. */
  ctx = (app*) user;
  if (NULL == ctx->converter) {
    TRAE("Cannot handle the frame as our converter is NULL.");
    r = -50;
    goto error;
  }

  r = tra_converter_convert(ctx->converter, type, data);
  if (r < 0) {
    TRAE("Failed to convert.");
    r = -60;
    goto error;
  }
  
 error:
    return r;
}

/* ------------------------------------------------------- */

static int on_encoded_data(uint32_t type, void* data, void* user) {
  
  int r = 0;

 error:
  return r;
}

/* ------------------------------------------------------- */

static int on_converted(uint32_t type, void* data, void* user) {

  static int saved = 0;
  
  tra_memory_image* img = NULL;
  char file_name[512] = { 0 };
  FILE* fp = NULL;
  int r = 0;
  int i = 0;
  int j = 0;
  int h = 0;

  /* Already saved. */
  if (1 == saved) {
    return 0;
  }

  if (TRA_MEMORY_TYPE_IMAGE != type) {
    TRAE("Cannot handle the convert data as it's not a host image.");
    r = -10;
    goto error;
  }

  img = (tra_memory_image*) data;
  if (NULL == img) {
    TRAE("Cannot handle the converted data as it's NULL.");
    r = -20;
    goto error;
  }

  r = snprintf(file_name, sizeof(file_name), "test_module_nvidia_converter_%dx%d_yuv420pUVI.yuv", img->image_width, img->image_height);
  if (r < 0) {
    TRAE("Failed to generate the output filename for the converted image.");
    r = -30;
    goto error;
  }

  fp = fopen(file_name, "wb");
  if (NULL == fp) {
    TRAE("Failed to open the output file: %s.", file_name);
    r = -40;
    goto error;
  }

  /* Write Y-plane */
  h = img->plane_heights[0];
  for (j = 0; j < h; ++j) {
    r = fwrite(img->plane_data[0] + (j * img->plane_strides[0]), img->image_width, 1, fp);
    if (1 != r) {
      TRAE("Failed to write a Y-plane line.");
      r = -50;
      goto error;
    }
  }

  /* Write UV-plane */
  h = img->plane_heights[1];
  for (j = 0; j < h; ++j) {
    r = fwrite(img->plane_data[1] + (j * img->plane_strides[1]), img->image_width, 1, fp);
    if (1 != r) {
      TRAE("Failed to write a UV-plane line.");
      r = -60;
      goto error;
    }
  }

  r = fclose(fp);
  if (0 != r) {
    TRAE("Failed to cleanly close the output file.");
    r = 70;
    goto error;
  }

  TRAI("Saved into `%s`.", file_name);
  
  fp = NULL;
  saved = 1;
  
 error:
  return r;
}

/* ------------------------------------------------------- */
