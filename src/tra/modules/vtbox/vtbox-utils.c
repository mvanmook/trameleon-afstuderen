/* ------------------------------------------------------- */

#include <VideoToolbox/VideoToolbox.h>
#include <tra/modules/vtbox/vtbox-utils.h>
#include <tra/types.h>
#include <tra/log.h>

/* ------------------------------------------------------- */

/* 

  GENERAL INFO:

    Prints info about the given pixel buffer pool. Used
    during development to verify certain attributes of the pool.

  PIXEL FORMAT ISSUE:

    There is a strange issue with the pixel format type. When I
    retrieve the pixel format type of the buffer pool, BEFORE
    I've called `VTCompressionSessionEncodeFrame()`, I get a
    `CFNumberRef` which looks like this, using `CFShow(fmt_ref)`
    to print the value:

        CFNumber 0x55f90c39ef2c8ff5 [0x7fff808d2b70]>{value = +875704438, type = kCFNumberSInt32Type}

    Then, after I have called
    `VTCompressionSessionEncodeFrame()` at least once, then it
    looks like:

         ( 875704438 )

    After a chat with @_vade, he advised me to use
    CVPixelBufferGetPixelFormat() from the returned
    CVPixelBuffer. This function returns a OSType, for which the
    value can be found in `CVPixelBuffer.h`

 */

#define USE_CHECK_PIX_FMT 0

int vtbox_print_pixelbufferpool(CVPixelBufferPoolRef pool) {

  CFDictionaryRef pix_attribs = NULL;
  CFNumberRef width_ref = NULL;
  CFNumberRef height_ref = NULL;
  Boolean status  = false;
  int width = 0;
  int height = 0;
  int r = 0;

#if USE_CHECK_PIX_FMT
  CFNumberRef fmt_ref = NULL;
  int fmt = 0;
#endif  
  
  if (NULL == pool) {
    TRAE("Cannot print pixel buffer pool info, given pool is NULL.");
    r = -1;
    goto error;
  }

  pix_attribs = CVPixelBufferPoolGetPixelBufferAttributes(pool);
  if (NULL == pix_attribs) {
    TRAE("Cannot print the pixel buffer pool info as we failed to get the attributes.");
    r = -2;
    goto error;
  }

  width_ref = (CFNumberRef) CFDictionaryGetValue(pix_attribs, kCVPixelBufferWidthKey);
  if (NULL == width_ref) {
    TRAE("Cannot print the pixel buffer pool info; failed to get the width attrib.");
    r = -3;
    goto error;
  }

  height_ref = (CFNumberRef) CFDictionaryGetValue(pix_attribs, kCVPixelBufferHeightKey);
  if (NULL == height_ref) {
    TRAE("Cannot print the pixel buffer pool info; failed to get the height attrib.");
    r = -4;
    goto error;
  }

#if USE_CHECK_PIX_FMT  
  fmt_ref = (CFNumberRef) CFDictionaryGetValue(pix_attribs, kCVPixelBufferPixelFormatTypeKey);
  if (NULL == fmt_ref) {
    TRAE("Cannot print the pixel buffer pool info; failed to get the pixel format. ");
    r = -5;
    goto error;
  }
#endif  

  status = CFNumberGetValue(width_ref, kCFNumberSInt32Type, &width);
  if (false == status) {
    TRAE("Cannot print the pixel buffer pool info; failed to get the value of the width.");
    r = -6;
    goto error;
  }

  status = CFNumberGetValue(height_ref, kCFNumberSInt32Type, &height);
  if (false == status) {
    TRAE("Cannot print the pixel buffer pool info; failed to get the value of the height.");
    r = -6;
    goto error;
  }

#if USE_CHECK_PIX_FMT  
  status = CFNumberGetValue(fmt_ref, kCFNumberSInt32Type, &fmt);
  if (false == status) {
    TRAE("Cannot print the pixel buffer pool info; failed to get the pixel format type from the pixel buffer.");
    r = -7;
    goto error;
  }
#endif
  
  TRAD("CVPixelBufferPool");
  TRAD("  width: %d", width);
  TRAD("  height: %d", height);
  
#if USE_CHECK_PIX_FMT  
  TRAD("  format: %s", vtbox_pixelformat_to_string(fmt));
  CFShow(fmt_ref);
#endif
  
 error:

  return r;
}

#undef USE_CHECK_PIX_FMT

/* ------------------------------------------------------- */

