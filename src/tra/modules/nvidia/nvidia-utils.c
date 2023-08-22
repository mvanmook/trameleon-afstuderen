/* ------------------------------------------------------- */

#include <tra/modules/nvidia/nvidia-utils.h>
#include <tra/types.h>
#include <tra/log.h>

/* ------------------------------------------------------- */

int tra_nv_imageformat_to_bufferformat(uint32_t fmt, NV_ENC_BUFFER_FORMAT* bufFormat) {

  if (NULL == bufFormat) {
    TRAE("Cannot convert the given image format into a NV_ENC_BUFFER_FORMAT as it's NULL.");
    return -1;
  }

  switch (fmt) {
    case TRA_IMAGE_FORMAT_NV12: {
      *bufFormat = NV_ENC_BUFFER_FORMAT_NV12;
      return 0;
    }
  }

  TRAE("Cannot convert the given image format as it's not supported.");
  return -2;
}

/* ------------------------------------------------------- */

int tra_nv_print_videoformat(CUVIDEOFORMAT* fmt) {

  if (NULL == fmt) {
    TRAE("Cannot print the video format, it's NULL.");
    return -1;
  }

  TRAD("CUVIDEOFORMAT");
  TRAD("  .codec: %s", tra_nv_cudavideocodec_to_string(fmt->codec));
  TRAD("  .frame_rate.numerator: %u", fmt->frame_rate.numerator);
  TRAD("  .frame_rate.denominator: %u", fmt->frame_rate.denominator);
  TRAD("  .progressive_sequence: %s", (0 == fmt->progressive_sequence) ? "interlaced" : "progressive");
  TRAD("  .bit_depth_luma_minus8: %u", fmt->bit_depth_luma_minus8);
  TRAD("  .bit_depth_chroma_minus8: %u", fmt->bit_depth_chroma_minus8);
  TRAD("  .min_num_decode_surfaces: %u", fmt->min_num_decode_surfaces);
  TRAD("  .coded_width: %u", fmt->coded_width);
  TRAD("  .coded_height: %u", fmt->coded_height);
  TRAD("  .display_area:");
  TRAD("     .left: %d", fmt->display_area.left);
  TRAD("     .top: %d", fmt->display_area.top);
  TRAD("     .right: %d", fmt->display_area.right);
  TRAD("     .bottom: %d", fmt->display_area.bottom);
  TRAD("  .chroma_format: %s", tra_nv_cudachromaformat_to_string(fmt->chroma_format));
  TRAD("  .bitrate: %u", fmt->bitrate);
  TRAD("  .display_aspect_ratio:");
  TRAD("    .x: %d", fmt->display_aspect_ratio.x);
  TRAD("    .y: %d", fmt->display_aspect_ratio.y);
  
  return 0;
}

const char* tra_nv_cudavideocodec_to_string(cudaVideoCodec codec) {

  switch (codec) {
    case cudaVideoCodec_MPEG1:      { return "cudaVideoCodec_MPEG1";     } 
    case cudaVideoCodec_MPEG2:      { return "cudaVideoCodec_MPEG2";     } 
    case cudaVideoCodec_MPEG4:      { return "cudaVideoCodec_MPEG4";     } 
    case cudaVideoCodec_VC1:        { return "cudaVideoCodec_VC1";       } 
    case cudaVideoCodec_H264:       { return "cudaVideoCodec_H264";      } 
    case cudaVideoCodec_JPEG:       { return "cudaVideoCodec_JPEG";      } 
    case cudaVideoCodec_H264_SVC:   { return "cudaVideoCodec_H264_SVC";  } 
    case cudaVideoCodec_H264_MVC:   { return "cudaVideoCodec_H264_MVC";  } 
    case cudaVideoCodec_HEVC:       { return "cudaVideoCodec_HEVC";      } 
    case cudaVideoCodec_VP8:        { return "cudaVideoCodec_VP8";       } 
    case cudaVideoCodec_VP9:        { return "cudaVideoCodec_VP9";       } 
    case cudaVideoCodec_NumCodecs:  { return "cudaVideoCodec_NumCodecs"; } 
    case cudaVideoCodec_YUV420:     { return "cudaVideoCodec_YUV420";    } 
    case cudaVideoCodec_YV12:       { return "cudaVideoCodec_YV1";       } 
    case cudaVideoCodec_NV12:       { return "cudaVideoCodec_NV1";       } 
    case cudaVideoCodec_YUYV:       { return "cudaVideoCodec_YUY";       } 
    case cudaVideoCodec_UYVY:       { return "cudaVideoCodec_UYV";       } 
    default:                        { return "UNKNOWN";                  }
  }
}

