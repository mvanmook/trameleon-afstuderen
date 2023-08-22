/* ------------------------------------------------------- */

#define GL_GLEXT_PROTOTYPES
#include <GL/glcorearb.h>

#include <stdlib.h>
#include <tra/modules/opengl/opengl-converter.h>
#include <tra/modules/opengl/opengl-api.h>
#include <tra/modules/opengl/opengl.h>
#include <tra/profiler.h>
#include <tra/registry.h>
#include <tra/module.h>
#include <tra/types.h>
#include <tra/log.h>

/* ------------------------------------------------------- */

static const char* NV12_VS = ""
    "#version 330\n"
    ""
    "uniform mat4 u_vm;"
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
    "  gl_Position = u_vm * vec4(pos[gl_VertexID], 0.0, 1.0); "
     " v_tex = tex[gl_VertexID];"
    "}"
  "";

static const char* NV12_FS_LUMA = ""
  "#version 330\n"
  ""
  "uniform sampler2D u_tex;"
  ""
  "in vec2 v_tex;"
  ""
  "layout (location = 0) out vec4 out_luma;"
  ""
  "void main() {"
  "  vec4 tc = texture(u_tex, v_tex);"
  "  float luma = (tc.r * 0.257) + (tc.g * 0.504) + (tc.b * 0.098) + 0.0625; "
  "  out_luma.r = luma; "
  "}"
  "";

static const char* NV12_FS_CHROMA = ""
  "#version 330\n"
  ""
  "uniform sampler2D u_tex;"
  ""
  "in vec2 v_tex;"
  ""
  "layout (location = 0) out vec4 out_uv;"
  ""
  "void main() { "
  ""
  "  vec4 tc = texture(u_tex, v_tex);"
  "  int is_odd = (int(gl_FragCoord.x) & 0x01);"
  ""
  "  if (0 == is_odd) { " 
  "    out_uv.r = -(tc.r * 0.148) - (tc.g * 0.291) + (tc.b * 0.439) + 0.5;" /* Cb */
  "  } "
  "  else { "
  "    out_uv.r = (tc.r * 0.439) - (tc.g * 0.368) - (tc.b * 0.071) + 0.5;" /* Cr */
  "  }"
  "}"
  "";

/* ------------------------------------------------------- */

struct tra_opengl_converter {

  tra_converter_callbacks callbacks;
  
  /* Convert */
  uint32_t input_type;
  uint32_t input_width;
  uint32_t input_height;
  uint32_t output_width;
  uint32_t output_height;
  uint32_t output_type;
  
  /* OpenGL */
  tra_opengl_funcs gl;
  GLuint vao;                  /* We need a vertex array object even though we don't use a VBO/IBO. */
  GLuint vs;
  GLuint prog_luma;
  GLuint prog_chroma;
  GLuint fs_luma;
  GLuint fs_chroma;
  GLuint* fbo_ids;             /* For each output texture we create its own FBO. We could have created one FBO and played with attachments, but using one FBO, one texture makes the code easier to understand. */
  GLuint* texture_ids;         /* The textures that will store the converted results. */
  uint32_t texfbo_count;       /* We allocate one or more output buffers where we store the converted textures. This indicates how many FBOs and textures we've allocated. */
  uint32_t texfbo_index;       /* The index into the `texture_ids` and `fbo_ids` varray that we use when converting. Is incremented each for each convert. */
  float vm[16];                /* View matrix, can be used to e.g. rotate the image. */
};

/* ------------------------------------------------------- */

static tra_converter_api converter_api;

/* ------------------------------------------------------- */

static const char* converter_get_name();
static const char* converter_get_author();
static int converter_create(tra_converter_settings* cfg, void* settings, tra_converter_object** obj);
static int converter_destroy(tra_converter_object* obj);
static int converter_convert(tra_converter_object* obj, uint32_t type, void* data);

/* ------------------------------------------------------- */

/*
  
  This function will allocate and initialize the OpenGL based
  converter.  When we have to convert the pixel format we will
  allocate a texture and framebuffer that we use to apply a
  conversion shader.
  
 */
