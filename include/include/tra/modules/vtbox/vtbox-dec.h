#ifndef VTBOX_DEC_H
#define VTBOX_DEC_H

/* ------------------------------------------------------- */

#include <stdint.h>

/* ------------------------------------------------------- */

typedef struct tra_decoder_settings tra_decoder_settings;
typedef struct vt_dec               vt_dec;

/* ------------------------------------------------------- */

int vt_dec_create(tra_decoder_settings* cfg, vt_dec** ctx);
int vt_dec_destroy(vt_dec* ctx);
int vt_dec_decode(vt_dec* ctx, uint32_t type, void* data); /* Decodes based on the given type which can be e.g. `TRA_MEMORY_TYPE_HOST_H264`. See `types.h` for the input types. */

/* ------------------------------------------------------- */

#endif
