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

  OPENGL CONVERTER 
  ================

  IMPORTANT:

    This test contains a lot of experimental and temporary code
    which I need to cleanup/remove. E.g. I've created a function
    that downloads a texture and writes it to a file while
    testing the NV12 shader; I still have to remove or document
    this.
    
  GENERAL INFO:

    I've created this file while researching and experimenting
    while implementing the OpenGL converter. The OpenGL converter
    should be able to resize an input texture and/or convert it
    from GL_RGBA to NV12.

  TEST IMAGE:

    We load a RGB24 buffer into a texture which we then
    convert using the OpenGL converter. To create this test image you
    can use ImageMagick:

      convert -size 1920x1080 xc:pink -colorspace RGB -depth 8 pink-1920x1080.rgb
      convert -size 1920x1080 -colorspace RGB -depth 8 desktop-1920x1080.jpg desktop-1920x1080.rgb

      # Create the test image that this test loads.
      convert netscape: -resize 1920x1080! desktop-1920x1080.png
      ffmpeg -i desktop-1920x1080.png -pix_fmt nv12 desktop_converted_with_ffmpeg_1920x1080_yuv420pUVI.yuv

 */

/* ------------------------------------------------------- */

static const char* TEST_VS = ""
    "#version 330\n"
    ""
    "out vec2 v_tex;"
    ""
    "const vec2 pos[] = vec2[4]("
    "  vec2(-1.0, 1.0), "
    "  vec2(-1.0, -1.0), "
    "  vec2(1.0, 1.0), "
    "  vec2(1.0, -1.0)  "
    ");"
    ""
    "const vec2 tex[] = vec2[4]("
    "  vec2(0.0, 1.0), "
    "  vec2(0.0, 0.0), "
    "  vec2(1.0, 1.0), "
    "  vec2(1.0, 0.0) "
    ");"
    ""
    "void main() {"
    "  gl_Position = vec4(pos[gl_VertexID], 0.0, 1.0); "
     " v_tex = tex[gl_VertexID];"
    "}"
  "";

static const char* TEST_FS = ""
  "#version 330\n"
  ""
  "uniform vec2 u_resolution;"
  "uniform float u_time;"
  "in vec2 v_tex;"
  ""
  "layout (location = 0) out vec4 fragcolor;"
  ""
  "void main() { "
  "  fragcolor = vec4(v_tex.r, v_tex.g, 0, 1.0);"

  " vec3 c;"
	" float l,z=u_time;"
	" for(int i=0;i<3;i++) {"
	" 	vec2 uv,p=gl_FragCoord.xy/u_resolution;"
	" 	uv=p;"
	" 	p-=.5;"
	" 	p.x*=u_resolution.x/u_resolution.y;"
	" 	z+=.07;"
	" 	l=length(p);"
	" 	uv+=p/l*(sin(z)+1.)*abs(sin(l*9.-z-z));"
	" 	c[i]=.01/length(mod(uv,1.)-.5);"
	" }"
	" fragcolor=vec4(c/l,u_time);"  
  "}"
  "";

/* ------------------------------------------------------- */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <tra/modules/nvidia/nvidia-cuda.h>
#include <tra/modules/nvidia/nvidia-enc-opengl.h>
#include <tra/modules/nvidia/nvidia-enc.h>
#include <tra/modules/nvidia/nvidia.h>
#include <tra/modules/opengl/opengl-converter.h>
#include <tra/modules/opengl/opengl-api.h>
#include <tra/modules/opengl/opengl.h>
#include <tra/module.h>
#include <tra/types.h>
#include <tra/core.h>
#include <tra/log.h>

/* ------------------------------------------------------- */

