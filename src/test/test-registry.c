/* ------------------------------------------------------- */

#include <stdio.h>
#include <stdlib.h>
#include <tra/log.h>
#include <tra/registry.h>

/* ------------------------------------------------------- */

int main(int argc, const char* argv[]) {

  tra_registry* reg = NULL;
  int r = 0;
  
  TRAI("Registry Test");

  r = tra_registry_create(&reg);
  if (r < 0) {
    r = -1;
    goto error;
  }

  r = tra_registry_print_encoder_apis(reg);
  if (r < 0) {
    r = -2;
    goto error;
  }

  r = tra_registry_print_apis(reg);
  if (r < 0) {
    goto error;
  }
  
 error:

  if (NULL != reg) {
    tra_registry_destroy(reg);
    reg = NULL;
  }
  
  if (r < 0) {
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}

/* ------------------------------------------------------- */
