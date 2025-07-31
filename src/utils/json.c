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
 **/

#include <stdlib.h>
#include <string.h>

#include <jansson.h>

#include "utils/log.h"
#include "json/json.h"

/**
 * 将元素值编码为JSON对象
 *
 * @param ele 要编码的元素
 * @return 编码后的JSON对象，失败返回NULL
 */
static json_t *encode_object_value(neu_json_elem_t *ele)
{
    json_t *ob = NULL;

    switch (ele->t) {
    case NEU_JSON_BIT:
        ob = json_integer(ele->v.val_bit);
        break;
    case NEU_JSON_INT:
        ob = json_integer(ele->v.val_int);
        break;
    case NEU_JSON_STR:
        ob = json_string(ele->v.val_str);
        break;
    case NEU_JSON_DOUBLE:
        ob = json_realp(ele->v.val_double, ele->precision);
        break;
    case NEU_JSON_FLOAT:
        ob = json_realp(ele->v.val_float, ele->precision);
        break;
    case NEU_JSON_BOOL:
        ob = json_boolean(ele->v.val_bool);
        break;
    default:
        break;
    }

    return ob;
}

/**
 * 将元素编码到JSON对象中
 *
 * @param object 目标JSON对象
 * @param ele 要编码的元素
 * @return 更新后的JSON对象
 */
static json_t *encode_object(json_t *object, neu_json_elem_t ele)
{
    json_t *ob = object;

    switch (ele.t) {
    case NEU_JSON_BIT:
        json_object_set_new(ob, ele.name, json_integer(ele.v.val_bit));
        break;
    case NEU_JSON_INT:
        json_object_set_new(ob, ele.name, json_integer(ele.v.val_int));
        break;
    case NEU_JSON_STR:
        json_object_set_new(ob, ele.name, json_string(ele.v.val_str));
        break;
    case NEU_JSON_FLOAT:
        json_object_set_new(ob, ele.name,
                            json_realp(ele.v.val_float, ele.precision));
        break;
    case NEU_JSON_DOUBLE:
        json_object_set_new(ob, ele.name,
                            json_realp(ele.v.val_double, ele.precision));
        break;
    case NEU_JSON_BOOL:
        json_object_set_new(ob, ele.name, json_boolean(ele.v.val_bool));
        break;
    case NEU_JSON_BYTES: {
        void *bytes = json_array();

        for (int i = 0; i < ele.v.val_bytes.length; i++) {
            json_array_append_new(bytes,
                                  json_integer(ele.v.val_bytes.bytes[i]));
        }

        json_object_set_new(ob, ele.name, bytes);
        break;
    }
    case NEU_JSON_OBJECT:
        json_object_set_new(ob, ele.name, ele.v.val_object);
        break;
    default:
        break;
    }

    return ob;
}

/**
 * 从JSON对象解码元素
 *
 * @param root JSON对象
 * @param ele 要填充的元素
 * @return 成功返回0，失败返回-1
 */