const char* tra_nv_cudachromaformat_to_string(cudaVideoChromaFormat chroma) {

  switch (chroma) {
    case cudaVideoChromaFormat_Monochrome: { return "cudaVideoChromaFormat_Monochrome"; }
    case cudaVideoChromaFormat_420:        { return "cudaVideoChromaFormat_420";        }
    case cudaVideoChromaFormat_422:        { return "cudaVideoChromaFormat_422";        }
    case cudaVideoChromaFormat_444:        { return "cudaVideoChromaFormat_444";        }
    default:                               { return "UNKNOWN";                          }
  }
}

/* ------------------------------------------------------- */

int tra_nv_print_decodecreateinfo(CUVIDDECODECREATEINFO* info) {

  if (NULL == info) {
    TRAE("Cannot print the `CUVIDDECODECREATEINFO` as it's NULL.");
    return -10;
  }

  TRAD("CUVIDDECODECREATEINFO");
  TRAD("  .ulWidth: %u", info->ulWidth);
  TRAD("  .ulHeight: %u", info->ulHeight);
  TRAD("  .ulNumDecodeSurfaces: %u", info->ulNumDecodeSurfaces);
  TRAD("  .CodecType: %s", tra_nv_cudavideocodec_to_string(info->CodecType));
  TRAD("  .ChromaFormat: %s", tra_nv_cudachromaformat_to_string(info->ChromaFormat));
  TRAD("  .ulCreationFlags: %u", info->ulCreationFlags);
  TRAD("  .bitDepthMinus8: %u", info->bitDepthMinus8);
  TRAD("  .ulIntraDecodeOnly: %u", info->ulIntraDecodeOnly);
  TRAD("  .ulMaxWidth: %u", info->ulMaxWidth);
  TRAD("  .ulMaxHeight: %u", info->ulMaxHeight);
  TRAD("  .display_area:");
  TRAD("    .left: %u", info->display_area.left);
  TRAD("    .top: %u", info->display_area.top);
  TRAD("    .right: %u", info->display_area.right);
  TRAD("    .bottom: %u", info->display_area.bottom);
  TRAD("  .OutputFormat: %s", tra_nv_cudavideosurfaceformat_to_string(info->OutputFormat));
  TRAD("  .DeinterlaceMode: %s", tra_nv_cudavideodeinterlacemode_to_string(info->DeinterlaceMode));
  TRAD("  .ulTargetWidth: %u", info->ulTargetWidth);
  TRAD("  .ulTargetHeight: %u", info->ulTargetHeight);
  TRAD("  .ulNumOutputSurfaces: %u", info->ulNumOutputSurfaces);
  TRAD("  .target_rect:");
  TRAD("    .left: %u", info->target_rect.left);
  TRAD("    .top: %u", info->target_rect.top);
  TRAD("    .right: %u", info->target_rect.right);
  TRAD("    .bottom: %u", info->target_rect.bottom);
  TRAD("  .enableHistogram: %u", info->enableHistogram);

  return 0;
}

/* ------------------------------------------------------- */

int tra_nv_print_result(CUresult result) {

  const char* str = NULL;
  
  cuGetErrorString(result, &str);

  if (NULL == str) {
    TRAE("Failed to get the error string for the given `CUresult`.");
    return -10;
  }
  
  TRAE("CUresult: %s.", str);

  return 0;
}

/* ------------------------------------------------------- */

int tra_nv_print_parserdispinfo(CUVIDPARSERDISPINFO* info) {

  if (NULL == info) {
    TRAE("Cannot print the `CUVIDPARSERDISPINFO` as it's NULL.");
    return -10;
  }

  TRAD("CUVIDPARSERDISPINFO");
  TRAD("  .picture_index: %d", info->picture_index);
  TRAD("  .progressive_frame: %d", info->progressive_frame);
  TRAD("  .top_field_first: %d", info->top_field_first);
  TRAD("  .repeat_first_field: %d", info->repeat_first_field);
  TRAD("  .timestamp: %lld", info->timestamp);

  return 0;
}

