#ifndef TRA_OPENGL_H
#define TRA_OPENGL_H
/*
  @TODO: I'VE TO CLEANUP THE MEMORY BUFFERS; JUST A TYPE + DATA
  SHOULD BE ENOUGH! ALSO, THE OPENGL STRUCTS NEED TO START
  WITH TRA_MEMORY_OPENGL_{SOMETHING} ... 
 */
/* ------------------------------------------------------- */

#include <stdint.h>

/* ------------------------------------------------------- */

typedef struct tra_upload_opengl_nv12   tra_upload_opengl_nv12;
typedef struct tra_draw_opengl_nv12     tra_draw_opengl_nv12;
typedef struct tra_opengl_texture_rgb24 tra_opengl_texture_rgb24;

/* ------------------------------------------------------- */

struct tra_upload_opengl_nv12 {
  uint32_t tex_y;
  uint32_t tex_uv;
};

/* ------------------------------------------------------- */

struct tra_draw_opengl_nv12 {
  uint32_t tex_y;
  uint32_t tex_uv;
  float x;
  float y;
  float width;
  float height;
};

/* ------------------------------------------------------- */

//struct tra_opengl_texture_rgb24 {
//  uint32_t id; /* The texture ID of a RGB24 texture. */
//};

/* ------------------------------------------------------- */

struct tra_memory_opengl_rgb24 {
  uint32_t tex_id; 
};

/* ------------------------------------------------------- */

// struct tra_memory_opengl_nv12 {
//   uint32_t tex_id;
//   uint32_t tex_stride;
// };

/* ------------------------------------------------------- */

#endif
