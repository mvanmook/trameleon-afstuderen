/* ------------------------------------------------------- */

#include <tra/modules/mft/MediaFoundationUtils.h>

extern "C" {
#  include <tra/types.h>
#  include <tra/log.h>  
}

/* ------------------------------------------------------- */

int mft_videoformat_to_imageformat(GUID fmt, uint32_t* outFormat) {

  if (NULL == outFormat) {
    TRAE("Cannot convert the MFTVideoFormat GUID into a image format because the given `outFormat` is NULL.");
    return -1;
  }

  if (MFVideoFormat_NV12 == fmt) {
    *outFormat = TRA_IMAGE_FORMAT_NV12;
    return 0;
  }

  TRAE("Unhandled video format; cannot convert into `TRA_IMAGE_FORMAT_*`.");
  return -2;
}

/* ------------------------------------------------------- */

/* Converts a Trameleon image format into a MFT format. */
int mft_imageformat_to_videoformat(uint32_t imageFormat, GUID* outFormat) {

  if (NULL == outFormat) {
    TRAE("Cannot convert th egiven `imageFormat` into a MFTVideoFormat GUID as the given output GUID* is NULL.");
    return -1;
  }

  switch (imageFormat) {
    case TRA_IMAGE_FORMAT_NV12: {
      *outFormat = MFVideoFormat_NV12;
      return 0;
    }
  }

  TRAE("Unhandled image foramt; cannot convert into a MFT format.");
  return -2;
}

/* ------------------------------------------------------- */

/* 
   Prints some info about the given media type. This can be used 
   to e.g. determine the default or preferred format for the
   decoder. 
*/
int mft_print_mediatype(IMFMediaType* mt) {

    uint32_t attr_stride = 0;
    GUID attr_mt = { 0 };
    GUID attr_st = { 0 };
    HRESULT hr = S_OK;

    if (NULL == mt) {
      TRAE("Cannot print `IMFMediaType` as the given pointer is NULL.");
      return -1;
    }

    hr = mt->GetGUID(MF_MT_MAJOR_TYPE, &attr_mt);
    if (false == SUCCEEDED(hr)) {
      TRAE("Failed to get the major type.");
      return -2;
    }

    hr = mt->GetGUID(MF_MT_SUBTYPE, &attr_st);
    if (false == SUCCEEDED(hr)) {
      TRAE("Failed to get the sub type.");
      return -3;
    }

    hr = mt->GetUINT32(MF_MT_DEFAULT_STRIDE, &attr_stride);
    if (false == SUCCEEDED(hr)) {
      TRAE("Failed to get teh stride.");
      return -4;
    }
  
    TRAD("IMFMediaType");
    TRAD(" MF_MT_MAJOR_TYPE: %s", mft_videoformat_to_string(attr_mt));
    TRAD(" MF_MT_SUBTYPE: %s", mft_videoformat_to_string(attr_st));
    TRAD(" MF_MT_DEFAULT_STRIDE: %u", attr_stride);

    return 0;
  }

/* ------------------------------------------------------- */

#define MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(param, val) if (param == val) return #val

