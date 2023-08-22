/* ------------------------------------------------------- */

#include <stdio.h>

#if defined(__linux) || defined(__APPLE__)
#  include <unistd.h>
#  include <dlfcn.h>
#endif

#include <tra/log.h>

/* ------------------------------------------------------- */

/* 
   Get the size of the given filepath. Returns < 0 on error. 0
   when we could get he file size.
*/
int tra_get_file_size(const char* filepath, uint64_t* result) {

  int r = 0;
  FILE* fp = NULL;
  int64_t size = 0;
    
  if (NULL == filepath) {
    TRAE("Cannot get the file size as the given `filepath` is NULL.");
    return -1;
  }

  if (NULL == result) {
    TRAE("Cannot get the file size; given destination `result` is NULL.");
    return -2;
  }

  fp = fopen(filepath, "rb");
  if (NULL == fp) {
    TRAE("Cannot get the file size. Failed to load `%s`.", filepath);
    return -3;
  }

  r = fseek(fp, 0, SEEK_END);
  if (0 != r) {
    TRAE("Failed to find the end of the file `%s`.", filepath);
    r = -4;
    goto error;
  }

  size = ftell(fp);
  if (size < 0) {
    TRAE("Failed to retrieve the size of the file: `%s`.", filepath);
    r = -5;
    goto error;
  }

  *result = size;

 error:

  if (NULL != fp) {
    fclose(fp);
    fp = NULL;
  }

  return r;
}

/* ------------------------------------------------------- */

#if defined(_WIN32)

int tra_load_module(const char* filepath, void** result)  {

  HMODULE handle = NULL;
  
  if (NULL == filepath) {
    TRAE("Cannot load the module, given `filepath` is NULL.");
    return -1;
  }

  if (NULL == result) {
    TRAE("Cannot load the module, given result `void**` is NULL.");
    return -2;
  }

  if (NULL != (*result)) {
    TRAE("Cannot load the module, givern `*void**` is not NULL. Make sure to initialize the variable to NULL.");
    return -3;
  }

  handle = LoadLibrary(filepath);
  if (NULL == handle) {
    TRAE("Failed to load the module `%s` with error 0x%08x.", filepath, GetLastError());
    return -4;
  }

  *result = handle;

  return 0;
}

int tra_load_function(void* module, const char* name, void** handle) {

  void* inst = NULL;
  
  if (NULL == module) {
    TRAE("Cannot load the function, given module pointer is NULL.");
    return -1;
  }

  if (NULL == name) {
    TRAE("Cannot load the function, given function name is NULL.");
    return -2;
  }

  if (0 == strlen(name)) {
    TRAE("Cannot load the function, given function name is empty.");
    return -3;
  }

  if (NULL == handle) {
    TRAE("Cannot load the function, given result handle is NULL.");
    return -4;
  }

  if (NULL != (*handle)) {
    TRAE("Cannot load the function, the result handle is not set to NULL.");
    return -5;
  }
  
  inst = (void *)GetProcAddress(module, name);
  if (NULL == inst) {
    TRAE("Failed to load the function `%s`.", name);
    return -6;
  }

  *handle = inst;

  return 0;
}

int tra_unload_module(void* module) {

  TRAE("@todo implement `tra_unload_module` on Windows.");
  
  return 0;
}

#endif /* _WIN32 */

#if defined(__linux) || defined(__APPLE__)

int tra_load_module(const char* filepath, void** result)  {

  void*  handle = NULL;
  
  if (NULL == filepath) {
    TRAE("Cannot load the module, given `filepath` is NULL.");
    return -1;
  }

  if (NULL == result) {
    TRAE("Cannot load the module, given result `void**` is NULL.");
    return -2;
  }

  if (NULL != (*result)) {
    TRAE("Cannot load the module, givern `*void**` is not NULL. Make sure to initialize the variable to NULL.");
    return -3;
  }

  handle = dlopen(filepath, RTLD_NOW | RTLD_GLOBAL);
  if (NULL == handle) {
    TRAE("Failed to load the module `%s` with error: %s", filepath, dlerror());
    return -4;
  }

  *result = handle;

  return 0;
}

int tra_load_function(void* module, const char* name, void** handle) {

  void* inst = NULL;
  
  if (NULL == module) {
    TRAE("Cannot load the function, given module pointer is NULL.");
    return -1;
  }

  if (NULL == name) {
    TRAE("Cannot load the function, given function name is NULL.");
    return -2;
  }

  if (0 == strlen(name)) {
    TRAE("Cannot load the function, given function name is empty.");
    return -3;
  }

  if (NULL == handle) {
    TRAE("Cannot load the function, given result handle is NULL.");
    return -4;
  }

  if (NULL != (*handle)) {
    TRAE("Cannot load the function, the result handle is not set to NULL.");
    return -5;
  }
  
  inst = (void *)dlsym(module, name);
  if (NULL == inst) {
    TRAE("Failed to load the function `%s`. %s", name , dlerror());
    return -6;
  }

  *handle = inst;

  return 0;
}

int tra_unload_module(void* module) {

  TRAE("@todo implement `tra_unload_module` on Linux.");
  
  return 0;

}

#endif /* __linux, __APPLE__ */
