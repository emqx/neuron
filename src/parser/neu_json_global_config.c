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

#include "tag.h"
#include "utils/log.h"
#include "json/json.h"

#include "neu_json_global_config.h"
#include "parser/neu_json_group_config.h"
#include "parser/neu_json_tag.h"

/**
 * 释放添加标签请求结构体内部分配的内存
 *
 * @param req 需要释放内部资源的neu_json_add_tags_req_t结构体指针
 */
static void fini_add_tags_req(neu_json_add_tags_req_t *req)
{
    neu_json_tag_array_t arr = {
        .len  = req->n_tag,
        .tags = req->tags,
    };
    neu_json_decode_tag_array_fini(&arr);

    free(req->node);
    free(req->group);
}

/**
 * 从JSON对象解码到添加标签请求结构体
 *
 * @param json_obj JSON对象指针
 * @param req 输出参数，已分配内存的neu_json_add_tags_req_t结构体指针
 * @return 成功返回0，失败返回-1
 */
static int decode_add_tags_req(void *json_obj, neu_json_add_tags_req_t *req)
{
    int ret = 0;

    neu_json_elem_t req_elems[] = {
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
    if (0 != ret) {
        goto decode_fail;
    }

    neu_json_tag_array_t arr = { 0 };
    ret = neu_json_decode_tag_array_json(req_elems[2].v.val_object, &arr);
    if (0 != ret) {
        goto decode_fail;
    }

    req->node  = req_elems[0].v.val_str;
    req->group = req_elems[1].v.val_str;
    req->n_tag = arr.len;
    req->tags  = arr.tags;

    return 0;

decode_fail:
    free(req_elems[0].v.val_str);
    free(req_elems[1].v.val_str);
    return -1;
}

/**
 * 从JSON对象解码到全局配置请求标签结构体
 *
 * @param json_obj JSON对象指针
 * @param result
 * 输出参数，指向解码后的neu_json_global_config_req_tags_t结构体指针
 * @return 成功返回0，失败返回-1
 */
int neu_json_decode_global_config_req_tags(
    void *json_obj, neu_json_global_config_req_tags_t **result)
{
    int                                ret       = 0;
    json_t *                           tag_array = NULL;
    neu_json_global_config_req_tags_t *req       = NULL;

    tag_array = json_object_get(json_obj, "tags");
    if (!json_is_array(tag_array)) {
        nlog_error("no `tags` array field");
        goto decode_fail;
    }

    req = calloc(1, sizeof(*req));
    if (req == NULL) {
        nlog_error("calloc fail");
        goto decode_fail;
    }

    req->n_tag = json_array_size(tag_array);

    req->tags = calloc(req->n_tag, sizeof(*req->tags));
    if (NULL == req->tags) {
        nlog_error("calloc fail");
        goto decode_fail;
    }

    for (int i = 0; i < req->n_tag; i++) {
        ret = decode_add_tags_req(json_array_get(tag_array, i), &req->tags[i]);
        if (ret != 0) {
            goto decode_fail;
        }
    }

    *result = req;
    goto decode_exit;

decode_fail:
    neu_json_decode_global_config_req_tags_free(req);
    ret = -1;

decode_exit:
    return ret;
}

/**
 * 释放全局配置请求标签结构体及其内部分配的内存
 *
 * @param req_tags 需要释放的neu_json_global_config_req_tags_t结构体指针
 */
void neu_json_decode_global_config_req_tags_free(
    neu_json_global_config_req_tags_t *req_tags)
{
    if (NULL == req_tags) {
        return;
    }

    for (int i = 0; i < req_tags->n_tag; ++i) {
        fini_add_tags_req(&req_tags->tags[i]);
    }

    free(req_tags->tags);
    free(req_tags);
}

/**
 * 解码JSON对象到订阅请求结构体
 *
 * @param json_obj JSON对象指针
 * @param req 输出参数，已分配内存的neu_json_subscribe_req_t结构体指针
 * @return 成功返回0，失败返回-1
 */
static int decode_subscribe_req(json_t *json_obj, neu_json_subscribe_req_t *req)
{
    neu_json_elem_t req_elems[] = {
        {
            .name = "driver",
            .t    = NEU_JSON_STR,
        },
        {
            .name = "group",
            .t    = NEU_JSON_STR,
        },
    };

    int ret = neu_json_decode_by_json(json_obj, NEU_JSON_ELEM_SIZE(req_elems),
                                      req_elems);
    if (ret != 0) {
        goto error;
    }

    ret = neu_json_dump_key(json_obj, "params", &req->params, false);
    if (0 != ret) {
        goto error;
    }

    req->driver = req_elems[0].v.val_str;
    req->group  = req_elems[1].v.val_str;
    return 0;

error:
    free(req_elems[0].v.val_str);
    free(req_elems[1].v.val_str);
    return -1;
}

/**
 * 释放订阅请求结构体内部分配的内存
 *
 * @param req 需要释放内部资源的neu_json_subscribe_req_t结构体指针
 */
static inline void fini_subscribe_req(neu_json_subscribe_req_t *req)
{
    free(req->app);
    free(req->driver);
    free(req->group);
    free(req->params);
}

/**
 * 从JSON对象解码到全局配置请求订阅结构体
 *
 * @param json_obj JSON对象指针
 * @param result
 * 输出参数，指向解码后的neu_json_global_config_req_subscriptions_t结构体指针
 * @return 成功返回0，失败返回-1
 */
int neu_json_decode_global_config_req_subscriptions(
    void *json_obj, neu_json_global_config_req_subscriptions_t **result)
{
    int                                         n_subscription = 0;
    json_t *                                    sub_array      = NULL;
    neu_json_subscribe_req_t *                  p              = NULL;
    neu_json_global_config_req_subscriptions_t *req            = NULL;

    sub_array = json_object_get(json_obj, "subscriptions");
    if (!json_is_array(sub_array)) {
        nlog_error("no `subscriptions` array field");
        return -1;
    }

    for (size_t i = 0; i < json_array_size(sub_array); ++i) {
        json_t *grp_array =
            json_object_get(json_array_get(sub_array, i), "groups");
        if (!json_is_array(grp_array)) {
            nlog_error("no subscriptions `group` field");
            return -1;
        }
        n_subscription += json_array_size(grp_array);
    }

    req = calloc(1, sizeof(*req));
    if (NULL == req) {
        nlog_error("calloc fail");
        return -1;
    }

    req->n_subscription = n_subscription;
    req->subscriptions =
        calloc(req->n_subscription, sizeof(*req->subscriptions));
    if (NULL == req->subscriptions) {
        free(req);
        nlog_error("calloc fail");
        return -1;
    }

    p = req->subscriptions;
    for (size_t i = 0; i < json_array_size(sub_array); ++i) {
        json_t *sub_obj = json_array_get(sub_array, i);
        json_t *app_obj = json_object_get(sub_obj, "app");

        if (NULL == app_obj) {
            nlog_error("no subscriptions `app` field");
            goto error;
        }

        json_t *grp_array = json_object_get(sub_obj, "groups");
        for (size_t j = 0; j < json_array_size(grp_array); ++j) {
            p->app = strdup(json_string_value(app_obj));
            if (NULL == p->app) {
                nlog_error("strdup fail");
                goto error;
            }
            if (0 != decode_subscribe_req(json_array_get(grp_array, j), p)) {
                free(p->app);
                goto error;
            }
            ++p;
        }
    }

    *result = req;
    return 0;

error:
    while (--p >= req->subscriptions) {
        fini_subscribe_req(p);
    }
    free(req->subscriptions);
    free(req);
    return -1;
}

/**
 * 释放全局配置请求订阅结构体及其内部分配的内存
 *
 * @param req_sub 需要释放的neu_json_global_config_req_subscriptions_t结构体指针
 */
void neu_json_decode_global_config_req_subscriptions_free(
    neu_json_global_config_req_subscriptions_t *req_sub)
{
    if (NULL == req_sub) {
        return;
    }

    for (int i = 0; i < req_sub->n_subscription; ++i) {
        fini_subscribe_req(&req_sub->subscriptions[i]);
    }

    free(req_sub->subscriptions);
    free(req_sub);
}

/**
 * 释放节点设置请求结构体内部分配的内存
 *
 * @param req 需要释放内部资源的neu_json_node_setting_req_t结构体指针
 */
static inline void fini_node_setting(neu_json_node_setting_req_t *req)
{
    free(req->node);
    free(req->setting);
}

/**
 * 从JSON对象解码到全局配置请求设置结构体
 *
 * @param json_obj JSON对象指针
 * @param result
 * 输出参数，指向解码后的neu_json_global_config_req_settings_t结构体指针
 * @return 成功返回0，失败返回-1
 */
int neu_json_decode_global_config_req_settings(
    void *json_obj, neu_json_global_config_req_settings_t **result)
{
    json_t *settings_array = json_object_get(json_obj, "settings");
    if (!json_is_array(settings_array)) {
        nlog_error("no `settings` array field");
        return -1;
    }

    neu_json_global_config_req_settings_t *req = calloc(1, sizeof(*req));
    if (NULL == req) {
        nlog_error("calloc fail");
        return -1;
    }

    req->n_setting = json_array_size(settings_array);
    req->settings  = calloc(req->n_setting, sizeof(*req->settings));
    if (NULL == req->settings) {
        free(req);
        nlog_error("calloc fail");
        return -1;
    }

    neu_json_node_setting_req_t *p = req->settings;
    for (int i = 0; i < req->n_setting; ++i) {
        json_t *setting_obj = json_array_get(settings_array, i);
        json_t *node_obj    = json_object_get(setting_obj, "node");

        if (NULL == node_obj) {
            nlog_error("no settings `node` field");
            goto error;
        }

        p->node = strdup(json_string_value(node_obj));
        if (NULL == p->node) {
            nlog_error("strdup fail");
            goto error;
        }

        if (0 != neu_json_dump_key(setting_obj, "params", &p->setting, true)) {
            free(p->node);
            nlog_error("no settings `params` field");
            goto error;
        }
        ++p;
    }

    *result = req;
    return 0;

error:
    while (--p >= req->settings) {
        fini_node_setting(p);
    }
    free(req->settings);
    free(req);
    return -1;
}

/**
 * 释放全局配置请求设置结构体及其内部分配的内存
 *
 * @param req_settings 需要释放的neu_json_global_config_req_settings_t结构体指针
 */
void neu_json_decode_global_config_req_settings_free(
    neu_json_global_config_req_settings_t *req_settings)
{
    if (req_settings) {
        for (int i = 0; i < req_settings->n_setting; ++i) {
            fini_node_setting(&req_settings->settings[i]);
        }
        free(req_settings->settings);
        free(req_settings);
    }
}

/**
 * 解码JSON到全局配置请求结构体
 *
 * @param buf 包含JSON数据的字符串
 * @param result 输出参数，指向解码后的neu_json_global_config_req_t结构体指针
 * @return 成功返回0，失败返回-1
 */
int neu_json_decode_global_config_req(char *                         buf,
                                      neu_json_global_config_req_t **result)
{
    neu_json_global_config_req_t *req = calloc(1, sizeof(*req));
    if (NULL == req) {
        return -1;
    }

    void *json_obj = neu_json_decode_new(buf);
    if (NULL == json_obj) {
        free(req);
        return -1;
    }

    int ret = 0;
    if (0 == neu_json_decode_get_nodes_resp_json(json_obj, &req->nodes) &&
        0 ==
            neu_json_decode_get_driver_group_resp_json(json_obj,
                                                       &req->groups) &&
        0 == neu_json_decode_global_config_req_tags(json_obj, &req->tags) &&
        0 ==
            neu_json_decode_global_config_req_subscriptions(
                json_obj, &req->subscriptions) &&
        0 ==
            neu_json_decode_global_config_req_settings(json_obj,
                                                       &req->settings)) {
        *result = req;
    } else {
        ret = -1;
        neu_json_decode_global_config_req_free(req);
    }

    neu_json_decode_free(json_obj);
    return ret;
}

/**
 * 释放全局配置请求结构体及其内部分配的内存
 *
 * @param req 需要释放的neu_json_global_config_req_t结构体指针
 */
void neu_json_decode_global_config_req_free(neu_json_global_config_req_t *req)
{
    if (req) {
        neu_json_decode_get_nodes_resp_free(req->nodes);
        neu_json_decode_get_driver_group_resp_free(req->groups);
        neu_json_decode_global_config_req_tags_free(req->tags);
        neu_json_decode_global_config_req_subscriptions_free(
            req->subscriptions);
        neu_json_decode_global_config_req_settings_free(req->settings);
        free(req);
    }
}