static int decode_object(json_t *root, neu_json_elem_t *ele)
{
    json_t *ob = NULL;

    if (ele->name == NULL) {
        ob = root;
    } else {
        ob = json_object_get(root, ele->name);
    }

    if (ob == NULL) {
        if (ele->attribute == NEU_JSON_ATTRIBUTE_OPTIONAL) {
            return 0;
        }
        zlog_error(neuron, "json decode: %s failed", ele->name);
        return -1;
    }

    if (ele->t == NEU_JSON_UNDEFINE) {
        if (json_is_string(ob)) {
            ele->t = NEU_JSON_STR;
        } else if (json_is_real(ob)) {
            ele->t = NEU_JSON_DOUBLE;
        } else if (json_is_boolean(ob)) {
            ele->t = NEU_JSON_BOOL;
        } else if (json_is_integer(ob)) {
            ele->t = NEU_JSON_INT;
        } else if (json_is_array(ob)) {
            ele->t = NEU_JSON_BYTES;
        }
    }

    switch (ele->t) {
    case NEU_JSON_BIT:
        ele->v.val_bit = json_integer_value(ob);
        break;
    case NEU_JSON_INT:
        ele->v.val_int = json_integer_value(ob);
        break;
    case NEU_JSON_STR: {
        const char *str = json_string_value(ob);
        if (str != NULL) {
            ele->v.val_str = strdup(str);
        } else {
            return -1;
        }
        break;
    }
    case NEU_JSON_FLOAT:
        if (json_is_integer(ob)) {
            ele->v.val_float = (float) json_integer_value(ob);
        } else {
            ele->v.val_float = json_real_value(ob);
        }
        break;
    case NEU_JSON_DOUBLE:
        if (json_is_integer(ob)) {
            ele->v.val_double = (double) json_integer_value(ob);
        } else {
            ele->v.val_double = json_real_value(ob);
        }
        break;
    case NEU_JSON_BOOL:
        ele->v.val_bool = json_boolean_value(ob);
        break;
    case NEU_JSON_BYTES: {
        json_t *value = NULL;

        ele->v.val_bytes.length = json_array_size(ob);
        if (ele->v.val_bytes.length > 0) {
            int index = 0;

            ele->v.val_bytes.bytes =
                calloc(ele->v.val_bytes.length, sizeof(uint8_t));
            json_array_foreach(ob, index, value)
            {
                ele->v.val_bytes.bytes[index] =
                    (uint8_t) json_integer_value(value);
            }
        }
        break;
    }
    case NEU_JSON_OBJECT:
        ele->v.val_object = ob;
        break;
    default:
        zlog_error(neuron, "json decode unknown type: %d", ele->t);
        return -1;
    }

    return 0;
}

/**
 * 创建新的JSON数组
 *
 * @return 新创建的JSON数组
 */
void *neu_json_array()
{
    return json_array();
}

/**
 * 解码JSON字符串到元素数组
 *
 * @param buf JSON字符串
 * @param size 元素数组大小
 * @param ele 用于存储解码结果的元素数组
 * @return 成功返回0，失败返回-1
 */
int neu_json_decode(char *buf, int size, neu_json_elem_t *ele)
{
    json_error_t error;
    json_t *     root = json_loads(buf, 0, &error);

    if (root == NULL) {
        zlog_error(neuron,
                   "json load error, line: %d, column: %d, position: %d, info: "
                   "%s",
                   error.line, error.column, error.position, error.text);
        return -1;
    }

    for (int i = 0; i < size; i++) {
        if (decode_object(root, &ele[i]) == -1) {
            json_decref(root);
            return -1;
        }
    }

    json_decref(root);
    return 0;
}

/**
 * 从JSON对象解码到元素数组
 *
 * @param json JSON对象
 * @param size 元素数组大小
 * @param ele 用于存储解码结果的元素数组
 * @return 成功返回0，失败返回-1
 */
int neu_json_decode_by_json(void *json, int size, neu_json_elem_t *ele)
{
    if (json == NULL) {
        zlog_error(neuron, "The param json is NULL");
        return -1;
    }

    for (int i = 0; i < size; i++) {
        if (decode_object(json, &ele[i]) == -1) {
            return -1;
        }
    }

    return 0;
}

/**
 * 解码JSON字符串中的数组元素
 *
 * @param buf JSON字符串
 * @param name 数组名称
 * @param index 数组索引
 * @param size 元素数组大小
 * @param ele 用于存储解码结果的元素数组
 * @return 成功返回0，失败返回-1
 */
int neu_json_decode_array(char *buf, char *name, int index, int size,
                          neu_json_elem_t *ele)
{
    json_t *     child  = NULL;
    json_t *     object = NULL;
    json_error_t error;
    json_t *     root = json_loads(buf, 0, &error);

    if (root == NULL) {
        zlog_error(
            neuron,
            "json load error, line: %d, column: %d, position: %d, info: %s",
            error.line, error.column, error.position, error.text);
        return -1;
    }

    object = json_object_get(root, name);
    if (object == NULL) {
        json_decref(root);
        return -1;
    }

    child = json_array_get(object, index);
    if (child == NULL) {
        json_decref(root);
        return -1;
    }

    for (int i = 0; i < size; i++) {
        if (decode_object(child, &ele[i]) == -1) {
            json_decref(root);
            return -1;
        }
    }

    json_decref(root);
    return 0;
}

