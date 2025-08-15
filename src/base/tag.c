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

/**
 * @brief 从标签的meta字段获取静态值指针
 *
 * @param tag 标签结构体
 * @param out 输出的值指针
 */
#define GET_STATIC_VALUE_PTR(tag, out)                      \
    do {                                                    \
        memcpy(&(out), (tag)->meta, sizeof(neu_value_u *)); \
    } while (0)

/**
 * @brief 设置标签的静态值指针到meta字段
 *
 * @param tag 标签结构体
 * @param ptr 值指针
 */
#define SET_STATIC_VALUE_PTR(tag, ptr)                      \
    do {                                                    \
        memcpy((tag)->meta, &(ptr), sizeof(neu_value_u *)); \
    } while (0)

/**
 * @brief 复制标签数组元素
 *
 * 用于UT_array的复制回调函数，深度复制标签结构体
 *
 * @param _dst 目标指针
 * @param _src 源指针
 */
static void tag_array_copy(void *_dst, const void *_src)
{
    neu_datatag_t *dst = (neu_datatag_t *) _dst;
    neu_datatag_t *src = (neu_datatag_t *) _src;

    /* 复制基本属性 */
    dst->type        = src->type;                /* 数据类型 */
    dst->attribute   = src->attribute;           /* 标签属性 */
    dst->precision   = src->precision;           /* 精度 */
    dst->decimal     = src->decimal;             /* 小数位数 */
    dst->option      = src->option;              /* 选项 */
    dst->address     = strdup(src->address);     /* 复制地址字符串 */
    dst->name        = strdup(src->name);        /* 复制名称字符串 */
    dst->description = strdup(src->description); /* 复制描述字符串 */

    /* 处理静态标签特有的值 */
    if (NEU_ATTRIBUTE_STATIC & src->attribute) {
        neu_value_u *dst_val = NULL, *src_val = NULL;
        GET_STATIC_VALUE_PTR(src, src_val); /* 获取源标签的静态值指针 */
        if (src_val && (dst_val = calloc(1, sizeof(*dst_val)))) {
            /* 分配并复制静态值内存 */
            memcpy(dst_val, src_val, sizeof(*dst_val));
            SET_STATIC_VALUE_PTR(dst, dst_val); /* 设置目标标签的静态值指针 */
        } else {
            /* 内存分配失败或源值为空，清零meta字段 */
            memset(dst->meta, 0, sizeof(dst->meta));
        }
    } else {
        /* 非静态标签，直接复制meta字段 */
        memcpy(dst->meta, src->meta, sizeof(src->meta));
    }
}

/**
 * @brief 释放标签数组元素
 *
 * 用于UT_array的释放回调函数，释放标签结构体中的动态分配内存
 *
 * @param _elt 要释放的元素指针
 */
static void tag_array_free(void *_elt)
{
    neu_datatag_t *elt = (neu_datatag_t *) _elt;

    /* 释放字符串字段 */
    free(elt->name);
    free(elt->address);
    free(elt->description);

    /* 处理静态标签特有的值 */
    if (NEU_ATTRIBUTE_STATIC & elt->attribute) {
        neu_value_u *cur = NULL;
        GET_STATIC_VALUE_PTR(elt, cur);          /* 获取静态值指针 */
        free(cur);                               /* 释放静态值内存 */
        memset(elt->meta, 0, sizeof(elt->meta)); /* 清零meta字段 */
    }
}

/**
 * @brief 标签数组的UT_array接口定义
 */
static UT_icd tag_icd = { sizeof(neu_datatag_t), NULL, tag_array_copy,
                          tag_array_free };

/**
 * @brief 获取标签数组的UT_array接口定义
 *
 * @return 标签数组的UT_icd指针
 */
UT_icd *neu_tag_get_icd()
{
    return &tag_icd;
}

/**
 * @brief 复制创建一个新的标签
 *
 * @param tag 源标签
 * @return 新创建的标签副本
 */
neu_datatag_t *neu_tag_dup(const neu_datatag_t *tag)
{
    neu_datatag_t *new = calloc(1, sizeof(*new));
    tag_array_copy(new, tag); /* 使用数组复制函数复制内容 */
    return new;
}

/**
 * @brief 将一个标签的内容复制到另一个已存在的标签
 *
 * 先释放目标标签中的动态资源，再复制源标签的内容
 *
 * @param tag 目标标签
 * @param other 源标签
 */
void neu_tag_copy(neu_datatag_t *tag, const neu_datatag_t *other)
{
    if (tag) {
        tag_array_free(tag);        /* 释放原有资源 */
        tag_array_copy(tag, other); /* 复制新内容 */
    }
}

