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

#ifndef _NEU_TAG_H_
#define _NEU_TAG_H_

#include "type.h"

typedef enum {
    NEU_TAG_ENDIAN_L16  = 0, // #L
    NEU_TAG_ENDIAN_B16  = 1, // #B
    NEU_TAG_ENDIAN_LL32 = 2, // #LL
    NEU_TAG_ENDIAN_LB32 = 3, // #LB
    NEU_TAG_ENDIAN_BB32 = 4, // #BB
    NEU_TAG_ENDIAN_BL32 = 5, // #BL
} neu_tag_endian_e;

typedef enum {
    NEU_TAG_STRING_TYPE_H = 0, // high-to-low endian
    NEU_TAG_STRING_TYPE_L = 1, // low-to-high endian
    NEU_TAG_STRING_TYPE_D = 2, // a high byte is stored in an int16
    NEU_TAG_STRING_TYPE_E = 3, // a low byte is stored in an int16
} neu_tag_string_type_e;

typedef union {
    struct {
        neu_tag_endian_e endian;
    } value16;
    struct {
        neu_tag_endian_e endian;
    } value32;
    struct {
        uint16_t              length;
        neu_tag_string_type_e type;
    } string;

    struct {
        uint8_t bit;
    } bit;
} neu_tag_addr_option_u;

typedef enum {
    NEU_TAG_ATTR_READ      = 1,
    NEU_TAG_ATTR_WRITE     = 2,
    NEU_TAG_ATTR_SUBSCRIBE = 4,
} neu_tag_attr_e;

#define NEU_TAG_NAME_LEN 64
#define NEU_TAG_ADDRESS_LEN 64

typedef struct {
    char name[NEU_TAG_NAME_LEN];
    char address[NEU_TAG_ADDRESS_LEN];

    neu_type_e            type;
    neu_tag_attr_e        attribute;
    neu_tag_addr_option_u option;
} neu_tag_t;

#endif