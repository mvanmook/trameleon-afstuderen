#include <stdlib.h>
#include <stdio.h>
#include <tra/log.h>
#include <tra/buffer.h>
#include <tra/dict.h>

/* ------------------------------------------------------- */

#define TRA_DICT_TYPE_NONE   0
#define TRA_DICT_TYPE_U8     1
#define TRA_DICT_TYPE_U16    2
#define TRA_DICT_TYPE_U32    3
#define TRA_DICT_TYPE_U64    4
#define TRA_DICT_TYPE_S8     5
#define TRA_DICT_TYPE_S16    6  
#define TRA_DICT_TYPE_S32    7
#define TRA_DICT_TYPE_S64    8
#define TRA_DICT_TYPE_FLT    9
#define TRA_DICT_TYPE_DBL    10
#define TRA_DICT_TYPE_STR    11
#define TRA_DICT_TYPE_ARRAY  12 
#define TRA_DICT_TYPE_OBJECT 13

/* ------------------------------------------------------- */

struct tra_dict {

  uint8_t type;
  char* name; /* [OURS]: The name of the item, is owned by the item. */
   
  union {
    uint8_t u8;
    uint16_t u16;
    uint32_t u32;
    uint64_t u64;
    int8_t s8;
    int16_t s16;
    int32_t s32;
    int64_t s64;
    float f;
    double d;
    char* str;
    tra_dict* values; /* [OURS]: When this holds a value, we will deallocate it. */
  } data;

  tra_dict* next;
};

/* ------------------------------------------------------- */

static int dict_create(uint8_t type, tra_dict** ctx);
static int dict_find(tra_dict* ctx, const char* name, tra_dict** result); /* Find the item inside `ctx` with the given name. Returns 0 when no error occured otherwise < 0. `result` is set to the item when we found it. */
static int dict_append(tra_dict* ctx, tra_dict* item); /* Appends the given `item` to the internal `data.values` member of `ctx`. */
static int dict_object_property_create(tra_dict* obj, uint8_t type, const char* name, tra_dict** result); /* Create and add a property to an object; performing validation. This function doesn't set the value; that's done by the higher level functions. */
static int dict_array_property_create(tra_dict* array, uint8_t type, tra_dict** result); /* Create and add a property to an array. */

/* ------------------------------------------------------- */
static int dict_json_from_dict(tra_dict* ctx, tra_buffer* buf, int depth);
static int dict_json_from_u64(tra_dict* ctx, tra_buffer* buf, int depth);
static int dict_json_from_u32(tra_dict* ctx, tra_buffer* buf, int depth);
static int dict_json_from_u16(tra_dict* ctx, tra_buffer* buf, int depth);
static int dict_json_from_u8(tra_dict* ctx, tra_buffer* buf, int depth);
static int dict_json_from_s64(tra_dict* ctx, tra_buffer* buf, int depth);
static int dict_json_from_s32(tra_dict* ctx, tra_buffer* buf, int depth);
static int dict_json_from_s16(tra_dict* ctx, tra_buffer* buf, int depth);
static int dict_json_from_s8(tra_dict* ctx, tra_buffer* buf, int depth);
static int dict_json_from_float(tra_dict* ctx, tra_buffer* buf, int depth);
static int dict_json_from_double(tra_dict* ctx, tra_buffer* buf, int depth);
static int dict_json_from_object(tra_dict* ctx, tra_buffer* buf, int depth);
static int dict_json_from_array(tra_dict* ctx, tra_buffer* buf, int depth);
static int dict_json_from_string(tra_dict* ctx, tra_buffer* buf, int depth);

static int dict_json_indent(tra_buffer* buf, int depth);

/* ------------------------------------------------------- */

static const char* dict_type_to_string(uint8_t type);

/* ------------------------------------------------------- */

static int dict_create(uint8_t type, tra_dict** ctx) {

  tra_dict* inst = NULL;

  if (NULL == ctx) {
    TRAE("Cannot create the `tra_dict`: given `tra_dict**` is NULL.");
    return -1;
  }

  if (NULL != *ctx) {
    TRAE("Cannot create the `tra_dict`: given `*tra_dict**` is NOT NULL. Did you forget to initialize the destination to NULL?");
    return -2;
  }

  inst = calloc(1, sizeof(tra_dict));
  if (NULL == inst) {
    TRAE("Cannot create the `tra_dict`: failed to allocate.");
    return -3;
  }

  inst->type = type;

  *ctx = inst;

  return 0;
}

/* ------------------------------------------------------- */

int tra_dict_create(tra_dict** ctx) {
  return dict_create(TRA_DICT_TYPE_OBJECT, ctx);
}

int tra_dict_array_create(tra_dict** ctx) {
  return dict_create(TRA_DICT_TYPE_ARRAY, ctx);
}

/* ------------------------------------------------------- */

uint64_t tra_dict_get_u64(tra_dict* ctx, const char* name, uint64_t def) {

  tra_dict* item = NULL;
  int r = 0;

  r = dict_find(ctx, name, &item);
  if (r < 0) {
    TRAE("Something went wrong while trying to find the item `%s`. ", name);
    return def;
  }

  if (NULL != item) {
    return item->data.u64;
  }

  return def;
}

uint32_t tra_dict_get_u32(tra_dict* ctx, const char* name, uint32_t def) {

  tra_dict* item = NULL;
  int r = 0;

  r = dict_find(ctx, name, &item);
  if (r < 0) {
    TRAE("Something went wrong while trying to find the item `%s`. ", name);
    return def;
  }

  if (NULL != item) {
    return item->data.u32;
  }

  return def;
}

uint16_t tra_dict_get_u16(tra_dict* ctx, const char* name, uint16_t def) {

  tra_dict* item = NULL;
  int r = 0;

  r = dict_find(ctx, name, &item);
  if (r < 0) {
    TRAE("Something went wrong while trying to find the item `%s`. ", name);
    return def;
  }

  if (NULL != item) {
    return item->data.u16;
  }

  return def;
}

uint8_t tra_dict_get_u8(tra_dict* ctx, const char* name, uint8_t def) {

  tra_dict* item = NULL;
  int r = 0;

  r = dict_find(ctx, name, &item);
  if (r < 0) {
    TRAE("Something went wrong while trying to find the item `%s`. ", name);
    return def;
  }

  if (NULL != item) {
    return item->data.u8;
  }

  return def;
}

/* ------------------------------------------------------- */

int64_t tra_dict_get_s64(tra_dict* ctx, const char* name, int64_t def) {

  tra_dict* item = NULL;
  int r = 0;

  r = dict_find(ctx, name, &item);
  if (r < 0) {
    TRAE("Something went wrong while trying to find the item `%s`. ", name);
    return def;
  }

  if (NULL != item) {
    return item->data.s64;
  }

  return def;
}

