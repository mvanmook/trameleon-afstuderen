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

  NVIDIA + OPENGL INTEROP
  ========================

  GENERAL INFO:

    After experimenting with NVDEC/CUDA and OpenGL interop in
    [this][0] test, I created this research code that
    demonstrates how to use the OpenGL interop with
    CUDA/NVIDIA. This test will generate H264 data using
    FFmpeg. Then it feeds the H264 into a NVIDIA based decoder.
    When we receive an decoded frame, which we keep on the GPU
    (in device memory), we copy the pixels into two OpenGL
    textures. This is done via the OpenGL graphics interop.

  REFERENCES:

    [0]: https://gist.github.com/roxlu/bd5c9b140281cd2de90f0efc8ec3116a "test-opengl.c"
    
*/

/* ------------------------------------------------------- */

#include <stdlib.h>
#include <stdio.h>
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <cuda.h>
#include <cudaGL.h>
#include <tra/modules/nvidia/nvidia-cuda.h>
#include <tra/modules/nvidia/nvidia-dec.h>
#include <tra/modules/nvidia/nvidia-converter.h>
#include <tra/modules/nvidia/nvidia.h>
#include <tra/modules/opengl/opengl-gfx.h>
#include <tra/modules/opengl/opengl-api.h>
#include <tra/modules/opengl/opengl-interop-cuda.h>
#include <tra/modules/opengl/opengl.h>
#include <tra/profiler.h>
#include <tra/module.h>
#include <tra/types.h>
#include <tra/core.h>
#include <tra/log.h>
#include <dev/generator.h>
#include <rxtx/trace.h>
#include <rxtx/log.h>

/* ------------------------------------------------------- */

static void on_key(GLFWwindow* win, int k, int s, int action, int mods);
static int on_encoded_data(uint32_t type, void* data, void* user);       /* Gets called by the generator when it has some h264. */
static int on_decoded_data(uint32_t type, void* data, void* user);       /* Gets called when the nvidia decoder has decoded data. */
static int on_uploaded(uint32_t type, void* data, void* user);           /* Gets called when we've uploaded decoded data using our interop module. */
static int on_converted(uint32_t type, void* data, void* user);          /* Gets called whenever we've converted some data. */

/* ------------------------------------------------------- */

typedef struct app app;

/* ------------------------------------------------------- */

struct app {

  /* General */
  uint32_t image_width;
  uint32_t image_height;

  /* H264 Generator */
  dev_generator_settings gen_cfg;
  dev_generator* gen;
  uint32_t is_ready; 

  /* Core */
  tra_core_settings core_settings;
  tra_core* core;

  /* Decoder */
  tra_nvdec_settings nv_settings;
  tra_decoder_callbacks dec_callbacks;
  tra_decoder_settings dec_settings;
  tra_decoder* dec;

  /* CUDA for decoder. */
  tra_cuda_context* cu_ctx;
  tra_cuda_device* cu_dev;
  tra_cuda_api* cu_api;

  /* Experimental: OpenGL graphics interop. */
  gl_gfx_settings gl_settings;
  tra_opengl_api* gl_api;
  tra_graphics_settings gfx_settings;
  tra_graphics* gfx;

  gl_cu_settings cu_settings;
  tra_interop_settings interop_settings;
  tra_interop* interop;

  /* Experimental: converter */
  tra_converter_settings converter_settings;
  tra_converter* converter;
};

static int app_init(app* ctx);
static int app_update(app* ctx);
static int app_shutdown(app* ctx);

/* ------------------------------------------------------- */