int tra_opengl_converter_create(
  tra_converter_settings* cfg,
  tra_opengl_converter_settings* settings,
  tra_opengl_converter** ctx
)
{
  tra_opengl_converter* inst = NULL;
  GLenum status = GL_NONE;
  GLint loc_tex = -1;
  GLint loc_vm = -1;
  uint32_t i = 0;
  int result = 0;
  int r = 0;

  /* ----------------------------------------------- */
  /* Validate                                        */
  /* ----------------------------------------------- */

  if (NULL == cfg) {
    TRAE("Cannot create the OpenGL converter as the given `tra_converter_settings*` is NULL.");
    r = -10;
    goto error;
  }

  if (0 == cfg->input_width) {
    TRAE("Cannot create the OpenGL converter as the given `tra_converter_settings::input_width` is 0.");
    r = -20;
    goto error;
  }
  
  if (0 == cfg->input_height) {
    TRAE("Cannot create the OpenGL converter as the given `tra_converter_settings::input_height` is 0.");
    r = -20;
    goto error;
  }

  if (0 == cfg->output_width) {
    TRAE("Cannot create the OpenGL converter as the given `tra_converter_settings::output_width` is 0.");
    r = -20;
    goto error;
  }

  if (0 == cfg->output_height) {
    TRAE("Cannot create the OpenGL converter as the given `tra_converter_settings::output_height` is 0.");
    r = -20;
    goto error;
  }

  if (NULL == cfg->callbacks.on_converted) {
    TRAE("Cannot create the OpenGL converter as the given `tra_converter_settings::callbacks::on_converted` hasn't been set.");
    r = -40;
    goto error;
  }

  /*
    Currently the functionality that the converter provides is
    limited to converting a RGB texture into a NV12 buffer.
    Below we will make sure that the given settings match the
    features we provide.
  */

  /* --- begin: check supported features. ---*/
  
  if (TRA_CONVERT_TYPE_OPENGL_RGB24 != cfg->input_type) {
    TRAE("Cannot create the OpenGL converter as we currently only support device images (e.g. textures).");
    r = -20;
    goto error;
  }
  
  if (TRA_CONVERT_TYPE_OPENGL_NV12_SINGLE_TEXTURE != cfg->output_type) {
    TRAE("Cannot create the OpenGL converter as we currently only support device images (e.g. textures).");
    r = -20;
    goto error;
  }
  
  if (TRA_IMAGE_FORMAT_RGB != cfg->input_format) {
    TRAE("Cannot create the OpenGL converter as we currently only support RGB input buffers.");
    r = -20;
    goto error;
  }
  
  if (TRA_IMAGE_FORMAT_NV12 != cfg->output_format) {
    TRAE("Cannot create the OpenGL converter as we currently only support NV12 output buffers.");
    r = -30;
    goto error;
  }
  
  /* --- end: check supported features. ---*/ 

  if (NULL == ctx) {
    TRAE("Cannot create the OpenGL converter as the given `tra_opengl_converter**` is NULL.");
    r = -20;
    goto error;
  }

  if (NULL != *ctx) {
    TRAE("Cannot create the OpenGL converter as the given `*(tra_opengl_converter**)` is NOT NULL. Did you already create it? Or maybe forgot to initialized to NULL?");
    r = -30;
    goto error;
  }

  if (NULL == settings) {
    TRAE("Cannot create the OpenGL converter as the given `tra_opengl_converter_settings*` is NULL.");
    r = -40;
    goto error;
  }

  if (NULL == settings->gl_api) {
    TRAE("Cannot create the OpenGL converter as the given `tra_opengl_converter_settings::gl_api` is NULL.");
    r = -50;
    goto error;
  }

  inst = calloc(1, sizeof(tra_opengl_converter));
  if (NULL == inst) {
    TRAE("Cannot create the OpenGL converter as we failed to allocate the context.");
    r = -40;
    goto error;
  }

  r = settings->gl_api->load_functions(&inst->gl);
  if (r < 0) {
    TRAE("Cannot create the OpenGL converter as we failed to load the OpenGL functions. Did you make your OpenGL context current?");
    r = -50;
    goto error;
  }

  /* ----------------------------------------------- */
  /* Create GL resources.                            */
  /* ----------------------------------------------- */

  /* Setup */
  inst->callbacks = cfg->callbacks;
  inst->input_type = cfg->input_type;
  inst->input_width = cfg->input_width;
  inst->input_height = cfg->input_height;
  inst->output_type = cfg->output_type;
  inst->output_width = cfg->output_width;
  inst->output_height = cfg->output_height;

  /* Create GL handles */
  inst->gl.glGenVertexArrays(1, &inst->vao);
  if (0 == inst->vao) {
    TRAE("Cannot create the OpenGL converter as we failed to create the vertex array object.");
    r = -55;
    goto error;
  }
  
  inst->vs = inst->gl.glCreateShader(GL_VERTEX_SHADER);
  if (0 == inst->vs) {
    TRAE("Cannot create the OpenGL converter as we failed to create the NV12 vertex shader.");
    r = -60;
    goto error;
  }
                                          
  inst->fs_luma = inst->gl.glCreateShader(GL_FRAGMENT_SHADER);
  if (0 == inst->fs_luma) {
    TRAE("Cannot create the OpenGL converter as we failed to create the NV12 fragment shader for the luma.");
    r = -70;
    goto error;
  }

  inst->fs_chroma = inst->gl.glCreateShader(GL_FRAGMENT_SHADER);
  if (0 == inst->fs_chroma) {
    TRAE("Cannot create the OpenGL converter as we failed to create the NV12 fragment shader for the chroma.");
    r = -80;
    goto error;
  }

  inst->prog_luma = inst->gl.glCreateProgram();
  if (0 == inst->prog_luma) {
    TRAE("Cannot create the OpenGL converter as we failed to create the NV12 luma program.");
    r = -90;
    goto error;
  }

  inst->prog_chroma = inst->gl.glCreateProgram();
  if (0 == inst->prog_chroma) {
    TRAE("Cannot create the OpenGL converter as we failed to create the NV12 chroma program.");
    r = -100;
    goto error;
  }

  /* Setup vertex shader. */
  inst->gl.glShaderSource(inst->vs, 1, &NV12_VS, NULL);
  inst->gl.glCompileShader(inst->vs);

  inst->gl.glGetShaderiv(inst->vs, GL_COMPILE_STATUS, &result);
  if (0 == result) {
    TRAE("Cannot create the OpenGL converter as we failed to compile the NV12 vertex shader.");
    r = -110;
    goto error;
  }

  /* Setup luma fragment shader */
  inst->gl.glShaderSource(inst->fs_luma, 1, &NV12_FS_LUMA, NULL);
  inst->gl.glCompileShader(inst->fs_luma);

  inst->gl.glGetShaderiv(inst->fs_luma, GL_COMPILE_STATUS, &result);
  if (0 == result) {
    TRAE("Cannot create the OpenGL converter as we failed to compile the NV12 luma fragment shader.");
    r = -120;
    goto error;
  }

  /* Setup chroma fragment shader */
  inst->gl.glShaderSource(inst->fs_chroma, 1, &NV12_FS_CHROMA, NULL);
  inst->gl.glCompileShader(inst->fs_chroma);

  inst->gl.glGetShaderiv(inst->fs_chroma, GL_COMPILE_STATUS, &result);
  if (0 == result) {
    TRAE("Cannot create the OpenGL converter as we failed to compile the NV12 chroma fragment shader.");
    r = -130;
    goto error;
  }

  /* Link the luma program */
  inst->gl.glAttachShader(inst->prog_luma, inst->vs);
  inst->gl.glAttachShader(inst->prog_luma, inst->fs_luma);
  inst->gl.glLinkProgram(inst->prog_luma);
  
  inst->gl.glGetProgramiv(inst->prog_luma, GL_LINK_STATUS, &result);
  if (0 == result) {
    TRAE("Cannot create the OpenGL converter as we failed to link the luma program.");
    r = -140;
    goto error;
  }

  /* Link the chroma program */
  inst->gl.glAttachShader(inst->prog_chroma, inst->vs);
  inst->gl.glAttachShader(inst->prog_chroma, inst->fs_chroma);
  inst->gl.glLinkProgram(inst->prog_chroma);
  
  inst->gl.glGetProgramiv(inst->prog_chroma, GL_LINK_STATUS, &result);
  if (0 == result) {
    TRAE("Cannot create the OpenGL converter as we failed to link the chroma program.");
    r = -150;
    goto error;
  }

  /* Create view matrix; can be used to flip or rotate; currently we used an identity matrix . */
  inst->vm[0] = 1.0f;
  inst->vm[5] = 1.0f;
  inst->vm[10] = 1.0f;
  inst->vm[15] = 1.0f;
  
  /* Set the uniforms. */
  inst->gl.glUseProgram(inst->prog_luma);
  loc_tex = inst->gl.glGetUniformLocation(inst->prog_luma, "u_tex");
  loc_vm = inst->gl.glGetUniformLocation(inst->prog_luma, "u_vm");

  inst->gl.glUniform1i(loc_tex, 0);
  inst->gl.glUniformMatrix4fv(loc_vm, 1, GL_FALSE, inst->vm);

  inst->gl.glUseProgram(inst->prog_chroma);
  loc_tex = inst->gl.glGetUniformLocation(inst->prog_chroma, "u_tex");
  loc_vm = inst->gl.glGetUniformLocation(inst->prog_chroma, "u_vm");

  inst->gl.glUniform1i(loc_tex, 0);
  inst->gl.glUniformMatrix4fv(loc_vm, 1, GL_FALSE, inst->vm);
  
  inst->gl.glUseProgram(0);

  /* Create the output textures and FBOs. */
  inst->texfbo_count = 4;
  inst->texfbo_index = 0;
  
  inst->texture_ids = calloc(1, sizeof(GLuint) * inst->texfbo_count);
  if (NULL == inst->texture_ids) {
    TRAE("Cannot create the OpenGL converter as we failed to allocate the output textures.");
    r = -160;
    goto error;
  }

  inst->fbo_ids = calloc(1, sizeof(GLuint) * inst->texfbo_count);
  if (NULL == inst->fbo_ids) {
    TRAE("Cannot create the OpenGL converter as we failed to allocate the FBOs ids.");
    r = -170;
    goto error;
  }
  
  inst->gl.glGenTextures(inst->texfbo_count, inst->texture_ids);
  inst->gl.glGenFramebuffers(inst->texfbo_count, inst->fbo_ids);

  /* Setup the output textures. */
  for (i = 0; i < inst->texfbo_count; ++i) {

    if (0 == inst->texture_ids[i]) {
      TRAE("Cannot create the OpenGL converter, we failed to create a texture for index %u.", i);
      r = -170;
      goto error;
    }

    if (0 == inst->fbo_ids[i]) {
      TRAE("Cannot create the OpenGL converter, we failed to allocate a FBO for index %u.", i);
      r = -180;
      goto error;
    }

    /* Setup the texture */    
    inst->gl.glBindTexture(GL_TEXTURE_2D, inst->texture_ids[i]);
    inst->gl.glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, cfg->output_width, cfg->output_height * 1.5, 0, GL_RED, GL_UNSIGNED_BYTE, NULL);
    inst->gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    inst->gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    /* Attach the texture to this FBO. */
    inst->gl.glBindFramebuffer(GL_DRAW_FRAMEBUFFER, inst->fbo_ids[i]);
    inst->gl.glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, inst->texture_ids[i], 0);

    status = inst->gl.glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER);
    if (GL_FRAMEBUFFER_COMPLETE != status) {
      TRAE("Cannot create the OpenGL converter, we failed to setup the FBO.");
      r = -190;
      goto error;
    }
    
    inst->gl.glBindTexture(GL_TEXTURE_2D, 0);
    inst->gl.glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
  }

  /* Finally assign. */
  *ctx = inst;

 error:

  if (r < 0) {
    
    if (NULL != inst) {
      result = tra_opengl_converter_destroy(inst);
      inst = NULL;
    }
    
    if (NULL != ctx) {
      *ctx = NULL;
    }
  }
  
  return r;
}

