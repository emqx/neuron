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
 * Lesser General Public License for more details.  *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 **/

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "neu_panic.h"
// #include "smart_ptr.h"

#include "neu_data_expr.h"
#include "neu_log.h"
#include "neu_vector.h"

struct neu_data_val {
    neu_dtype_e type;
    union {
        bool     val_bool;
        int8_t   val_int8;
        int16_t  val_int16;
        int32_t  val_int32;
        int64_t  val_int64;
        uint8_t  val_uint8;
        uint16_t val_uint16;
        uint32_t val_uint32;
        uint64_t val_uint64;
        float    val_float;
        double   val_double;
        void *   val_data;
    };

    // For align with 8 bytes
    uint64_t buf[0]; ///< inplace internal buffer
    // Don’t add any member after here
};

static bool type_has_pointer(neu_dtype_e type)
{
    neu_dtype_e val_type;

    val_type = neu_value_type_in_dtype(type);
    switch (val_type) {
    case NEU_DTYPE_INT_VAL:
    case NEU_DTYPE_STRING_VAL:
    case NEU_DTYPE_CSTR:
    case NEU_DTYPE_STRING:
    case NEU_DTYPE_BYTES:
    case NEU_DTYPE_TEXT:
    case NEU_DTYPE_STRUCT:
        return true;

    default:
        if (type & (NEU_DTYPE_ARRAY | NEU_DTYPE_VEC)) {
            return true;
        }
        return false;
    }
}

static neu_data_val_t *data_val_base_new(neu_dtype_e type, size_t size,
                                         bool is_inplace)
{
    neu_dtype_e     real_type;
    neu_data_val_t *val;

    val = malloc(size);
    if (val == NULL) {
        return NULL;
    }

    if (type_has_pointer(type)) {
        real_type = type | NEU_DTYPE_PTR;
        if (is_inplace) {
            // set inplace flag
            real_type &= ~(NEU_DTYPE_EXTERN_PTR | NEU_DTYPE_OWNERED_PTR);
        } else {
            real_type |= NEU_DTYPE_EXTERN_PTR | NEU_DTYPE_OWNERED_PTR;
        }
    } else {
        real_type = type;
    }

    val->type     = real_type;
    val->val_data = NULL;
    return val;
}

neu_dtype_e neu_dvalue_get_type(neu_data_val_t *val)
{
    return val->type;
}

/* new a normal neu_data_val_t with external buffer */
neu_data_val_t *neu_dvalue_new(neu_dtype_e type)
{
    return data_val_base_new(type, sizeof(neu_data_val_t), false);
}

/* new a void type neu_data_val_t, it should be panic. */
neu_data_val_t *neu_dvalue_void_new()
{
    neu_panic("An exception occurred, let be crash!");
    return NULL;
}

/* new a uint type neu_data_val_t, it should be translate to another data value.
 */
neu_data_val_t *neu_dvalue_unit_new()
{
    return data_val_base_new(NEU_DTYPE_UNIT, sizeof(neu_data_val_t), false);
}

/* new a neu_data_val_t with inplace buffer */
neu_data_val_t *neu_dvalue_inplace_new(neu_dtype_e type, size_t buf_size)
{
    size_t size;

    size = sizeof(neu_data_val_t) + buf_size;
    return data_val_base_new(type, size, true);
}

/* new a neu_data_val_t include array with external buffer
 * parameter type is type of element in array
 */
neu_data_val_t *neu_dvalue_array_new(neu_dtype_e type, size_t length,
                                     size_t esize)
{
    neu_dtype_e        real_type;
    neu_fixed_array_t *array;
    neu_data_val_t *   val;

    size_t size = sizeof(neu_fixed_array_t) + length * esize;
    array       = (neu_fixed_array_t *) malloc(size);
    if (array == NULL) {
        return NULL;
    }

    array->length = length;
    array->esize  = esize;

    type      = neu_value_type_in_dtype(type);
    real_type = type | NEU_DTYPE_ARRAY;
    val       = data_val_base_new(real_type, sizeof(neu_data_val_t), false);
    if (val == NULL) {
        free(array);
        return NULL;
    }

    val->val_data = array;
    return val;
}

/* new a neu_data_val_t include vector with external buffer
 * parameter type is type of element in vector
 */
neu_data_val_t *neu_dvalue_vec_new(neu_dtype_e type, size_t length,
                                   size_t esize)
{
    neu_dtype_e     real_type;
    vector_t *      vec;
    neu_data_val_t *val;

    vec = vector_new(length, esize);
    if (vec == NULL) {
        return NULL;
    }

    type      = neu_value_type_in_dtype(type);
    real_type = type | NEU_DTYPE_VEC;
    val       = data_val_base_new(real_type, sizeof(neu_data_val_t), false);
    if (val == NULL) {
        free(vec);
        return NULL;
    }

    val->val_data = vec;
    return val;
}

void neu_dvalue_uninit(neu_data_val_t *val)
{
    if ((val->type & (NEU_DTYPE_EXTERN_PTR | NEU_DTYPE_OWNERED_PTR)) &&
        val->val_data != NULL) {

        if (val->type & NEU_DTYPE_VEC) {
            // TODO: deep free sub value
            vector_free(val->val_data);
        } else if (val->type & NEU_DTYPE_ARRAY) {
            // TODO: deep free sub value
            neu_fixed_array_free(val->val_data);
        } else if (val->type & NEU_DTYPE_PTR) {
            switch (neu_value_type_in_dtype(val->type)) {
            case NEU_DTYPE_INT_VAL:
                neu_int_val_free((neu_int_val_t *) val->val_data);
                break;
            case NEU_DTYPE_STRING_VAL:
                neu_string_val_free((neu_string_val_t *) val->val_data);
                break;
            case NEU_DTYPE_STRING:
                neu_string_free((neu_string_t *) val->val_data);
                break;
            case NEU_DTYPE_BYTES:
                neu_bytes_free((neu_bytes_t *) val->val_data);
                break;
            case NEU_DTYPE_TEXT:
            case NEU_DTYPE_STRUCT:
            case NEU_DTYPE_CSTR:
            default:
                free(val->val_data);
                break;
            }
        } else if (val->type & NEU_DTYPE_SPTR) {
            neu_panic("Smart pointer for data vlaue hasn't implemented");
            // sfree(val->val_data);
        } else {
            log_warn("The data type is invalid");
        }
    }
}

void neu_dvalue_free(neu_data_val_t *val)
{
    neu_dvalue_uninit(val);
    free(val);
}

void *neu_dvalue_get_ptr(neu_data_val_t *val)
{
    bool has_pointer;

    has_pointer = type_has_pointer(val->type);
    assert(has_pointer == true);
    if (has_pointer) {
        return val->val_data;
    }

    return NULL;
}

/*
 * define all value initialize functions
 */
void neu_dvalue_init_bit(neu_data_val_t *val, uint8_t bit)
{
    val->type      = NEU_DTYPE_BIT;
    val->val_uint8 = bit > 0 ? 1 : 0;
}

void neu_dvalue_init_bool(neu_data_val_t *val, bool b)
{
    val->type     = NEU_DTYPE_BOOL;
    val->val_bool = b;
}

void neu_dvalue_init_int8(neu_data_val_t *val, int8_t i8)
{
    val->type     = NEU_DTYPE_INT8;
    val->val_int8 = i8;
}

void neu_dvalue_init_int16(neu_data_val_t *val, int16_t i16)
{
    val->type      = NEU_DTYPE_INT16;
    val->val_int16 = i16;
}

void neu_dvalue_init_int32(neu_data_val_t *val, int32_t i32)
{
    val->type      = NEU_DTYPE_INT32;
    val->val_int32 = i32;
}

void neu_dvalue_init_int64(neu_data_val_t *val, int64_t i64)
{
    val->type      = NEU_DTYPE_INT64;
    val->val_int64 = i64;
}

void neu_dvalue_init_uint8(neu_data_val_t *val, uint8_t u8)
{
    val->type      = NEU_DTYPE_UINT8;
    val->val_uint8 = u8;
}

void neu_dvalue_init_uint16(neu_data_val_t *val, uint16_t u16)
{
    val->type       = NEU_DTYPE_UINT16;
    val->val_uint16 = u16;
}

void neu_dvalue_init_uint32(neu_data_val_t *val, uint32_t u32)
{
    val->type       = NEU_DTYPE_UINT32;
    val->val_uint32 = u32;
}

