/* ------------------------------------------------------- */

#include <stdlib.h>

#include <tra/golomb.h>
#include <tra/avc.h>
#include <tra/log.h>

/* ------------------------------------------------------- */

#define TRA_MAX_SPS 32
#define TRA_MAX_PPS 256

/* ------------------------------------------------------- */

static const char* naltype_to_string(uint8_t type); 
  
/* ------------------------------------------------------- */

struct tra_avc_reader {
  tra_golomb_reader bs;
  tra_sps sps_list[TRA_MAX_SPS]; /* Indexed by the SPS ID. */
  tra_pps pps_list[TRA_MAX_PPS]; /* Indexed by the PPS ID. */
};

/* ------------------------------------------------------- */

int tra_avc_reader_create(tra_avc_reader** ctx) {

  tra_avc_reader* inst = NULL;
  uint32_t i = 0;
  int r = 0;

  TRAE("@todo I should make all H264 fields which are bit flags a uint8_t.");
  
  if (NULL == ctx) {
    TRAE("Cannot create the `tra_avc_reader as the given destination is NULL.");
    return -1;
  }

  inst = calloc(1, sizeof(tra_avc_reader));
  if (NULL == inst) {
    TRAE("Cannot create the `tra_avc_reader`, failed to allocate. Ouf of memory?");
    return -2;
  }

  /* We initialize all the sps-id and pps-id reference as invalid. */
  for (i = 0; i < TRA_MAX_SPS; ++i) {
    inst->sps_list[i].seq_parameter_set_id = UINT32_MAX;
  }

  for (i = 0; i < TRA_MAX_PPS; ++i) {
    inst->pps_list[i].pic_parameter_set_id = UINT32_MAX;
    inst->pps_list[i].seq_parameter_set_id = UINT32_MAX;
  }

  *ctx = inst;

 error:

  if (r < 0) {
    
    if (NULL != inst) {
      tra_avc_reader_destroy(inst);
      inst = NULL;
    }

    if (NULL != ctx) {
      *ctx = NULL;
    }
  }
    
  return r;
}

/* ------------------------------------------------------- */

int tra_avc_reader_destroy(tra_avc_reader* ctx) {

  int r = 0;
  int result = 0;
  
  if (NULL == ctx) {
    TRAE("Cannot destroy the given `tra_avc_reader` as the given pointer is NULL.");
    return -1;
  }

  r = tra_golomb_reader_shutdown(&ctx->bs);
  if (r < 0) {
    TRAE("Failed to cleanly shutdown the golomb reader.");
    result -= 2;
  }

  free(ctx);
  ctx = NULL;

  return result;
}

/* ------------------------------------------------------- */

int tra_avc_parse(tra_avc_reader* ctx, uint8_t* data, uint32_t nbytes) {

  tra_nal nal = { 0 };
  int r = 0;
  
  if (NULL == ctx) {
    TRAE("Cannot parse the AVC data as the given `tra_avc_reader` is NULL.");
    return -1;
  }

  if (NULL == data) {
    TRAE("Cannot parse the AVC data as the given data is NULL.");
    return -2;
  }

  if (0 == nbytes) {
    TRAE("Cannot parse the AVC data as the given `nbytes` is 0.");
    return -3;
  }

  tra_nal_print(&nal);

  return 0;
}

/* ------------------------------------------------------- */

int tra_avc_parse_nal(tra_avc_reader* ctx, uint8_t* data, uint32_t nbytes, tra_nal* nal) {

  int r = 0;
  
  if (NULL == ctx) {
    TRAE("Cannot parse a nal as the given `tra_avc_reader*` is NULL.");
    return -1;
  }

  if (NULL == nal) {
    TRAE("Cannot parse a nal as the given `tra_nal*` is NULL.");
    return -2;
  }

  r = tra_golomb_reader_init(&ctx->bs, data, nbytes);
  if (r < 0) {
    TRAE("Cannot parse a nal, failed to initialize the bitstream reader.");
    return -3;
  }

  nal->forbidden_zero_bit = tra_golomb_read_bit(&ctx->bs);
  nal->nal_ref_idc = tra_golomb_read_bits(&ctx->bs, 2);
  nal->nal_unit_type = tra_golomb_read_bits(&ctx->bs, 5);

  tra_nal_print(nal);

  return r;
}

/* ------------------------------------------------------- */

