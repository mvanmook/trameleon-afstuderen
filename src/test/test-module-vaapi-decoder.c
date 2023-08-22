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

  VAAPI DECODER
  =============

  GENERAL INFO:

    This file creates an instance of `vaapidec` and decodes some
    h264. This file is mainly used during the development of the
    vaapi module.

*/

/* ------------------------------------------------------- */

#include <stdlib.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>

#include <tra/registry.h>
#include <tra/module.h>
#include <tra/types.h>
#include <tra/core.h>
#include <tra/dict.h>
#include <tra/log.h>

/* ------------------------------------------------------- */

static int on_decoded_data(uint32_t type, void* data, void* user);

/* ------------------------------------------------------- */

int main(int argc, const char* argv[]) {

  const char* input_filename = "./test-module-vaapi-encoder.h264";
  AVFormatContext* fmt_ctx = NULL;
  AVPacket* pkt = NULL;
  int r = 0;

  tra_core_settings core_cfg = { };
  tra_core* core = NULL;

  tra_memory_h264 host_mem = { 0 };
  tra_decoder_settings dec_opt = { 0 };
  tra_decoder* dec = NULL;

  TRAD("VAAPI Decoder Test");

  /* ------------------------------------------------------- */
  /* Create format context to parse input file.              */
  /* ------------------------------------------------------- */

  r = avformat_open_input(&fmt_ctx, input_filename, NULL, NULL);
  if (r < 0) {
    TRAE("Failed to open `%s`.", input_filename);
    r = -10;
    goto error;
  }

  pkt = av_packet_alloc();
  if (NULL == pkt) {
    TRAE("Failed to allocate the AVPacket.");
    r = -20;
    goto error;
  }

  av_dump_format(fmt_ctx, 0, input_filename, 0);

  /* ------------------------------------------------------- */
  /* Create the decoder instance                             */
  /* ------------------------------------------------------- */
  
  r = tra_core_create(&core_cfg, &core);
  if (r < 0) {
    r = -30;
    goto error;
  }

  dec_opt.image_width = 1280;
  dec_opt.image_height = 720;
  dec_opt.callbacks.on_decoded_data = on_decoded_data;
  dec_opt.callbacks.user = NULL;

  r = tra_core_decoder_create(core, "vaapidec", &dec_opt, NULL, &dec);
  if (r < 0) {
    r = -40;
    goto error;
  }

  /* ------------------------------------------------------- */
  /* Decode                                                  */
  /* ------------------------------------------------------- */

  TRAI("Start decoding");
  
  while (av_read_frame(fmt_ctx, pkt) >= 0) {
        
    host_mem.data = pkt->data;
    host_mem.size = pkt->size;
    
    r = tra_decoder_decode(dec, TRA_MEMORY_TYPE_H264, &host_mem);
    if (r < 0) {
      r = -50;
      goto error;
    }
  }
  
 error:

   if (NULL != fmt_ctx) {
    avformat_close_input(&fmt_ctx);
    fmt_ctx = NULL;
  }

  if (NULL != pkt) {
    av_packet_free(&pkt);
    pkt = NULL;
  }

  if (NULL != dec) {
    tra_decoder_destroy(dec);
    dec = NULL;
  }

  if (NULL != core) {
    tra_core_destroy(core);
    core = NULL;
  }
  
  if (r < 0) {
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}

/* ------------------------------------------------------- */

static int on_decoded_data(uint32_t type, void* data, void* user) {

  char filename[128] = { 0 };
  static uint8_t did_create_file = 0;
  static uint16_t count = 0;
  uint32_t plane_stride = 0;
  uint8_t* plane_ptr = NULL;
  tra_memory_image* img = NULL;
  uint32_t half_h = 0;
  uint32_t i = 0;
  uint32_t j = 0;
  FILE* fp = NULL;
  int r = 0;
  
  if (TRA_MEMORY_TYPE_IMAGE != type) {
    TRAE("Received decoded data that we can't handle. We can only handle `TRA_MEMORY_TYPE_IMAGE`.");
    r = -10;
    goto error;
  }

  img = (tra_memory_image*)data;
  
  TRAD("Received decoded data.");
  
  tra_memoryimage_print(img);

  if (0 != did_create_file) {
    r = 0;
    goto error;
  }
  
  if (count > 10) {
    r = 0;
    goto error;
  }

  if (TRA_IMAGE_FORMAT_NV12 != img->image_format) {
    TRAE("Received decoded data, but we're not writing it to a file as we only support NV12, we received: %s", tra_imageformat_to_string(img->image_format));
    r = -20;
    goto error;
  }

  r = snprintf(
    filename,
    sizeof(filename),
    "test-module-vaapi-decoder-image-%s-%04u.yuv",
    tra_imageformat_to_string(img->image_format),
    count
  );

  if (r < 0) {
    TRAE("Failed to generate a unique filename.");
    r = -40;
    goto error;
  }

  fp = fopen(filename, "wb");
  if (NULL == fp) {
    TRAE("Failed to open the output file to store a decoded frame.");
    r = -50;
    goto error;
  }

  plane_ptr = img->plane_data[0];
  plane_stride = img->plane_strides[0];
  
  for (j = 0; j < img->image_height; ++j) {

    r = fwrite(plane_ptr, plane_stride, 1, fp);
    if (1 != r) {
      TRAE("Failed to write some data of the Y-plane to a file.");
      r = -60;
      goto error;
    }
    
    plane_ptr += plane_stride;
  }

  half_h = img->image_height / 2;
  plane_ptr = img->plane_data[1];
  plane_stride = img->plane_strides[1];
  
  for (j = 0; j < half_h; ++j) {

    r = fwrite(plane_ptr, plane_stride, 1, fp);
    if (1 != r) {
      TRAE("Failed to write some data of the UV-plane to a file.");
      r = -70;
      goto error;
    }
    
    plane_ptr += plane_stride;
  }

  did_create_file = 1;
  count++;

 error:

  if (NULL != fp) {
    fclose(fp);
    fp = NULL;
  }
  
  return 0;
}

/* ------------------------------------------------------- */
