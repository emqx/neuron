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

#include <string.h>

#include <jansson.h>

#include "utils/log.h"
#include "json/json.h"

#include "neu_json_template.h"

/**
 * 将模板组编码成JSON对象
 *
 * @param json_obj JSON对象指针，用于存储编码结果
 * @param param 指向模板组的指针，作为要编码的数据
 * @return 成功返回0，失败返回非0值
 */
int neu_json_encode_template_group(void *json_obj, void *param)
{
    int                        ret = 0;
    neu_json_template_group_t *grp = param;

    void *tag_array = neu_json_array();
    if (NULL == tag_array) {
        return -1;
    }

    ret = neu_json_encode_tag_array(tag_array, &grp->tags);
    if (0 != ret) {
        neu_json_encode_free(tag_array);
        return ret;
    }

    neu_json_elem_t req_elems[] = {
        {
            .name      = "name",
            .t         = NEU_JSON_STR,
            .v.val_str = grp->name,
        },
        {
            .name      = "interval",
            .t         = NEU_JSON_INT,
            .v.val_int = grp->interval,
        },
        {
            .name         = "tags",
            .t            = NEU_JSON_OBJECT,
            .v.val_object = tag_array,
        },
    };

    ret = neu_json_encode_field(json_obj, req_elems,
                                NEU_JSON_ELEM_SIZE(req_elems));

    return ret;
}

/**
 * 从JSON对象解码模板组数据
 *
 * @param json_obj JSON对象指针，包含要解码的数据
 * @param grp 指向模板组结构的指针，用于存储解码结果
 * @return 成功返回0，失败返回-1
 */
int neu_json_decode_template_group(void *                     json_obj,
                                   neu_json_template_group_t *grp)
{
    int                  ret  = 0;
    neu_json_tag_array_t tags = { 0 };

    neu_json_elem_t req_elems[] = {
        {
            .name = "name",
            .t    = NEU_JSON_STR,
        },
        {
            .name = "interval",
            .t    = NEU_JSON_INT,
        },
        {
            .name = "tags",
            .t    = NEU_JSON_OBJECT,
        },
    };

    ret = neu_json_decode_by_json(json_obj, NEU_JSON_ELEM_SIZE(req_elems),
                                  req_elems);
    if (0 != ret) {
        goto error;
    }

    ret = neu_json_decode_tag_array_json(req_elems[2].v.val_object, &tags);
    if (0 != ret) {
        goto error;
    }

    grp->name     = req_elems[0].v.val_str;
    grp->interval = req_elems[1].v.val_int;
    grp->tags     = tags;

    return 0;

error:
    free(req_elems[0].v.val_str);
    return -1;
}

/**
 * 释放模板组结构中分配的内存资源
 *
 * @param grp 指向要释放资源的模板组结构的指针
 */
void neu_json_decode_template_group_fini(neu_json_template_group_t *grp)
{
    free(grp->name);
    neu_json_decode_tag_array_fini(&grp->tags);
}

/**
 * 将模板组数组编码成JSON对象
 *
 * @param json_obj JSON对象指针，用于存储编码结果
 * @param param 指向模板组数组的指针，作为要编码的数据
 * @return 成功返回0，失败返回-1
 */
int neu_json_encode_template_group_array(void *json_obj, void *param)
{
    neu_json_template_group_array_t *array = param;

    if (!json_is_array((json_t *) json_obj)) {
        return -1;
    }

    for (int i = 0; i < array->len; i++) {
        json_t *grp_obj = json_object();
        if (NULL == grp_obj || 0 != json_array_append_new(json_obj, grp_obj)) {
            return -1;
        }
        if (0 != neu_json_encode_template_group(grp_obj, &array->groups[i])) {
            return -1;
        }
    }

    return 0;
}

/**
 * 从JSON对象解码模板组数组数据
 *
 * @param json_obj JSON对象指针，包含要解码的数据
 * @param arr 指向模板组数组结构的指针，用于存储解码结果
 * @return 成功返回0，失败返回-1
 */
