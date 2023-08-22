/* ------------------------------------------------------- */

#include <stdlib.h>
#include <va/va.h>

#include <tra/modules/vaapi/vaapi-utils.h>
#include <tra/types.h>
#include <tra/log.h>

/* ------------------------------------------------------- */

void va_util_error_callback(void* user, const char* msg) {
  TRAE("%s", msg);
}

/* ------------------------------------------------------- */

void va_util_info_callback(void* user, const char* msg) {
  TRAD("%s", msg);
}

/* ------------------------------------------------------- */

/*

  GENERAL:

    This function is used to sort the list0 when we're dealing
    with a P frame. This P frame needs a list with the reference
    frames that it can use. This list is sorted on _PicNum_ which
    is `frame_num modulo max_frame_num`. The `frame_num` is
    stored as `frame_idx` in the `VAPictureH264`.

*/
int va_util_compare_h264picture_frame_num(const void* a, const void* b) {

  VAPictureH264* pa = NULL;
  VAPictureH264* pb = NULL;
  
  if (NULL == a) {
    TRAE("Cannot sort the h264 pictures as the given `a` is NULL. (exiting)");
    exit(EXIT_FAILURE);
  }

  if (NULL == b) {
    TRAE("Cannot sort the h264 pictures as the given `b` is NULL. (exiting).");
    exit(EXIT_FAILURE);
  }

  pa = (VAPictureH264*) a;
  pb = (VAPictureH264*) b;

  if (pa->frame_idx > pb->frame_idx) {
    return -1;
  }
  
  if (pa->frame_idx < pb->frame_idx) {
    return 1;
  }
  
  return 0;
}

/* ------------------------------------------------------- */

/* 
   Maps the Trameleon pixel format to the VAAPI type. See the
   notes for the `enc_map_image_format_to_fourcc`.
*/
int va_util_map_image_format_to_sampling_format(uint32_t imageFormat, uint32_t* outFormat) {

  if (NULL == outFormat) {
    TRAE("Cannot map the image format to a VAAPI pixel format as the output argument, `outFormat` is NULL.");
    return -1;
  }
  
  switch (imageFormat) {

    case TRA_IMAGE_FORMAT_NV12: {
      *outFormat = VA_RT_FORMAT_YUV420;
      return 0;
    }
  }

  TRAE("Cannot map the image format into a VAAPI pixel format: %u. ", imageFormat);
  return -2;
}

/* ------------------------------------------------------- */

/* 

   VAAPI makes a distinction between the sampling format (render
   target format) and the storage (fourcc). For example, when we
   use the TRA_IMAGE_FORMAT_NV12 or TRA_IMAGE_FORMAT_I420 they
   both use a YUV 4:2:0 sampling format but their strorage is
   different.

   The fourcc from VAAPI defines the storage. 

 */
int va_util_map_image_format_to_storage_format(uint32_t imageFormat, uint32_t* outFormat) {

  if (NULL == outFormat) {
    TRAE("Cannot map the image format to a VAAPI fourcc format as the output argument is NULL.");
    return -1;
  }

  switch (imageFormat) {
    
    case TRA_IMAGE_FORMAT_NV12: {
      *outFormat = VA_FOURCC_NV12; /* Y plane, UV plane */
      return 0;
    }
  }

  TRAE("Cannot map the image format (%u) to a VAAPI fourcc format.", imageFormat);
  return -2;
}

/* ------------------------------------------------------- */

/* Maps a VAAPI fourcc to a `TRA_IMAGE_FORMAT_*` type. */
int va_util_map_storage_format_to_image_format(uint32_t storageFormat, uint32_t* outImageFormat) {

  if (NULL == outImageFormat) {
    TRAE("Cannot map the given storage format into a image format as the output argument is NULL.");
    return -1;
  }

  switch (storageFormat) {
    case VA_FOURCC_NV12: {
      *outImageFormat = TRA_IMAGE_FORMAT_NV12;
      return 0;
    }
  }

  TRAE("Cannot map the storage format to a VAAPI fourcc format; unhandled fourcc.");
  return -2;
}

/* ------------------------------------------------------- */