/* ------------------------------------------------------- */

int tra_nv_print_initparams(NV_ENC_INITIALIZE_PARAMS* cfg) {

  NV_ENC_CONFIG* ec = NULL;
  NV_ENC_RC_PARAMS* rcp = NULL;
    
  if (NULL == cfg) {
    TRAE("Cannot print the `NV_ENC_INITIALIZE_PARAMS` as it's NULL.");
    return -1;
  }

  TRAD("NV_ENC_INITIALIZE_PARAMS");
  TRAD("  .version: %u", cfg->version);
  TRAD("  .encodeGUID: %s", tra_nv_codecguid_to_string(&cfg->encodeGUID));
  TRAD("  .presetGUID: %s", tra_nv_presetguid_to_string(&cfg->presetGUID));
  TRAD("  .encodeWidth: %u", cfg->encodeWidth);
  TRAD("  .encodeHeight: %u", cfg->encodeHeight);
  TRAD("  .darWidth: %u", cfg->darWidth);
  TRAD("  .darHeight: %u", cfg->darHeight);
  TRAD("  .frameRateNum: %u", cfg->frameRateNum);
  TRAD("  .frameRateDen: %u", cfg->frameRateDen);
  TRAD("  .enableEncodeAsync: %u", cfg->enableEncodeAsync);
  TRAD("  .enablePTD: %u", cfg->enablePTD);
  TRAD("  .reportSliceOffsets: %u", cfg->reportSliceOffsets);
  TRAD("  .enableSubFrameWrite: %u", cfg->enableSubFrameWrite);
  TRAD("  .enableExternalMEHints: %u", cfg->enableExternalMEHints);
  TRAD("  .enableMEOnlyMode: %u", cfg->enableMEOnlyMode);
  TRAD("  .enableWeightedPrediction: %u", cfg->enableWeightedPrediction);
  TRAD("  .enableOutputInVidmem: %u", cfg->enableOutputInVidmem);
  TRAD("  .privDataSize: %u", cfg->privDataSize);
  TRAD("  .privData: %p", cfg->privData);

  if (NULL != cfg->encodeConfig) {

    ec = cfg->encodeConfig;
    rcp = &ec->rcParams;
       
    TRAD("  .encodeConfig");
    TRAD("    .version: %u", ec->version);
    TRAD("    .profileGUID: %s", tra_nv_profileguid_to_string(&ec->profileGUID));
    TRAD("    .gopLength: %u", ec->gopLength);
    TRAD("    .frameIntervalP: %d", ec->frameIntervalP);
    TRAD("    .monoChromeEncoding: %u", ec->monoChromeEncoding);
    TRAD("    .frameFieldMode: %s", tra_nv_framefieldmode_to_string(ec->frameFieldMode));
    TRAD("    .mvPrecision: %s", tra_nv_mvprecision_to_string(ec->mvPrecision));
    TRAD("    .rcParams: ");
    TRAD("      .version: %u", rcp->version);
    TRAD("      .rateControlMode: %s", tra_nv_rcmode_to_string(rcp->rateControlMode));
    TRAD("      .constQP: ");
    TRAD("        .qpInterP: %u", rcp->constQP.qpInterP);
    TRAD("        .qpInterB: %u", rcp->constQP.qpInterB);
    TRAD("        .qpIntra: %u", rcp->constQP.qpIntra);
    TRAD("      .averageBitRate: %u", rcp->averageBitRate);
    TRAD("      .maxBitRate: %u", rcp->maxBitRate);
    TRAD("      .vbvBufferSize: %u", rcp->vbvBufferSize);
    TRAD("      .vbvInitialDelay: %u", rcp->vbvInitialDelay);
    TRAD("      .enableMinQP: %u", rcp->enableMinQP);
    TRAD("      .enableMaxQP: %u", rcp->enableMaxQP);
    TRAD("      .enableInitialRCQP: %u", rcp->enableInitialRCQP);
    TRAD("      .enableAQ: %u", rcp->enableAQ);
    TRAD("      .enableLookahead: %u", rcp->enableLookahead);
    TRAD("      .disableIadapt: %u", rcp->disableIadapt);
    TRAD("      .disableBadapt: %u", rcp->disableBadapt);
    TRAD("      .enableTemporalAQ: %u", rcp->enableTemporalAQ);
    TRAD("      .zeroReorderDelay: %u", rcp->zeroReorderDelay);
    TRAD("      .enableNonRefP: %u", rcp->enableNonRefP);
    TRAD("      .strictGOPTarget: %u", rcp->strictGOPTarget);
    TRAD("      .aqStrength: %u", rcp->aqStrength);
    TRAD("      .minQP:");
    TRAD("         .qpInterP: %u", rcp->minQP.qpInterP);
    TRAD("         .qpInterB: %u", rcp->minQP.qpInterB);
    TRAD("         .qpIntra: %u", rcp->minQP.qpIntra);
    TRAD("      .maxQP:");
    TRAD("         .qpInterP: %u", rcp->maxQP.qpInterP);
    TRAD("         .qpInterB: %u", rcp->maxQP.qpInterB);
    TRAD("         .qpIntra: %u", rcp->maxQP.qpIntra);
    TRAD("      .initialRCQP:");
    TRAD("         .qpInterP: %u", rcp->initialRCQP.qpInterP);
    TRAD("         .qpInterB: %u", rcp->initialRCQP.qpInterB);
    TRAD("         .qpIntra: %u", rcp->initialRCQP.qpIntra);
    TRAD("      .temporallayerIdxMask: %u", rcp->temporallayerIdxMask);
    TRAD("      .targetQuality: %u", rcp->targetQuality);
    TRAD("      .targetQualityLSB: %u", rcp->targetQualityLSB);
    TRAD("      .lookaheadDepth: %u", rcp->lookaheadDepth);
    TRAD("      .lowDelayKeyFrameScale: %u", rcp->lowDelayKeyFrameScale);
    TRAD("      .qpMapMode: @todo");
    TRAD("      .multiPass: @todo");
    TRAD("      .alphaLayerBitrateRatio: %u", rcp->alphaLayerBitrateRatio);
    TRAD("      .cbQPIndexOffset: %u", rcp->cbQPIndexOffset);
    TRAD("      .crQPIndexOffset: %u", rcp->crQPIndexOffset);
    TRAD("    .encodeCodecConfig: @todo");
  }
  
  TRAD("  .maxEncodeWidth: %u", cfg->maxEncodeWidth);
  TRAD("  .maxEncodeHeight: %u", cfg->maxEncodeHeight);
  TRAD("  .tuningInfo: %s", tra_nv_tuninginfo_to_string(cfg->tuningInfo));
  TRAD("  .bufferFormat: %s", tra_nv_bufferformat_to_string(cfg->bufferFormat));

  return 0;
}