int neu_json_decode_template_group_array_json(
    void *json_obj, neu_json_template_group_array_t *arr)
{
    int                        len    = 0;
    neu_json_template_group_t *groups = NULL;

    if (!json_is_array((json_t *) json_obj)) {
        return -1;
    }

    len = json_array_size(json_obj);
    if (0 == len) {
        // success on empty group array
        arr->len    = 0;
        arr->groups = NULL;
        return 0;
    }

    groups = calloc(len, sizeof(*groups));
    if (NULL == groups) {
        return -1;
    }

    int i = 0;
    for (i = 0; i < len; i++) {
        json_t *grp_obj = json_array_get(json_obj, i);
        if (0 != neu_json_decode_template_group(grp_obj, &groups[i])) {
            goto decode_fail;
        }
    }

    arr->len    = len;
    arr->groups = groups;
    return 0;

decode_fail:
    while (--i > 0) {
        neu_json_decode_template_group_fini(&groups[i]);
    }
    free(groups);
    return -1;
}

/**
 * 释放模板组数组结构中分配的内存资源
 *
 * @param arr 指向要释放资源的模板组数组结构的指针
 */
void neu_json_decode_template_group_array_fini(
    neu_json_template_group_array_t *arr)
{
    for (int i = 0; i < arr->len; ++i) {
        neu_json_decode_template_group_fini(&arr->groups[i]);
    }
    free(arr->groups);
}

/**
 * 将模板编码成JSON对象
 *
 * @param json_obj JSON对象指针，用于存储编码结果
 * @param param 指向模板的指针，作为要编码的数据
 * @return 成功返回0，失败返回非0值
 */
int neu_json_encode_template(void *json_obj, void *param)
{
    int                  ret  = 0;
    neu_json_template_t *tmpl = param;

    void *grp_array = neu_json_array();
    if (NULL == grp_array) {
        return -1;
    }

    ret = neu_json_encode_template_group_array(grp_array, &tmpl->groups);
    if (0 != ret) {
        neu_json_encode_free(grp_array);
        return ret;
    }

    neu_json_elem_t req_elems[] = {
        {
            .name      = "name",
            .t         = NEU_JSON_STR,
            .v.val_str = tmpl->name,
        },
        {
            .name      = "plugin",
            .t         = NEU_JSON_STR,
            .v.val_str = tmpl->plugin,
        },
        {
            .name         = "groups",
            .t            = NEU_JSON_OBJECT,
            .v.val_object = grp_array,
        },
    };

    ret = neu_json_encode_field(json_obj, req_elems,
                                NEU_JSON_ELEM_SIZE(req_elems));

    return ret;
}

/**
 * 从JSON字符串解码模板数据
 *
 * @param buf 包含JSON数据的字符串
 * @param result 二级指针，用于返回解码后的模板结构
 * @return 成功返回0，失败返回-1
 */
int neu_json_decode_template(char *buf, neu_json_template_t **result)
{
    neu_json_template_t *tmpl = calloc(1, sizeof(neu_json_add_tags_req_t));
    if (tmpl == NULL) {
        return -1;
    }

    void *json_obj = neu_json_decode_new(buf);
    if (NULL == json_obj) {
        free(tmpl);
        return -1;
    }

    int ret = neu_json_decode_template_json(json_obj, tmpl);
    if (0 == ret) {
        *result = tmpl;
    } else {
        free(tmpl);
    }

    neu_json_decode_free(json_obj);
    return ret;
}

/**
 * 从JSON对象解码模板数据
 *
 * @param json_obj JSON对象指针，包含要解码的数据
 * @param tmpl 指向模板结构的指针，用于存储解码结果
 * @return 成功返回0，失败返回-1
 */