int main(int argc, const char* argv[]) {

  tra_profiler_settings prof_cfg = { 0 };
  GLFWwindow* win = NULL;
  int width = 1280;
  int height = 720;
  app app = { 0 };
  int r = 0;

  rx_log_start("profiler-nvidia-graphics", 1024, 1024 * 1024, argc, argv);
  rx_trace_start("profiler-nvidia-graphics.json", 1024 * 1024);

  TRAI("OpenGL Development Test");

  /* Setup profiler */
  prof_cfg.timer_begin = rx_trace_timer_begin;
  prof_cfg.timer_end = rx_trace_timer_end;
  tra_profiler_start(&prof_cfg);
  
  /* ----------------------------------------------- */
  /* Create window.                                  */
  /* ----------------------------------------------- */

  r = glfwInit();
  if (0 == r) {
    TRAE("Failed to initialize GLFW.");
    r = -10;
    goto error;
  }

  glfwWindowHint(GLFW_DEPTH_BITS, 16);
  glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_FALSE);
  glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  
  win = glfwCreateWindow(width, height, "Trameleon", NULL, NULL );
  if (NULL == win) {
    TRAE("Failed to create the window.");
    r = -20;
    goto error;
  }

  glfwSetKeyCallback(win, on_key);
  glfwMakeContextCurrent(win);
  glfwSwapInterval(1);

  r = gladLoadGL(glfwGetProcAddress);
  if (0 == r) {
    TRAE("Failed to load the GL functions.");
    r = -30;
    goto error;
  }

  /* Create the app. */
  app.image_width = width;
  app.image_height = height;

  r = app_init(&app);
  if (r < 0) {
    TRAE("Failed to initialize the app.");
    r = -40;
    goto error;
  }

  while (0 == glfwWindowShouldClose(win)) {

    /* While not ready, update; triggers callbacks on this thread. */
    if (0 == app.is_ready) {
      
      r = app_update(&app);
      if (r < 0) {
        glfwSetWindowShouldClose(win, GLFW_TRUE);
        continue;
      }

      glfwSwapBuffers(win);
    }

    glfwPollEvents();
  }

 error:

  tra_profiler_stop();
  rx_log_stop();
  rx_trace_stop();

  r = app_shutdown(&app);

  if (r < 0) {
    return EXIT_FAILURE;
  }
  
  return EXIT_SUCCESS;
}

/* ------------------------------------------------------- */
 
static void on_key(
  GLFWwindow* win,
  int k,
  int s,
  int action,
  int mods
)
{
  if (action != GLFW_PRESS) {
    return;
  }

  switch (k) {
    case GLFW_KEY_ESCAPE: {
      glfwSetWindowShouldClose(win, GLFW_TRUE);
      break;
    }
  }
}

/* ------------------------------------------------------- */