/* ------------------------------------------------------- */
  
int tra_opengl_converter_destroy(tra_opengl_converter* ctx) {

  int result = 0;
  int r = 0;

  if (NULL == ctx) {
    TRAE("Cannot destroy the OpenGL conveter as it's NULL.");
    result = -10;
    goto error;
  }

  TRAE("@todo delete FBOs and textures");

  /* Cleanup GL resources. */
  if(0 != ctx->vao) {
    ctx->gl.glDeleteVertexArrays(1, &ctx->vao);
  }
  
  if (0 != ctx->vs) {
    ctx->gl.glDeleteShader(ctx->vs);
  }

  if (0 != ctx->fs_luma) {
    ctx->gl.glDeleteShader(ctx->fs_luma);
  }

  if (0 != ctx->fs_chroma) {
    ctx->gl.glDeleteShader(ctx->fs_chroma);
  }

  if (0 != ctx->prog_luma) {
    ctx->gl.glDeleteProgram(ctx->prog_luma);
  }

  if (0 != ctx->prog_chroma) {
    ctx->gl.glDeleteProgram(ctx->prog_chroma);
  }

  if (NULL != ctx->fbo_ids) {
    ctx->gl.glDeleteFramebuffers(ctx->texfbo_count, ctx->fbo_ids);
  }

  if (NULL != ctx->texture_ids) {
    ctx->gl.glDeleteTextures(ctx->texfbo_count, ctx->texture_ids);
  }

  ctx->callbacks.on_converted = NULL;
  ctx->callbacks.user = NULL;

  ctx->input_width = 0;
  ctx->input_height = 0;
  ctx->output_width = 0;
  ctx->output_height = 0;

  ctx->vao = 0;
  ctx->vs = 0;
  ctx->prog_luma = 0;
  ctx->prog_chroma = 0;
  ctx->fs_luma = 0;
  ctx->fs_chroma = 0;

  ctx->fbo_ids = NULL;
  ctx->texture_ids = NULL;
  
 error:
  return result;
}