int32_t tra_dict_get_s32(tra_dict* ctx, const char* name, int32_t def) {

  tra_dict* item = NULL;
  int r = 0;

  r = dict_find(ctx, name, &item);
  if (r < 0) {
    TRAE("Something went wrong while trying to find the item `%s`. ", name);
    return def;
  }

  if (NULL != item) {
    return item->data.s32;
  }

  return def;
}

int16_t tra_dict_get_s16(tra_dict* ctx, const char* name, int16_t def) {

  tra_dict* item = NULL;
  int r = 0;

  r = dict_find(ctx, name, &item);
  if (r < 0) {
    TRAE("Something went wrong while trying to find the item `%s`. ", name);
    return def;
  }

  if (NULL != item) {
    return item->data.s16;
  }

  return def;
}

int8_t tra_dict_get_s8(tra_dict* ctx, const char* name, int8_t def) {

  tra_dict* item = NULL;
  int r = 0;

  r = dict_find(ctx, name, &item);
  if (r < 0) {
    TRAE("Something went wrong while trying to find the item `%s`. ", name);
    return def;
  }

  if (NULL != item) {
    return item->data.s8;
  }

  return def; 
}

/* ------------------------------------------------------- */

float tra_dict_get_float(tra_dict* ctx, const char* name, float def) {

  tra_dict* item = NULL;
  int r = 0;

  r = dict_find(ctx, name, &item);
  if (r < 0) {
    TRAE("Something went wrong while trying to find the item `%s`. ", name);
    return def;
  }

  if (NULL != item) {
    return item->data.f;
  }

  return def; 
}
  
double tra_dict_get_double(tra_dict* ctx, const char* name, double def) {
  
  tra_dict* item = NULL;
  int r = 0;
  
  r = dict_find(ctx, name, &item);
  if (r < 0) {
    TRAE("Something went wrong while trying to find the item `%s`. ", name);
    return def;
  }

  if (NULL != item) {
    return item->data.d;
  }

  return def; 
}

/* ------------------------------------------------------- */

uint64_t tra_dict_get_unumber(tra_dict* ctx, const char* name, uint64_t def) {

  tra_dict* item = NULL;
  int r = 0;

  r = dict_find(ctx, name, &item);
  if (r < 0) {
    TRAE("Something went wrong while trying to find the item `%s`. ", name);
    return def;
  }

  if (NULL == item) {
    return def;
  }

  /* Return any of the unsigned types. */
  switch (item->type) {
    
    case TRA_DICT_TYPE_U64: {
      return item->data.u64;
    }
    
    case TRA_DICT_TYPE_U32: {
      return item->data.u32;
    }
    
    case TRA_DICT_TYPE_U16: {
      return item->data.u16;
    }
    
    case TRA_DICT_TYPE_U8: {
      return item->data.u8;
    }
  }

  return def;
}

int64_t tra_dict_get_snumber(tra_dict* ctx, const char* name, int64_t def) {

  tra_dict* item = NULL;
  int r = 0;

  r = dict_find(ctx, name, &item);
  if (r < 0) {
    TRAE("Something went wrong while trying to find the item `%s`. ", name);
    return def;
  }

  if (NULL == item) {
    return def;
  }

  /* Return any of the signed types. */
  switch (item->type) {
    
    case TRA_DICT_TYPE_S64: {
      return item->data.s64;
    }
    
    case TRA_DICT_TYPE_S32: {
      return item->data.s32;
    }
    
    case TRA_DICT_TYPE_S16: {
      return item->data.s16;
    }
    
    case TRA_DICT_TYPE_S8: {
      return item->data.s8;
    }
  }

  return def;
}

double tra_dict_get_real(tra_dict* ctx, const char* name, double def) {

  tra_dict* item = NULL;
  int r = 0;

  r = dict_find(ctx, name, &item);
  if (r < 0) {
    TRAE("Something went wrong while trying to find the item `%s`. ", name);
    return def;
  }

  if (NULL == item) {
    return def;
  }

  /* Return any of the real types. */
  switch (item->type) {
    
    case TRA_DICT_TYPE_FLT: {
      return item->data.f;
    }
    
    case TRA_DICT_TYPE_DBL: {
      return item->data.d;
    }
  }

  return def;
}

/* ------------------------------------------------------- */

int tra_dict_set_u64(tra_dict* ctx, const char* name, uint64_t val) {

  tra_dict* item = NULL;
  int r = 0;

  r = dict_object_property_create(ctx, TRA_DICT_TYPE_U64, name, &item);
  if (r < 0) {
    TRAE("Failed to create the `u64` property.");
    return -1;
  }

  item->data.u64 = val;

  return r;
}

int tra_dict_set_u32(tra_dict* ctx, const char* name, uint32_t val) {

  tra_dict* item = NULL;
  int r = 0;

  r = dict_object_property_create(ctx, TRA_DICT_TYPE_U32, name, &item);
  if (r < 0) {
    TRAE("Failed to create the `u32` property.");
    return -1;
  }

  item->data.u32 = val;

  return r;
}

int tra_dict_set_u16(tra_dict* ctx, const char* name, uint16_t val) {

  tra_dict* item = NULL;
  int r = 0;

  r = dict_object_property_create(ctx, TRA_DICT_TYPE_U16, name, &item);
  if (r < 0) {
    TRAE("Failed to create the `u16` property.");
    return -1;
  }

  item->data.u16 = val;

  return r;
}

int tra_dict_set_u8(tra_dict* ctx, const char* name, uint8_t val) {

  tra_dict* item = NULL;
  int r = 0;

  r = dict_object_property_create(ctx, TRA_DICT_TYPE_U8, name, &item);
  if (r < 0) {
    TRAE("Failed to create the `u8` property.");
    return -1;
  }

  item->data.u8 = val;

  return r;
}

/* ------------------------------------------------------- */

int tra_dict_set_s64(tra_dict* ctx, const char* name, int64_t val) {

  tra_dict* item = NULL;
  int r = 0;

  r = dict_object_property_create(ctx, TRA_DICT_TYPE_S64, name, &item);
  if (r < 0) {
    TRAE("Failed to create the `s64` property.");
    return -1;
  }

  item->data.s64 = val;

  return r;
}

int tra_dict_set_s32(tra_dict* ctx, const char* name, int32_t val) {

  tra_dict* item = NULL;
  int r = 0;

  r = dict_object_property_create(ctx, TRA_DICT_TYPE_S32, name, &item);
  if (r < 0) {
    TRAE("Failed to create the `s32` property.");
    return -1;
  }

  item->data.s32 = val;

  return r;
}

