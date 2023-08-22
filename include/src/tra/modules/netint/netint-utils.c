/* ------------------------------------------------------- */

#include <ni_device_api_logan.h>
#include <ni_rsrc_api_logan.h>
#include <ni_util_logan.h>
#include <tra/modules/netint/netint-utils.h>
#include <tra/log.h>
#include <tra/types.h>

/* ------------------------------------------------------- */

int netint_print_encoderparams(ni_logan_encoder_params_t* params) {

  if (NULL == params) {
    TRAE("Cannot print the params as the given `params` is NULL.");
    return -1;
  }

  TRAD("ni_logan_encoder_params");
  TRAD("  log: %d", params->log);
  TRAD("  preset: %d", params->preset);
  TRAD("  fps_number: %u:", params->fps_number);
  TRAD("  fps_denominator: %u", params->fps_denominator);  
  TRAD("  source_width: %d", params->source_width);
  TRAD("  source_height: %d", params->source_height);
  TRAD("  bitrate: %d", params->bitrate);  
  TRAD("  roi_demo_mode: %d", params->roi_demo_mode);
  TRAD("  reconf_demo_mode: %d", params->reconf_demo_mode);
  TRAD("  force_pic_qp_demo_mode: %d", params->force_pic_qp_demo_mode);  
  TRAD("  low_delay_mode: %d", params->low_delay_mode);
  TRAD("  padding: %d", params->padding);
  TRAD("  generate_enc_hdrs: %d", params->generate_enc_hdrs);  
  TRAD("  use_low_delay_poc_type: %d", params->use_low_delay_poc_type);
  TRAD("  strict_timeout_mode: %d", params->strict_timeout_mode);
  TRAD("  dolby_vision_profile: %d", params->dolby_vision_profile);  
  TRAD("  hrd_enable: %d", params->hrd_enable);
  TRAD("  enable_aud: %d", params->enable_aud);
  TRAD("  force_frame_type: %d", params->force_frame_type);  
  TRAD("  hdrEnableVUI: %d", params->hdrEnableVUI);
  TRAD("  crf: %d", params->crf);
  TRAD("  cbr: %d", params->cbr);  
  TRAD("  cacheRoi: %d", params->cacheRoi);
  TRAD("  enable_vfr: %d", params->enable_vfr);
  TRAD("  ui32flushGop: %u", params->ui32flushGop);  
  TRAD("  ui32MinIntraRefreshCycle: %u", params->ui32minIntraRefreshCycle);
  TRAD("  ui32VuiDataSizeInBits: %u", params->ui32VuiDataSizeBits);
  TRAD("  ui32VuiDataSizeBytes: %u", params->ui32VuiDataSizeBytes);  
  TRAD("  pos_num_units_in_tick: %u", params->pos_num_units_in_tick);
  TRAD("  pos_time_scale: %u", params->pos_time_scale);
  TRAD("  nb_save_pkt: %u", params->nb_save_pkt);  
  TRAD("  hwframes: %u", params->hwframes);  
  TRAD("");

  return 0;
}

/* ------------------------------------------------------- */

int netint_print_sessioncontext(ni_logan_session_context_t* session) {

  if (NULL == session) {
    TRAE("Cannot print the session as it's NULL.");
    return -1;
  }

  TRAD("ni_logan_session_context");
  TRAD("  prev_read_frame_time: %llu", session->prev_read_frame_time);
  TRAD("  last_pts: %lld", session->last_pts);
  TRAD("  last_dts: %lld", session->last_dts);
  TRAD("  enc_pts_r_idx: %lld", session->enc_pts_r_idx);
  TRAD("  enc_pts_w_idx: %lld", session->enc_pts_w_idx);
  TRAD("  pts_correction_num_faulty_dts: %d", session->pts_correction_num_faulty_dts);
  TRAD("  pts_correction_last_dts: %lld", session->pts_correction_last_dts);
  TRAD("  pts_correction_num_faulty_pts: %d", session->pts_correction_num_faulty_pts);
  TRAD("  pts_correction_last_pts: %lld", session->pts_correction_last_pts);
  TRAD("  pkt_index: %llu", session->pkt_index);
  TRAD("  is_dec_pkt_512_aligned: %d", session->is_dec_pkt_512_aligned);
  TRAD("  template_config_id: %u", session->template_config_id);
  TRAD("  p_session_config: %p", session->p_session_config);
  TRAD("  max_nvme_io_size: %u", session->max_nvme_io_size);
  TRAD("  hw_id: %d", session->hw_id);
  TRAD("  session_id: %u", session->session_id);
  TRAD("  session_timestamp: %llu", session->session_timestamp);
  TRAD("  device_type: %u", session->device_type);
  TRAD("  codec_format: %s",  netint_codecformat_to_string(session->codec_format));
  TRAD("  src_bit_depth: %d", session->src_bit_depth);
  TRAD("  src_endian: %d", session->src_endian);
  TRAD("  bit_depth_factor: %d", session->bit_depth_factor);
  TRAD("  p_leftover: %p", session->p_leftover);
  TRAD("  prev_size: %d", session->prev_size);
  TRAD("  sent_size: %u", session->sent_size);
  TRAD("");
  
  /* @todo see ni_logan_device_api.h */
  
  return 0;
}

