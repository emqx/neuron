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

#include "neu_types.h"
#include "neu_vector.h"

/**
 * define data type of expression
 * ---------
 * A list of data type, ADT type
 **/
typedef enum {
    NEU_DTYPE_VOID, ///< like Haskell, a no value type
    NEU_DTYPE_UNIT, ///< like Haskell, a () type, same as void in C

    /* value types */
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
    NEU_DTYPE_CSTR,   ///< C string with '\0'
    NEU_DTYPE_STRING, ///< string with length
    NEU_DTYPE_BYTES,  ///< bytes buffer with length
    NEU_DTYPE_TEXT,   ///< localized text
    NEU_DTYPE_STRUCT, ///< valued structure, no pointer in field of structure
    NEU_DTYPE_ERRORCODE,
    NEU_DTYPE_DATETIME,
    NEU_DTYPE_UUID,
    NEU_DTYPE_ADDRESS,
    NEU_DTYPE_STATE,
    NEU_DTYPE_TAG,
    NEU_DTYPE_TAG_ID,
    NEU_DTYPE_PARAMETER,
    NEU_DTYPE_MODULE,
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
    NEU_DTYPE_FLAGS_START = 1 << 15,

    /* bit flags for constructor types */
    /*
     * Fixed Array layout:
     * { length, esize, elem1, elem2, elem3... }
     * a example:
     * { 4, 4, 3.48, 5.23, 2.72, 8.34 }
     */
    NEU_DTYPE_ARRAY = 1 << 15, ///< a fixed length array in val_data
    NEU_DTYPE_VEC   = 1 << 16, ///< a @vector_t@ in val_data
    NEU_DTYPE_PTR   = 1 << 17, ///< normal pointer
    NEU_DTYPE_SPTR  = 1 << 18, ///< smart pointer, like c++

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

/* NEU_DTYPE_VOID */
typedef struct neu_void neu_void_t;

/* NEU_DTYPE_UNIT */
typedef struct neu_dtype_unit {
} neu_dtype_unit_t;

neu_dtype_e neu_value_type_in_dtype(neu_dtype_e type);

/**
 * A generic neuron data value
 */
typedef struct neu_data_val neu_data_val_t;

/*
 * basic function for neu_data_val_t
 */
neu_data_val_t *neu_dvalue_new(neu_dtype_e type);

/* new a neu_data_val_t with inplace buffer */
neu_data_val_t *neu_dvalue_inplace_new(neu_dtype_e type, size_t buf_size);

/* new a neu_data_val_t include array with external buffer
 * parameter type is type of element in array
 */
neu_data_val_t *neu_dvalue_array_new(neu_dtype_e type, size_t length,
                                     size_t esize);

/* new a neu_data_val_t include vector with external buffer
 * parameter type is type of element in vector
 */
neu_data_val_t *neu_dvalue_vec_new(neu_dtype_e type, size_t length,
                                   size_t esize);

void  neu_dvalue_uninit(neu_data_val_t *val);
void  neu_dvalue_free(neu_data_val_t *val);
void *neu_dvalue_get_ptr(neu_data_val_t *val);

/*
 * declare all value initialize functions
 */
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
void neu_dvalue_init_clone_cstr(neu_data_val_t *val, char *cstr);
void neu_dvalue_init_clone_bytes(neu_data_val_t *val, neu_bytes_t *bytes);
void neu_dvalue_init_clone_array(neu_data_val_t *val, neu_fixed_array_t *array);
void neu_dvalue_init_clone_vec(neu_data_val_t *val, vector_t *vec);
void neu_dvalue_init_ref_cstr(neu_data_val_t *val, char *cstr);
void neu_dvalue_init_ref_bytes(neu_data_val_t *val, neu_bytes_t *bytes);
void neu_dvalue_init_ref_array(neu_data_val_t *val, neu_dtype_e type,
                               neu_fixed_array_t *array);
void neu_dvalue_init_ref_vec(neu_data_val_t *val, neu_dtype_e type,
                             vector_t *vec);

/* The owership of parameter array is move to neu_data_val_t
 */
void neu_dvalue_init_move_array(neu_data_val_t *val, neu_dtype_e type,
                                neu_fixed_array_t *array);
/* The owership of parameter vector is move to neu_data_val_t
 */
void neu_dvalue_init_move_vec(neu_data_val_t *val, neu_dtype_e type,
                              vector_t *vec);

/*
 * declare all value set functions
 */
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
int neu_dvalue_set_cstr(neu_data_val_t *val, char *cstr);
int neu_dvalue_set_bytes(neu_data_val_t *val, neu_bytes_t *bytes);
int neu_dvalue_set_array(neu_data_val_t *val, neu_fixed_array_t *array);
int neu_dvalue_set_vec(neu_data_val_t *val, vector_t *vec);
int neu_dvalue_set_ref_cstr(neu_data_val_t *val, char *cstr);
int neu_dvalue_set_ref_bytes(neu_data_val_t *val, neu_bytes_t *bytes);
int neu_dvalue_set_ref_array(neu_data_val_t *val, neu_fixed_array_t *array);
int neu_dvalue_set_ref_vec(neu_data_val_t *val, vector_t *vec);
int neu_dvalue_set_sptr_cstr(neu_data_val_t *val, char *cstr);
int neu_dvalue_set_sptr_bytes(neu_data_val_t *val, neu_bytes_t *bytes);

/*
 * declare all value get functions
 */
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
int neu_dvalue_get_cstr(neu_data_val_t *val, char **p_cstr);
int neu_dvalue_get_bytes(neu_data_val_t *val, neu_bytes_t **p_bytes);
int neu_dvalue_get_array(neu_data_val_t *val, neu_fixed_array_t **p_array);
int neu_dvalue_get_vec(neu_data_val_t *val, vector_t **p_vec);

/*
 * declare functions for serialize and deserialize
 */
int neu_dvalue_serialize(neu_data_val_t *val, uint8_t **p_buf);
int neu_dvalue_desialize(uint8_t *buf, neu_data_val_t **p_val);

#ifdef __cplusplus
}
#endif

#endif
