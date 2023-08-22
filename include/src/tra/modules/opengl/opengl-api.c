/* ------------------------------------------------------- */

#define GL_GLEXT_PROTOTYPES
#include <GL/glcorearb.h>

#include <stdlib.h>
#include <tra/modules/opengl/opengl-api.h>
#include <tra/registry.h>
#include <tra/utils.h>
#include <tra/log.h>

/* ------------------------------------------------------- */

static int gl_load_func(void* lib, const char* name, int* hasError, void** result);

/* ------------------------------------------------------- */

static tra_opengl_api opengl_api;

/* ------------------------------------------------------- */

/*
  This is the `laod_functions()` implementation of the
  `tra_opengl_api`. We will load the GL function form the OpenGL
  context which is made "current" at the the that you call this
  function. See `opengl-api.h` for more info about function
  loading.
 */
static int gl_load_functions(tra_opengl_funcs* funcs) {

  int has_error = 0;
  void* lib = NULL;
  int r = 0;

  if (NULL == funcs) {
    TRAE("Cannot load the OpenGL functions as the given `tra_opengl_funcs` is NULL.");
    r = -10;
    goto error;
  }

  r = tra_load_module("libGL.so", &lib);
  if (r < 0) {
    TRAE("Failed to load the GL module.");
    r = -20;
    goto error;
  }

  gl_load_func(lib, "glActiveTexture", &has_error, (void**) &funcs->glActiveTexture);
  gl_load_func(lib, "glAttachShader", &has_error, (void**) &funcs->glAttachShader);
  gl_load_func(lib, "glBindBuffer", &has_error, (void**) &funcs->glBindBuffer);
  gl_load_func(lib, "glBindFramebuffer", &has_error, (void**) &funcs->glBindFramebuffer);
  gl_load_func(lib, "glBindTexture", &has_error, (void**) &funcs->glBindTexture);
  gl_load_func(lib, "glBindVertexArray", &has_error, (void**) &funcs->glBindVertexArray);
  gl_load_func(lib, "glBlitFramebuffer", &has_error, (void**) &funcs->glBlitFramebuffer);
  gl_load_func(lib, "glBufferData", &has_error, (void**) &funcs->glBufferData);
  gl_load_func(lib, "glCheckFramebufferStatus", &has_error, (void**) &funcs->glCheckFramebufferStatus);
  gl_load_func(lib, "glClear", &has_error, (void**) &funcs->glClear);
  gl_load_func(lib, "glCompileShader", &has_error, (void**) &funcs->glCompileShader);
  gl_load_func(lib, "glCreateProgram", &has_error, (void**) &funcs->glCreateProgram);
  gl_load_func(lib, "glCreateShader", &has_error, (void**) &funcs->glCreateShader);
  gl_load_func(lib, "glDeleteBuffers", &has_error, (void**) &funcs->glDeleteBuffers);
  gl_load_func(lib, "glDeleteFramebuffers", &has_error, (void**) &funcs->glDeleteFramebuffers);
  gl_load_func(lib, "glDeleteProgram", &has_error, (void**) &funcs->glDeleteProgram);
  gl_load_func(lib, "glDeleteRenderbuffers", &has_error, (void**) &funcs->glDeleteRenderbuffers);
  gl_load_func(lib, "glDeleteShader", &has_error, (void**) &funcs->glDeleteShader);
  gl_load_func(lib, "glDeleteTextures", &has_error, (void**) &funcs->glDeleteTextures);
  gl_load_func(lib, "glDeleteVertexArrays", &has_error, (void**) &funcs->glDeleteVertexArrays);
  gl_load_func(lib, "glDisable", &has_error, (void**) &funcs->glDisable);
  gl_load_func(lib, "glDrawArrays", &has_error, (void**) &funcs->glDrawArrays);
  gl_load_func(lib, "glDrawBuffer", &has_error, (void**) &funcs->glDrawBuffer);
  gl_load_func(lib, "glDrawBuffers", &has_error, (void**) &funcs->glDrawBuffers);
  gl_load_func(lib, "glEnable", &has_error, (void**) &funcs->glEnable);
  gl_load_func(lib, "glFramebufferRenderbuffer", &has_error, (void**) &funcs->glFramebufferRenderbuffer);
  gl_load_func(lib, "glFramebufferTexture2D", &has_error, (void**) &funcs->glFramebufferTexture2D);
  gl_load_func(lib, "glGenBuffers", &has_error, (void**) &funcs->glGenBuffers);
  gl_load_func(lib, "glGenFramebuffers", &has_error, (void**) &funcs->glGenFramebuffers);
  gl_load_func(lib, "glGenRenderbuffers", &has_error, (void**) &funcs->glGenRenderbuffers);
  gl_load_func(lib, "glGenTextures", &has_error, (void**) &funcs->glGenTextures);
  gl_load_func(lib, "glGenVertexArrays", &has_error, (void**) &funcs->glGenVertexArrays);
  gl_load_func(lib, "glGetIntegerv", &has_error, (void**) &funcs->glGetIntegerv);
  gl_load_func(lib, "glGetProgramiv", &has_error, (void**) &funcs->glGetProgramiv);
  gl_load_func(lib, "glGetShaderiv", &has_error, (void**) &funcs->glGetShaderiv);
  gl_load_func(lib, "glGetUniformLocation", &has_error, (void**) &funcs->glGetUniformLocation);
  gl_load_func(lib, "glLinkProgram", &has_error, (void**) &funcs->glLinkProgram);
  gl_load_func(lib, "glReadBuffer", &has_error, (void**) &funcs->glReadBuffer);
  gl_load_func(lib, "glRenderbufferStorage", &has_error, (void**) &funcs->glRenderbufferStorage);
  gl_load_func(lib, "glScissor", &has_error, (void**) &funcs->glScissor);
  gl_load_func(lib, "glShaderSource", &has_error, (void**) &funcs->glShaderSource);
  gl_load_func(lib, "glTexImage2D", &has_error, (void**) &funcs->glTexImage2D);
  gl_load_func(lib, "glTexParameteri", &has_error, (void**) &funcs->glTexParameteri);
  gl_load_func(lib, "glTexSubImage2D", &has_error, (void**) &funcs->glTexSubImage2D);
  gl_load_func(lib, "glUniform1i", &has_error, (void**) &funcs->glUniform1i);
  gl_load_func(lib, "glUniformMatrix4fv", &has_error, (void**) &funcs->glUniformMatrix4fv);
  gl_load_func(lib, "glUseProgram", &has_error, (void**) &funcs->glUseProgram);
  gl_load_func(lib, "glViewport", &has_error, (void**) &funcs->glViewport);

  if (1 == has_error) {
    TRAE("Something went wrong while loading the GL functions.");
    r = -30;
    goto error;
  }

 error:

  return r;
}