/**
 * 解码JSON数组中的元素
 *
 * @param json JSON数组
 * @param index 数组索引
 * @param size 元素数组大小
 * @param ele 用于存储解码结果的元素数组
 * @return 成功返回0，失败返回-1
 */
int neu_json_decode_array_elem(void *json, int index, int size,
                               neu_json_elem_t *ele)
{
    json_t *child = NULL;

    child = json_array_get(json, index);
    if (child == NULL) {
        return -1;
    }

    for (int i = 0; i < size; i++) {
        if (decode_object(child, &ele[i]) == -1) {
            return -1;
        }
    }

    return 0;
}

/**
 * 从JSON对象解码数组元素
 *
 * @param json JSON对象
 * @param name 数组名称
 * @param index 数组索引
 * @param size 元素数组大小
 * @param ele 用于存储解码结果的元素数组
 * @return 成功返回0，失败返回-1
 */
int neu_json_decode_array_by_json(void *json, char *name, int index, int size,
                                  neu_json_elem_t *ele)
{
    json_t *child  = NULL;
    json_t *object = NULL;

    object = json_object_get(json, name);
    if (object == NULL) {
        return -1;
    }

    child = json_array_get(object, index);
    if (child == NULL) {
        return -1;
    }

    for (int i = 0; i < size; i++) {
        if (decode_object(child, &ele[i]) == -1) {
            return -1;
        }
    }

    return 0;
}

/**
 * 获取JSON对象中数组的大小
 *
 * @param json JSON对象
 * @param child 数组名称
 * @return 成功返回数组大小，失败返回-1
 */
int neu_json_decode_array_size_by_json(void *json, char *child)
{
    json_t *ob;
    int     ret = -1;

    ob = json_object_get(json, child);
    if (ob != NULL && json_is_array(ob)) {
        ret = json_array_size(ob);
    } else {
        zlog_error(neuron, "json get array object fail, %s", child);
    }

    return ret;
}

/**
 * 获取JSON字符串中数组的大小
 *
 * @param buf JSON字符串
 * @param child 数组名称
 * @return 成功返回数组大小，失败返回-1
 */
int neu_json_decode_array_size(char *buf, char *child)
{
    json_error_t error;
    json_t *     root = json_loads(buf, 0, &error);
    json_t *     ob;
    int          ret = -1;

    if (root == NULL) {
        zlog_error(
            neuron,
            "json load error, line: %d, column: %d, position: %d, info: %s",
            error.line, error.column, error.position, error.text);
        return -1;
    }

    ob = json_object_get(root, child);
    if (ob != NULL && json_is_array(ob)) {
        ret = json_array_size(ob);
    } else {
        zlog_error(neuron, "json get array object fail, %s", child);
    }

    json_decref(root);
    return ret;
}

/**
 * 将元素值编码为JSON数组
 *
 * @param array 现有数组或NULL创建新数组
 * @param t 元素数组
 * @param n 元素数量
 * @return 更新后的JSON数组
 */
void *neu_json_encode_array_value(void *array, neu_json_elem_t *t, int n)
{

    if (array == NULL) {
        array = json_array();
    }

    for (int i = 0; i < n; i++) {
        json_array_append_new(array, encode_object_value(&t[i]));
    }

    return array;
}

/**
 * 将元素编码为JSON数组
 *
 * @param array 现有数组或NULL创建新数组
 * @param t 元素数组
 * @param n 元素数量
 * @return 更新后的JSON数组
 */
void *neu_json_encode_array(void *array, neu_json_elem_t *t, int n)
{
    if (array == NULL) {
        array = json_array();
    }

    json_t *ob = json_object();
    for (int j = 0; j < n; j++) {
        encode_object(ob, t[j]);
    }

    json_array_append_new(array, ob);
    return array;
}

/**
 * 创建新的JSON对象
 *
 * @return 新创建的JSON对象
 */
void *neu_json_encode_new()
{
    return json_object();
}

/**
 * 释放JSON对象
 *
 * @param json_object 要释放的JSON对象
 */
void neu_json_encode_free(void *json_object)
{
    json_decref(json_object);
}

/**
 * 编码元素到JSON对象
 *
 * @param json_object 目标JSON对象
 * @param elem 元素数组
 * @param n 元素数量
 * @return 成功返回0
 */