int vtbox_print_pixelbuffer(CVPixelBufferRef buf) {

  OSType pix_fmt = 0;
  size_t plane_count = 0;
  size_t i = 0;
  int r = 0;
  
  if (NULL == buf) {
    TRAE("Cannot print pixel buffer info as the given buffer is NULL.");
    r = -1;
    goto error;
  }

  pix_fmt = CVPixelBufferGetPixelFormatType(buf);
  plane_count = CVPixelBufferGetPlaneCount(buf);

  TRAD("CVPixelBuffer");
  TRAD("  format: %s", vtbox_pixelformat_to_string(pix_fmt));
  TRAD("  planes count: %zu", plane_count);

  for (i = 0; i < plane_count; ++i) {
    TRAD("  plane: %zu", i);
    TRAD("    width: %u", CVPixelBufferGetWidthOfPlane(buf, i));
    TRAD("    height: %u", CVPixelBufferGetHeightOfPlane(buf, i));
    TRAD("    stride: %zu", CVPixelBufferGetBytesPerRowOfPlane(buf, i));
  }

 error:
  
  return r;
}

/* ------------------------------------------------------- */

/* Converts a CVPixelBufferFormat to an image format like `TRA_IMAGE_FORMAT_{...}`. */
int vtbox_cvpixelformat_to_imageformat(OSType fmt, uint32_t* outFormat) {

  if (NULL == outFormat) {
    TRAE("Cannot map the pixel format to an image format as the given `outFormat` is NULL.");
    return -1;
  }

  switch (fmt) {
    
    case kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange:
    case kCVPixelFormatType_420YpCbCr8BiPlanarFullRange: {
      *outFormat = TRA_IMAGE_FORMAT_NV12;
      return 0;
    }
  }

  TRAE("Cannot map the pixel format to a image format.");
  return -2;
}

/* ------------------------------------------------------- */

