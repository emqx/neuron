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

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "json/json.h"

#include "json/neu_json_fn.h"
#include "json/neu_json_mqtt.h"
#include "json/neu_json_param.h"
#include "json/neu_json_rw.h"

/**
 * 使用指定的编码函数将数据编码为JSON字符串
 *
 * @param param 要编码的数据
 * @param fn 编码函数指针
 * @param result 输出参数，指向编码后的JSON字符串
 * @return 成功返回0，失败返回-1
 */
int neu_json_encode_by_fn(void *param, neu_json_encode_fn fn, char **result)
{
    void *object = neu_json_encode_new();

    if (fn != NULL) {
        if (fn(object, param) != 0) {
            neu_json_encode_free(object);
            return -1;
        }
    }

    int ret = neu_json_encode(object, result);
    neu_json_decode_free(object);
    return ret;
}

/**
 * 使用指定的编码函数和MQTT编码函数将数据编码为JSON字符串
 *
 * @param param 要编码的数据
 * @param fn 编码函数指针
 * @param mqtt_param MQTT相关参数
 * @param mqtt_fn MQTT编码函数指针
 * @param result 输出参数，指向编码后的JSON字符串
 * @return 成功返回0，失败返回-1
 */
int neu_json_encode_with_mqtt(void *param, neu_json_encode_fn fn,
                              void *mqtt_param, neu_json_encode_fn mqtt_fn,
                              char **result)
{
    void *object = neu_json_encode_new();

    if (mqtt_fn != NULL) {
        if (mqtt_fn(object, mqtt_param) != 0) {
            neu_json_encode_free(object);
            return -1;
        }
    }

    if (fn != NULL) {
        if (fn(object, param) != 0) {
            neu_json_encode_free(object);
            return -1;
        }
    }

    int ret = neu_json_encode(object, result);
    neu_json_decode_free(object);
    return ret;
}

/**
 * 从JSON字符串中解析参数
 *
 * @param buf 包含JSON数据的字符串
 * @param err_param 错误时输出的参数名称
 * @param n 参数个数
 * @param ele 第一个neu_json_elem_t参数
 * @param ... 更多neu_json_elem_t参数
 * @return 成功返回0，失败返回-1
 */
int neu_parse_param(const char *buf, char **err_param, int n,
                    neu_json_elem_t *ele, ...)
{
    void *          json       = neu_json_decode_new(buf);
    neu_json_elem_t params_ele = { .name = "params", .t = NEU_JSON_OBJECT };
    va_list         ap;
    int             ret = 0;

    if (neu_json_decode_value(json, &params_ele) != 0) {
        neu_json_decode_free(json);
        if (err_param) {
            *err_param = strdup("params");
        }
        neu_json_decode_free(json);
        return -1;
    }

    if (neu_json_decode_value(params_ele.v.val_object, ele) != 0) {
        if (err_param) {
            *err_param = strdup(ele->name);
        }
        neu_json_decode_free(json);
        return -1;
    }

    va_start(ap, ele);
    for (int i = 1; i < n; i++) {
        neu_json_elem_t *tmp_ele = va_arg(ap, neu_json_elem_t *);
        if (neu_json_decode_value(params_ele.v.val_object, tmp_ele) != 0) {
            if (err_param) {
                *err_param = strdup(tmp_ele->name);
            }
            ret = -1;
            break;
        }
    }

    va_end(ap);

    neu_json_decode_free(json);
    return ret;
}