/* ------------------------------------------------------- */

/*

  This function will convert the given input texture into the
  output texture type that was used to configure the
  `tra_opengl_converter` instance. Currently this converter can
  only output NV12 as a single texture.  We will read from the
  given input texture and write into one of the `texture_ids`
  buffers using a render-to-texture pipeline.
  
 */
int tra_opengl_converter_convert(
  tra_opengl_converter* ctx,
  uint32_t type,
  void* data
)
{
  tra_convert_opengl_nv12_single_texture out_mem = { 0 }; 
  tra_convert_opengl_rgb24* in_mem = NULL;
  GLenum bufs[] = { GL_COLOR_ATTACHMENT0 };
  uint32_t dx = 0;
  int r = 0;

  if (NULL == ctx) {
    TRAE("Cannot convert as the given `tra_opengl_converter*` is NULL.");
    r = -10;
    goto error;
  }

  if (TRA_CONVERT_TYPE_OPENGL_NV12_SINGLE_TEXTURE != ctx->output_type) {
    TRAE("Cannot convert the given input type as we currently only  support NV12 single texture outputs. When we're going to add support for alternative output formats, make sure to update the output stride too! (see our call to `on_converted()` below.");
    r = -20;
    goto error;
  }

  if (TRA_CONVERT_TYPE_OPENGL_RGB24 != ctx->input_type) {
    TRAE("Cannot convert as we currently only support `TRA_CONVERT_TYPE_OPENGL_RGB24` input types.");
    r = -30;
    goto error;
  }

  if (type != ctx->input_type) {
    TRAE("Cannot convert the given input as it differs from the input type that was used to create the converter.");
    r = -40;
    goto error;
  }

  if (0 == ctx->output_width) {
    TRAE("Cannot convert the given input as the `tra_opengl_converted::output_width` member is 0. We need this value to determine the output width.");
    r = -50;
    goto error;
  }

  if (TRA_CONVERT_TYPE_OPENGL_RGB24 != type) {
    TRAE("Cannot convert as the given type is not supported.");
    r = -20;
    goto error;
  }

  if (NULL == data) {
    TRAE("Cannot convert as the given `data` is NULL.");
    r = -30;
    goto error;
  }

  in_mem = (tra_convert_opengl_rgb24*) data;
  if (0 == in_mem->tex_id) {
    TRAE("Cannot convert as the given texture id is 0 which means we received an invalid texture.");
    r = -40;
    goto error;
  }

  dx = ctx->texfbo_index;
  
  /* Bind the destination buffers and VAO. */
  ctx->gl.glBindVertexArray(ctx->vao);
  ctx->gl.glBindFramebuffer(GL_DRAW_FRAMEBUFFER, ctx->fbo_ids[dx]);
  ctx->gl.glDrawBuffers(1, bufs);
   
  /* Bind the texture we read from. */
  ctx->gl.glActiveTexture(GL_TEXTURE0);
  ctx->gl.glBindTexture(GL_TEXTURE_2D, in_mem->tex_id);

  /* Render luma */
  ctx->gl.glViewport(0, 0, ctx->output_width, ctx->output_height);
  ctx->gl.glUseProgram(ctx->prog_luma);
  ctx->gl.glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

  /* Render chroma-plane */
  ctx->gl.glViewport(0, ctx->output_height, ctx->output_width, ctx->output_height * 0.5);
  ctx->gl.glUseProgram(ctx->prog_chroma);
  ctx->gl.glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  
  ctx->gl.glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

  /* Notify the user with the converted texture. */
  out_mem.tex_id = ctx->texture_ids[dx];
  out_mem.tex_stride = ctx->output_width;

  ctx->callbacks.on_converted(
    ctx->output_type,
    &out_mem,
    ctx->callbacks.user
  );
  
  /* Next call we will use this destination texture and FBO */
  ctx->texfbo_index = (ctx->texfbo_index + 1) % ctx->texfbo_count;
  
 error:
  return r;
}

