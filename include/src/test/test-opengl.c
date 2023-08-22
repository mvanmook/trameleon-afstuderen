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


  OPENGL AND NVIDIA EXPERIMENT
  ============================

  GENERAL INFO:

    I've created this test while looking into the API for
    graphics interop with decoder and encoders. This test uplaods
    a CUDA buffer which holds decoded NV12 data, to the GPU using
    a PBO. Once I got this test to render/display the decoded
    video I started with the graphics structs that Trameleon will
    use for graphics interop. See `test-module-nvidia-graphics.c`
    which provides the same features as this file but uses the
    interop layer intead.

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
#include <tra/modules/nvidia/nvidia.h>
#include <tra/modules/opengl/opengl-gfx.h>
#include <tra/modules/opengl/opengl-api.h>
#include <tra/module.h>
#include <tra/types.h>
#include <tra/core.h>
#include <tra/log.h>
#include <dev/generator.h>

/* ------------------------------------------------------- */

static const char* NV12_VS = ""
    "#version 330\n"
    ""
    "uniform mat4 u_pm;"
    "uniform mat4 u_vm;"
    ""
    "out vec2 v_tex;"
    ""
    "const vec2 pos[] = vec2[4]("
    "  vec2(0.0, 1.0), "
    "  vec2(0.0, 0.0), "
    "  vec2(1.0, 1.0), "
    "  vec2(1.0, 0.0)  "
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
    "  gl_Position = u_pm * u_vm * vec4(pos[gl_VertexID], 0.0, 1.0); "
     "  v_tex = tex[gl_VertexID];"
    "}"
  "";

static const char* NV12_FS = ""
    "#version 330\n"
    ""
    "uniform sampler2D u_tex_y;"
    "uniform sampler2D u_tex_uv;"
    "in vec2 v_tex;"
    "layout ( location = 0 ) out vec4 fragcolor;"
    ""
    "const vec3 r_coeff = vec3(1.164,  0.000,  1.596);\n"
    "const vec3 g_coeff = vec3(1.164, -0.391, -0.813);\n"
    "const vec3 b_coeff = vec3(1.164,  2.018,  0.000);\n"
    ""
    "const vec3 offset = vec3(-0.0627451017, -0.501960814, -0.501960814);\n"
    ""
    "void main() {"
    "  vec3 yuv, rgb;"
    "  float r,g,b;"
    "  yuv.x = texture(u_tex_y, v_tex).r;"
    "  yuv.yz = texture(u_tex_uv, v_tex).rg;"
    "  yuv += offset;"
    "  r = dot(yuv, r_coeff);"
    "  g = dot(yuv, g_coeff);"
    "  b = dot(yuv, b_coeff);"
    "  fragcolor.rgb = vec3(r,g,b);"
    "  fragcolor.a = 1.0;"
    ""
    "}"
  "";

/* ------------------------------------------------------- */

static void on_key(GLFWwindow* win, int k, int s, int action, int mods);
static int on_encoded_data(uint32_t type, void* data, void* user); /* Gets called by the generator when it has some h264. */
static int on_decoded_data(uint32_t type, void* data, void* user); /* Gets called when the nvidia decoder has decoded data. */

/* ------------------------------------------------------- */

typedef struct app app;

/* ------------------------------------------------------- */

struct app {

  /* General */
  uint32_t image_width;
  uint32_t image_height;
  uint32_t is_ready; 

  /* H264 Generator */
  dev_generator_settings gen_cfg;
  dev_generator* gen;

  /* Core */
  tra_core_settings core_settings;
  tra_core* core;

  /* Decoder */
  nvdec_settings nv_settings;
  tra_decoder_callbacks dec_callbacks;
  tra_decoder_settings dec_settings;
  tra_decoder* dec;

  /* CUDA for decoder. */
  tra_cuda_context* cu_ctx;
  tra_cuda_device* cu_dev;
  tra_cuda_api* cu_api;

  /* OpenGL resources. */
  GLuint pbo_y;
  GLuint pbo_uv;
  GLuint tex_y;
  GLuint tex_uv;
  GLuint vs; /* Vertex shader */
  GLuint fs; /* Fragment shader */
  GLuint prog; /* The shader program that we use when rendering. */
  GLuint vao;
  float pm[16]; /* Orthographic projection matrix. */
  float vm[16]; /* View matrix */

