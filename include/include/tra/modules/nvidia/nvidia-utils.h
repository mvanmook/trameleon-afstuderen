#ifndef TRA_NVIDIA_UTILS_H
#define TRA_NVIDIA_UTILS_H

/* ------------------------------------------------------- */

#include <tra/modules/nvidia/nvEncodeAPI.h>
#include <tra/modules/nvidia/cuviddec.h>
#include <tra/modules/nvidia/nvcuvid.h>

/* ------------------------------------------------------- */

int tra_nv_imageformat_to_bufferformat(uint32_t imageFormat, NV_ENC_BUFFER_FORMAT* bufFormat);

/* ------------------------------------------------------- */

int tra_nv_print_initparams(NV_ENC_INITIALIZE_PARAMS* cfg);
int tra_nv_print_videoformat(CUVIDEOFORMAT* fmt);
int tra_nv_print_decodecreateinfo(CUVIDDECODECREATEINFO* info);
int tra_nv_print_result(CUresult result);
int tra_nv_print_parserdispinfo(CUVIDPARSERDISPINFO* info);
const char* tra_nv_status_to_string(NVENCSTATUS status);
const char* tra_nv_codecguid_to_string(GUID* guid);
const char* tra_nv_presetguid_to_string(GUID* guid);
const char* tra_nv_profileguid_to_string(GUID* guid);
const char* tra_nv_framefieldmode_to_string(NV_ENC_PARAMS_FRAME_FIELD_MODE mode);
const char* tra_nv_mvprecision_to_string(NV_ENC_MV_PRECISION prec);
const char* tra_nv_rcmode_to_string(NV_ENC_PARAMS_RC_MODE mode);
const char* tra_nv_tuninginfo_to_string(NV_ENC_TUNING_INFO info);
const char* tra_nv_bufferformat_to_string(NV_ENC_BUFFER_FORMAT fmt);
const char* tra_nv_cudavideocodec_to_string(cudaVideoCodec codec);
const char* tra_nv_cudachromaformat_to_string(cudaVideoChromaFormat chroma);
const char* tra_nv_cudavideosurfaceformat_to_string(cudaVideoSurfaceFormat fmt);
const char* tra_nv_cudavideodeinterlacemode_to_string(cudaVideoDeinterlaceMode mode);

/* ------------------------------------------------------- */

#endif
