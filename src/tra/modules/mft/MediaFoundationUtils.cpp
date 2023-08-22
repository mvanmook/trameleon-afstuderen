#include "tra/modules/mft/MediaFoundationUtils.h"
#include "tra/types.h"

int convert_trameleon_to_mft_hardAcc(GUID &hardAcc, uint32_t type) {
  switch (type) {

  case TRA_IMAGE_FORMAT_H264: hardAcc = CODECAPI_AVDecVideoAcceleration_H264; return 0;
  // case TRA_IMAGE_FORMAT_ : hardAcc = CODECAPI_AVDecVideoAcceleration_MPEG2; return 0; @TODO figure out this conversion
  case TRA_IMAGE_FORMAT_WVC1: hardAcc = CODECAPI_AVDecVideoAcceleration_VC1; return 0;

  default: return -1;
  }
}

int convert_trameleon_to_mft_image_format(uint32_t type, GUID &image_format) {
  switch (type) {
  case TRA_IMAGE_FORMAT_H264: image_format = MFVideoFormat_H264; return 0;
  case TRA_IMAGE_FORMAT_I420: image_format = MFVideoFormat_I420; return 0;
  case TRA_IMAGE_FORMAT_YV12: image_format = MFVideoFormat_YV12; return 0;
  case TRA_IMAGE_FORMAT_NV12: image_format = MFVideoFormat_NV12; return 0;
  case TRA_IMAGE_FORMAT_UYVY: image_format = MFVideoFormat_UYVY; return 0;
  case TRA_IMAGE_FORMAT_V210: image_format = MFVideoFormat_v210; return 0;
  case TRA_IMAGE_FORMAT_AI44: image_format = MFVideoFormat_AI44; return 0;
  case TRA_IMAGE_FORMAT_AYUV: image_format = MFVideoFormat_AYUV; return 0;
  case TRA_IMAGE_FORMAT_YUY2: image_format = MFVideoFormat_YUY2; return 0;
  case TRA_IMAGE_FORMAT_YVYU: image_format = MFVideoFormat_YVYU; return 0;
  case TRA_IMAGE_FORMAT_YVU9: image_format = MFVideoFormat_YVU9; return 0;
  case TRA_IMAGE_FORMAT_NV11: image_format = MFVideoFormat_NV11; return 0;
  case TRA_IMAGE_FORMAT_IYUV: image_format = MFVideoFormat_IYUV; return 0;
  case TRA_IMAGE_FORMAT_Y210: image_format = MFVideoFormat_Y210; return 0;
  case TRA_IMAGE_FORMAT_Y216: image_format = MFVideoFormat_Y216; return 0;
  case TRA_IMAGE_FORMAT_Y410: image_format = MFVideoFormat_Y410; return 0;
  case TRA_IMAGE_FORMAT_Y416: image_format = MFVideoFormat_Y416; return 0;
  case TRA_IMAGE_FORMAT_Y41P: image_format = MFVideoFormat_Y41P; return 0;
  case TRA_IMAGE_FORMAT_Y41T: image_format = MFVideoFormat_Y41T; return 0;
  case TRA_IMAGE_FORMAT_Y42T: image_format = MFVideoFormat_Y42T; return 0;
  case TRA_IMAGE_FORMAT_P210: image_format = MFVideoFormat_P210; return 0;
  case TRA_IMAGE_FORMAT_P010: image_format = MFVideoFormat_P010; return 0;
  case TRA_IMAGE_FORMAT_P016: image_format = MFVideoFormat_P016; return 0;
  case TRA_IMAGE_FORMAT_v216: image_format = MFVideoFormat_v216; return 0;
  case TRA_IMAGE_FORMAT_v410: image_format = MFVideoFormat_v410; return 0;
  case TRA_IMAGE_FORMAT_MP43: image_format = MFVideoFormat_MP43; return 0;
  case TRA_IMAGE_FORMAT_MP4S: image_format = MFVideoFormat_MP4S; return 0;
  case TRA_IMAGE_FORMAT_M4S2: image_format = MFVideoFormat_M4S2; return 0;
  case TRA_IMAGE_FORMAT_MP4V: image_format = MFVideoFormat_MP4V; return 0;
  case TRA_IMAGE_FORMAT_WMV1: image_format = MFVideoFormat_WMV1; return 0;
  case TRA_IMAGE_FORMAT_WMV2: image_format = MFVideoFormat_WMV2; return 0;
  case TRA_IMAGE_FORMAT_WMV3: image_format = MFVideoFormat_WMV3; return 0;
  case TRA_IMAGE_FORMAT_WVC1: image_format = MFVideoFormat_WVC1; return 0;
  case TRA_IMAGE_FORMAT_MSS1: image_format = MFVideoFormat_MSS1; return 0;
  case TRA_IMAGE_FORMAT_MSS2: image_format = MFVideoFormat_MSS2; return 0;
  case TRA_IMAGE_FORMAT_MPG1: image_format = MFVideoFormat_MPG1; return 0;
  case TRA_IMAGE_FORMAT_DVSL: image_format = MFVideoFormat_DVSL; return 0;
  case TRA_IMAGE_FORMAT_DVSD: image_format = MFVideoFormat_DVSD; return 0;
  case TRA_IMAGE_FORMAT_DVHD: image_format = MFVideoFormat_DVHD; return 0;
  case TRA_IMAGE_FORMAT_DV25: image_format = MFVideoFormat_DV25; return 0;
  case TRA_IMAGE_FORMAT_DV50: image_format = MFVideoFormat_DV50; return 0;
  case TRA_IMAGE_FORMAT_DVH1: image_format = MFVideoFormat_DVH1; return 0;
  case TRA_IMAGE_FORMAT_DVC: image_format = MFVideoFormat_DVC; return 0;
  case TRA_IMAGE_FORMAT_H265: image_format = MFVideoFormat_H265; return 0;
  case TRA_IMAGE_FORMAT_MJPG: image_format = MFVideoFormat_MJPG; return 0;
  case TRA_IMAGE_FORMAT_4200: image_format = MFVideoFormat_420O; return 0;
  case TRA_IMAGE_FORMAT_HEVC: image_format = MFVideoFormat_HEVC; return 0;
  case TRA_IMAGE_FORMAT_HEVC_ES: image_format = MFVideoFormat_HEVC_ES; return 0;
  case TRA_IMAGE_FORMAT_VP80: image_format = MFVideoFormat_VP80; return 0;
  case TRA_IMAGE_FORMAT_VP90: image_format = MFVideoFormat_VP90; return 0;
  default: return -1;
  }
}