int tra_dict_set_s16(tra_dict* ctx, const char* name, int16_t val) {

  tra_dict* item = NULL;
  int r = 0;

  r = dict_object_property_create(ctx, TRA_DICT_TYPE_S16, name, &item);
  if (r < 0) {
    TRAE("Failed to create the `s16` property.");
    return -1;
  }

  item->data.s16 = val;

  return r;
}

int tra_dict_set_s8(tra_dict* ctx, const char* name, int8_t val) {

  tra_dict* item = NULL;
  int r = 0;

  r = dict_object_property_create(ctx, TRA_DICT_TYPE_S8, name, &item);
  if (r < 0) {
    TRAE("Failed to create the `s8` property.");
    return -1;
  }

  item->data.s8 = val;

  return r;
}

/* ------------------------------------------------------- */

int tra_dict_set_float(tra_dict* ctx, const char* name, float val) {

  tra_dict* item = NULL;
  int r = 0;

  r = dict_object_property_create(ctx, TRA_DICT_TYPE_FLT, name, &item);
  if (r < 0) {
    TRAE("Failed to create the `float` property.");
    return -1;
  }

  item->data.f = val;

  return r;
}

int tra_dict_set_double(tra_dict* ctx, const char* name, double val) {

  tra_dict* item = NULL;
  int r = 0;

  r = dict_object_property_create(ctx, TRA_DICT_TYPE_DBL, name, &item);
  if (r < 0) {
    TRAE("Failed to create the `double` property.");
    return -1;
  }

  item->data.d = val;

  return r;
}

/* ------------------------------------------------------- */

int tra_dict_set_string(tra_dict* ctx, const char* name, const char* val) {

  tra_dict* item = NULL;
  int r = 0;

  if (NULL == val) {
    TRAE("Cannot create a `string` property as the given value is NULL.");
    return -1;
  }

  if (0 == strlen(val)) {
    TRAE("Cannot create a `string` property as the given value is empty.");
    return -2;
  }

  r = dict_object_property_create(ctx, TRA_DICT_TYPE_STR, name, &item);
  if (r < 0) {
    TRAE("Cannot create a `string` property.");
    return -3;
  }

  item->data.str = strdup(val);
  
  if (NULL == item->data.str) {
    TRAE("Failed to copy the string value. Out of memory?");
    TRAE("@todo: at this point the item has been added but not deleted! We have to remove it from the object property list.");
    return -4;
  }

  return r;
}

/* ------------------------------------------------------- */

int tra_dict_set_array(tra_dict* ctx, const char* name, tra_dict* array) {

  int r = 0;

  if (NULL == ctx) {
    TRAE("Cannot set array on the object as the given object is NULL.");
    return -1;
  }

  if (NULL == name) {
    TRAE("Cannot set the array on the object as the given name is NULL.");
    return -2;
  }

  if (0 == strlen(name)) {
    TRAE("Cannot set the array on the object as the given name is empty.");
    return -3;
  }

  if (NULL == array) {
    TRAE("Cannot set the array on the object as the given array is NULL.");
    return -4;
  }

  if (TRA_DICT_TYPE_OBJECT != ctx->type) {
    TRAE("Cannot set the array on the object as the given object is not an object.");
    return -5;
  }

  if (TRA_DICT_TYPE_ARRAY != array->type) {
    TRAE("Cannot set the array on the object as the given array is not an array.");
    return -6;
  }

  if (NULL != array->name) {
    TRAE("Cannot set the array on the object as it has a name already; only properties have names.");
    return -7;
  }

  array->name = strdup(name);
  if (NULL == array->name) {
    TRAE("Cannot set the array on the object as we failed to copy the name for the array property.");
    return -8;
  }

  r = dict_append(ctx, array);
  if (r < 0) {
    TRAE("Cannot set the array on the object as we failed to append the array to the object.");
    r = -9;
    goto error;
  }

 error:

  if (r < 0) {
    /* Deallocate the name that we might have set. */
    if (NULL != array->name) {
      free(array->name);
      array->name = NULL;
    }
  }

  return r;
}

/* ------------------------------------------------------- */

int tra_dict_set_object(tra_dict* ctx, const char* name, tra_dict* val) {

  int r = 0;
  
  if (NULL == ctx) {
    TRAE("Cannot set an object for the given `tra_dict*` as the given object `tra_dict*` is NULL.");
    return -1;
  }

  if (NULL == val) {
    TRAE("Cannot set an object for the given `tra_dict*` as the given value `tra_dict*` is NULL.");
    return -2;
  }

  if (NULL == name) {
    TRAE("Cannot set the object on the object as the given name is NULL.");
    return -3;
  }

  if (0 == strlen(name)) {
    TRAE("Cannot set the object on the object as the given name is empty.");
    return -4;
  }

  if (TRA_DICT_TYPE_OBJECT != ctx->type) {
    TRAE("Cannot set the object as the given type is not an object.");
    return -5;
  }

  if (TRA_DICT_TYPE_OBJECT != val->type) {
    TRAE("Cannot set the object as the given value is not an object.");
    return -6;
  }

  if (NULL != val->name) {
    TRAE("Cannot add the given object as it alraedy has a name. Already added as an attribute to another object?");
    return -7;
  }

  val->name = strdup(name);
  if (NULL == val->name) {
    TRAE("Failed to set the name for the given object.");
    return -8;
  }

  r = dict_append(ctx, val);
  if (r < 0) {
    TRAE("Failed to append the given object to another object.");
    r = -9;
    goto error;
  }

 error:

  if (r < 0) {
    if (NULL != val->name) {
      free(val->name);
      val->name = NULL;
    }
  }

  return r;
}

/* ------------------------------------------------------- */

int tra_dict_array_add_u64(tra_dict* ctx, uint64_t val) {

  tra_dict* item = NULL;
  int r = 0;

  r = dict_array_property_create(ctx, TRA_DICT_TYPE_U64, &item);
  if (r < 0) {
    TRAE("Failed to create a `u64` property for an array.");
    return -1;
  }

  item->data.u64 = val;
  
  return r;
}

int tra_dict_array_add_u32(tra_dict* ctx, uint32_t val) {

  tra_dict* item = NULL;
  int r = 0;

  r = dict_array_property_create(ctx, TRA_DICT_TYPE_U32, &item);
  if (r < 0) {
    TRAE("Failed to create a `u32` property for an array.");
    return -1;
  }

  item->data.u32 = val;
  
  return r;
}

int tra_dict_array_add_u16(tra_dict* ctx, uint16_t val) {

  tra_dict* item = NULL;
  int r = 0;

  r = dict_array_property_create(ctx, TRA_DICT_TYPE_U16, &item);
  if (r < 0) {
    TRAE("Failed to create a `u16` property for an array.");
    return -1;
  }

  item->data.u16 = val;
  
  return r;
}

