#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <tra/utils.h>
#include <tra/log.h>
#include <tra/buffer.h>

/* ------------------------------------------------------- */

static int tra_buffer_load_file(tra_buffer* buffer, const char* filepath);

/* ------------------------------------------------------- */

int tra_buffer_create(uint32_t capacity, tra_buffer** buf) {

  tra_buffer* inst = NULL;
  int r = 0;
  
  if (NULL == buf) {
    TRAE("Cannot create the buffer; given `tra_buffer**` is NULL.");
    return -1;
  }

  if (NULL != (*buf)) {
    TRAE("Cannot create the buffer; given `*tra_buffer**` is NOT NULL. Iniitialize your variable to NULL.");
    return -2;
  }

  inst = malloc(sizeof(tra_buffer));
  if (NULL == inst) {
    TRAE("Failed to allocate the `tra_buffer`.");
    return -4;
  }
  
  memset(inst, 0x00, sizeof(tra_buffer));

  /* Only allocate some space when requested. */
  if (capacity > 0) {
    
    inst->data = malloc(capacity);
    if (NULL == inst->data) {
      TRAE("Failed to allocate the storage for the buffer.");
      r = -5;
      goto error;
    }

    memset(inst->data, 0x00, capacity);
  }

  inst->capacity = capacity;
  inst->size = 0;

  *buf = inst;

 error:

  if (r < 0) {
    if (NULL != inst) {
      tra_buffer_destroy(inst);
      inst = NULL;
    }
  }

  return r;
}

int tra_buffer_destroy(tra_buffer* buf) {

  if (NULL == buf) {
    TRAE("Cannot destroy the `tra_buffer` as it's NULL.");
    return -1;
  }

  if (NULL != buf->data) {
    free(buf->data);
    buf->data = NULL;
  }

  buf->size = 0;
  buf->capacity = 0;

  free(buf);
  buf = NULL;

  return 0;
}

/* ------------------------------------------------------- */

/* This will load the given file exactly as it is and stores it into the buffer. */
int tra_buffer_load_file_as_bytes(tra_buffer* buf, const char* filepath) {

  int r = tra_buffer_load_file(buf, filepath);
  if (r < 0) {
    TRAE("Failed to load the file as bytes.");
    return r;
  }

  return 0;
}

/*
  This will load the file and allocates one extra byte for the
  '\0' charater that is necessary to laod the file as a string.
*/
int tra_buffer_load_file_as_string(tra_buffer* buf, const char* filepath) {

  uint32_t curr_nbytes = 0;
  uint64_t file_nbytes = 0;
  int r = 0;

  if (NULL == buf) {
    TRAE("Cannot load the file as string as the given `tra_buffer*` is NULL.");
    return -1;
  }

  r = tra_get_file_size(filepath, &file_nbytes);
  if (r < 0) {
    TRAE("Cannot load the file as string as we failed to get the file size.");
    return -2;
  }

  curr_nbytes = buf->size;
  
  r = tra_buffer_load_file(buf, filepath);
  if (r < 0) {
    TRAE("Failed to laod the file as string. (%d)", r);
    return -2;
  }

  /* Add the '\0' character. */
  buf->data[curr_nbytes + file_nbytes] = '\0';

  return 0;
}
  
/* ------------------------------------------------------- */

static int tra_buffer_load_file(tra_buffer* buf, const char* filepath) {

  FILE* fp = NULL;
  int r = 0;
  int64_t size = 0;
  size_t nread = 0;
  uint64_t file_nbytes = 0;
  
  if (NULL == buf) {
    TRAE("Cannot load the file, given `tra_buffer*` is NULL.");
    return -1;
  }

  if (NULL == filepath) {
    TRAE("Cannot load the file, given `filepath` is NULL.");
    return -2;
  }

  r = tra_get_file_size(filepath, &file_nbytes);
  if (r < 0) {
    TRAE("Cannot load the file. Failed to get the file size.");
    return -3;
  }

  fp = fopen(filepath, "rb");
  if (NULL == fp) {
    TRAE("Failed to load `%s`.", filepath);
    return -3;
  }

  r = tra_buffer_ensure_space(buf, file_nbytes);
  if (r < 0) {
    TRAE("Failed to grow our buffer to the size we need to read the file. %s.", filepath);
    r = -7;
    goto error;
  }

  nread = fread(buf->data + buf->size, file_nbytes, 1, fp);
  if (1 != nread) {
    TRAE("Failed to read the file `%s`.", filepath);
    r = -8;
    goto error;
  }

  buf->size += file_nbytes;
  
 error:

  if (NULL != fp) {
    fclose(fp);
    fp = NULL;
  }
  
  return r;
}

