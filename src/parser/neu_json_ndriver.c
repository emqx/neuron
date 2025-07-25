/**
 * NEURON IIoT System for Industry 4.0
 * Copyright (C) 2020-2023 EMQ Technologies Co., Ltd All rights reserved.
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

#include "json/json.h"

#include "neu_json_ndriver.h"

/**
 * @brief 将ndriver映射对象编码为JSON
 *
 * @param json_object 输出的JSON对象
 * @param param 要编码的ndriver映射结构体指针(neu_json_ndriver_map_t类型)
 * @return int 成功返回0，失败返回非0
 */
int neu_json_encode_ndriver_map(void *json_object, void *param)
{
    int                     ret = 0;
    neu_json_ndriver_map_t *req = param;

    neu_json_elem_t req_elems[] = {
        {
            .name      = "ndriver",
            .t         = NEU_JSON_STR,
            .v.val_str = req->ndriver,
        },
        {
            .name      = "driver",
            .t         = NEU_JSON_STR,
            .v.val_str = req->driver,
        },
        {
            .name      = "group",
            .t         = NEU_JSON_STR,
            .v.val_str = req->group,
        },
    };

    ret = neu_json_encode_field(json_object, req_elems,
                                NEU_JSON_ELEM_SIZE(req_elems));

    return ret;
}

/**
 * @brief 释放ndriver映射对象资源的前向声明
 *
 * @param req 要释放的ndriver映射结构体指针
 */
void neu_json_decode_ndriver_map_free(neu_json_ndriver_map_t *req);

/**
 * @brief 从JSON字符串解码ndriver映射对象
 *
 * @param buf 包含JSON数据的字符串
 * @param result 解码结果存储位置，由函数分配内存
 * @return int 成功返回0，失败返回-1
 */
int neu_json_decode_ndriver_map(char *buf, neu_json_ndriver_map_t **result)
{
    int                     ret      = 0;
    void *                  json_obj = NULL;
    neu_json_ndriver_map_t *req      = NULL;

    json_obj = neu_json_decode_new(buf);
    if (NULL == json_obj) {
        return -1;
    }

    req = calloc(1, sizeof(*req));
    if (req == NULL) {
        goto error;
    }

    neu_json_elem_t req_elems[] = {
        {
            .name = "ndriver",
            .t    = NEU_JSON_STR,
        },
        {
            .name = "driver",
            .t    = NEU_JSON_STR,
        },
        {
            .name = "group",
            .t    = NEU_JSON_STR,
        },
    };
    ret = neu_json_decode_by_json(json_obj, NEU_JSON_ELEM_SIZE(req_elems),
                                  req_elems);
    req->ndriver = req_elems[0].v.val_str;
    req->driver  = req_elems[1].v.val_str;
    req->group   = req_elems[2].v.val_str;

    if (ret != 0) {
        goto error;
    }

    *result = req;
    neu_json_decode_free(json_obj);
    return 0;

error:
    neu_json_decode_ndriver_map_free(req);
    neu_json_decode_free(json_obj);
    return -1;
}

/**
 * @brief 释放ndriver映射对象资源
 *
 * @param req 要释放的ndriver映射结构体指针
 */
void neu_json_decode_ndriver_map_free(neu_json_ndriver_map_t *req)
{
    if (req) {
        free(req->ndriver);
        free(req->driver);
        free(req->group);
        free(req);
    }
}

/**
 * @brief 将ndriver组数组编码为JSON数组
 *
 * @param json_arr 输出的JSON数组对象
 * @param param 要编码的组数组结构体指针(neu_json_ndriver_map_group_array_t类型)
 * @return int 成功返回0，失败返回-1
 */