void neu_dvalue_init_uint64(neu_data_val_t *val, uint64_t u64)
{
    val->type       = NEU_DTYPE_UINT64;
    val->val_uint64 = u64;
}

void neu_dvalue_init_float(neu_data_val_t *val, float f32)
{
    val->type      = NEU_DTYPE_FLOAT;
    val->val_float = f32;
}

void neu_dvalue_init_double(neu_data_val_t *val, double f64)
{
    val->type       = NEU_DTYPE_DOUBLE;
    val->val_double = f64;
}

void neu_dvalue_init_int_val(neu_data_val_t *val, neu_int_val_t int_val)
{
    neu_dtype_e type;

    type = NEU_DTYPE_INT_VAL;
    type |= NEU_DTYPE_PTR | NEU_DTYPE_EXTERN_PTR;
    type |= NEU_DTYPE_OWNERED_PTR;
    val->type     = type;
    val->val_data = malloc(sizeof(neu_int_val_t));
    if (val->val_data != NULL) {
        neu_int_val_move((neu_int_val_t *) val->val_data, &int_val);
    }
}

void neu_dvalue_init_string_val(neu_data_val_t * val,
                                neu_string_val_t string_val)
{
    neu_dtype_e type;

    type = NEU_DTYPE_STRING_VAL;
    type |= NEU_DTYPE_PTR | NEU_DTYPE_EXTERN_PTR;
    type |= NEU_DTYPE_OWNERED_PTR;
    val->type     = type;
    val->val_data = malloc(sizeof(neu_string_val_t));
    if (val->val_data != NULL) {
        neu_string_val_move((neu_string_val_t *) val->val_data, &string_val);
    }
}

void neu_dvalue_init_data_val(neu_data_val_t *val, void *data)
{
    val->type     = NEU_DTYPE_DATA_VAL;
    val->val_data = data;
}

void neu_dvalue_init_errorcode_val(neu_data_val_t *val, int32_t i32)
{
    val->type      = NEU_DTYPE_ERRORCODE;
    val->val_int32 = i32;
}

void neu_dvalue_init_copy_cstr(neu_data_val_t *val, char *cstr)
{
    val->type = NEU_DTYPE_CSTR | NEU_DTYPE_PTR | NEU_DTYPE_EXTERN_PTR;
    val->type |= NEU_DTYPE_OWNERED_PTR;
    val->val_data = strdup(cstr);
}

void neu_dvalue_init_copy_string(neu_data_val_t *val, neu_string_t *string)
{
    val->type = NEU_DTYPE_STRING | NEU_DTYPE_PTR | NEU_DTYPE_EXTERN_PTR;
    val->type |= NEU_DTYPE_OWNERED_PTR;
    val->val_data = neu_string_clone(string);
}

void neu_dvalue_init_copy_bytes(neu_data_val_t *val, neu_bytes_t *bytes)
{
    val->type = NEU_DTYPE_BYTES | NEU_DTYPE_PTR | NEU_DTYPE_EXTERN_PTR;
    val->type |= NEU_DTYPE_OWNERED_PTR;
    val->val_data = neu_bytes_clone(bytes);
}

void neu_dvalue_init_copy_array(neu_data_val_t *val, neu_fixed_array_t *array)
{
    val->type = NEU_DTYPE_ARRAY | NEU_DTYPE_PTR | NEU_DTYPE_EXTERN_PTR;
    val->type |= NEU_DTYPE_OWNERED_PTR;
    val->val_data = neu_fixed_array_clone(array);
}

void neu_dvalue_init_copy_vec(neu_data_val_t *val, vector_t *vec)
{
    val->type = NEU_DTYPE_VEC | NEU_DTYPE_PTR | NEU_DTYPE_EXTERN_PTR;
    val->type |= NEU_DTYPE_OWNERED_PTR;
    val->val_data = vector_clone(vec);
}

void neu_dvalue_init_ref_cstr(neu_data_val_t *val, char *cstr)
{
    val->type = NEU_DTYPE_CSTR | NEU_DTYPE_PTR | NEU_DTYPE_EXTERN_PTR;
    val->type &= ~NEU_DTYPE_OWNERED_PTR;
    val->val_data = cstr;
}

void neu_dvalue_init_ref_string(neu_data_val_t *val, neu_string_t *string)
{
    val->type = NEU_DTYPE_STRING | NEU_DTYPE_PTR | NEU_DTYPE_EXTERN_PTR;
    val->type &= ~NEU_DTYPE_OWNERED_PTR;
    val->val_data = string;
}

void neu_dvalue_init_ref_bytes(neu_data_val_t *val, neu_bytes_t *bytes)
{
    val->type = NEU_DTYPE_BYTES | NEU_DTYPE_PTR | NEU_DTYPE_EXTERN_PTR;
    val->type &= ~NEU_DTYPE_OWNERED_PTR;
    val->val_data = bytes;
}

void neu_dvalue_init_ref_array(neu_data_val_t *val, neu_dtype_e type,
                               neu_fixed_array_t *array)
{
    type = neu_value_type_in_dtype(type);
    type |= NEU_DTYPE_ARRAY | NEU_DTYPE_PTR | NEU_DTYPE_EXTERN_PTR;
    type &= ~NEU_DTYPE_OWNERED_PTR;
    val->type     = type;
    val->val_data = array;
}

void neu_dvalue_init_ref_vec(neu_data_val_t *val, neu_dtype_e type,
                             vector_t *vec)
{
    type = neu_value_type_in_dtype(type);
    type |= NEU_DTYPE_VEC | NEU_DTYPE_PTR | NEU_DTYPE_EXTERN_PTR;
    type &= ~NEU_DTYPE_OWNERED_PTR;
    val->type     = type;
    val->val_data = vec;
}

/* The owership of parameter cstr is move to neu_data_val_t
 */
void neu_dvalue_init_move_cstr(neu_data_val_t *val, char *cstr)
{
    neu_dtype_e type;

    type = NEU_DTYPE_CSTR;
    type |= NEU_DTYPE_PTR | NEU_DTYPE_EXTERN_PTR;
    type |= NEU_DTYPE_OWNERED_PTR;
    val->type     = type;
    val->val_data = cstr;
}

/* The owership of parameter string is move to neu_data_val_t
 */
void neu_dvalue_init_move_string(neu_data_val_t *val, neu_string_t *string)
{
    neu_dtype_e type;

    type = NEU_DTYPE_STRING;
    type |= NEU_DTYPE_PTR | NEU_DTYPE_EXTERN_PTR;
    type |= NEU_DTYPE_OWNERED_PTR;
    val->type     = type;
    val->val_data = string;
}

/* The owership of parameter bytes is move to neu_data_val_t
 */
void neu_dvalue_init_move_bytes(neu_data_val_t *val, neu_bytes_t *bytes)
{
    neu_dtype_e type;

    type = NEU_DTYPE_BYTES;
    type |= NEU_DTYPE_PTR | NEU_DTYPE_EXTERN_PTR;
    type |= NEU_DTYPE_OWNERED_PTR;
    val->type     = type;
    val->val_data = bytes;
}

/* The owership of parameter array is move to neu_data_val_t
 */
void neu_dvalue_init_move_array(neu_data_val_t *val, neu_dtype_e type,
                                neu_fixed_array_t *array)
{
    type = neu_value_type_in_dtype(type);
    type |= NEU_DTYPE_ARRAY | NEU_DTYPE_PTR | NEU_DTYPE_EXTERN_PTR;
    type |= NEU_DTYPE_OWNERED_PTR;
    val->type     = type;
    val->val_data = array;
}

/* The owership of parameter vector is move to neu_data_val_t
 */
void neu_dvalue_init_move_vec(neu_data_val_t *val, neu_dtype_e type,
                              vector_t *vec)
{
    type = neu_value_type_in_dtype(type);
    type |= NEU_DTYPE_VEC | NEU_DTYPE_PTR | NEU_DTYPE_EXTERN_PTR;
    type |= NEU_DTYPE_OWNERED_PTR;
    val->type     = type;
    val->val_data = vec;
}

/*
 * define all value set functions
 */
int neu_dvalue_set_bit(neu_data_val_t *val, uint8_t bit)
{
    assert(val->type == NEU_DTYPE_BIT);
    if (val->type == NEU_DTYPE_BIT) {
        val->val_uint8 = bit > 0 ? 1 : 0;
        return 0;
    } else {
        return -1;
    }
}

int neu_dvalue_set_bool(neu_data_val_t *val, bool b)
{
    assert(val->type == NEU_DTYPE_BOOL);
    if (val->type == NEU_DTYPE_BOOL) {
        val->val_bool = b;
        return 0;
    } else {
        return -1;
    }
}