static int app_init(app* ctx) {

  int result = 0;
  int r = 0;

  /* ----------------------------------------------- */
  /* Validate                                        */
  /* ----------------------------------------------- */
 
  if (NULL == ctx) {
    TRAE("Cannot intialize as the given app is NULL.");
    r = -10;
    goto error;
  }

  if (0 == ctx->image_width) {
    TRAE("`image_width` not set.");
    r = -20;
    goto error;
  }

  if (0 == ctx->image_height) {
    TRAE("`image_height` not set.");
    r = -30;
    goto error;
  }

  /* ----------------------------------------------- */
  /* Trameleon + CUDA                                */
  /* ----------------------------------------------- */
 
  r = tra_core_create(&ctx->core_settings, &ctx->core);
  if (r < 0) {
    TRAE("Failed to create the core context.");
    r = -10;
    goto error;
  }

  /* Create the cuda context */
  r = tra_core_api_get(ctx->core, "cuda", (void**) &ctx->cu_api);
  if (r < 0) {
    TRAE("Failed to get the CUDA api.");
    r = -20;
    goto error;
  }
 
  r = ctx->cu_api->init();
  if (r < 0) {
    TRAE("Failed to initialize the CUDA api.");
    r = -30;
    goto error;
  }

  ctx->cu_api->device_list();

  /* Use the default device. */
  r = ctx->cu_api->device_create(0, &ctx->cu_dev);
  if (r < 0) {
    TRAE("Failed to create a cuda device.");
    r = -40;
    goto error;
  }

  r = ctx->cu_api->context_create(ctx->cu_dev, 0, &ctx->cu_ctx);
  if (r < 0) {
    TRAE("Failed to create the cuda context.");
    r = -50;
    goto error;
  }

  r = ctx->cu_api->device_get_handle(ctx->cu_dev, &ctx->nv_settings.cuda_device_handle);
  if (r < 0) {
    TRAE("Failed to get the cuda device handle.");
    r = -87;
    goto error;
  }

  r = ctx->cu_api->context_get_handle(ctx->cu_ctx, &ctx->nv_settings.cuda_context_handle);
  if (r < 0) {
    TRAE("Failed to get the cuda context handle.");
    r = -88;
    goto error;
  }

  /* ----------------------------------------------- */
  /* Decoder                                         */
  /* ----------------------------------------------- */

  /* Create the nvidia decoder context. */
  ctx->dec_settings.callbacks.on_decoded_data = on_decoded_data;
  ctx->dec_settings.callbacks.user = ctx;
  ctx->dec_settings.image_width = ctx->image_width;
  ctx->dec_settings.image_height = ctx->image_height;
  ctx->dec_settings.output_type = TRA_MEMORY_TYPE_CUDA;

  /* @todo currently we're not able to set the required output format; in this test, this MUST be NV12 otherwise the `opengl` interop won't work. */
  r = tra_core_decoder_create(ctx->core, "nvdec", &ctx->dec_settings, &ctx->nv_settings, &ctx->dec);
  if (r < 0) {
    TRAE("Failed to create `nvdec`.");
    r = -83;
    goto error;
  }

  /* ----------------------------------------------- */
  /* Graphics interop.                               */
  /* ----------------------------------------------- */

  /* Load the GL API plugin that the graphics interop uses. */
  r = tra_core_api_get(ctx->core, "opengl", (void**) &ctx->gl_api);
  if (r < 0) {
    TRAE("Failed to load the `opengl` API.");
    r = -90;
    goto error;
  }

  ctx->gfx_settings.image_width = ctx->image_width;
  ctx->gfx_settings.image_height = ctx->image_height;
  ctx->gfx_settings.image_format = TRA_IMAGE_FORMAT_NV12;
  ctx->gl_settings.gl_api = ctx->gl_api;

  ctx->interop_settings.image_width = ctx->image_width;
  ctx->interop_settings.image_height = ctx->image_height;
  ctx->interop_settings.image_format = TRA_IMAGE_FORMAT_NV12;
  ctx->interop_settings.callbacks.on_uploaded = on_uploaded;
  ctx->interop_settings.callbacks.user = ctx;
  ctx->cu_settings.gl_api = ctx->gl_api;

  /* @todo currently we're not setting the image format for the decoder! Below we assume it's NV12. */
  r = tra_core_graphics_create(ctx->core, "opengl-gfx", &ctx->gfx_settings, &ctx->gl_settings, &ctx->gfx);
  if (r < 0) {
    TRAE("Failed to create the `opengl` graphics module.");
    r = -90;
    goto error;
  }

  /* Create the OpenGL <> CUDA interop */
  r = tra_core_interop_create(ctx->core, "opengl-interop-cuda", &ctx->interop_settings, &ctx->cu_settings, &ctx->interop);
  if (r < 0) {
    TRAE("Failed to create the `opengl-cuda` interop module.");
    r = -100;
    goto error;
  }
    
  /* ----------------------------------------------- */
  /* H264 Generator                                  */
  /* ----------------------------------------------- */

  ctx->gen_cfg.image_width = ctx->image_width;
  ctx->gen_cfg.image_height = ctx->image_height;
  ctx->gen_cfg.duration = 30;
  ctx->gen_cfg.on_encoded_data = on_encoded_data;
  ctx->gen_cfg.user = ctx;
  
  r = dev_generator_create(&ctx->gen_cfg, &ctx->gen);
  if (r < 0) {
    TRAE("Failed to create the generator.");
    r = -20;
    goto error;
  }

  /* ----------------------------------------------- */
  /* Converter                                          */
  /* ----------------------------------------------- */

  ctx->converter_settings.callbacks.on_converted = on_converted;
  ctx->converter_settings.callbacks.user = ctx;
  ctx->converter_settings.input_type = TRA_MEMORY_TYPE_CUDA; /* We will pass decoded images into the converter which are still on the GPU. */
  ctx->converter_settings.input_format = TRA_IMAGE_FORMAT_NV12;
  ctx->converter_settings.input_width = ctx->image_width;
  ctx->converter_settings.input_height = ctx->image_height;
  ctx->converter_settings.output_width = ctx->image_width / 2;
  ctx->converter_settings.output_height = ctx->image_height / 2;
  ctx->converter_settings.output_type = TRA_MEMORY_TYPE_CUDA; /* Keep the converted image on GPU. */
  ctx->converter_settings.output_format = TRA_IMAGE_FORMAT_NV12;

  r = tra_core_converter_create(ctx->core, "nvconverter", &ctx->converter_settings, NULL, &ctx->converter);
  if (r < 0) {
    TRAE("Failed to create the converter.");
    r = -120;
    goto error;
  }  
  
 error:
  return r;
}