int neu_json_decode_template_json(void *json_obj, neu_json_template_t *tmpl)
{
    int                             ret  = 0;
    neu_json_template_group_array_t grps = { 0 };

    neu_json_elem_t req_elems[] = {
        {
            .name = "name",
            .t    = NEU_JSON_STR,
        },
        {
            .name = "plugin",
            .t    = NEU_JSON_STR,
        },
        {
            .name      = "groups",
            .t         = NEU_JSON_OBJECT,
            .attribute = NEU_JSON_ATTRIBUTE_OPTIONAL,
        },
    };

    ret = neu_json_decode_by_json(json_obj, NEU_JSON_ELEM_SIZE(req_elems),
                                  req_elems);
    if (0 != ret) {
        goto error;
    }

    ret = neu_json_decode_template_group_array_json(req_elems[2].v.val_object,
                                                    &grps);
    if (req_elems[2].v.val_object && 0 != ret) {
        goto error;
    }

    tmpl->name   = req_elems[0].v.val_str;
    tmpl->plugin = req_elems[1].v.val_str;
    tmpl->groups = grps;

    return 0;

error:
    free(req_elems[0].v.val_str);
    free(req_elems[1].v.val_str);
    return -1;
}

/**
 * 释放模板结构中分配的内存资源
 *
 * @param tmpl 指向要释放资源的模板结构的指针
 */
void neu_json_decode_template_fini(neu_json_template_t *tmpl)
{
    free(tmpl->name);
    free(tmpl->plugin);
    neu_json_decode_template_group_array_fini(&tmpl->groups);
}

/**
 * 释放模板结构及其自身的内存
 *
 * @param tmpl 指向要释放的模板结构的指针
 */
void neu_json_decode_template_free(neu_json_template_t *tmpl)
{
    if (tmpl) {
        neu_json_decode_template_fini(tmpl);
        free(tmpl);
    }
}

/**
 * 将模板信息数组编码成JSON对象
 *
 * @param json_obj JSON对象指针，用于存储编码结果
 * @param param 指向模板信息数组的指针，作为要编码的数据
 * @return 成功返回0，失败返回-1
 */
int neu_json_encode_template_info_array(void *json_obj, void *param)
{
    int                             ret = 0;
    neu_json_template_info_array_t *arr = param;

    if (!json_is_array((json_t *) json_obj)) {
        return -1;
    }

    for (int i = 0; i < arr->len; ++i) {
        json_t *info_obj = NULL;
        if (NULL == (info_obj = json_object()) ||
            // ownership of info_obj moved
            0 != json_array_append_new(json_obj, info_obj)) {
            ret = -1;
            break;
        }

        neu_json_elem_t req_elems[] = {
            {
                .name      = "name",
                .t         = NEU_JSON_STR,
                .v.val_str = arr->info[i].name,
            },
            {
                .name      = "plugin",
                .t         = NEU_JSON_STR,
                .v.val_str = arr->info[i].plugin,
            },
        };
        // ownership of info_obj moved
        ret = neu_json_encode_field(info_obj, req_elems,
                                    NEU_JSON_ELEM_SIZE(req_elems));
        if (0 != ret) {
            break;
        }
    }

    return ret;
}

/**
 * 将获取模板响应编码成JSON对象
 *
 * @param json_obj JSON对象指针，用于存储编码结果
 * @param param 指向要编码的数据
 * @return 成功返回0，失败返回-1
 */
int neu_json_encode_get_templates_resp(void *json_obj, void *param)
{
    void *info_array = neu_json_array();
    if (NULL == info_array) {
        return -1;
    }

    if (0 != neu_json_encode_template_info_array(info_array, param)) {
        neu_json_encode_free(info_array);
        return -1;
    }

    neu_json_elem_t req_elems[] = {
        {
            .name         = "templates",
            .t            = NEU_JSON_OBJECT,
            .v.val_object = info_array,
        },
    };

    // ownership of info_array moved
    return neu_json_encode_field(json_obj, req_elems,
                                 NEU_JSON_ELEM_SIZE(req_elems));
}