int tra_avc_parse_sps(
  tra_avc_reader* ctx,
  uint8_t* data,
  uint32_t nbytes,
  tra_avc_parsed_sps* result
)
{
  uint32_t sps_id = 0;
  tra_sps* sps = NULL;
  int r = 0;

  if (NULL == ctx) {
    TRAE("Cannot parse SPS as the given `tra_avc_reader*` is NULL.");
    return -1;
  }

  if (NULL == data) {
    TRAE("Cannot parse the SPS as the given `data` is NULL.");
    return -2;
  }

  if (0 == nbytes) {
    TRAE("Cannot parse the SPS as the given `nbytes` is 0.");
    return -3;
  }

  if (NULL == result) {
    TRAE("Cannot parse the SPS as the given `tra_avc_parsed_sps` is NULL.");
    return -4;
  }

  r = tra_avc_parse_nal(ctx, data, nbytes, &result->nal);
  if (r < 0) {
    TRAE("Failed to parse the nal.");
    return -5;
  }

  /* Make sure to unset the result SPS */
  result->sps = NULL;

  r = tra_golomb_reader_init(&ctx->bs, data + 1, nbytes - 1);
  if (r < 0) {
    TRAE("Cannot parse the SPS, failed to initialize the bitstream reader.");
    return -3;
  }

  sps_id = tra_golomb_peek_ue(&ctx->bs, 3);
  if (sps_id >= TRA_MAX_SPS) {
    TRAE("Cannot parse the SPS as the ID is out of bounds (%u).", sps_id);
    return -4;
  }

  /* Get the destination SPS. */
  sps = ctx->sps_list + sps_id;

  /* Read the SPS */
  sps->profile_idc = tra_golomb_read_u8(&ctx->bs);
  sps->constraint_set0_flag = tra_golomb_read_bit(&ctx->bs);
  sps->constraint_set1_flag = tra_golomb_read_bit(&ctx->bs);
  sps->constraint_set2_flag = tra_golomb_read_bit(&ctx->bs);
  sps->constraint_set3_flag = tra_golomb_read_bit(&ctx->bs);
  sps->constraint_set4_flag = tra_golomb_read_bit(&ctx->bs);
  sps->constraint_set5_flag = tra_golomb_read_bit(&ctx->bs);
  sps->reserved_zero_2bits = tra_golomb_read_bits(&ctx->bs, 2);
  sps->level_idc = tra_golomb_read_u8(&ctx->bs);
  sps->seq_parameter_set_id = tra_golomb_read_ue(&ctx->bs);

  /* chroma_format_idc: defaults to 1 */
  sps->chroma_format_idc = 1;

  /* chroma_format_idc: read from RBSP */
  switch (sps->profile_idc) {
    case 100:
    case 110:
    case 122:
    case 244:
    case 44:
    case 83:
    case 86:
    case 118:
    case 128:
    case 138:
    case 139:
    case 134:
    case 135: {
      /* @todo we're skipping some fields here */
      TRAE("@todo we need to parse color information.");
      r = -3;
      goto error;
    };
  }

  sps->log2_max_frame_num_minus4 = tra_golomb_read_ue(&ctx->bs);
  sps->pic_order_cnt_type = tra_golomb_read_ue(&ctx->bs);

  if (0 == sps->pic_order_cnt_type) {
    sps->log2_max_pic_order_cnt_lsb_minus4 = tra_golomb_read_ue(&ctx->bs);
  }
  else {
    TRAE("@todo we need to parse `pic_order_cnt_type` which is not 0 but `%u`.", sps->pic_order_cnt_type);
    r = -4;
    goto error;
  }

  sps->max_num_ref_frames = tra_golomb_read_ue(&ctx->bs);
  sps->gaps_in_frame_num_value_allowed_flag = tra_golomb_read_bit(&ctx->bs);
  sps->pic_width_in_mbs_minus1 = tra_golomb_read_ue(&ctx->bs);
  sps->pic_height_in_map_units_minus1 = tra_golomb_read_ue(&ctx->bs);
  sps->frame_mbs_only_flag = tra_golomb_read_bit(&ctx->bs);

  if (0 == sps->frame_mbs_only_flag) {
    sps->mb_adaptive_frame_field_flag = tra_golomb_read_bit(&ctx->bs);
  }

  sps->direct_8x8_inference_flag = tra_golomb_read_bit(&ctx->bs);
  sps->frame_cropping_flag = tra_golomb_read_bit(&ctx->bs);

  if (1 == sps->frame_cropping_flag) {
    sps->frame_crop_left_offset = tra_golomb_read_ue(&ctx->bs);
    sps->frame_crop_right_offset = tra_golomb_read_ue(&ctx->bs);
    sps->frame_crop_top_offset = tra_golomb_read_ue(&ctx->bs);
    sps->frame_crop_bottom_offset = tra_golomb_read_ue(&ctx->bs);
  }

  result->sps = sps;
    
 error:

  if (r < 0) {
    if (NULL != result) {
      result->sps = NULL;
    }
  }
  
  return r;
}

/* ------------------------------------------------------- */

