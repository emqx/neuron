/**
 * NEURON IIoT System for Industry 4.0
 * Copyright (C) 2020-2022 EMQ Technologies Co., Ltd All rights reserved.
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
config_ **/

#ifndef NEURON_TAG_H
#define NEURON_TAG_H

#include <stdbool.h>
#include <stdint.h>

#include "utils/utextend.h"

#define NEU_TAG_ADDRESS_SIZE 128

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

typedef enum {
    NEU_ATTRIBUTE_READ      = 1,
    NEU_ATTRIBUTE_WRITE     = 2,
    NEU_ATTRIBUTE_SUBSCRIBE = 4,
} neu_attribute_e;

typedef struct {
    char *          name;
    char *          addr_str;
    char *          description;
    neu_attribute_e attribute;
    neu_dtype_e     type;
} neu_datatag_t;

UT_icd *neu_tag_get_icd();

typedef enum {
    NEU_DATATAG_ENDIAN_L16  = 0, // #L
    NEU_DATATAG_ENDIAN_B16  = 1, // #B
    NEU_DATATAG_ENDIAN_LL32 = 2, // #LL
    NEU_DATATAG_ENDIAN_LB32 = 3, // #LB
    NEU_DATATAG_ENDIAN_BB32 = 4, // #BB
    NEU_DATATAG_ENDIAN_BL32 = 5, // #BL
} neu_datatag_endian_e;

typedef enum {
    NEU_DATATAG_STRING_TYPE_H = 0, // high-to-low endian
    NEU_DATATAG_STRING_TYPE_L = 1, // low-to-high endian
    NEU_DATATAG_STRING_TYPE_D = 2, // a high byte is stored in an int16
    NEU_DATATAG_STRING_TYPE_E = 3, // a low byte is stored in an int16
} neu_datatag_string_type_e;

typedef union {
    struct {
        neu_datatag_endian_e endian;
    } value16;
    struct {
        neu_datatag_endian_e endian;
    } value32;
    struct {
        uint16_t                  length;
        neu_datatag_string_type_e type;
    } string;

    struct {
        uint8_t bit;
    } bit;
} neu_datatag_addr_option_u;

int neu_datatag_parse_addr_option(neu_datatag_t *            datatag,
                                  neu_datatag_addr_option_u *option);

bool neu_datatag_string_is_utf8(char *data, int len);

int neu_datatag_string_htol(char *str, int len);
int neu_datatag_string_ltoh(char *str, int len);
int neu_datatag_string_etod(char *str, int len);
int neu_datatag_string_dtoe(char *str, int len);
int neu_datatag_string_etoh(char *str, int len);
int neu_datatag_string_dtoh(char *str, int len);
int neu_datatag_string_tod(char *str, int len, int buf_len);
int neu_datatag_string_toe(char *str, int len, int buf_len);

#endif