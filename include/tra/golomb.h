#ifndef TRA_GOLOMB_H
#define TRA_GOLOMB_H

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

  EXPONENTIAL GOLOMB CODING
  =========================

  GENERAL INFO:

    [Exponential Golomb][0] coding is a technique to store "data"
    in a bitstream. This coding style is used by the H264
    standard. It's an [universal code][1], which is related to
    data compression. In data compression an [universal code][1]
    is a prefix code that maps positive integers onto binary code
    words.

    An universal code has the property that whatever the true
    probability distribution on integers, as long as the
    distribution is monotonic, the expected lengths of the
    codewords are within a constant factor of the expected
    lengths that the optimal code for that propability
    distribution would have assigned. In general most prefix
    codes for integers assign longer codewords to larger
    integers.

    An Exponential Golomb code is used to encode _positive_
    integers. To encode a positive integer `X`, as an golomb code
    you follow these simple steps:

      - Write down `X + 1` in binary
      - Count the bits written => `Y`
      - Write down `Y - 1` => Z
      - Write down the number of starting zero bits preceding the previous bitstring.

    For example we take the decimal number 3 which is X. Add one
    to X which makes 4. 4 can be represented using the bits
    `100`. We need 3 bits to write down 4. The next step is to
    subtract one from the number of bits we need, which makes
    3. Then, we subtract one from this number. This number, 2,
    indicates how many zeros we have to write down in front of the
    number we created to coun the number of bits which is
    X. Therefore the Golomb style encoding of 3 is `00100`.

    From Wikipedia:

      0 ⇒ 1 ⇒ 1
      1 ⇒ 10 ⇒ 010
      2 ⇒ 11 ⇒ 011
      3 ⇒ 100 ⇒ 00100
      4 ⇒ 101 ⇒ 00101
      5 ⇒ 110 ⇒ 00110
      6 ⇒ 111 ⇒ 00111
      7 ⇒ 1000 ⇒ 0001000
      8 ⇒ 1001 ⇒ 0001001

    H264 uses an extension to the Exponential Golomb coding which
    makes it possible to add negative integers. In this case
    negative integers are stored by starting with `-2X`, making
    them positive and even. Positive integers are stored by started
    with `2X - 1`, making them odd. From [Wikipedia][0]:

       0 ⇒ 0 ⇒ 1 ⇒ 1
       1 ⇒ 1 ⇒ 10 ⇒ 010
      −1 ⇒ 2 ⇒ 11 ⇒ 011
       2 ⇒ 3 ⇒ 100 ⇒ 00100
      −2 ⇒ 4 ⇒ 101 ⇒ 00101
       3 ⇒ 5 ⇒ 110 ⇒ 00110
      −3 ⇒ 6 ⇒ 111 ⇒ 00111
       4 ⇒ 7 ⇒ 1000 ⇒ 0001000

  IMPLEMENTATION:

    The current implementation is based on the [dirac][3] (see
    byteio.cpp) and [libva-utils][4] implementation. We could
    optimize this by using [find first set/one][2]
    intrinsics.
    
  REFERENCES:

     [0]: https://en.wikipedia.org/wiki/Exponential-Golomb_coding
     [1]: https://en.wikipedia.org/wiki/Universal_code_(data_compression) "Universal Code"
     [2]: https://en.wikipedia.org/wiki/Find_first_set "Find first set"
     [3]: https://sourceforge.net/projects/dirac/ "LibDirac"
     [4]: https://github.com/intel/libva-utils/blob/master/encode/h264encode.c "H264 encode example"
     [5]: https://github.com/blueskyson/Exponential-Golomb-coding/blob/master/exp-golomb.c "Implementation using `__builtin_clz`.
     [6]: https://github.com/aizvorski/h264bitstream/blob/master/bs.h
     [7]: https://github.com/gilanghamidy/DifferentialFuzzingWASM-gecko-dev/blob/master/media/webrtc/trunk/webrtc/rtc_base/bitbuffer.cc "BitBuffer from WebRtc"
 
*/

/* ------------------------------------------------------- */

#include <stdint.h>

/* ------------------------------------------------------- */

#define TRA_NAL_REF_IDC_NONE   0
#define TRA_NAL_REF_IDC_LOW    1
#define TRA_NAL_REF_IDC_MEDIUM 2
#define TRA_NAL_REF_IDC_HIGH   3

/* ------------------------------------------------------- */

typedef struct tra_golomb_writer {
  uint32_t capacity;                                                                              /* How many bytes we can store in `data`. */
  uint32_t byte_offset;                                                                           /* The offset of the byte into which we currently write. */
  uint8_t bit_offset;                                                                             /* Starts at the most significant bit. 0 represents the most significant bit, a value of 7 represents the least significant bit. */
  uint8_t* data;                                                                                  /* The data the writer has stored. */
} tra_golomb_writer;