int tra_dict_array_add_u8(tra_dict* ctx, uint8_t val) {

  tra_dict* item = NULL;
  int r = 0;

  r = dict_array_property_create(ctx, TRA_DICT_TYPE_U8, &item);
  if (r < 0) {
    TRAE("Failed to create a `u8` property for an array.");
    return -1;
  }

  item->data.u8 = val;
  
  return r;
}

/* ------------------------------------------------------- */

int tra_dict_array_add_s64(tra_dict* ctx, int64_t val) {

  tra_dict* item = NULL;
  int r = 0;

  r = dict_array_property_create(ctx, TRA_DICT_TYPE_S64, &item);
  if (r < 0) {
    TRAE("Failed to create a `s64` property for an array.");
    return -1;
  }

  item->data.s64 = val;
  
  return r;
}

int tra_dict_array_add_s32(tra_dict* ctx, int32_t val) {

  tra_dict* item = NULL;
  int r = 0;

  r = dict_array_property_create(ctx, TRA_DICT_TYPE_S32, &item);
  if (r < 0) {
    TRAE("Failed to create a `s32` property for an array.");
    return -1;
  }

  item->data.s32 = val;
  
  return r;
}

int tra_dict_array_add_s16(tra_dict* ctx, int16_t val) {

  tra_dict* item = NULL;
  int r = 0;

  r = dict_array_property_create(ctx, TRA_DICT_TYPE_S16, &item);
  if (r < 0) {
    TRAE("Failed to create a `s16` property for an array.");
    return -1;
  }

  item->data.s16 = val;
  
  return r;
}

int tra_dict_array_add_s8(tra_dict* ctx, int8_t val) {

  tra_dict* item = NULL;
  int r = 0;

  r = dict_array_property_create(ctx, TRA_DICT_TYPE_S8, &item);
  if (r < 0) {
    TRAE("Failed to create a `s8` property for an array.");
    return -1;
  }

  item->data.s8 = val;
  
  return r;
}

/* ------------------------------------------------------- */

int tra_dict_array_add_string(tra_dict* ctx, const char* val) {

  tra_dict* item = NULL;
  int r = 0;

  if (NULL == val) {
    TRAE("Cannot create a `string` array item, as the given value is NULL.");
    return -1;
  }

  if (0 == strlen(val)) {
    TRAE("Cannot create a `string` array item as the given value is empty.");
    return -2;
  }

  r = dict_array_property_create(ctx, TRA_DICT_TYPE_STR, &item);
  if (r < 0) {
    TRAE("Failed to create a `string` property for an array.");
    return -1;
  }

  item->data.str = strdup(val);
  
  if (NULL == item->data.str) {
    TRAE("Failed to copy the string value for an array. Out of memory?");
    TRAE("@todo: at this point the item has been added but not deleted! We have to remove it from the array property list.");
    return -4;
  }

  return r;
}

/* ------------------------------------------------------- */

int tra_dict_array_add_object(tra_dict* ctx, tra_dict* val) {
  
  int r = 0;

  if (NULL == ctx) {
    TRAE("Cannot add the given object to the array as the given array is NULL.");
    return -1;
  }

  if (NULL == val) {
    TRAE("Cannot add the given object to the array as the given object is NULL.");
    return -2;
  }

  if (TRA_DICT_TYPE_ARRAY != ctx->type) {
    TRAE("Cannot add the given object to the array as the it's not an array.");
    return -3;
  }

  if (TRA_DICT_TYPE_OBJECT != val->type) {
    TRAE("Cannot add the given object value to the array as it's not an object.");
    return -4;
  }
    
  r = dict_append(ctx, val);
  if (r < 0) {
    TRAE("Failed to append the object to the object.");
    return -5;
  }

  return 0;
}

/* ------------------------------------------------------- */

int tra_dict_array_add_array(tra_dict* ctx, tra_dict* val) {
  
  int r = 0;

  if (NULL == ctx) {
    TRAE("Cannot add the given array to the array as the given array is NULL.");
    return -1;
  }

  if (NULL == val) {
    TRAE("Cannot add the given array to the array as the given object is NULL.");
    return -2;
  }

  if (TRA_DICT_TYPE_ARRAY != ctx->type) {
    TRAE("Cannot add the given array to the array as the it's not an array.");
    return -3;
  }

  if (TRA_DICT_TYPE_ARRAY != val->type) {
    TRAE("Cannot add the given array value to the array as it's not an array.");
    return -4;
  }
    
  r = dict_append(ctx, val);
  if (r < 0) {
    TRAE("Failed to append the array to the array.");
    return -5;
  }

  return 0;
}

/* ------------------------------------------------------- */

int tra_dict_to_json(tra_dict* ctx, tra_buffer* buf) {
  return dict_json_from_dict(ctx, buf, 0);
}

/* ------------------------------------------------------- */

/* 
   This function will destroy the given dict and all of it's
   "children" recursively; we destroy the leafs first. When an 
   error occurs we return < 0, otherwise 0.
 */
int tra_dict_destroy(tra_dict* ctx) {

  tra_dict* el = NULL;
  int r = 0;
  
  if (NULL == ctx) {
    TRAE("Cannot destroy the `tra_dict`: the given `tra_dict*` is NULL.");
    return -1;
  }

  /* Recurse into object and array types. */
  if (TRA_DICT_TYPE_OBJECT == ctx->type
      || TRA_DICT_TYPE_ARRAY == ctx->type)
    {

      el = ctx->data.values;
      
      while (NULL != el) {
        
        r = tra_dict_destroy(el);
        if (r < 0) {
          TRAE("Failed to cleanly destroy an member of a `%s` type.", dict_type_to_string(el->type));
          return -2;
        }
          
        el = el->next;
      }

      /* The item was deallocate by the recursive call. */
      ctx->data.values = NULL;
    }

  /* Deallocate the name. */
  if (NULL != ctx->name) {
    free(ctx->name);
    ctx->name = NULL;
  }

  /* Dealloate string type */
  if (TRA_DICT_TYPE_STR == ctx->type
      && NULL != ctx->data.str)
    {
      free(ctx->data.str);
      ctx->data.str = NULL;
    }
  
  memset(ctx, 0x00, sizeof(tra_dict));
  free(ctx);
  ctx = NULL;

  return 0;
}

/* ------------------------------------------------------- */

/*
  Returns < 0 when the given name wasn't found or an error
  occured (we will log the error). When found we return 0.
*/
int tra_dict_has_property(tra_dict* ctx, const char* name) {

  tra_dict* item = NULL;
  int r = 0;
  
  if (NULL == ctx) {
    TRAE("Cannot check if `tra_dict` has a property; given `tra_dict*` is NULL.");
    return -1;
  }

  if (NULL == name) {
    TRAE("Cannot check if `tra_dict` has a property; given property name is NULL.");
    return -2;
  }

  if (0 == strlen(name)) {
    TRAE("Cannot check if `tra_dict` has a property; given property name is empty.");
    return -3;
  }

  r = dict_find(ctx, name, &item);
  if (r < 0) {
    return r;
  }

  if (NULL == item) {
    return -4;
  }

  /* Yes, we found the item! */
  return 0;
}