/**
 * 将添加模板组请求编码成JSON对象
 *
 * @param json_obj JSON对象指针，用于存储编码结果
 * @param param 指向添加模板组请求的指针，作为要编码的数据
 * @return 成功返回0，失败返回非0值
 */
int neu_json_encode_template_add_group_req(void *json_obj, void *param)
{
    int                                ret = 0;
    neu_json_template_add_group_req_t *req = param;

    neu_json_elem_t req_elems[] = { {
                                        .name      = "template",
                                        .t         = NEU_JSON_STR,
                                        .v.val_str = req->tmpl,
                                    },
                                    {
                                        .name      = "group",
                                        .t         = NEU_JSON_STR,
                                        .v.val_str = req->group,
                                    },
                                    {
                                        .name      = "interval",
                                        .t         = NEU_JSON_INT,
                                        .v.val_int = req->interval,
                                    } };
    ret                         = neu_json_encode_field(json_obj, req_elems,
                                NEU_JSON_ELEM_SIZE(req_elems));

    return ret;
}

/**
 * 从JSON字符串解码添加模板组请求
 *
 * @param buf 包含JSON数据的字符串
 * @param result 二级指针，用于返回解码后的添加模板组请求结构
 * @return 成功返回0，失败返回-1
 */
int neu_json_decode_template_add_group_req(
    char *buf, neu_json_template_add_group_req_t **result)
{
    int                                ret      = 0;
    void *                             json_obj = NULL;
    neu_json_template_add_group_req_t *req      = NULL;

    json_obj = neu_json_decode_new(buf);
    if (NULL == json_obj) {
        return -1;
    }

    req = calloc(1, sizeof(*req));
    if (req == NULL) {
        goto decode_fail;
    }

    neu_json_elem_t req_elems[] = { {
                                        .name = "template",
                                        .t    = NEU_JSON_STR,
                                    },
                                    {
                                        .name = "group",
                                        .t    = NEU_JSON_STR,
                                    },
                                    {
                                        .name = "interval",
                                        .t    = NEU_JSON_INT,
                                    } };
    ret = neu_json_decode_by_json(json_obj, NEU_JSON_ELEM_SIZE(req_elems),
                                  req_elems);

    req->tmpl     = req_elems[0].v.val_str;
    req->group    = req_elems[1].v.val_str;
    req->interval = req_elems[2].v.val_int;

    if (ret != 0) {
        goto decode_fail;
    }

    *result = req;
    neu_json_decode_free(json_obj);
    return 0;

decode_fail:
    neu_json_decode_template_add_group_req_free(req);
    neu_json_decode_free(json_obj);
    return -1;
}

/**
 * 释放添加模板组请求结构的内存
 *
 * @param req 指向要释放的添加模板组请求结构的指针
 */
void neu_json_decode_template_add_group_req_free(
    neu_json_template_add_group_req_t *req)
{
    if (req) {
        free(req->tmpl);
        free(req->group);
        free(req);
    }
}

/**
 * 从JSON字符串解码更新模板组请求
 *
 * @param buf 包含JSON数据的字符串
 * @param result 二级指针，用于返回解码后的更新模板组请求结构
 * @return 成功返回0，失败返回-1
 */