typedef struct app {

  /* General */
  uint32_t win_width;       /* Width of the window and framebuffer. */
  uint32_t win_height;      /* Height of the window and framebuffer. */

  /* Core */
  tra_core_settings core_settings;
  tra_core* core;

  /* OpenGL */
  tra_opengl_api* gl_api;
  tra_opengl_converter_settings opengl_converter_settings;
  tra_converter* converter;
  tra_converter_settings converter_settings;
  tra_convert_opengl_rgb24 source_tex;  /* The texture that e.g. holds the scene that we want to encode. This is converted into NV12 using the OpenGL converter. */

  /* We need to create a CUDA context for the encoder. */
  tra_cuda_context* cu_ctx;
  tra_cuda_device* cu_dev;
  tra_cuda_api* cu_api;
  
  /* Encoder */
  tra_nvenc_settings nvenc_settings;
  tra_encoder_settings enc_settings;
  tra_sample enc_sample;
  tra_encoder* enc;

  /* Testing */
  uint32_t test_vs;
  uint32_t test_fs;
  uint32_t test_prog;
  uint32_t test_fbo;
  uint32_t test_tex;
  uint32_t test_vao;
  int32_t loc_resolution; /* Uniform location of the resolution. */
  int32_t loc_time;       /* Uniform location of the time. */
  float time;             /* Time in millis since the start of the app. */
  
  /* Debugging */
  uint32_t test_nv12_tex_id; /* I've converted a JPG to NV12 and loaded that into a texture that I feed into the OpenGL based encoder. I created this while trying to figure out why I got an error (NV_ENC_ERR_RESOURCE_REGISTER_FAILED) while calling `nvEncRegisterResource`. */
  char* test_nv12_buf; /* The NV12 data we load from the test file. */
  FILE* output_fp; /* Used to store the created H264. */
  
} app;

/* ------------------------------------------------------- */

static void on_key(GLFWwindow* win, int k, int s, int action, int mods);
static int on_converted(uint32_t type, void* data, void* user);
static int on_converted_save(app* app, uint32_t type, void* data);
static int on_converted_encode(app* app, uint32_t type, void* data);
static int on_encoded_data(uint32_t type, void* data, void* user);

/* ------------------------------------------------------- */

static int app_init(app* app);
static int app_update(app* app);
static int app_shutdown(app* app);
static int app_load_test_texture(app* app);
static int app_load_test_program(app* app); /* Creates the buffers that we can use to generate some video. */

/* ------------------------------------------------------- */

static int save_nv12_texture(const char* filename, GLuint tex, uint32_t width, uint32_t height);

/* ------------------------------------------------------- */