/* ------------------------------------------------------- */

static int app_shutdown(app* ctx) {

  int result = 0;
  int r = 0;

  if (NULL == ctx) {
    TRAE("Cannot shutdown the app as it's NULL.");
    return -10;
  }

  /* Destroy the generator. */
  if (NULL != ctx->gen) {
    dev_generator_destroy(ctx->gen);
    ctx->gen = NULL;
  }

  /* Destroy Trameleon related contexts. */
  if (NULL != ctx->cu_dev
      && NULL != ctx->cu_api)
    {
      ctx->cu_api->device_destroy(ctx->cu_dev);
      ctx->cu_dev = NULL;
    }

  if (NULL != ctx->cu_ctx
      && NULL != ctx->cu_api)
    {
      ctx->cu_api->context_destroy(ctx->cu_ctx);
      ctx->cu_ctx = NULL;
    }

  if (NULL != ctx->dec) {
    tra_decoder_destroy(ctx->dec);
    ctx->dec = NULL;
  }

  if (NULL != ctx->gfx) {
    tra_graphics_destroy(ctx->gfx);
    ctx->gfx = NULL;
  }

  if (NULL != ctx->converter) {
    tra_converter_destroy(ctx->converter);
    ctx->converter = NULL;
  }
  
  if (NULL != ctx->core) {
    tra_core_destroy(ctx->core);
    ctx->core = NULL;
  }
  
  return result;
}

/* ------------------------------------------------------- */

static int app_update(app* ctx) {

  int r = 0;
  
  if (NULL == ctx) {
    TRAE("Cannot update the app as it's empty.");
    r = -10;
    goto error;
  }

  ctx->is_ready = dev_generator_is_ready(ctx->gen);
  if (1 == ctx->is_ready) {
    r = 0;
    goto error;
  }
                             
  if (ctx->is_ready < 0) {
    TRAE("Failed to check if the generator is ready.");
    r = -20;
    goto error;
  }

  /* This will generate a h264 buffer and a call to `on_encoded_data()`. */
  r = dev_generator_update(ctx->gen);
  if (r < 0) {
    TRAE("Failed to update.");
    r = -30;
    goto error;
  }

 error: 
  return r;
}

/* ------------------------------------------------------- */