int neu_json_encode_ndriver_group_array(void *json_arr, void *param)
{
    neu_json_ndriver_map_group_array_t *resp =
        (neu_json_ndriver_map_group_array_t *) param;

    for (int i = 0; i < resp->n_group; i++) {
        json_t *ob = json_object();

        if (0 !=
                json_object_set_new(ob, "driver",
                                    json_string(resp->groups[i].driver)) ||
            0 !=
                json_object_set_new(ob, "group",
                                    json_string(resp->groups[i].group)) ||
            0 != json_array_append_new(json_arr, ob)) {
            json_decref(ob);
            return -1;
        }
    }

    return 0;
}

/**
 * @brief 编码获取ndriver映射响应
 *
 * @param json_obj 输出的JSON对象
 * @param param 要编码的组数组结构体指针(neu_json_ndriver_map_group_array_t类型)
 * @return int 成功返回0，失败返回-1
 */
int neu_json_encode_get_ndriver_maps_resp(void *json_obj, void *param)
{
    int ret = 0;

    void *group_array = neu_json_array();
    if (NULL == group_array) {
        return -1;
    }

    if (0 != neu_json_encode_ndriver_group_array(group_array, param)) {
        neu_json_encode_free(group_array);
        return -1;
    }

    neu_json_elem_t resp_elems[] = { {
        .name         = "groups",
        .t            = NEU_JSON_OBJECT,
        .v.val_object = group_array,
    } };
    ret                          = neu_json_encode_field(json_obj, resp_elems,
                                NEU_JSON_ELEM_SIZE(resp_elems));

    return ret;
}

/**
 * @brief 将单个ndriver标签编码为JSON对象
 *
 * @param json_obj 输出的JSON对象
 * @param param 要编码的标签结构体指针(neu_json_ndriver_tag_t类型)
 * @return int 成功返回0，失败返回-1
 */
int neu_json_encode_ndriver_tag(void *json_obj, void *param)
{
    int                     ret = 0;
    neu_json_ndriver_tag_t *tag = param;

    neu_json_elem_t elems[] = {
        {
            .name      = "name",
            .t         = NEU_JSON_STR,
            .v.val_str = tag->name,
        },
        {
            .name      = "address",
            .t         = NEU_JSON_STR,
            .v.val_str = tag->address,
        },
        {
            .name      = "attribute",
            .t         = NEU_JSON_INT,
            .v.val_int = tag->attribute,
        },
        {
            .name      = "type",
            .t         = NEU_JSON_INT,
            .v.val_int = tag->type,
        },
    };

    ret = neu_json_encode_field(json_obj, elems, NEU_JSON_ELEM_SIZE(elems));
    if (0 != ret) {
        return -1;
    }

    if (tag->params) {
        ret = neu_json_load_key(json_obj, "params", tag->params, true);
    }

    return ret;
}

/**
 * @brief 将ndriver标签数组编码为JSON数组
 *
 * @param json_obj 输出的JSON数组对象
 * @param param 要编码的标签数组结构体指针(neu_json_ndriver_tag_array_t类型)
 * @return int 成功返回0，失败返回-1
 */
int neu_json_encode_ndriver_tag_array(void *json_obj, void *param)
{
    neu_json_ndriver_tag_array_t *array = param;

    if (!json_is_array((json_t *) json_obj)) {
        return -1;
    }

    for (int i = 0; i < array->len; i++) {
        json_t *tag_obj = json_object();
        if (NULL == tag_obj || 0 != json_array_append_new(json_obj, tag_obj)) {
            return -1;
        }
        if (0 != neu_json_encode_ndriver_tag(tag_obj, &array->data[i])) {
            return -1;
        }
    }

    return 0;
}

/**
 * @brief 编码获取ndriver标签响应
 *
 * @param json_obj 输出的JSON对象
 * @param param 要编码的标签数组结构体指针(neu_json_ndriver_tag_array_t类型)
 * @return int 成功返回0，失败返回-1
 */
