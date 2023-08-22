#ifndef TRA_AVC_H
#define TRA_AVC_H

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

  H264 / AVC 
  ==========

  GENERAL INFO:

    This file contains functions that can be used to parse h264
    bitstreams. These features were created while working on the
    VAAPI based h264 decoder. We follow the 2016 H264 spec as
    much as possible.

 */

/* ------------------------------------------------------- */

#include <stdint.h>

/* ------------------------------------------------------- */

#define TRA_NAL_TYPE_UNSPECIFIED                     0
#define TRA_NAL_TYPE_CODED_SLICE_NON_IDR             1
#define TRA_NAL_TYPE_CODED_SLICE_DATA_PARTITION_A    2
#define TRA_NAL_TYPE_CODED_SLICE_DATA_PARTITION_B    3
#define TRA_NAL_TYPE_CODED_SLICE_DATA_PARTITION_C    4
#define TRA_NAL_TYPE_CODED_SLICE_IDR                 5
#define TRA_NAL_TYPE_SEI                             6
#define TRA_NAL_TYPE_SPS                             7
#define TRA_NAL_TYPE_PPS                             8
#define TRA_NAL_TYPE_ACCESS_UNIT_DELIMITER           9
#define TRA_NAL_TYPE_END_OF_SEQUENCE                10
#define TRA_NAL_TYPE_END_OF_STREAM                  11
#define TRA_NAL_TYPE_FILLER_DATA                    12
#define TRA_NAL_TYPE_SPS_EXTENSION                  13
#define TRA_NAL_TYPE_PREFIX_NAL                     14
#define TRA_NAL_TYPE_SUBSET_SPS                     15
#define TRA_NAL_TYPE_DPS                            16

#define TRA_SLICE_TYPE_P                             0
#define TRA_SLICE_TYPE_B                             1
#define TRA_SLICE_TYPE_I                             2
#define TRA_SLICE_TYPE_SP                            3
#define TRA_SLICE_TYPE_SI                            4
#define TRA_SLICE_TYPE_P_ONLY                        5
#define TRA_SLICE_TYPE_B_ONLY                        6
#define TRA_SLICE_TYPE_I_ONLY                        7
#define TRA_SLICE_TYPE_SP_ONLY                       8
#define TRA_SLICE_TYPE_SI_ONLY                       9

/* ------------------------------------------------------- */

typedef struct tra_avc_reader       tra_avc_reader;
typedef struct tra_nal              tra_nal;
typedef struct tra_sps              tra_sps;
typedef struct tra_pps              tra_pps;
typedef struct tra_vui              tra_vui;
typedef struct tra_slice            tra_slice;
typedef struct tra_avc_parsed_sps   tra_avc_parsed_sps;
typedef struct tra_avc_parsed_pps   tra_avc_parsed_pps;
typedef struct tra_avc_parsed_slice tra_avc_parsed_slice;

/* ------------------------------------------------------- */

struct tra_nal {
  uint8_t forbidden_zero_bit;
  uint8_t nal_ref_idc;
  uint8_t nal_unit_type;
};

/* ------------------------------------------------------- */

struct tra_vui {
  uint32_t aspect_ratio_info_present_flag;
  uint32_t aspect_ratio_idc;
  uint32_t sar_width;
  uint32_t sar_height;
  uint32_t overscan_info_present_flag;
  uint32_t overscan_appropriate_flag;
  uint32_t video_signal_type_present_flag;
  uint32_t video_format;
  uint32_t video_full_range_flag;
  uint32_t colour_description_present_flag;
  uint32_t colour_primaries;
  uint32_t transfer_characteristics;
  uint32_t matrix_coefficients;
  uint32_t chroma_loc_info_present_flag;
  uint32_t chroma_sample_loc_type_top_field;
  uint32_t chroma_sample_loc_type_bottom_field;
  uint32_t timing_info_present_flag;
  uint32_t num_units_in_tick;
  uint32_t time_scale;
  uint32_t fixed_frame_rate_flag;
  uint32_t nal_hrd_parameters_present_flag;
  uint32_t vcl_hrd_parameters_present_flag;
  uint32_t low_delay_hrd_flag;
  uint32_t pic_struct_present_flag;
  uint32_t bitstream_restriction_flag;
  uint32_t motion_vectors_over_pic_boundaries_flag;
  uint32_t max_bytes_per_pic_denom;
  uint32_t max_bits_per_mb_denom;
  uint32_t log2_max_mv_length_horizontal;
  uint32_t log2_max_mv_length_vertical;
  uint32_t num_reorder_frames;
  uint32_t max_dec_frame_buffering;
};