/* ------------------------------------------------------- */

static const char* converter_get_name() {
  return "glconverter";
}

/* ------------------------------------------------------- */

static const char* converter_get_author() {
  return "roxlu";
}

/* ------------------------------------------------------- */

static int converter_create(
  tra_converter_settings* cfg,
  void* settings,
  tra_converter_object** obj
)
{
  tra_opengl_converter* inst = NULL;
  int status = 0;
  int r = 0;
  
  if (NULL == cfg) {
    TRAE("Cannot create the converter, given `tra_converter_settings*` is NULL");
    r = -10;
    goto error;
  }

  if (NULL == obj) {
    TRAE("Cannot create the converter as the given output `tra_converter_object**` is NULL.");
    r = -20;
    goto error;
  }

  if (NULL != *(obj)) {
    TRAE("Cannot create the converter as the given `*tra_converter_object**` is not NULL. Already created? Not initialized to NULL?");
    r = -30;
    goto error;
  }

  r = tra_opengl_converter_create(cfg, settings, &inst);
  if (r < 0) {
    TRAE("Cannot create the converter, the call to `nv_converter_create()` returned an error.");
    r = -40;
    goto error;
  }

  /* Finally assign */
  *obj = (tra_converter_object*) inst;

 error:

  if (r < 0) {
    
    if (NULL != inst) {
      status = tra_opengl_converter_destroy(inst);
      inst = NULL;
    }

    /* Make sure that the output is set to NULL when an error occured. */
    if (NULL != obj) {
      *obj = NULL;
    }
  }
  
  return r;
}

