#ifndef TRA_X264_H
#define TRA_X264_H
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

  X264 ENCODER
  ============

  GENERAL INFO:

    This file provides some features for the x264 based encoder
    of Trameleon. This x264 based encoder is a good example that
    demonstrates how to wrap an encoder.

    
  X264 SETTINGS:

    At the time of writing this, there isn't supported that
    allows us to indicate profile, preset or tuning
    settings. Therefore I've created the `tra_x264_settings` that
    you can use to override the defaults that we use.

*/

/* ------------------------------------------------------- */

typedef struct tra_x264_settings tra_x264_settings;

/* ------------------------------------------------------- */

struct tra_x264_settings {
  const char* profile;   /* When given we will select this profile that is supported by x264. E.g. baseline, main, high, ... */
  const char* preset;    /* When given we will select this preset. E.g. ultrafast, superfast, veryfast, etc. */
  const char* tune;      /* Whyen given we will select this tune. E.g. fastdecode, zerolatency, etc. */
};

/* ------------------------------------------------------- */

#endif
