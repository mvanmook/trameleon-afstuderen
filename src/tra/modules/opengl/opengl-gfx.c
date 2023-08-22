/* ------------------------------------------------------- */

#define GL_GLEXT_PROTOTYPES
#include <GL/glcorearb.h>

#include <stdlib.h>
#include <cuda.h>
#include <cudaGL.h>
#include <tra/modules/opengl/opengl-gfx.h>
#include <tra/modules/opengl/opengl-api.h>
#include <tra/modules/opengl/opengl.h>
#include <tra/modules/nvidia/nvidia.h>
#include <tra/profiler.h>
#include <tra/registry.h>
#include <tra/module.h>
#include <tra/utils.h>
#include <tra/types.h>
#include <tra/log.h>

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

static const char* graphics_get_name();
static const char* graphics_get_author();
static int graphics_create(tra_graphics_settings* cfg, void* settings, tra_graphics_object** obj);
static int graphics_destroy(tra_graphics_object* obj);
static int graphics_draw(tra_graphics_object* obj, uint32_t type, void* data);

/* ------------------------------------------------------- */

static tra_graphics_api graphics_api; /* The struct that we use to register the OpenGL graphics module. */

/* ------------------------------------------------------- */

struct gl_gfx {

  /* General */
  uint32_t image_width;
  uint32_t image_height;
  tra_opengl_funcs gl;

  /* Used when uploading CUDA device memory into NV12 buffers. */
  GLuint vs;       /* Vertex shader. */
  GLuint fs;       /* Fragment shader. */
  GLuint prog;     /* Shader program */
  GLuint vao;      /* Vertex array object; we "generate" vertices in the VS shader but still need a VertexArray object. */
  GLint u_vm;
  GLint u_pm;
  float pm[16];    /* Orthographic projection matrix. */
  float vm[16];    /* View matrix */
};

/* ------------------------------------------------------- */