/* ------------------------------------------------------- */

const char* netint_codecformat_to_string(uint32_t fmt) {
  switch (fmt) {
    case NI_LOGAN_CODEC_FORMAT_H264: { return "NI_LOGAN_CODEC_FORMAT_H264"; }
    case NI_LOGAN_CODEC_FORMAT_H265: { return "NI_LOGAN_CODEC_FORMAT_H265"; }
    default:                   { return "UNKNOWN";              } 
  }
}

/* ------------------------------------------------------- */

const char* netint_retcode_to_string(ni_logan_retcode_t code) {

  switch (code) { 
    case NI_LOGAN_RETCODE_SUCCESS:                           { return "NI_LOGAN_RETCODE_SUCCESS";                           }
    case NI_LOGAN_RETCODE_FAILURE:                           { return "NI_LOGAN_RETCODE_FAILURE";                           }
    case NI_LOGAN_RETCODE_INVALID_PARAM:                     { return "NI_LOGAN_RETCODE_INVALID_PARAM";                     }
    case NI_LOGAN_RETCODE_ERROR_MEM_ALOC:                    { return "NI_LOGAN_RETCODE_ERROR_MEM_ALOC";                    }
    case NI_LOGAN_RETCODE_ERROR_NVME_CMD_FAILED:             { return "NI_LOGAN_RETCODE_ERROR_NVME_CMD_FAILED";             }
    case NI_LOGAN_RETCODE_ERROR_INVALID_SESSION:             { return "NI_LOGAN_RETCODE_ERROR_INVALID_SESSION";             }
    case NI_LOGAN_RETCODE_ERROR_RESOURCE_UNAVAILABLE:        { return "NI_LOGAN_RETCODE_ERROR_RESOURCE_UNAVAILABLE";        }
    case NI_LOGAN_RETCODE_PARAM_INVALID_NAME:                { return "NI_LOGAN_RETCODE_PARAM_INVALID_NAME";                }
    case NI_LOGAN_RETCODE_PARAM_INVALID_VALUE:               { return "NI_LOGAN_RETCODE_PARAM_INVALID_VALUE";               }
    case NI_LOGAN_RETCODE_PARAM_ERROR_FRATE:                 { return "NI_LOGAN_RETCODE_PARAM_ERROR_FRATE";                 }
    case NI_LOGAN_RETCODE_PARAM_ERROR_BRATE:                 { return "NI_LOGAN_RETCODE_PARAM_ERROR_BRATE";                 }
    case NI_LOGAN_RETCODE_PARAM_ERROR_TRATE:                 { return "NI_LOGAN_RETCODE_PARAM_ERROR_TRATE";                 }
    case NI_LOGAN_RETCODE_PARAM_ERROR_RC_INIT_DELAY:         { return "NI_LOGAN_RETCODE_PARAM_ERROR_RC_INIT_DELAY";         }
    case NI_LOGAN_RETCODE_PARAM_ERROR_INTRA_PERIOD:          { return "NI_LOGAN_RETCODE_PARAM_ERROR_INTRA_PERIOD";          }
    case NI_LOGAN_RETCODE_PARAM_ERROR_INTRA_QP:              { return "NI_LOGAN_RETCODE_PARAM_ERROR_INTRA_QP";              }
    case NI_LOGAN_RETCODE_PARAM_ERROR_GOP_PRESET:            { return "NI_LOGAN_RETCODE_PARAM_ERROR_GOP_PRESET";            }
    case NI_LOGAN_RETCODE_PARAM_ERROR_CU_SIZE_MODE:          { return "NI_LOGAN_RETCODE_PARAM_ERROR_CU_SIZE_MODE";          }
    case NI_LOGAN_RETCODE_PARAM_ERROR_MX_NUM_MERGE:          { return "NI_LOGAN_RETCODE_PARAM_ERROR_MX_NUM_MERGE";          }
    case NI_LOGAN_RETCODE_PARAM_ERROR_DY_MERGE_8X8_EN:       { return "NI_LOGAN_RETCODE_PARAM_ERROR_DY_MERGE_8X8_EN";       }
    case NI_LOGAN_RETCODE_PARAM_ERROR_DY_MERGE_16X16_EN:     { return "NI_LOGAN_RETCODE_PARAM_ERROR_DY_MERGE_16X16_EN";     }
    case NI_LOGAN_RETCODE_PARAM_ERROR_DY_MERGE_32X32_EN:     { return "NI_LOGAN_RETCODE_PARAM_ERROR_DY_MERGE_32X32_EN";     }
    case NI_LOGAN_RETCODE_PARAM_ERROR_CU_LVL_RC_EN:          { return "NI_LOGAN_RETCODE_PARAM_ERROR_CU_LVL_RC_EN";          }
    case NI_LOGAN_RETCODE_PARAM_ERROR_HVS_QP_EN:             { return "NI_LOGAN_RETCODE_PARAM_ERROR_HVS_QP_EN";             }
    case NI_LOGAN_RETCODE_PARAM_ERROR_HVS_QP_SCL:            { return "NI_LOGAN_RETCODE_PARAM_ERROR_HVS_QP_SCL";            }
    case NI_LOGAN_RETCODE_PARAM_ERROR_MN_QP:                 { return "NI_LOGAN_RETCODE_PARAM_ERROR_MN_QP";                 }
    case NI_LOGAN_RETCODE_PARAM_ERROR_MX_QP:                 { return "NI_LOGAN_RETCODE_PARAM_ERROR_MX_QP";                 }
    case NI_LOGAN_RETCODE_PARAM_ERROR_MX_DELTA_QP:           { return "NI_LOGAN_RETCODE_PARAM_ERROR_MX_DELTA_QP";           }
    case NI_LOGAN_RETCODE_PARAM_ERROR_CONF_WIN_TOP:          { return "NI_LOGAN_RETCODE_PARAM_ERROR_CONF_WIN_TOP";          }
    case NI_LOGAN_RETCODE_PARAM_ERROR_CONF_WIN_BOT:          { return "NI_LOGAN_RETCODE_PARAM_ERROR_CONF_WIN_BOT";          }
    case NI_LOGAN_RETCODE_PARAM_ERROR_CONF_WIN_L:            { return "NI_LOGAN_RETCODE_PARAM_ERROR_CONF_WIN_L";            }
    case NI_LOGAN_RETCODE_PARAM_ERROR_CONF_WIN_R:            { return "NI_LOGAN_RETCODE_PARAM_ERROR_CONF_WIN_R";            }
    case NI_LOGAN_RETCODE_PARAM_ERROR_USR_RMD_ENC_PARAM:     { return "NI_LOGAN_RETCODE_PARAM_ERROR_USR_RMD_ENC_PARAM";     }
    case NI_LOGAN_RETCODE_PARAM_ERROR_BRATE_LT_TRATE:        { return "NI_LOGAN_RETCODE_PARAM_ERROR_BRATE_LT_TRATE";        }
    case NI_LOGAN_RETCODE_PARAM_ERROR_RCINITDELAY:           { return "NI_LOGAN_RETCODE_PARAM_ERROR_RCINITDELAY";           }
    case NI_LOGAN_RETCODE_PARAM_ERROR_RCENABLE:              { return "NI_LOGAN_RETCODE_PARAM_ERROR_RCENABLE";              }
    case NI_LOGAN_RETCODE_PARAM_ERROR_MAXNUMMERGE:           { return "NI_LOGAN_RETCODE_PARAM_ERROR_MAXNUMMERGE";           }
    case NI_LOGAN_RETCODE_PARAM_ERROR_CUSTOM_GOP:            { return "NI_LOGAN_RETCODE_PARAM_ERROR_CUSTOM_GOP";            }
    case NI_LOGAN_RETCODE_PARAM_ERROR_PIC_WIDTH:             { return "NI_LOGAN_RETCODE_PARAM_ERROR_PIC_WIDTH";             }
    case NI_LOGAN_RETCODE_PARAM_ERROR_PIC_HEIGHT:            { return "NI_LOGAN_RETCODE_PARAM_ERROR_PIC_HEIGHT";            }
    case NI_LOGAN_RETCODE_PARAM_ERROR_DECODING_REFRESH_TYPE: { return "NI_LOGAN_RETCODE_PARAM_ERROR_DECODING_REFRESH_TYPE"; }
    case NI_LOGAN_RETCODE_PARAM_ERROR_CUSIZE_MODE_8X8_EN:    { return "NI_LOGAN_RETCODE_PARAM_ERROR_CUSIZE_MODE_8X8_EN";    }
    case NI_LOGAN_RETCODE_PARAM_ERROR_CUSIZE_MODE_16X16_EN:  { return "NI_LOGAN_RETCODE_PARAM_ERROR_CUSIZE_MODE_16X16_EN";  }
    case NI_LOGAN_RETCODE_PARAM_ERROR_CUSIZE_MODE_32X32_EN:  { return "NI_LOGAN_RETCODE_PARAM_ERROR_CUSIZE_MODE_32X32_EN";  }
    case NI_LOGAN_RETCODE_PARAM_ERROR_TOO_BIG:               { return "NI_LOGAN_RETCODE_PARAM_ERROR_TOO_BIG";               }
    case NI_LOGAN_RETCODE_PARAM_ERROR_TOO_SMALL:             { return "NI_LOGAN_RETCODE_PARAM_ERROR_TOO_SMALL";             }
    case NI_LOGAN_RETCODE_PARAM_ERROR_ZERO:                  { return "NI_LOGAN_RETCODE_PARAM_ERROR_ZERO";                  }
    case NI_LOGAN_RETCODE_PARAM_ERROR_OOR:                   { return "NI_LOGAN_RETCODE_PARAM_ERROR_OOR";                   }
    case NI_LOGAN_RETCODE_PARAM_ERROR_WIDTH_TOO_BIG:         { return "NI_LOGAN_RETCODE_PARAM_ERROR_WIDTH_TOO_BIG";         }
    case NI_LOGAN_RETCODE_PARAM_ERROR_WIDTH_TOO_SMALL:       { return "NI_LOGAN_RETCODE_PARAM_ERROR_WIDTH_TOO_SMALL";       }
    case NI_LOGAN_RETCODE_PARAM_ERROR_HEIGHT_TOO_BIG:        { return "NI_LOGAN_RETCODE_PARAM_ERROR_HEIGHT_TOO_BIG";        }
    case NI_LOGAN_RETCODE_PARAM_ERROR_HEIGHT_TOO_SMALL:      { return "NI_LOGAN_RETCODE_PARAM_ERROR_HEIGHT_TOO_SMALL";      }
    case NI_LOGAN_RETCODE_PARAM_ERROR_AREA_TOO_BIG:          { return "NI_LOGAN_RETCODE_PARAM_ERROR_AREA_TOO_BIG";          }
    case NI_LOGAN_RETCODE_ERROR_EXCEED_MAX_NUM_SESSIONS:     { return "NI_LOGAN_RETCODE_ERROR_EXCEED_MAX_NUM_SESSIONS";     }
    case NI_LOGAN_RETCODE_ERROR_GET_DEVICE_POOL:             { return "NI_LOGAN_RETCODE_ERROR_GET_DEVICE_POOL";             }
    case NI_LOGAN_RETCODE_ERROR_LOCK_DOWN_DEVICE:            { return "NI_LOGAN_RETCODE_ERROR_LOCK_DOWN_DEVICE";            }
    case NI_LOGAN_RETCODE_ERROR_UNLOCK_DEVICE:               { return "NI_LOGAN_RETCODE_ERROR_UNLOCK_DEVICE";               }
    case NI_LOGAN_RETCODE_ERROR_OPEN_DEVICE:                 { return "NI_LOGAN_RETCODE_ERROR_OPEN_DEVICE";                 }
    case NI_LOGAN_RETCODE_ERROR_INVALID_HANDLE:              { return "NI_LOGAN_RETCODE_ERROR_INVALID_HANDLE";              }
    case NI_LOGAN_RETCODE_ERROR_INVALID_ALLOCATION_METHOD:   { return "NI_LOGAN_RETCODE_ERROR_INVALID_ALLOCATION_METHOD";   }
    case NI_LOGAN_RETCODE_ERROR_VPU_RECOVERY:                { return "NI_LOGAN_RETCODE_ERROR_VPU_RECOVERY";                }
    case NI_LOGAN_RETCODE_PARAM_GOP_INTRA_INCOMPATIBLE:      { return "NI_LOGAN_RETCODE_PARAM_GOP_INTRA_INCOMPATIBLE";      }
    case NI_LOGAN_RETCODE_NVME_SC_WRITE_BUFFER_FULL:         { return "NI_LOGAN_RETCODE_NVME_SC_WRITE_BUFFER_FULL";         }
    case NI_LOGAN_RETCODE_NVME_SC_RESOURCE_UNAVAILABLE:      { return "NI_LOGAN_RETCODE_NVME_SC_RESOURCE_UNAVAILABLE";      }
    case NI_LOGAN_RETCODE_NVME_SC_RESOURCE_IS_EMPTY:         { return "NI_LOGAN_RETCODE_NVME_SC_RESOURCE_IS_EMPTY";         }
    case NI_LOGAN_RETCODE_NVME_SC_RESOURCE_NOT_FOUND:        { return "NI_LOGAN_RETCODE_NVME_SC_RESOURCE_NOT_FOUND";        }
    case NI_LOGAN_RETCODE_NVME_SC_REQUEST_NOT_COMPLETED:     { return "NI_LOGAN_RETCODE_NVME_SC_REQUEST_NOT_COMPLETED";     }
    case NI_LOGAN_RETCODE_NVME_SC_REQUEST_IN_PROGRESS:       { return "NI_LOGAN_RETCODE_NVME_SC_REQUEST_IN_PROGRESS";       }
    case NI_LOGAN_RETCODE_NVME_SC_INVALID_PARAMETER:         { return "NI_LOGAN_RETCODE_NVME_SC_INVALID_PARAMETER";         }
    case NI_LOGAN_RETCODE_NVME_SC_VPU_RECOVERY:              { return "NI_LOGAN_RETCODE_NVME_SC_VPU_RECOVERY";              }
    case NI_LOGAN_RETCODE_NVME_SC_VPU_RSRC_INSUFFICIENT:     { return "NI_LOGAN_RETCODE_NVME_SC_VPU_RSRC_INSUFFICIENT";     }
    case NI_LOGAN_RETCODE_NVME_SC_VPU_GENERAL_ERROR:         { return "NI_LOGAN_RETCODE_NVME_SC_VPU_GENERAL_ERROR";         }
    case NI_LOGAN_RETCODE_DEFAULT_SESSION_ERR_NO:            { return "NI_LOGAN_RETCODE_DEFAULT_SESSION_ERR_NO";            }
    default:                                           { return "UNKNOWN";                                      }
  }
}

/* ------------------------------------------------------- */

int convert_trameleon_to_netint_codec(uint32_t *codec, uint32_t trameleon_format){
    switch (trameleon_format) {
        case TRA_MEMORY_TYPE_H264:
            *codec = EN_H264;
            return 0;
        case TRA_MEMORY_TYPE_H265:
            *codec = EN_H265;
            return 0;
        default:
            return -1;
    }
}


int convert_trameleon_to_netint_alignment(uint8_t* alignment, uint32_t trameleon_format){
    switch (trameleon_format) {
        case TRA_MEMORY_TYPE_H264:
            *alignment = 1;
            return 0;
        case TRA_MEMORY_TYPE_H265:
            *alignment = 0;
            return 0;
        default:
            return -1;

    }
}


int convert_trameleon_to_netint_codec_format(uint32_t* codec_format, uint32_t trameleon_format){
    switch (trameleon_format) {
        case TRA_MEMORY_TYPE_H264:
            *codec_format = NI_LOGAN_CODEC_FORMAT_H264;
            return 0;
        case TRA_MEMORY_TYPE_H265:
            *codec_format = NI_LOGAN_CODEC_FORMAT_H265;
            return 0;
        default:
            return -1;

    }
}