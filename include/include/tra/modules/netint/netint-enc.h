#ifndef TRA_NETINT_ENC_H
#define TRA_NETINT_ENC_H
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

typedef struct netint_enc netint_enc;
typedef struct tra_encoder_settings tra_encoder_settings;
typedef struct tra_sample tra_sample;

/* ------------------------------------------------------- */

/**
 * initiate the Netint encoder
 * @param cfg the settings that the encoder will be initialised on
 * @param ctx the encoder object that will be created and stores the initiated
 * @param bitDepth used to initiate the bit Depth to 8 or 10;
 * @returns a negative number when an error occurs otherwise returns 0
 */
int netint_enc_create(tra_encoder_settings* cfg, netint_enc** ctx, int bitDepth);

/**
 * destroy the encoder variables
 * @param ctx the encoder object that stores the initiated variables
 * @returns a negative number when an error occurs otherwise returns 0
 */
int netint_enc_destroy(netint_enc* ctx);

/**
 * encode @param data
 * @param ctx the encoder object that stores the initiated variables
 * @param sample
 * @param type the trameleon format type of the data
 * @param data that needs to be encoded
 * @returns a negative number when an error occurs otherwise returns 0
 */
int netint_enc_encode(netint_enc* ctx, tra_sample* sample, uint32_t type, void* data); /* Encode the given `data` that is represented as `type` type. See `tra/types.h` for the available input types. */

/**
 * empty the buffers of the encoder
 * @param ctx the encoder object that stores the initiated variables
 * @returns a negative number when an error occurs otherwise returns 0
 */
int netint_enc_flush(netint_enc *ctx);

/* ------------------------------------------------------- */

#endif