int neu_json_decode_template_update_group_req(
    char *buf, neu_json_template_update_group_req_t **result)
{
    int ret = 0;

    neu_json_template_update_group_req_t *req = calloc(1, sizeof(*req));
    if (req == NULL) {
        return -1;
    }

    void *json_obj = neu_json_decode_new(buf);
    if (NULL == json_obj) {
        free(req);
        return -1;
    }

    neu_json_elem_t req_elems[] = {
        {
            .name = "template",
            .t    = NEU_JSON_STR,
        },
        {
            .name = "group",
            .t    = NEU_JSON_STR,
        },
        {
            .name      = "new_name",
            .t         = NEU_JSON_STR,
            .attribute = NEU_JSON_ATTRIBUTE_OPTIONAL,
        },
    };
    ret       = neu_json_decode_by_json(json_obj, NEU_JSON_ELEM_SIZE(req_elems),
                                  req_elems);
    req->tmpl = req_elems[0].v.val_str;
    req->group    = req_elems[1].v.val_str;
    req->new_name = req_elems[2].v.val_str;
    if (0 != ret) {
        goto error;
    }

    json_t *json_interval = json_object_get(json_obj, "interval");
    if (NULL != json_interval) {
        if (!json_is_integer(json_interval)) {
            nlog_error("decode interval is not integer");
            goto error;
        }
        req->set_interval = true;
        req->interval     = json_integer_value(json_interval);
    } else if (NULL == req->new_name) {
        // at least one of `new_name` or `interval` should be provided
        goto error;
    }

    *result = req;
    neu_json_decode_free(json_obj);
    return 0;

error:
    neu_json_decode_template_update_group_req_free(req);
    neu_json_decode_free(json_obj);
    return -1;
}

/**
 * 释放更新模板组请求结构的内存
 *
 * @param req 指向要释放的更新模板组请求结构的指针
 */
void neu_json_decode_template_update_group_req_free(
    neu_json_template_update_group_req_t *req)
{
    free(req->tmpl);
    free(req->group);
    free(req->new_name);

    free(req);
}

/**
 * 将删除模板组请求编码成JSON对象
 *
 * @param json_object JSON对象指针，用于存储编码结果
 * @param param 指向删除模板组请求的指针，作为要编码的数据
 * @return 成功返回0，失败返回非0值
 */
int neu_json_encode_template_del_group_req(void *json_object, void *param)
{
    int                                ret = 0;
    neu_json_template_del_group_req_t *req = param;

    neu_json_elem_t req_elems[] = { {
                                        .name      = "template",
                                        .t         = NEU_JSON_STR,
                                        .v.val_str = req->tmpl,
                                    },
                                    {
                                        .name      = "group",
                                        .t         = NEU_JSON_STR,
                                        .v.val_str = req->group,
                                    } };
    ret                         = neu_json_encode_field(json_object, req_elems,
                                NEU_JSON_ELEM_SIZE(req_elems));

    return ret;
}

/**
 * 从JSON字符串解码删除模板组请求
 *
 * @param buf 包含JSON数据的字符串
 * @param result 二级指针，用于返回解码后的删除模板组请求结构
 * @return 成功返回0，失败返回-1
 */
int neu_json_decode_template_del_group_req(
    char *buf, neu_json_template_del_group_req_t **result)
{
    int                                ret      = 0;
    void *                             json_obj = NULL;
    neu_json_template_del_group_req_t *req      = NULL;

    json_obj = neu_json_decode_new(buf);
    if (NULL == json_obj) {
        return -1;
    }

    req = calloc(1, sizeof(*req));
    if (NULL == req) {
        goto decode_fail;
    }

    neu_json_elem_t req_elems[] = { {
                                        .name = "template",
                                        .t    = NEU_JSON_STR,
                                    },
                                    {
                                        .name = "group",
                                        .t    = NEU_JSON_STR,
                                    } };
    ret       = neu_json_decode_by_json(json_obj, NEU_JSON_ELEM_SIZE(req_elems),
                                  req_elems);
    req->tmpl = req_elems[0].v.val_str;
    req->group = req_elems[1].v.val_str;

    if (ret != 0) {
        goto decode_fail;
    }

    *result = req;
    neu_json_decode_free(json_obj);
    return 0;

decode_fail:
    neu_json_decode_template_del_group_req_free(req);
    neu_json_decode_free(json_obj);
    return -1;
}

/**
 * 释放删除模板组请求结构的内存
 *
 * @param req 指向要释放的删除模板组请求结构的指针
 */
void neu_json_decode_template_del_group_req_free(
    neu_json_template_del_group_req_t *req)
{
    if (req) {
        free(req->tmpl);
        free(req->group);
        free(req);
    }
}