int neu_dvalue_set_int8(neu_data_val_t *val, int8_t i8)
{
    assert(val->type == NEU_DTYPE_INT8);
    if (val->type == NEU_DTYPE_INT8) {
        val->val_int8 = i8;
        return 0;
    } else {
        return -1;
    }
}

int neu_dvalue_set_int16(neu_data_val_t *val, int16_t i16)
{
    assert(val->type == NEU_DTYPE_INT16);
    if (val->type == NEU_DTYPE_INT16) {
        val->val_int16 = i16;
        return 0;
    } else {
        return -1;
    }
}

int neu_dvalue_set_int32(neu_data_val_t *val, int32_t i32)
{
    assert(val->type == NEU_DTYPE_INT32);
    if (val->type == NEU_DTYPE_INT32) {
        val->val_int32 = i32;
        return 0;
    } else {
        return -1;
    }
}

int neu_dvalue_set_int64(neu_data_val_t *val, int64_t i64)
{
    assert(val->type == NEU_DTYPE_INT64);
    if (val->type == NEU_DTYPE_INT64) {
        val->val_int64 = i64;
        return 0;
    } else {
        return -1;
    }
}

int neu_dvalue_set_uint8(neu_data_val_t *val, uint8_t u8)
{
    assert(val->type == NEU_DTYPE_UINT8);
    if (val->type == NEU_DTYPE_UINT8) {
        val->val_uint8 = u8;
        return 0;
    } else {
        return -1;
    }
}

int neu_dvalue_set_uint16(neu_data_val_t *val, uint16_t u16)
{
    assert(val->type == NEU_DTYPE_UINT16);
    if (val->type == NEU_DTYPE_UINT16) {
        val->val_uint16 = u16;
        return 0;
    } else {
        return -1;
    }
}

int neu_dvalue_set_uint32(neu_data_val_t *val, uint32_t u32)
{
    assert(val->type == NEU_DTYPE_UINT32);
    if (val->type == NEU_DTYPE_UINT32) {
        val->val_uint32 = u32;
        return 0;
    } else {
        return -1;
    }
}

int neu_dvalue_set_uint64(neu_data_val_t *val, uint64_t u64)
{
    assert(val->type == NEU_DTYPE_UINT64);
    if (val->type == NEU_DTYPE_UINT64) {
        val->val_uint64 = u64;
        return 0;
    } else {
        return -1;
    }
}

int neu_dvalue_set_float(neu_data_val_t *val, float f32)
{
    assert(val->type == NEU_DTYPE_FLOAT);
    if (val->type == NEU_DTYPE_FLOAT) {
        val->val_float = f32;
        return 0;
    } else {
        return -1;
    }
}

int neu_dvalue_set_double(neu_data_val_t *val, double f64)
{
    assert(val->type == NEU_DTYPE_DOUBLE);
    if (val->type == NEU_DTYPE_DOUBLE) {
        val->val_double = f64;
        return 0;
    } else {
        return -1;
    }
}

int neu_dvalue_set_int_val(neu_data_val_t *val, neu_int_val_t int_val)
{
    neu_dtype_e type;

    type = NEU_DTYPE_INT_VAL | NEU_DTYPE_PTR | NEU_DTYPE_EXTERN_PTR;
    type |= NEU_DTYPE_OWNERED_PTR;
    assert(val->type == type);
    if (val->type == type) {
        if (val->val_data != NULL) {
            neu_int_val_t int_val;
            int_val = *(neu_int_val_t *) val->val_data;
            neu_dvalue_free(int_val.val);
            free(val->val_data);
        }
        val->val_data = malloc(sizeof(neu_int_val_t));
        if (val->val_data != NULL) {
            neu_int_val_move((neu_int_val_t *) val->val_data, &int_val);
        }
        return 0;
    } else {
        return -1;
    }
}

int neu_dvalue_set_string_val(neu_data_val_t *val, neu_string_val_t string_val)
{
    neu_dtype_e type;

    type = NEU_DTYPE_STRING_VAL | NEU_DTYPE_PTR | NEU_DTYPE_EXTERN_PTR;
    type |= NEU_DTYPE_OWNERED_PTR;
    assert(val->type == type);
    if (val->type == type) {
        if (val->val_data != NULL) {
            neu_string_val_t string_val;
            string_val = *(neu_string_val_t *) val->val_data;
            neu_dvalue_free(string_val.val);
            free(val->val_data);
        }
        val->val_data = malloc(sizeof(neu_string_val_t));
        if (val->val_data != NULL) {
            neu_string_val_move((neu_string_val_t *) val->val_data,
                                &string_val);
        }
        return 0;
    } else {
        return -1;
    }
}

int neu_dvalue_set_errorcode(neu_data_val_t *val, int32_t i32)
{
    assert(val->type == NEU_DTYPE_ERRORCODE);
    if (val->type == NEU_DTYPE_ERRORCODE) {
        val->val_int32 = i32;
        return 0;
    } else {
        return -1;
    }
}

int neu_dvalue_set_cstr(neu_data_val_t *val, char *cstr)
{
    neu_dtype_e type;

    type = NEU_DTYPE_CSTR | NEU_DTYPE_PTR | NEU_DTYPE_EXTERN_PTR;
    type |= NEU_DTYPE_OWNERED_PTR;
    assert(val->type == type);
    if (val->type == type) {
        if (val->val_data != NULL) {
            free(val->val_data);
        }
        val->val_data = strdup(cstr);
        return 0;
    } else {
        return -1;
    }
}

int neu_dvalue_set_string(neu_data_val_t *val, neu_string_t *string)
{
    neu_dtype_e type;

    type = NEU_DTYPE_BYTES | NEU_DTYPE_PTR | NEU_DTYPE_EXTERN_PTR;
    type |= NEU_DTYPE_OWNERED_PTR;
    assert(val->type == type);
    if (val->type == type) {
        if (val->val_data != NULL) {
            neu_string_free(val->val_data);
        }
        val->val_data = neu_string_clone(string);
        return 0;
    } else {
        return -1;
    }
}

int neu_dvalue_set_bytes(neu_data_val_t *val, neu_bytes_t *bytes)
{
    neu_dtype_e type;

    type = NEU_DTYPE_BYTES | NEU_DTYPE_PTR | NEU_DTYPE_EXTERN_PTR;
    type |= NEU_DTYPE_OWNERED_PTR;
    assert(val->type == type);
    if (val->type == type) {
        if (val->val_data != NULL) {
            neu_bytes_free(val->val_data);
        }
        val->val_data = neu_bytes_clone(bytes);
        return 0;
    } else {
        return -1;
    }
}

int neu_dvalue_set_array(neu_data_val_t *val, neu_fixed_array_t *array)
{
    neu_dtype_e type;

    type = NEU_DTYPE_ARRAY | NEU_DTYPE_PTR | NEU_DTYPE_EXTERN_PTR;
    type |= NEU_DTYPE_OWNERED_PTR;
    assert(neu_flags_type_in_dtype(val->type) == type);
    if (neu_flags_type_in_dtype(val->type) == type) {
        if (val->val_data != NULL) {
            neu_fixed_array_free(val->val_data);
        }
        val->val_data = neu_fixed_array_clone(array);
        return 0;
    } else {
        log_error("value type: 0x%08x not match expect type: 0x%08x", val->type,
                  type);
        return -1;
    }
}

int neu_dvalue_set_vec(neu_data_val_t *val, vector_t *vec)
{
    neu_dtype_e type;

    type = NEU_DTYPE_VEC | NEU_DTYPE_PTR | NEU_DTYPE_EXTERN_PTR;
    type |= NEU_DTYPE_OWNERED_PTR;
    assert(neu_flags_type_in_dtype(val->type) == type);
    if (neu_flags_type_in_dtype(val->type) == type) {
        if (val->val_data != NULL) {
            vector_free(val->val_data);
        }
        val->val_data = vector_clone(vec);
        return 0;
    } else {
        log_error("value type: 0x%08x not match expect type: 0x%08x", val->type,
                  type);
        return -1;
    }
}

