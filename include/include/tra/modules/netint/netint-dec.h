#ifndef TRA_NETINT_DEC_H
#define TRA_NETINT_DEC_H
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

 */

/* ------------------------------------------------------- */

#include <stdint.h>

/* ------------------------------------------------------- */

typedef struct netint_dec netint_dec;
typedef struct tra_decoder_settings tra_decoder_settings;

/* ------------------------------------------------------- */

/**
 * initiate the Netint decoder
 * @param cfg the settings that the encoder will be initialised on
 * @param ctx the decoder object that will be created and stores the initiated
 * @param bitDepth used to initiate the bit Depth to 8 or 10;
 * @returns a negative number when an error occurs otherwise returns 0
 */
int netint_dec_create(tra_decoder_settings* cfg, netint_dec** ctx, int bitDepth);

/**
 * destroy the decoder variables
 * @param ctx the decoder object that stores the initiated variables
 * @returns a negative number when an error occurs otherwise returns 0
 */
int netint_dec_destroy(netint_dec* ctx);

/**
 * decode @param data
 * @param ctx the decoder object that stores the initiated variables
 * @param type the trameleon format type of the data
 * @param data that needs to be encoded
 * @returns a negative number when an error occurs otherwise returns 0
 */
int netint_dec_decode(netint_dec* ctx, uint32_t type, void* data); /* Decode the given `data` that is represented as `type` type. See `tra/types.h` for the available input types. */

/* ------------------------------------------------------- */

#endif
