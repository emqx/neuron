/**
 * NEURON IIoT System for Industry 4.0
 * Copyright (C) 2020-2021 EMQ Technologies Co., Ltd All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 **/

#ifndef NEURON_DATA_EXPR_H
#define NEURON_DATA_EXPR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <assert.h>

#include "neu_vector.h"
#include "types.h"

/**
 * define data type of expression
 * ---------
 * A list of data type, ADT type
 **/
typedef enum {
    NEU_DTYPE_VOID, ///< like Haskell, a no value type
    NEU_DTYPE_UNIT, ///< like Haskell, a () type, same as void in C

    /* primary value types */
    NEU_VALUE_TYPE_START,
    NEU_DTYPE_BYTE = NEU_VALUE_TYPE_START,
    NEU_DTYPE_INT8,
    NEU_DTYPE_INT16,
    NEU_DTYPE_INT32,
    NEU_DTYPE_INT64,
    NEU_DTYPE_UINT8,
    NEU_DTYPE_UINT16,
    NEU_DTYPE_UINT32,
    NEU_DTYPE_UINT64,
    NEU_DTYPE_FLOAT,
    NEU_DTYPE_DOUBLE,
    NEU_DTYPE_BOOL,
    NEU_DTYPE_BIT,

    /* buffer valude with pointer */
    NEU_DTYPE_CSTR,   ///< C string with '\0', it's not good, we not recommended
    NEU_DTYPE_STRING, ///< string with length, we recommended use it to replace
                      ///< cstr
    NEU_DTYPE_BYTES,  ///< bytes buffer with length
    NEU_DTYPE_TEXT,   ///< localized text
    NEU_DTYPE_STRUCT, ///< valued structure, no pointer in field of structure

    /**
     * A int-value pair for IntMap
     */
    NEU_DTYPE_INT_VAL,

    /**
     * A string-value pair for StringMap
     */
    NEU_DTYPE_STRING_VAL,

    /* special value types */
    NEU_DTYPE_ERRORCODE,
    NEU_DTYPE_DATETIME,
    NEU_DTYPE_UUID,
    NEU_VALUE_TYPE_END,

    /* recursive type definition */
    NEU_RECURSIVE_TYPE_START = 1 << 8,
    /*
     * the val_data pointer to a neu_data_val_t
     */
    NEU_DTYPE_DATA_VAL = NEU_RECURSIVE_TYPE_START,
    NEU_RECURSIVE_TYPE_END,

    /* customer data value type */
    NEU_DTYPE_CUSTOM_START = 15 << 8,
    NEU_DTYPE_CUSTOM_END,

    /* bit flags definiton */
    NEU_DTYPE_FLAGS_START = 1 << 16,

    /* bit flags for constructor types */
    /*
     * Fixed Array layout:
     * { length, esize, elem1, elem2, elem3... }
     * a example:
     * { 4, 4, 3.48, 5.23, 2.72, 8.34 }
     */
    NEU_DTYPE_ARRAY = 1 << 16, ///< a fixed length array in val_data
    NEU_DTYPE_VEC   = 1 << 17, ///< a @vector_t@ in val_data
    NEU_DTYPE_PTR   = 1 << 18, ///< normal pointer
    NEU_DTYPE_SPTR  = 1 << 19, ///< smart pointer, like c++

    NEU_DTYPE_PTR_MASK = NEU_DTYPE_PTR | NEU_DTYPE_SPTR,

    /* bit flags for attribute of types */
    NEU_DTYPE_ATTR_MASK = 3 << 29,

    NEU_DTYPE_INPLACE_PTR = 0 << 29, ///< the pointer points to internal buffer
                                     ///< in neu_data_val_t
    NEU_DTYPE_EXTERN_PTR  = 1 << 29, ///< the pointer points to external buffer
    NEU_DTYPE_OWNERED_PTR = 1 << 30, ///< the pointer owner the resource,
                                     ///< it need free resource
} neu_dtype_e;

static_assert(NEU_VALUE_TYPE_END < NEU_RECURSIVE_TYPE_START,
              "Too many base value types");