int tra_avc_parse_pps(
  tra_avc_reader* ctx,
  uint8_t* data,
  uint32_t nbytes,
  tra_avc_parsed_pps* result
)
{

  uint32_t pps_id = 0;
  tra_pps* pps = NULL;
  int r = 0;
  
  if (NULL == ctx) {
    TRAE("Cannot parse the PPS as the given `tra_avc_reader` is NULL.");
    return -1;
  }

  if (NULL == data) {
    TRAE("Cannot parse the PPS as the given `data` is NULL.");
    return -2;
  }

  if (0 == nbytes) {
    TRAE("Cannot parse the PPS as the given `nbytes` is 0.");
    return -3;
  }

  if (NULL == result) {
    TRAE("Cannot parse the PPS as the given `tra_avc_parsed_pps*` is NULL.");
    return -4;
  }

  r = tra_avc_parse_nal(ctx, data, nbytes, &result->nal);
  if (r < 0) {
    TRAE("Cannot parse the PPS, failed to parse the nal header.");
    return -5;
  }

  /* Make sure to unset the PPS */
  result->pps = NULL;

  /* Make sure the bit streams uses the correct data. */
  r = tra_golomb_reader_init(&ctx->bs, data + 1, nbytes - 1);
  if (r < 0) {
    TRAE("Cannot parse the PPS, failed to initialize the bitstream reader.");
    return -7;
  }

  pps_id = tra_golomb_read_ue(&ctx->bs);
  if (pps_id >= TRA_MAX_PPS) {
    TRAE("The read PPS ID (%u) is out of bounds (%u)", pps_id, TRA_MAX_SPS);
    return -8;
  }

  /* Get the PPS */
  pps = ctx->pps_list + pps_id;

  /* Fill the PPS */
  pps->pic_parameter_set_id = pps_id;
  pps->seq_parameter_set_id = tra_golomb_read_ue(&ctx->bs);
  pps->entropy_coding_mode_flag = tra_golomb_read_bit(&ctx->bs);
  pps->bottom_field_pic_order_in_frame_present_flag = tra_golomb_read_bit(&ctx->bs);
  pps->num_slice_groups_minus1 = tra_golomb_read_ue(&ctx->bs);

  if (pps->num_slice_groups_minus1 > 0) {
    TRAE("The PPS specifies slice groups which we don't support yet.");
    return -9;
  }

  pps->num_ref_idx_l0_default_active_minus1 = tra_golomb_read_ue(&ctx->bs);
  pps->num_ref_idx_l1_default_active_minus1 = tra_golomb_read_ue(&ctx->bs);
  pps->weighted_pred_flag = tra_golomb_read_bit(&ctx->bs);
  pps->weighted_bipred_idc = tra_golomb_read_bits(&ctx->bs, 2);
  pps->pic_init_qp_minus26 = tra_golomb_read_se(&ctx->bs);
  pps->pic_init_qs_minus26 = tra_golomb_read_se(&ctx->bs);
  pps->chroma_qp_index_offset = tra_golomb_read_se(&ctx->bs);
  pps->deblocking_filter_control_present_flag = tra_golomb_read_bit(&ctx->bs);
  pps->constrained_intra_pred_flag = tra_golomb_read_bit(&ctx->bs);
  pps->redundant_pic_cnt_present_flag = tra_golomb_read_bit(&ctx->bs);

  /* Finally assign the result. */
  result->pps = pps;

  return 0;
}

/* ------------------------------------------------------- */

/*

  This function will parse the slice header as described in the
  H264 2016 spec, "7.3.3 Slice header syntax". Currently we focus
  on implementing a decoder for the Constrained Baseline profile.

  The `data` should point to the nal-header; e.g. the first byte
  that comes after the annex-b header. We parse the nal and then
  the slice header. We need the nal header to be able to parse
  certain fields of the slice header; e.g. we need to know if the
  current nal has the `IdrPicFlag` set.

  I must store the SPS/PPS in the `tra_avc_reader` because
  otherwise we can't parse the slices. To parse a slice we 
  need to know what flags are enabled. 

  IdrPicFlag: 
  
    When `nal_unit_type` is 5, the flag is set to 1, otherwise
    it's 0. When the value is equal to 5, we have to read the
    `idr_pic_id ue(v)`.

 */
