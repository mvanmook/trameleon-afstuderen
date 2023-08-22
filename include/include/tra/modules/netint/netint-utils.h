#ifndef TRA_NETINT_UTILS_H
#define TRA_NETINT_UTILS_H

/* ------------------------------------------------------- */



/* ------------------------------------------------------- */

typedef struct _ni_logan_encoder_params  ni_logan_encoder_params_t;
typedef struct _ni_logan_session_context ni_logan_session_context_t;

/* ------------------------------------------------------- */

int netint_print_encoderparams(ni_logan_encoder_params_t* params);
int netint_print_sessioncontext(ni_logan_session_context_t* session);

const char* netint_codecformat_to_string(uint32_t fmt);
const char* netint_retcode_to_string(ni_logan_retcode_t code);

/* ------------------------------------------------------- */

/**
 * translate @param trameleon_format to get Netint @param codec
 * @param codec the codec to be assigned
 * @param trameleon_format the format used in trameleon to be translated
 * @returns a negative number when an error occurs otherwise returns 0
 */
int convert_trameleon_to_netint_codec(uint32_t *codec, uint32_t trameleon_format);

/**
 * retrieve the alignment from @param trameleon_format
 * @param alignment that will be set to the value needed
 * @param trameleon_format the value the aligment is dependent on
 * @returns a negative number when an error occurs otherwise returns 0
 */
int convert_trameleon_to_netint_alignment(uint8_t *alignment, uint32_t trameleon_format);

/**
 * translate @param trameleon_format to get Netint @param codec_format
 * @param codec_format the codec format to be assigned
 * @param trameleon_format the format used in trameleon to be translated
 * @returns a negative number when an error occurs otherwise returns 0
 */
int convert_trameleon_to_netint_codec_format(uint32_t *codec_format, uint32_t trameleon_format);
  
#endif