/* ------------------------------------------------------- */

/*
  
  This function will load the OpenGL function that is passed as
  `name`. The `lib` is a pointer to the opened shared library
  from which we will load the function. When an error occurs we
  will set `hasError` to 1.
  
 */
static int gl_load_func(
  void* lib,
  const char* name,
  int* hasError,
  void** result
)
{

  int r = 0;

  if (NULL == lib) {
    TRAE("Cannot load a GL function as the given lib handle is NULL.");
    r = -5;
    goto error;
  }
  
  if (NULL == name) {
    TRAE("Cannot load the GL function as the given `name` is empty.");
    r = -10;
    goto error;
  }

  if (NULL == hasError) {
    TRAE("Cannot load the GL function (%s) as `hasError` is NULL. (exiting).", name);
    exit(EXIT_FAILURE);
  }

  if (NULL == result) {
    TRAE("Cannot load the GL function as the destination is NULL.");
    r = -20;
    goto error;
  }

  r = tra_load_function(lib, name, result);
  if (r < 0) {
    TRAE("Failed to load `%s`", name);
    r = -20;
    goto error;
  }

 error:
  
  if (r < 0) {
    if (NULL != hasError) {
      *hasError |= 1;
    }
  }

  return r;
}

/* ------------------------------------------------------- */

int tra_load(tra_registry* reg) {
  
  int r = 0;

  if (NULL == reg) {
    TRAE("Cannot load the OpenGL API as the given `tra_registry*` is NULL.");
    return -10;
  }

  r = tra_registry_add_api(reg, "opengl", &opengl_api);
  if (r < 0) {
    TRAE("Failed to add the `opengl` API.");
    return -20;
  }

  return 0;
}

/* ------------------------------------------------------- */

static tra_opengl_api opengl_api = {
  .load_functions = gl_load_functions,
};

/* ------------------------------------------------------- */