int tra_avc_parse_slice(
  tra_avc_reader* ctx,
  uint8_t* data,
  uint32_t nbytes,
  tra_avc_parsed_slice* result
)
{
  tra_nal* nal = NULL;
  tra_sps* sps = NULL;
  tra_pps* pps = NULL;
  tra_slice* slice = NULL;
  int r = 0;
  uint32_t slice_type_mod5 = 0;

  if (NULL == ctx) {
    TRAE("Cannot parse a slice as the given `tra_avc_reader*` is NULL.");
    return -1;
  }

  if (NULL == data) {
    TRAE("Cannot parse a slice as the given `data` is NULL.");
    return -2;
  }

  if (0 == nbytes) {
    TRAE("Cannot parse a slice as the given `nbytes` is 0.");
    return -3;
  }

  if (NULL == result) {
    TRAE("Cannot parse a slice as the given `tra_avc_parsed_slice*` is NULL.");
    return -4;
  }

  nal = &result->nal;
  slice = &result->slice;
  
  r = tra_avc_parse_nal(ctx, data, nbytes, nal);
  if (r < 0) {
    TRAE("Cannot parse a slice as we failed to pase the nal.");
    return -5;
  }

  /* Make sure the bit streams uses the correct data. */
  r = tra_golomb_reader_init(&ctx->bs, data + 1, nbytes - 1);
  if (r < 0) {
    TRAE("Cannot parse the slice, failed to initialize the bitstream reader.");
    return -6;
  }
  
  /* Make sure that every field is set to it's defaults. */
  memset((char*)slice, 0x00, sizeof(*slice));
  
  slice->first_mb_in_slice = tra_golomb_read_ue(&ctx->bs);
  slice->slice_type = tra_golomb_read_ue(&ctx->bs);
  slice->pic_parameter_set_id = tra_golomb_read_ue(&ctx->bs);

  if (TRA_SLICE_TYPE_B == slice->slice_type) {
    TRAE("@todo we do not support B-slices yet.A");
    return -7;
  }  
      
  /* ---------------------------------------------- */
  /* Check if the PPS is valid and supported        */
  /* ---------------------------------------------- */

  if (slice->pic_parameter_set_id >= TRA_MAX_PPS) {
    TRAE("Cannot parse the slice, the `pic_parameter_set_id` (%u) is out of bounds.", slice->pic_parameter_set_id);
    return -8;
  }

  pps = ctx->pps_list + slice->pic_parameter_set_id;
  if (UINT32_MAX == pps->pic_parameter_set_id) {
    TRAE("Cannot parse the slice, the PPS it refers to is invalid (%u).", slice->pic_parameter_set_id);
    return -9;
  }

  if (UINT32_MAX == pps->seq_parameter_set_id) {
    TRAE("Cannot parse the slice, the PPS refers to an invalid SPS.");
    return -10;
  }

  if (1 == pps->redundant_pic_cnt_present_flag) {
    TRAE("@todo we do not support `pps.redundant_pic_cnt_present_flag` other than 0.");
    return -11;
  }

  if (1 == pps->weighted_pred_flag) {
    TRAE("@todo we do not support `pps.weighted_pred_flag` other than 0.");
    return -12;
  }

  if (0 != pps->entropy_coding_mode_flag) {
    TRAE("@todo we do not support `pps.entropy_coding_mode_flag` other than 0.");
    return -13;
  }

  if (0 != pps->num_slice_groups_minus1) {
    TRAE("@todo we do not support `pps.num_slice_groups_minus1` other than 0.");
    return -14;
  }

  /* ---------------------------------------------- */
  /* Check the PPS is valid and supported           */
  /* ---------------------------------------------- */
  
  sps = ctx->sps_list + pps->seq_parameter_set_id;
  
  if (1 == sps->separate_colour_plane_flag) {
    TRAE("@todo we do not support separate colour planes yet.");
    return -15;
  }
  
  if (0 == sps->frame_mbs_only_flag) {
    TRAE("@todo we do not support slice based decoding yet.");
    return -16;
  }

  if (0 != sps->pic_order_cnt_type) {
    TRAE("@todo we do not support other pic_order_cnt_type than 0. The stream uses %u", sps->pic_order_cnt_type);
    return -17;
  }

  /* ---------------------------------------------- */
  /* Parse the SLICE header                         */
  /* ---------------------------------------------- */
  
  slice->frame_num = tra_golomb_read_bits(&ctx->bs, sps->log2_max_frame_num_minus4 + 4);

  if (5 == nal->nal_unit_type) {
    slice->idr_pic_id = tra_golomb_read_ue(&ctx->bs);
  }

  if (0 == sps->pic_order_cnt_type) {
    slice->pic_order_cnt_lsb = tra_golomb_read_bits(&ctx->bs, sps->log2_max_pic_order_cnt_lsb_minus4 + 4);
  }

  /* Skipping some fields here; delta_pic_* fields */
  if (TRA_SLICE_TYPE_P == slice->slice_type) {

    slice->num_ref_idx_active_override_flag = tra_golomb_read_bit(&ctx->bs);

    if (1 == slice->num_ref_idx_active_override_flag) {
      slice->num_ref_idx_l0_active_minus1 = tra_golomb_read_ue(&ctx->bs);
    }
  }

  /* ref_pic_list_modification() */
  slice_type_mod5 = slice->slice_type % 5;
  
  if (2 != slice_type_mod5 && 5 != slice_type_mod5) {

    slice->ref_pic_list_modification_flag_l0 = tra_golomb_read_bit(&ctx->bs);

    if (1 == slice->ref_pic_list_modification_flag_l0) {
      TRAE("@todo we do not yet support a `ref_pic_list_modification_flag_l0` other than 0 yet.");
      return -18;
    }
  }

  if (1 == slice_type_mod5) {
    slice->ref_pic_list_modification_flag_l1 = tra_golomb_read_bit(&ctx->bs);
    if (1 == slice->ref_pic_list_modification_flag_l1) {
      TRAE("@#todo we do not yet support a `ref_pic_list_modification_flag_l1` other than 0 yet.");
      return -19;
    }
  }

  /* Skipping weighted_pred_flag */

  /* Reference picture */
  if (0 != nal->nal_ref_idc) {
    if (5 == nal->nal_unit_type) {
      slice->no_output_of_prior_pics_flag = tra_golomb_read_bit(&ctx->bs);
      slice->long_term_reference_flag = tra_golomb_read_bit(&ctx->bs);
    }
    else {
      slice->adaptive_ref_pic_marking_mode_flag = tra_golomb_read_bit(&ctx->bs);
      if (0 != slice->adaptive_ref_pic_marking_mode_flag) {
        TRAE("@todo we do not yet support a `adaptive_ref_pic_marking_mode_flag` other than 0.");
        return -20;
      }
    }
  }

  /* Skipping entropy mode other than 0 */

  slice->slice_qp_delta = tra_golomb_read_se(&ctx->bs);

  /* Skipping qs delta for SP, SI and SP slices */

  if (1 == pps->deblocking_filter_control_present_flag) {
    slice->disable_deblocking_filter_idc = tra_golomb_read_ue(&ctx->bs);
    if (1 != slice->disable_deblocking_filter_idc) {
      slice->slice_alpha_c0_offset_div2 = tra_golomb_read_se(&ctx->bs);
      slice->slice_beta_offset_div2 = tra_golomb_read_se(&ctx->bs);
    }
  }
    
  /* Skipping slice groups */

  /* 

     Calculate the bit offset to the slice data. The
     `data_bit_offset` is the offset measured from and including
     the nal unit byte (i.e. the byte with the
     forbidding_zero_bit, nal_ref_idc and nal_unit_type.). The
     offset is measured in bits. 

     Below you see `1 + ctx->bs.byte_offset` and the `1` is the
     nal unit byte, `ctx->bs.byte_offset` is the byte offset up
     to the point where we parsed all the slice attributes. Then
     we have to count the number of bits that we've parsed into
     the slice header. As we start counting at 7 with the
     `bit_offset` (as it's used to read the MSB first using `1 <<
     bit_offset), we have to do `7 - bit_offset`. A
     `bs.bit_offset` value that is equal to 7, means that we
     didn't read any bits yet.

     Note: I assume that the nal unit header is always 1 byte
     long.

   */
  result->data_bit_offset = ((1 + ctx->bs.byte_offset) * 8) + 7 - ctx->bs.bit_offset;

  return 0;
}