/* ------------------------------------------------------- */

 /* Make sure that we can store the given number of bytes. */
int tra_buffer_ensure_space(tra_buffer* buf, uint32_t nbytes) {

  uint32_t curr_space = 0;
  uint8_t* tmp = NULL;
  
  if (NULL == buf) {
    TRAE("Cannot ensure space as the given `tra_buffer*` is NULL.");
    return -1;
  }

  if (0 == nbytes) {
    TRAE("Requested to ensure 0 bytes. Probably not what you meant.");
    return -2;
  }

  /* Do we have enough space left? */
  curr_space = buf->capacity - buf->size;
  if (curr_space >= nbytes) {
    return 0;
  }

  /* We need to grow. */
  tmp = realloc(buf->data, buf->capacity + nbytes);
  if (NULL == tmp) {
    TRAE("Failed to reallocate when trying to ensure space in the buffer.");
    return -3;
  }

  buf->data = tmp;
  buf->capacity += nbytes;
  
  return 0;
}

/* ------------------------------------------------------- */

int tra_buffer_print(tra_buffer* buf) {

  if (NULL == buf) {
    TRAE("Cannot print info about the given `tra_buffer` as it's NULL.");
    return -1;
  }

  TRAD("tra_buffer");
  TRAD("  data: %p", buf->data);
  TRAD("  nbytes: %u", buf->size);
  TRAD("  capacity: %u", buf->capacity);
  TRAD("");

  return 0;
}

/* ------------------------------------------------------- */

int tra_buffer_write(tra_buffer* buf, char* fmt, ...) {

  va_list args;
  int r = 0;
  int nbytes_needed = 0;

  va_start(args, fmt);
  
  if (NULL == buf) {
    TRAE("Cannot write into buffer as the given `tra_buffer*` is NULL.");
    r = -1;
    goto error;
  }

  if (NULL == fmt) {
    TRAE("Cannot write into buffer as the given format (const char*) is NULL.");
    r = -2;
    goto error;
  }

  /* Check how many bytes we need. */
  nbytes_needed = vsnprintf(NULL, 0, fmt, args);
  if (nbytes_needed < 0) {
    TRAE("Failed to write into buffer, `vsnprintf()` failed to calculate the required size.");
    r = -3;
    goto error;
  }

  /* `vsnprintf()` returns the required size w/o the null character so we use `+ 1`.  */
  r = tra_buffer_ensure_space(buf, (uint32_t) nbytes_needed + 1);
  if (r < 0) {
    TRAE("Failed to make sure we have space to write %d bytes.", nbytes_needed);
    r = -4;
    goto error;
  }

  if (buf->capacity < buf->size) {
    TRAE("Cannot write into buffer; capacity or nbytes have an incorrect value.");
    r = -6;
    goto error;
  }

  /* We have to restart `va_args` when we want to use it multiple times. */
  va_end(args);
  va_start(args, fmt);
  
  /* Now write the data. */
  r = vsnprintf((char*)buf->data + buf->size, buf->capacity - buf->size, fmt, args);
  if (r < 0) {
    TRAE("Failed to write the data into the buffer. `vsnprint()` returned < 0 (%d).",  r);
    r = -5;
    goto error;
  }
  
  buf->size += nbytes_needed;

 error:
  
  va_end(args);

  return r;
}

/* ------------------------------------------------------- */

int tra_buffer_append_bytes(tra_buffer* buf, uint32_t nbytes, const uint8_t* data) {

  int r = 0;
  
  if (NULL == buf) {
    TRAE("Cannot append bytes because the given `buf` is NULL.");
    r = -10;
    goto error;
  }

  if (0 == nbytes) {
    TRAE("Cannot append bytes because the given number of bytes to append is 0.");
    r = -20;
    goto error;
  }

  if (NULL == data) {
    TRAE("Cannot append bytes as the given bytes pointer is NULL.");
    r = -30;
    goto error;
  }

  r = tra_buffer_ensure_space(buf, nbytes);
  if (r < 0) {
    TRAE("Cannot append bytes as we failed to ensure the required space.");
    r = -40;
    goto error;
  }

  memcpy(buf->data + buf->size, data, nbytes);

  buf->size += nbytes;

 error:

  return r;
}

/* ------------------------------------------------------- */

/* 
   Reset the buffer state; at the time of writing, this means we
   reset the number of bytes stored (`nbytes`).
*/
int tra_buffer_reset(tra_buffer* buf) {

  if (NULL == buf) {
    TRAE("Cannot reset the buffer as the given `tra_buffer*` is NULL.");
    return -1;
  }

  buf->size = 0;

  return 0;
}

/* ------------------------------------------------------- */