/**
 * 将修改模板标签请求编码成JSON对象
 *
 * @param json_obj JSON对象指针，用于存储编码结果
 * @param param 指向修改模板标签请求的指针，作为要编码的数据
 * @return 成功返回0，失败返回非0值
 */
int neu_json_encode_template_mod_tags_req(void *json_obj, void *param)
{
    int                               ret = 0;
    neu_json_template_mod_tags_req_t *req = param;

    void *tag_array = neu_json_array();
    if (NULL == tag_array) {
        return -1;
    }

    neu_json_tag_array_t arr = {
        .len  = req->n_tag,
        .tags = req->tags,
    };
    ret = neu_json_encode_tag_array(tag_array, &arr);
    if (0 != ret) {
        neu_json_encode_free(tag_array);
        return ret;
    }

    neu_json_elem_t req_elems[] = { {
                                        .name      = "template",
                                        .t         = NEU_JSON_STR,
                                        .v.val_str = req->tmpl,
                                    },
                                    {
                                        .name      = "group",
                                        .t         = NEU_JSON_STR,
                                        .v.val_str = req->group,
                                    },
                                    {
                                        .name         = "tags",
                                        .t            = NEU_JSON_OBJECT,
                                        .v.val_object = tag_array,
                                    } };

    ret = neu_json_encode_field(json_obj, req_elems,
                                NEU_JSON_ELEM_SIZE(req_elems));

    return ret;
}

/**
 * 从JSON字符串解码修改模板标签请求
 *
 * @param buf 包含JSON数据的字符串
 * @param result 二级指针，用于返回解码后的修改模板标签请求结构
 * @return 成功返回0，失败返回-1
 */