/* ------------------------------------------------------- */

/*

  GENERAL INFO:

   This function will try to find the nal which is stored in
   `data` that holds `nbytes` number of bytes. When we find an
   annex-b header we set `nalStart` to the first byte in the nal
   and `nalSize` will be set to the number of bytes that make up
   the nal.

   This implementation is based on the h264 parse function form
   [GStreamer][0] which takes a nice approach. It fills a
   uint32_t with new bytes from the bitstream (data). Then it
   checks of the variable, flags, contains either a 3-byte or
   4-byte header. This function assumes that the given data
   starts with an annex-b header.

   REFERENCES:

     [0]: https://github.com/GStreamer/gstreamer-vaapi/blob/master/gst/vaapi/gstvaapiencode_h264.c#L435
     [1]: https://www.geeksforgeeks.org/boyer-moore-algorithm-for-pattern-searching/
   
 */
int tra_nal_find(uint8_t* data, uint32_t nbytes, uint8_t** nalStart, uint32_t* nalSize) {

  uint8_t* nal_start_ptr = NULL;
  uint32_t nal_start_offset = 0;
  uint8_t* buf_curr = NULL;
  uint8_t* buf_end = NULL;
  uint32_t flag = 0xFFFFFFFF;
  uint32_t nal_nbytes = 0;

  if (NULL == data) {
    TRAE("Cannot find the nal as the given `data*` is NULL.");
    return -1;
  }

  if (0 == nbytes) {
    TRAE("Cannot find the nal as the given `nbytes` is 0.");
    return -2;
  }

  if (NULL == nalStart) {
    TRAE("Cannot find the nal as the givern `nalStart*` is NULL.");
    return -3;
  }

  if (NULL == nalSize) {
    TRAE("Cannot find the nal as the given `nalSize*` is NULL.");
    return -4;
  }

  /* Start position */
  if (0x00 == data[0] && 0x00 == data[1]) {
    if (0x01 == data[2]) {
      /* 0x00 0x00 0x01 */
      nal_start_offset = 3;
    }
    else if (0x00 == data[2] 
             && nbytes >= 4
             && data[3] == 0x01)
      {
        /* 0x00 0x00 0x00 0x01 */
        nal_start_offset = 4;
      }
  }

  if (0 == nal_start_offset) {
    TRAE("Cannot find the nal, the given `data` doesn't start at an annex-b header.");
    return -5;
  }

  /* Find the size */
  nal_start_ptr = data + nal_start_offset;
  buf_curr = data + nal_start_offset;
  buf_end = data + nbytes;

  while (buf_curr < buf_end) {

    flag = (flag << 8) | (*buf_curr++); 
    if ((flag & 0x00FFFFFF) == 0x00000001) {
      if (flag == 0x00000001) {
        nal_nbytes = buf_curr - 4 - nal_start_ptr;
      }
      else {
        nal_nbytes = buf_curr - 3 - nal_start_ptr;
      }
      break;
    }
  }

  *nalSize = nal_nbytes;
  *nalStart = nal_start_ptr;
  
  if (buf_curr >= buf_end) {
    *nalSize = (buf_end - nal_start_ptr);
    if (nal_start_ptr >= buf_end) {
      TRAE("@todo does this ever happen?");
    }
  }

  return 0;
}

/* ------------------------------------------------------- */