int neu_json_encode_get_ndriver_tags_resp(void *json_obj, void *param)
{
    int ret = 0;

    void *tags_array = neu_json_array();
    if (NULL == tags_array) {
        return -1;
    }

    if (0 != neu_json_encode_ndriver_tag_array(tags_array, param)) {
        neu_json_encode_free(tags_array);
        return -1;
    }

    neu_json_elem_t resp_elems[] = { {
        .name         = "tags",
        .t            = NEU_JSON_OBJECT,
        .v.val_object = tags_array,
    } };
    ret                          = neu_json_encode_field(json_obj, resp_elems,
                                NEU_JSON_ELEM_SIZE(resp_elems));

    return ret;
}

/**
 * @brief 释放ndriver标签参数资源
 *
 * @param tag_param 要释放的标签参数结构体指针
 */
void neu_json_ndriver_tag_param_fini(neu_json_ndriver_tag_param_t *tag_param)
{
    free(tag_param->name);
    free(tag_param->params);
}

/**
 * @brief 从JSON对象解码ndriver标签参数
 *
 * @param json_obj 包含JSON数据的对象
 * @param tag_param_p 解码结果存储位置
 * @return int 成功返回0，失败返回-1
 */
int neu_json_decode_ndriver_tag_param_json(
    void *json_obj, neu_json_ndriver_tag_param_t *tag_param_p)
{
    if (NULL == tag_param_p) {
        return -1;
    }

    neu_json_elem_t tag_elems[] = {
        {
            .name = "name",
            .t    = NEU_JSON_STR,
        },
    };

    int ret = neu_json_decode_by_json(json_obj, NEU_JSON_ELEM_SIZE(tag_elems),
                                      tag_elems);
    if (0 != ret) {
        return -1;
    }

    neu_json_ndriver_tag_param_t tag_param = {
        .name = tag_elems[0].v.val_str,
    };

    ret = neu_json_dump_key(json_obj, "params", &tag_param.params, true);
    if (0 != ret) {
        free(tag_param.name);
        return -1;
    }

    *tag_param_p = tag_param;
    return 0;
}

/**
 * @brief 释放ndriver标签参数数组资源
 *
 * @param arr 要释放的标签参数数组结构体指针
 */
void neu_json_ndriver_tag_param_array_fini(
    neu_json_ndriver_tag_param_array_t *arr)
{
    for (int i = 0; i < arr->len; ++i) {
        neu_json_ndriver_tag_param_fini(&arr->data[i]);
    }
}

/**
 * @brief 从JSON对象解码ndriver标签参数数组
 *
 * @param json_obj 包含JSON数据的数组对象
 * @param arr 解码结果存储位置
 * @return int 成功返回0，失败返回-1
 */
int neu_json_decode_ndriver_tag_param_array_json(
    void *json_obj, neu_json_ndriver_tag_param_array_t *arr)
{
    int                           len  = 0;
    neu_json_ndriver_tag_param_t *data = NULL;

    if (!json_is_array((json_t *) json_obj)) {
        return -1;
    }

    len = json_array_size(json_obj);
    if (0 == len) {
        // success on empty tag array
        arr->len  = 0;
        arr->data = NULL;
        return 0;
    }

    data = calloc(len, sizeof(*data));
    if (NULL == data) {
        return -1;
    }

    int i = 0;
    for (i = 0; i < len; i++) {
        json_t *obj = json_array_get(json_obj, i);
        if (0 != neu_json_decode_ndriver_tag_param_json(obj, &data[i])) {
            goto error;
        }
    }

    arr->len  = len;
    arr->data = data;
    return 0;

error:
    while (--i > 0) {
        neu_json_ndriver_tag_param_fini(&data[i]);
    }
    free(data);
    return -1;
}

/**
 * @brief 从JSON字符串解码更新ndriver标签参数请求
 *
 * @param buf 包含JSON数据的字符串
 * @param result 解码结果存储位置，由函数分配内存
 * @return int 成功返回0，失败返回-1
 */
