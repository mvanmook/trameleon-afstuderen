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


  EASY ENCODER
  ============

  GENERAL INFO:

    I've created this test while implementing the easy layer for
    Trameleon. You can read more about this easy layer in
    `easy.h`, but in short: the easy layer provides a couple of
    applications which hide certain required and boiler plate
    code. This hides things such as creating a CUDA context for
    an NVIDIA based encoder. The goal of the easy layer, is to
    make it as easy as possible to use an encoder, decoder,
    transcoder, etc When you want more control over the process
    you can always use the use the module API directly.
  
 */
/* ------------------------------------------------------- */

#include <stdlib.h>
#include <stdio.h>
#include <tra/easy.h>
#include <tra/log.h>

/* ------------------------------------------------------- */

static int on_encoded(uint32_t type, void* data, void* user);
static int on_flushed(void* user);

/* ------------------------------------------------------- */

int main(int argc, const char* argv[]) {

  tra_easy_settings ez_cfg = { 0 };
  tra_memory_image mem = { 0 };
  tra_sample sample = { 0 };
  tra_easy* ez_ctx = NULL;

  uint32_t image_height = 720;
  uint32_t image_width = 1280;
  char* image_data = NULL;
  uint32_t i = 0;
  int r = 0;

  TRAD("Easy Encoder");
  
  /* Allocate a test buffer */
  image_data = calloc(1, (image_width * image_height) + (image_width * (image_height / 2)));
  if (NULL == image_data) {
    TRAE("Failed to allocate the test image buffer.");
    r = -20;
    goto error;
  }

  mem.image_format = TRA_IMAGE_FORMAT_NV12;
  mem.image_width = image_width;
  mem.image_height = image_height;
  mem.plane_strides[0] = image_width;
  mem.plane_strides[1] = image_width;
  mem.plane_strides[2] = 0;
  mem.plane_heights[0] = image_height;
  mem.plane_heights[1] = image_height / 2;
  mem.plane_heights[2] = 0;
  mem.plane_data[0] = image_data;
  mem.plane_data[1] = image_data + (image_width * image_height);
  mem.plane_data[2] = NULL;
  mem.plane_count = 2;

  /* Create the easy encoder application. */
  ez_cfg.type = TRA_EASY_APPLICATION_TYPE_ENCODER;

  r = tra_easy_create(&ez_cfg, &ez_ctx);
  if (r < 0) {
    TRAE("Failed to create the application instance.");
    r = -10;
    goto error;
  }

  /* Configure the encoder */
  tra_easy_set_opt(ez_ctx, TRA_EOPT_INPUT_SIZE, mem.image_width, mem.image_height);
  tra_easy_set_opt(ez_ctx, TRA_EOPT_INPUT_FORMAT, mem.image_format);
  tra_easy_set_opt(ez_ctx, TRA_EOPT_FPS, 25, 1);
  tra_easy_set_opt(ez_ctx, TRA_EOPT_ENCODED_CALLBACK, on_encoded);
  tra_easy_set_opt(ez_ctx, TRA_EOPT_FLUSHED_CALLBACK, on_flushed);

  /* Once we have configured the encoder we can initialize it! */
  r = tra_easy_init(ez_ctx);
  if (r < 0) {
    TRAE("Failed to initialize the easy encoder.");
    r = -30;
    goto error;
  }

  for (i = 0; i < 40; ++i) {
    r = tra_easy_encode(ez_ctx, &sample, TRA_MEMORY_TYPE_IMAGE, &mem);
    if (r < 0) {
      TRAE("Failed to encode an image.");
      r = -30;
      goto error;
    }
  }

  r = tra_easy_flush(ez_ctx);
  if (r < 0) {
    TRAE("Failed to cleanly flush the easy encoder.");
    r = -50;
    goto error;
  }

 error:

  if (NULL != image_data) {
    free(image_data);
    image_data = NULL;
  }

  if (NULL != ez_ctx) {
    r = tra_easy_destroy(ez_ctx);
    if (r < 0) {
      TRAE("Failed to cleanly destroy the `tra_easy` handle.");
    }
  }

  if (r < 0) {
    return EXIT_FAILURE;
  }
  
  return EXIT_SUCCESS;
}

/* ------------------------------------------------------- */

static int on_encoded(uint32_t type, void* data, void* user) {

  static FILE* fp = NULL;
  tra_memory_h264* mem = NULL;
  int r = 0;

  if (NULL == fp) {
    fp = fopen("./test-easy-encoder.h264", "wb");
    if (NULL == fp) {
      TRAE("Failed to open the output file for the encoded data. (exiting).");
      exit(EXIT_FAILURE);
    }
  }

  if (NULL == data) {
    TRAE("The encoded data is NULL. (exiting).");
    exit(EXIT_FAILURE);
  }

  if (TRA_MEMORY_TYPE_H264 != type) {
    TRAE("Cannot handle encoded data; we expect H264. (exiting).");
    exit(EXIT_FAILURE);
  }

  mem = (tra_memory_h264*) data;
  if (NULL == mem->data) {
    TRAE("Cannot handle encoded `tra_memory_h264` as it's NULL. (exiting).");
    exit(EXIT_FAILURE);
  }

  if (0 == mem->size) {
    TRAE("Cannot handle encoded `tra_memory_h254` as it's size is 0. (exiting).");
    exit(EXIT_FAILURE);
  }

  r = fwrite(mem->data, mem->size, 1, fp);
  if (1 != r) {
    TRAE("Failed to write the encoded data. (exiting).");
    exit(EXIT_FAILURE);
  }

  r = 0;
  
  TRAD("Got encoded data.");
  return r;
}

/* ------------------------------------------------------- */

static int on_flushed(void* user) {
  int r = 0;
  return r;
}

/* ------------------------------------------------------- */
