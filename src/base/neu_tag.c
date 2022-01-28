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

#include <assert.h>
#include <string.h>

#include "neu_tag.h"

bool neu_tag_check_attribute(neu_attribute_e attribute)
{
    return !((attribute & NEU_ATTRIBUTE_READ) == 0 &&
             (attribute & NEU_ATTRIBUTE_WRITE) == 0 &&
             (attribute & NEU_ATTRIBUTE_SUBSCRIBE) == 0);
}

neu_datatag_t *neu_datatag_alloc(neu_attribute_e attr, neu_dtype_e type,
                                 neu_addr_str_t addr, const char *name)
{
    neu_datatag_t *tag = malloc(sizeof(neu_datatag_t));

    if (NULL == tag) {
        return NULL;
    }

    tag->id        = 0;
    tag->attribute = attr;
    tag->type      = type;

    tag->addr_str = strdup(addr);
    if (NULL == tag->addr_str) {
        goto error;
    }

    tag->name = strdup(name);
    if (NULL == tag->name) {
        goto error;
    }

    return tag;

error:
    free(tag->addr_str);
    free(tag->name);
    free(tag);
    return NULL;
}

void neu_datatag_free(neu_datatag_t *datatag)
{
    if (datatag) {
        free(datatag->addr_str);
        free(datatag->name);
    }
    free(datatag);
}

int neu_datatag_parse_addr_option(neu_datatag_t *            datatag,
                                  neu_datatag_addr_option_u *option)
{
    int ret = 0;

    switch (datatag->type) {
    case NEU_DTYPE_CSTR: {
        char *op = strchr(datatag->addr_str, '.');

        if (op == NULL) {
            ret = -1;
        } else {
            char t = 0;
            int  n = sscanf(op, ".%hd%c", &option->string.length, &t);

            switch (t) {
            case 'H':
                option->string.type = NEU_DATATAG_STRING_TYPE_H;
                break;
            case 'L':
                option->string.type = NEU_DATATAG_STRING_TYPE_L;
                break;
            case 'D':
                option->string.type = NEU_DATATAG_STRING_TYPE_D;
                break;
            case 'E':
                option->string.type = NEU_DATATAG_STRING_TYPE_D;
                break;
            default:
                option->string.type = NEU_DATATAG_STRING_TYPE_H;
                break;
            }

            if (n < 1 || option->string.length <= 0) {
                ret = -1;
            }
        }

        break;
    }
    case NEU_DTYPE_INT16:
    case NEU_DTYPE_UINT16: {
        char *op = strchr(datatag->addr_str, '#');

        option->value16.endian = NEU_DATATAG_ENDIAN_L16;
        if (op != NULL) {
            char e = 0;
            sscanf(op, "#%c", &e);

            switch (e) {
            case 'B':
                option->value16.endian = NEU_DATATAG_ENDIAN_B16;
                break;
            case 'L':
                option->value16.endian = NEU_DATATAG_ENDIAN_L16;
                break;
            default:
                option->value16.endian = NEU_DATATAG_ENDIAN_L16;
                break;
            }
        }

        break;
    }
    case NEU_DTYPE_FLOAT:
    case NEU_DTYPE_UINT32:
    case NEU_DTYPE_INT32: {
        char *op = strchr(datatag->addr_str, '#');

        option->value32.endian = NEU_DATATAG_ENDIAN_LL32;
        if (op != NULL) {
            char e1 = 0;
            char e2 = 0;
            int  n  = sscanf(op, "#%c%c", &e1, &e2);

            if (n == 2) {
                if (e1 == 'B' && e2 == 'B') {
                    option->value32.endian = NEU_DATATAG_ENDIAN_BB32;
                }
                if (e1 == 'B' && e2 == 'L') {
                    option->value32.endian = NEU_DATATAG_ENDIAN_BL32;
                }
                if (e1 == 'L' && e2 == 'L') {
                    option->value32.endian = NEU_DATATAG_ENDIAN_LL32;
                }
                if (e1 == 'L' && e2 == 'B') {
                    option->value32.endian = NEU_DATATAG_ENDIAN_LB32;
                }
            }
        }

        break;
    }
    case NEU_DTYPE_BOOL: {
        char *op = strchr(datatag->addr_str, '.');

        if (op != NULL) {
            sscanf(op, ".%hhd", &option->boolean.bit);
        }

        break;
    }
    default:
        break;
    }

    return ret;
}