const char* tra_nv_status_to_string(NVENCSTATUS status) {
  
  switch (status) {
    case NV_ENC_SUCCESS:                      { return "NV_ENC_SUCCESS";                      }
    case NV_ENC_ERR_NO_ENCODE_DEVICE:         { return "NV_ENC_ERR_NO_ENCODE_DEVICE";         }
    case NV_ENC_ERR_UNSUPPORTED_DEVICE:       { return "NV_ENC_ERR_UNSUPPORTED_DEVICE";       }
    case NV_ENC_ERR_INVALID_ENCODERDEVICE:    { return "NV_ENC_ERR_INVALID_ENCODERDEVICE";    }
    case NV_ENC_ERR_INVALID_DEVICE:           { return "NV_ENC_ERR_INVALID_DEVICE";           }
    case NV_ENC_ERR_DEVICE_NOT_EXIST:         { return "NV_ENC_ERR_DEVICE_NOT_EXIST";         }
    case NV_ENC_ERR_INVALID_PTR:              { return "NV_ENC_ERR_INVALID_PTR";              }
    case NV_ENC_ERR_INVALID_EVENT:            { return "NV_ENC_ERR_INVALID_EVENT";            }
    case NV_ENC_ERR_INVALID_PARAM:            { return "NV_ENC_ERR_INVALID_PARAM";            }
    case NV_ENC_ERR_INVALID_CALL:             { return "NV_ENC_ERR_INVALID_CALL";             }
    case NV_ENC_ERR_OUT_OF_MEMORY:            { return "NV_ENC_ERR_OUT_OF_MEMORY";            }
    case NV_ENC_ERR_ENCODER_NOT_INITIALIZED:  { return "NV_ENC_ERR_ENCODER_NOT_INITIALIZED";  }
    case NV_ENC_ERR_UNSUPPORTED_PARAM:        { return "NV_ENC_ERR_UNSUPPORTED_PARAM";        }
    case NV_ENC_ERR_LOCK_BUSY:                { return "NV_ENC_ERR_LOCK_BUSY";                }
    case NV_ENC_ERR_NOT_ENOUGH_BUFFER:        { return "NV_ENC_ERR_NOT_ENOUGH_BUFFER";        }
    case NV_ENC_ERR_INVALID_VERSION:          { return "NV_ENC_ERR_INVALID_VERSION";          }
    case NV_ENC_ERR_MAP_FAILED:               { return "NV_ENC_ERR_MAP_FAILED";               }
    case NV_ENC_ERR_NEED_MORE_INPUT:          { return "NV_ENC_ERR_NEED_MORE_INPUT";          }
    case NV_ENC_ERR_ENCODER_BUSY:             { return "NV_ENC_ERR_ENCODER_BUSY";             }
    case NV_ENC_ERR_EVENT_NOT_REGISTERD:      { return "NV_ENC_ERR_EVENT_NOT_REGISTERD";      }
    case NV_ENC_ERR_GENERIC:                  { return "NV_ENC_ERR_GENERIC";                  }
    case NV_ENC_ERR_INCOMPATIBLE_CLIENT_KEY:  { return "NV_ENC_ERR_INCOMPATIBLE_CLIENT_KEY";  }
    case NV_ENC_ERR_UNIMPLEMENTED:            { return "NV_ENC_ERR_UNIMPLEMENTED";            }
    case NV_ENC_ERR_RESOURCE_REGISTER_FAILED: { return "NV_ENC_ERR_RESOURCE_REGISTER_FAILED"; }
    case NV_ENC_ERR_RESOURCE_NOT_REGISTERED:  { return "NV_ENC_ERR_RESOURCE_NOT_REGISTERED";  }
    case NV_ENC_ERR_RESOURCE_NOT_MAPPED:      { return "NV_ENC_ERR_RESOURCE_NOT_MAPPED";      }
    default:                                  { return "UNKNOWN";                             }
  }
}