/* ------------------------------------------------------- */

struct tra_sps {
  uint8_t profile_idc;
  uint8_t constraint_set0_flag;
  uint8_t constraint_set1_flag;
  uint8_t constraint_set2_flag;
  uint8_t constraint_set3_flag;
  uint8_t constraint_set4_flag;
  uint8_t constraint_set5_flag;
  uint8_t reserved_zero_2bits;
  uint8_t level_idc;
  uint32_t seq_parameter_set_id;                                /* Is set to UINT32_MAX when the `tra_avc_reader` is created. */
  uint32_t chroma_format_idc;
  uint8_t separate_colour_plane_flag;
  uint32_t log2_max_frame_num_minus4;
  uint32_t pic_order_cnt_type;
  uint32_t log2_max_pic_order_cnt_lsb_minus4;
  uint32_t max_num_ref_frames;
  uint8_t gaps_in_frame_num_value_allowed_flag;
  uint32_t pic_width_in_mbs_minus1;
  uint32_t pic_height_in_map_units_minus1;
  uint8_t frame_mbs_only_flag;
  uint8_t mb_adaptive_frame_field_flag;
  uint8_t direct_8x8_inference_flag;
  uint8_t frame_cropping_flag;
  uint32_t frame_crop_left_offset;
  uint32_t frame_crop_right_offset;
  uint32_t frame_crop_top_offset;
  uint32_t frame_crop_bottom_offset;
  uint8_t vui_parameters_present_flag;
  tra_vui vui;
};

/* ------------------------------------------------------- */

struct tra_pps {
  uint32_t pic_parameter_set_id;                                /* Is set to `UINT32_MAX` in `tra_avc_reader_create()` to indicate an invalid/unused PPS */
  uint32_t seq_parameter_set_id;                                /* Is set to `UINT32_MAX` in `tra_avc_reader_create()` to indicate an invalid/unused SPS */
  uint32_t entropy_coding_mode_flag;
  uint32_t bottom_field_pic_order_in_frame_present_flag;        /* Renamed from `pic_order_preset_flag` */
  uint32_t num_slice_groups_minus1;
  uint32_t num_ref_idx_l0_default_active_minus1;
  uint32_t num_ref_idx_l1_default_active_minus1;
  uint32_t weighted_pred_flag;
  uint32_t weighted_bipred_idc;
  int32_t pic_init_qp_minus26;
  int32_t pic_init_qs_minus26;
  int32_t chroma_qp_index_offset;
  uint32_t deblocking_filter_control_present_flag;
  uint32_t constrained_intra_pred_flag;
  uint32_t redundant_pic_cnt_present_flag;
};

/* ------------------------------------------------------- */

struct tra_slice {
  uint32_t first_mb_in_slice;
  uint32_t slice_type;
  uint32_t pic_parameter_set_id;
  uint32_t colour_plane_id;
  uint32_t frame_num;
  uint8_t field_pic_flag;
  uint8_t bottom_field_flag;
  uint32_t idr_pic_id;
  uint32_t pic_order_cnt_lsb;
  uint32_t redundant_pic_cnt;
  uint8_t num_ref_idx_active_override_flag;
  uint32_t num_ref_idx_l0_active_minus1;
  uint32_t num_ref_idx_l1_active_minus1;
  uint8_t ref_pic_list_modification_flag_l0;                    /* ref_pic_list_modification() */
  uint8_t ref_pic_list_modification_flag_l1;                    /* ref_pic_list_modification() */
  uint8_t no_output_of_prior_pics_flag;                         /* dec_ref_pic_marking() */
  uint8_t long_term_reference_flag;                             /* dec_ref_pic_marking() */
  uint8_t adaptive_ref_pic_marking_mode_flag;                   /* dec_ref_pic_marking() */
  uint32_t cabac_init_idc;
  int32_t slice_qp_delta;
  int32_t slice_qs_delta;
  uint32_t disable_deblocking_filter_idc;
  int32_t slice_alpha_c0_offset_div2;
  int32_t slice_beta_offset_div2;
};