static_assert(NEU_DTYPE_CUSTOM_END < NEU_DTYPE_FLAGS_START,
              "Too many custom types");

/**
 * A generic neuron data value
 */
typedef struct neu_data_val neu_data_val_t;

/* NEU_DTYPE_VOID */
typedef struct neu_void neu_void_t;

/* NEU_DTYPE_UNIT */
typedef struct neu_unit {
} neu_unit_t;

/* NEU_DTYPE_INT_VAL */
typedef struct neu_int_val {
    neu_data_val_t *val;
    uint32_t        key;
} neu_int_val_t;

/* NEU_DTYPE_STRING_VAL */
typedef struct neu_string_val {
    neu_string_t *  key;
    neu_data_val_t *val;
} neu_string_val_t;

static inline void neu_int_val_move(neu_int_val_t *dst, neu_int_val_t *src)
{
    dst->key = src->key;
    // the data value move to destination
    dst->val = src->val;
    src->val = NULL;
    return;
}

static inline void neu_string_val_move(neu_string_val_t *dst,
                                       neu_string_val_t *src)
{
    // the string key move to destination
    dst->key = src->key;
    src->key = NULL;
    // the data value move to destination
    dst->val = src->val;
    src->val = NULL;
    return;
}

static inline neu_dtype_e neu_value_type_in_dtype(neu_dtype_e type)
{
    neu_dtype_e ret_type;
    ret_type = (neu_dtype_e)(type & (NEU_DTYPE_FLAGS_START - 1));
    return ret_type;
}

static inline neu_dtype_e neu_flags_type_in_dtype(neu_dtype_e type)
{
    neu_dtype_e ret_type;
    ret_type = (neu_dtype_e)(type & ~(NEU_DTYPE_FLAGS_START - 1));
    return ret_type;
}

neu_dtype_e neu_dvalue_get_type(neu_data_val_t *val);
neu_dtype_e neu_dvalue_get_value_type(neu_data_val_t *val);
const char *neu_dvalue_type_to_str(neu_dtype_e type);

/*
 * declare all basic function for neu_data_val_t
 */
/**
 * New a normal data value
 */
neu_data_val_t *neu_dvalue_new(neu_dtype_e type);

/**
 * New a void data value, because the void data value hasn't a value, so this
 * function should be panic. Call this function when an exception occurred.
 */
neu_data_val_t *neu_dvalue_void_new();

/**
 * New a unit data value, the value is empty, it will translate to other type
 * data value.
 */
neu_data_val_t *neu_dvalue_unit_new();

/**
 * new a neu_data_val_t with inplace buffer
 */
neu_data_val_t *neu_dvalue_inplace_new(neu_dtype_e type, size_t buf_size);

void  neu_dvalue_uninit(neu_data_val_t *val);
void  neu_dvalue_free(neu_data_val_t *val);
void *neu_dvalue_get_ptr(neu_data_val_t *val);

/*
 * declare all value initialize functions
 */
void neu_dvalue_init_bit(neu_data_val_t *val, uint8_t bit);
void neu_dvalue_init_bool(neu_data_val_t *val, bool b);
void neu_dvalue_init_int8(neu_data_val_t *val, int8_t i8);
void neu_dvalue_init_int16(neu_data_val_t *val, int16_t i16);
void neu_dvalue_init_int32(neu_data_val_t *val, int32_t i32);
void neu_dvalue_init_int64(neu_data_val_t *val, int64_t i64);
void neu_dvalue_init_uint8(neu_data_val_t *val, uint8_t u8);
void neu_dvalue_init_uint16(neu_data_val_t *val, uint16_t u16);
void neu_dvalue_init_uint32(neu_data_val_t *val, uint32_t u32);
void neu_dvalue_init_uint64(neu_data_val_t *val, uint64_t u64);
void neu_dvalue_init_float(neu_data_val_t *val, float f32);
void neu_dvalue_init_double(neu_data_val_t *val, double f64);
void neu_dvalue_init_errorcode(neu_data_val_t *val, int32_t i32);
void neu_dvalue_init_int_val(neu_data_val_t *val, neu_int_val_t int_val);
void neu_dvalue_init_string_val(neu_data_val_t * val,
                                neu_string_val_t string_val);