int convert_trameleon_to_mft_profile(uint32_t &profile, uint32_t type) {
  switch (type) {
  case TRA_Profile_H264_BASE: profile = eAVEncH264VProfile_Base; return 0;
  case TRA_Profile_H264_MAIN: profile = eAVEncH264VProfile_Main; return 0;
  case TRA_Profile_H264_HIGH: profile = eAVEncH264VProfile_High; return 0;
  case TRA_Profile_H264_H10: profile = eAVEncH264VProfile_High10; return 0;
  case TRA_Profile_H264_H422: profile = eAVEncH264VProfile_422; return 0;
  case TRA_Profile_H264_H444: profile = eAVEncH264VProfile_444; return 0;
  default: return -1;
  }
}

int convert_mft_to_trameleon_image_format(GUID &image_format, uint32_t &type) {
  if (MFVideoFormat_H264 == image_format) {
    type = TRA_IMAGE_FORMAT_H264;
    return 0;
  } else if (MFVideoFormat_I420 == image_format) {
    type = TRA_IMAGE_FORMAT_I420;
    return 0;
  } else if (MFVideoFormat_YV12 == image_format) {
    type = TRA_IMAGE_FORMAT_YV12;
    return 0;
  } else if (MFVideoFormat_NV12 == image_format) {
    type = TRA_IMAGE_FORMAT_NV12;
    return 0;
  } else if (MFVideoFormat_UYVY == image_format) {
    type = TRA_IMAGE_FORMAT_UYVY;
    return 0;
  } else if (MFVideoFormat_v210 == image_format) {
    type = TRA_IMAGE_FORMAT_V210;
    return 0;
  } else if (MFVideoFormat_AI44 == image_format) {
    type = TRA_IMAGE_FORMAT_AI44;
    return 0;
  } else if (MFVideoFormat_AYUV == image_format) {
    type = TRA_IMAGE_FORMAT_AYUV;
    return 0;
  } else if (MFVideoFormat_YUY2 == image_format) {
    type = TRA_IMAGE_FORMAT_YUY2;
    return 0;
  } else if (MFVideoFormat_YVYU == image_format) {
    type = TRA_IMAGE_FORMAT_YVYU;
    return 0;
  } else if (MFVideoFormat_YVU9 == image_format) {
    type = TRA_IMAGE_FORMAT_YVU9;
    return 0;
  } else if (MFVideoFormat_NV11 == image_format) {
    type = TRA_IMAGE_FORMAT_NV11;
    return 0;
  } else if (MFVideoFormat_IYUV == image_format) {
    type = TRA_IMAGE_FORMAT_IYUV;
    return 0;
  } else if (MFVideoFormat_Y210 == image_format) {
    type = TRA_IMAGE_FORMAT_Y210;
    return 0;
  } else if (MFVideoFormat_Y216 == image_format) {
    type = TRA_IMAGE_FORMAT_Y216;
    return 0;
  } else if (MFVideoFormat_Y410 == image_format) {
    type = TRA_IMAGE_FORMAT_Y410;
    return 0;
  } else if (MFVideoFormat_Y416 == image_format) {
    type = TRA_IMAGE_FORMAT_Y416;
    return 0;
  } else if (MFVideoFormat_Y41P == image_format) {
    type = TRA_IMAGE_FORMAT_Y41P;
    return 0;
  } else if (MFVideoFormat_Y41T == image_format) {
    type = TRA_IMAGE_FORMAT_Y41T;
    return 0;
  } else if (MFVideoFormat_Y42T == image_format) {
    type = TRA_IMAGE_FORMAT_Y42T;
    return 0;
  } else if (MFVideoFormat_P210 == image_format) {
    type = TRA_IMAGE_FORMAT_P210;
    return 0;
  } else if (MFVideoFormat_P010 == image_format) {
    type = TRA_IMAGE_FORMAT_P010;
    return 0;
  } else if (MFVideoFormat_P016 == image_format) {
    type = TRA_IMAGE_FORMAT_P016;
    return 0;
  } else if (MFVideoFormat_v216 == image_format) {
    type = TRA_IMAGE_FORMAT_v216;
    return 0;
  } else if (MFVideoFormat_v410 == image_format) {
    type = TRA_IMAGE_FORMAT_v410;
    return 0;
  } else if (MFVideoFormat_MP43 == image_format) {
    type = TRA_IMAGE_FORMAT_MP43;
    return 0;
  } else if (MFVideoFormat_MP4S == image_format) {
    type = TRA_IMAGE_FORMAT_MP4S;
    return 0;
  } else if (MFVideoFormat_M4S2 == image_format) {
    type = TRA_IMAGE_FORMAT_M4S2;
    return 0;
  } else if (MFVideoFormat_MP4V == image_format) {
    type = TRA_IMAGE_FORMAT_MP4V;
    return 0;
  } else if (MFVideoFormat_WMV1 == image_format) {
    type = TRA_IMAGE_FORMAT_WMV1;
    return 0;
  } else if (MFVideoFormat_WMV2 == image_format) {
    type = TRA_IMAGE_FORMAT_WMV2;
    return 0;
  } else if (MFVideoFormat_WMV3 == image_format) {
    type = TRA_IMAGE_FORMAT_WMV3;
    return 0;
  } else if (MFVideoFormat_WVC1 == image_format) {
    type = TRA_IMAGE_FORMAT_WVC1;
    return 0;
  } else if (MFVideoFormat_MSS1 == image_format) {
    type = TRA_IMAGE_FORMAT_MSS1;
    return 0;
  } else if (MFVideoFormat_MSS2 == image_format) {
    type = TRA_IMAGE_FORMAT_MSS2;
    return 0;
  } else if (MFVideoFormat_MPG1 == image_format) {
    type = TRA_IMAGE_FORMAT_MPG1;
    return 0;
  } else if (MFVideoFormat_DVSL == image_format) {
    type = TRA_IMAGE_FORMAT_DVSL;
    return 0;
  } else if (MFVideoFormat_DVSD == image_format) {
    type = TRA_IMAGE_FORMAT_DVSD;
    return 0;
  } else if (MFVideoFormat_DVHD == image_format) {
    type = TRA_IMAGE_FORMAT_DVHD;
    return 0;
  } else if (MFVideoFormat_DV25 == image_format) {
    type = TRA_IMAGE_FORMAT_DV25;
    return 0;
  } else if (MFVideoFormat_DV50 == image_format) {
    type = TRA_IMAGE_FORMAT_DV50;
    return 0;
  } else if (MFVideoFormat_DVH1 == image_format) {
    type = TRA_IMAGE_FORMAT_DVH1;
    return 0;
  } else if (MFVideoFormat_DVC == image_format) {
    type = TRA_IMAGE_FORMAT_DVC;
    return 0;
  } else if (MFVideoFormat_H265 == image_format) {
    type = TRA_IMAGE_FORMAT_H265;
    return 0;
  } else if (MFVideoFormat_MJPG == image_format) {
    type = TRA_IMAGE_FORMAT_MJPG;
    return 0;
  } else if (MFVideoFormat_420O == image_format) {
    type = TRA_IMAGE_FORMAT_4200;
    return 0;
  } else if (MFVideoFormat_HEVC == image_format) {
    type = TRA_IMAGE_FORMAT_HEVC;
    return 0;
  } else if (MFVideoFormat_HEVC_ES == image_format) {
    type = TRA_IMAGE_FORMAT_HEVC_ES;
    return 0;
  } else if (MFVideoFormat_VP80 == image_format) {
    type = TRA_IMAGE_FORMAT_VP80;
    return 0;
  } else if (MFVideoFormat_VP90 == image_format) {
    type = TRA_IMAGE_FORMAT_VP90;
    return 0;
  } else {
    return -1;
  }
}