int neu_json_decode_template_mod_tags_req(
    char *buf, neu_json_template_mod_tags_req_t **result)
{
    int                               ret      = 0;
    void *                            json_obj = NULL;
    neu_json_template_mod_tags_req_t *req      = calloc(1, sizeof(*req));
    if (req == NULL) {
        return -1;
    }

    json_obj = neu_json_decode_new(buf);
    if (NULL == json_obj) {
        goto decode_fail;
    }

    neu_json_elem_t req_elems[] = {
        {
            .name = "template",
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
    if (ret != 0) {
        goto decode_fail;
    }

    neu_json_tag_array_t arr = { 0 };
    ret = neu_json_decode_tag_array_json(req_elems[2].v.val_object, &arr);
    if (ret != 0) {
        goto decode_fail;
    }
    if (arr.len <= 0) {
        goto decode_fail;
    }

    req->tmpl  = req_elems[0].v.val_str;
    req->group = req_elems[1].v.val_str;
    req->n_tag = arr.len;
    req->tags  = arr.tags;
    *result    = req;
    goto decode_exit;

decode_fail:
    free(req);
    free(req_elems[0].v.val_str);
    free(req_elems[1].v.val_str);
    ret = -1;

decode_exit:
    if (json_obj != NULL) {
        neu_json_decode_free(json_obj);
    }
    return ret;
}

/**
 * 释放修改模板标签请求结构的内存
 *
 * @param req 指向要释放的修改模板标签请求结构的指针
 */
void neu_json_decode_template_mod_tags_req_free(
    neu_json_template_mod_tags_req_t *req)
{
    if (NULL == req) {
        return;
    }

    neu_json_tag_array_t arr = {
        .len  = req->n_tag,
        .tags = req->tags,
    };
    neu_json_decode_tag_array_fini(&arr);

    free(req->tmpl);
    free(req->group);
    free(req);
}

/**
 * 将删除模板标签请求编码成JSON对象
 *
 * @param json_object JSON对象指针，用于存储编码结果
 * @param param 指向删除模板标签请求的指针，作为要编码的数据
 * @return 成功返回0，失败返回非0值
 */
int neu_json_encode_template_del_tags_req(void *json_object, void *param)
{
    int                               ret = 0;
    neu_json_template_del_tags_req_t *req = param;

    void *                        tag_array = neu_json_array();
    neu_json_del_tags_req_name_t *p_name    = req->tags;
    for (int i = 0; i < req->n_tags; i++) {
        neu_json_elem_t tag_elems[] = {
            {
                .name      = NULL,
                .t         = NEU_JSON_STR,
                .v.val_str = *p_name,
            },
        };
        tag_array = neu_json_encode_array_value(tag_array, tag_elems,
                                                NEU_JSON_ELEM_SIZE(tag_elems));
        p_name++;
    }

    neu_json_elem_t req_elems[] = { {
                                        .name      = "template",
                                        .t         = NEU_JSON_STR,
                                        .v.val_str = req->tmpl,
                                    },
                                    {
                                        .name      = "group",
                                        .t         = NEU_JSON_STR,
                                        .v.val_str = req->group,
                                    },
                                    {
                                        .name         = "tags",
                                        .t            = NEU_JSON_OBJECT,
                                        .v.val_object = tag_array,
                                    } };

    ret = neu_json_encode_field(json_object, req_elems,
                                NEU_JSON_ELEM_SIZE(req_elems));

    return ret;
}

/**
 * 从JSON字符串解码删除模板标签请求
 *
 * @param buf 包含JSON数据的字符串
 * @param result 二级指针，用于返回解码后的删除模板标签请求结构
 * @return 成功返回0，失败返回-1
 */
int neu_json_decode_template_del_tags_req(
    char *buf, neu_json_template_del_tags_req_t **result)
{
    int                               ret       = 0;
    void *                            json_obj  = NULL;
    neu_json_template_del_tags_req_t *req       = NULL;
    json_t *                          names_arr = NULL;

    req = calloc(1, sizeof(*req));
    if (req == NULL) {
        return -1;
    }

    json_obj = neu_json_decode_new(buf);
    if (NULL == json_obj) {
        goto error;
    }

    neu_json_elem_t req_elems[] = { {
                                        .name = "template",
                                        .t    = NEU_JSON_STR,
                                    },
                                    {
                                        .name = "group",
                                        .t    = NEU_JSON_STR,
                                    } };
    ret       = neu_json_decode_by_json(json_obj, NEU_JSON_ELEM_SIZE(req_elems),
                                  req_elems);
    req->tmpl = req_elems[0].v.val_str;
    req->group = req_elems[1].v.val_str;
    if (ret != 0) {
        goto error;
    }

    names_arr = json_object_get(json_obj, "tags");
    if (!json_is_array(names_arr)) {
        goto error;
    }

    size_t n_tags = json_array_size(names_arr);
    req->tags     = calloc(n_tags, sizeof(req->tags[0]));
    for (size_t i = 0; i < n_tags; i++) {
        req->n_tags += 1;
        json_t *name = json_array_get(names_arr, i);
        if (!json_is_string(name) ||
            NULL == (req->tags[i] = strdup(json_string_value(name)))) {
            goto error;
        }
    }

    *result = req;
    neu_json_decode_free(json_obj);
    return 0;

error:
    neu_json_decode_template_del_tags_req_free(req);
    neu_json_decode_free(json_obj);
    return -1;
}

/**
 * 释放删除模板标签请求结构的内存
 *
 * @param req 指向要释放的删除模板标签请求结构的指针
 */
void neu_json_decode_template_del_tags_req_free(
    neu_json_template_del_tags_req_t *req)
{
    if (NULL == req) {
        return;
    }

    free(req->tmpl);
    free(req->group);

    for (int i = 0; i < req->n_tags; i++) {
        free(req->tags[i]);
    }
    free(req->tags);

    free(req);
}

/**
 * 从JSON对象解码模板实例请求
 *
 * @param json_obj JSON对象指针，包含要解码的数据
 * @param req 指向模板实例请求结构的指针，用于存储解码结果
 * @return 成功返回0，失败返回非0值
 */
int decode_template_inst_req_json(void *                        json_obj,
                                  neu_json_template_inst_req_t *req)
{
    int ret = 0;

    neu_json_elem_t req_elems[] = {
        {
            .name = "name",
            .t    = NEU_JSON_STR,
        },
        {
            .name = "node",
            .t    = NEU_JSON_STR,
        },
    };
    ret = neu_json_decode_by_json(json_obj, NEU_JSON_ELEM_SIZE(req_elems),
                                  req_elems);
    if (0 != ret) {
        free(req_elems[0].v.val_str);
        free(req_elems[1].v.val_str);
        return ret;
    }

    req->name = req_elems[0].v.val_str;
    req->node = req_elems[1].v.val_str;
    return ret;
}

/**
 * 从JSON字符串解码模板实例请求
 *
 * @param buf 包含JSON数据的字符串
 * @param result 二级指针，用于返回解码后的模板实例请求结构
 * @return 成功返回0，失败返回非0值
 */
int neu_json_decode_template_inst_req(char *                         buf,
                                      neu_json_template_inst_req_t **result)
{
    int                           ret      = 0;
    neu_json_template_inst_req_t *req      = NULL;
    void *                        json_obj = NULL;

    req = calloc(1, sizeof(*req));
    if (req == NULL) {
        return -1;
    }

    json_obj = neu_json_decode_new(buf);
    if (NULL == json_obj) {
        free(req);
        return -1;
    }

    ret = decode_template_inst_req_json(json_obj, req);
    if (0 == ret) {
        *result = req;
    } else {
        free(req);
    }

    neu_json_decode_free(json_obj);
    return ret;
}

/**
 * 释放模板实例请求结构的内存
 *
 * @param req 指向要释放的模板实例请求结构的指针
 */
void neu_json_decode_template_inst_req_free(neu_json_template_inst_req_t *req)
{
    if (req) {
        free(req->name);
        free(req->node);
        free(req);
    }
}

/**
 * 从JSON字符串解码模板实例数组请求
 *
 * @param buf 包含JSON数据的字符串
 * @param result 二级指针，用于返回解码后的模板实例数组请求结构
 * @return 成功返回0，失败返回-1
 */
int neu_json_decode_template_insts_req(char *                          buf,
                                       neu_json_template_insts_req_t **result)
{
    neu_json_template_insts_req_t *req      = NULL;
    void *                         json_obj = NULL;

    req = calloc(1, sizeof(*req));
    if (req == NULL) {
        return -1;
    }

    json_obj = neu_json_decode_new(buf);
    if (NULL == json_obj) {
        free(req);
        return -1;
    }

    json_t *json_arr = json_object_get(json_obj, "nodes");
    if (NULL == json_arr) {
        nlog_error("decode no key: node");
        goto error;
    }

    if (!json_is_array(json_arr)) {
        nlog_error("decode key `node` is not array");
        goto error;
    }

    int len    = json_array_size(json_arr);
    req->insts = calloc(len, sizeof(req->insts[0]));
    if (NULL == req->insts) {
        goto error;
    }

    for (int i = 0; i < len; ++i) {
        if (0 !=
            decode_template_inst_req_json(json_array_get(json_arr, i),
                                          &req->insts[i])) {
            break;
        }
        req->len += 1;
    }

    *result = req;
    neu_json_decode_free(json_obj);
    return 0;

error:
    neu_json_decode_template_insts_req_free(req);
    neu_json_decode_free(json_obj);
    return -1;
}

/**
 * 释放模板实例数组请求结构的内存
 *
 * @param req 指向要释放的模板实例数组请求结构的指针
 */
void neu_json_decode_template_insts_req_free(neu_json_template_insts_req_t *req)
{
    if (req) {
        for (int i = 0; i < req->len; ++i) {
            free(req->insts[i].name);
            free(req->insts[i].node);
        }
        free(req->insts);
        free(req);
    }
}
