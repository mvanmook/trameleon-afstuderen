#ifndef TRA_NETINT_UTILS_H
#define TRA_NETINT_UTILS_H

/* ------------------------------------------------------- */

#include <ni_defs.h>

/* ------------------------------------------------------- */

typedef struct _ni_encoder_params  ni_encoder_params_t;
typedef struct _ni_session_context ni_session_context_t;

/* ------------------------------------------------------- */

int netint_print_encoderparams(ni_encoder_params_t* params);
int netint_print_sessioncontext(ni_session_context_t* session); 

const char* netint_codecformat_to_string(uint32_t fmt);
const char* netint_retcode_to_string(ni_retcode_t code);

/* ------------------------------------------------------- */
  
#endif
