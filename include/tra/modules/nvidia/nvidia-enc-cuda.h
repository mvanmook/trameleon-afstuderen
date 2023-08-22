#ifndef TRA_NVIDIA_ENC_CUDA_H
#define TRA_NVIDIA_ENC_CUDA_H
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

  NVIDIA ENCODER FOR CUDA RESOURCES:
  ==================================

  GENERAL INFO:

    The `tra_nvenccuda` is used to encode buffers which are already
    stored in device memory. Different from the `tra_nvenchost`
    encoder, we don't have to allocate and copy buffers
    around. This encoder expects that you already have upload the
    buffers to the device.

    This encoder is for example used when you want to transcode
    buffers. You use the NVIDIA based decoder which gives you
    device buffers. Then you feed those buffers directly into
    this encoder.

 */
/* ------------------------------------------------------- */

typedef struct tra_nvenccuda        tra_nvenccuda;
typedef struct tra_encoder_settings tra_encoder_settings;
typedef struct tra_sample           tra_sample;
typedef struct tra_nvenc_settings   tra_nvenc_settings;

/* ------------------------------------------------------- */

int tra_nvenccuda_create(tra_encoder_settings* cfg, tra_nvenc_settings* settings, tra_nvenccuda** ctx);
int tra_nvenccuda_destroy(tra_nvenccuda* ctx);
int tra_nvenccuda_encode(tra_nvenccuda* ctx, tra_sample* sample, uint32_t type, void* data);
int tra_nvenccuda_flush(tra_nvenccuda* ctx); 

/* ------------------------------------------------------- */

#endif
