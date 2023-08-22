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

  EASY TRANSCODER DEVELOPMENT TEST
  =================================

  GENERAL INFO:

    I've created this test while implementing the easy
    transcoder. See the `tra/modules/easy/easy-transcoder.c` file
    for the actual imlementation and additional documentation.

    Here we create the easy transcoder instance and then create a
    transcode list. This transcode list holds the different
    outputs into which we want to transcode.

 */

/* ------------------------------------------------------- */

#include <stdlib.h>
#include <tra/easy.h>
#include <tra/log.h>

/* ------------------------------------------------------- */

typedef struct app app;

/* ------------------------------------------------------- */

struct app {
  tra_transcode_list_settings list_cfg;
  tra_transcode_list* list_ctx;
  tra_easy_settings easy_cfg;
  tra_easy* easy_ctx;
};

/* ------------------------------------------------------- */

static int app_init(app* ctx, const char* inputFilename);
static int app_execute(app* ctx);
static int app_shutdown(app* ctx);
static int app_on_decoded(uint32_t type, void* data, void* user);

/* ------------------------------------------------------- */

int main(int argc, const char* argv[]) {

  app ctx = { 0 };
  int r = 0;
  
  TRAD("Easy Transcoder");

  r = app_init(&ctx, "timer-baseline-1280x720.h264");
  if (r < 0) {
    TRAE("Failed to initialize the application.");
    r = -10;
    goto error;
  }

 error:

  r = app_shutdown(&ctx);
  if (r < 0) {
    TRAE("Failed to cleanly shutdown the app.");
    r = -30;
  }

  if (r < 0) {
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}

/* ------------------------------------------------------- */

static int app_init(app* ctx, const char* inputFilename) {

  tra_transcode_profile profile = { 0 };
  int r = 0;
  
  if (NULL == ctx) {
    TRAE("Cannot initialize the app as the given app is NULL.");
    r = -10;
    goto error;
  }

  if (NULL == inputFilename) {
    TRAE("Cannot initialize the app as the given `inputFilename` is NULL.");
    r = -20;
    goto error;
  }

  /* ---------------------------------------- */
  /* TRANSCODE LIST                           */
  /* ---------------------------------------- */

  r = tra_transcode_list_create(&ctx->list_cfg, &ctx->list_ctx);
  if (r < 0) {
    TRAE("Cannot initialize the app as we failed to create the transcode list.");
    r = -30;
    goto error;
  }

  /* Add transcoding profile */
  profile.width = 640;
  profile.height = 480;
  profile.bitrate = 700;

  r = tra_transcode_list_add_profile(ctx->list_ctx, &profile);
  if (r < 0) {
    TRAE("Failed to add the transcode profile. (1)");
    r = -40;
    goto error;
  }

  /* Add transcoding profile */
  profile.width = 1024;
  profile.height = 768;
  profile.bitrate = 700;

  r = tra_transcode_list_add_profile(ctx->list_ctx, &profile);
  if (r < 0) {
    TRAE("Failed to add the transcode profile. (2)");
    r = -50;
    goto error;
  }

  tra_transcode_list_print(ctx->list_ctx);

  /* ---------------------------------------- */
  /* EASY CONTEXT                             */
  /* ---------------------------------------- */
  
  /* Create and configure the easy context. */
  ctx->easy_cfg.type = TRA_EASY_APPLICATION_TYPE_TRANSCODER;
  
  r = tra_easy_create(&ctx->easy_cfg, &ctx->easy_ctx);
  if (r < 0) {
    TRAE("Cannot initialize the app as we failed to create the easy context.");
    r = -30;
    goto error;
  }

  tra_easy_set_opt(ctx->easy_ctx, TRA_EOPT_INPUT_SIZE, 1280, 720);
  tra_easy_set_opt(ctx->easy_ctx, TRA_EOPT_DECODED_CALLBACK, app_on_decoded);
  tra_easy_set_opt(ctx->easy_ctx, TRA_EOPT_DECODED_USER, ctx);

 error:

  if (r < 0) {
    if (NULL != ctx) {
      app_shutdown(ctx);
    }
  }
  
  return r;
}

/* ------------------------------------------------------- */
  
static int app_execute(app* ctx) {

  int r = 0;

  if (NULL == ctx) {
    TRAE("Cannot execute the app as it's NULL.");
    r = -10;
    goto error;
  }

 error:
  return r;
}

/* ------------------------------------------------------- */

static int app_shutdown(app* ctx) {

  int result = 0;
  int r = 0;

  if (NULL != ctx->list_ctx) {
    r = tra_transcode_list_destroy(ctx->list_ctx);
    if (r < 0) {
      TRAE("Failed to cleanly destroy the transcode list.");
      result -= 10;
    }
  }

  ctx->list_ctx = NULL;

 error:
  return result;
}

/* ------------------------------------------------------- */

static int app_on_decoded(uint32_t type, void* data, void* user) {
  
  int r = 0;

  return r;
}

/* ------------------------------------------------------- */