int main(int argc, const char* argv[]) {

  GLFWwindow* win = NULL;
  app ctx = { 0 };
  int r = 0;
  GLuint tmp_tex = 0;

  ctx.win_width = 1920;
  ctx.win_height = 1080;

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

  /* temp: using a height of 1.5 as this is the output height of the nv12 buffer. */
  win = glfwCreateWindow(ctx.win_width, ctx.win_height * 1.5, "Trameleon", NULL, NULL );
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

  /* Initialize the app */
  r = app_init(&ctx);
  if (r < 0) {
    TRAE("Failed to initialize the app.");
    r = -10;
    goto error;
  }

  r = app_load_test_texture(&ctx);
  if (r < 0) {
    TRAE("Failed to load the test texture.");
    r = -20;
    goto error;
  }

  r = app_load_test_program(&ctx);
  if (r < 0) {
    TRAE("Failed to create the test program.");
    r = -30;
    goto error;
  }
  
  /* ----------------------------------------------- */
  /* Run                                             */
  /* ----------------------------------------------- */

  TRAE("@todo > I'm currently directly passing a TRA_DEVICE_MEMORY_TYPE_OPENGL_TEXTURE_RGB24 into the convert function; this is fine but not following the standard I've used so far; I think the solution I use here is better because it means less casting; just pass the type and data.");

  GLenum bufs[] = { GL_COLOR_ATTACHMENT0 };
  
  while (0 == glfwWindowShouldClose(win)) {

    glViewport(0, 0, ctx.win_width, ctx.win_height);
    glClearColor(0.0f, 1.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    {
      glBindFramebuffer(GL_DRAW_FRAMEBUFFER, ctx.test_fbo);
      glDrawBuffers(1, bufs);
      glViewport(0, 0, ctx.win_width, ctx.win_height);
      glClear(GL_COLOR_BUFFER_BIT);
    
      glUseProgram(ctx.test_prog);
      glUniform1f(ctx.loc_time, ctx.time);
      glBindVertexArray(ctx.test_vao);
      glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindVertexArray(ctx.test_vao);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
      
    ctx.time = ctx.time + (1.0f/ctx.enc_settings.fps_num);

#if 1    
    
    r = app_update(&ctx);
    if (r < 0) {
      glfwSetWindowShouldClose(win, GLFW_TRUE);
      continue;
    }

    glBindTexture(GL_TEXTURE_2D, tmp_tex);
    
#if 1
    r = tra_converter_convert(
      ctx.converter,
      TRA_CONVERT_TYPE_OPENGL_RGB24,
 //     &ctx.source_tex
      &ctx.test_tex
    );
#endif    
#endif

    usleep((1.0f/ctx.enc_settings.fps_num) * 1e6);
    
    glfwSwapBuffers(win);
    glfwPollEvents();
  }

 error:

  r = app_shutdown(&ctx);

  if (r < 0) {
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}

/* ------------------------------------------------------- */

static int app_init(app* ctx) {

  //const char* file_name = "./wallpaper-1920x1080.rgb";
  const char* file_name = "./desktop-1920x1080.rgb";
  int64_t file_size = 0;
  char* file_buf = NULL;
  FILE* fp = NULL;
  int r = 0;

  if (NULL == ctx) {
    TRAE("Cannot initialize the app as it's NULL.");
    r = -10;
    goto error;
  }

  if (0 == ctx->win_width) {
    TRAE("Make sure to set the `win_width` before calling `app_init()`.");
    r = -20;
    goto error;
  }

  if (0 == ctx->win_height) {
    TRAE("Make sure to set the `win_height` before calling `app_init()`.");
    r = -30;
    goto error;
  }

  /* Core */
  r = tra_core_create(&ctx->core_settings, &ctx->core);
  if (r < 0) {
    TRAE("Failed to create the core context.");
    r = -40;
    goto error;
  }

  /* CUDA (for encoder) */
  r = tra_core_api_get(ctx->core, "cuda", (void**) &ctx->cu_api);
  if (r < 0) {
    TRAE("Failed to get the CUDA api.");
    r = -83;
    goto error;
  }

  r = ctx->cu_api->init();
  if (r < 0) {
    TRAE("Failed to initialize the CUDA api.");
    r = -84;
    goto error;
  }

  r = ctx->cu_api->device_create(0, &ctx->cu_dev);
  if (r < 0) {
    TRAE("Failed to create a cuda device.");
    r = -85;
    goto error;
  }

  r = ctx->cu_api->context_create(ctx->cu_dev, 0, &ctx->cu_ctx);
  if (r < 0) {
    TRAE("Failed to create the cuda context.");
    r = -86;
    goto error;
  }

  r = ctx->cu_api->device_get_handle(ctx->cu_dev, &ctx->nvenc_settings.cuda_device_handle);
  if (r < 0) {
    TRAE("Failed to get the cuda device handle.");
    r = -87;
    goto error;
  }

  r = ctx->cu_api->context_get_handle(ctx->cu_ctx, &ctx->nvenc_settings.cuda_context_handle);
  if (r < 0) {
    TRAE("Failed to get the cuda context handle.");
    r = -88;
    goto error;
  }

  /* OpenGL API */
  r = tra_core_api_get(ctx->core, "opengl", (void**) &ctx->gl_api);
  if (r < 0) {
    TRAE("Failed to load the `opengl` API.");
    r = -50;
    goto error;
  }

  /* OpenGL converter */
  ctx->opengl_converter_settings.gl_api = ctx->gl_api;

  ctx->converter_settings.callbacks.on_converted = on_converted;
  ctx->converter_settings.callbacks.user = ctx;
  ctx->converter_settings.input_format  = TRA_IMAGE_FORMAT_RGB;
  ctx->converter_settings.input_type = TRA_CONVERT_TYPE_OPENGL_RGB24;
  ctx->converter_settings.input_width = ctx->win_width;
  ctx->converter_settings.input_height = ctx->win_height;  
  ctx->converter_settings.output_format = TRA_IMAGE_FORMAT_NV12;
  ctx->converter_settings.output_type = TRA_CONVERT_TYPE_OPENGL_NV12_SINGLE_TEXTURE;
  ctx->converter_settings.output_width = ctx->win_width;
  ctx->converter_settings.output_height = ctx->win_height;

  r = tra_core_converter_create(
    ctx->core,
    "glconverter",
    &ctx->converter_settings,
    &ctx->opengl_converter_settings,
    &ctx->converter
  );
  
  if (r < 0) {
    TRAE("Failed to create the `glconverter`.");
    r = -60;
    goto error;
  }

  /* Encoder */
  ctx->enc_settings.image_width = ctx->converter_settings.output_width;
  ctx->enc_settings.image_height = ctx->converter_settings.output_height;
  ctx->enc_settings.image_format = ctx->converter_settings.output_format;
  ctx->enc_settings.fps_num = 25;
  ctx->enc_settings.fps_den = 1;
  ctx->enc_settings.callbacks.on_encoded_data = on_encoded_data;
  ctx->enc_settings.callbacks.user = ctx;

  r = tra_core_encoder_create(
    ctx->core,
    "nvencgl",
    &ctx->enc_settings,
    &ctx->nvenc_settings,
    &ctx->enc
  );
  
  if (r < 0) {
    TRAE("Failed to create `nvencgl`.");
    r = -90;
    goto error;
  }

  /* Load the test image. */
  fp = fopen(file_name, "rb");
  if (NULL == fp) {
    TRAE("Failed to open %s.", file_name);
    r = -70;
    goto error;
  }

  r = fseek(fp, 0, SEEK_END);
  if (0 != r) {
    TRAE("Failed to seek the end of the input image.");
    r = -80;
    goto error;
  }

  file_size = ftell(fp);
  if (file_size < 0) {
    TRAE("Failed to get the file size of the input image..");
    r = -90;
    goto error;
  }

  /* Make sure we've loaded a RAW RGB24 image file. */
  if (file_size != (ctx->win_width * ctx->win_height * 3)) {
    TRAE("We expect the test input image file to hold the raw RGB pixes of an image that's the same size as the window: %u x %u", ctx->win_width, ctx->win_height);
    r = -100;
    goto error;
  }

  r = fseek(fp, 0, SEEK_SET);
  if (0 != r) {
    TRAE("Failed to jump back to the beginning of the file.");
    r = -100;
    goto error;
  }

  file_buf = malloc(file_size);
  if (NULL == file_buf) {
    TRAE("Failed to allocate the buffer for the input image.");
    r = -110;
    goto error;
  }
      
  r = fread(file_buf, file_size, 1, fp);
  if (1 != r) {
    TRAE("Failed to read the input file.");
    r = -120;
    goto error;
  }

  glGenTextures(1, &ctx->source_tex.tex_id);
  if (0 == ctx->source_tex.tex_id) {
    TRAE("Failed to allocate the source texture.");
    r = -130;
    goto error;
  }

  glBindTexture(GL_TEXTURE_2D, ctx->source_tex.tex_id);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, ctx->win_width, ctx->win_height, 0, GL_RGB, GL_UNSIGNED_BYTE, file_buf);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glBindTexture(GL_TEXTURE_2D, 0);
                                
  TRAE("@todo also cleanup the source texture");

  /* Create the output file where we store the generated h264. */
  ctx->output_fp = fopen("test-opengl-converter.h264", "wb");
  if (NULL == ctx->output_fp) {
    TRAE("Failed to open the output file into which we want to write the H264.");
    r = -140;
    goto error;
  }

 error:

  if (NULL != fp) {
    fclose(fp);
    fp = NULL;
  }

  if (NULL != file_buf) {
    free(file_buf);
    file_buf = NULL;
  }

  return r;
}

/* ------------------------------------------------------- */

static int app_update(app* ctx) {

  int r = 0;
  
 error:
  return r;
}

/* ------------------------------------------------------- */

static int app_shutdown(app* ctx) {

  int result = 0;
  int r = 0;

  if (NULL == ctx) {
    TRAE("Cannot shutdown as the given `app*` is NULL.");
    result -= 10;
    goto error;
  }

  if (NULL != ctx->converter) {
    r = tra_converter_destroy(ctx->converter);
    if (r < 0) {
      TRAE("Failed to cleanly destroy the converter.");
      result -= 20;
    }
  }

  if (NULL != ctx->enc) {
    r = tra_encoder_destroy(ctx->enc);
    if (r < 0) {
      TRAE("Failed to cleanly destroy the encoder.");
      result -= 25;
    }
  }

  if (NULL != ctx->core) {
    r = tra_core_destroy(ctx->core);
    if (r < 0) {
      TRAE("Failed to cleanly destroy the core.");
      result -= 30;
    }
  }

  if (NULL != ctx->output_fp) {
    fclose(ctx->output_fp);
    ctx->output_fp = NULL;
  }

  ctx->converter = NULL;
  ctx->core = NULL;
  
 error:
  return result;
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

/*
  This function gets called when the OpenGL based converter has
  converted the input texture. Ths OpenGL converter will convert
  a buffer from e.g. RGB24 into NV12. This function will forward
  the converted data into the encoder. This means that we're
  using a texture as source for the encoder.
 */
static int on_converted(uint32_t type, void* data, void* user) {

  app* ctx = NULL;
  int r = 0;

  if (NULL == user) {
    TRAE("Cannot handle the converted result; `user` pointer is NULL and it should point to our app instance.");
    r = -10;
    goto error;
  }

  ctx = (app*) user;

  r = on_converted_save(ctx, type, data);
  if (r < 0) {
    TRAE("Failed to save the converted data to a file.");
    r = -20;
    goto error;
  }

  r = on_converted_encode(ctx, type, data);
  if (r < 0) {
    TRAE("Failed to encode the converted texture.");
    r = -30;
    goto error;
  }
  
 error:
  return r;
}

/* ------------------------------------------------------- */

/*
  
  When the converted calls our `on_converted()` callback, we will
  save the result into a file that can directly be opened by
  YUView.
  
 */
static int on_converted_save(app* app, uint32_t type, void* data) {

  static int done = 0;
  tra_convert_opengl_nv12_single_texture* converted = NULL;
  int r = 0;

  /* Already saved once; return directly. */
  if (1 == done) {
    r = 0;
    goto error;
  }

  if (NULL == app) {
    TRAE("Cannot save the converted data to a file as the given `app*` is NULL.");
    r = -10;
    goto error;
  }

  if (TRA_CONVERT_TYPE_OPENGL_NV12_SINGLE_TEXTURE != type) {
    TRAE("Cannot save the converted data to a file as the given type is unsupported.");
    r = -20;
    goto error;
  }

  converted = (tra_convert_opengl_nv12_single_texture*) data;
  if (0 == converted->tex_id) {
    TRAE("Cannot save the converted data to a file as the received texture id is 0.");
    r = -30;
    goto error;
  }

  r = save_nv12_texture(
    "test_opengl_converter_from_gpu_1920x1080_yuv420pUVI.yuv",
    converted->tex_id,
    app->converter_settings.output_width,
    app->converter_settings.output_height
  );

  if (r < 0) {
    TRAE("Failed to save the NV12 texture.");
    r = -40;
    goto error;
  }
  
  done = 1;

 error:
  return r;
}

/* ------------------------------------------------------- */

/*
  
  This function will be called when the converter has converted
  the RGB input texture in a NV12 texture. This function will
  forward the converted into the encoder.

 */
static int on_converted_encode(app* app, uint32_t type, void* data) {

  static int done = 0;
  tra_convert_opengl_nv12_single_texture* src_tex = NULL;
  tra_memory_opengl_nv12 mem_enc = { 0 };
  int r = 0;

  if (1 == done) {
    r = 0;
    goto error;
  }
  
  if (NULL == app) {
    TRAE("Cannot encode as the given `app*` is NULL.");
    r = -10;
    goto error;
  }

  if (NULL == app->enc) {
    TRAE("Cannot encode as the `enc` member is NULL.");
    r = -20;
    goto error;
  }

  if (TRA_CONVERT_TYPE_OPENGL_NV12_SINGLE_TEXTURE != type) {
    TRAE("The result type of the converted texture is not what we can handle.");
    r = -30;
    goto error;
  }

  if (NULL == data) {
    TRAE("Cannot encode the converted texture as the given data is NULL.");
    r = -40;
    goto error;
  }

  src_tex = (tra_convert_opengl_nv12_single_texture*) data;
  if (0 == src_tex->tex_id) {
    TRAE("Cannot encode the converted texture as the texture id is 0.");
    r = -50;
    goto error;
  }

  if (0 == src_tex->tex_id) {
    TRAE("Cannot convert the converted texture as the `tex_id` from the converter is 0.");
    r = -60;
    goto error;
  }

  if (0 == src_tex->tex_stride) {
    TRAE("Cannot convert the converted texture as the `tex_stride` is 0.");
    r = -70;
    goto error;
  }

  mem_enc.tex_id = src_tex->tex_id;
  mem_enc.tex_stride = src_tex->tex_stride;

#if 0  
  TRAE("@todo I've hardcoded the use of the test texture: %u", app->test_nv12_tex_id);
  mem_enc.tex_id = app->test_nv12_tex_id;
  glBindTexture(GL_TEXTURE_2D, app->test_nv12_tex_id);
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, app->win_width, app->win_height + app->win_height / 2, GL_RED, GL_UNSIGNED_BYTE, app->test_nv12_buf);
  glBindTexture(GL_TEXTURE_2D, 0);
#endif  

  r = tra_encoder_encode(
    app->enc,
    &app->enc_sample,
    TRA_MEMORY_TYPE_OPENGL_NV12,
    &mem_enc
  );

  //done = 1;
  
  if (r < 0) {
    TRAE("Failed to encode.");
    r = -60;
    goto error;
  }

 error:
  return r;
}

/* ------------------------------------------------------- */

static int on_encoded_data(uint32_t type, void* data, void* user) {

  tra_memory_h264* mem = NULL; 
  app* ctx = NULL;
  int r = 0;
  
  if (NULL == data) {
    TRAE("Cannot handle the encoded data as the `data` pointer is NULL.");
    r = -10;
    goto error;
  }

  if (NULL == user) {
    TRAE("Cannot handle the encoded data as the user pointer is NULL.");
    r = -20;
    goto error;
  }

  if (TRA_MEMORY_TYPE_H264 != type) {
    TRAE("Cannot handle encoded data, unsupported type: %u. (exiting)", type);
    exit(EXIT_FAILURE);
  }
  
  mem = (tra_memory_h264*) data;
  if (NULL == mem) {
    TRAE("Cannot handle encoded data, `data` is NULL. (exiting).");
    exit(EXIT_FAILURE);
  }

  if (0 == mem->size) {
    TRAE("Cannot handle encoded data, the `size` is 0. (exiting).") ;
    exit(EXIT_FAILURE);
  }

  ctx = (app*) user;
  if (NULL == ctx->output_fp) {
    TRAE("Cannot write the generated h264 as the output file handle is NULL. (exiting).");
    exit(EXIT_FAILURE);
  }

  r = fwrite(mem->data, mem->size, 1, ctx->output_fp);
  if (1 != r) {
    TRAE("Cannot handle encoded data. Failed to write the encoded data. (exiting)");
    exit(EXIT_FAILURE);
  }

  TRAD("Wrote encoded data!");

 error:

  return r;
}

/* ------------------------------------------------------- */

/*
 */
static int save_nv12_texture(
  const char* filename,
  GLuint tex,
  uint32_t width,
  uint32_t height
)
{
  uint8_t* out_buf = NULL;
  uint32_t out_size = 0;
  FILE* out_fp = NULL;
  int r = 0;
  
  out_fp = fopen(filename, "wb");
  if (NULL == out_fp) {
    TRAE("Failed to open the output test image.");
    r = -50;
    goto error;
  }
  
  out_size = (width * height) + (width * height * 0.5);

  out_buf = malloc(out_size);
  if (NULL == out_buf) {
    TRAE("Failed to allocate the output buffer.");
    r = -60;
    goto error;
  }

  /* Write the converted image to a file. */
  glBindTexture(GL_TEXTURE_2D, tex);
  glGetTexImage(GL_TEXTURE_2D, 0, GL_RED, GL_UNSIGNED_BYTE, out_buf);
  fwrite(out_buf, out_size, 1, out_fp);
  fclose(out_fp);

 error:

  if (NULL != out_buf) {
    free(out_buf);
    out_buf = NULL;
  }
  return r;
}

/* ------------------------------------------------------- */

/*
  
  Quick test to see how the converted buffer should look like in
  render doc. I've also used this test while trying to figure out
  the cause of the bug where I couldn't register a texture. We assume
  taht the texture that we load is a NV12 buffer and has the same
  resolution as `win_width` x `win_height`.
 
  You can create the test NV12 file using:

     convert netscape: -resize 1920x1080! desktop-1920x1080.png
     ffmpeg -i desktop-1920x1080.png -pix_fmt nv12 desktop_converted_with_ffmpeg_1920x1080_yuv420pUVI.yuv
      
*/

static int app_load_test_texture(app* app) {

  const char* file_name = "desktop_converted_with_ffmpeg_1920x1080_yuv420pUVI.yuv";
  uint32_t texture_target = GL_TEXTURE_2D;
  size_t expected_size = 0;
  size_t file_size = 0;
  //char* buf = NULL;
  FILE* fp = NULL;
  int r = 0;

  if (NULL == app) {
    TRAE("Cannot load the test texture; given `app*` is NULL.");
    r = -10;
    goto error;
  }

  if (0 == app->win_width) {
    TRAE("Cannot load the test texture as we expect the `win_width` to be set.");
    r = -20;
    goto error;
  }

  if (0 == app->win_height) {
    TRAE("Cannot load the test texture as the `win_height` is not set.");
    r = -30;
    goto error;
  }

  /* Read the file */
  fp = fopen(file_name, "rb");
  if (NULL == fp) {
    TRAE("Failed to open the test NV12 image: `%s`.", file_name);
    r = -40;
    goto error;
  }

  expected_size = app->win_width * app->win_height + (app->win_width * (app->win_height / 2));
  
  r = fseek(fp, 0, SEEK_END);
  if (0 != r) {
    TRAE("Failed to seek to the end.");
    r = -50;
    goto error;
  }

  file_size = ftell(fp);

  r = fseek(fp, 0, SEEK_SET);
  if (0 != r) {
    TRAE("Failed to rewind the file.");
    r = -60;
    goto error;
  }

  if (file_size != expected_size) {
    TRAE("The NV12 test file has a different file size (%zu) than we expect (%zu).", file_size, expected_size);
    r = -70;
    goto error;
  }
  
  app->test_nv12_buf = malloc(file_size);
  if (NULL == app->test_nv12_buf) {
    TRAE("Failed to allocate the buffer for the NV12.");
    r = -80;
    goto error;
  }
  
  r = fread(app->test_nv12_buf, file_size, 1, fp);
  if (1 != r) {
    TRAE("Failed to read the test file.");
    r = -90;
    goto error;
  }

  /* Create the test texture. */
  glGenTextures(1, &app->test_nv12_tex_id);
  glBindTexture(texture_target, app->test_nv12_tex_id);
  // glTexImage2D(texture_target, 0, GL_R8, app->win_width, app->win_height + app->win_height / 2, 0, GL_RED, GL_UNSIGNED_BYTE, NULL);
  glTexImage2D(texture_target, 0, GL_R8, app->win_width, app->win_height * 3 / 2, 0, GL_RED, GL_UNSIGNED_BYTE, NULL);
  glTexParameteri(texture_target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(texture_target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

  TRAD("Loaded the test NV12 texture: %u", app->test_nv12_tex_id);
  TRAD("Texture: ");
  TRAD("  width: %u", app->win_width);
  TRAD("  height: %u", (uint32_t) (app->win_height * 1.5));

 error:

  if (NULL != fp) {
    fclose(fp);
    fp = NULL;
  }
  
  return r;
}

/* ------------------------------------------------------- */

/* Creates the buffers that we can use to generate some video. */
static int app_load_test_program(app* app) {

  GLenum status = GL_NONE;
  int result = 0;
  int r = 0;

  if (NULL == app) {
    TRAE("Cannot create the test program as the given `app*` is NULL.");
    r = -10;
    goto error;
  }

  if (0 == app->win_width) {
    TRAE("Cannot create the test program; `win_width` not set.");
    r = -20;
    goto error;
  }

  if (0 == app->win_height) {
    TRAE("Cannot create the test program; `win_height` not set.");
    r = -20;
    goto error;
  }

  /* Create resources */
  glGenVertexArrays(1, &app->test_vao);
  if (0 == app->test_vao) {
    TRAE("Cannot create the test program as we failed to create the vertex array.");
    r = -20;
    goto error;
  }

  app->test_vs = glCreateShader(GL_VERTEX_SHADER);
  if (0 == app->test_vs) {
    TRAE("Cannot create the test program as we failed to create the vertex shader.");
    r = -20;
    goto error;
  }
  
  app->test_fs = glCreateShader(GL_FRAGMENT_SHADER);
  if (0 == app->test_fs) {
    TRAE("Cannot create the test program as we failed to create the fragment shader.");
    r = -30;
    goto error;
  }
  
  app->test_prog = glCreateProgram();
  if (0 == app->test_prog) {
    TRAE("Cannot create the test program as we failed to create the program.");
    r = -40;
    goto error;
  }

  /* Setup shaders */
  glShaderSource(app->test_vs, 1, &TEST_VS, NULL);
  glCompileShader(app->test_vs);

  glGetShaderiv(app->test_vs, GL_COMPILE_STATUS, &result);
  if (0 == result) {
    TRAE("Failed to compile the vertex shader.");
    r = -50;
    goto error;
  }

  TRAE("@todo we're still using TEST_FS and TEST_VS; rename, cleanup, etc.");
  
  glShaderSource(app->test_fs, 1, &TEST_FS, NULL);
  glCompileShader(app->test_fs);

  glGetShaderiv(app->test_fs, GL_COMPILE_STATUS, &result);
  if (0 == result) {
    TRAE("Failed to compile the fragment shader.");
    r = -60;
    goto error;
  }

  /* Link the program. */
  glAttachShader(app->test_prog, app->test_vs);
  glAttachShader(app->test_prog, app->test_fs);
  glLinkProgram(app->test_prog);

  glGetProgramiv(app->test_prog, GL_LINK_STATUS, &result);
  if (0 == result) {
    TRAE("Failed to link the test program.");
    r = -70;
    goto error;  
  }

  /* Create Texture + FBO */
  glGenTextures(1, &app->test_tex);
  if (0 == app->test_tex) {
    TRAE("Failed to create the texture.");
    r = -80;
    goto error;
  }
  
  glGenFramebuffers(1, &app->test_fbo);
  if (0 == app->test_fbo) {
    TRAE("Failed to create the FBO.");
    r = -90;
    goto error;
  }

  glBindTexture(GL_TEXTURE_2D, app->test_tex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, app->win_width, app->win_height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, app->test_fbo);
  glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, app->test_tex, 0);

  status = glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER);
  if (GL_FRAMEBUFFER_COMPLETE != status) {
    TRAE("Failed to setup the framebuffer correctly.");
    r = -100;
    goto error;
  }
    
  /* Configure */
  app->loc_resolution = glGetUniformLocation(app->test_prog, "u_resolution");
  app->loc_time = glGetUniformLocation(app->test_prog, "u_time");

  glUseProgram(app->test_prog);
  glUniform2f(app->loc_resolution, app->win_width, app->win_height);
  glUseProgram(0);
  
 error:

  return r;
}

/* ------------------------------------------------------- */