/* Find the given nal type in the given data. */
int tra_nal_find_type(uint8_t* data, uint32_t nbytes, uint8_t type, uint8_t** nalStart, uint32_t* nalSize) { 

  uint32_t nbytes_parsed = 0;
  uint8_t* nal_start = NULL;
  uint32_t nal_size = 0;
  uint8_t* buf = data;
  int r = 0;
  
  if (NULL == data) {
    TRAE("Cannot find the nal by type as the given `data` is NULL.");
    return -1;
  }

  if (0 == nbytes) {
    TRAE("Cannot find the nal by type as the given `nbytes` is 0.");
    return -2;
  }

  if (NULL == nalStart) {
    TRAE("Cannot find the nal by type as the given `nalStart` is NULL.");
    return -3;
  }

  if (NULL == nalSize) {
    TRAE("Cannot find the nal by type as the given `nalSize` is NULL.");
    return -4;
  }

  while(nbytes_parsed < nbytes) {

    nal_start = NULL;
    nal_size = 0;
    
    r = tra_nal_find(buf, nbytes - nbytes_parsed, &nal_start, &nal_size);
    if (r < 0) {
      TRAE("Failed to find a the nal.");
      return -5;
    }

    if (NULL == nal_start) {
      TRAE("`nal_start` is NULL; should hold a value! This is a bug. (exiting).");
      exit(EXIT_FAILURE);
    }

    if (0 == nal_size) {
      TRAE("`nal_size` is 0; should hold a value! This is a bug. (exiting).");
      exit(EXIT_FAILURE);
    }

    if ((nal_start[0] & 0x1F) == type) {
      *nalStart = nal_start;
      *nalSize = nal_size;
      return 0;
    }

    /* `nal_start - buf` is the number of prefix bytes, 3 or 4. */
    nbytes_parsed += (nal_start - buf) + nal_size;
    buf = nal_start + nal_size;
  }

  TRAE("Failed to find the nal type `%s`.", naltype_to_string(type));
  
  return -6;
}

/* ------------------------------------------------------- */

/* 
   This function tries to find the first SPS in the given
   `data`. When found it will set `nalStart` to the nal header
   (e.g. the byte which starts with 0x67). This is important
   because the `tra_avc_parse_sps()` wants to receive a pointer to
   the first byte of the actual SPS and not the nal header.
 */
int tra_nal_find_sps(uint8_t* data, uint32_t nbytes, uint8_t** nalStart, uint32_t* nalSize) {
  return tra_nal_find_type(data, nbytes, TRA_NAL_TYPE_SPS, nalStart, nalSize);
}

/* ------------------------------------------------------- */

/* 
   This function tries to find the first PPS in the given `data`. When
   found it will set `nalStart` to the nal header (e.g. the bytte which 
   start with 0x68). This is important because the `tra_avc_parse_pps()`
   wants to receive a pointer to the first byte of the PPS and not the
   nal header. 
 */
int tra_nal_find_pps(uint8_t* data, uint32_t nbytes, uint8_t** nalStart, uint32_t* nalSize) {
  return tra_nal_find_type(data, nbytes, TRA_NAL_TYPE_PPS, nalStart, nalSize);
}


/* ------------------------------------------------------- */

/* This function tries to find a slice nal unit. */
int tra_nal_find_slice(uint8_t* data, uint32_t nbytes, uint8_t** nalStart, uint32_t* nalSize) {

  uint32_t nbytes_parsed = 0;
  uint8_t nal_unit_type = 0;
  uint8_t* nal_start = NULL;
  uint32_t nal_size = 0;
  uint8_t* buf = data;
  int r = 0;
  
  if (NULL == data) {
    TRAE("Cannot find the slice as the given `data` is NULL.");
    return -1;
  }

  if (0 == nbytes) {
    TRAE("Cannot find the slice as the given `nbytes` is 0.");
    return -2;
  }

  if (NULL == nalStart) {
    TRAE("Cannot find the slice as the given `nalStart` is NULL.");
    return -3;
  }

  if (NULL == nalSize) {
    TRAE("Cannot find the slice as the given `nalSize` is NULL.");
    return -4;
  }

  while(nbytes_parsed < nbytes) {

    nal_start = NULL;
    nal_size = 0;
    
    r = tra_nal_find(buf, nbytes - nbytes_parsed, &nal_start, &nal_size);
    if (r < 0) {
      TRAE("Failed to find a the nal.");
      return -5;
    }

    if (NULL == nal_start) {
      TRAE("`nal_start` is NULL; should hold a value! This is a bug. (exiting).");
      exit(EXIT_FAILURE);
    }

    if (0 == nal_size) {
      TRAE("`nal_size` is 0; should hold a value! This is a bug. (exiting).");
      exit(EXIT_FAILURE);
    }

    /* Did we find a slice? */
    nal_unit_type = nal_start[0] & 0x1F;
    
    if (nal_unit_type >= 1 && nal_unit_type <= 5) {
      *nalStart = nal_start;
      *nalSize = nal_size;
      return 0;
    }

    /* `nal_start - buf` is the number of prefix bytes, 3 or 4. */
    nbytes_parsed += (nal_start - buf) + nal_size;
    buf = nal_start + nal_size;
  }

  TRAE("Failed to find the slice.");
  return -6;
}

/* ------------------------------------------------------- */

int tra_nal_print(tra_nal* nal) {

  if (NULL == nal) {
    TRAE("Cannot print the `tra_nal*` as it's NULL.");
    return -1;
  }

  TRAD("tra_nal");
  TRAD("  forbidden_zero_bit: %u", nal->forbidden_zero_bit);
  TRAD("  nal_ref_idc: %u", nal->nal_ref_idc);
  TRAD("  nal_unit_type: %s (%u)", naltype_to_string(nal->nal_unit_type), nal->nal_unit_type);
  TRAD("");
  
  return 0;
}

