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


    TRANSCODER TEST USING WINDOWS MEDIA FOUNDATION
    ==============================================

        GENERAL INFO:

    This file creates a transcoder instance of `mftdec` and `mftenc`and
transcodes h264. This file is mainly used during the development of the
                transcoder.

        */

/* ------------------------------------------------------- */

#include <stdlib.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>

#include <tra/core.h>
#include <tra/dict.h>
#include <tra/log.h>
#include <tra/module.h>
#include <tra/transcoder.h>//todo should we merge transcoder and module
#include <tra/registry.h>
#include <tra/types.h>

/* ------------------------------------------------------- */

static int on_encoded_data(uint32_t type, void *data, void *user);

/* ------------------------------------------------------- */

int main(int argc, const char *argv[]) {

  const char *input_filename = "./test-module-mft-encoder.h264";
  AVFormatContext *fmt_ctx = NULL;
  AVPacket *pkt = NULL;
  FILE* fp = NULL;
  int r = 0;

  tra_core_settings core_cfg = {0};
  tra_core *core = NULL;

  tra_memory_h264 host_mem = {0};
  tra_decoder_callbacks dec_callbacks = {0};
  tra_transcoder_settings trans_opt = {0};
  tra_transcoder *trans = NULL;

  TRAD("Media Foundation Transform Decoder Test");

  fp = fopen("./test-module-mft-encoder.h264", "wb");
  if (NULL == fp) {
    TRAE("Failed to generate the output file.");
    r = -55;
    goto error;
  }

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

  trans_opt.dec_settings->image_width = 1280;
  trans_opt.dec_settings->image_height = 720;
  trans_opt.dec_settings->output_format = TRA_IMAGE_FORMAT_NV12;

  trans_opt.enc_settings->image_width = 1280;
  trans_opt.enc_settings->image_height = 720;
  trans_opt.enc_settings->input_format = TRA_IMAGE_FORMAT_NV12;
  trans_opt.enc_settings->callbacks.on_encoded_data = on_encoded_data;
  trans_opt.enc_settings->callbacks.user = fp;

  r = tra_core_transcoder_create(core, "mftdec", &trans_opt.dec_settings, NULL, "mftenc", &trans_opt.enc_settings, NULL, &trans);
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

    r = tra_combinded_transcoder_transcode(trans, TRA_MEMORY_TYPE_H264, &host_mem);
    if (r < 0) {
      r = -50;
      goto error;
    }

    TRAD("%d", r);
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

  if (NULL != trans) {
    tra_combinded_transcoder_destroy(trans);
    trans = NULL;
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

static int on_encoded_data(uint32_t type, void *data, void *user) {

  tra_memory_h264 *encoded = (tra_memory_h264 *)data;
  FILE *fp = (FILE *)user;
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
    TRAE("Cannot handle encoded data, the `size` is 0. (exiting).");
    exit(EXIT_FAILURE);
  }

  r = fwrite(encoded->data, encoded->size, 1, fp);
  if (1 != r) {
    TRAE("Cannot handle encoded data. Failed to write the encoded data. "
         "(exiting)");
    exit(EXIT_FAILURE);
  }

  TRAD("Got encoded data, type: %u, data: %p, bytes: %p, size: %u", type, data,
       encoded->data, encoded->size);

  return 0;
}