int gl_gfx_create(
  tra_graphics_settings* cfg,
  gl_gfx_settings* settings,
  gl_gfx** obj
)
{
  
  TRAP_TIMER_BEGIN(prof, "gl_gfx_create");
  
  gl_gfx* inst = NULL;
  GLint u_tex_uv = -1;
  GLint u_tex_y = -1;
  GLint u_pm = -1;
  GLint u_vm = -1;
  int result = 0;
  int r = 0;

  /* ----------------------------------------------- */
  /* Validate                                        */
  /* ----------------------------------------------- */

  if (NULL == cfg) {
    TRAE("Cannot create the `opengl-gfx` graphics interop as the given `tra_graphics_settings*` is NULL.");
    r = -10;
    goto error;
  }

  if (NULL == obj) {
    TRAE("Cannot create the `opengl-gfx` graphics interop intance as the given destination is NULL.");
    r = -20;
    goto error;
  }

  if (NULL != *(obj)) {
    TRAE("Cannot create the `opengl-gfx` graphics interop intance as the given destination is NOT NULL. Already created? Not initialized to NULL?");
    r = -30;
    goto error;
  }

  if (0 == cfg->image_width) {
    TRAE("Cannot create the `opengl-gfx` graphics interop as the given `image_width` is 0.");
    r = -40;
    goto error;
  }

  if (0 == cfg->image_height) {
    TRAE("Cannot create the `opengl-gfx` graphics interop as the given `image_height` is 0.");
    r = -50;
    goto error;
  }

  if (TRA_IMAGE_FORMAT_NV12 != cfg->image_format) {
    TRAE("Cannot create the `opengl-gfx` graphics interop as the given `image_format` is unupported. Currently we only support `TRA_IMAGE_FORMAT_NV12`.");
    r = -60;
    goto error;
  }

  if (NULL == settings) {
    TRAE("Cannot create the `opengl-gfx` graphics interop as the given `gl_gfx_settings*` is NULL.");
    r = -70;
    goto error;
  }

  if (NULL == settings->gl_api) {
    TRAE("Cannot create the `opengl-gfx` graphics interop as the given `gl_gfx_settings::gl` member is NULL. ");
    r = -70;
    goto error;
  }

  /* ----------------------------------------------- */
  /* Create                                          */
  /* ----------------------------------------------- */

  inst = calloc(1, sizeof(gl_gfx));
  if (NULL == inst) {
    TRAE("Cannot create the `opengl-gfx` graphics interop. Failed to allocate the `graphics` instance.");
    r = -40;
    goto error;
  }

  /* ----------------------------------------------- */
  /* OpenGL                                          */
  /* ----------------------------------------------- */

  /* Assign the GL functions that the user provided. */
  r = settings->gl_api->load_functions(&inst->gl);
  if (r < 0) {
    TRAE("Cannot create the `opengl-gfx` graphics interop. Failed to load the OpenGL functions.");
    r = -50;
    goto error;
  }

  inst->vs = inst->gl.glCreateShader(GL_VERTEX_SHADER);
  if (0 == inst->vs) {
    TRAE("Failed to create the vertex shader.");
    r = -60;
    goto error;
  }

  inst->fs = inst->gl.glCreateShader(GL_FRAGMENT_SHADER);
  if (0 == inst->fs) {
    TRAE("Failed to create the fragment shader.");
    r = -70;
    goto error;
  }

  inst->prog = inst->gl.glCreateProgram();
  if (0 == inst->prog) {
    TRAE("Failed to create the shader program.");
    r = -80;
    goto error;
  }

  inst->gl.glGenVertexArrays(1, &inst->vao);
  if (0 == inst->vao) {
    TRAE("Failed to create the vertex array");
    r = -90;
    goto error;
  }

  /* Setup vertex shader. */
  inst->gl.glShaderSource(inst->vs, 1, &NV12_VS, NULL);
  inst->gl.glCompileShader(inst->vs);

  inst->gl.glGetShaderiv(inst->vs, GL_COMPILE_STATUS, &result);
  if (0 == result) {
    TRAE("Failed to compile the vertex shader.");
    r = -70;
    goto error;
  }

  /* Setup fragment shader. */
  inst->gl.glShaderSource(inst->fs, 1, &NV12_FS, NULL);
  inst->gl.glCompileShader(inst->fs);

  inst->gl.glGetShaderiv(inst->fs, GL_COMPILE_STATUS, &result);
  if (0 == result) {
    TRAE("Failed to compile the fragment shader.");
    r = -80;
    goto error;
  }

  /* Setup the program. */
  inst->gl.glAttachShader(inst->prog, inst->vs);
  inst->gl.glAttachShader(inst->prog, inst->fs);
  inst->gl.glLinkProgram(inst->prog);

  inst->gl.glGetProgramiv(inst->prog, GL_LINK_STATUS, &result);
  if (0 == result) {
    TRAE("Failed to link the shader program.");
    r = -90;
    goto error;
  }

  /* Create orthographic projection matrix (scale to 2-unit box, centralize). */
  inst->pm[0] = 2.0f / cfg->image_width;
  inst->pm[5] = -2.0f / cfg->image_height;
  inst->pm[10] = 1.0f;

  inst->pm[12] = -1.0f;
  inst->pm[13] =  1.0f;
  inst->pm[15] =  1.0f;

  /* Create view matrix: as we draw a unit rect in the shader, we scale it by the size of the video. */
  inst->vm[0] = cfg->image_width;
  inst->vm[5] = cfg->image_height;
  inst->vm[10] = 1.0f;
  inst->vm[15] = 1.0f;

  /* Get uniforms so we can configure the shader. */
  inst->gl.glUseProgram(inst->prog);

  u_pm = inst->gl.glGetUniformLocation(inst->prog, "u_pm");
  if (-1 == u_pm) {
    TRAE("Failed to get the `u_pm` uniform.");
    r = -100;
    goto error;
  }
  
  u_vm = inst->gl.glGetUniformLocation(inst->prog, "u_vm");
  if (-1 == u_vm) {
    TRAE("Failed to get the `u_vm` uniform.");
    r = -110;
    goto error;
  }

  u_tex_y = inst->gl.glGetUniformLocation(inst->prog, "u_tex_y");
  if (-1 == u_tex_y) {
    TRAE("Failed to get the `u_tex_y` uniform.");
    r = -120;
    goto error;
  }
  
  u_tex_uv = inst->gl.glGetUniformLocation(inst->prog, "u_tex_uv");
  if (-1 == u_tex_uv) {
    TRAE("Failed to get he `u_tex_uv` uniform.");
    r = -130;
    goto error;
  }
  
  inst->gl.glUniformMatrix4fv(u_pm, 1, GL_FALSE, inst->pm);
  inst->gl.glUniformMatrix4fv(u_vm, 1, GL_FALSE, inst->vm);
  inst->gl.glUniform1i(u_tex_y, 0);
  inst->gl.glUniform1i(u_tex_uv, 1);

  inst->gl.glUseProgram(0);

  /* Finally assign the result. */
  inst->image_width = cfg->image_width;
  inst->image_height = cfg->image_height;
  inst->u_vm = u_vm;
  inst->u_pm = u_pm;

  *obj = inst;
          
 error:

  if (r < 0) {
    
    if (NULL != inst) {
      
      result = gl_gfx_destroy(inst);
      if (result < 0) {
        TRAE("After we failed to create the `opengl-gfx` graphics interop we also failed to cleanly destroy it.");
      }

      inst = NULL;
    }

    if (NULL != obj) {
      *obj = NULL;
    }
  }

  TRAP_TIMER_END(prof, "gl_gfx_create");

  return r;
}

