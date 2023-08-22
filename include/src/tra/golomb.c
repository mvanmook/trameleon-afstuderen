/* ------------------------------------------------------- */

#include <stdlib.h>
#include <stdio.h>
#include <limits.h>

#include <tra/golomb.h>
#include <tra/log.h>

/* ------------------------------------------------------- */

int tra_golomb_writer_create(tra_golomb_writer** ctx, uint32_t capacity) {

  tra_golomb_writer* inst = NULL;
  int status = 0;
  int r = 0;
  
  if (NULL == ctx) {
    TRAE("Cannot create the `tra_golomb` instance as the given `tra_golomb_writer**` is NULL.");
    r = -10;
    goto error;
  }

  if (NULL != (*ctx)) {
    TRAE("Cannot create the `tra_golomb` instance as the given `tra_golomb_writer**` is not set to NULL. Maybe created already.");
    r = -20;
    goto error;
  }

  if (0 == capacity) {
    TRAE("Cannot create the `tra_golomb` instance as the given capacity is 0.");
    r = -30;
    goto error;
  }

  inst = calloc(1, sizeof(tra_golomb_writer));
  if (NULL == inst) {
    TRAE("Cannot create teh `tra_golomb` instance, we failed to allocate it. Out of memory?");
    r = -40;
    goto error;
  }

  inst->data = calloc(1, capacity);
  if (NULL == inst->data) {
    TRAE("Cannot create the `tra_golomb` instance, we failed to allocate the buffer for `tra_golomb`. Out of memory?");
    r = -50;
    goto error;
  }

  inst->capacity = capacity;
  inst->byte_offset = 0;
  inst->bit_offset = 0;
  *ctx = inst;

 error:

  if (r < 0) {
    
    if (NULL != inst) {
      
      status = tra_golomb_writer_destroy(inst);
      if (status < 0) {
        TRAE("After we failed to create the `tra_golomb` we also failed to cleanly destroy it.");
      }

      inst = NULL;
    }

    if (NULL != ctx) {
      *ctx = NULL;
    }
  }

  return r;
}

/* ------------------------------------------------------- */

int tra_golomb_writer_destroy(tra_golomb_writer* ctx) {

  if (NULL == ctx) {
    TRAE("Cannot destroy the given `tra_golomb` it's NULL.");
    return -1;
  }

  if (NULL != ctx->data) {
    free(ctx->data);
  }
  
  ctx->data = NULL;
  ctx->capacity = 0;
  ctx->byte_offset = 0;
  ctx->bit_offset = 0;

  free(ctx);
  ctx = NULL;
  
  return 0;
}

/* ------------------------------------------------------- */

/*
  Resets the byte and bit offsets; e.g. start with a fresh
  slate. This will also make sure that all bits in the buffer are
  set to 0x00.
*/
int tra_golomb_writer_reset(tra_golomb_writer* ctx) {

  if (NULL == ctx) {
    TRAE("Cannot reset the `tra_golomb` instance as it's NULL.");
    return -1;
  }

  if (NULL == ctx->data) {
    TRAE("Cannot reset the `tra_golomb` instance as it's `data` member is NULL.");
    return -2;
  }

  if (0 == ctx->capacity) {
    TRAE("Cannot reset the `tra_golomb` instance as it's `capacity` member is 0.");
    return -3;
  }

  memset(ctx->data, 0x00, ctx->capacity);
  
  ctx->byte_offset = 0;
  ctx->bit_offset = 0;

  return 0;
}

/* ------------------------------------------------------- */