/* ------------------------------------------------------- */

int tra_sps_print(tra_sps* sps) {

  if (NULL == sps) {
    TRAE("Cannot print the `tra_nal*` as it's NULL.");
    return -1;
  }

  TRAD("tra_sps");
  TRAD("  profile_idc: %u", sps->profile_idc);
  TRAD("  constraint_set0_flag: %u", sps->constraint_set0_flag);
  TRAD("  constraint_set1_flag: %u", sps->constraint_set1_flag);
  TRAD("  constraint_set2_flag: %u", sps->constraint_set2_flag);
  TRAD("  constraint_set3_flag: %u", sps->constraint_set3_flag);
  TRAD("  constraint_set4_flag: %u", sps->constraint_set4_flag);
  TRAD("  constraint_set5_flag: %u", sps->constraint_set5_flag);
  TRAD("  reserved_zero_2bits: %u", sps->reserved_zero_2bits);
  TRAD("  level_idc: %u", sps->level_idc);
  TRAD("  chroma_format_idc: %u", sps->chroma_format_idc);
  TRAD("  separate_colour_plane_flag: %u", sps->separate_colour_plane_flag);
  TRAD("  seq_parameter_set_id: %u", sps->seq_parameter_set_id);
  TRAD("  log2_max_frame_num_minus4: %u", sps->log2_max_frame_num_minus4);
  TRAD("  pic_order_cnt_type: %u", sps->pic_order_cnt_type);
  TRAD("  log2_max_pic_order_cnt_lsb_minus4: %u", sps->log2_max_pic_order_cnt_lsb_minus4);
  TRAD("  max_num_ref_frames: %u", sps->max_num_ref_frames);
  TRAD("  gaps_in_frame_num_value_allowed_flag: %u", sps->gaps_in_frame_num_value_allowed_flag);
  TRAD("  pic_width_in_mbs_minus1: %u", sps->pic_width_in_mbs_minus1);
  TRAD("  pic_height_in_map_units_minus1: %u", sps->pic_height_in_map_units_minus1);
  TRAD("  frame_mbs_only_flag: %u", sps->frame_mbs_only_flag);

  if (0 == sps->frame_mbs_only_flag) { 
    TRAD("  mb_adaptive_frame_field_flag: %u", sps->mb_adaptive_frame_field_flag);
  }
  
  TRAD("  direct_8x8_inference_flag: %u", sps->direct_8x8_inference_flag);
  TRAD("  frame_cropping_flag: %u", sps->frame_cropping_flag);

  if (1 == sps->frame_cropping_flag) {
    TRAD("  frame_crop_left_offset: %u", sps->frame_crop_left_offset);
    TRAD("  frame_crop_right_offset: %u", sps->frame_crop_right_offset);
    TRAD("  frame_crop_top_offset: %u", sps->frame_crop_top_offset);
    TRAD("  frame_crop_bottom_offset: %u", sps->frame_crop_bottom_offset);
  }

  TRAD("  vui_parameters_present_flag: %u", sps->vui_parameters_present_flag);
  TRAD("");

  return 0;
}

/* ------------------------------------------------------- */

int tra_pps_print(tra_pps* pps) {

  if (NULL == pps) {
    TRAE("Cannot print the `tra_pps` as it's NULL.");
    return -1;
  }

  TRAD("tra_pps");
  TRAD("  pic_parameter_set_id: %u", pps->pic_parameter_set_id);
  TRAD("  seq_parameter_set_id: %u", pps->seq_parameter_set_id);
  TRAD("  entropy_coding_mode_flag: %u", pps->entropy_coding_mode_flag);
  TRAD("  bottom_field_pic_order_in_frame_present_flag: %u", pps->bottom_field_pic_order_in_frame_present_flag);
  TRAD("  num_slice_groups_minus1: %u", pps->num_slice_groups_minus1);
  TRAD("  num_ref_idx_l0_default_active_minus1: %u", pps->num_ref_idx_l0_default_active_minus1);
  TRAD("  num_ref_idx_l1_default_active_minus1: %u", pps->num_ref_idx_l1_default_active_minus1);
  TRAD("  weighted_pred_flag: %u", pps->weighted_pred_flag);
  TRAD("  weighted_bipred_idc: %u", pps->weighted_bipred_idc);
  TRAD("  pic_init_qp_minus26: %u", pps->pic_init_qp_minus26);
  TRAD("  pic_init_qs_minus26: %u", pps->pic_init_qs_minus26);
  TRAD("  chroma_qp_index_offset: %u", pps->chroma_qp_index_offset);
  TRAD("  deblocking_filter_control_present_flag: %u", pps->deblocking_filter_control_present_flag);
  TRAD("  constrained_intra_pred_flag: %u", pps->constrained_intra_pred_flag);
  TRAD("  redundant_pic_cnt_present_flag: %u", pps->redundant_pic_cnt_present_flag);
  TRAD("");
                                                                 
  return 0;                                                      
}                                                                
                                                                 
/* ------------------------------------------------------- */