const char* tra_nv_codecguid_to_string(GUID* guid) {

  if (NULL == guid) {
    TRAE("Given GUID is NULL, cannot convert it to a string.");
    return "UNKNOWN";
  }

  if (0 == memcmp(&NV_ENC_CODEC_H264_GUID, guid, sizeof(*guid))) {
    return "NV_ENC_CODEC_H264_GUID";
  }

  if (0 == memcmp(&NV_ENC_CODEC_HEVC_GUID, guid, sizeof(*guid))) {
    return "NV_ENC_CODEC_HEVC_GUID";
  }

  return "UNKNOWN";
}

const char* tra_nv_presetguid_to_string(GUID* guid) {

  if (NULL == guid) {
    TRAE("Given GUID is NULL, cannot convert it to a string.");
    return "UNKNOWN";
  }

  if (0 == memcmp(&NV_ENC_PRESET_DEFAULT_GUID, guid, sizeof(*guid))) {
    return "NV_ENC_PRESET_DEFAULT_GUID";
  }
  
  if (0 == memcmp(&NV_ENC_PRESET_HP_GUID, guid, sizeof(*guid))) {
    return "NV_ENC_PRESET_HP_GUID";
  }
  
  if (0 == memcmp(&NV_ENC_PRESET_HQ_GUID, guid, sizeof(*guid))) {
    return "NV_ENC_PRESET_HQ_GUID";
  }
  
  if (0 == memcmp(&NV_ENC_PRESET_BD_GUID, guid, sizeof(*guid))) {
    return "NV_ENC_PRESET_BD_GUID";
  }
  
  if (0 == memcmp(&NV_ENC_PRESET_LOW_LATENCY_DEFAULT_GUID, guid, sizeof(*guid))) {
    return "NV_ENC_PRESET_LOW_LATENCY_DEFAULT_GUID";
  }
  
  if (0 == memcmp(&NV_ENC_PRESET_LOW_LATENCY_HQ_GUID, guid, sizeof(*guid))) {
    return "NV_ENC_PRESET_LOW_LATENCY_HQ_GUID";
  }
  
  if (0 == memcmp(&NV_ENC_PRESET_LOW_LATENCY_HP_GUID, guid, sizeof(*guid))) {
    return "NV_ENC_PRESET_LOW_LATENCY_HP_GUID";
  }
  
  if (0 == memcmp(&NV_ENC_PRESET_LOSSLESS_DEFAULT_GUID, guid, sizeof(*guid))) {
    return "NV_ENC_PRESET_LOSSLESS_DEFAULT_GUID";
  }
  
  if (0 == memcmp(&NV_ENC_PRESET_LOSSLESS_HP_GUID, guid, sizeof(*guid))) {
    return "NV_ENC_PRESET_LOSSLESS_HP_GUID";
  }
  
  if (0 == memcmp(&NV_ENC_PRESET_P1_GUID, guid, sizeof(*guid))) {
    return "NV_ENC_PRESET_P1_GUID";
  }
  
  if (0 == memcmp(&NV_ENC_PRESET_P2_GUID, guid, sizeof(*guid))) {
    return "NV_ENC_PRESET_P2_GUID";
  }
  
  if (0 == memcmp(&NV_ENC_PRESET_P3_GUID, guid, sizeof(*guid))) {
    return "NV_ENC_PRESET_P3_GUID";
  }
  
  if (0 == memcmp(&NV_ENC_PRESET_P4_GUID, guid, sizeof(*guid))) {
    return "NV_ENC_PRESET_P4_GUID";
  }
  
  if (0 == memcmp(&NV_ENC_PRESET_P5_GUID, guid, sizeof(*guid))) {
    return "NV_ENC_PRESET_P5_GUID";
  }
  
  if (0 == memcmp(&NV_ENC_PRESET_P6_GUID, guid, sizeof(*guid))) {
    return "NV_ENC_PRESET_P6_GUID";
  }
  
  if (0 == memcmp(&NV_ENC_PRESET_P7_GUID, guid, sizeof(*guid))) {
    return "NV_ENC_PRESET_P7_GUID";
  }

  return "UNKNOWN";
}