static int on_encoded_data(uint32_t type, void* data, void* user) {

  tra_memory_h264* host_mem = NULL;
  app* ctx = NULL;
  int r = 0;
  
  if (NULL == data) {
    TRAE("Cannot handle encoded data; it's NULL.");
    r = -10;
    goto error;
  }

  if (NULL == user) {
    TRAE("Cannot handle encoded data; the `user` pointer is NULL.");
    r = -20;
    goto error;
  }

  if (TRA_MEMORY_TYPE_H264 != type) {
    TRAE("Cannot handle encoded data as we expect it to be store in host memory.");
    r = -30;
    goto error;
  }

  ctx = (app*) user;
  if (NULL == ctx->dec) {
    TRAE("Cannot handle encoded data as the decoder instance is NULL.");
    r = -40;
    goto error;
  }

  host_mem = (tra_memory_h264*) data;
  if (NULL == host_mem->data) {
    TRAE("Cannot handle encoded data as the encoded data is NULL.");
    r = -50;
    goto error;
  }

  if (0 == host_mem->size) {
    TRAE("Cannot handle encoded data as the size is 0.");
    r = -60;
    goto error;
  }

  r = tra_decoder_decode(ctx->dec, TRA_MEMORY_TYPE_H264, host_mem);
  if (r < 0) {
    TRAE("Failed to decode the received h264 data.");
    r = -70;
    goto error;
  }

 error:
  
  return r;
}

/* ------------------------------------------------------- */

/*
  This function gets called whenever the decoder has a frame
  which is ready to be displayed. Here we use the OpenGL graphics
  interop to upload the decoded data.
*/
static int on_decoded_data(uint32_t type, void* data, void* user) {

  app* ctx = NULL;
  int r = 0;

  if (NULL == user) {
    TRAE("Cannot handle the decoded data; the user pointer is NULL. This should be a pointer to our app instance.");
    r = -20;
    goto error;
  }

  ctx = (app*) user;

  if (NULL == ctx->interop) {
    TRAE("Cannot handle the decoded data as our interop is NULL.");
    r = -50;
    goto error;
  }

  r = tra_interop_upload(ctx->interop, type, data);
  if (r < 0) {
    TRAE("Failed to upload the decoded data.");
    r = -60;
    goto error;
  }

  /* Testing the converter */
  r = tra_converter_convert(ctx->converter, type, data);
  if (r < 0) {
    TRAE("Failed to convert the decoded data.");
    r = -70;
    goto error;
  }

 error:
  
  return r;
}

/* ------------------------------------------------------- */

static int on_uploaded(uint32_t type, void* data, void* user) {

  tra_upload_opengl_nv12* up_info = NULL;
  tra_draw_opengl_nv12 draw_info = { 0 } ;
  struct app* ctx = NULL;
  int r = 0;

  if (TRA_UPLOAD_TYPE_OPENGL_NV12 != type) {
    TRAE("Cannot handle the uploaded data as we only handle NV12 data.");
    r = -20;
    goto error;
  }
  
  if (NULL == data) {
    TRAE("Cannot handle the uploaded data as the received `data` is NULL.");
    r = -30;
    goto error;
  }

  if (NULL == user) {
    TRAE("Cannot handle the uploaded data as the received user pointer is NULL.");
    r = -40;
    goto error;
  }

  ctx = (app*) user;
  if (NULL == ctx->gfx) {
    TRAE("Cannot handle the uploaded data as the graphics modules is NULL.");
    r = -50;
    goto error;
  }

  up_info = (tra_upload_opengl_nv12*) data;
  if (0 == up_info->tex_y) {
    TRAE("Cannot handle the uploaded data as the `tex_y` is not set.");
    r = -50;
    goto error;
  }

  if (0 == up_info->tex_uv) {
    TRAE("Cannot handle the uploaded data as the `tex_uv` is not set.");
    r = -60;
    goto error; 
  }

  draw_info.tex_y = up_info->tex_y;
  draw_info.tex_uv = up_info->tex_uv;
  draw_info.x = 0;
  draw_info.y = 0;
  draw_info.width = ctx->image_width;
  draw_info.height = ctx->image_height;

  r = tra_graphics_draw(ctx->gfx, TRA_DRAW_TYPE_OPENGL_NV12, &draw_info);
  if (r < 0) {
    TRAE("failed to draw the uploaded and decoded data.");
    r = -60;
    goto error;
  }

 error:
  return r;
}

/* ------------------------------------------------------- */

static int on_converted(uint32_t type, void* data, void* user) {

  int r = 0;

  TRAD("Got converted data.");

  return r;
}

/* ------------------------------------------------------- */