int neu_dvalue_set_ref_cstr(neu_data_val_t *val, char *cstr)
{
    neu_dtype_e type;
    neu_dtype_e val_type;

    type = NEU_DTYPE_CSTR | NEU_DTYPE_PTR | NEU_DTYPE_EXTERN_PTR;
    type &= ~NEU_DTYPE_OWNERED_PTR;
    val_type = val->type & ~NEU_DTYPE_OWNERED_PTR;
    assert(val_type == type);
    if (val_type == type) {
        if (val->type & NEU_DTYPE_OWNERED_PTR) {
            free(val->val_data);
        }
        val->val_data = cstr;
        val->type     = type;
        return 0;
    } else {
        return -1;
    }
}

int neu_dvalue_set_ref_string(neu_data_val_t *val, neu_string_t *string)
{
    neu_dtype_e type;
    neu_dtype_e val_type;

    type = NEU_DTYPE_STRING | NEU_DTYPE_PTR | NEU_DTYPE_EXTERN_PTR;
    type &= ~NEU_DTYPE_OWNERED_PTR;
    val_type = val->type & ~NEU_DTYPE_OWNERED_PTR;
    assert(val_type == type);
    if (val_type == type) {
        if (val->type & NEU_DTYPE_OWNERED_PTR) {
            neu_string_free(val->val_data);
        }
        val->val_data = string;
        val->type     = type;
        return 0;
    } else {
        return -1;
    }
}

int neu_dvalue_set_ref_bytes(neu_data_val_t *val, neu_bytes_t *bytes)
{
    neu_dtype_e type;
    neu_dtype_e val_type;

    type = NEU_DTYPE_BYTES | NEU_DTYPE_PTR | NEU_DTYPE_EXTERN_PTR;
    type &= ~NEU_DTYPE_OWNERED_PTR;
    val_type = val->type & ~NEU_DTYPE_OWNERED_PTR;
    assert(val_type == type);
    if (val_type == type) {
        if (val->type & NEU_DTYPE_OWNERED_PTR) {
            neu_bytes_free(val->val_data);
        }
        val->val_data = bytes;
        val->type     = type;
        return 0;
    } else {
        return -1;
    }
}

int neu_dvalue_set_ref_array(neu_data_val_t *val, neu_fixed_array_t *array)
{
    neu_dtype_e type;
    neu_dtype_e flags_type;

    type = NEU_DTYPE_ARRAY | NEU_DTYPE_PTR | NEU_DTYPE_EXTERN_PTR;
    type &= ~NEU_DTYPE_OWNERED_PTR;
    flags_type = neu_flags_type_in_dtype(val->type);
    assert((flags_type & ~NEU_DTYPE_OWNERED_PTR) == type);
    if ((flags_type & ~NEU_DTYPE_OWNERED_PTR) == type) {
        if (val->type & NEU_DTYPE_OWNERED_PTR) {
            neu_fixed_array_free(val->val_data);
        }
        val->val_data = array;
        val->type     = neu_value_type_in_dtype(val->type) | type;
        return 0;
    } else {
        return -1;
    }
}

int neu_dvalue_set_ref_vec(neu_data_val_t *val, vector_t *vec)
{
    neu_dtype_e type;
    neu_dtype_e flags_type;

    type = NEU_DTYPE_VEC | NEU_DTYPE_PTR | NEU_DTYPE_EXTERN_PTR;
    type &= ~NEU_DTYPE_OWNERED_PTR;
    flags_type = neu_flags_type_in_dtype(val->type);
    assert((flags_type & ~NEU_DTYPE_OWNERED_PTR) == type);
    if ((flags_type & ~NEU_DTYPE_OWNERED_PTR) == type) {
        if (val->type & NEU_DTYPE_OWNERED_PTR) {
            neu_fixed_array_free(val->val_data);
        }
        val->val_data = vec;
        val->type     = neu_value_type_in_dtype(val->type) | type;
        return 0;
    } else {
        return -1;
    }
}

/*
int neu_dvalue_set_sptr_cstr(neu_data_val_t *val, char *cstr)
{
    neu_dtype_e type;

    type = NEU_DTYPE_CSTR | NEU_DTYPE_SPTR | NEU_DTYPE_EXTERN_PTR;
    type &= ~NEU_DTYPE_OWNERED_PTR;
    assert(val->type == type);
    if (val->type == type) {
        (void) cstr;
        neu_panic("Smart pointer for data vlaue hasn't implemented");
        // val->val_data = sref(cstr);
        return 0;
    } else {
        return -1;
    }
}

int neu_dvalue_set_sptr_bytes(neu_data_val_t *val, neu_bytes_t *bytes)
{
    neu_dtype_e type;

    type = NEU_DTYPE_BYTES | NEU_DTYPE_SPTR | NEU_DTYPE_EXTERN_PTR;
    type &= ~NEU_DTYPE_OWNERED_PTR;
    assert(val->type == type);
    if (val->type == type) {
        (void) bytes;
        neu_panic("Smart pointer for data vlaue hasn't implemented");
        // val->val_data = sref(bytes);
        return 0;
    } else {
        return -1;
    }
}
*/

int neu_dvalue_set_move_cstr(neu_data_val_t *val, char *cstr)
{
    neu_dtype_e type;

    type = NEU_DTYPE_CSTR | NEU_DTYPE_PTR | NEU_DTYPE_EXTERN_PTR;
    type |= NEU_DTYPE_OWNERED_PTR;
    assert(val->type == type);
    if (val->type == type) {
        val->val_data = cstr;
        return 0;
    } else {
        return -1;
    }
}

int neu_dvalue_set_move_string(neu_data_val_t *val, neu_string_t *string)
{
    neu_dtype_e type;

    type = NEU_DTYPE_STRING | NEU_DTYPE_PTR | NEU_DTYPE_EXTERN_PTR;
    type |= NEU_DTYPE_OWNERED_PTR;
    assert(val->type == type);
    if (val->type == type) {
        val->val_data = string;
        return 0;
    } else {
        return -1;
    }
}

int neu_dvalue_set_move_bytes(neu_data_val_t *val, neu_bytes_t *bytes)
{
    neu_dtype_e type;

    type = NEU_DTYPE_BYTES | NEU_DTYPE_PTR | NEU_DTYPE_EXTERN_PTR;
    type |= NEU_DTYPE_OWNERED_PTR;
    assert(val->type == type);
    if (val->type == type) {
        val->val_data = bytes;
        return 0;
    } else {
        return -1;
    }
}

int neu_dvalue_set_move_array(neu_data_val_t *val, neu_fixed_array_t *array)
{
    neu_dtype_e type;

    type = NEU_DTYPE_ARRAY | NEU_DTYPE_PTR | NEU_DTYPE_EXTERN_PTR;
    type |= NEU_DTYPE_OWNERED_PTR;
    assert(neu_flags_type_in_dtype(val->type) == type);
    if (neu_flags_type_in_dtype(val->type) == type) {
        val->val_data = array;
        return 0;
    } else {
        return -1;
    }
}

int neu_dvalue_set_move_vec(neu_data_val_t *val, vector_t *vec)
{
    neu_dtype_e type;

    type = NEU_DTYPE_VEC | NEU_DTYPE_PTR | NEU_DTYPE_EXTERN_PTR;
    type |= NEU_DTYPE_OWNERED_PTR;
    assert(neu_flags_type_in_dtype(val->type) == type);
    if (neu_flags_type_in_dtype(val->type) == type) {
        val->val_data = vec;
        return 0;
    } else {
        return -1;
    }
}

/*
 * define all value get functions
 */
int neu_dvalue_get_bit(neu_data_val_t *val, uint8_t *p_b)
{
    assert(val->type == NEU_DTYPE_BIT);
    if (val->type == NEU_DTYPE_BIT) {
        *p_b = val->val_uint8;
        return 0;
    } else {
        return -1;
    }
}

int neu_dvalue_get_bool(neu_data_val_t *val, bool *p_b)
{
    assert(val->type == NEU_DTYPE_BOOL);
    if (val->type == NEU_DTYPE_BOOL) {
        *p_b = val->val_bool;
        return 0;
    } else {
        return -1;
    }
}

int neu_dvalue_get_int8(neu_data_val_t *val, int8_t *p_i8)
{
    assert(val->type == NEU_DTYPE_INT8);
    if (val->type == NEU_DTYPE_INT8) {
        *p_i8 = val->val_int8;
        return 0;
    } else {
        return -1;
    }
}

int neu_dvalue_get_int16(neu_data_val_t *val, int16_t *p_i16)
{
    assert(val->type == NEU_DTYPE_INT16);
    if (val->type == NEU_DTYPE_INT16) {
        *p_i16 = val->val_int16;
        return 0;
    } else {
        return -1;
    }
}