const char* vtbox_pixelformat_to_string(int fmt) {
  
  switch(fmt) {
    case kCVPixelFormatType_1Monochrome:                  { return "kCVPixelFormatType_1Monochrome";                  }
    case kCVPixelFormatType_2Indexed:                     { return "kCVPixelFormatType_2Indexed";                     } 
    case kCVPixelFormatType_4Indexed:                     { return "kCVPixelFormatType_4Indexed";                     } 
    case kCVPixelFormatType_8Indexed:                     { return "kCVPixelFormatType_8Indexed";                     } 
    case kCVPixelFormatType_1IndexedGray_WhiteIsZero:     { return "kCVPixelFormatType_1IndexedGray_WhiteIsZero";     } 
    case kCVPixelFormatType_2IndexedGray_WhiteIsZero:     { return "kCVPixelFormatType_2IndexedGray_WhiteIsZero";     } 
    case kCVPixelFormatType_4IndexedGray_WhiteIsZero:     { return "kCVPixelFormatType_4IndexedGray_WhiteIsZero";     } 
    case kCVPixelFormatType_8IndexedGray_WhiteIsZero:     { return "kCVPixelFormatType_8IndexedGray_WhiteIsZero";     } 
    case kCVPixelFormatType_16BE555:                      { return "kCVPixelFormatType_16BE555";                      } 
    case kCVPixelFormatType_16LE555:                      { return "kCVPixelFormatType_16LE555";                      } 
    case kCVPixelFormatType_16LE5551:                     { return "kCVPixelFormatType_16LE5551";                     } 
    case kCVPixelFormatType_16BE565:                      { return "kCVPixelFormatType_16BE565";                      } 
    case kCVPixelFormatType_16LE565:                      { return "kCVPixelFormatType_16LE565";                      } 
    case kCVPixelFormatType_24RGB:                        { return "kCVPixelFormatType_24RGB";                        } 
    case kCVPixelFormatType_24BGR:                        { return "kCVPixelFormatType_24BGR";                        } 
    case kCVPixelFormatType_32ARGB:                       { return "kCVPixelFormatType_32ARGB";                       } 
    case kCVPixelFormatType_32BGRA:                       { return "kCVPixelFormatType_32BGRA";                       } 
    case kCVPixelFormatType_32ABGR:                       { return "kCVPixelFormatType_32ABGR";                       } 
    case kCVPixelFormatType_32RGBA:                       { return "kCVPixelFormatType_32RGBA";                       } 
    case kCVPixelFormatType_64ARGB:                       { return "kCVPixelFormatType_64ARGB";                       } 
    case kCVPixelFormatType_48RGB:                        { return "kCVPixelFormatType_48RGB";                        } 
    case kCVPixelFormatType_32AlphaGray:                  { return "kCVPixelFormatType_32AlphaGray";                  } 
    case kCVPixelFormatType_16Gray:                       { return "kCVPixelFormatType_16Gray";                       } 
    case kCVPixelFormatType_422YpCbCr8:                   { return "kCVPixelFormatType_422YpCbCr8";                   } 
    case kCVPixelFormatType_4444YpCbCrA8:                 { return "kCVPixelFormatType_4444YpCbCrA8";                 } 
    case kCVPixelFormatType_4444YpCbCrA8R:                { return "kCVPixelFormatType_4444YpCbCrA8R";                } 
    case kCVPixelFormatType_444YpCbCr8:                   { return "kCVPixelFormatType_444YpCbCr8";                   } 
    case kCVPixelFormatType_422YpCbCr16:                  { return "kCVPixelFormatType_422YpCbCr16";                  } 
    case kCVPixelFormatType_422YpCbCr10:                  { return "kCVPixelFormatType_422YpCbCr10";                  } 
    case kCVPixelFormatType_444YpCbCr10:                  { return "kCVPixelFormatType_444YpCbCr10";                  } 
    case kCVPixelFormatType_420YpCbCr8Planar:             { return "kCVPixelFormatType_420YpCbCr8Planar";             } 
    case kCVPixelFormatType_420YpCbCr8PlanarFullRange:    { return "kCVPixelFormatType_420YpCbCr8PlanarFullRange";    } 
    case kCVPixelFormatType_422YpCbCr_4A_8BiPlanar:       { return "kCVPixelFormatType_422YpCbCr_4A_8BiPlanar";       } 
    case kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange: { return "kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange"; } 
    case kCVPixelFormatType_420YpCbCr8BiPlanarFullRange:  { return "kCVPixelFormatType_420YpCbCr8BiPlanarFullRange";  } 
    case kCVPixelFormatType_422YpCbCr8_yuvs:              { return "kCVPixelFormatType_422YpCbCr8_yuvs";              } 
    case kCVPixelFormatType_422YpCbCr8FullRange:          { return "kCVPixelFormatType_422YpCbCr8FullRange";          } 
    case kCMVideoCodecType_Animation:                     { return "kCMVideoCodecType_Animation";                     } 
    case kCMVideoCodecType_Cinepak:                       { return "kCMVideoCodecType_Cinepak";                       } 
    case kCMVideoCodecType_JPEG:                          { return "kCMVideoCodecType_JPEG";                          } 
    case kCMVideoCodecType_JPEG_OpenDML:                  { return "kCMVideoCodecType_JPEG_OpenDML";                  } 
    case kCMVideoCodecType_SorensonVideo:                 { return "kCMVideoCodecType_SorensonVideo";                 } 
    case kCMVideoCodecType_SorensonVideo3:                { return "kCMVideoCodecType_SorensonVideo3";                } 
    case kCMVideoCodecType_H263:                          { return "kCMVideoCodecType_H263";                          } 
    case kCMVideoCodecType_H264:                          { return "kCMVideoCodecType_H264";                          } 
    case kCMVideoCodecType_MPEG4Video:                    { return "kCMVideoCodecType_MPEG4Video";                    } 
    case kCMVideoCodecType_MPEG2Video:                    { return "kCMVideoCodecType_MPEG2Video";                    } 
    case kCMVideoCodecType_MPEG1Video:                    { return "kCMVideoCodecType_MPEG1Video";                    } 
    case kCMVideoCodecType_DVCNTSC:                       { return "kCMVideoCodecType_DVCNTSC";                       } 
    case kCMVideoCodecType_DVCPAL:                        { return "kCMVideoCodecType_DVCPAL";                        } 
    case kCMVideoCodecType_DVCProPAL:                     { return "kCMVideoCodecType_DVCProPAL";                     } 
    case kCMVideoCodecType_DVCPro50NTSC:                  { return "kCMVideoCodecType_DVCPro50NTSC";                  } 
    case kCMVideoCodecType_DVCPro50PAL:                   { return "kCMVideoCodecType_DVCPro50PAL";                   } 
    case kCMVideoCodecType_DVCPROHD720p60:                { return "kCMVideoCodecType_DVCPROHD720p60";                } 
    case kCMVideoCodecType_DVCPROHD720p50:                { return "kCMVideoCodecType_DVCPROHD720p50";                } 
    case kCMVideoCodecType_DVCPROHD1080i60:               { return "kCMVideoCodecType_DVCPROHD1080i60";               } 
    case kCMVideoCodecType_DVCPROHD1080i50:               { return "kCMVideoCodecType_DVCPROHD1080i50";               } 
    case kCMVideoCodecType_DVCPROHD1080p30:               { return "kCMVideoCodecType_DVCPROHD1080p30";               } 
    case kCMVideoCodecType_DVCPROHD1080p25:               { return "kCMVideoCodecType_DVCPROHD1080p25";               } 
    case kCMVideoCodecType_AppleProRes4444:               { return "kCMVideoCodecType_AppleProRes4444";               } 
    case kCMVideoCodecType_AppleProRes422HQ:              { return "kCMVideoCodecType_AppleProRes422HQ";              } 
    case kCMVideoCodecType_AppleProRes422:                { return "kCMVideoCodecType_AppleProRes422";                } 
    case kCMVideoCodecType_AppleProRes422LT:              { return "kCMVideoCodecType_AppleProRes422LT";              } 
    case kCMVideoCodecType_AppleProRes422Proxy:           { return "kCMVideoCodecType_AppleProRes422Proxy";           } 
    default:                                              { return "UNKNOWN";                                         }
  }
}

/* ------------------------------------------------------- */