/* ------------------------------------------------------- */

int tra_dict_print(tra_dict* ctx) {

  tra_buffer* buf = NULL;
  int r = 0;
  
  if (NULL == ctx) {
    TRAE("Cannot print the `tra_dict` as it's NULL.");
    return -1;
  }

  if (NULL == buf) {
    r = tra_buffer_create(1024, &buf);
    if (r < 0) {
      TRAE("Failed to create the `tra_buffer` that we use to print a `tra_dict`.");
      return -2;
    }
  }

  r = tra_dict_to_json(ctx, buf);
  if (r < 0) {
    TRAE("Failed to convert the given `tra_dict` into a JSON string.");
    r = -3;
    goto error;
  }

  TRAD("tra_dict: \n\n%*s\n", buf->size, buf->data);

 error:

  if (NULL != buf) {
    
    r = tra_buffer_destroy(buf);
    if (r < 0) {
      TRAE("Failed to cleanly destroy the buffer that was used to print the contents of the `tra_dict`. ");
    }
    
    /* Still fall through ... */
    buf = NULL;
  }
  
  return r;
}

/* ------------------------------------------------------- */

static int dict_json_from_dict(tra_dict* ctx, tra_buffer* buf, int depth) {

  int r = 0;
  tra_dict* el = NULL;
  
  if (NULL == ctx) {
    TRAE("Cannot convert the given `tra_dict*` to JSON as it's NULL.");
    return -1;
  }

  if (NULL == buf) {
    TRAE("Convert the given `tra_dict*` to JSON as the given destination buffer is NULL.");
    return -2;
  }

  switch (ctx->type) {

    case TRA_DICT_TYPE_S64: {
      r = dict_json_from_s64(ctx, buf, depth);
      if (r < 0) {
        TRAE("Failed to convert an `s64` type into JSON.");
        return r;
      }
      break;
    }

    case TRA_DICT_TYPE_S32: {
      r = dict_json_from_s32(ctx, buf, depth);
      if (r < 0) {
        TRAE("Failed to convert an `s32` type into JSON.");
        return r;
      }
      break;
    }

    case TRA_DICT_TYPE_S16: {
      r = dict_json_from_s16(ctx, buf, depth);
      if (r < 0) {
        TRAE("Failed to convert an `s16` type into JSON.");
        return r;
      }
      break;
    }
    case TRA_DICT_TYPE_S8: {
      r = dict_json_from_s8(ctx, buf, depth);
      if (r < 0) {
        TRAE("Failed to convert an `s8` type into JSON.");
        return r;
      }
      break;
    }
        
    case TRA_DICT_TYPE_U64: {
      r = dict_json_from_u64(ctx, buf, depth);
      if (r < 0) {
        TRAE("Failed to convert an `u64` type into JSON.");
        return r;
      }
      break;
    }

    case TRA_DICT_TYPE_U32: {
      r = dict_json_from_u32(ctx, buf, depth);
      if (r < 0) {
        TRAE("Failed to convert an `u32` type into JSON.");
        return r;
      }
      break;
    }

    case TRA_DICT_TYPE_U16: {
      r = dict_json_from_u16(ctx, buf, depth);
      if (r < 0) {
        TRAE("Failed to convert an `u16` type into JSON.");
        return r;
      }
      break;
    }
    
    case TRA_DICT_TYPE_U8: {
      r = dict_json_from_u8(ctx, buf, depth);
      if (r < 0) {
        TRAE("Failed to convert an `u8` type into JSON.");
        return r;
      }
      break;
    }

    case TRA_DICT_TYPE_FLT: {
      r = dict_json_from_float(ctx, buf, depth);
      if (r < 0) {
        TRAE("Failed to convert an `float` type into JSON.");
        return r;
      }
      break;
    }

    case TRA_DICT_TYPE_DBL: {
      r = dict_json_from_double(ctx, buf, depth);
      if (r < 0) {
        TRAE("Failed to convert an `double` type into JSON.");
        return r;
      }
      break;
    }
    
    case TRA_DICT_TYPE_OBJECT: {
      r = dict_json_from_object(ctx, buf, depth);
      if (r < 0) {
        TRAE("Failed to convert an `object` type into JSON.");
        return r;
      }
      break;
    }

    case TRA_DICT_TYPE_ARRAY: {
      r = dict_json_from_array(ctx, buf, depth);
      if (r < 0) {
        TRAE("Failed to convert an `array` type into JSON.");
        return r;
      }
      break;
    }

    case TRA_DICT_TYPE_STR: {
      r = dict_json_from_string(ctx, buf, depth);
      if (r < 0) {
        TRAE("Failed to convert `string` type into JSON.");
        return r;
      }
      break;
    }

    default: {
      TRAE("No serialization implemented for type `%s` yet.", dict_type_to_string(ctx->type));
      return -1;
    }
  }

  return 0;
}

/* ------------------------------------------------------- */

static int dict_json_from_u64(tra_dict* ctx, tra_buffer* buf, int depth) {

  int r = 0;

  r = dict_json_indent(buf, depth);
  if (r < 0) {
    TRAE("Failed to indent.");
    return -1;
  }

  /* Object property. */
  if (NULL != ctx->name) {
    
    r = tra_buffer_write(buf, "\"%s\": %llu", ctx->name, ctx->data.u64);
    if (r < 0) {
      TRAE("Failed to write a `%s` property.", dict_type_to_string(ctx->type));
      return -2;
    }

    return 0;
  }
  
  /* Single value (e.g. array item). */
  r = tra_buffer_write(buf, "%llu", ctx->data.u64);
  if (r < 0) {
    TRAE("Failed to write a `%s` value.", dict_type_to_string(ctx->type));
    return -3;
  }

  return 0;
}

static int dict_json_from_u32(tra_dict* ctx, tra_buffer* buf, int depth) {

  int r = 0;

  r = dict_json_indent(buf, depth);
  if (r < 0) {
    TRAE("Failed to indent.");
    return -1;
  }

  /* Object property. */
  if (NULL != ctx->name) {
    
    r = tra_buffer_write(buf, "\"%s\": %u", ctx->name, ctx->data.u32);
    if (r < 0) {
      TRAE("Failed to write a `%s` property.", dict_type_to_string(ctx->type));
      return -2;
    }

    return 0;
  }
  
  /* Single value (e.g. array item). */
  r = tra_buffer_write(buf, "%u", ctx->data.u32);
  if (r < 0) {
    TRAE("Failed to write a `%s` value.", dict_type_to_string(ctx->type));
    return -3;
  }

  return 0;
}