/* 

   Write an unsigned integer Exponential Golomb code word.

   Exponential Golomb encoding, counts the number of bits that
   are required to store (val + 1) and writes this number minus
   one of zeros before writing (val + 1). Therefore the first
   step is to determine the number of bits that are
   required. There are intrinsics that can help with that; this
   is something we can improve in the future.

   Once we have the number of bits that are required to store
   (val + 1), we have to write this amount of zeros followed by
   the number.  Therefore we have to write:

   (num_bits - 1) => this amount of zeros
   (num_bits) => the actual value (val + 1).

   This is why we're doing: 2 * num_bits - 1, below. 

   Let's say we have `len = num_bits * 2 - 1`, and len is 19,
   then we start reading bit number 19, 18, 17 etc. and write
   those values into the buffer. We can assure that all bits
   "left of" `num_bits` are zeros as those are not used to 
   store (val + 1). 

   For example, imagine we want to store the (value+1) = 1000,
   which is represented by this bit string and requires 10 bits.
   Then we start "reading" (in tra_golomb_write_bits) at
   position: 2 * 10 - 1 = 19, which will always be zero.

   00000000 00000000 00000011 11101000
                 ^
                 | pos 19

   We take the same approach as used in [this reference][6], from
   Alex Izvorski, aizvorski@gmail.com.

*/
void tra_golomb_write_ue(tra_golomb_writer* ctx, uint32_t val) {

  uint32_t code_num = val + 1;
  uint32_t num_bits = 0;

  if (NULL == ctx) {
    TRAE("Cannot write an unsigned integer as the given `tra_golomb` is NULL. (exiting).");
    exit(EXIT_FAILURE);
  }

  while (0 != code_num) {
    code_num = code_num >> 1;
    num_bits++;
  }

  /* Store the code num */
  tra_golomb_write_bits(ctx, val + 1, 2 * num_bits - 1);
}

/* ------------------------------------------------------- */

/* 
   Write the given `val` using `num` bits. Note that this does
   not use Exponential Golomb coding. This function implements
   the `u(n)` value from the H264/AVC reference.
*/
void tra_golomb_write_u(tra_golomb_writer* ctx, uint32_t val, uint32_t num) {

  uint32_t i = 0;

  if (NULL == ctx) {
    TRAE("Cannot `tra_golomb_write_u()` as the given `tra_golomb_writer*` is NULL. (exiting).");
    exit(EXIT_FAILURE);
  }

  for (i = 0; i < num; ++i) {
    tra_golomb_write_bit(ctx, (val >> 1) & 0x01);
  }
}

/* ------------------------------------------------------- */

/* Write an signed integer. */
void tra_golomb_write_se(tra_golomb_writer* ctx, int32_t val) {

  if (NULL == ctx) {
    TRAE("Cannot write a signed integer as the given `tra_golomb` is NULL. (exiting).");
    exit(EXIT_FAILURE);
  }

  if (val <= 0) {
    tra_golomb_write_ue(ctx, -val * 2);
  }
  else {
    tra_golomb_write_ue(ctx, val * 2 - 1);
  }
} 

/* ------------------------------------------------------- */

void tra_golomb_write_bit(tra_golomb_writer* ctx, uint32_t val) {
  tra_golomb_write_bits(ctx, val, 1);
}

/* ------------------------------------------------------- */

/*

  This function will write the given `src` value using `num`
  bits. This function is called through
  e.g. `tra_golomb_write_ue()` and `tra_golomb_write_se()`. See the
  documentation of the `tra_golomb_write_ue()` function for a
  detailed description of this implementation. Also note that we
  will automatically grow the buffer if required.

  IMPORTANT: we extract `num` bits from `src`. This means that
  when you want to write a couple of ones (1) you call this
  function like: `tra_golomb_write_bits(ctx, 0xFF, 8)`. This will
  write 8 bits with a value 1.

  IMPORTANT: when you want to write an integer using some bits.
  e.g. the `u(n)` syntax from the H264 standard, you can us
  `tra_golomb_write_u()`.
  
 */
void tra_golomb_write_bits(tra_golomb_writer* ctx, uint32_t src, uint32_t num) {

  uint32_t nbytes_required = 0;
  uint32_t nbytes_left = 0;
  uint32_t new_capacity = 0;
  uint8_t* new_data = NULL;
  uint32_t bit_pos = 0;
  uint8_t is_set = 0;

  /* We're adding some strict validation here to make sure our buffers stays valid. */
  if (NULL == ctx) {
    TRAE("Cannot write the bits as the given `tra_golomb` is NULL. (exiting).");
    exit(EXIT_FAILURE);
  }

  if (NULL == ctx->data) {
    TRAE("Cannot write the bits as our storage hasn't been allocated. (exiting).");
    exit(EXIT_FAILURE);
  }

  if (ctx->byte_offset > ctx->capacity) {
    TRAE("The `byte_offset` of the `tra_golomb` is larger than the capacity; something went wrong. (exiting).");
    exit(EXIT_FAILURE);
  }
  
  if (0 == num) {
    TRAE("Cannot write 0 bits; makes no sense. (exiting).");
    exit(EXIT_FAILURE);
  }

  /* Do we need to grow? */
  nbytes_required = (num + CHAR_BIT - 1) / CHAR_BIT;
  nbytes_left = ctx->capacity - ctx->byte_offset;

  if (nbytes_required >= nbytes_left) {
    
    new_capacity = ctx->capacity;
    while (nbytes_required > new_capacity) {
      new_capacity = new_capacity * 2;
    }

    new_data = realloc(ctx->data, new_capacity);
    if (NULL == new_data) {
      TRAE("Failed to reallocate our buffer. We tried to grow from %u to %u. (exiting).", ctx->capacity, new_capacity);
      exit(EXIT_FAILURE);
    }

    ctx->data = new_data;
    ctx->capacity = new_capacity;
  }

  /* Write the requested bits from `src`. */
  while (bit_pos < num) {
    
    /* Toggle the bit to 1 when it's set in the source value. */
    is_set = (src & ((uint32_t)1 << (num - bit_pos - 1))) ? 1 : 0;
    ctx->data[ctx->byte_offset] |= (is_set << (CHAR_BIT - 1 - ctx->bit_offset));

    bit_pos++;

    /* When we've written all bits in the current byte; advance to the next byte position. */
    if (ctx->bit_offset == (CHAR_BIT - 1)) {
      ctx->bit_offset = 0;
      ctx->byte_offset++;
      continue;
    }

    /* Advance the bit position for the current byte. */
    ctx->bit_offset++;
  }
}

