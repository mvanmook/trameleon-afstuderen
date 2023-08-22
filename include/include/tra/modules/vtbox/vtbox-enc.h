#ifndef VTBOX_ENC_H
#define VTBOX_ENC_H

/* ------------------------------------------------------- */

#include <stdint.h>

/* ------------------------------------------------------- */

typedef struct vt_enc                vt_enc;
typedef struct tra_encoder_settings  tra_encoder_settings;
typedef struct tra_sample            tra_sample;

/* ------------------------------------------------------- */

int vt_enc_create(tra_encoder_settings* cfg, vt_enc** ctx);
int vt_enc_destroy(vt_enc* ctx);
int vt_enc_encode(vt_enc* ctx, tra_sample* sample, uint32_t type, void* data); 

/* ------------------------------------------------------- */

#endif