/* The parameter cstr/bytes/array/vec is copy to neu_data_val_t
 */
void neu_dvalue_init_cstr(neu_data_val_t *val, char *cstr);
void neu_dvalue_init_string(neu_data_val_t *val, neu_string_t *string);
void neu_dvalue_init_bytes(neu_data_val_t *val, neu_bytes_t *bytes);

/* The neu_data_val_t reference the parameter cstr/bytes/array/vec
 */
void neu_dvalue_init_ref_cstr(neu_data_val_t *val, char *cstr);
void neu_dvalue_init_ref_string(neu_data_val_t *val, neu_string_t *string);
void neu_dvalue_init_ref_bytes(neu_data_val_t *val, neu_bytes_t *bytes);
// The parameter type is value type that get by neu_value_type_in_dtype()
void neu_dvalue_init_ref_array(neu_data_val_t *val, neu_dtype_e type,
                               neu_fixed_array_t *array);
// The parameter type is value type that get by neu_value_type_in_dtype()
void neu_dvalue_init_ref_vec(neu_data_val_t *val, neu_dtype_e type,
                             vector_t *vec);

/* The owership of parameter cstr/bytes/array/vec is move to neu_data_val_t
 */
void neu_dvalue_init_move_cstr(neu_data_val_t *val, char *cstr);
void neu_dvalue_init_move_string(neu_data_val_t *val, neu_string_t *string);
void neu_dvalue_init_move_bytes(neu_data_val_t *val, neu_bytes_t *bytes);
void neu_dvalue_init_move_data_val(neu_data_val_t *val, neu_data_val_t *data);
// The parameter type is value type that get by neu_value_type_in_dtype()
void neu_dvalue_init_move_array(neu_data_val_t *val, neu_dtype_e type,
                                neu_fixed_array_t *array);
// The parameter type is value type that get by neu_value_type_in_dtype()
void neu_dvalue_init_move_vec(neu_data_val_t *val, neu_dtype_e type,
                              vector_t *vec);

/*
 * declare all value set functions
 */
int neu_dvalue_set_bit(neu_data_val_t *val, uint8_t bit);
int neu_dvalue_set_bool(neu_data_val_t *val, bool b);
int neu_dvalue_set_int8(neu_data_val_t *val, int8_t i8);
int neu_dvalue_set_int16(neu_data_val_t *val, int16_t i16);
int neu_dvalue_set_int32(neu_data_val_t *val, int32_t i32);
int neu_dvalue_set_int64(neu_data_val_t *val, int64_t i64);
int neu_dvalue_set_uint8(neu_data_val_t *val, uint8_t u8);
int neu_dvalue_set_uint16(neu_data_val_t *val, uint16_t u16);
int neu_dvalue_set_uint32(neu_data_val_t *val, uint32_t u32);
int neu_dvalue_set_uint64(neu_data_val_t *val, uint64_t u64);
int neu_dvalue_set_float(neu_data_val_t *val, float f32);
int neu_dvalue_set_double(neu_data_val_t *val, double f64);
int neu_dvalue_set_errorcode(neu_data_val_t *val, int32_t i32);
int neu_dvalue_set_int_val(neu_data_val_t *val, neu_int_val_t int_val);
int neu_dvalue_set_string_val(neu_data_val_t *val, neu_string_val_t string_val);

/* The parameter cstr/bytes/array/vec is copy to neu_data_val_t
 */
int neu_dvalue_set_cstr(neu_data_val_t *val, char *cstr);
int neu_dvalue_set_string(neu_data_val_t *val, neu_string_t *string);
int neu_dvalue_set_bytes(neu_data_val_t *val, neu_bytes_t *bytes);

/* The neu_data_val_t reference the parameter cstr/bytes/array/vec
 * So the lifetime of neu_data_val_t must less than parameter.
 */