/* ------------------------------------------------------- */

int tra_golomb_writer_save_to_file(tra_golomb_writer* ctx, const char* filepath) {

  FILE* fp = NULL;
  int r = 0;
  
  if (NULL == ctx) {
    TRAE("Cannot save the `tra_golomb` buffer to a file as the given `tra_golomb_writer*` is NULL.");
    return -1;
  }

  if (NULL == filepath) {
    TRAE("Cannot save the `tra_golomb` buffer to a file as the given `filepath` is NULL.");
    return -2;
  }

  fp = fopen(filepath, "wb");
  if (NULL == fp) {
    TRAE("Cannot save the `tra_golomb` buffer to a file, failed to open `%s`.", filepath);
    return -3;
  }

  r = fwrite(ctx->data, ctx->byte_offset, 1, fp);
  if (1 != r) {
    TRAE("Failed to write the buffer of the given `tra_golomb`. ");
    r = -4;
    goto error;
  }

  r = fflush(fp);
  if (0 != r) {
    TRAE("Failed to flush the output file.");
    r = -5;
    goto error;
  }

 error:

  if (NULL != fp) {
    fclose(fp);
    fp = NULL;
  }
  
  return r;
}

/* ------------------------------------------------------- */
 
int tra_golomb_writer_print(tra_golomb_writer* ctx) {

  uint32_t max_dx = 0;
  uint32_t i = 0;
  char buf[32] = { 0 };
     
  if (NULL == ctx) {
    TRAE("Cannot print info about the `tra_golomb` as it's NULL.");
    return -1;
  }

  TRAD("tra_golomb");
  TRAD("  capacity: %u", ctx->capacity);
  TRAD("  byte_offset: %u", ctx->byte_offset);
  TRAD("  bit_offset: %u", ctx->bit_offset);
  TRAD("  data: %p", ctx->data);

  /* Print the first `max_val` values. */
  if (ctx->byte_offset > 0) {
    max_dx = ctx->byte_offset < 10 ? ctx->byte_offset : 10;
  }

  /* When not "flushed" the byte offset can be 0; we still want to see the first byte. */
  if (0 == max_dx && ctx->bit_offset > 0) {
    max_dx = 1;
  }

  for (i = 0; i < max_dx; ++i) {
    TRAD("  data[%u] = %s (%02x)", i, tra_golomb_get_bit_string(ctx->data[i]), ctx->data[i]);
  }
  
  return 0;
}

/* ------------------------------------------------------- */

int tra_golomb_reader_init(tra_golomb_reader* ctx, uint8_t* data, uint32_t nbytes) {

  if (NULL == ctx) {
    TRAE("Cannot initialize the `tra_golomb_reader`, given context pointer is NULL.");
    return -1;
  }

  if (NULL == data) {
    TRAE("Cannot initialize the `tra_golomb_reader` the given data is NULL.");
    return -2;
  }

  ctx->data = data;
  ctx->nbytes = nbytes;
  ctx->bit_offset = 7;
  ctx->byte_offset = 0;

  return 0;
}

/* ------------------------------------------------------- */