static int dict_json_from_u16(tra_dict* ctx, tra_buffer* buf, int depth) {

  int r = 0;

  r = dict_json_indent(buf, depth);
  if (r < 0) {
    TRAE("Failed to indent.");
    return -1;
  }
    
  /* Object property. */
  if (NULL != ctx->name) {
    
    r = tra_buffer_write(buf, "\"%s\": %u", ctx->name, ctx->data.u16);
    if (r < 0) {
      TRAE("Failed to write a `%s` property.", dict_type_to_string(ctx->type));
      return -2;
    }

    return 0;
  }
  
  /* Single value (e.g. array item). */
  r = tra_buffer_write(buf, "%u", ctx->data.u16);
  if (r < 0) {
    TRAE("Failed to write a `%s` value.", dict_type_to_string(ctx->type));
    return -3;
  }

  return 0;
}

static int dict_json_from_u8(tra_dict* ctx, tra_buffer* buf, int depth) {

  int r = 0;

  r = dict_json_indent(buf, depth);
  if (r < 0) {
    TRAE("Failed to indent.");
    return -1;
  }
    
  /* Object property. */
  if (NULL != ctx->name) {

    r = tra_buffer_write(buf, "\"%s\": %u", ctx->name, ctx->data.u8);
    if (r < 0) {
      TRAE("Failed to write a `%s` property.", dict_type_to_string(ctx->type));
      return -2;
    }

    return 0;
  }
  
  /* Single value (e.g. array item). */
  r = tra_buffer_write(buf, "%u", ctx->data.u8);
  if (r < 0) {
    TRAE("Failed to write a `%s` value.", dict_type_to_string(ctx->type));
    return -3;
  }

  return 0;
}

/* ------------------------------------------------------- */

static int dict_json_from_s64(tra_dict* ctx, tra_buffer* buf, int depth) {

  int r = 0;

  r = dict_json_indent(buf, depth);
  if (r < 0) {
    TRAE("Failed to indent.");
    return -1;
  }

  /* Object property. */
  if (NULL != ctx->name) {
    
    r = tra_buffer_write(buf, "\"%s\": %ld", ctx->name, ctx->data.s64);
    if (r < 0) {
      TRAE("Failed to write a `%s` property.", dict_type_to_string(ctx->type));
      return -2;
    }

    return 0;
  }
  
  /* Single value (e.g. array item). */
  r = tra_buffer_write(buf, "%ld", ctx->data.s64);
  if (r < 0) {
    TRAE("Failed to write a `%s` value.", dict_type_to_string(ctx->type));
    return -3;
  }

  return 0;
}

static int dict_json_from_s32(tra_dict* ctx, tra_buffer* buf, int depth) {

  int r = 0;

  r = dict_json_indent(buf, depth);
  if (r < 0) {
    TRAE("Failed to indent.");
    return -1;
  }

  /* Object property. */
  if (NULL != ctx->name) {
    
    r = tra_buffer_write(buf, "\"%s\": %d", ctx->name, ctx->data.s32);
    if (r < 0) {
      TRAE("Failed to write a `%s` property.", dict_type_to_string(ctx->type));
      return -2;
    }

    return 0;
  }
  
  /* Single value (e.g. array item). */
  r = tra_buffer_write(buf, "%*s%d", depth, "  ", ctx->data.s32);
  if (r < 0) {
    TRAE("Failed to write a `%s` value.", dict_type_to_string(ctx->type));
    return -3;
  }

  return 0;
}

static int dict_json_from_s16(tra_dict* ctx, tra_buffer* buf, int depth) {

  int r = 0;

  r = dict_json_indent(buf, depth);
  if (r < 0) {
    TRAE("Failed to indent.");
    return -1;
  }

  /* Object property. */
  if (NULL != ctx->name) {
    
    r = tra_buffer_write(buf, "\"%s\": %d", ctx->name, ctx->data.s16);
    if (r < 0) {
      TRAE("Failed to write a `%s` property.", dict_type_to_string(ctx->type));
      return -2;
    }

    return 0;
  }
  
  /* Single value (e.g. array item). */
  r = tra_buffer_write(buf, "%d", ctx->data.s16);
  if (r < 0) {
    TRAE("Failed to write a `%s` value.", dict_type_to_string(ctx->type));
    return -3;
  }

  return 0;
}

static int dict_json_from_s8(tra_dict* ctx, tra_buffer* buf, int depth) {

  int r = 0;

  r = dict_json_indent(buf, depth);
  if (r < 0) {
    TRAE("Failed to indent.");
    return -1;
  }

  /* Object property. */
  if (NULL != ctx->name) {
    
    r = tra_buffer_write(buf, "\"%s\": %d", ctx->name, ctx->data.s8);
    if (r < 0) {
      TRAE("Failed to write a `%s` property.", dict_type_to_string(ctx->type));
      return -2;
    }

    return 0;
  }
  
  /* Single value (e.g. array item). */
  r = tra_buffer_write(buf, "%d", ctx->data.s8);
  if (r < 0) {
    TRAE("Failed to write a `%s` value.", dict_type_to_string(ctx->type));
    return -3;
  }

  return 0;
}

/* ------------------------------------------------------- */

static int dict_json_from_float(tra_dict* ctx, tra_buffer* buf, int depth) {

  int r = 0;

  r = dict_json_indent(buf, depth);
  if (r < 0) {
    TRAE("Failed to indent.");
    return -1;
  }

  /* Object property. */
  if (NULL != ctx->name) {
    
    r = tra_buffer_write(buf, "\"%s\": %f", ctx->name, ctx->data.f);
    if (r < 0) {
      TRAE("Failed to write a `%s` property.", dict_type_to_string(ctx->type));
      return -2;
    }

    return 0;
  }
  
  /* Single value (e.g. array item). */
  r = tra_buffer_write(buf, "%f", ctx->data.f);
  if (r < 0) {
    TRAE("Failed to write a `%s` value.", dict_type_to_string(ctx->type));
    return -3;
  }

  return 0;
}

static int dict_json_from_double(tra_dict* ctx, tra_buffer* buf, int depth) {

  int r = 0;

  r = dict_json_indent(buf, depth);
  if (r < 0) {
    TRAE("Failed to indent.");
    return -1;
  }

  /* Object property. */
  if (NULL != ctx->name) {
    
    r = tra_buffer_write(buf, "\"%s\": %f", ctx->name, ctx->data.d);
    if (r < 0) {
      TRAE("Failed to write a `%s` property.", dict_type_to_string(ctx->type));
      return -2;
    }

    return 0;
  }
  
  /* Single value (e.g. array item). */
  r = tra_buffer_write(buf, "%*s%f", ctx->data.d);
  if (r < 0) {
    TRAE("Failed to write a `%s` value.", dict_type_to_string(ctx->type));
    return -3;
  }

  return 0;
}