const char* tra_nv_profileguid_to_string(GUID* guid) {

  if (NULL == guid) {
    TRAE("Cannot convert the given profile GUID to a string as it's NULL.");
    return "UNKNOWN";
  }

  if (0 == memcmp(&NV_ENC_CODEC_PROFILE_AUTOSELECT_GUID, guid, sizeof(*guid))) {
    return "NV_ENC_CODEC_PROFILE_AUTOSELECT_GUID";
  }
  
  if (0 == memcmp(&NV_ENC_H264_PROFILE_BASELINE_GUID, guid, sizeof(*guid))) {
    return "NV_ENC_H264_PROFILE_BASELINE_GUID";
  }
  
  if (0 == memcmp(&NV_ENC_H264_PROFILE_MAIN_GUID, guid, sizeof(*guid))) {
    return "NV_ENC_H264_PROFILE_MAIN_GUID";
  }
  
  if (0 == memcmp(&NV_ENC_H264_PROFILE_HIGH_GUID, guid, sizeof(*guid))) {
    return "NV_ENC_H264_PROFILE_HIGH_GUID";
  }
  
  if (0 == memcmp(&NV_ENC_H264_PROFILE_HIGH_444_GUID, guid, sizeof(*guid))) {
    return "NV_ENC_H264_PROFILE_HIGH_444_GUID";
  }
  
  if (0 == memcmp(&NV_ENC_H264_PROFILE_STEREO_GUID, guid, sizeof(*guid))) {
    return "NV_ENC_H264_PROFILE_STEREO_GUID";
  }
  
  if (0 == memcmp(&NV_ENC_H264_PROFILE_PROGRESSIVE_HIGH_GUID, guid, sizeof(*guid))) {
    return "NV_ENC_H264_PROFILE_PROGRESSIVE_HIGH_GUID";
  }
  
  if (0 == memcmp(&NV_ENC_H264_PROFILE_CONSTRAINED_HIGH_GUID, guid, sizeof(*guid))) {
    return "NV_ENC_H264_PROFILE_CONSTRAINED_HIGH_GUID";
  }
  
  if (0 == memcmp(&NV_ENC_HEVC_PROFILE_MAIN_GUID, guid, sizeof(*guid))) {
    return "NV_ENC_HEVC_PROFILE_MAIN_GUID";
  }
  
  if (0 == memcmp(&NV_ENC_HEVC_PROFILE_MAIN10_GUID, guid, sizeof(*guid))) {
    return "NV_ENC_HEVC_PROFILE_MAIN10_GUID";
  }

  if (0 == memcmp(&NV_ENC_HEVC_PROFILE_FREXT_GUID, guid, sizeof(*guid))) {
    return "NV_ENC_HEVC_PROFILE_FREXT_GUID";
  }

  return "UNKNOWN";
}

