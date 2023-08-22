/* ------------------------------------------------------- */

#include <tra/types.h>
#include <tra/log.h>

/* ------------------------------------------------------- */

int tra_memoryimage_print(tra_memory_image* img) {

  uint32_t i = 0;
  
  if (NULL == img) {
    TRAE("Cannot print the image as it's NULL.");
    return -10;
  }

  TRAD("tra_image");
  TRAD("  image_format: %s", tra_imageformat_to_string(img->image_format));
  TRAD("  image_width: %u", img->image_width);
  TRAD("  image_height: %u", img->image_height);
  TRAD("  plane_count: %u", img->plane_count);

  for (i = 0; i < TRA_MAX_IMAGE_PLANES; ++i) {
    TRAD("  plane_data[%u]: %p", i, img->plane_data[i]);
  }
  
  for (i = 0; i < TRA_MAX_IMAGE_PLANES; ++i) {
    TRAD("  plane_strides[%u]: %u", i, img->plane_strides[i]);
  }
  
  for (i = 0; i < TRA_MAX_IMAGE_PLANES; ++i) {
    TRAD("  plane_heights[%u]: %u", i, img->plane_heights[i]);
  }

  return 0;
}

/* ------------------------------------------------------- */

const char* tra_imageformat_to_string(uint32_t fmt) {

  switch (fmt) {
    case TRA_IMAGE_FORMAT_NONE: { return "TRA_IMAGE_FORMAT_NONE"; }
    case TRA_IMAGE_FORMAT_I400: { return "TRA_IMAGE_FORMAT_I400"; }
    case TRA_IMAGE_FORMAT_I420: { return "TRA_IMAGE_FORMAT_I420"; }
    case TRA_IMAGE_FORMAT_YV12: { return "TRA_IMAGE_FORMAT_YV12"; }
    case TRA_IMAGE_FORMAT_NV12: { return "TRA_IMAGE_FORMAT_NV12"; }
    case TRA_IMAGE_FORMAT_NV21: { return "TRA_IMAGE_FORMAT_NV21"; }
    case TRA_IMAGE_FORMAT_I422: { return "TRA_IMAGE_FORMAT_I422"; }
    case TRA_IMAGE_FORMAT_YV16: { return "TRA_IMAGE_FORMAT_YV16"; }
    case TRA_IMAGE_FORMAT_NV16: { return "TRA_IMAGE_FORMAT_NV16"; }
    case TRA_IMAGE_FORMAT_YUYV: { return "TRA_IMAGE_FORMAT_YUYV"; }
    case TRA_IMAGE_FORMAT_UYVY: { return "TRA_IMAGE_FORMAT_UYVY"; }
    case TRA_IMAGE_FORMAT_V210: { return "TRA_IMAGE_FORMAT_V210"; }
    case TRA_IMAGE_FORMAT_I444: { return "TRA_IMAGE_FORMAT_I444"; }
    case TRA_IMAGE_FORMAT_YV24: { return "TRA_IMAGE_FORMAT_YV24"; }
    case TRA_IMAGE_FORMAT_BGR:  { return "TRA_IMAGE_FORMAT_BGR";  }
    case TRA_IMAGE_FORMAT_BGRA: { return "TRA_IMAGE_FORMAT_BGRA"; }
    case TRA_IMAGE_FORMAT_RGB:  { return "TRA_IMAGE_FORMAT_RGB";  }
    default:                    { return "UNKNOWN";               }
  }
}
        
/* ------------------------------------------------------- */

const char* tra_memorytype_to_string(uint32_t type) {

  switch (type) {
    case TRA_MEMORY_TYPE_NONE:  { return "TRA_MEMORY_TYPE_NONE";  }
    case TRA_MEMORY_TYPE_IMAGE: { return "TRA_MEMORY_TYPE_IMAGE"; }
    case TRA_MEMORY_TYPE_CUDA:  { return "TRA_MEMORY_TYPE_CUDA";  }
    case TRA_MEMORY_TYPE_H264:  { return "TRA_MEMORY_TYPE_H264";  }
    default:                    { return "UNKNOWN";               }
  }
}

/* ------------------------------------------------------- */