/* ------------------------------------------------------- */

int gl_gfx_destroy(gl_gfx* ctx) {

  TRAP_TIMER_BEGIN(prof, "gl_gfx_destroy");
  
  int result = 0;
  int r = 0;

  if (NULL == ctx) {
    TRAE("Cannot destroy the `opengl-gfx` graphics objects as the given pointer is NULL.");
    result = -10;
    goto error;
  }
  
  if (0 != ctx->vs) {
    ctx->gl.glDeleteShader(ctx->vs);
    ctx->vs = 0;
  }

  if (0 != ctx->fs) {
    ctx->gl.glDeleteShader(ctx->fs);
    ctx->fs = 0;
  }

  if (0 != ctx->prog) {
    ctx->gl.glDeleteProgram(ctx->prog);
    ctx->prog = 0;
  }

  if (0 != ctx->vao) {
    ctx->gl.glDeleteVertexArrays(1, &ctx->vao);
    ctx->vao = 0;
  }

  ctx->image_width = 0;
  ctx->image_height = 0;

 error:
  
  TRAP_TIMER_END(prof, "gl_gfx_destroy");
  
  return result;
}

/* ------------------------------------------------------- */

int gl_gfx_draw(gl_gfx* ctx, uint32_t type, void* data) {

  TRAP_TIMER_BEGIN(prof, "gl_gfx_draw");

  tra_draw_opengl_nv12* info = NULL;
  int r = 0;

  if (NULL == ctx) {
    TRAE("Cannot draw as the given `gl_gfx*` is NULL.");
    r = -10;
    goto error;
  }

  if (TRA_DRAW_TYPE_OPENGL_NV12 != type) {
    TRAE("Cannot draw as we only support `TRA_DRAW_TYPE_OPENGL_NV12` for now.");
    r = -20;
    goto error;
  }

  if (NULL == data) {
    TRAE("Cannot draw as the given `data` pointer is NULL.");
    r = -30;
    goto error;
  }

  info = (tra_draw_opengl_nv12*) data;
  if (0 == info->tex_y) {
    TRAE("Cannot draw as the `tex_y` member of `tra_draw_opengl_nv12` is not set.");
    r = -40;
    goto error;
  }

  if (0 == info->tex_uv) {
    TRAE("Cannot draw as the `tex_uv` member of `tra_draw_opengl_nv12` is not set.");
    r = -50;
    goto error;
  }

  if (0 == info->width) {
    TRAE("Cannot draw as the given `width` of the `tra_draw_opengl_nv12` is 0.");
    r = -60;
    goto error;
  }

  if (0 == info->height) {
    TRAE("Cannot draw as the given `height` of the `tra_draw_opengl_nv12` is 0.");
    r = -70;
    goto error;
  }

  if (0 == ctx->vao) {
    TRAE("Cannot draw as the `vao` member is 0. Not created?");
    r = -40;
    goto error;
  }

  if (0 == ctx->prog) {
    TRAE("Cannot draw as the `prog` member is 0. Not created?");
    r = -50;
    goto error;
  }

  /* Update the view matrix. */
  ctx->vm[0] = info->width;
  ctx->vm[5] = info->height;
  ctx->vm[12] = info->x;
  ctx->vm[13] = info->y;

  ctx->gl.glActiveTexture(GL_TEXTURE0);
  ctx->gl.glBindTexture(GL_TEXTURE_2D, info->tex_y);

  ctx->gl.glActiveTexture(GL_TEXTURE1);
  ctx->gl.glBindTexture(GL_TEXTURE_2D, info->tex_uv);

  ctx->gl.glBindVertexArray(ctx->vao);
  ctx->gl.glUseProgram(ctx->prog);
  ctx->gl.glUniformMatrix4fv(ctx->u_vm, 1, GL_FALSE, ctx->vm);
  ctx->gl.glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

 error:
  
  TRAP_TIMER_END(prof, "gl_gfx_draw");
  
  return r;
}