/* ------------------------------------------------------- */

typedef struct tra_golomb_reader {
  uint8_t* data;                                                                                  /* The data we read from; owned by you, is set via `tra_golomb_reader_init()`. */
  uint32_t nbytes;                                                                                /* The number of bytes in `data` */
  uint8_t bit_offset;                                                                             /* The bit-offset in the current byte (indicated by `byte_offset`) as Golomb encoding works on a bit level. */
  uint32_t byte_offset;                                                                           /* Then bytes offset of our "read" position. */
} tra_golomb_reader;

/* ------------------------------------------------------- */

int tra_golomb_writer_create(tra_golomb_writer** ctx, uint32_t capacity);                         /* Create a golomb writer that can hold `capacity` numbert of bytes.  */
int tra_golomb_writer_destroy(tra_golomb_writer* ctx);                                            /* Destroys and deallocates all the used memory of the writer. */
int tra_golomb_writer_reset(tra_golomb_writer* ctx);                                              /* Resets the byte and bit offsets; e.g. start with a fresh slate. This will also make sure that all bits in the buffer are set to 0x00. */
int tra_golomb_writer_save_to_file(tra_golomb_writer* ctx, const char* filepath);                 /* Writes out the current buffer. */
int tra_golomb_writer_print(tra_golomb_writer* ctx);                                              /* Prints some info about the `tra_golomb` instance. */

/* ------------------------------------------------------- */

int tra_golomb_reader_init(tra_golomb_reader* ctx, uint8_t* data, uint32_t nbytes);               /* Resets the members of the `tra_golomb_reader`. You own the given `data` buffer. You can call this mulitple time after each other. */
int tra_golomb_reader_shutdown(tra_golomb_reader* ctx);                                           /* Unsets the members of the given reader. As you own the `data` member we don't deallocate. */
uint8_t tra_golomb_read_u8(tra_golomb_reader* ctx);                                
uint8_t tra_golomb_read_bit(tra_golomb_reader* ctx);
uint32_t tra_golomb_read_bits(tra_golomb_reader* ctx, uint32_t num);
uint8_t tra_golomb_peek_bit(tra_golomb_reader* ctx);
void tra_golomb_skip_bit(tra_golomb_reader* ctx);
void tra_golomb_skip_bits(tra_golomb_reader* ctx, uint32_t num);
uint32_t tra_golomb_read_ue(tra_golomb_reader* ctx);                                              /* Reads an Exponential Golomb encoded value. */
int32_t tra_golomb_read_se(tra_golomb_reader* ctx);
uint32_t tra_golomb_peek_ue(tra_golomb_reader* ctx, uint32_t offset);                             /* Peek the value of an Exponential Golomb encoded integer at the given offset. The offset is the absolute offset! We start reading from the MSB at `offset` */

/* ------------------------------------------------------- */

void tra_golomb_write_bit(tra_golomb_writer* ctx, uint32_t val);
void tra_golomb_write_bits(tra_golomb_writer* ctx, uint32_t src, uint32_t num);                   /* IMPORTANT: this will write the bits that are set in the given `src`. So when `src` is (0x01) and `num` is 2, it will write two bits:  1 and 0. */
void tra_golomb_write_u(tra_golomb_writer* ctx, uint32_t val, uint32_t num);                      /* Write the given `val` using `num` bits. Note that this does not use Exponential Golomb coding. */
void tra_golomb_write_ue(tra_golomb_writer* ctx, uint32_t val);                                   /* Write an unsigned integer Exponential Golomb coded integer. */
void tra_golomb_write_se(tra_golomb_writer* ctx, int32_t val);                                    /* Write an signed integer Exponential Golomb coded integer. */

/* ------------------------------------------------------- */

const char* tra_golomb_get_bit_string(uint8_t val);                                               /* Returns the bitstring of the given byte; e.g. `00100000` */

/* ------------------------------------------------------- */

void tra_h264_write_trailing_bits(tra_golomb_writer* ctx);                                        /* When bit offset is not on a byte boundary we fill the remaining bits with zeros. */
void tra_h264_write_annexb_header(tra_golomb_writer* ctx);                                        /* Writes 0x00 0x00 0x00 0x01. */
void tra_h264_write_nal_header(tra_golomb_writer* ctx, uint32_t nalRefIdc, uint32_t nalUnitType); /* Writes the 1-byte nal header. */

/* ------------------------------------------------------- */

#endif