int neu_dvalue_get_int32(neu_data_val_t *val, int32_t *p_i32)
{
    assert(val->type == NEU_DTYPE_INT32);
    if (val->type == NEU_DTYPE_INT32) {
        *p_i32 = val->val_int32;
        return 0;
    } else {
        return -1;
    }
}

int neu_dvalue_get_int64(neu_data_val_t *val, int64_t *p_i64)
{
    assert(val->type == NEU_DTYPE_INT64);
    if (val->type == NEU_DTYPE_INT64) {
        *p_i64 = val->val_int64;
        return 0;
    } else {
        return -1;
    }
}

int neu_dvalue_get_uint8(neu_data_val_t *val, uint8_t *p_u8)
{
    assert(val->type == NEU_DTYPE_UINT8);
    if (val->type == NEU_DTYPE_UINT8) {
        *p_u8 = val->val_uint8;
        return 0;
    } else {
        return -1;
    }
}

int neu_dvalue_get_uint16(neu_data_val_t *val, uint16_t *p_u16)
{
    assert(val->type == NEU_DTYPE_UINT16);
    if (val->type == NEU_DTYPE_UINT16) {
        *p_u16 = val->val_uint16;
        return 0;
    } else {
        return -1;
    }
}

int neu_dvalue_get_uint32(neu_data_val_t *val, uint32_t *p_u32)
{
    assert(val->type == NEU_DTYPE_UINT32);
    if (val->type == NEU_DTYPE_UINT32) {
        *p_u32 = val->val_uint32;
        return 0;
    } else {
        return -1;
    }
}

int neu_dvalue_get_uint64(neu_data_val_t *val, uint64_t *p_u64)
{
    assert(val->type == NEU_DTYPE_UINT64);
    if (val->type == NEU_DTYPE_UINT64) {
        *p_u64 = val->val_uint64;
        return 0;
    } else {
        return -1;
    }
}

int neu_dvalue_get_float(neu_data_val_t *val, float *p_f32)
{
    assert(val->type == NEU_DTYPE_FLOAT);
    if (val->type == NEU_DTYPE_FLOAT) {
        *p_f32 = val->val_float;
        return 0;
    } else {
        return -1;
    }
}

int neu_dvalue_get_double(neu_data_val_t *val, double *p_f64)
{
    assert(val->type == NEU_DTYPE_DOUBLE);
    if (val->type == NEU_DTYPE_DOUBLE) {
        *p_f64 = val->val_double;
        return 0;
    } else {
        return -1;
    }
}

int neu_dvalue_get_int_val(neu_data_val_t *val, neu_int_val_t *p_int_val)
{
    neu_dtype_e val_type;
    neu_dtype_e type;

    type = NEU_DTYPE_INT_VAL | NEU_DTYPE_EXTERN_PTR;
    type |= NEU_DTYPE_OWNERED_PTR;

    val_type = val->type & (~NEU_DTYPE_PTR_MASK);
    assert(val_type == type);
    if (val_type == type) {
        neu_int_val_move(p_int_val, (neu_int_val_t *) val->val_data);
        return 0;
    } else {
        return -1;
    }
}

int neu_dvalue_get_string_val(neu_data_val_t *  val,
                              neu_string_val_t *p_string_val)
{
    neu_dtype_e val_type;
    neu_dtype_e type;

    type = NEU_DTYPE_STRING_VAL | NEU_DTYPE_EXTERN_PTR;
    type |= NEU_DTYPE_OWNERED_PTR;

    val_type = val->type & (~NEU_DTYPE_PTR_MASK);
    assert(val_type == type);
    if (val_type == type) {
        neu_string_val_move(p_string_val, (neu_string_val_t *) val->val_data);
        return 0;
    } else {
        return -1;
    }
}

int neu_dvalue_get_data_val(neu_data_val_t *val, void **p_data)
{
    assert(val->type == NEU_DTYPE_DATA_VAL);
    if (val->type == NEU_DTYPE_DATA_VAL) {
        *p_data = val->val_data;
        return 0;
    } else {
        return -1;
    }
}

int neu_dvalue_get_errorcode(neu_data_val_t *val, int32_t *p_i32)
{
    assert(val->type == NEU_DTYPE_ERRORCODE);
    if (val->type == NEU_DTYPE_ERRORCODE) {
        *p_i32 = val->val_int32;
        return 0;
    } else {
        return -1;
    }
}

int neu_dvalue_get_cstr(neu_data_val_t *val, char **p_cstr)
{
    neu_dtype_e val_type;
    neu_dtype_e type;

    type = NEU_DTYPE_CSTR | NEU_DTYPE_EXTERN_PTR;
    type |= NEU_DTYPE_OWNERED_PTR;

    val_type = val->type & (~NEU_DTYPE_PTR_MASK);
    assert(val_type == type);
    if (val_type == type) {
        *p_cstr = strdup((char *) val->val_data);
        return 0;
    } else {
        *p_cstr = NULL;
        return -1;
    }
}

int neu_dvalue_get_string(neu_data_val_t *val, neu_string_t **p_string)
{
    neu_dtype_e val_type;
    neu_dtype_e type;

    type = NEU_DTYPE_STRING | NEU_DTYPE_EXTERN_PTR;
    type |= NEU_DTYPE_OWNERED_PTR;

    val_type = val->type & (~NEU_DTYPE_PTR_MASK);
    assert(val_type == type);
    if (val_type == type) {
        *p_string = neu_string_clone((neu_string_t *) val->val_data);
        return 0;
    } else {
        *p_string = NULL;
        return -1;
    }
}

int neu_dvalue_get_bytes(neu_data_val_t *val, neu_bytes_t **p_bytes)
{
    neu_dtype_e val_type;
    neu_dtype_e type;

    type = NEU_DTYPE_BYTES | NEU_DTYPE_EXTERN_PTR;
    type |= NEU_DTYPE_OWNERED_PTR;

    val_type = val->type & (~NEU_DTYPE_PTR_MASK);
    assert(val_type == type);
    if (val_type == type) {
        *p_bytes = neu_bytes_clone((neu_bytes_t *) val->val_data);
        return 0;
    } else {
        *p_bytes = NULL;
        return -1;
    }
}

int neu_dvalue_get_array(neu_data_val_t *val, neu_fixed_array_t **p_array)
{
    neu_dtype_e val_type;
    neu_dtype_e type;

    type = NEU_DTYPE_ARRAY;

    val_type = val->type & (~NEU_DTYPE_PTR_MASK);
    val_type &= ~NEU_DTYPE_ATTR_MASK;
    assert(neu_flags_type_in_dtype(val_type) == type);
    if (neu_flags_type_in_dtype(val_type) == type) {
        *p_array = neu_fixed_array_clone((neu_fixed_array_t *) val->val_data);
        return 0;
    } else {
        *p_array = NULL;
        log_error("value type: 0x%08x not match expect type: 0x%08x", val_type,
                  type);
        return -1;
    }
}

int neu_dvalue_get_vec(neu_data_val_t *val, vector_t **p_vec)
{
    neu_dtype_e val_type;
    neu_dtype_e type;

    type = NEU_DTYPE_VEC;

    val_type = val->type & (~NEU_DTYPE_PTR_MASK);
    val_type &= ~NEU_DTYPE_ATTR_MASK;
    assert(neu_flags_type_in_dtype(val_type) == type);
    if (neu_flags_type_in_dtype(val_type) == type) {
        *p_vec = vector_clone((vector_t *) val->val_data);
        return 0;
    } else {
        *p_vec = NULL;
        log_error("value type: 0x%08x not match expect type: 0x%08x", val_type,
                  type);
        return -1;
    }
}

int neu_dvalue_get_ref_cstr(neu_data_val_t *val, char **p_cstr)
{
    neu_dtype_e type;
    neu_dtype_e val_type;

    type = NEU_DTYPE_CSTR | NEU_DTYPE_EXTERN_PTR;
    type &= ~NEU_DTYPE_OWNERED_PTR;

    val_type = val->type & ~NEU_DTYPE_OWNERED_PTR;
    val_type &= ~NEU_DTYPE_PTR_MASK;
    assert(val_type == type);
    if (val_type == type) {
        *p_cstr = (char *) val->val_data;
        return 0;
    } else {
        *p_cstr = NULL;
        return -1;
    }
}

