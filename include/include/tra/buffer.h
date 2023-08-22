#ifndef TRA_BUFFER_H
#define TRA_BUFFER_H

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

    BUFFER
    =======

    GENERAL INFO:

      The `tra_buffer` is a generic type that can be used to read
      and write arbitrary data. It can, e.g. be used to generate
      bitstreams, read files, etc.
  
 */
/* ------------------------------------------------------- */

#include <stdint.h>

/* ------------------------------------------------------- */

typedef struct tra_buffer tra_buffer;

/* ------------------------------------------------------- */

struct tra_buffer {
  uint32_t capacity;                                                                /* Total number of bytes we can store in `data`. */
  uint32_t size;                                                                    /* Number of bytes stored in `data`. */
  uint8_t* data;
};

/* ------------------------------------------------------- */

int tra_buffer_create(uint32_t capacity, tra_buffer** buf);                         /* Allocate the `tra_buffer` that can hold `capacity` number of bytes before it needs to reallocate. */
int tra_buffer_destroy(tra_buffer* buf);                                            /* Deallocates the given `tra_buffer*` and makes sure to free up any used memory. */
int tra_buffer_load_file_as_bytes(tra_buffer* buf, const char* filepath);           /* IMPORTANT: When you want to load a TEXT file as STRING use `tra_buffer_load_file_as_string()` as the loaded data won't be `\0` terminated otherwise. */
int tra_buffer_load_file_as_string(tra_buffer* buf, const char* filepath);          /* Loads the file as a string: e.g. NULL terminates the file, i.e. adds `\0`. */
int tra_buffer_ensure_space(tra_buffer* buf, uint32_t nbytes);                      /* Make sure that we can store the given number of bytes. */

int tra_buffer_reset(tra_buffer* buf);                                              /* Resets the write pointer. */
int tra_buffer_print(tra_buffer* buf);                                              /* Print debug info. */
int tra_buffer_write(tra_buffer* buf, char* fmt, ...);                              /* Use `print()` style writing into the buffer. We make sure that `nbytes` increments as necessary. */
int tra_buffer_append_bytes(tra_buffer* buf, uint32_t nbytes, const uint8_t* data); /* This will append `num` number of bytes and makes sure we resize when necessary. */

/* ------------------------------------------------------- */

#endif