/* ------------------------------------------------------- */

struct tra_avc_parsed_sps {
  tra_nal nal;
  tra_sps* sps;
};

struct tra_avc_parsed_pps {
  tra_nal nal;
  tra_pps* pps;
};

struct tra_avc_parsed_slice {
  tra_nal nal;
  tra_slice slice;
  uint8_t data_bit_offset;                                      /* The number of read bits from the nal unit byte (with nal_ref_idc, nal_unit_type) up to the current position; i.e. up to where the slice data starts. This value is used by the VAAPI decoder. */
};

/* ------------------------------------------------------- */

int tra_avc_reader_create(tra_avc_reader** ctx);
int tra_avc_reader_destroy(tra_avc_reader* ctx);
int tra_avc_parse(tra_avc_reader* ctx, uint8_t* data, uint32_t nbytes);
int tra_avc_parse_nal(tra_avc_reader* ctx, uint8_t* data, uint32_t nbytes, tra_nal* nal);
int tra_avc_parse_sps(tra_avc_reader* ctx, uint8_t* data, uint32_t nbytes, tra_avc_parsed_sps* result);                /* [`result.sps` is OWNED BY READER]. `nal` is parsed too. We set the `result.sps` to the SPS that is owned by the `tra_avc_reader`. The SPS instances are kept internally as they are used when parsing slices. */
int tra_avc_parse_pps(tra_avc_reader* ctx, uint8_t* data, uint32_t nbytes, tra_avc_parsed_pps* result);                /* [`result.pps` is OWNED BY READER]. `nal` is parsed too. We set the `result.pps` to the PPS that is owned by the `tra_avc_reader`.  The PPS instances are kept internally as they are used when parsing slices. */
int tra_avc_parse_slice(tra_avc_reader* ctx, uint8_t* data, uint32_t nbytes, tra_avc_parsed_slice* result);            /* You pass a pointer to a `tra_nal` and `tra_slice` instance that are contained by the `result`. We parse both the nal and slice. */ 

int tra_nal_find(uint8_t* data, uint32_t nbytes, uint8_t** nalStart, uint32_t* nalSize);                               /* This function assumes that `data` starts with the annex-b header. This function will search for the next annex-b header and then sets `nalStart` to the first byte of the nal and `nalSize` to the number of bytes in the nal; excluding the annex-b bytes.*/
int tra_nal_find_type(uint8_t* data, uint32_t nbytes, uint8_t type, uint8_t** nalStart, uint32_t* nalSize);            /* Find the given nal type in the given data. */
int tra_nal_find_sps(uint8_t* data, uint32_t nbytes, uint8_t** nalStart, uint32_t* nalSize);                           /* IMPORTANT: When found, `nalStart` points to the nal header, e.g. 0x67. */
int tra_nal_find_pps(uint8_t* data, uint32_t nbytes, uint8_t** nalStart, uint32_t* nalSize);                           /* IMPORTANT: When found, `nalStart` points to the nal header, e.g. 0x68. */
int tra_nal_find_slice(uint8_t* data, uint32_t nbytes, uint8_t** nalStart, uint32_t* nalSize);                         /* IMPORTANT: When found, `nalStart` points to the nal header, e.g. 0x25. */

int tra_nal_print(tra_nal* nal);
int tra_sps_print(tra_sps* sps);
int tra_pps_print(tra_pps* pps);
int tra_slice_print(tra_slice* slice);

/* ------------------------------------------------------- */

#endif
