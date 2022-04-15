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

#include "errcodes.h"
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

neu_datatag_t *neu_datatag_alloc_with_id(neu_attribute_e attr, neu_dtype_e type,
                                         neu_addr_str_t addr, const char *name,
                                         neu_datatag_id_t id)
{
    neu_datatag_t *tag = malloc(sizeof(neu_datatag_t));

    if (NULL == tag) {
        return NULL;
    }

    tag->id        = id;
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

static char *find_last_character(char *str, char character)
{
    char *find = strchr(str, character);
    char *ret  = find;

    while (find != NULL) {
        ret  = find;
        find = strchr(find + 1, character);
    }

    return ret;
}

int neu_datatag_parse_addr_option(neu_datatag_t *            datatag,
                                  neu_datatag_addr_option_u *option)
{
    int ret = 0;

    switch (datatag->type) {
    case NEU_DTYPE_CSTR: {
        char *op = find_last_character(datatag->addr_str, '.');

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
        char *op = find_last_character(datatag->addr_str, '#');

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
        char *op = find_last_character(datatag->addr_str, '#');

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
    case NEU_DTYPE_BIT: {
        char *op = find_last_character(datatag->addr_str, '.');

        if (op != NULL) {
            sscanf(op, ".%hhd", &option->bit.bit);
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
    case NEU_DTYPE_BIT:
        tv = neu_dvalue_new(NEU_DTYPE_BIT);
        neu_dvalue_set_bit(tv, *(uint8_t *) data);
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

static int pre_num(unsigned char byte)
{
    unsigned char mask = 0x80;
    int           num  = 0;
    for (int i = 0; i < 8; i++) {
        if ((byte & mask) == mask) {
            mask = mask >> 1;
            num++;
        } else {
            break;
        }
    }
    return num;
}

bool neu_datatag_string_is_utf8(char *data, int len)
{
    int num = 0;
    int i   = 0;

    while (i < len) {
        if ((data[i] & 0x80) == 0x00) {
            // 0XXX_XXXX
            i++;
            continue;
        } else if ((num = pre_num(data[i])) > 2) {
            // 110X_XXXX 10XX_XXXX
            // 1110_XXXX 10XX_XXXX 10XX_XXXX
            // 1111_0XXX 10XX_XXXX 10XX_XXXX 10XX_XXXX
            // 1111_10XX 10XX_XXXX 10XX_XXXX 10XX_XXXX 10XX_XXXX
            // 1111_110X 10XX_XXXX 10XX_XXXX 10XX_XXXX 10XX_XXXX 10XX_XXXX
            i++;
            for (int j = 0; j < num - 1; j++) {
                if ((data[i] & 0xc0) != 0x80) {
                    return false;
                }
                i++;
            }
        } else {
            return false;
        }
    }
    return true;
}

int neu_datatag_string_htol(char *str, int len)
{
    char t = 0;

    for (int i = 0; i < len; i += 2) {
        t          = str[i];
        str[i]     = str[i + 1];
        str[i + 1] = t;
    }

    return len;
}

int neu_datatag_string_ltoh(char *str, int len)
{
    return neu_datatag_string_htol(str, len);
}

int neu_datatag_string_etod(char *str, int len)
{
    for (int i = 0; i < len; i += 2) {
        str[i + 1] = str[i];
        str[i]     = 0;
    }

    return len;
}

int neu_datatag_string_dtoe(char *str, int len)
{
    for (int i = 0; i < len; i += 2) {
        str[i]     = str[i + 1];
        str[i + 1] = 0;
    }

    return len;
}

int neu_datatag_string_etoh(char *str, int len)
{
    char *t = calloc(len, sizeof(char));

    for (int i = 0; i < len; i++) {
        t[i] = str[i * 2];
    }
    memset(str, 0, len);
    strncpy(str, t, strlen(str));

    free(t);
    return len / 2;
}

int neu_datatag_string_dtoh(char *str, int len)
{
    char *t = calloc(len, sizeof(char));

    for (int i = 0; i < len; i++) {
        t[i] = str[i * 2 + 1];
    }
    memset(str, 0, len);
    strncpy(str, t, strlen(str));

    free(t);
    return len / 2;
}

int neu_datatag_string_tod(char *str, int len, int buf_len)
{
    assert(len * 2 < len);
    char *t = strdup(str);

    memset(str, 0, buf_len);
    for (int i = 0; i < len; i++) {
        str[i * 2 + 1] = t[i];
    }

    free(t);
    return len * 2;
}

int neu_datatag_string_toe(char *str, int len, int buf_len)
{
    assert(len * 2 < len);
    char *t = strdup(str);

    memset(str, 0, buf_len);
    for (int i = 0; i < len; i++) {
        str[i * 2] = t[i];
    }

    free(t);
    return len * 2;
}

void neu_datatag_unpack(neu_data_val_t *req_val, void *data,
                        neu_datatag_write_value_unpack_callback fn)
{
    neu_fixed_array_t *req_array = NULL;

    neu_dvalue_get_ref_array(req_val, &req_array);

    for (uint32_t i = 0; i < req_array->length; i++) {
        neu_int_val_t *int_val = neu_fixed_array_get(req_array, i);
        neu_dtype_e    type =
            neu_value_type_in_dtype(neu_dvalue_get_type(int_val->val));

        switch (type) {
        case NEU_DTYPE_INT8: {
            int8_t value = 0;

            neu_dvalue_get_int8(int_val->val, &value);
            fn(data, i, int_val->key, &value);
            break;
        }
        case NEU_DTYPE_UINT8: {
            uint8_t value = 0;

            neu_dvalue_get_uint8(int_val->val, &value);
            fn(data, i, int_val->key, &value);
            break;
        }
        case NEU_DTYPE_INT16: {
            int16_t value = 0;

            neu_dvalue_get_int16(int_val->val, &value);
            fn(data, i, int_val->key, &value);
            break;
        }
        case NEU_DTYPE_UINT16: {
            uint16_t value = 0;

            neu_dvalue_get_uint16(int_val->val, &value);
            fn(data, i, int_val->key, &value);
            break;
        }
        case NEU_DTYPE_UINT32: {
            uint32_t value = 0;

            neu_dvalue_get_uint32(int_val->val, &value);
            fn(data, i, int_val->key, &value);
            break;
        }
        case NEU_DTYPE_INT32: {
            int32_t value = 0;

            neu_dvalue_get_int32(int_val->val, &value);
            fn(data, i, int_val->key, &value);
            break;
        }
        case NEU_DTYPE_INT64: {
            int64_t value = 0;

            neu_dvalue_get_int64(int_val->val, &value);
            fn(data, i, int_val->key, &value);
            break;
        }
        case NEU_DTYPE_UINT64: {
            uint64_t value = 0;

            neu_dvalue_get_uint64(int_val->val, &value);
            fn(data, i, int_val->key, &value);
            break;
        }
        case NEU_DTYPE_FLOAT: {
            float value = 0;

            neu_dvalue_get_float(int_val->val, &value);
            fn(data, i, int_val->key, &value);
            break;
        }
        case NEU_DTYPE_DOUBLE: {
            double value = 0;

            neu_dvalue_get_double(int_val->val, &value);
            fn(data, i, int_val->key, &value);
            break;
        }
        case NEU_DTYPE_BIT: {
            uint8_t value = 0;

            neu_dvalue_get_bit(int_val->val, &value);
            fn(data, i, int_val->key, &value);
            break;
        }
        case NEU_DTYPE_BOOL: {
            bool value = false;

            neu_dvalue_get_bool(int_val->val, &value);
            fn(data, i, int_val->key, &value);
            break;
        }
        case NEU_DTYPE_CSTR: {
            char *value = NULL;

            neu_dvalue_get_cstr(int_val->val, &value);
            fn(data, i, int_val->key, value);
            free(value);
            break;
        }
        default:
            assert(1 == 0);
            break;
        }
    }
}