int neu_dvalue_get_ref_string(neu_data_val_t *val, neu_string_t **p_string)
{
    neu_dtype_e val_type;
    neu_dtype_e type;

    type = NEU_DTYPE_STRING | NEU_DTYPE_EXTERN_PTR;
    type &= ~NEU_DTYPE_OWNERED_PTR;

    val_type = val->type & ~NEU_DTYPE_OWNERED_PTR;
    val_type &= ~NEU_DTYPE_PTR_MASK;
    assert(val_type == type);
    if (val_type == type) {
        *p_string = (neu_string_t *) val->val_data;
        return 0;
    } else {
        *p_string = NULL;
        return -1;
    }
}

int neu_dvalue_get_ref_bytes(neu_data_val_t *val, neu_bytes_t **p_bytes)
{
    neu_dtype_e val_type;
    neu_dtype_e type;

    type = NEU_DTYPE_BYTES | NEU_DTYPE_EXTERN_PTR;
    type &= ~NEU_DTYPE_OWNERED_PTR;

    val_type = val->type & ~NEU_DTYPE_OWNERED_PTR;
    val_type &= ~NEU_DTYPE_PTR_MASK;
    assert(val_type == type);
    if (val_type == type) {
        *p_bytes = (neu_bytes_t *) val->val_data;
        return 0;
    } else {
        *p_bytes = NULL;
        return -1;
    }
}

int neu_dvalue_get_ref_array(neu_data_val_t *val, neu_fixed_array_t **p_array)
{
    neu_dtype_e val_type;
    neu_dtype_e type;

    type = NEU_DTYPE_ARRAY;

    val_type = val->type & (~NEU_DTYPE_PTR_MASK);
    val_type &= ~NEU_DTYPE_ATTR_MASK;
    assert(neu_flags_type_in_dtype(val_type) == type);
    if (neu_flags_type_in_dtype(val_type) == type) {
        *p_array = (neu_fixed_array_t *) val->val_data;
        return 0;
    } else {
        *p_array = NULL;
        log_error("value type: 0x%08x not match expect type: 0x%08x", val_type,
                  type);
        return -1;
    }
}

int neu_dvalue_get_ref_vec(neu_data_val_t *val, vector_t **p_vec)
{
    neu_dtype_e val_type;
    neu_dtype_e type;

    type = NEU_DTYPE_VEC;

    val_type = val->type & (~NEU_DTYPE_PTR_MASK);
    val_type &= ~NEU_DTYPE_ATTR_MASK;
    assert(neu_flags_type_in_dtype(val_type) == type);
    if (neu_flags_type_in_dtype(val_type) == type) {
        *p_vec = (vector_t *) val->val_data;
        return 0;
    } else {
        *p_vec = NULL;
        log_error("value type: 0x%08x not match expect type: 0x%08x", val_type,
                  type);
        return -1;
    }
}

int neu_dvalue_get_move_cstr(neu_data_val_t *val, char **p_cstr)
{
    neu_dtype_e val_type;
    neu_dtype_e type;

    type = NEU_DTYPE_CSTR | NEU_DTYPE_EXTERN_PTR;
    type |= NEU_DTYPE_OWNERED_PTR;

    val_type = val->type & (~NEU_DTYPE_PTR_MASK);
    assert(val_type == type);
    if (val_type == type) {
        *p_cstr       = (char *) val->val_data;
        val->val_data = NULL;
        val->type &= ~NEU_DTYPE_OWNERED_PTR;
        return 0;
    } else {
        *p_cstr = NULL;
        return -1;
    }
}

int neu_dvalue_get_move_string(neu_data_val_t *val, neu_string_t **p_string)
{
    neu_dtype_e val_type;
    neu_dtype_e type;

    type = NEU_DTYPE_STRING | NEU_DTYPE_EXTERN_PTR;
    type |= NEU_DTYPE_OWNERED_PTR;

    val_type = val->type & (~NEU_DTYPE_PTR_MASK);
    assert(val_type == type);
    if (val_type == type) {
        *p_string     = (neu_string_t *) val->val_data;
        val->val_data = NULL;
        val->type &= ~NEU_DTYPE_OWNERED_PTR;
        return 0;
    } else {
        *p_string = NULL;
        return -1;
    }
}

int neu_dvalue_get_move_bytes(neu_data_val_t *val, neu_bytes_t **p_bytes)
{
    neu_dtype_e val_type;
    neu_dtype_e type;

    type = NEU_DTYPE_BYTES | NEU_DTYPE_EXTERN_PTR;
    type |= NEU_DTYPE_OWNERED_PTR;

    val_type = val->type & (~NEU_DTYPE_PTR_MASK);
    assert(val_type == type);
    if (val_type == type) {
        *p_bytes      = (neu_bytes_t *) val->val_data;
        val->val_data = NULL;
        val->type &= ~NEU_DTYPE_OWNERED_PTR;
        return 0;
    } else {
        *p_bytes = NULL;
        return -1;
    }
}

int neu_dvalue_get_move_array(neu_data_val_t *val, neu_fixed_array_t **p_array)
{
    neu_dtype_e val_type;
    neu_dtype_e type;

    type = NEU_DTYPE_ARRAY;

    val_type = val->type & (~NEU_DTYPE_PTR_MASK);
    val_type &= ~NEU_DTYPE_ATTR_MASK;
    assert(neu_flags_type_in_dtype(val_type) == type);
    if (neu_flags_type_in_dtype(val_type) == type) {
        *p_array      = (neu_fixed_array_t *) val->val_data;
        val->val_data = NULL;
        val->type &= ~NEU_DTYPE_OWNERED_PTR;
        return 0;
    } else {
        *p_array = NULL;
        log_error("value type: 0x%08x not match expect type: 0x%08x", val_type,
                  type);
        return -1;
    }
}

int neu_dvalue_get_move_vec(neu_data_val_t *val, vector_t **p_vec)
{
    neu_dtype_e val_type;
    neu_dtype_e type;

    type = NEU_DTYPE_VEC;

    val_type = val->type & (~NEU_DTYPE_PTR_MASK);
    val_type &= ~NEU_DTYPE_ATTR_MASK;
    assert(neu_flags_type_in_dtype(val_type) == type);
    if (neu_flags_type_in_dtype(val_type) == type) {
        *p_vec        = (vector_t *) val->val_data;
        val->val_data = NULL;
        val->type &= ~NEU_DTYPE_OWNERED_PTR;
        return 0;
    } else {
        *p_vec = NULL;
        log_error("value type: 0x%08x not match expect type: 0x%08x", val_type,
                  type);
        return -1;
    }
}

/*
 * define functions for serialize and deserialize
 */
#define prim_val_ser_size_case(upcase_type, locase_type, locase_type_t) \
    case NEU_DTYPE_##upcase_type: {                                     \
        size += sizeof(locase_type_t);                                  \
        break;                                                          \
    }

