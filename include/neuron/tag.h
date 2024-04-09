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

#include "define.h"
#include "type.h"
#include "utils/utextend.h"
#include "json/json.h"

typedef enum {
    NEU_ATTRIBUTE_READ      = 1,
    NEU_ATTRIBUTE_WRITE     = 2,
    NEU_ATTRIBUTE_SUBSCRIBE = 4,
    NEU_ATTRIBUTE_STATIC    = 8,
} neu_attribute_e;

typedef enum {
    NEU_DATATAG_ENDIAN_L16  = 0, // #L  2,1
    NEU_DATATAG_ENDIAN_B16  = 1, // #B  1,2
    NEU_DATATAG_ENDIAN_LL32 = 2, // #LL 4,3,2,1
    NEU_DATATAG_ENDIAN_LB32 = 3, // #LB 3,4,1,2
    NEU_DATATAG_ENDIAN_BB32 = 4, // #BB 1,2,3,4
    NEU_DATATAG_ENDIAN_BL32 = 5, // #BL 2,1,4,3
    NEU_DATATAG_ENDIAN_L64  = 6, // #L  8,7,6,5,4,3,2,1
    NEU_DATATAG_ENDIAN_B64  = 7, // #B  1,2,3,4,5,6,7,8
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
        bool                 is_default;
    } value32;
    struct {
        neu_datatag_endian_e endian;
    } value64;
    struct {
        uint16_t                  length;
        neu_datatag_string_type_e type;
        bool                      is_default;
    } string;

    struct {
        uint8_t length;
    } bytes;

    struct {
        bool    op;
        uint8_t bit;
    } bit;
} neu_datatag_addr_option_u;

typedef struct {
    char *                    name;
    char *                    address;
    neu_attribute_e           attribute;
    neu_type_e                type;
    uint8_t                   precision;
    double                    decimal;
    char *                    description;
    neu_datatag_addr_option_u option;
    uint8_t                   meta[NEU_TAG_META_LENGTH];
} neu_datatag_t;

typedef struct neu_tag_meta {
    char         name[NEU_TAG_NAME_LEN];
    neu_dvalue_t value;
} neu_tag_meta_t;

UT_icd *neu_tag_get_icd();

neu_datatag_t *neu_tag_dup(const neu_datatag_t *tag);
void           neu_tag_copy(neu_datatag_t *tag, const neu_datatag_t *other);
void           neu_tag_fini(neu_datatag_t *tag);
void           neu_tag_free(neu_datatag_t *tag);

inline static bool neu_tag_attribute_test(const neu_datatag_t *tag,
                                          neu_attribute_e      attribute)
{
    return (tag->attribute & attribute) == attribute;
}

/**
 * @brief Special usage of parsing tag address, e.g. setting length of string
 * type, setting of endian.
 *
 * @param[in] datatag contains all the information about the tag.
 * @param[in] option contain actions that the label can perform.
 * @return  0 on success, -1 on failure.
 */
int neu_datatag_parse_addr_option(const neu_datatag_t *      datatag,
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

int neu_tag_get_static_value(const neu_datatag_t *tag, neu_value_u *value);
int neu_tag_set_static_value(neu_datatag_t *tag, const neu_value_u *value);

int neu_tag_get_static_value_json(neu_datatag_t *tag, neu_json_type_e *t,
                                  neu_json_value_u *v);
int neu_tag_set_static_value_json(neu_datatag_t *tag, neu_json_type_e t,
                                  const neu_json_value_u *v);

char *neu_tag_dump_static_value(const neu_datatag_t *tag);
int   neu_tag_load_static_value(neu_datatag_t *tag, const char *s);

typedef struct {
    char *          name;
    char *          address;
    neu_attribute_e attribute;
    neu_type_e      type;
    char *          params;
} neu_ndriver_tag_t;

UT_icd *neu_ndriver_tag_get_icd();

neu_datatag_t *neu_ndriver_tag_dup(const neu_ndriver_tag_t *tag);
void           neu_ndriver_tag_copy(neu_ndriver_tag_t *      tag,
                                    const neu_ndriver_tag_t *other);
void           neu_ndriver_tag_fini(neu_ndriver_tag_t *tag);
void           neu_ndriver_tag_free(neu_ndriver_tag_t *tag);

#endif