int tra_slice_print(tra_slice* slice) {

  if (NULL == slice) {
    TRAE("Cannot print the `tra_slice` as it's NULL.");
    return -1;
  }

  TRAD("tra_slice");
  TRAD("  first_mb_in_slice: %u", slice->first_mb_in_slice);
  TRAD("  slice_type: %u", slice->slice_type);
  TRAD("  pic_parameter_set_id: %u", slice->pic_parameter_set_id);
  TRAD("  colour_plane_id: %u", slice->colour_plane_id);
  TRAD("  frame_num: %u", slice->frame_num);
  TRAD("  field_pic_flag: %u", slice->field_pic_flag);
  TRAD("  bottom_field_flag: %u", slice->bottom_field_flag);
  TRAD("  idr_pic_id: %u", slice->idr_pic_id);
  TRAD("  pic_order_cnt_lsb: %u", slice->pic_order_cnt_lsb);
  TRAD("  redundant_pic_cnt: %u", slice->redundant_pic_cnt);
  TRAD("  num_ref_idx_active_override_flag: %u", slice->num_ref_idx_active_override_flag);
  TRAD("  num_ref_idx_l0_active_minus1: %u", slice->num_ref_idx_l0_active_minus1);
  TRAD("  num_ref_idx_l1_active_minus1: %u", slice->num_ref_idx_l1_active_minus1);
  TRAD("  rplm.ref_pic_list_modification_flag_l0: %u", slice->ref_pic_list_modification_flag_l0);
  TRAD("  rplm.ref_pic_list_modification_flag_l1: %u", slice->ref_pic_list_modification_flag_l1);
  TRAD("  drpm.no_output_of_prior_pics_flag: %u", slice->no_output_of_prior_pics_flag);
  TRAD("  drpm.long_term_reference_flag: %u", slice->long_term_reference_flag);
  TRAD("  drpm.adaptive_ref_pic_marking_mode_flag: %u", slice->adaptive_ref_pic_marking_mode_flag);
  TRAD("  cabac_init_idc: %u", slice->cabac_init_idc);
  TRAD("  slice_qp_delta: %d", slice->slice_qp_delta);
  TRAD("  slice_qs_delta: %d", slice->slice_qs_delta);
  TRAD("  disable_deblocking_filter_idc: %u", slice->disable_deblocking_filter_idc);
  TRAD("  slice_alpha_c0_offset_div2: %d", slice->slice_alpha_c0_offset_div2);
  TRAD("  slice_beta_offset_div2: %d", slice->slice_beta_offset_div2);
  TRAD("");

  return 0;
}

/* ------------------------------------------------------- */
static const char* naltype_to_string(uint8_t type) {       
                                                           
  switch (type) {
    case TRA_NAL_TYPE_UNSPECIFIED:                  { return "TRA_NAL_TYPE_UNSPECIFIED";                  }
    case TRA_NAL_TYPE_CODED_SLICE_NON_IDR:          { return "TRA_NAL_TYPE_CODED_SLICE_NON_IDR";          }
    case TRA_NAL_TYPE_CODED_SLICE_DATA_PARTITION_A: { return "TRA_NAL_TYPE_CODED_SLICE_DATA_PARTITION_A"; }
    case TRA_NAL_TYPE_CODED_SLICE_DATA_PARTITION_B: { return "TRA_NAL_TYPE_CODED_SLICE_DATA_PARTITION_B"; }
    case TRA_NAL_TYPE_CODED_SLICE_DATA_PARTITION_C: { return "TRA_NAL_TYPE_CODED_SLICE_DATA_PARTITION_C"; }
    case TRA_NAL_TYPE_CODED_SLICE_IDR:              { return "TRA_NAL_TYPE_CODED_SLICE_IDR";              }
    case TRA_NAL_TYPE_SEI:                          { return "TRA_NAL_TYPE_SEI";                          }
    case TRA_NAL_TYPE_SPS:                          { return "TRA_NAL_TYPE_SPS";                          }
    case TRA_NAL_TYPE_PPS:                          { return "TRA_NAL_TYPE_PPS";                          }
    case TRA_NAL_TYPE_ACCESS_UNIT_DELIMITER:        { return "TRA_NAL_TYPE_ACCESS_UNIT_DELIMITER";        }
    case TRA_NAL_TYPE_END_OF_SEQUENCE:              { return "TRA_NAL_TYPE_END_OF_SEQUENCE";              }
    case TRA_NAL_TYPE_END_OF_STREAM:                { return "TRA_NAL_TYPE_END_OF_STREAM";                }
    case TRA_NAL_TYPE_FILLER_DATA:                  { return "TRA_NAL_TYPE_FILLER_DATA";                  }
    case TRA_NAL_TYPE_SPS_EXTENSION:                { return "TRA_NAL_TYPE_SPS_EXTENSION";                }
    case TRA_NAL_TYPE_PREFIX_NAL:                   { return "TRA_NAL_TYPE_PREFIX_NAL";                   }
    case TRA_NAL_TYPE_SUBSET_SPS:                   { return "TRA_NAL_TYPE_SUBSET_SPS";                   }
    case TRA_NAL_TYPE_DPS:                          { return "TRA_NAL_TYPE_DPS";                          }
    default:                                        { return "UNKNOWN";                                  }
  }
}

/* ------------------------------------------------------- */
