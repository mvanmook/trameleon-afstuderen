#ifndef TRA_DICT_H
#define TRA_DICT_H
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

  DICTIONARY
  ==========

  GENERAL INFO:

    The `tra_dict_*` functions are used to create basic key-value
    pair storage.  The main usage of this dictionary in this
    library is as a general settings object. The `tra_dict` was
    created to be serializable to/from JSON and therefore only
    supports valid JSON types. 

  USAGE:

    There are a couple of things you need to know when using 
    the `tra_dict` type and functions.

    - We support "array" and "object" types. Each property that
      that holds an object or array must point to a unique array
      or object. You cannot set the same object or array to
      different properties. For each object or array type
      property you have to call `tra_dict_create()` for objects
      or `tra_dict_array_create()` for arrays.

    - IMPORTANT: When you create a root `tra_dict` and add
      properties to this root item, all children that you add
      using the `_dict_set_*()` functions or `array_add_*()`
      functions will be deallocated automatically when you destroy
      the dictionary.

    - IMPORTANT: We do not allow values to change! Once you've
      added an array value or set an object type, it can't be
      changed anymore!

    - IMPORTANT: This is a very lean and mean implemenatation and
      the JSON suppport is limited and a bit lacking in features.

*/

/* ------------------------------------------------------- */

#include <stdint.h>

/* ------------------------------------------------------- */

typedef struct tra_dict tra_dict;
typedef struct tra_buffer tra_buffer;

/* ------------------------------------------------------- */

int tra_dict_create(tra_dict** ctx);
int tra_dict_array_create(tra_dict** ctx);
int tra_dict_destroy(tra_dict* ctx);
int tra_dict_to_json(tra_dict* ctx, tra_buffer* buf);
int tra_dict_print(tra_dict* ctx);
int tra_dict_has_property(tra_dict* ctx, const char* name); /* Returns < 0 when the given name wasn't found or an error occured (we will log the error). When found we return 0 */

/* ------------------------------------------------------- */

uint64_t tra_dict_get_u64(tra_dict* ctx, const char* name, uint64_t def);
uint32_t tra_dict_get_u32(tra_dict* ctx, const char* name, uint32_t def);
uint16_t tra_dict_get_u16(tra_dict* ctx, const char* name, uint16_t def);
uint8_t tra_dict_get_u8(tra_dict* ctx, const char* name, uint8_t def);
int64_t tra_dict_get_s64(tra_dict* ctx, const char* name, int64_t def);
int32_t tra_dict_get_s32(tra_dict* ctx, const char* name, int32_t def);
int16_t tra_dict_get_s16(tra_dict* ctx, const char* name, int16_t def);
int8_t tra_dict_get_s8(tra_dict* ctx, const char* name, int8_t def);
float tra_dict_get_float(tra_dict* ctx, const char* name, float def);
double tra_dict_get_double(tra_dict* ctx, const char* name, double def);
double tra_dict_get_real(tra_dict* ctx, const char* name, double def); /* Get a real number (either float or double). */
uint64_t tra_dict_get_unumber(tra_dict* ctx, const char* name, uint64_t def); /* Get numeric, unsigned number. */
int64_t tra_dict_get_snumber(tra_dict* ctx, const char* name, int64_t def); /* Get numeric, signed number. */

/* ------------------------------------------------------- */

int tra_dict_set_u64(tra_dict* ctx, const char* name, uint64_t val);
int tra_dict_set_u32(tra_dict* ctx, const char* name, uint32_t val);
int tra_dict_set_u16(tra_dict* ctx, const char* name, uint16_t val);
int tra_dict_set_u8(tra_dict* ctx, const char* name, uint8_t val);
int tra_dict_set_s64(tra_dict* ctx, const char* name, int64_t val);
int tra_dict_set_s32(tra_dict* ctx, const char* name, int32_t val);
int tra_dict_set_s16(tra_dict* ctx, const char* name, int16_t val);
int tra_dict_set_s8(tra_dict* ctx, const char* name, int8_t val);
int tra_dict_set_float(tra_dict* ctx, const char* name, float val);
int tra_dict_set_double(tra_dict* ctx, const char* name, double val);
int tra_dict_set_string(tra_dict* ctx, const char* name, const char* val);
int tra_dict_set_array(tra_dict* ctx, const char* name, tra_dict* val);
int tra_dict_set_object(tra_dict* ctx, const char* name, tra_dict* val);

/* ------------------------------------------------------- */

int tra_dict_array_add_u64(tra_dict* ctx, uint64_t val);
int tra_dict_array_add_u32(tra_dict* ctx, uint32_t val);
int tra_dict_array_add_u16(tra_dict* ctx, uint16_t val);
int tra_dict_array_add_u8(tra_dict* ctx, uint8_t val);
int tra_dict_array_add_s64(tra_dict* ctx, int64_t val);
int tra_dict_array_add_s32(tra_dict* ctx, int32_t val);
int tra_dict_array_add_s16(tra_dict* ctx, int16_t val);
int tra_dict_array_add_s8(tra_dict* ctx, int8_t val);
int tra_dict_array_add_string(tra_dict* ctx, const char* val);
int tra_dict_array_add_object(tra_dict* ctx, tra_dict* val);
int tra_dict_array_add_array(tra_dict* ctx, tra_dict* val);

/* ------------------------------------------------------- */

#endif
