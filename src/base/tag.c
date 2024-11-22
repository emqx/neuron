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

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <jansson.h>

#include "errcodes.h"
#include "tag.h"

static void tag_array_copy(void *_dst, const void *_src)
{
    neu_datatag_t *dst = (neu_datatag_t *) _dst;
    neu_datatag_t *src = (neu_datatag_t *) _src;

    dst->type        = src->type;
    dst->attribute   = src->attribute;
    dst->precision   = src->precision;
    dst->decimal     = src->decimal;
    dst->bias        = src->bias;
    dst->option      = src->option;
    dst->address     = strdup(src->address);
    dst->name        = strdup(src->name);
    dst->description = strdup(src->description);

    memcpy(dst->format, src->format, sizeof(src->format));
    dst->n_format = src->n_format;
    memcpy(dst->meta, src->meta, sizeof(src->meta));
}

static void tag_array_free(void *_elt)
{
    neu_datatag_t *elt = (neu_datatag_t *) _elt;

    free(elt->name);
    free(elt->address);
    free(elt->description);
}

void neu_tag_format_str(const neu_datatag_t *tag, char *buf, int len)
{
    int offset = 0;

    for (int i = 0; i < tag->n_format; i++) {
        if (i == 0) {
            offset = snprintf(buf, len, "%d", (int) tag->format[i]);
        } else {
            offset += snprintf(buf + offset, len - offset, ",%d",
                               (int) tag->format[i]);
        }
    }
}

int neu_format_from_str(const char *format_str, uint8_t *formats)
{
    int n = 0;

    if (format_str == NULL || strlen(format_str) == 0) {
        return n;
    }

    n = sscanf(format_str,
               "%hhu,%hhu,%hhu,%hhu,%hhu,%hhu,%hhu,%hhu,%hhu,%hhu,%hhu,%hhu,"
               "%hhu,%hhu,%hhu,%hhu",
               &formats[0], &formats[1], &formats[2], &formats[3], &formats[4],
               &formats[5], &formats[6], &formats[7], &formats[8], &formats[9],
               &formats[10], &formats[11], &formats[12], &formats[13],
               &formats[14], &formats[15]);

    return n;
}

static UT_icd tag_icd = { sizeof(neu_datatag_t), NULL, tag_array_copy,
                          tag_array_free };

UT_icd *neu_tag_get_icd()
{
    return &tag_icd;
}

neu_datatag_t *neu_tag_dup(const neu_datatag_t *tag)
{
    neu_datatag_t *new = calloc(1, sizeof(*new));
    tag_array_copy(new, tag);
    return new;
}

void neu_tag_copy(neu_datatag_t *tag, const neu_datatag_t *other)
{
    if (tag) {
        tag_array_free(tag);
        tag_array_copy(tag, other);
    }
}

void neu_tag_fini(neu_datatag_t *tag)
{
    if (tag) {
        tag_array_free(tag);
    }
}

void neu_tag_free(neu_datatag_t *tag)
{
    if (tag) {
        tag_array_free(tag);
        free(tag);
    }
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

int neu_datatag_parse_addr_option(const neu_datatag_t *      datatag,
                                  neu_datatag_addr_option_u *option)
{
    int ret = 0;

    switch (datatag->type) {
    case NEU_TYPE_BYTES: {
        char *op = find_last_character(datatag->address, '.');

        if (op == NULL) {
            ret = -1;
        } else {
            int n = sscanf(op, ".%hhd", &option->bytes.length);
            if (n != 1 || option->string.length <= 0) {
                ret = -1;
            }
        }
        break;
    }
    case NEU_TYPE_STRING: {
        char *op = find_last_character(datatag->address, '.');

        if (op == NULL) {
            ret = -1;
        } else {
            char t = 0;
            int  n = sscanf(op, ".%hd%c", &option->string.length, &t);

            switch (t) {
            case 'H':
                option->string.type       = NEU_DATATAG_STRING_TYPE_H;
                option->string.is_default = false;
                break;
            case 'L':
                option->string.type       = NEU_DATATAG_STRING_TYPE_L;
                option->string.is_default = false;
                break;
            case 'D':
                option->string.type       = NEU_DATATAG_STRING_TYPE_D;
                option->string.is_default = false;
                break;
            case 'E':
                option->string.type       = NEU_DATATAG_STRING_TYPE_E;
                option->string.is_default = false;
                break;
            default:
                option->string.type       = NEU_DATATAG_STRING_TYPE_H;
                option->string.is_default = true;
                break;
            }

            if (n < 1 || option->string.length <= 0) {
                ret = -1;
            }
        }

        break;
    }
    case NEU_TYPE_INT16:
    case NEU_TYPE_UINT16: {
        char *op = find_last_character(datatag->address, '#');

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
    case NEU_TYPE_FLOAT:
    case NEU_TYPE_INT32:
    case NEU_TYPE_UINT32:
    case NEU_TYPE_DATA_AND_TIME:
    case NEU_TYPE_TIME: {
        char *op = find_last_character(datatag->address, '#');

        option->value32.endian     = NEU_DATATAG_ENDIAN_LL32;
        option->value32.is_default = true;
        if (op != NULL) {
            char e1 = 0;
            char e2 = 0;
            int  n  = sscanf(op, "#%c%c", &e1, &e2);

            if (n == 2) {
                if (e1 == 'B' && e2 == 'B') {
                    option->value32.endian     = NEU_DATATAG_ENDIAN_BB32;
                    option->value32.is_default = false;
                }
                if (e1 == 'B' && e2 == 'L') {
                    option->value32.endian     = NEU_DATATAG_ENDIAN_BL32;
                    option->value32.is_default = false;
                }
                if (e1 == 'L' && e2 == 'L') {
                    option->value32.endian     = NEU_DATATAG_ENDIAN_LL32;
                    option->value32.is_default = false;
                }
                if (e1 == 'L' && e2 == 'B') {
                    option->value32.endian     = NEU_DATATAG_ENDIAN_LB32;
                    option->value32.is_default = false;
                }
            }
        }

        break;
    }
    case NEU_TYPE_DOUBLE:
    case NEU_TYPE_INT64:
    case NEU_TYPE_UINT64: {
        char *op = find_last_character(datatag->address, '#');

        option->value64.endian = NEU_DATATAG_ENDIAN_L64;
        if (op != NULL) {
            char e = 0;
            sscanf(op, "#%c", &e);

            switch (e) {
            case 'B':
                option->value64.endian = NEU_DATATAG_ENDIAN_B64;
                break;
            case 'L':
                option->value64.endian = NEU_DATATAG_ENDIAN_L64;
                break;
            default:
                option->value64.endian = NEU_DATATAG_ENDIAN_L64;
                break;
            }
        }

        break;
    }

    case NEU_TYPE_BIT: {
        char *op = find_last_character(datatag->address, '.');

        if (op != NULL) {
            sscanf(op, ".%hhd", &option->bit.bit);
            option->bit.op = true;
        } else {
            option->bit.op = false;
        }

        break;
    }
    default:
        break;
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
        } else if ((num = pre_num(data[i])) > 1) {
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

    for (int i = 0; i < len; i += 2) {
        char t = str[i];

        str[i]     = str[i + 1];
        str[i + 1] = t;
    }

    return len;
}

int neu_datatag_string_ltoh(char *str, int len)
{
    return neu_datatag_string_htol(str, len);
}

/*int neu_datatag_string_etod(char *str, int len)
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
}*/