#define prim_val_ser_size_case_t(upcase_type, locase_type) \
    prim_val_ser_size_case(upcase_type, locase_type, locase_type##_t)

static size_t dvalue_get_serialized_size(neu_data_val_t *val)
{
    neu_dtype_e type;
    size_t      size;

    size = sizeof(neu_dtype_e);
    type = val->type;
    type &= ~(NEU_DTYPE_PTR_MASK | NEU_DTYPE_ATTR_MASK);
    if (type == neu_value_type_in_dtype(type)) {
        switch (type) {
            prim_val_ser_size_case_t(INT8, int8);
            prim_val_ser_size_case_t(INT16, int16);
            prim_val_ser_size_case_t(INT32, int32);
            prim_val_ser_size_case_t(INT64, int64);

            prim_val_ser_size_case_t(UINT8, uint8);
            prim_val_ser_size_case_t(UINT16, uint16);
            prim_val_ser_size_case_t(UINT32, uint32);
            prim_val_ser_size_case_t(UINT64, uint64);

            prim_val_ser_size_case(FLOAT, float, float);
            prim_val_ser_size_case(DOUBLE, double, double);

        case NEU_DTYPE_INT_VAL: {
            neu_int_val_t *int_val;
            int_val = (neu_int_val_t *) val->val_data;
            size += sizeof(int_val->key);
            size += dvalue_get_serialized_size(int_val->val);
            break;
        }

        case NEU_DTYPE_STRING_VAL: {
            neu_string_val_t *string_val;
            string_val = (neu_string_val_t *) val->val_data;
            size += neu_string_serialized_size(string_val->key);
            size += dvalue_get_serialized_size(string_val->val);
            break;
        }

        case NEU_DTYPE_CSTR: {
            char *cstr;
            cstr = (char *) val->val_data;
            size += strlen(cstr) + 1;
            break;
        }

        case NEU_DTYPE_STRING: {
            size += neu_string_serialized_size(val->val_data);
            break;
        }

        case NEU_DTYPE_BYTES: {
            neu_bytes_t *bytes;
            bytes = (neu_bytes_t *) val->val_data;
            size += bytes->length + sizeof(size_t);
            break;
        }

        default:
            log_error("Not support base type(%d) of data value in sized", type);
            size = 0;
            break;
        }
    } else if ((type & NEU_DTYPE_ARRAY) == NEU_DTYPE_ARRAY) {
        neu_fixed_array_t *array;
        array = (neu_fixed_array_t *) val->val_data;
        if (NEU_DTYPE_DATA_VAL == neu_value_type_in_dtype(val->type)) {
            size_t index;
            for (index = 0; index < array->length; index++) {
                neu_data_val_t *sub_val;
                sub_val =
                    *(neu_data_val_t **) neu_fixed_array_get(array, index);
                size += dvalue_get_serialized_size(sub_val);
            }
            size += 2 * sizeof(size_t);
        } else {
            size += array->length * array->esize + 2 * sizeof(size_t);
        }
    } else if ((type & NEU_DTYPE_VEC) == NEU_DTYPE_VEC) {
        vector_t *vec;
        vec = (vector_t *) val->val_data;
        if (NEU_DTYPE_DATA_VAL == neu_value_type_in_dtype(val->type)) {
            VECTOR_FOR_EACH(vec, iter)
            {
                neu_data_val_t *sub_val;
                sub_val = (neu_data_val_t *) iterator_get(&iter);
                size += dvalue_get_serialized_size(sub_val);
            }
            size += 2 * sizeof(size_t);
        } else {
            size += vector_byte_size(vec) + 2 * sizeof(size_t);
        }
    } else {
        log_error("Not support type(%d) of data value in sized", type);
        size = 0;
    }

    return size;
}

#define prim_val_serial_case(upcase_type, locase_type, locase_type_t) \
    case NEU_DTYPE_##upcase_type: {                                   \
        *(locase_type_t *) cur_ptr = val->val_##locase_type;          \
        size += sizeof(locase_type_t);                                \
        cur_ptr += sizeof(locase_type_t);                             \
        break;                                                        \
    }

#define prim_val_serial_case_t(upcase_type, locase_type) \
    prim_val_serial_case(upcase_type, locase_type, locase_type##_t)

static ssize_t do_dvalue_serialize(neu_data_val_t *val, uint8_t *buf)
{
    ssize_t     size;
    neu_dtype_e type;
    uint8_t *   cur_ptr;

    cur_ptr = buf;
    type    = val->type & (~(NEU_DTYPE_PTR_MASK | NEU_DTYPE_ATTR_MASK));
    *(neu_dtype_e *) cur_ptr = type;
    cur_ptr += sizeof(neu_dtype_e);
    size = sizeof(neu_dtype_e);
    if (type == neu_value_type_in_dtype(type)) {
        switch (type) {
            prim_val_serial_case_t(INT8, int8);
            prim_val_serial_case_t(INT16, int16);
            prim_val_serial_case_t(INT32, int32);
            prim_val_serial_case_t(INT64, int64);

            prim_val_serial_case_t(UINT8, uint8);
            prim_val_serial_case_t(UINT16, uint16);
            prim_val_serial_case_t(UINT32, uint32);
            prim_val_serial_case_t(UINT64, uint64);

            prim_val_serial_case(FLOAT, float, float);
            prim_val_serial_case(DOUBLE, double, double);

        case NEU_DTYPE_INT_VAL: {
            neu_int_val_t *int_val;
            int_val                           = (neu_int_val_t *) val->val_data;
            *(typeof(int_val->key) *) cur_ptr = int_val->key;
            cur_ptr += sizeof(int_val->key);
            size += sizeof(int_val->key);
            size_t val_size = do_dvalue_serialize(int_val->val, cur_ptr);
            cur_ptr += val_size;
            size += val_size;
            break;
        }

        case NEU_DTYPE_STRING_VAL: {
            neu_string_val_t *string_val;
            string_val      = (neu_string_val_t *) val->val_data;
            size_t key_size = neu_string_serialize(string_val->key, cur_ptr);
            cur_ptr += key_size;
            size += key_size;
            size_t val_size = do_dvalue_serialize(string_val->val, cur_ptr);
            cur_ptr += val_size;
            size += val_size;
            break;
        }

        case NEU_DTYPE_CSTR: {
            char *cstr;
            cstr       = (char *) val->val_data;
            size_t len = strlen(cstr) + 1;
            strcpy((char *) cur_ptr, cstr);
            cur_ptr += len;
            size += len;
            break;
        }

        case NEU_DTYPE_STRING: {
            neu_string_t *string;
            string          = (neu_string_t *) val->val_data;
            size_t str_size = neu_string_serialize(string, cur_ptr);
            cur_ptr += str_size;
            size += str_size;
            break;
        }

        case NEU_DTYPE_BYTES: {
            neu_bytes_t *bytes;
            bytes      = (neu_bytes_t *) val->val_data;
            size_t len = neu_bytes_size(bytes);
            neu_bytes_copy((neu_bytes_t *) cur_ptr, bytes);
            cur_ptr += len;
            size += len;
            break;
        }

        default:
            log_error("Not support type(%d) of data value in serialize", type);
            size = -1;
            break;
        }
    } else if (type & NEU_DTYPE_ARRAY) {
        neu_fixed_array_t *array;
        array = (neu_fixed_array_t *) val->val_data;
        if (NEU_DTYPE_DATA_VAL == neu_value_type_in_dtype(val->type)) {
            size_t index;
            *(size_t *) cur_ptr = array->length;
            size += sizeof(size_t);
            cur_ptr += sizeof(size_t);
            *(size_t *) cur_ptr = array->esize;
            size += sizeof(size_t);
            cur_ptr += sizeof(size_t);

            for (index = 0; index < array->length; index++) {
                neu_data_val_t *sub_val;
                ssize_t         sub_size;

                sub_val =
                    *(neu_data_val_t **) neu_fixed_array_get(array, index);
                sub_size = do_dvalue_serialize(sub_val, cur_ptr);
                if (sub_size < 0) {
                    size = sub_size;
                    break;
                }
                size += sub_size;
                cur_ptr += sub_size;
            }
        } else {
            *(size_t *) cur_ptr = array->length;
            size += sizeof(size_t);
            cur_ptr += sizeof(size_t);
            *(size_t *) cur_ptr = array->esize;
            size += sizeof(size_t);
            cur_ptr += sizeof(size_t);

            size_t buf_size;
            buf_size = array->length * array->esize;
            memcpy(cur_ptr, array->buf, buf_size);
            size += buf_size;
            cur_ptr += buf_size;
        }
    } else if (type & NEU_DTYPE_VEC) {
        vector_t *vec;
        vec = (vector_t *) val->val_data;
        if (NEU_DTYPE_DATA_VAL == neu_value_type_in_dtype(val->type)) {
            *(size_t *) cur_ptr = vec->size;
            size += sizeof(size_t);
            cur_ptr += sizeof(size_t);
            *(size_t *) cur_ptr = vec->element_size;
            size += sizeof(size_t);
            cur_ptr += sizeof(size_t);

            VECTOR_FOR_EACH(vec, iter)
            {
                neu_data_val_t *sub_val;
                ssize_t         sub_size;

                sub_val  = (neu_data_val_t *) iterator_get(&iter);
                sub_size = do_dvalue_serialize(sub_val, cur_ptr);
                if (sub_size < 0) {
                    size = sub_size;
                    break;
                }
                size += sub_size;
                cur_ptr += sub_size;
            }
        } else {
            *(size_t *) cur_ptr = vec->size;
            size += sizeof(size_t);
            cur_ptr += sizeof(size_t);
            *(size_t *) cur_ptr = vec->element_size;
            size += sizeof(size_t);
            cur_ptr += sizeof(size_t);

            size_t buf_size;
            buf_size = vector_byte_size(vec);
            memcpy(cur_ptr, vec->data, buf_size);
            size += buf_size;
            cur_ptr += buf_size;
        }
    } else {
        log_error("Not support type(%d) of data value in serialize", type);
        size = -1;
    }

    return size;
}

ssize_t neu_dvalue_serialize(neu_data_val_t *val, uint8_t **p_buf)
{
    assert(val != NULL && p_buf != NULL);
    if (val == NULL || p_buf == NULL) {
        log_error("Serialize the data value with NULL value or buf");
        return -1;
    }

    *p_buf = NULL;

    uint8_t *out_buf;
    ssize_t  size;

    size = dvalue_get_serialized_size(val);
    log_debug("type(0x%x) serialized size is %d", val->type, size);
    out_buf = (uint8_t *) malloc(size);
    if (out_buf == NULL) {
        log_error("Failed to allocate buffer for serialize value");
        return -1;
    }

    size = do_dvalue_serialize(val, out_buf);
    if (size < 0) {
        free(out_buf);
        return size;
    }

    *p_buf = out_buf;
    return size;
}

#define prim_val_deserial_case(upcase_type, locase_type, locase_type_t) \
    case NEU_DTYPE_##upcase_type: {                                     \
        locase_type_t prim_val = *(locase_type_t *) cur_ptr;            \
        size += sizeof(locase_type_t);                                  \
        cur_ptr += sizeof(locase_type_t);                               \
        neu_dvalue_init_##locase_type(val, prim_val);                   \
        break;                                                          \
    }