neu_data_val_t *neu_datatag_pack_create(int size)
{
    neu_data_val_t *   val   = neu_dvalue_unit_new();
    neu_fixed_array_t *array = neu_fixed_array_new(size, sizeof(neu_int_val_t));

    neu_dvalue_init_move_array(val, NEU_DTYPE_INT_VAL, array);

    return val;
}

int neu_datatag_pack_add(neu_data_val_t *val, uint16_t index, neu_dtype_e type,
                         uint32_t key, void *data)
{
    neu_fixed_array_t *array = NULL;
    int                ret   = 0;
    neu_int_val_t      elem  = { 0 };
    neu_data_val_t *   tv    = NULL;

    neu_dvalue_get_ref_array(val, &array);
    assert(neu_fixed_array_size(array) > index);

    switch (type) {
    case NEU_DTYPE_ERRORCODE:
        tv = neu_dvalue_new(NEU_DTYPE_ERRORCODE);
        neu_dvalue_set_errorcode(tv, *(int32_t *) data);
        break;
    case NEU_DTYPE_INT8:
        tv = neu_dvalue_new(NEU_DTYPE_INT8);
        neu_dvalue_set_int8(tv, *(int8_t *) data);
        break;
    case NEU_DTYPE_UINT8:
        tv = neu_dvalue_new(NEU_DTYPE_UINT8);
        neu_dvalue_set_uint8(tv, *(uint8_t *) data);
        break;
    case NEU_DTYPE_INT16:
        tv = neu_dvalue_new(NEU_DTYPE_INT16);
        neu_dvalue_set_int16(tv, *(int16_t *) data);
        break;
    case NEU_DTYPE_UINT16:
        tv = neu_dvalue_new(NEU_DTYPE_UINT16);
        neu_dvalue_set_uint16(tv, *(uint16_t *) data);
        break;
    case NEU_DTYPE_INT32:
        tv = neu_dvalue_new(NEU_DTYPE_INT32);
        neu_dvalue_set_int32(tv, *(int32_t *) data);
        break;
    case NEU_DTYPE_UINT32:
        tv = neu_dvalue_new(NEU_DTYPE_UINT32);
        neu_dvalue_set_uint32(tv, *(uint32_t *) data);
        break;
    case NEU_DTYPE_INT64:
        tv = neu_dvalue_new(NEU_DTYPE_INT64);
        neu_dvalue_set_int64(tv, *(int64_t *) data);
        break;
    case NEU_DTYPE_UINT64:
        tv = neu_dvalue_new(NEU_DTYPE_UINT64);
        neu_dvalue_set_uint64(tv, *(uint64_t *) data);
        break;
    case NEU_DTYPE_FLOAT:
        tv = neu_dvalue_new(NEU_DTYPE_FLOAT);
        neu_dvalue_set_float(tv, *(float *) data);
        break;
    case NEU_DTYPE_DOUBLE:
        tv = neu_dvalue_new(NEU_DTYPE_DOUBLE);
        neu_dvalue_set_double(tv, *(double *) data);
        break;
    case NEU_DTYPE_BOOL:
        tv = neu_dvalue_new(NEU_DTYPE_BOOL);
        neu_dvalue_set_bool(tv, *(bool *) data);
        break;
    case NEU_DTYPE_CSTR:
        tv = neu_dvalue_new(NEU_DTYPE_CSTR);
        neu_dvalue_set_cstr(tv, (char *) data);
        break;
    case NEU_DTYPE_BYTES:
        tv = neu_dvalue_new(NEU_DTYPE_BYTES);
        neu_dvalue_set_bytes(tv, (neu_bytes_t *) data);
        break;
    default:
        ret = -1;
        break;
    }

    if (ret == 0) {
        neu_int_val_init(&elem, key, tv);
        neu_fixed_array_set(array, index, (void *) &elem);
    }

    return ret;
}