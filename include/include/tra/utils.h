#ifndef TRA_UTILS_H
#define TRA_UTILS_H

/* ------------------------------------------------------- */

#include <stdint.h>

/* ------------------------------------------------------- */

int tra_get_file_size(const char* filepath, uint64_t* result); /* Get the size ofthe given filepath. Returns < 0 on error. 0 when we could get he file size. */

/* ------------------------------------------------------- */

int tra_load_module(const char* filepath, void** result);
int tra_load_function(void* module, const char* name, void** handle);
int tra_unload_module(void* module);

/* ------------------------------------------------------- */

#endif