  /* Experimental: OpenGL graphics interop. */
  gl_gfx_settings gl_settings;
  tra_opengl_api* gl_api;
  tra_graphics_settings gfx_settings;
  tra_graphics* gfx;
};

static int app_init(app* ctx);
static int app_update(app* ctx);
static int app_shutdown(app* ctx);

/* ------------------------------------------------------- */

int main(int argc, const char* argv[]) {

  GLFWwindow* win = NULL;
  int width = 1280;
  int height = 720;
  app app = { 0 };
  int r = 0;
   
  TRAI("OpenGL Development Test");
  
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

    if (0 == app.is_ready) {
      r = app_update(&app);
      if (r < 0) {
        glfwSetWindowShouldClose(win, GLFW_TRUE);
        continue;
      }
    }

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, app.tex_y);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, app.tex_uv);

    glDisable(GL_DEPTH_TEST);
    glBindVertexArray(app.vao);
    glUseProgram(app.prog);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    glfwSwapBuffers(win);
    glfwPollEvents();
  }

 error:

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

  float sx = 0.0f;
  float sy = 0.0f;
  float rml = 0.0f;
  float fmn = 0.0f;
  float tmb = 0.0f;
  GLint u_pm = -1;
  GLint u_vm = -1;
  GLint u_tex_y = -1;
  GLint u_tex_uv = -1;
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
  ctx->dec_settings.output_type = TRA_MEMORY_TYPE_DEVICE_IMAGE;

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

  /* @todo currently we're not setting the image format for the decoder! Below we assume it's NV12. */
  r = tra_core_graphics_create(ctx->core, "opengl", &ctx->gfx_settings, &ctx->gl_settings, &ctx->gfx);
  if (r < 0) {
    TRAE("Failed to create the `opengl` graphics interop.");
    r = -90;
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
  /* OpenGL                                          */
  /* ----------------------------------------------- */

  glGenBuffers(1, &ctx->pbo_y);
  if (0 == ctx->pbo_y) {
    TRAE("Failed to create the Y PBO.");
    r = -10;
    goto error;
  }

  glGenBuffers(1, &ctx->pbo_uv);
  if (0 == ctx->pbo_uv) {
    TRAE("Failed to allocate the UV PBO.");
    r = -20;
    goto error;
  }

  glGenTextures(1, &ctx->tex_y);
  if (0 == ctx->tex_y) {
    TRAE("Failed to create a texture for the Y-plane.");
    r = -50;
    goto error;
  }

  glGenTextures(1, &ctx->tex_uv);
  if (0 == ctx->tex_uv) {
    TRAE("Failed to create the texture for the UV-plane.");
    r = -60;
    goto error;
  }

  ctx->vs = glCreateShader(GL_VERTEX_SHADER);
  if (0 == ctx->vs) {
    TRAE("Failed to create the vertex shader.");
    r = -60;
    goto error;
  }

  ctx->fs = glCreateShader(GL_FRAGMENT_SHADER);
  if (0 == ctx->fs) {
    TRAE("Failed to create the fragment shader.");
    r = -70;
    goto error;
  }

  ctx->prog = glCreateProgram();
  if (0 == ctx->prog) {
    TRAE("Failed to create the shader program.");
    r = -80;
    goto error;
  }
  
  glGenVertexArrays(1, &ctx->vao);
  if (0 == ctx->vao) {
    TRAE("Failed to create the vertex array");
    r = -90;
    goto error;
  }

  /* Setup the PBOs */
  glBindBuffer(GL_PIXEL_UNPACK_BUFFER, ctx->pbo_y);
  glBufferData(GL_PIXEL_UNPACK_BUFFER, ctx->image_width * ctx->image_height, NULL, GL_STREAM_DRAW);
  glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

  glBindBuffer(GL_PIXEL_UNPACK_BUFFER, ctx->pbo_uv);
  glBufferData(GL_PIXEL_UNPACK_BUFFER, (ctx->image_width * ctx->image_height) / 2, NULL, GL_STREAM_DRAW);
  glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

  /* Setup Y-texture. */
  glBindTexture(GL_TEXTURE_2D, ctx->tex_y);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, ctx->image_width, ctx->image_height, 0, GL_RED, GL_UNSIGNED_BYTE, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glBindTexture(GL_TEXTURE_2D, 0);

  /* Setup UV-texture. */
  glBindTexture(GL_TEXTURE_2D, ctx->tex_uv);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RG, ctx->image_width / 2, ctx->image_height / 2, 0, GL_RG, GL_UNSIGNED_BYTE, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glBindTexture(GL_TEXTURE_2D, 0);

  /* Setup vertex shader. */
  glShaderSource(ctx->vs, 1, &NV12_VS, NULL);
  glCompileShader(ctx->vs);

  glGetShaderiv(ctx->vs, GL_COMPILE_STATUS, &result);
  if (0 == result) {
    TRAE("Failed to compile the vertex shader.");
    r = -70;
    goto error;
  }

  /* Setup fragment shader. */
  glShaderSource(ctx->fs, 1, &NV12_FS, NULL);
  glCompileShader(ctx->fs);

  glGetShaderiv(ctx->fs, GL_COMPILE_STATUS, &result);
  if (0 == result) {
    TRAE("Failed to compile the fragment shader.");
    r = -80;
    goto error;
  }

  glAttachShader(ctx->prog, ctx->vs);
  glAttachShader(ctx->prog, ctx->fs);
  glLinkProgram(ctx->prog);

  glGetProgramiv(ctx->prog, GL_LINK_STATUS, &result);
  if (0 == result) {
    TRAE("Failed to link the shader program.");
    r = -90;
    goto error;
  }

  /* Create orthographic projection matrix (scale to 2-unit box, centralize) . */
  sx = 2.0f / ctx->image_width;
  sy = 2.0f / ctx->image_height;

  ctx->pm[0] = 2.0f / ctx->image_width;
  ctx->pm[5] = -2.0f / ctx->image_height;
  ctx->pm[10] = 1.0f;
  ctx->pm[12] = -1.0f;
  ctx->pm[13] =  1.0f;
  ctx->pm[15] =  1.0f;

  /* Create view matrix: as we draw a unit rect in the shader, we scale it by the size of the video. */
  ctx->vm[0] = ctx->image_width;
  ctx->vm[5] = ctx->image_height;
  ctx->vm[10] = 1.0f;
  ctx->vm[15] = 1.0f;

  /* Get uniforms so we can configure the shader. */
  glUseProgram(ctx->prog);

  u_pm = glGetUniformLocation(ctx->prog, "u_pm");
  if (-1 == u_pm) {
    TRAE("Failed to get the `u_pm` uniform.");
    r = -100;
    goto error;
  }
  
  u_vm = glGetUniformLocation(ctx->prog, "u_vm");
  if (-1 == u_vm) {
    TRAE("Failed to get the `u_vm` uniform.");
    r = -110;
    goto error;
  }

  u_tex_y = glGetUniformLocation(ctx->prog, "u_tex_y");
  if (-1 == u_tex_y) {
    TRAE("Failed to get the `u_tex_y` uniform.");
    r = -120;
    goto error;
  }
  
  u_tex_uv = glGetUniformLocation(ctx->prog, "u_tex_uv");
  if (-1 == u_tex_uv) {
    TRAE("Failed to get he `u_tex_uv` uniform.");
    r = -130;
    goto error;
  }
  
  glUniformMatrix4fv(u_pm, 1, GL_FALSE, ctx->pm);
  glUniformMatrix4fv(u_vm, 1, GL_FALSE, ctx->vm);
  glUniform1i(u_tex_y, 0);
  glUniform1i(u_tex_uv, 1);
  
  glUseProgram(0);
  
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
  
  if (NULL != ctx->core) {
    tra_core_destroy(ctx->core);
    ctx->core = NULL;
  }

  /* Destroy GL related objects. */
  if (0 != ctx->pbo_y) {
    glDeleteBuffers(1, &ctx->pbo_y);
    ctx->pbo_y = 0;
  }

  if (0 != ctx->pbo_uv) {
    glDeleteBuffers(1, &ctx->pbo_uv);
    ctx->pbo_uv = 0;
  }

  if (0 != ctx->tex_y) {
    glDeleteTextures(1, &ctx->tex_y);
    ctx->tex_y = 0;
  }

  if (0 != ctx->tex_uv) {
    glDeleteTextures(1, &ctx->tex_uv);
    ctx->tex_uv = 0;
  }

  if (0 != ctx->vs) {
    glDeleteShader(ctx->vs);
    ctx->vs = 0;
  }

  if (0 != ctx->fs) {
    glDeleteShader(ctx->fs);
    ctx->fs = 0;
  }

  if (0 != ctx->prog) {
    glDeleteProgram(ctx->prog);
    ctx->prog = 0;
  }

  if (0 != ctx->vao) {
    glDeleteVertexArrays(1, &ctx->vao);
    ctx->vao = 0;
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

  tra_encoded_host_memory* host_mem = { 0 };
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

  if (TRA_MEMORY_TYPE_HOST_H264 != type) {
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

  host_mem = (tra_encoded_host_memory*) data;
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

  r = tra_decoder_decode(ctx->dec, TRA_MEMORY_TYPE_HOST_H264, host_mem);
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
  Gets called when the nvidia decoder has decoded data. This
  is based on the `FramePresenterGL.h` implementation.
*/
static int on_decoded_data(uint32_t type, void* data, void* user) {

  tra_decoded_device_memory* dev_mem = NULL;
  tra_cuda_device_memory* cuda_mem = NULL;
  CUgraphicsResource reg_y = { 0 };
  CUgraphicsResource reg_uv = { 0 };
  CUdeviceptr ptr_y = { 0 };
  CUdeviceptr ptr_uv = { 0 };
  CUDA_MEMCPY2D mem_copy = { 0 };
  size_t mapped_size = 0;
  CUresult result = CUDA_SUCCESS;
  app* ctx = NULL;
  int is_y_registered = 0;
  int is_y_mapped = 0;
  int is_uv_registered = 0;
  int is_uv_mapped = 0;
  int r = 0;

  /* ----------------------------------------------- */
  /* Validate                                        */
  /* ----------------------------------------------- */

  if (NULL == data) {
    TRAE("Cannot handle the decoded data as it's NULL.");
    r = -10;
    goto error;
  }

  if (NULL == user) {
    TRAE("Cannot handle the decoded data; the user pointer is NULL. This should be a pointer to our app instance.");
    r = -20;
    goto error;
  }

  if (TRA_MEMORY_TYPE_DEVICE_IMAGE != type) {
    TRAE("Received decoded data but it's not stored on device; we can only handle device memory at this point.");
    r = -30;
    goto error;
  }

  dev_mem = (tra_decoded_device_memory*) data;
  if (TRA_DEVICE_MEMORY_TYPE_CUDA != dev_mem->type) {
    TRAE("We received decoded device memory but it's not CUDA memory. We expect it to be CUDA memory.");
    r = -40;
    goto error;
  }

  if (NULL == dev_mem->data) {
    TRAE("The received CUDA device memory data is NULL. ");
    r = -50;
    goto error;
  }

  cuda_mem = (tra_cuda_device_memory*) dev_mem->data;
  if (0 == cuda_mem->ptr) {
    TRAE("The receivced cuda device memory pointer is NULL.");
    r = -60;
    goto error;
  }

  ctx = (app*) user;
  if (0 == ctx->pbo_y) {
    TRAE("Cannot handle the decode data as our `pbo` is not created.");
    r = -40;
    goto error;
  }

  /* ----------------------------------------------- */
  /* Get pointer to registered PBO                   */
  /* ----------------------------------------------- */

  /* Register + map */
  result = cuGraphicsGLRegisterBuffer(&reg_y, ctx->pbo_y, CU_GRAPHICS_REGISTER_FLAGS_WRITE_DISCARD);
  if (CUDA_SUCCESS != result) {
    TRAE("Failed to register the Y-PBO.");
    r = -40;
    goto error;
  }

  is_y_registered = 1;

  result = cuGraphicsGLRegisterBuffer(&reg_uv, ctx->pbo_uv, CU_GRAPHICS_REGISTER_FLAGS_WRITE_DISCARD);
  if (CUDA_SUCCESS != result) {
    TRAE("Failed to register the UV-PBO.");
    r = -50;
    goto error;
  }

  is_uv_registered = 1;

  /* Map the PBO so it's accessible by CUDA */
  result = cuGraphicsMapResources(1, &reg_y, 0);
  if (CUDA_SUCCESS != result) {
    TRAE("Failed to map the registered PBO resource.");
    r = -50;
    goto error;
  }

  is_y_mapped = 1;

  result = cuGraphicsMapResources(1, &reg_uv, 0);
  if (CUDA_SUCCESS != result) {
    TRAE("Failed to map the registerd UV-PBO resource.");
    r = -70;
    goto error;
  }

  is_uv_mapped = 1;

  /* Now that we've mapped the PBO registration we can get the device pointer that represents this mapping. */
  result = cuGraphicsResourceGetMappedPointer(&ptr_y, &mapped_size, reg_y);
  if (CUDA_SUCCESS != result) {
    TRAE("Failed to get the mapped pointer for Y.");
    r = -60;
    goto error;
  }
  
  result = cuGraphicsResourceGetMappedPointer(&ptr_uv, &mapped_size, reg_uv);
  if (CUDA_SUCCESS != result) {
    TRAE("Failed to get the mapped pointer for UV.");
    r = -60;
    goto error;
  }

  /* ----------------------------------------------- */
  /* Copy decoded data into PBO.                     */
  /* ----------------------------------------------- */

  /* Copy into the Y-PBO */
  mem_copy.srcMemoryType = CU_MEMORYTYPE_DEVICE;
  mem_copy.srcDevice = cuda_mem->ptr;
  mem_copy.srcPitch = cuda_mem->pitch;
  mem_copy.dstMemoryType = CU_MEMORYTYPE_DEVICE;
  mem_copy.dstDevice = ptr_y;
  mem_copy.dstPitch = ctx->image_width;
  mem_copy.WidthInBytes = ctx->image_width;
  mem_copy.Height = ctx->image_height;
  
  result = cuMemcpy2DAsync(&mem_copy, 0);
  if (CUDA_SUCCESS != result) {
    TRAE("Failed to copy the decoded buffer.");
    r = -70;
    goto error;
  }

  /* Copy into the UV-PBO */
  mem_copy.srcMemoryType = CU_MEMORYTYPE_DEVICE;
  mem_copy.srcDevice = cuda_mem->ptr + (cuda_mem->pitch * ctx->image_height);
  mem_copy.srcPitch = cuda_mem->pitch;
  mem_copy.dstMemoryType = CU_MEMORYTYPE_DEVICE;
  mem_copy.dstDevice = ptr_uv;
  mem_copy.dstPitch = ctx->image_width; /* The image width is half the size of the Y-plane, though we have 2 values U and V per pixel; so the destination pitch is `(2 * half_width) = width` */
  mem_copy.WidthInBytes = ctx->image_width;
  mem_copy.Height = ctx->image_height / 2;
  
  result = cuMemcpy2DAsync(&mem_copy, 0);
  if (CUDA_SUCCESS != result) {
    TRAE("Failed to copy the decoded buffer.");
    r = -70;
    goto error;
  }

  /* This performs a copy directly from the PBO into the textures. */
  glBindBufferARB(GL_PIXEL_UNPACK_BUFFER, ctx->pbo_y);
  glBindTexture(GL_TEXTURE_2D, ctx->tex_y);
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, ctx->image_width, ctx->image_height, GL_RED, GL_UNSIGNED_BYTE, 0);

  glBindBufferARB(GL_PIXEL_UNPACK_BUFFER, ctx->pbo_uv);
  glBindTexture(GL_TEXTURE_2D, ctx->tex_uv);
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, ctx->image_width / 2 , ctx->image_height / 2, GL_RG, GL_UNSIGNED_BYTE, 0);

  glBindBufferARB(GL_PIXEL_UNPACK_BUFFER, 0);
  
 error:
  
  if (1 == is_y_registered) {
    cuGraphicsUnmapResources(1, &reg_y, 0);
    is_y_registered = 0;
  }
  
  if (1 == is_y_mapped) {
    cuGraphicsUnregisterResource(reg_y);
    is_y_mapped = 0;
  }

  if (1 == is_uv_registered) {
    cuGraphicsUnmapResources(1, &reg_uv, 0);
    is_uv_registered = 0;
  }
  
  if (1 == is_uv_mapped) {
    cuGraphicsUnregisterResource(reg_uv);
    is_uv_mapped = 0;
  }

  return r;
}

/* ------------------------------------------------------- */

