/*
  ---------------------------------------------------------------

                       ██████╗  ██████╗ ██╗  ██╗██╗     ██╗   ██╗
                       ██╔══██╗██╔═══██╗╚██╗██╔╝██║     ██║   ██║
                       ██████╔╝██║   ██║ ╚███╔╝ ██║     ██║   ██║
                       ██╔══██╗██║   ██║ ██╔██╗ ██║     ██║   ██║
                       ██║  ██║╚██████╔╝██╔╝ ██╗███████╗╚██████╔╝
                       ╚═╝  ╚═╝ ╚═════╝ ╚═╝  ╚═╝╚══════╝ ╚═════╝ 

                                                    www.roxlu.com
                                            www.twitter.com/roxlu
  
  ---------------------------------------------------------------

  VAAPI UTILS
  =========== 

  GENERAL INFO:

    The `vaapi-utils.h` defines helper functions and types that
    are used by the encoder and decoder wrapper.

 */

#ifndef MO_VAAPI_UTILS_H
#define MO_VAAPI_UTILS_H

/* ------------------------------------------------------- */

void va_util_error_callback(void* user, const char* msg);
void va_util_info_callback(void* user, const char* msg);
int va_util_compare_h264picture_frame_num(const void* a, const void* b); /* Used to sort on `frame_num` for list0. */
int va_util_map_image_format_to_sampling_format(uint32_t imageFormat, uint32_t* outFormat); /* Maps the image format to a sampling format (YUV420, YUV422, YUV444, etc.) */
int va_util_map_image_format_to_storage_format(uint32_t imageFormat, uint32_t* outFormat); /* Maps the Trameleon pixel format to the VAAPI storage fourcc code. VAAPI make a different between sampling method (e.g. YUV420, YUV422, YUV444) and storage method NV12 (2 planes), YV12 (3 planes) */
int va_util_map_storage_format_to_image_format(uint32_t storageFormat, uint32_t* outImageFormat); /* Maps a VAAPI fourcc to a `TRA_IMAGE_FORMAT_*` type. */

/* ------------------------------------------------------- */

#endif
