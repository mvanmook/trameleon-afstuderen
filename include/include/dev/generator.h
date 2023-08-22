#ifndef DEV_GENERATOR_H
#define DEV_GENERATOR_H

/*
  
  GENERATOR
  ==========

  GENERAL INFO:

    After implementing several FFmpeg pipelines I decided to
    write this helper. This `generator` will generate a h264
    stream that you can use to test encoders.
  
*/

/* ------------------------------------------------------- */

typedef struct dev_generator dev_generator;
typedef struct dev_generator_settings dev_generator_settings;

/* ------------------------------------------------------- */

struct dev_generator_settings {

  /* General */
  uint32_t image_width;
  uint32_t image_height;
  uint32_t duration; /* Number of seconds that we should generate video for. */
  void* user;
  
  /* Callbacks */
  int (*on_frame)(uint32_t type, void* data, void* user);        /* Get's called when we've got a buffer with NV12 data. */
  int (*on_encoded_data)(uint32_t type, void* data, void* user); /* Get's called when we've got a buffer with H264 Annex-B */
};

/* ------------------------------------------------------- */

int dev_generator_create(dev_generator_settings* cfg, dev_generator** ctx);
int dev_generator_destroy(dev_generator* ctx);
int dev_generator_update(dev_generator* ctx); /* Generates H264. */
int dev_generator_is_ready(dev_generator* ctx); /* Returns 1 when ready, 0 when not ready and < 0 in case of an error. */

/* ------------------------------------------------------- */

#endif