/* ------------------------------------------------------- */

static int converter_destroy(tra_converter_object* obj) {

  int r = 0;
  
  if (NULL == obj) {
    TRAE("Cannot destroy the converter as it's NULL.");
    return -10;
  }

  r = tra_opengl_converter_destroy((tra_opengl_converter*) obj);
  if (r < 0) {
    TRAE("Failed to cleanly destroy the converter.");
    return -20;
  }

  return r;
}

/* ------------------------------------------------------- */

static int converter_convert(tra_converter_object* obj, uint32_t type, void* data) {
  return tra_opengl_converter_convert((tra_opengl_converter*) obj, type, data);
}

/* ------------------------------------------------------- */

/* Register the OpenGL converter with the registry. */
int tra_load(tra_registry* reg) {

  int r = 0;

  if (NULL == reg) {
    TRAE("Cannot register the OpenGL converter as the given `tra_registry*` is NULL.");
    return -10;
  }

  r = tra_registry_add_converter_api(reg, &converter_api);
  if (r < 0) {
    TRAE("Failed to register the `glconverter` api.");
    return -20;
  }

  return r;
}

/* ------------------------------------------------------- */
  
static tra_converter_api converter_api = {
  .get_name = converter_get_name,
  .get_author = converter_get_author,
  .create = converter_create,
  .destroy = converter_destroy,
  .convert = converter_convert,
};

/* ------------------------------------------------------- */
 