int tra_golomb_reader_shutdown(tra_golomb_reader* ctx) {

  if (NULL == ctx) {
    TRAE("Cannot shutodwn the `tra_golomb_reader` as the given reader is NULL.");
    return -1;
  }

  ctx->data = NULL;
  ctx->nbytes = 0;
  ctx->bit_offset = 7;
  ctx->byte_offset = 0;

  return 0;
}

/* ------------------------------------------------------- */

uint8_t tra_golomb_read_u8(tra_golomb_reader* ctx) {

  if (NULL == ctx) {
    TRAE("Cannot read u8, given `tra_golomb_reader*` is NULL. (exiting).");
    exit(EXIT_FAILURE);
  }

  if (ctx->byte_offset + 1 >= ctx->nbytes) {
    TRAE("Cannot read u8, would go out of bounds. (exiting).");
    exit(EXIT_FAILURE);
  }

  /* When we're at a byte boundary we can read the byte directly. */
  if (7 == ctx->bit_offset) {
    ctx->byte_offset++;
    return ctx->data[ctx->byte_offset - 1];
  }

  TRAE("@todo read a byte when we're not at a byte boundary (%u), bit offset: %u. (exiting).", ctx->byte_offset, ctx->bit_offset);
  exit(EXIT_FAILURE);

  return 0;
}

/* ------------------------------------------------------- */

uint8_t tra_golomb_read_bit(tra_golomb_reader* ctx) {

  uint8_t val = 0;
  
  if (NULL == ctx) {
    TRAE("Cannot read a bit, given `tra_golomb_reader*` is NULL. (exiting).");
    exit(EXIT_FAILURE);
  }

  //TRAD("  reading bit %u from byte %02X", ctx->bit_offset, ctx->data[ctx->byte_offset]);
  
  val = ctx->data[ctx->byte_offset] & (1 << ctx->bit_offset) ? 1 : 0;

  /* There are still bits to read (reading from MSB to LSB) */
  if (ctx->bit_offset > 0) {
    ctx->bit_offset--;
    return val;
  }

  /* We read all the bits in the current byte, reset the bit offset; starting with MSB == bit 7 (start counting at 0).  */
  ctx->bit_offset = 7;

  /* We have to jump to the next byte; check if the we stay inside our buffer. */
  if (ctx->byte_offset + 1 >= ctx->nbytes) {
    return val;
  }

  ctx->byte_offset++;

  return val;
}

uint8_t tra_golomb_peek_bit(tra_golomb_reader* ctx) {

  if (NULL == ctx) {
    TRAE("Cannot peek a bit as the given `tra_golomb_reader*` is NULL. (exiting).");
    exit(EXIT_FAILURE);
  }

  return ctx->data[ctx->byte_offset] & (1 << ctx->bit_offset) ? 1 : 0;
}

/* ------------------------------------------------------- */

uint32_t tra_golomb_read_bits(tra_golomb_reader* ctx, uint32_t num) {

  uint32_t val = 0;
  uint32_t i = 0;
  uint8_t bit = 0;

  if (NULL == ctx) {
    TRAE("Cannot read bits as the given `tra_golomb_reader*` is NULL. (exiting).");
    exit(EXIT_FAILURE);
  }

  if (num > 32) {
    TRAE("Cannot read %u bit's we can only read 32 bits max. (exiting).", num);
    exit(EXIT_FAILURE);
  }

  while (num > 0)  {

    bit = (ctx->data[ctx->byte_offset] & (1 << ctx->bit_offset)) ? 1 : 0;
    val |= bit << (num - 1);
    num--;
      
    /* When we don't have to increment the byte offset we continue. */
    if (ctx->bit_offset > 0) {
      ctx->bit_offset--;
      continue;
    }

    /* We've read all bits of the current byte. */
    ctx->bit_offset = 7;

    /* As long as we didn't read everything incrment the byte offset */
    if ((ctx->byte_offset + 1) < ctx->nbytes) {
      ctx->byte_offset++;
    }
  }

  return val;
}

/* ------------------------------------------------------- */

void tra_golomb_skip_bit(tra_golomb_reader* ctx) {

  if (NULL == ctx) {
    TRAE("Cannot skip a bit as the given `tra_golomb_reader*` is NULL. (exiting). ");
    exit(EXIT_FAILURE);
  }
  
  /* We don't have to move the byte offset */
  if (ctx->bit_offset > 0) {
    ctx->bit_offset--;
    return;
  }

  /* Reset the bit offset; we start reading at MSB */
  ctx->bit_offset = 7;

  /* We have read everything */
  if (ctx->byte_offset + 1 >= ctx->nbytes) {
    return;
  }

  ctx->byte_offset++;
}