int neu_dvalue_set_ref_cstr(neu_data_val_t *val, char *cstr);
int neu_dvalue_set_ref_string(neu_data_val_t *val, neu_string_t *string);
int neu_dvalue_set_ref_bytes(neu_data_val_t *val, neu_bytes_t *bytes);
// The parameter type is value type that get by neu_value_type_in_dtype()
int neu_dvalue_set_ref_array(neu_data_val_t *val, neu_dtype_e type,
                             neu_fixed_array_t *array);
// The parameter type is value type that get by neu_value_type_in_dtype()
int neu_dvalue_set_ref_vec(neu_data_val_t *val, neu_dtype_e type,
                           vector_t *vec);

/* The owership of parameter cstr/bytes/array/vec is move to neu_data_val_t
 */
int neu_dvalue_set_move_cstr(neu_data_val_t *val, char *cstr);
int neu_dvalue_set_move_string(neu_data_val_t *val, neu_string_t *string);
int neu_dvalue_set_move_bytes(neu_data_val_t *val, neu_bytes_t *bytes);
int neu_dvalue_set_move_data_val(neu_data_val_t *val, neu_data_val_t *data);
// The parameter type is value type that get by neu_value_type_in_dtype()
int neu_dvalue_set_move_array(neu_data_val_t *val, neu_dtype_e type,
                              neu_fixed_array_t *array);
// The parameter type is value type that get by neu_value_type_in_dtype()
int neu_dvalue_set_move_vec(neu_data_val_t *val, neu_dtype_e type,
                            vector_t *vec);

/*
 * declare all value get functions
 */
int neu_dvalue_get_bit(neu_data_val_t *val, uint8_t *p_b);
int neu_dvalue_get_bool(neu_data_val_t *val, bool *p_b);
int neu_dvalue_get_int8(neu_data_val_t *val, int8_t *p_i8);
int neu_dvalue_get_int16(neu_data_val_t *val, int16_t *p_i16);
int neu_dvalue_get_int32(neu_data_val_t *val, int32_t *p_i32);
int neu_dvalue_get_int64(neu_data_val_t *val, int64_t *p_i64);
int neu_dvalue_get_uint8(neu_data_val_t *val, uint8_t *p_u8);
int neu_dvalue_get_uint16(neu_data_val_t *val, uint16_t *p_u16);
int neu_dvalue_get_uint32(neu_data_val_t *val, uint32_t *p_u32);
int neu_dvalue_get_uint64(neu_data_val_t *val, uint64_t *p_u64);
int neu_dvalue_get_float(neu_data_val_t *val, float *p_f32);
int neu_dvalue_get_double(neu_data_val_t *val, double *p_f64);
int neu_dvalue_get_errorcode(neu_data_val_t *val, int32_t *p_i32);
int neu_dvalue_get_int_val(neu_data_val_t *val, neu_int_val_t *p_int_val);
int neu_dvalue_get_string_val(neu_data_val_t *  val,
                              neu_string_val_t *p_string_val);
int neu_dvalue_get_data_val(neu_data_val_t *val, void **p_data);

/* The parameter cstr/bytes/array/vec is copy from neu_data_val_t
 */
int neu_dvalue_get_cstr(neu_data_val_t *val, char **p_cstr);
int neu_dvalue_get_string(neu_data_val_t *val, neu_string_t **p_string);
int neu_dvalue_get_bytes(neu_data_val_t *val, neu_bytes_t **p_bytes);

/* The parameter cstr/bytes/array/vec get reference of value in neu_data_val_t.
 * So the lifetime of parameter must less than neu_data_val_t.
 */
int neu_dvalue_get_ref_cstr(neu_data_val_t *val, char **p_cstr);
int neu_dvalue_get_ref_string(neu_data_val_t *val, neu_string_t **p_string);
int neu_dvalue_get_ref_bytes(neu_data_val_t *val, neu_bytes_t **p_bytes);
// The parameter type is value type that get by neu_value_type_in_dtype()
int neu_dvalue_get_ref_array(neu_data_val_t *val, neu_fixed_array_t **p_array);
// The parameter type is value type that get by neu_value_type_in_dtype()
int neu_dvalue_get_ref_vec(neu_data_val_t *val, vector_t **p_vec);