/* ------------------------------------------------------- */

static int dict_json_from_string(tra_dict* ctx, tra_buffer* buf, int depth) {

  int r = 0;

  r = dict_json_indent(buf, depth);
  if (r < 0) {
    TRAE("Failed to indent.");
    return -1;
  }

  /* Object property. */
  if (NULL != ctx->name) {

    r = tra_buffer_write(buf, "\"%s\": \"%s\"", ctx->name, ctx->data.str);
    if (r < 0) {
      TRAE("Failed to write a `string` property.");
      return -2;
    }

    return 0;
  }

  /* Single value (e.g. array item). */
  r = tra_buffer_write(buf, "\"%s\"", ctx->data.str);
  if (r < 0) {
    TRAE("Failed to write a `string` value.");
    return -3;
  }

  return 0;
}

static int dict_json_from_object(tra_dict* ctx, tra_buffer* buf, int depth) {

  tra_dict* el = NULL;
  int r = 0;

  r = dict_json_indent(buf, depth);
  if (r < 0) {
    TRAE("Failed to indent the json.");
    return -1;
  }
    
  if (NULL != ctx->name) {

    r = tra_buffer_write(buf, "\"%s\": ", ctx->name);
    if (r < 0) {
      TRAE("Failed to write the object name.");
      return -2;
    }
  }

  r = tra_buffer_write(buf, "{\n");
  if (r < 0) {
    TRAE("Failed to write the object opening tag.");
    return -3;
  }

  el = ctx->data.values;
  
  while (NULL != el) {

    r = dict_json_from_dict(el, buf, depth + 1);
    if (r < 0) {
      TRAE("Failed to write an object property.");
      return -4;
    }
    
    el = el->next;

    if (NULL != el) {
      
      r = tra_buffer_write(buf, ",\n");
      if (r < 0) {
        TRAE("Failed to write the comma for the next property in the object.");
        return -5;
      }
    }
  }

  r = tra_buffer_write(buf, "\n");
  if (r < 0) {
    TRAE("Failed to add a newline before closing the object.");
    return -6;
  }

  r = dict_json_indent(buf, depth);
  if (r < 0) {
    TRAE("Failed to indent the json.");
    return -7;
  }
  
  r = tra_buffer_write(buf, "}");
  if (r < 0) {
    TRAE("Failed to write the buffer closing tag.");
    return -8;
  }

  return 0;
}

static int dict_json_from_array(tra_dict* ctx, tra_buffer* buf, int depth) {

  tra_dict* el = NULL;
  int r = 0;

  r = dict_json_indent(buf, depth);
  if (r < 0) {
    TRAE("Failed to indent the json.");
    return -1;
  }
  
  if (NULL != ctx->name) {
    r = tra_buffer_write(buf, "\"%s\": ", ctx->name);
    if (r < 0) {
      TRAE("Failed to write the array name.");
      return -2;
    }
  }
      
  r = tra_buffer_write(buf, "[\n");
  if (r < 0) {
    TRAE("Failed to write the array opening tag.");
    return -3;
  }

  el = ctx->data.values;
  
  while (NULL != el) {
        
    r = dict_json_from_dict(el, buf, depth + 1);
    if (r < 0) {
      TRAE("Failed to write an array property.");
      return -4;
    }
    
    el = el->next;
        
    if (NULL != el) {
      
      r = tra_buffer_write(buf, ",\n");
      if (r < 0) {
        TRAE("Failed to write the comma for the next property in the array.");
        return -5;
      }
    }
  }

  r = tra_buffer_write(buf, "\n");
  if (r < 0) {
    TRAE("Failed to write the endline before the array closing tag.");
    return -6;
  }

  r = dict_json_indent(buf, depth);
  if (r < 0) {
    TRAE("Failed to indent the json.");
    return -7;
  }
    
  r = tra_buffer_write(buf, "]");
  if (r < 0) {
    TRAE("Failed to write the buffer closing tag.");
    return -8;
  }

  return 0;
}

/* ------------------------------------------------------- */

static int dict_json_indent(tra_buffer* buf, int depth) {

  int i = 0;
  
  if (NULL == buf) {
    TRAE("Cannot add indentation; given `tra_buffer` is NULL.");
    return -1;
  }

  for (i = 0; i < depth; ++i) {
    tra_buffer_write(buf, "%s", "  ");
  }

  return 0;
}

/* ------------------------------------------------------- */

/* 
   This function will search for an item with the name given by
   `name`. When found, we set `result`. When an error occurs we
   return < 0, otherwise 0. The return value can be 0 and result
   can still be NULL when nog found.
*/
static int dict_find(tra_dict* ctx, const char* name, tra_dict** result) {

  tra_dict* el = NULL;
  int r = 0;
  
  if (NULL == ctx) {
    TRAE("Cannot find an item, as the given container `tra_dict*` is NULL.");
    return -1;
  }

  if (NULL == name) {
    TRAE("Cannot find an item, as the given `name` is NULL.");
    return -2;
  }

  if (0 == strlen(name)) {
    TRAE("Cannot find an item, as the given name is empty.");
    return -3;
  }

  if (NULL == result) {
    TRAE("Cannot find an item, as the given `tra_dict**` result is NULL.");
    return -4;
  }

  if (TRA_DICT_TYPE_OBJECT != ctx->type) {
    TRAE("Cannot find an item as the given `tra_dict*` is not an object; we can only find items inside objects.");
    return -5;
  }

  el = ctx->data.values;

  while (NULL != el) {

    r = strcmp(el->name, name);
    if (0 == r) {
      *result = el;
      return 0;
    }

    el = el->next;
  }

  return 0;
}

/* ------------------------------------------------------- */

/* 
   Append the given `item` to the internal `data.values` member
   of `ctx`. When an error occurs we return < 0; otherwise 0.
*/
static int dict_append(tra_dict* ctx, tra_dict* item) {

  tra_dict* el = NULL;
  
  if (NULL == ctx) {
    TRAE("Cannot append a `tra_dict*` as the given container is NULL.");
    return -1;
  }

  if (NULL == item) {
    TRAE("Cannot append a `tra_dict*` as it's NULL.");
    return -2;
  }

  if (TRA_DICT_TYPE_ARRAY != ctx->type
      && TRA_DICT_TYPE_OBJECT != ctx->type)
    {
      TRAE("Cannot append a `tra_dict*` as the given type is not an array or object.");
      return -3;
    }

  /* 
     Here, we handle the situation where an item was added
     already; this is the case which occurs most often. We add
     the item to the end of the list.
  */
  if (NULL != ctx->data.values) {
    
    el = ctx->data.values;
    
    while (NULL != el) {
      
      if (NULL == el->next) {
        break;
      }
      
      el = el->next;
    }
    
    el->next = item;
    
    return 0;
  }

  /* Add a new item: only done once for the fist item. */
  if (NULL == ctx->data.values) {
    ctx->data.values = item;
    return 0;
  }

  return 0;
}

