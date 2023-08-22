#ifndef TRA_NVIDIA_CONVERTER_H
#define TRA_NVIDIA_CONVERTER_H
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

  NVIDIA BASED CONVERTER
  ======================

  GENERAL INFO:

    This converter uses CUDA to scale input data. This can be used
    during a transcode session where we want to scale an input
    into a different output.
    
 */

/* ------------------------------------------------------- */

#include <stdint.h>

/* ------------------------------------------------------- */

typedef struct tra_converter_settings tra_converter_settings;
typedef struct tra_nvconverter        tra_nvconverter;

/* ------------------------------------------------------- */

int tra_nvconverter_create(tra_converter_settings* cfg, void* settings, tra_nvconverter** ctx);
int tra_nvconverter_destroy(tra_nvconverter* ctx);
int tra_nvconverter_convert(tra_nvconverter* ctx, uint32_t type, void* data);

/* ------------------------------------------------------- */

#endif