/**
 * @brief 清理标签内部资源但不释放标签结构本身
 *
 * @param tag 要清理的标签
 */
void neu_tag_fini(neu_datatag_t *tag)
{
    if (tag) {
        tag_array_free(tag); /* 释放标签内部资源 */
    }
}

/**
 * @brief 释放标签及其资源
 *
 * @param tag 要释放的标签
 */
void neu_tag_free(neu_datatag_t *tag)
{
    if (tag) {
        tag_array_free(tag); /* 释放标签内部资源 */
        free(tag);           /* 释放标签结构本身 */
    }
}

/**
 * @brief 在字符串中查找最后一个指定字符的位置
 *
 * @param str 要搜索的字符串
 * @param character 要查找的字符
 * @return 找到则返回指向该字符的指针，未找到返回NULL
 */
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

/**
 * @brief 解析标签地址中的选项参数
 *
 * 从标签地址字符串中提取类型特定的选项参数
 *
 * @param datatag 数据标签
 * @param option 用于存储解析结果的选项结构
 * @return 成功返回0，失败返回-1
 */
int neu_datatag_parse_addr_option(const neu_datatag_t       *datatag,
                                  neu_datatag_addr_option_u *option)
{
    int ret = 0;

    switch (datatag->type) {
    case NEU_TYPE_BYTES: {
        /* 查找字节类型地址中的长度选项（格式如 "地址.长度"） */
        char *op = find_last_character(datatag->address, '.');

        if (op == NULL) {
            ret = -1; /* 未找到分隔符 */
        } else {
            /* 解析字节长度 */
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
    case NEU_TYPE_UINT32: {
        char *op = find_last_character(datatag->address, '#');

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
    case NEU_TYPE_DOUBLE:
    case NEU_TYPE_INT64:
    case NEU_TYPE_UINT64: {
        char *op = find_last_character(datatag->address, '#');

        option->value64.endian     = NEU_DATATAG_ENDIAN_L64;
        option->value64.is_default = true;
        if (op != NULL) {
            char e1 = 0;
            char e2 = 0;
            int  n  = sscanf(op, "#%c%c", &e1, &e2);

            if (n == 2) {
                if (e1 == 'B' && e2 == 'B') {
                    option->value64.endian     = NEU_DATATAG_ENDIAN_BB64;
                    option->value64.is_default = false;
                } else if (e1 == 'B' && e2 == 'L') {
                    option->value64.endian     = NEU_DATATAG_ENDIAN_BL64;
                    option->value64.is_default = false;
                } else if (e1 == 'L' && e2 == 'L') {
                    option->value64.endian     = NEU_DATATAG_ENDIAN_LL64;
                    option->value64.is_default = false;
                } else if (e1 == 'L' && e2 == 'B') {
                    option->value64.endian     = NEU_DATATAG_ENDIAN_LB64;
                    option->value64.is_default = false;
                }
            } else if (n == 1) {
                if (e1 == 'B') {
                    option->value64.endian     = NEU_DATATAG_ENDIAN_B64;
                    option->value64.is_default = false;
                } else if (e1 == 'L') {
                    option->value64.endian     = NEU_DATATAG_ENDIAN_L64;
                    option->value64.is_default = false;
                }
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

int neu_tag_get_static_value(const neu_datatag_t *tag, neu_value_u *value)
{
    neu_value_u *cur = NULL;

    if (!neu_tag_attribute_test(tag, NEU_ATTRIBUTE_STATIC)) {
        return -1;
    }

    GET_STATIC_VALUE_PTR(tag, cur);
    if (NULL == cur) {
        return -1;
    }

    memcpy(value, cur, sizeof(*cur));
    return 0;
}

int neu_tag_set_static_value(neu_datatag_t *tag, const neu_value_u *value)
{
    neu_value_u *cur = NULL;

    if (!neu_tag_attribute_test(tag, NEU_ATTRIBUTE_STATIC)) {
        return -1;
    }

    GET_STATIC_VALUE_PTR(tag, cur);
    if (NULL == cur) {
        cur = calloc(1, sizeof(*cur));
        if (NULL == cur) {
            return -1;
        }
        SET_STATIC_VALUE_PTR(tag, cur);
    }

    memcpy(cur, value, sizeof(*cur));

    return 0;
}

int neu_tag_get_static_value_json(neu_datatag_t *tag, neu_json_type_e *t,
                                  neu_json_value_u *v)
{
    neu_value_u *value = NULL;
    GET_STATIC_VALUE_PTR(tag, value);

    if (NULL == value) {
        return -1;
    }

    switch (tag->type) {
    case NEU_TYPE_BIT:
        *t         = NEU_JSON_INT;
        v->val_bit = value->u8;
        break;
    case NEU_TYPE_BOOL:
        *t          = NEU_JSON_BOOL;
        v->val_bool = value->boolean;
        break;
    case NEU_TYPE_INT8:
        *t         = NEU_JSON_INT;
        v->val_int = value->i8;
        break;
    case NEU_TYPE_UINT8:
        *t         = NEU_JSON_INT;
        v->val_int = value->u8;
        break;
    case NEU_TYPE_INT16:
        *t         = NEU_JSON_INT;
        v->val_int = value->i16;
        break;
    case NEU_TYPE_WORD:
    case NEU_TYPE_UINT16:
        *t         = NEU_JSON_INT;
        v->val_int = value->u16;
        break;
    case NEU_TYPE_INT32:
        *t         = NEU_JSON_INT;
        v->val_int = value->i32;
        break;
    case NEU_TYPE_DWORD:
    case NEU_TYPE_UINT32:
        *t         = NEU_JSON_INT;
        v->val_int = value->u32;
        break;
    case NEU_TYPE_INT64:
        *t         = NEU_JSON_INT;
        v->val_int = value->i64;
        break;
    case NEU_TYPE_LWORD:
    case NEU_TYPE_UINT64:
        *t         = NEU_JSON_INT;
        v->val_int = value->u64;
        break;
    case NEU_TYPE_FLOAT:
        *t           = NEU_JSON_FLOAT;
        v->val_float = value->f32;
        break;
    case NEU_TYPE_DOUBLE:
        *t            = NEU_JSON_DOUBLE;
        v->val_double = value->d64;
        break;
    case NEU_TYPE_STRING:
        *t         = NEU_JSON_STR;
        v->val_str = value->str;
        break;
    default:
        return -1;
    }

    return 0;
}

int neu_tag_set_static_value_json(neu_datatag_t *tag, neu_json_type_e t,
                                  const neu_json_value_u *v)
{
    int         rv    = 0;
    neu_value_u value = { 0 };

    if (!neu_tag_attribute_test(tag, NEU_ATTRIBUTE_STATIC)) {
        return -1;
    }

    switch (t) {
    case NEU_JSON_BIT: {
        if (NEU_TYPE_BIT == tag->type) {
            value.u8 = v->val_bit;
        } else {
            rv = -1;
        }
        break;
    }
    case NEU_JSON_BOOL: {
        if (NEU_TYPE_BOOL == tag->type) {
            value.boolean = v->val_bool;
        } else {
            rv = -1;
        }
        break;
    }
    case NEU_JSON_INT: {
        switch (tag->type) {
        case NEU_TYPE_BIT:
            value.u8 = v->val_int;
            break;
        case NEU_TYPE_INT8:
            value.i8 = v->val_int;
            break;
        case NEU_TYPE_UINT8:
            value.u8 = v->val_int;
            break;
        case NEU_TYPE_INT16:
            value.i16 = v->val_int;
            break;
        case NEU_TYPE_WORD:
        case NEU_TYPE_UINT16:
            value.u16 = v->val_int;
            break;
        case NEU_TYPE_INT32:
            value.i32 = v->val_int;
            break;
        case NEU_TYPE_DWORD:
        case NEU_TYPE_UINT32:
            value.u32 = v->val_int;
            break;
        case NEU_TYPE_INT64:
            value.i64 = v->val_int;
            break;
        case NEU_TYPE_LWORD:
        case NEU_TYPE_UINT64:
            value.u64 = v->val_int;
            break;
        case NEU_TYPE_FLOAT:
            value.f32 = v->val_int;
            break;
        case NEU_TYPE_DOUBLE:
            value.d64 = v->val_int;
            break;
        default:
            rv = -1;
            break;
        }
        break;
    }
    case NEU_JSON_FLOAT: {
        if (NEU_TYPE_FLOAT == tag->type) {
            value.f32 = v->val_float;
        } else if (NEU_TYPE_DOUBLE == tag->type) {
            value.d64 = v->val_float;
        } else {
            rv = -1;
        }
        break;
    }
    case NEU_JSON_DOUBLE: {
        if (NEU_TYPE_FLOAT == tag->type) {
            value.f32 = v->val_double;
        } else if (NEU_TYPE_DOUBLE == tag->type) {
            value.d64 = v->val_double;
        } else {
            rv = -1;
        }
        break;
    }
    case NEU_JSON_STR: {
        if (NEU_TYPE_STRING == tag->type) {
            snprintf(value.str, sizeof(value.str), "%s", v->val_str);
        } else {
            rv = -1;
        }
        break;
    }
    default:
        rv = -1;
    }

    if (0 == rv) {
        rv = neu_tag_set_static_value(tag, &value);
    }

    return rv;
}

char *neu_tag_dump_static_value(const neu_datatag_t *tag)
{
    json_t *jval = NULL;

    if (!neu_tag_attribute_test(tag, NEU_ATTRIBUTE_STATIC)) {
        return NULL;
    }

    neu_value_u value = { 0 };
    if (0 != neu_tag_get_static_value(tag, &value)) {
        return NULL;
    }

    switch (tag->type) {
    case NEU_TYPE_BOOL:
        jval = json_boolean(value.boolean);
        break;
    case NEU_TYPE_INT8:
        jval = json_integer(value.i8);
        break;
    case NEU_TYPE_BIT:
    case NEU_TYPE_UINT8:
        jval = json_integer(value.u8);
        break;
    case NEU_TYPE_INT16:
        jval = json_integer(value.i16);
        break;
    case NEU_TYPE_WORD:
    case NEU_TYPE_UINT16:
        jval = json_integer(value.u16);
        break;
    case NEU_TYPE_INT32:
        jval = json_integer(value.i32);
        break;
    case NEU_TYPE_DWORD:
    case NEU_TYPE_UINT32:
        jval = json_integer(value.u32);
        break;
    case NEU_TYPE_INT64:
        jval = json_integer(value.i64);
        break;
    case NEU_TYPE_LWORD:
    case NEU_TYPE_UINT64:
        jval = json_integer(value.u64);
        break;
    case NEU_TYPE_FLOAT:
        jval = json_real(value.f32);
        break;
    case NEU_TYPE_DOUBLE:
        jval = json_real(value.d64);
        break;
    case NEU_TYPE_STRING:
        jval = json_string(value.str);
        break;
    default:
        break;
    }

    char *s = json_dumps(jval, JSON_ENCODE_ANY);
    json_decref(jval);
    return s;
}

int neu_tag_load_static_value(neu_datatag_t *tag, const char *s)
{
    if (!neu_tag_attribute_test(tag, NEU_ATTRIBUTE_STATIC) || !s) {
        return -1;
    }

    void *jval = neu_json_decode_new(s);
    if (NULL == jval) {
        return -1;
    }

    neu_json_elem_t elem = {
        .name = NULL,
        .t    = NEU_JSON_VALUE,
    };

    int rv = neu_json_decode_value(jval, &elem);
    if (0 == rv) {
        rv = neu_tag_set_static_value_json(tag, elem.t, &elem.v);
    }

    if (NEU_JSON_STR == elem.t) {
        free(elem.v.val_str);
    }
    neu_json_decode_free(jval);

    return rv;
}

static inline void ndriver_tag_cpy(void *_dst, const void *_src)
{
    neu_ndriver_tag_t *dst = (neu_ndriver_tag_t *) _dst;
    neu_ndriver_tag_t *src = (neu_ndriver_tag_t *) _src;

    dst->name      = strdup(src->name);
    dst->address   = strdup(src->address);
    dst->attribute = src->attribute;
    dst->type      = src->type;
    dst->params    = src->params ? strdup(src->params) : NULL;
}

static inline void ndriver_tag_dtor(void *_elt)
{
    neu_ndriver_tag_t *elt = (neu_ndriver_tag_t *) _elt;
    free(elt->name);
    free(elt->address);
    free(elt->params);
}

UT_icd ndriver_tag_icd = { sizeof(neu_datatag_t), NULL, ndriver_tag_cpy,
                           ndriver_tag_dtor };

UT_icd *neu_ndriver_tag_get_icd()
{
    return &ndriver_tag_icd;
}

neu_datatag_t *neu_ndriver_tag_dup(const neu_ndriver_tag_t *tag)
{
    neu_datatag_t *new = calloc(1, sizeof(*new));
    ndriver_tag_cpy(new, tag);
    return new;
}

void neu_ndriver_tag_copy(neu_ndriver_tag_t       *tag,
                          const neu_ndriver_tag_t *other)
{
    ndriver_tag_dtor(tag);
    ndriver_tag_cpy(tag, other);
}

void neu_ndriver_tag_fini(neu_ndriver_tag_t *tag)
{
    ndriver_tag_dtor(tag);
}

void neu_ndriver_tag_free(neu_ndriver_tag_t *tag)
{
    if (tag) {
        ndriver_tag_dtor(tag);
        free(tag);
    }
}