const char* mft_videoformat_to_string(const GUID& guid) {

  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MF_MT_MAJOR_TYPE);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MF_MT_SUBTYPE);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MF_MT_ALL_SAMPLES_INDEPENDENT);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MF_MT_FIXED_SIZE_SAMPLES);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MF_MT_COMPRESSED);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MF_MT_SAMPLE_SIZE);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MF_MT_WRAPPED_TYPE);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MF_MT_AUDIO_NUM_CHANNELS);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MF_MT_AUDIO_SAMPLES_PER_SECOND);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MF_MT_AUDIO_FLOAT_SAMPLES_PER_SECOND);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MF_MT_AUDIO_AVG_BYTES_PER_SECOND);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MF_MT_AUDIO_BLOCK_ALIGNMENT);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MF_MT_AUDIO_BITS_PER_SAMPLE);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MF_MT_AUDIO_VALID_BITS_PER_SAMPLE);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MF_MT_AUDIO_SAMPLES_PER_BLOCK);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MF_MT_AUDIO_CHANNEL_MASK);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MF_MT_AUDIO_FOLDDOWN_MATRIX);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MF_MT_AUDIO_WMADRC_PEAKREF);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MF_MT_AUDIO_WMADRC_PEAKTARGET);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MF_MT_AUDIO_WMADRC_AVGREF);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MF_MT_AUDIO_WMADRC_AVGTARGET);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MF_MT_AUDIO_PREFER_WAVEFORMATEX);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MF_MT_AAC_PAYLOAD_TYPE);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MF_MT_AAC_AUDIO_PROFILE_LEVEL_INDICATION);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MF_MT_FRAME_SIZE);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MF_MT_FRAME_RATE);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MF_MT_FRAME_RATE_RANGE_MAX);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MF_MT_FRAME_RATE_RANGE_MIN);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MF_MT_PIXEL_ASPECT_RATIO);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MF_MT_DRM_FLAGS);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MF_MT_PAD_CONTROL_FLAGS);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MF_MT_SOURCE_CONTENT_HINT);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MF_MT_VIDEO_CHROMA_SITING);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MF_MT_INTERLACE_MODE);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MF_MT_TRANSFER_FUNCTION);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MF_MT_VIDEO_PRIMARIES);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MF_MT_CUSTOM_VIDEO_PRIMARIES);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MF_MT_YUV_MATRIX);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MF_MT_VIDEO_LIGHTING);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MF_MT_VIDEO_NOMINAL_RANGE);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MF_MT_GEOMETRIC_APERTURE);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MF_MT_MINIMUM_DISPLAY_APERTURE);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MF_MT_PAN_SCAN_APERTURE);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MF_MT_PAN_SCAN_ENABLED);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MF_MT_AVG_BITRATE);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MF_MT_AVG_BIT_ERROR_RATE);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MF_MT_MAX_KEYFRAME_SPACING);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MF_MT_DEFAULT_STRIDE);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MF_MT_PALETTE);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MF_MT_USER_DATA);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MF_MT_AM_FORMAT_TYPE);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MF_MT_MPEG_START_TIME_CODE);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MF_MT_MPEG2_PROFILE);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MF_MT_MPEG2_LEVEL);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MF_MT_MPEG2_FLAGS);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MF_MT_MPEG_SEQUENCE_HEADER);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MF_MT_DV_AAUX_SRC_PACK_0);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MF_MT_DV_AAUX_CTRL_PACK_0);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MF_MT_DV_AAUX_SRC_PACK_1);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MF_MT_DV_AAUX_CTRL_PACK_1);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MF_MT_DV_VAUX_SRC_PACK);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MF_MT_DV_VAUX_CTRL_PACK);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MF_MT_ARBITRARY_HEADER);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MF_MT_ARBITRARY_FORMAT);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MF_MT_IMAGE_LOSS_TOLERANT);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MF_MT_MPEG4_SAMPLE_DESCRIPTION);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MF_MT_MPEG4_CURRENT_SAMPLE_ENTRY);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MF_MT_ORIGINAL_4CC);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MF_MT_ORIGINAL_WAVE_FORMAT_TAG);

  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MFMediaType_Audio);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MFMediaType_Video);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MFMediaType_Protected);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MFMediaType_SAMI);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MFMediaType_Script);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MFMediaType_Image);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MFMediaType_HTML);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MFMediaType_Binary);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MFMediaType_FileTransfer);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MFVideoFormat_AI44);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MFVideoFormat_ARGB32);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MFVideoFormat_AYUV);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MFVideoFormat_DV25);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MFVideoFormat_DV50);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MFVideoFormat_DVH1);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MFVideoFormat_DVSD);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MFVideoFormat_DVSL);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MFVideoFormat_H264);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MFVideoFormat_I420);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MFVideoFormat_IYUV);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MFVideoFormat_M4S2);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MFVideoFormat_MJPG);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MFVideoFormat_MP43);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MFVideoFormat_MP4S);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MFVideoFormat_MP4V);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MFVideoFormat_MPG1);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MFVideoFormat_MSS1);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MFVideoFormat_MSS2);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MFVideoFormat_NV11);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MFVideoFormat_NV12);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MFVideoFormat_P010);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MFVideoFormat_P016);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MFVideoFormat_P210);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MFVideoFormat_P216);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MFVideoFormat_RGB24);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MFVideoFormat_RGB32);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MFVideoFormat_RGB555);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MFVideoFormat_RGB565);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MFVideoFormat_RGB8);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MFVideoFormat_UYVY);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MFVideoFormat_v210);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MFVideoFormat_v410);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MFVideoFormat_WMV1);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MFVideoFormat_WMV2);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MFVideoFormat_WMV3);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MFVideoFormat_WVC1);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MFVideoFormat_Y210);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MFVideoFormat_Y216);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MFVideoFormat_Y410);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MFVideoFormat_Y416);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MFVideoFormat_Y41P);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MFVideoFormat_Y41T);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MFVideoFormat_YUY2);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MFVideoFormat_YV12);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MFVideoFormat_YVYU);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MFAudioFormat_PCM);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MFAudioFormat_Float);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MFAudioFormat_DTS);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MFAudioFormat_Dolby_AC3_SPDIF);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MFAudioFormat_DRM);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MFAudioFormat_WMAudioV8);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MFAudioFormat_WMAudioV9);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MFAudioFormat_WMAudio_Lossless);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MFAudioFormat_WMASPDIF);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MFAudioFormat_MSP1);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MFAudioFormat_MP3);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MFAudioFormat_MPEG);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MFAudioFormat_AAC);
  MEDIAFOUNDATION_VIDEOFORMAT_TO_STR(guid, MFAudioFormat_ADTS);

  return "UNKNOWN";
}

#undef MEDIAFOUNDATION_VIDEOFORMAT_TO_STR

/* ------------------------------------------------------- */