int neu_json_decode_update_ndriver_tag_param_req(
    char *buf, neu_json_update_ndriver_tag_param_req_t **result)
{
    int                                      ret      = 0;
    void *                                   json_obj = NULL;
    neu_json_update_ndriver_tag_param_req_t *req      = calloc(1, sizeof(*req));
    if (NULL == req) {
        return -1;
    }

    json_obj = neu_json_decode_new(buf);
    if (NULL == json_obj) {
        goto error;
    }

    neu_json_elem_t req_elems[] = {
        {
            .name = "ndriver",
            .t    = NEU_JSON_STR,
        },
        {
            .name = "driver",
            .t    = NEU_JSON_STR,
        },
        {
            .name = "group",
            .t    = NEU_JSON_STR,
        },
        {
            .name = "tags",
            .t    = NEU_JSON_OBJECT,
        },
    };
    ret = neu_json_decode_by_json(json_obj, NEU_JSON_ELEM_SIZE(req_elems),
                                  req_elems);
    // set the fields before check for easy clean up on error
    req->ndriver = req_elems[0].v.val_str;
    req->driver  = req_elems[1].v.val_str;
    req->group   = req_elems[2].v.val_str;

    if (0 != ret) {
        goto error;
    }

    ret = neu_json_decode_ndriver_tag_param_array_json(
        req_elems[3].v.val_object, &req->tags);
    if (ret != 0) {
        goto error;
    }
    if (0 == req->tags.len) {
        goto error;
    }

    neu_json_decode_free(json_obj);
    *result = req;
    return 0;

error:
    neu_json_decode_update_ndriver_tag_param_req_free(req);
    neu_json_decode_free(json_obj);
    return -1;
}

/**
 * @brief 释放更新ndriver标签参数请求资源
 *
 * @param req 要释放的请求结构体指针
 */
void neu_json_decode_update_ndriver_tag_param_req_free(
    neu_json_update_ndriver_tag_param_req_t *req)
{
    if (req) {
        free(req->ndriver);
        free(req->driver);
        free(req->group);
        neu_json_ndriver_tag_param_array_fini(&req->tags);
        free(req->tags.data);
        free(req);
    }
}

/**
 * @brief 释放ndriver标签信息资源
 *
 * @param tag_info 要释放的标签信息结构体指针
 */
void neu_json_ndriver_tag_info_fini(neu_json_ndriver_tag_info_t *tag_info)
{
    free(tag_info->name);
    free(tag_info->address);
}

/**
 * @brief 从JSON对象解码ndriver标签信息
 *
 * @param json_obj 包含JSON数据的对象
 * @param tag_info_p 解码结果存储位置
 * @return int 成功返回0，失败返回-1
 */
int neu_json_decode_ndriver_tag_info_json(
    void *json_obj, neu_json_ndriver_tag_info_t *tag_info_p)
{
    if (NULL == tag_info_p) {
        return -1;
    }

    neu_json_elem_t tag_elems[] = {
        {
            .name = "name",
            .t    = NEU_JSON_STR,
        },
        {
            .name = "address",
            .t    = NEU_JSON_STR,
        },
    };

    int ret = neu_json_decode_by_json(json_obj, NEU_JSON_ELEM_SIZE(tag_elems),
                                      tag_elems);

    // set the fields before check for easy clean up on error
    neu_json_ndriver_tag_info_t tag_info = {
        .name    = tag_elems[0].v.val_str,
        .address = tag_elems[1].v.val_str,
    };

    if (0 != ret) {
        goto error;
    }

    *tag_info_p = tag_info;
    return 0;

error:
    neu_json_ndriver_tag_info_fini(&tag_info);
    return -1;
}

/**
 * @brief 释放ndriver标签信息数组资源
 *
 * @param arr 要释放的标签信息数组结构体指针
 */