void tra_golomb_skip_bits(tra_golomb_reader* ctx, uint32_t num) {

  uint32_t i = 0;
  
  if (NULL == ctx) {
    TRAE("Cannot skip a bit as the given `tra_golomb_reader*` is NULL. (exiting). ");
    exit(EXIT_FAILURE);
  }

  for (i = 0; i < num; ++i) {
    tra_golomb_skip_bit(ctx);
  }
}

/* ------------------------------------------------------- */

/* 
   GENERAL INFO :

     Reads an Exponential Golomb encoded value. The process is 
     described in the [H264 spec][0] and is used to read
     H264 bitstream elements with a described which is listed
     below:
     
     - ue(v)
     - me(v)
     - se(v)
     - te(v) - Truncated Exp. Golomb coded.
     
     To read an Exp. Golomb encoded value we first find the 
     _leadingZeroBits which is done using the following steps: 
     
       - read the bits up to and including the first bit with 1
       - count the bits leading to this bit with a value of 1
     
     Then we can find the value using:
     
        value = 2^(leadingZeroBits) - 1 + read_bits(leadingZeroBits).
     
      The `read_bits()` function reads the `leadingZeroBits` bits
      as an unsigned integer with the MSB first.

  REFERENCES:

    - [0]: https://www.itu.int/rec/dologin_pub.asp?lang=e&id=T-REC-H.264-201610-S!!PDF-E&type=items "H264 Spec"
   
*/
uint32_t tra_golomb_read_ue(tra_golomb_reader* ctx) {

  uint32_t leading_zeros = 0;
  uint32_t val = 0;

  if (NULL == ctx) {
    TRAE("Cannot `tra_golomb_read_ue()`, given `tra_golomb_reader` is NULL. (exiting). ");
    exit(EXIT_FAILURE);
  }

  while (1 != tra_golomb_read_bit(ctx)) {
    leading_zeros++;
  }

  if (0 == leading_zeros) {
    return 0;
  }

  val = (1 << leading_zeros) - 1 + tra_golomb_read_bits(ctx, leading_zeros);

  return val;
}

/* ------------------------------------------------------- */

/*

  This function can be used to read a ue(v) value from the
  bitstream at the given `offset` position. The `offset` is the
  absolute offset into the bitstream.

  I've created this function to be able to read the
  `seq_parameter_set_id` of the SPS before parsing the actual
  buffer/fields. When you create a h264 parser, you will most
  likely create an array like: `sps sps_list[32]`. The spec
  defines a maximum `seq_parameter_set_id` of 31. Therefore it
  makes sense to store them into an array. Now, to quickly find
  the SPS that you want to setup, you can use
  `tra_golomb_peek_ue(bs, 3)` to read the
  `seq_parameter_set_id()`. You can use it in a similar way to
  get the ids of the PPS and slice header.

  We could have implemented all our read functions using a peek
  and then moving the offsets; though I think its clean to have a
  specific function just to peek() as it's used in a couple of
  rare cases.

  The approach to reading the ue(v) value, is the same as
  `tra_golomb_read_ue()`. 

  - Count the leading zeros up and including the first bit that has a value of 1.
  - Create `num = (1 << leading_zeros) - 1`.
  - Read `leading_zeros` bit as an unsigned integer where the first read bit is the MSB.
  - Result: `num = (1 << leading_zeros) - 1 + (readBits(leading_zeros))`

 */