/* ------------------------------------------------------- */

/* 
   Create and add a property to an object; performing the necessary
   validation. This function doesn't set the value; that's done
   by the higher level functions. It does however add the newly
   created property to the given `obj`. 
*/
static int dict_object_property_create(
  tra_dict* obj,
  uint8_t type,
  const char* name,
  tra_dict** result
)
{
  tra_dict* item = NULL;
  int r = 0;

  if (NULL == obj) {
    TRAE("Cannot set a `%s` value as the given `tra_dict*` is NULL.", dict_type_to_string(type));
    return -1;
  }

  if (TRA_DICT_TYPE_OBJECT != obj->type) {
    TRAE("Cannot set a `%s` value as the given `tra_dict*` is not an object.", dict_type_to_string(type));
    return -2;
  }

  if (NULL == result) {
    TRAE("Cannot set a `%s` as the given `tra_dict**` result is NULL.", dict_type_to_string(type));
    return -3;
  }

  if (NULL != (*result)) {
    TRAE("Cannot set a `%s` as the given `*(tra_dict**)` is not NULL. Make sure to initialize your result `tra_dict` to NULL.", dict_type_to_string(type));
    return -4;
  }

  r = dict_find(obj, name, &item);
  if (r < 0) {
    TRAE("Cannot set a `%s` value as an error occured while checking for an existing item.", dict_type_to_string(type));
    return -5;
  }

  /* @todo currently we don't support duplicate entries or updating existing values. */
  if (NULL != item) {
    TRAE("Cannot set a `%s` value for `%s` as the key already exists.", dict_type_to_string(type), name);
    return -6;
  }
  
  r = dict_create(type, &item);
  if (r < 0) {
    TRAE("Cannot set a `%s` value as we failed to create a new item.", dict_type_to_string(type));
    return -7;
  }

  /* Set the name (we allocate, `tra_dict` owns the memory. */
  if (NULL != name
      && strlen(name) > 0)
    {
      item->name = strdup(name);
      if (NULL == item->name) {
        TRAE("Cannot set a `%s` value, we failed to set the name of the newly created item.", dict_type_to_string(type));
        r = -8;
        goto error;
      }
    }

  /* Append the item; note: we don't set the value; that's the responsibility of the caller. */
  r = dict_append(obj, item);
  if (r < 0) {
    TRAE("Failed to append the `%s` item.", dict_type_to_string(type));
    r = -9;
    goto error;
  }

  /* Finally, assign the value to the result. */
  *result = item;

 error:

  if (r < 0) {
    
    r = tra_dict_destroy(item);
    if (r < 0) {
      TRAE("After failing to create a `%s` item, we also failed to cleanly destroy it.", dict_type_to_string(type));
    }
    
    item = NULL;
  }

  return r;
}

/* ------------------------------------------------------- */

/* 
   Create and add a property to an array. We do not set the
   value; that's the responsibliity of the caller.
*/
static int dict_array_property_create(
  tra_dict* array,
  uint8_t type,
  tra_dict** result
)
{
  tra_dict* item = NULL;
  int r = 0;
  
  if (NULL == array) {
    TRAE("Cannot create a `%s` for the array as the given `tra_dict*` is NULL.", dict_type_to_string(type));
    return -1;
  }

  if (TRA_DICT_TYPE_ARRAY != array->type) {
    TRAE("Cannot create a `%s` to the given `tra_dict*` as it's not an array.", dict_type_to_string(type));
    return -2;
  }

  if (NULL == result) {
    TRAE("Cannot set a `%s` as the given `tra_dict**` result is NULL.", dict_type_to_string(type));
    return -3;
  }

  if (NULL != (*result)) {
    TRAE("Cannot set a `%s` as the given `*(tra_dict**)` is not NULL. Make sure to initialize your result `tra_dict` to NULL.", dict_type_to_string(type));
    return -4;
  }

  r = dict_create(type, &item);
  if (r < 0) {
    TRAE("Cannot create a `%s` for to the given array as we failed to create a new `tra_dict` item.", dict_type_to_string(type));
    return -5;
  }

  r = dict_append(array, item);
  if (r < 0) {
    TRAE("Failed to append the `%s` to the array.", dict_type_to_string(type));
    r = -6;
    goto error;
  }

  /* Finally assign the value. */
  *result = item;

 error:

  if (r < 0) {
    
    if (NULL != item) {
      
      r = tra_dict_destroy(item);
      if (r < 0) {
        TRAE("After failling to add a `%s` to the array, we failed to cleanly destroy it.", dict_type_to_string(type));
      }
    }
    
    item = NULL;
  }

  return r;
}

/* ------------------------------------------------------- */

static const char* dict_type_to_string(uint8_t type) {

  switch (type)  {
    case TRA_DICT_TYPE_NONE:   { return "TRA_DICT_TYPE_NONE";   } 
    case TRA_DICT_TYPE_U8:     { return "TRA_DICT_TYPE_U8";     } 
    case TRA_DICT_TYPE_U16:    { return "TRA_DICT_TYPE_U16";    } 
    case TRA_DICT_TYPE_U32:    { return "TRA_DICT_TYPE_U32";    } 
    case TRA_DICT_TYPE_U64:    { return "TRA_DICT_TYPE_U64";    } 
    case TRA_DICT_TYPE_S8:     { return "TRA_DICT_TYPE_S8";     } 
    case TRA_DICT_TYPE_S16:    { return "TRA_DICT_TYPE_S16";    } 
    case TRA_DICT_TYPE_S32:    { return "TRA_DICT_TYPE_S32";    } 
    case TRA_DICT_TYPE_S64:    { return "TRA_DICT_TYPE_S64";    } 
    case TRA_DICT_TYPE_FLT:    { return "TRA_DICT_TYPE_FLT";    } 
    case TRA_DICT_TYPE_DBL:    { return "TRA_DICT_TYPE_DBL";    } 
    case TRA_DICT_TYPE_STR:    { return "TRA_DICT_TYPE_STR";    } 
    case TRA_DICT_TYPE_ARRAY:  { return "TRA_DICT_TYPE_ARRAY";  } 
    case TRA_DICT_TYPE_OBJECT: { return "TRA_DICT_TYPE_OBJECT"; }
    default:                  { return "UNKNOWN";             }
  };
}

/* ------------------------------------------------------- */