void neu_json_ndriver_tag_info_array_fini(
    neu_json_ndriver_tag_info_array_t *arr)
{
    for (int i = 0; i < arr->len; ++i) {
        neu_json_ndriver_tag_info_fini(&arr->data[i]);
    }
}

/**
 * @brief 从JSON对象解码ndriver标签信息数组
 *
 * @param json_obj 包含JSON数据的数组对象
 * @param arr 解码结果存储位置
 * @return int 成功返回0，失败返回-1
 */
int neu_json_decode_ndriver_tag_info_array_json(
    void *json_obj, neu_json_ndriver_tag_info_array_t *arr)
{
    int                          len  = 0;
    neu_json_ndriver_tag_info_t *data = NULL;

    if (!json_is_array((json_t *) json_obj)) {
        return -1;
    }

    len = json_array_size(json_obj);
    if (0 == len) {
        // success on empty tag array
        arr->len  = 0;
        arr->data = NULL;
        return 0;
    }

    data = calloc(len, sizeof(*data));
    if (NULL == data) {
        return -1;
    }

    int i = 0;
    for (i = 0; i < len; i++) {
        json_t *obj = json_array_get(json_obj, i);
        if (0 != neu_json_decode_ndriver_tag_info_json(obj, &data[i])) {
            goto error;
        }
    }

    arr->len  = len;
    arr->data = data;
    return 0;

error:
    while (--i > 0) {
        neu_json_ndriver_tag_info_fini(&data[i]);
    }
    free(data);
    return -1;
}

/**
 * @brief 从JSON字符串解码更新ndriver标签信息请求
 *
 * @param buf 包含JSON数据的字符串
 * @param result 解码结果存储位置，由函数分配内存
 * @return int 成功返回0，失败返回-1
 */
int neu_json_decode_update_ndriver_tag_info_req(
    char *buf, neu_json_update_ndriver_tag_info_req_t **result)
{
    int                                     ret      = 0;
    void *                                  json_obj = NULL;
    neu_json_update_ndriver_tag_info_req_t *req      = calloc(1, sizeof(*req));
    if (NULL == req) {
        return -1;
    }

    json_obj = neu_json_decode_new(buf);
    if (NULL == json_obj) {
        goto error;
    }

    neu_json_elem_t req_elems[] = {
        {
            .name = "ndriver",
            .t    = NEU_JSON_STR,
        },
        {
            .name = "driver",
            .t    = NEU_JSON_STR,
        },
        {
            .name = "group",
            .t    = NEU_JSON_STR,
        },
        {
            .name = "tags",
            .t    = NEU_JSON_OBJECT,
        },
    };
    ret = neu_json_decode_by_json(json_obj, NEU_JSON_ELEM_SIZE(req_elems),
                                  req_elems);
    // set the fields before check for easy clean up on error
    req->ndriver = req_elems[0].v.val_str;
    req->driver  = req_elems[1].v.val_str;
    req->group   = req_elems[2].v.val_str;

    if (0 != ret) {
        goto error;
    }

    ret = neu_json_decode_ndriver_tag_info_array_json(req_elems[3].v.val_object,
                                                      &req->tags);
    if (ret != 0) {
        goto error;
    }
    if (0 == req->tags.len) {
        goto error;
    }

    neu_json_decode_free(json_obj);
    *result = req;
    return 0;

error:
    neu_json_decode_update_ndriver_tag_info_req_free(req);
    neu_json_decode_free(json_obj);
    return -1;
}

/**
 * @brief 释放更新ndriver标签信息请求资源
 *
 * @param req 要释放的请求结构体指针
 */
void neu_json_decode_update_ndriver_tag_info_req_free(
    neu_json_update_ndriver_tag_info_req_t *req)
{
    if (req) {
        free(req->ndriver);
        free(req->driver);
        free(req->group);
        neu_json_ndriver_tag_info_array_fini(&req->tags);
        free(req->tags.data);
        free(req);
    }
}