uint32_t tra_golomb_peek_ue(tra_golomb_reader* ctx, uint32_t offset) {

  uint32_t leading_zeros = 0;
  uint8_t bit_offset = 7;
  uint32_t val = 0;
  uint32_t num = 0;
  uint8_t bit = 0;
  
  if (NULL == ctx) {
    TRAE("Cannot peek_ue() as the given `tra_golomb_reader*` is NULL. (exiting).");
    exit(EXIT_FAILURE);
  }

  if (offset >= ctx->nbytes) {
    TRAE("Cannot `tra_golomb_peek_ue()` as we would read out of bounds (%u > %u). (exiting)", offset, ctx->nbytes);
    exit(EXIT_FAILURE);
  }

  /* Read all bits which are 0. */
  while (0 == bit) {

    /* Is the current bit a zero? */
    bit = (0 == (ctx->data[offset] & (1 << bit_offset))) ? 0 : 1;
    if (0 == bit) {
      leading_zeros++;
    }

    if (bit_offset > 0) {
      bit_offset--;
      continue;
    }

    /* Jump to the next byte. */
    bit_offset = 7;
      
    if ((offset + 1) >= ctx->nbytes) {
      /* We've read all bytes. */
      break;
    }

    offset++;
  }

  /* This is our first step to get our value. */
  num = (1 << leading_zeros) - 1;

  /* 
     The next step is to read the next `leading_zeros` bits as an
     unsigned integer, starting with the MSB.
  */
  while (leading_zeros > 0) {

    bit = (0 == (ctx->data[offset] & (1 << bit_offset))) ? 0x00 : 0x01;
    val = (val << 1) | bit;
    leading_zeros--;
    
    if (bit_offset > 0) {
      bit_offset--;
      continue;
    }

    /* Jump to the next byte. */
    bit_offset = 7;

    if ((offset + 1) >= ctx->nbytes) {
      /* We've read all bytes. */
      break;
    }

    offset++;
  }

  num = num + val;

  return num;
}

/* ------------------------------------------------------- */

int32_t tra_golomb_read_se(tra_golomb_reader* ctx) {

  int32_t val = 0;
  
  if (NULL == ctx) {
    TRAE("Cannot `tra_golomb_read_se()` as the given `tra_golomb_reader` is NULL. (exiting).");
    exit(EXIT_FAILURE);
  }

  val = tra_golomb_read_ue(ctx);

  /* Positive value */
  if (val & 0x01) {
    return (val + 1) / 2;
  }

  /* Negative value. */
  return -(val / 2);
}

/* ------------------------------------------------------- */

const char* tra_golomb_get_bit_string(uint8_t val) {

  static char buf[16] = { 0 };
  uint32_t bit = 0;
  
  for (bit = 0; bit < 8; ++bit) {
    snprintf(buf + bit, sizeof(buf) - bit, "%c", (val & (1 << (7 - bit))) ? '1' : '0');
  }

  return buf;
}

/* ------------------------------------------------------- */

/* 

   This will write the "RBSP trailing bits" as defined in section
   7.3.2.11 of the H264/AVC standard. We first write one bit,
   which is called the `rbsp_stop_one_bit()` and then fill up the
   remaning bits until we are at a byte offset.

   When the bit offset is not on a byte boundary (e.g. 0) we fill
   the remaining bits with zeros. We will also advance the `byte_offset`
   value so that we can continue writing at the next byte. 

   REFERENCES:

     [0]: https://imgur.com/a/Ud4RiXi "RBSP trailing bits syntax"

*/
void tra_h264_write_trailing_bits(tra_golomb_writer* ctx) {

  if (NULL == ctx) {
    TRAE("Cannot write the trailing bits, given `tra_golomb_writer*` is NULL. (exiting)");
    exit(EXIT_FAILURE);
  }

  /* Write the `rbsp_stop_one_bit()`. */
  tra_golomb_write_bit(ctx, 1);

  while (ctx->bit_offset != CHAR_BIT) {
    ctx->data[ctx->byte_offset] &= ~(1 << (CHAR_BIT - 1 - ctx->bit_offset));
    ctx->bit_offset++;
  }

  ctx->bit_offset = 0;
  ctx->byte_offset++;
}

/* ------------------------------------------------------- */

/* Writes 0x00 0x00 0x00 0x01. */
void tra_h264_write_annexb_header(tra_golomb_writer* ctx) {

  if (NULL == ctx) {
    TRAE("Cannot write the annex-b header, given `tra_golomb_writer*` is NULL. (exiting).");
    exit(EXIT_FAILURE);
  }

  tra_golomb_write_bits(ctx, 0x00000001, 32); 
}

/* ------------------------------------------------------- */

void tra_h264_write_nal_header(tra_golomb_writer* ctx, uint32_t nalRefIdc, uint32_t nalUnitType) {

  if (NULL == ctx) {
    TRAE("The given `tra_golomb_writer*` is NULL, cannot write the nal header. (exiting)");
    exit(EXIT_FAILURE);
  }

  tra_golomb_write_bit(ctx, 0);
  tra_golomb_write_u(ctx, nalRefIdc, 2);
  tra_golomb_write_bits(ctx, nalUnitType, 5);
}

/* ------------------------------------------------------- */