/* The owership of parameter cstr/bytes/array/vec is move from neu_data_val_t
 */
int neu_dvalue_get_move_cstr(neu_data_val_t *val, char **p_cstr);
int neu_dvalue_get_move_string(neu_data_val_t *val, neu_string_t **p_string);
int neu_dvalue_get_move_bytes(neu_data_val_t *val, neu_bytes_t **p_bytes);
int neu_dvalue_get_move_data_val(neu_data_val_t *val, neu_data_val_t **p_data);
// The parameter type is value type that get by neu_value_type_in_dtype()
int neu_dvalue_get_move_array(neu_data_val_t *val, neu_fixed_array_t **p_array);
// The parameter type is value type that get by neu_value_type_in_dtype()
int neu_dvalue_get_move_vec(neu_data_val_t *val, vector_t **p_vec);

/*
 * declare functions for serialize and deserialize
 */
ssize_t neu_dvalue_serialize(neu_data_val_t *val, uint8_t **p_buf);
ssize_t neu_dvalue_deserialize(uint8_t *buf, size_t buf_len,
                               neu_data_val_t **p_val);

/**
 * The input data value will owner all cstr/bytes/string/array/vec and sub data
 * value in array/vec for this data value.
 * After call this function, the user can share this data value to other
 * adapter or thread.
 * Note: If the function return NULL, it is must free the input data value,
 * and don't use the input data value in future.
 */
neu_data_val_t *neu_dvalue_to_owned(neu_data_val_t *val);

/*
 * declare functions for int-value and string-value pair
 */
/* The ownership of val is move into int_val, so don't free val after call
 * neu_int_val_init function
 */
static inline void neu_int_val_init(neu_int_val_t *int_val, uint32_t key,
                                    neu_data_val_t *val)
{
    int_val->key = key;
    int_val->val = val;
    return;
}

/* The ownership of val is move into int_val, so don't free val after call
 * neu_int_val_new function
 */
static inline neu_int_val_t *neu_int_val_new(uint32_t key, neu_data_val_t *val)
{
    neu_int_val_t *int_val;

    int_val = (neu_int_val_t *) malloc(sizeof(neu_int_val_t));
    if (int_val == NULL) {
        return NULL;
    }

    neu_int_val_init(int_val, key, val);
    return int_val;
}

static inline void neu_int_val_uninit(neu_int_val_t *int_val)
{
    if (int_val->val != NULL) {
        neu_dvalue_free(int_val->val);
    }
    return;
}

static inline void neu_int_val_free(neu_int_val_t *int_val)
{
    neu_int_val_uninit(int_val);
    free(int_val);
    return;
}

/* The ownership of val is move into string_val, so don't free val after call
 * neu_string_val_init function
 */
static inline void neu_string_val_init(neu_string_val_t *string_val,
                                       neu_string_t *key, neu_data_val_t *val)
{
    string_val->key = key;
    string_val->val = val;
    return;
}

/* The ownership of val is move into string_val, so don't free val after call
 * neu_string_val_new function
 */
static inline neu_string_val_t *neu_string_val_new(neu_string_t *  key,
                                                   neu_data_val_t *val)
{
    neu_string_val_t *string_val;

    string_val = (neu_string_val_t *) malloc(sizeof(neu_string_val_t));
    if (string_val == NULL) {
        return NULL;
    }

    neu_string_val_init(string_val, key, val);
    return string_val;
}

static inline void neu_string_val_uninit(neu_string_val_t *string_val)
{
    if (string_val->key != NULL) {
        neu_string_free(string_val->key);
    }
    if (string_val->val != NULL) {
        neu_dvalue_free(string_val->val);
    }
    return;
}

static inline void neu_string_val_free(neu_string_val_t *string_val)
{
    neu_string_val_uninit(string_val);
    free(string_val);
    return;
}

#ifdef __cplusplus
}
#endif

#endif