const char* tra_nv_framefieldmode_to_string(NV_ENC_PARAMS_FRAME_FIELD_MODE mode) {
  
  switch (mode) {
    case NV_ENC_PARAMS_FRAME_FIELD_MODE_FRAME: { return "NV_ENC_PARAMS_FRAME_FIELD_MODE_FRAME"; }
    case NV_ENC_PARAMS_FRAME_FIELD_MODE_FIELD: { return "NV_ENC_PARAMS_FRAME_FIELD_MODE_FIELD"; }
    case NV_ENC_PARAMS_FRAME_FIELD_MODE_MBAFF: { return "NV_ENC_PARAMS_FRAME_FIELD_MODE_MBAFF"; }
    default:                                   { return "UNKNOWN";                              }
  }
}

const char* tra_nv_mvprecision_to_string(NV_ENC_MV_PRECISION prec) {

  switch (prec) {
    case NV_ENC_MV_PRECISION_DEFAULT:     { return "NV_ENC_MV_PRECISION_DEFAULT";     }
    case NV_ENC_MV_PRECISION_FULL_PEL:    { return "NV_ENC_MV_PRECISION_FULL_PEL";    }
    case NV_ENC_MV_PRECISION_HALF_PEL:    { return "NV_ENC_MV_PRECISION_HALF_PEL";    }
    case NV_ENC_MV_PRECISION_QUARTER_PEL: { return "NV_ENC_MV_PRECISION_QUARTER_PEL"; }
    default:                              { return "UNKNOWN";                         }
  }
}

const char* tra_nv_rcmode_to_string(NV_ENC_PARAMS_RC_MODE mode) {
  
  switch (mode) {
    case NV_ENC_PARAMS_RC_CONSTQP:         { return "NV_ENC_PARAMS_RC_CONSTQP";         }
    case NV_ENC_PARAMS_RC_VBR:             { return "NV_ENC_PARAMS_RC_VBR";             }
    case NV_ENC_PARAMS_RC_CBR:             { return "NV_ENC_PARAMS_RC_CBR";             }
    case NV_ENC_PARAMS_RC_CBR_LOWDELAY_HQ: { return "NV_ENC_PARAMS_RC_CBR_LOWDELAY_HQ"; }
    case NV_ENC_PARAMS_RC_CBR_HQ:          { return "NV_ENC_PARAMS_RC_CBR_HQ";          }
    case NV_ENC_PARAMS_RC_VBR_HQ:          { return "NV_ENC_PARAMS_RC_VBR_HQ";          }
    default:                               { return "UNKNOWN";                          }
  }
}