int neu_json_encode_field(void *json_object, neu_json_elem_t *elem, int n)
{
    for (int i = 0; i < n; i++) {
        encode_object(json_object, elem[i]);
    }

    return 0;
}

/**
 * 将JSON对象编码为字符串
 *
 * @param json_object JSON对象
 * @param str 输出参数，存储编码后的字符串
 * @return 成功返回0
 */
int neu_json_encode(void *json_object, char **str)
{
    *str = json_dumps(json_object, JSON_REAL_PRECISION(16));

    return 0;
}

/**
 * 从字符串解析新的JSON对象
 *
 * @param buf JSON字符串
 * @return 成功返回JSON对象，失败返回NULL
 */
void *neu_json_decode_new(const char *buf)
{
    json_error_t error;
    json_t *     root = json_loads(buf, JSON_DECODE_ANY, &error);

    if (root == NULL) {
        zlog_error(
            neuron,
            "json load error, line: %d, column: %d, position: %d, info: %s",
            error.line, error.column, error.position, error.text);
        return NULL;
    }

    return root;
}

/**
 * 从二进制数据解析新的JSON对象
 *
 * @param buf 二进制数据
 * @param len 数据长度
 * @return 成功返回JSON对象，失败返回NULL
 */
void *neu_json_decode_newb(char *buf, size_t len)
{
    json_error_t error;
    json_t *     root = json_loadb(buf, len, 0, &error);

    if (root == NULL) {
        zlog_error(
            neuron,
            "json load error, line: %d, column: %d, position: %d, info: %s",
            error.line, error.column, error.position, error.text);
        return NULL;
    }

    return root;
}

/**
 * 释放JSON对象
 *
 * @param ob 要释放的JSON对象
 */
void neu_json_decode_free(void *ob)
{
    json_decref(ob);
}

/**
 * 从JSON对象解码单个元素值
 *
 * @param object JSON对象
 * @param ele 要填充的元素
 * @return 成功返回0，失败返回-1
 */
int neu_json_decode_value(void *object, neu_json_elem_t *ele)
{
    return decode_object(object, ele);
}

/**
 * 从JSON对象中提取指定键的值并转为JSON字符串
 *
 * @param object JSON对象
 * @param key 键名
 * @param result 输出参数，存储提取结果的JSON字符串
 * @param must_exist 键是否必须存在
 * @return 成功返回0，失败返回-1
 */
int neu_json_dump_key(void *object, const char *key, char **const result,
                      bool must_exist)
{
    int rv        = 0;
    json_t *new   = NULL;
    json_t *value = NULL;

    value = json_object_get(object, key);
    if (NULL == value) {
        if (must_exist) {
            zlog_error(neuron, "json has no key `%s`", key);
            return -1;
        } else {
            *result = NULL;
            return 0;
        }
    }

    if (NULL == (new = json_object()) || json_object_set(new, key, value) < 0 ||
        NULL == (*result = json_dumps(new, JSON_COMPACT))) {
        rv = -1;
    }

    json_decref(new);
    return rv;
}

/**
 * 从JSON字符串加载指定键的值到JSON对象
 *
 * @param object 目标JSON对象
 * @param key 键名
 * @param input 输入JSON字符串
 * @param must_exist 键是否必须存在
 * @return 成功返回0，失败返回-1
 */
int neu_json_load_key(void *object, const char *key, const char *input,
                      bool must_exist)
{
    int          rv           = 0;
    json_t *     input_object = NULL;
    json_t *     value        = NULL;
    json_error_t error        = {};

    input_object = json_loads(input, 0, &error);
    if (NULL == input_object) {
        zlog_error(neuron,
                   "json load error, line: %d, column: %d, position: %d, info: "
                   "%s",
                   error.line, error.column, error.position, error.text);
        return -1;
    }

    value = json_object_get(input_object, key);
    if (NULL == value) {
        if (must_exist) {
            zlog_error(neuron, "json has no key `%s`", key);
            rv = -1;
        }
    } else if (json_object_set(object, key, value) < 0) {
        rv = -1;
    }

    json_decref(input_object);
    return rv;
}