/* ------------------------------------------------------- */

static const char* graphics_get_name() {
  return "opengl-gfx";
}

/* ------------------------------------------------------- */

static const char* graphics_get_author() {
  return "roxlu";
}

/* ------------------------------------------------------- */

static int graphics_create(
  tra_graphics_settings* cfg,
  void* settings,
  tra_graphics_object** obj
)
{
  gl_gfx* inst = NULL;
  int result = 0;
  int r = 0;
  
  if (NULL == cfg) {
    TRAE("Cannot create the `opengl-gfx` graphics module as the given `tra_graphics_settings*` is NULL.");
    r = -10;
    goto error;
  }

  if (NULL == obj) {
    TRAE("Cannot create the `opengl-gfx` graphics module as the given result pointer is NULL.");
    r = -20;
    goto error;
  }

  r = gl_gfx_create(cfg, settings, &inst);
  if (r < 0) {
    TRAE("Failed to create the `opengl-gfx` graphics module.");
    r = -30;
    goto error;
  }

  /* And assing the result. */
  *obj = (tra_graphics_object*) inst;

 error:

  if (r < 0) {
    if (NULL != inst) {
      result = graphics_destroy((tra_graphics_object*)inst);
      if (result < 0) {
        TRAE("After we've failed to create the `opengl-gfx` graphics module we also failed to cleanly destroy it.");
      }
    }
  }

  return r;
}

/* ------------------------------------------------------- */

static int graphics_destroy(tra_graphics_object* obj) {

  int r = 0;

  if (NULL == obj) {
    TRAE("Cannot destroy the given `opengl-gfx` graphics module as it's NULL.");
    return -10;
  }

  r = gl_gfx_destroy((gl_gfx*) obj);
  if (r < 0) {
    TRAE("Failed to cleanly destroy the `opengl-gfx` graphics module.");
    return -20;
  }

  return r;
}

/* ------------------------------------------------------- */

static int graphics_draw(tra_graphics_object* obj, uint32_t type, void* data) {

  int r = 0;

  if (NULL == obj) {
    TRAE("Cannot upload as the given `tra_graphics_object*` is NULL.");
    return -10;
  }

  r = gl_gfx_draw((gl_gfx*) obj, type, data);
  if (r < 0) {
    TRAE("Failed to draw.");
    return -20;
  }

  return r;
}

/* ------------------------------------------------------- */

/*
  This is called when `tra_core` is created and the registry
  scans the module directory. This should register this OpenGL
  graphics module.
*/
int tra_load(tra_registry* reg) {

  int r = 0;

  if (NULL == reg) {
    TRAE("Cannot load the OpenGL graphics module as the given `tra_registry*` is NULL.");
    return -10;
  }

  r = tra_registry_add_graphics_api(reg, &graphics_api);
  if (r < 0) {
    TRAE("Failed to register the `opengl-gfx` graphics api.");
    return -30;
  }

  return 0;
}

/* ------------------------------------------------------- */

static tra_graphics_api graphics_api = {
  .get_name = graphics_get_name,
  .get_author = graphics_get_author,
  .create = graphics_create,
  .destroy = graphics_destroy,
  .draw = graphics_draw,
};

/* ------------------------------------------------------- */

