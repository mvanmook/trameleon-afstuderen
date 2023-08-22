/* ------------------------------------------------------- */

#include <stdio.h>
#include <stdlib.h>
#include <tra/log.h>
#include <tra/core.h>

/* ------------------------------------------------------- */

int main(int argc, const char* argv[]) {

  tra_core_settings core_cfg = { };
  tra_core* core = NULL;
  int r = 0;
  
  TRAD("Module Loading Test");

  r = tra_core_create(&core_cfg, &core);
  if (r < 0) {
    r = -1;
    goto error;
  }

 error:

  if (r < 0) {
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}

/* ------------------------------------------------------- */

/* ------------------------------------------------------- */