#define prim_val_deserial_case_t(upcase_type, locase_type) \
    prim_val_deserial_case(upcase_type, locase_type, locase_type##_t)

static ssize_t do_dvalue_deserialize(uint8_t *buf, size_t buf_len,
                                     neu_data_val_t *val)
{
    ssize_t     size;
    neu_dtype_e type;
    uint8_t *   cur_ptr;

    (void) buf_len;

    cur_ptr = buf;
    type    = *(neu_dtype_e *) cur_ptr;
    size    = sizeof(neu_dtype_e);
    cur_ptr += sizeof(neu_dtype_e);
    if (type == neu_value_type_in_dtype(type)) {
        switch (type) {
            prim_val_deserial_case_t(INT8, int8);
            prim_val_deserial_case_t(INT16, int16);
            prim_val_deserial_case_t(INT32, int32);
            prim_val_deserial_case_t(INT64, int64);

            prim_val_deserial_case_t(UINT8, uint8);
            prim_val_deserial_case_t(UINT16, uint16);
            prim_val_deserial_case_t(UINT32, uint32);
            prim_val_deserial_case_t(UINT64, uint64);

            prim_val_deserial_case(FLOAT, float, float);
            prim_val_deserial_case(DOUBLE, double, double);

        case NEU_DTYPE_INT_VAL: {
            ssize_t         sub_size;
            neu_data_val_t *sub_val;
            neu_int_val_t   int_val;

            int_val.key = *(typeof(int_val.key) *) cur_ptr;
            size += sizeof(int_val.key);
            cur_ptr += sizeof(int_val.key);
            sub_val  = neu_dvalue_unit_new();
            sub_size = do_dvalue_deserialize(cur_ptr, buf_len - size, sub_val);
            if (sub_size < 0) {
                size = sub_size;
                break;
            }
            int_val.val = sub_val;
            neu_dvalue_init_int_val(val, int_val);
            size += sub_size;
            cur_ptr += sub_size;
            break;
        }

        case NEU_DTYPE_STRING_VAL: {
            ssize_t          sub_size;
            neu_data_val_t * sub_val;
            neu_string_val_t string_val;

            size_t key_size = neu_string_deserialize(cur_ptr, &string_val.key);
            size += key_size;
            cur_ptr += key_size;
            sub_val  = neu_dvalue_unit_new();
            sub_size = do_dvalue_deserialize(cur_ptr, buf_len - size, sub_val);
            if (sub_size < 0) {
                size = sub_size;
                break;
            }
            string_val.val = sub_val;
            neu_dvalue_init_string_val(val, string_val);
            size += sub_size;
            cur_ptr += sub_size;
            break;
        }

        case NEU_DTYPE_CSTR: {
            size_t cstr_size = strlen((char *) cur_ptr) + 1;
            neu_dvalue_init_copy_cstr(val, (char *) cur_ptr);
            size += cstr_size;
            cur_ptr += cstr_size;
            break;
        }

        case NEU_DTYPE_STRING: {
            neu_string_t *string;
            size_t        str_size = neu_string_deserialize(cur_ptr, &string);
            neu_dvalue_init_move_string(val, string);
            size += str_size;
            cur_ptr += str_size;
            break;
        }

        case NEU_DTYPE_BYTES: {
            size_t bytes_size = *(size_t *) cur_ptr + sizeof(size_t);
            neu_dvalue_init_copy_bytes(val, (neu_bytes_t *) cur_ptr);
            size += bytes_size;
            cur_ptr += size;
            break;
        }

        default:
            log_error("Not support type(%d) of data value in deserialize",
                      type);
            size = -1;
            break;
        }
    } else if (type & NEU_DTYPE_ARRAY) {
        neu_fixed_array_t *array;
        neu_dtype_e        val_type;
        size_t             length, esize;

        length = *(size_t *) cur_ptr;
        size += sizeof(size_t);
        cur_ptr += sizeof(size_t);
        esize = *(size_t *) cur_ptr;
        size += sizeof(size_t);
        cur_ptr += sizeof(size_t);
        array    = neu_fixed_array_new(length, esize);
        val_type = neu_value_type_in_dtype(type);
        if (val_type == NEU_DTYPE_DATA_VAL) {
            size_t index;
            for (index = 0; index < array->length; index++) {
                ssize_t          sub_size;
                neu_data_val_t * sub_val;
                neu_data_val_t **item_ptr;

                sub_val = neu_dvalue_unit_new();
                sub_size =
                    do_dvalue_deserialize(cur_ptr, buf_len - size, sub_val);
                if (sub_size < 0) {
                    size = sub_size;
                    break;
                }
                item_ptr =
                    (neu_data_val_t **) neu_fixed_array_get(array, index);
                *item_ptr = sub_val;
                size += sub_size;
                cur_ptr += sub_size;
            }
            neu_dvalue_init_move_array(val, val_type, array);
        } else {
            size_t buf_size = array->length * array->esize;
            memcpy(array->buf, cur_ptr, buf_size);
            neu_dvalue_init_move_array(val, val_type, array);
            size += buf_size;
            cur_ptr += buf_size;
        }
    } else if (type & NEU_DTYPE_VEC) {
        vector_t *  vec;
        neu_dtype_e val_type;
        size_t      length, esize;

        length = *(size_t *) cur_ptr;
        size += sizeof(size_t);
        cur_ptr += sizeof(size_t);
        esize = *(size_t *) cur_ptr;
        size += sizeof(size_t);
        cur_ptr += sizeof(size_t);
        vec      = vector_new(length, esize);
        val_type = neu_value_type_in_dtype(type);
        if (val_type == NEU_DTYPE_DATA_VAL) {
            VECTOR_FOR_EACH(vec, iter)
            {
                ssize_t          sub_size;
                neu_data_val_t * sub_val;
                neu_data_val_t **item_ptr;

                sub_val = neu_dvalue_unit_new();
                sub_size =
                    do_dvalue_deserialize(cur_ptr, buf_len - size, sub_val);
                if (sub_size < 0) {
                    size = sub_size;
                    break;
                }
                item_ptr  = (neu_data_val_t **) iterator_get(&iter);
                *item_ptr = sub_val;
                size += sub_size;
                cur_ptr += sub_size;
            }
            neu_dvalue_init_move_vec(val, val_type, vec);
        } else {
            size_t buf_size = vector_byte_size(vec);
            memcpy(vec->data, cur_ptr, buf_size);
            neu_dvalue_init_move_vec(val, val_type, vec);
            size += buf_size;
            cur_ptr += buf_size;
        }
    } else {
        log_error("Not support type(%d) of data value in deserialize", type);
        size = -1;
    }
    return size;
}

ssize_t neu_dvalue_desialize(uint8_t *buf, size_t buf_len,
                             neu_data_val_t **p_val)
{
    ssize_t size;

    assert(buf != NULL && p_val != NULL);
    if (buf == NULL || p_val == NULL) {
        log_error("Deserialize the data value with NULL buf or value pointer");
        return -1;
    }

    if (buf_len == 0) {
        log_error("Deserialize the data value with 0 length buffer");
        return -1;
    }

    *p_val = NULL;

    neu_data_val_t *out_val;
    out_val = neu_dvalue_unit_new();
    if (out_val == NULL) {
        log_error("Failed to allocate datat value for deserialize value");
        return -1;
    }

    size = do_dvalue_deserialize(buf, buf_len, out_val);
    if (size > 0) {
        *p_val = out_val;
    } else {
        neu_dvalue_free(out_val);
    }
    return size;
}