const char* tra_nv_tuninginfo_to_string(NV_ENC_TUNING_INFO info) {
  
  switch (info) {
    case NV_ENC_TUNING_INFO_UNDEFINED:         { return "NV_ENC_TUNING_INFO_UNDEFINED";         } 
    case NV_ENC_TUNING_INFO_HIGH_QUALITY:      { return "NV_ENC_TUNING_INFO_HIGH_QUALITY";      } 
    case NV_ENC_TUNING_INFO_LOW_LATENCY:       { return "NV_ENC_TUNING_INFO_LOW_LATENCY";       } 
    case NV_ENC_TUNING_INFO_ULTRA_LOW_LATENCY: { return "NV_ENC_TUNING_INFO_ULTRA_LOW_LATENCY"; } 
    case NV_ENC_TUNING_INFO_LOSSLESS:          { return "NV_ENC_TUNING_INFO_LOSSLESS";          } 
    case NV_ENC_TUNING_INFO_COUNT:             { return "NV_ENC_TUNING_INFO_COUNT";             }
    default:                                   { return "UNKNOWN";                              }
  }
}

const char* tra_nv_bufferformat_to_string(NV_ENC_BUFFER_FORMAT fmt) {
  
  switch (fmt) {
    case NV_ENC_BUFFER_FORMAT_UNDEFINED:    { return "NV_ENC_BUFFER_FORMAT_UNDEFINED";    }
    case NV_ENC_BUFFER_FORMAT_NV12:         { return "NV_ENC_BUFFER_FORMAT_NV12";         }
    case NV_ENC_BUFFER_FORMAT_YV12:         { return "NV_ENC_BUFFER_FORMAT_YV12";         }
    case NV_ENC_BUFFER_FORMAT_IYUV:         { return "NV_ENC_BUFFER_FORMAT_IYUV";         }
    case NV_ENC_BUFFER_FORMAT_YUV444:       { return "NV_ENC_BUFFER_FORMAT_YUV444";       }
    case NV_ENC_BUFFER_FORMAT_YUV420_10BIT: { return "NV_ENC_BUFFER_FORMAT_YUV420_10BIT"; }
    case NV_ENC_BUFFER_FORMAT_YUV444_10BIT: { return "NV_ENC_BUFFER_FORMAT_YUV444_10BIT"; }
    case NV_ENC_BUFFER_FORMAT_ARGB:         { return "NV_ENC_BUFFER_FORMAT_ARGB";         }
    case NV_ENC_BUFFER_FORMAT_ARGB10:       { return "NV_ENC_BUFFER_FORMAT_ARGB10";       }
    case NV_ENC_BUFFER_FORMAT_AYUV:         { return "NV_ENC_BUFFER_FORMAT_AYUV";         }
    case NV_ENC_BUFFER_FORMAT_ABGR:         { return "NV_ENC_BUFFER_FORMAT_ABGR";         }
    case NV_ENC_BUFFER_FORMAT_ABGR10:       { return "NV_ENC_BUFFER_FORMAT_ABGR10";       }
    case NV_ENC_BUFFER_FORMAT_U8:           { return "NV_ENC_BUFFER_FORMAT_U8";           }
    default:                                { return "UNKNOWN";                           }
  }
}

const char* tra_nv_cudavideosurfaceformat_to_string(cudaVideoSurfaceFormat fmt) {

  switch (fmt) {
    case cudaVideoSurfaceFormat_NV12:         { return "cudaVideoSurfaceFormat_NV12";         }
    case cudaVideoSurfaceFormat_P016:         { return "cudaVideoSurfaceFormat_P016";         }
    case cudaVideoSurfaceFormat_YUV444:       { return "cudaVideoSurfaceFormat_YUV444";       }
    case cudaVideoSurfaceFormat_YUV444_16Bit: { return "cudaVideoSurfaceFormat_YUV444_16Bit"; }
    default:                                  { return "UNKNOWN";                             }
  }
}

const char* tra_nv_cudavideodeinterlacemode_to_string(cudaVideoDeinterlaceMode mode) {

  switch (mode ) {
    case cudaVideoDeinterlaceMode_Weave:    { return "cudaVideoDeinterlaceMode_Weave";    }
    case cudaVideoDeinterlaceMode_Bob:      { return "cudaVideoDeinterlaceMode_Bob";      }
    case cudaVideoDeinterlaceMode_Adaptive: { return "cudaVideoDeinterlaceMode_Adaptive"; }
    default:                                { return "UNKNOWN";                           }
  }
}
