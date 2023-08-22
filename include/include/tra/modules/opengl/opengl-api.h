#ifndef TRA_OPENGL_API_H
#define TRA_OPENGL_API_H
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
  
  OPENGL
  ======

  GENERAL INFO:

    The `opengl` layer is experimental and we need to get our
    hands dirty to see if this provides what we need. See the
    `livepeer-documentation` repository and specifically the
    `research-graphics.md` file. My idea is to add a GL function
    loader and potentially some helper functions which make it
    simple to provide graphics interop with encoders and
    decoders.

    Though to create a OpenGL function loader (note: not a window
    or GL context creation library), takes more research and time
    to implement. When we're going to add this we probably want
    to use the same approach as [GLAD][0] does. This means we
    parse the `gl.xml` spec file and generate a header + loader.

    We need a loader to be able to load function on
    request. Loading GL functions required a GL context to be
    "current". As we do not create the context (this is most
    likely already created by the application of the user) we
    load the functions and assume that the user had made the GL
    context current. For now we also only load functions that are
    used by our interop layer. Read the next paragraph which
    describes how to add new functions that we should load.

  ADDING NEW FUNCTIONS 

    - Open [glcorearb.h][2] and copy the function pointers to `tra_opengl_funcs`.
    - Load [this][1] helper HTML page in a browser.
    - Paste the `gl*` members from the `tra_opengl_funcs` struct into the first textarea.
    - Press `generate` button.
    - Copy and paste the output into `gl_load_functions()`. 

  REFERENCES:

    [0]: https://github.com/Dav1dde/glad/ "GLAD OpenGL function loader."
    [1]: https://gist.github.com/roxlu/1d089783f7ed215d8832db41781a9480 "HTML page that you can use to easily create the OpenGL function calls for `opengl-api.c`. "
    [2]: https://gist.github.com/roxlu/938751dd47e21f15c392a02fa7060216 "glcorearb.h"
    
 */

/* ------------------------------------------------------- */

typedef struct tra_opengl_funcs tra_opengl_funcs;
typedef struct tra_opengl_api tra_opengl_api;

/* ------------------------------------------------------- */

struct tra_opengl_funcs {
  PFNGLACTIVETEXTUREPROC glActiveTexture;
  PFNGLATTACHSHADERPROC glAttachShader;
  PFNGLBINDBUFFERPROC glBindBuffer;
  PFNGLBINDFRAMEBUFFERPROC glBindFramebuffer;
  PFNGLBINDTEXTUREPROC glBindTexture;
  PFNGLBINDVERTEXARRAYPROC glBindVertexArray;
  PFNGLBLITFRAMEBUFFERPROC glBlitFramebuffer;
  PFNGLBUFFERDATAPROC glBufferData;
  PFNGLCHECKFRAMEBUFFERSTATUSPROC glCheckFramebufferStatus;
  PFNGLCLEARPROC glClear;
  PFNGLCOMPILESHADERPROC glCompileShader;
  PFNGLCREATEPROGRAMPROC glCreateProgram;
  PFNGLCREATESHADERPROC glCreateShader;
  PFNGLDELETEBUFFERSPROC glDeleteBuffers;
  PFNGLDELETEFRAMEBUFFERSPROC glDeleteFramebuffers;
  PFNGLDELETEPROGRAMPROC glDeleteProgram;
  PFNGLDELETERENDERBUFFERSPROC glDeleteRenderbuffers;
  PFNGLDELETESHADERPROC glDeleteShader;
  PFNGLDELETETEXTURESPROC glDeleteTextures;
  PFNGLDELETEVERTEXARRAYSPROC glDeleteVertexArrays;
  PFNGLDISABLEPROC glDisable;
  PFNGLDRAWARRAYSPROC glDrawArrays;
  PFNGLDRAWBUFFERPROC glDrawBuffer;
  PFNGLDRAWBUFFERSPROC glDrawBuffers;
  PFNGLENABLEPROC glEnable;
  PFNGLFRAMEBUFFERRENDERBUFFERPROC glFramebufferRenderbuffer;
  PFNGLFRAMEBUFFERTEXTURE2DPROC glFramebufferTexture2D;
  PFNGLGENBUFFERSPROC glGenBuffers;
  PFNGLGENFRAMEBUFFERSPROC glGenFramebuffers;
  PFNGLGENRENDERBUFFERSPROC glGenRenderbuffers;
  PFNGLGENTEXTURESPROC glGenTextures;
  PFNGLGENVERTEXARRAYSPROC glGenVertexArrays;
  PFNGLGETINTEGERVPROC glGetIntegerv;
  PFNGLGETPROGRAMIVPROC glGetProgramiv;
  PFNGLGETSHADERIVPROC glGetShaderiv;
  PFNGLGETUNIFORMLOCATIONPROC glGetUniformLocation;
  PFNGLLINKPROGRAMPROC glLinkProgram;
  PFNGLREADBUFFERPROC glReadBuffer;
  PFNGLRENDERBUFFERSTORAGEPROC glRenderbufferStorage;
  PFNGLSCISSORPROC glScissor;
  PFNGLSHADERSOURCEPROC glShaderSource;
  PFNGLTEXIMAGE2DPROC glTexImage2D;
  PFNGLTEXPARAMETERIPROC glTexParameteri;
  PFNGLTEXSUBIMAGE2DPROC glTexSubImage2D;
  PFNGLUNIFORM1IPROC glUniform1i;
  PFNGLUNIFORMMATRIX4FVPROC glUniformMatrix4fv;
  PFNGLUSEPROGRAMPROC glUseProgram;
  PFNGLVIEWPORTPROC glViewport;
};

/* ------------------------------------------------------- */

struct tra_opengl_api {
  int (*load_functions)(tra_opengl_funcs* funcs); /* Load the GL functions; make sure that you have made your GL context current. */
};

/* ------------------------------------------------------- */

#endif
