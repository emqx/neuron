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
config_ **/

#ifndef NEURON_TAG_H
#define NEURON_TAG_H

#include <stdbool.h>
#include <stdint.h>

#include "data_expr.h"

#define NEU_TAG_NAME_SIZE 128
#define NEU_TAG_ADDRESS_SIZE 128

typedef uint32_t neu_datatag_id_t;
typedef char *   neu_addr_str_t;
typedef char *   neu_tag_name;

typedef enum {
    NEU_ATTRIBUTE_READ      = 1,
    NEU_ATTRIBUTE_WRITE     = 2,
    NEU_ATTRIBUTE_SUBSCRIBE = 4,
} neu_attribute_e;

typedef struct {
    neu_datatag_id_t id;
    neu_attribute_e  attribute;
    neu_dtype_e      type;
    neu_addr_str_t   addr_str;
    neu_tag_name     name;
} neu_datatag_t;

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

bool neu_tag_check_attribute(neu_attribute_e attribute);
// The id is set to zero, and we make a copy of the name.
neu_datatag_t *neu_datatag_alloc(neu_attribute_e attr, neu_dtype_e type,
                                 neu_addr_str_t addr, const char *name);
// The id is set to the value passed in, and we make a copy of the name.
neu_datatag_t *neu_datatag_alloc_with_id(neu_attribute_e attr, neu_dtype_e type,
                                         neu_addr_str_t addr, const char *name,
                                         neu_datatag_id_t id);
void           neu_datatag_free(neu_datatag_t *datatag);

int neu_datatag_parse_addr_option(neu_datatag_t *            datatag,
                                  neu_datatag_addr_option_u *option);

typedef void (*neu_datatag_write_value_unpack_callback)(void *   data,
                                                        uint16_t index,
                                                        uint32_t id,
                                                        void *   value);

void neu_datatag_unpack(neu_data_val_t *req_val, void *data,
                        neu_datatag_write_value_unpack_callback fn);

neu_data_val_t *neu_datatag_pack_create(int size);
int  neu_datatag_pack_add(neu_data_val_t *val, uint16_t index, neu_dtype_e type,
                          uint32_t key, void *data);
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