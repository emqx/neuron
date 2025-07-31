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

#include "define.h"
#include "errcodes.h"
#include "tag.h"
#include "utils/http.h"
#include "utils/http_handler.h"
#include "utils/log.h"
#include "json/neu_json_error.h"
#include "json/neu_json_fn.h"

#include "parser/neu_json_template.h"

#include "handle.h"

/**
 * @brief 根据JSON数据设置标签属性
 *
 * 将JSON解析得到的标签信息应用到neu_datatag_t结构体
 *
 * @param tag 目标标签结构体
 * @param json_tag 源JSON标签数据
 * @return int 成功返回0，失败返回错误码
 */
static int set_tag_by_json(neu_datatag_t *tag, neu_json_tag_t *json_tag)
{
    if (strlen(json_tag->name) >= NEU_TAG_NAME_LEN) {
        return NEU_ERR_TAG_NAME_TOO_LONG;
    }

    if (strlen(json_tag->address) >= NEU_TAG_ADDRESS_LEN) {
        return NEU_ERR_TAG_ADDRESS_TOO_LONG;
    }

    if (NULL == json_tag->description &&
        NULL == (json_tag->description = strdup(""))) {
        return NEU_ERR_EINTERNAL;
    }

    tag->name        = json_tag->name;
    tag->type        = json_tag->type;
    tag->address     = json_tag->address;
    tag->attribute   = json_tag->attribute;
    tag->precision   = json_tag->precision;
    tag->decimal     = json_tag->decimal;
    tag->description = json_tag->description;
    // ownership moved
    json_tag->name        = NULL;
    json_tag->address     = NULL;
    json_tag->description = NULL;

    if ((NEU_ATTRIBUTE_STATIC & json_tag->attribute) &&
        0 !=
            neu_tag_set_static_value_json(tag, json_tag->t, &json_tag->value)) {
        return NEU_ERR_EINTERNAL;
    }

    return 0;
}

/**
 * @brief 将JSON模板数据转移到命令结构体
 *
 * 解析JSON模板数据并将其移动到命令结构体中
 *
 * @param cmd 目标命令结构体
 * @param req 源JSON模板数据
 * @return int 成功返回0，失败返回错误码
 */
static int move_template_json(neu_req_add_template_t *cmd,
                              neu_json_template_t *   req)
{
    int ret = 0;

    if (strlen(req->name) >= NEU_TEMPLATE_NAME_LEN) {
        return NEU_ERR_TEMPLATE_NAME_TOO_LONG;
    }

    if (strlen(req->plugin) >= NEU_PLUGIN_NAME_LEN) {
        return NEU_ERR_PLUGIN_NAME_TOO_LONG;
    }

    cmd->n_group = 0;
    cmd->groups  = calloc(req->groups.len, sizeof(*cmd->groups));
    if (NULL == cmd->groups) {
        return NEU_ERR_EINTERNAL;
    }

    for (int i = 0; i < req->groups.len; i++) {
        ++cmd->n_group;
        neu_json_template_group_t *   req_grp = &req->groups.groups[i];
        neu_reqresp_template_group_t *cmd_grp = &cmd->groups[i];

        if (strlen(req_grp->name) >= NEU_GROUP_NAME_LEN) {
            ret = NEU_ERR_GROUP_NAME_TOO_LONG;
            goto error;
        }

        if (req_grp->interval < NEU_GROUP_INTERVAL_LIMIT) {
            ret = NEU_ERR_GROUP_PARAMETER_INVALID;
            goto error;
        }

        cmd_grp->n_tag = 0;
        cmd_grp->tags  = calloc(req_grp->tags.len, sizeof(*cmd_grp->tags));
        if (NULL == cmd_grp->tags) {
            ret = NEU_ERR_EINTERNAL;
            goto error;
        }

        strncpy(cmd_grp->name, req_grp->name, sizeof(cmd_grp->name));
        cmd_grp->interval = req_grp->interval;

        for (int j = 0; j < req_grp->tags.len; ++j) {
            ++cmd_grp->n_tag;
            neu_datatag_t * tag     = &cmd_grp->tags[j];
            neu_json_tag_t *req_tag = &req_grp->tags.tags[j];
            if (0 != (ret = set_tag_by_json(tag, req_tag))) {
                goto error;
            }
        }
    }

    strncpy(cmd->name, req->name, sizeof(cmd->name));
    strncpy(cmd->plugin, req->plugin, sizeof(cmd->plugin));

    return ret;

error:
    for (int i = 0; i < cmd->n_group; ++i) {
        for (int j = 0; j < cmd->groups[i].n_tag; ++j) {
            neu_tag_fini(&cmd->groups[i].tags[j]);
        }
        free(cmd->groups[i].tags);
    }
    free(cmd->groups);
    return ret;
}

/**
 * @brief 处理添加模板的HTTP请求
 *
 * 解析请求并添加一个新的模板
 *
 * @param aio NNG异步I/O对象，用于处理HTTP请求和响应
 */
void handle_add_template(nng_aio *aio)
{
    neu_plugin_t *plugin = neu_rest_get_plugin();

    NEU_PROCESS_HTTP_REQUEST_VALIDATE_JWT(
        aio, neu_json_template_t, neu_json_decode_template, {
            int ret = 0;

            neu_reqresp_head_t header = { 0 };
            header.type               = NEU_REQ_ADD_TEMPLATE;
            header.ctx                = aio;

            neu_req_add_template_t cmd = { 0 };
            ret                        = move_template_json(&cmd, req);
            if (0 == ret) {
                if (0 != neu_plugin_op(plugin, header, &cmd)) {
                    ret = NEU_ERR_IS_BUSY;
                    neu_reqresp_template_fini(&cmd);
                }
            }

            if (0 != ret) {
                NEU_JSON_RESPONSE_ERROR(
                    ret, { neu_http_response(aio, ret, result_error); });
            }
        })
}

/**
 * @brief 处理删除模板的HTTP请求
 *
 * 解析请求并删除指定名称的模板
 *
 * @param aio NNG异步I/O对象，用于处理HTTP请求和响应
 */
void handle_del_template(nng_aio *aio)
{
    neu_plugin_t *plugin = neu_rest_get_plugin();

    NEU_VALIDATE_JWT(aio);

    neu_req_del_template_t cmd = { 0 };

    int ret = neu_http_get_param_str(aio, "name", cmd.name, sizeof(cmd.name));
    // optional parameter
    if (-2 != ret && (ret <= 0 || sizeof(cmd.name) <= (size_t) ret)) {
        NEU_JSON_RESPONSE_ERROR(NEU_ERR_PARAM_IS_WRONG, {
            neu_http_response(aio, NEU_ERR_PARAM_IS_WRONG, result_error);
        });
        return;
    }

    neu_reqresp_head_t header = {
        .ctx  = aio,
        .type = NEU_REQ_DEL_TEMPLATE,
    };

    if (0 != neu_plugin_op(plugin, header, &cmd)) {
        NEU_JSON_RESPONSE_ERROR(NEU_ERR_IS_BUSY, {
            neu_http_response(aio, NEU_ERR_IS_BUSY, result_error);
        });
    }
}

/**
 * @brief 处理获取模板的HTTP请求
 *
 * 解析请求并获取指定名称的模板或所有模板
 *
 * @param aio NNG异步I/O对象，用于处理HTTP请求和响应
 */
void handle_get_template(nng_aio *aio)
{
    neu_plugin_t *plugin = neu_rest_get_plugin();

    NEU_VALIDATE_JWT(aio);

    neu_reqresp_head_t header = {
        .ctx = aio,
    };
    neu_req_get_template_t cmd = { 0 };

    int ret = neu_http_get_param_str(aio, "name", cmd.name, sizeof(cmd.name));
    if (0 < ret && (size_t) ret < sizeof(cmd.name)) {
        // valid name parameter provided
        header.type = NEU_REQ_GET_TEMPLATE;
    } else if (-2 == ret) {
        // no name parameter
        header.type = NEU_REQ_GET_TEMPLATES;
    } else {
        // invalid name parameter
        NEU_JSON_RESPONSE_ERROR(NEU_ERR_PARAM_IS_WRONG, {
            neu_http_response(aio, NEU_ERR_PARAM_IS_WRONG, result_error);
        });
        return;
    }

    if (0 != neu_plugin_op(plugin, header, &cmd)) {
        NEU_JSON_RESPONSE_ERROR(NEU_ERR_IS_BUSY, {
            neu_http_response(aio, NEU_ERR_IS_BUSY, result_error);
        });
    }
}

/**
 * @brief 处理获取单个模板的响应
 *
 * 将获取的模板数据格式化为JSON并发送HTTP响应
 *
 * @param aio NNG异步I/O对象，用于处理HTTP响应
 * @param resp 获取的模板响应数据
 */
void handle_get_template_resp(nng_aio *aio, neu_resp_get_template_t *resp)
{
    int                        rv     = 0;
    int                        i      = 0;
    char *                     result = NULL;
    neu_json_template_group_t *grp    = NULL;

    if (0 == resp->n_group) {
        // no groups
        goto end;
    }

    grp = calloc(resp->n_group, sizeof(*grp));
    if (NULL == grp) {
        rv = NEU_ERR_EINTERNAL;
        goto end;
    }

    for (i = 0; i < resp->n_group; ++i) {
        grp[i].name     = resp->groups[i].name;
        grp[i].interval = resp->groups[i].interval;

        if (0 == resp->groups[i].n_tag) {
            // no tags
            continue;
        }

        neu_json_tag_array_t tags = { 0 };
        tags.len                  = resp->groups[i].n_tag;
        tags.tags                 = calloc(tags.len, sizeof(*tags.tags));
        if (NULL == tags.tags) {
            rv = NEU_ERR_EINTERNAL;
            goto end;
        }

        for (int j = 0; j < tags.len; ++j) {
            neu_datatag_t *tag       = &resp->groups[i].tags[j];
            tags.tags[j].name        = tag->name;
            tags.tags[j].address     = tag->address;
            tags.tags[j].description = tag->description;
            tags.tags[j].type        = tag->type;
            tags.tags[j].attribute   = tag->attribute;
            tags.tags[j].precision   = tag->precision;
            tags.tags[j].decimal     = tag->decimal;
            if (neu_tag_attribute_test(tag, NEU_ATTRIBUTE_STATIC)) {
                neu_tag_get_static_value_json(tag, &tags.tags[j].t,
                                              &tags.tags[j].value);
            } else {
                tags.tags[j].t = NEU_JSON_UNDEFINE;
            }
        }

        grp[i].tags = tags;
    }

end:
    if (0 == rv) {
        neu_json_template_t res = {
            .name          = resp->name,
            .plugin        = resp->plugin,
            .groups.len    = resp->n_group,
            .groups.groups = grp,
        };
        neu_json_encode_by_fn(&res, neu_json_encode_template, &result);
    } else {
        neu_json_error_resp_t error_code = { .error = rv };
        neu_json_encode_by_fn(&error_code, neu_json_encode_error_resp, &result);
    }

    neu_http_ok(aio, result);

    while (--i >= 0) {
        free(grp[i].tags.tags);
    }
    free(grp);
    free(result);
    neu_reqresp_template_fini(resp);
    return;
}

/**
 * @brief 处理获取多个模板的响应
 *
 * 将获取的多个模板数据格式化为JSON并发送HTTP响应
 *
 * @param aio NNG异步I/O对象，用于处理HTTP响应
 * @param resp 获取的多个模板响应数据
 */
void handle_get_templates_resp(nng_aio *aio, neu_resp_get_templates_t *resp)

{
    neu_json_template_info_t *info = calloc(resp->n_templates, sizeof(*info));
    if (NULL == info) {
        NEU_JSON_RESPONSE_ERROR(NEU_ERR_EINTERNAL, {
            neu_http_response(aio, NEU_ERR_EINTERNAL, result_error);
        });
        neu_resp_get_templates_fini(resp);
        return;
    }

    for (int i = 0; i < resp->n_templates; ++i) {
        info[i].name   = resp->templates[i].name;
        info[i].plugin = resp->templates[i].plugin;
    }

    char *                         result = NULL;
    neu_json_template_info_array_t res    = {
        .len  = resp->n_templates,
        .info = info,
    };
    neu_json_encode_by_fn(&res, neu_json_encode_get_templates_resp, &result);

    neu_http_ok(aio, result);
    free(result);
    free(info);
    neu_resp_get_templates_fini(resp);
    return;
}

/**
 * @brief 发送添加模板组请求
 *
 * 解析并验证添加模板组请求参数，然后发送请求
 *
 * @param aio NNG异步I/O对象，用于处理HTTP响应
 * @param req 添加模板组请求数据
 * @return int 成功返回0，失败返回错误码
 */
static inline int
send_template_add_group_req(nng_aio *                          aio,
                            neu_json_template_add_group_req_t *req)
{
    neu_plugin_t *plugin = neu_rest_get_plugin();

    if (strlen(req->tmpl) >= NEU_TEMPLATE_NAME_LEN) {
        return NEU_ERR_TEMPLATE_NAME_TOO_LONG;
    } else if (strlen(req->group) >= NEU_GROUP_NAME_LEN) {
        return NEU_ERR_GROUP_NAME_TOO_LONG;
    } else if (req->interval < NEU_GROUP_INTERVAL_LIMIT) {
        return NEU_ERR_GROUP_PARAMETER_INVALID;
    }

    neu_req_add_template_group_t cmd = { 0 };
    strcpy(cmd.tmpl, req->tmpl);
    strcpy(cmd.group, req->group);
    cmd.interval = req->interval;

    neu_reqresp_head_t header = {
        .ctx  = aio,
        .type = NEU_REQ_ADD_TEMPLATE_GROUP,
    };

    if (0 != neu_plugin_op(plugin, header, &cmd)) {
        return NEU_ERR_IS_BUSY;
    }

    return 0;
}

/**
 * @brief 处理添加模板组的HTTP请求
 *
 * 解析请求并添加一个新的模板组
 *
 * @param aio NNG异步I/O对象，用于处理HTTP请求和响应
 */
void handle_add_template_group(nng_aio *aio)
{
    NEU_PROCESS_HTTP_REQUEST_VALIDATE_JWT(
        aio, neu_json_template_add_group_req_t,
        neu_json_decode_template_add_group_req, {
            int ret = send_template_add_group_req(aio, req);
            if (ret != 0) {
                NEU_JSON_RESPONSE_ERROR(
                    ret, { neu_http_response(aio, ret, result_error); });
            }
        })
}

/**
 * @brief 发送更新模板组请求
 *
 * 解析并验证更新模板组请求参数，然后发送请求
 *
 * @param aio NNG异步I/O对象，用于处理HTTP响应
 * @param req 更新模板组请求数据
 * @return int 成功返回0，失败返回错误码
 */
static inline int
send_template_update_group_req(nng_aio *                             aio,
                               neu_json_template_update_group_req_t *req)
{
    neu_plugin_t *plugin = neu_rest_get_plugin();

    if (strlen(req->tmpl) >= NEU_TEMPLATE_NAME_LEN) {
        return NEU_ERR_TEMPLATE_NAME_TOO_LONG;
    } else if (strlen(req->group) >= NEU_GROUP_NAME_LEN) {
        return NEU_ERR_GROUP_NAME_TOO_LONG;
    } else if (req->set_interval &&
               (req->interval < NEU_GROUP_INTERVAL_LIMIT ||
                req->interval > UINT32_MAX)) {
        return NEU_ERR_GROUP_PARAMETER_INVALID;
    }

    // for backward compatibility,
    // `new_name` or `interval` (inclusive) should be provided
    if (!req->new_name && !req->set_interval) {
        return NEU_ERR_BODY_IS_WRONG;
    }

    neu_req_update_template_group_t cmd = { 0 };

    if (req->new_name) {
        // if `new_name` is provided, then it should be valid
        int len = strlen(req->new_name);
        if (0 == len) {
            return NEU_ERR_BODY_IS_WRONG;
        } else if (len >= NEU_GROUP_NAME_LEN) {
            return NEU_ERR_GROUP_NAME_TOO_LONG;
        }
        strcpy(cmd.new_name, req->new_name);
    } else {
        // if `new_name` is omitted, then keep the group name
        strcpy(cmd.new_name, req->group);
    }

    strcpy(cmd.tmpl, req->tmpl);
    strcpy(cmd.group, req->group);
    cmd.interval = req->interval;

    neu_reqresp_head_t header = {
        .ctx  = aio,
        .type = NEU_REQ_UPDATE_TEMPLATE_GROUP,
    };

    if (0 != neu_plugin_op(plugin, header, &cmd)) {
        return NEU_ERR_IS_BUSY;
    }

    return 0;
}

/**
 * @brief 处理更新模板组的HTTP请求
 *
 * 解析请求并更新指定的模板组
 *
 * @param aio NNG异步I/O对象，用于处理HTTP请求和响应
 */
void handle_update_template_group(nng_aio *aio)
{
    NEU_PROCESS_HTTP_REQUEST_VALIDATE_JWT(
        aio, neu_json_template_update_group_req_t,
        neu_json_decode_template_update_group_req, {
            int ret = send_template_update_group_req(aio, req);
            if (ret != 0) {
                NEU_JSON_RESPONSE_ERROR(
                    ret, { neu_http_response(aio, ret, result_error); });
            }
        })
}

/**
 * @brief 发送删除模板组请求
 *
 * 解析并验证删除模板组请求参数，然后发送请求
 *
 * @param aio NNG异步I/O对象，用于处理HTTP响应
 * @param req 删除模板组请求数据
 * @return int 成功返回0，失败返回错误码
 */
static inline int
send_template_del_group_req(nng_aio *                          aio,
                            neu_json_template_del_group_req_t *req)
{
    neu_plugin_t *plugin = neu_rest_get_plugin();

    if (strlen(req->tmpl) >= NEU_TEMPLATE_NAME_LEN) {
        return NEU_ERR_TEMPLATE_NAME_TOO_LONG;
    }

    if (strlen(req->group) >= NEU_GROUP_NAME_LEN) {
        return NEU_ERR_GROUP_NAME_TOO_LONG;
    }

    neu_req_add_template_tag_t cmd = { 0 };
    strcpy(cmd.tmpl, req->tmpl);
    strcpy(cmd.group, req->group);

    neu_reqresp_head_t header = {
        .ctx  = aio,
        .type = NEU_REQ_DEL_TEMPLATE_GROUP,
    };

    if (0 != neu_plugin_op(plugin, header, &cmd)) {
        return NEU_ERR_IS_BUSY;
    }

    return 0;
}

/**
 * @brief 处理删除模板组的HTTP请求
 *
 * 解析请求并删除指定的模板组
 *
 * @param aio NNG异步I/O对象，用于处理HTTP请求和响应
 */
void handle_del_template_group(nng_aio *aio)
{
    NEU_PROCESS_HTTP_REQUEST_VALIDATE_JWT(
        aio, neu_json_template_del_group_req_t,
        neu_json_decode_template_del_group_req, {
            int ret = send_template_del_group_req(aio, req);
            if (0 != ret) {
                NEU_JSON_RESPONSE_ERROR(
                    ret, { neu_http_response(aio, ret, result_error); });
            }
        })
}

/**
 * @brief 发送模板修改标签请求
 *
 * 解析并验证模板修改标签请求参数，然后发送请求
 *
 * @param aio NNG异步I/O对象，用于处理HTTP响应
 * @param type 请求类型，如添加或更新标签
 * @param req 模板修改标签请求数据
 * @return int 成功返回0，失败返回错误码
 */
static int send_template_mod_tags_req(nng_aio *aio, neu_reqresp_type_e type,
                                      neu_json_template_mod_tags_req_t *req)
{
    int           ret    = 0;
    neu_plugin_t *plugin = neu_rest_get_plugin();

    if (strlen(req->tmpl) >= NEU_TEMPLATE_NAME_LEN) {
        return NEU_ERR_TEMPLATE_NAME_TOO_LONG;
    }

    if (strlen(req->group) >= NEU_GROUP_NAME_LEN) {
        return NEU_ERR_GROUP_NAME_TOO_LONG;
    }

    neu_req_add_template_tag_t cmd = { 0 };
    strcpy(cmd.tmpl, req->tmpl);
    strcpy(cmd.group, req->group);

    cmd.tags = calloc(req->n_tag, sizeof(neu_datatag_t));
    if (NULL == cmd.tags) {
        return NEU_ERR_EINTERNAL;
    }

    for (int i = 0; i < req->n_tag; i++) {
        cmd.n_tag += 1;
        if (0 != (ret = set_tag_by_json(&cmd.tags[i], &req->tags[i]))) {
            neu_req_add_template_tag_fini(&cmd);
            return ret;
        }
    }

    neu_reqresp_head_t header = {
        .ctx  = aio,
        .type = type,
    };

    if (0 != neu_plugin_op(plugin, header, &cmd)) {
        neu_req_add_template_tag_fini(&cmd);
        return NEU_ERR_IS_BUSY;
    }

    return ret;
}

/**
 * @brief 发送删除模板标签请求
 *
 * 解析并验证删除模板标签请求参数，然后发送请求
 *
 * @param aio NNG异步I/O对象，用于处理HTTP响应
 * @param req 删除模板标签请求数据
 * @return int 成功返回0，失败返回错误码
 */
static int send_template_del_tags_req(nng_aio *                         aio,
                                      neu_json_template_del_tags_req_t *req)
{
    int           ret    = 0;
    neu_plugin_t *plugin = neu_rest_get_plugin();

    if (strlen(req->tmpl) >= NEU_TEMPLATE_NAME_LEN) {
        return NEU_ERR_TEMPLATE_NAME_TOO_LONG;
    }

    if (strlen(req->group) >= NEU_GROUP_NAME_LEN) {
        return NEU_ERR_GROUP_NAME_TOO_LONG;
    }

    neu_req_del_template_tag_t cmd = { 0 };
    strcpy(cmd.tmpl, req->tmpl);
    strcpy(cmd.group, req->group);

    cmd.tags = calloc(req->n_tags, sizeof(char *));
    if (NULL == cmd.tags) {
        return NEU_ERR_EINTERNAL;
    }

    for (int i = 0; i < req->n_tags; i++) {
        if (strlen(req->tags[i]) >= NEU_TAG_NAME_LEN) {
            neu_req_del_template_tag_fini(&cmd);
            return NEU_ERR_TAG_NAME_TOO_LONG;
        }
        cmd.tags[i]  = req->tags[i];
        req->tags[i] = NULL; // ownership moved
        cmd.n_tag += 1;
    }

    neu_reqresp_head_t header = {
        .ctx  = aio,
        .type = NEU_REQ_DEL_TEMPLATE_TAG,
    };

    if (ret != neu_plugin_op(plugin, header, &cmd)) {
        neu_req_del_template_tag_fini(&cmd);
        ret = NEU_ERR_IS_BUSY;
    }

    return ret;
}

/**
 * @brief 处理添加模板标签的HTTP请求
 *
 * 解析请求并添加模板标签
 *
 * @param aio NNG异步I/O对象，用于处理HTTP请求和响应
 */
void handle_add_template_tags(nng_aio *aio)
{
    NEU_PROCESS_HTTP_REQUEST_VALIDATE_JWT(
        aio, neu_json_template_mod_tags_req_t,
        neu_json_decode_template_mod_tags_req, {
            int ret =
                send_template_mod_tags_req(aio, NEU_REQ_ADD_TEMPLATE_TAG, req);
            if (0 != ret) {
                NEU_JSON_RESPONSE_ERROR(
                    ret, { neu_http_response(aio, ret, result_error); });
            }
        })
}

/**
 * @brief 处理更新模板标签的HTTP请求
 *
 * 解析请求并更新模板标签
 *
 * @param aio NNG异步I/O对象，用于处理HTTP请求和响应
 */
void handle_update_template_tags(nng_aio *aio)
{
    NEU_PROCESS_HTTP_REQUEST_VALIDATE_JWT(
        aio, neu_json_template_mod_tags_req_t,
        neu_json_decode_template_mod_tags_req, {
            int ret = send_template_mod_tags_req(
                aio, NEU_REQ_UPDATE_TEMPLATE_TAG, req);
            if (0 != ret) {
                NEU_JSON_RESPONSE_ERROR(
                    ret, { neu_http_response(aio, ret, result_error); });
            }
        })
}

/**
 * @brief 处理删除模板标签的HTTP请求
 *
 * 解析请求并删除模板标签
 *
 * @param aio NNG异步I/O对象，用于处理HTTP请求和响应
 */
void handle_del_template_tags(nng_aio *aio)
{
    NEU_PROCESS_HTTP_REQUEST_VALIDATE_JWT(
        aio, neu_json_template_del_tags_req_t,
        neu_json_decode_template_del_tags_req, {
            int ret = send_template_del_tags_req(aio, req);
            if (0 != ret) {
                NEU_JSON_RESPONSE_ERROR(
                    ret, { neu_http_response(aio, ret, result_error); });
            }
        })
}

/**
 * @brief 处理获取模板标签的HTTP请求
 *
 * 解析请求并获取模板标签信息
 *
 * @param aio NNG异步I/O对象，用于处理HTTP请求和响应
 */
void handle_get_template_tags(nng_aio *aio)
{
    neu_plugin_t *plugin = neu_rest_get_plugin();
    int           ret    = 0;

    NEU_VALIDATE_JWT(aio);

    neu_reqresp_head_t header = {
        .ctx  = aio,
        .type = NEU_REQ_GET_TEMPLATE_TAG,
    };

    neu_req_get_template_tag_t cmd = { 0 };

    ret = neu_http_get_param_str(aio, "template", cmd.tmpl, sizeof(cmd.tmpl));
    // required parameter
    if (ret <= 0 || sizeof(cmd.tmpl) <= (size_t) ret) {
        ret = NEU_ERR_PARAM_IS_WRONG;
        goto end;
    }

    ret = neu_http_get_param_str(aio, "group", cmd.group, sizeof(cmd.group));
    // required parameter
    if (ret <= 0 || sizeof(cmd.tmpl) <= (size_t) ret) {
        ret = NEU_ERR_PARAM_IS_WRONG;
        goto end;
    }

    ret = neu_http_get_param_str(aio, "name", cmd.name, sizeof(cmd.name));
    // optional parameter
    if (-2 != ret && (ret <= 0 || sizeof(cmd.name) <= (size_t) ret)) {
        ret = NEU_ERR_PARAM_IS_WRONG;
        goto end;
    }

    ret = neu_plugin_op(plugin, header, &cmd);
    if (0 != ret) {
        ret = NEU_ERR_IS_BUSY;
    }

end:
    if (0 != ret) {
        NEU_JSON_RESPONSE_ERROR(ret,
                                { neu_http_response(aio, ret, result_error); });
    }
}

/**
 * @brief 发送模板实例化请求
 *
 * 解析并验证模板实例化请求参数，然后发送请求
 *
 * @param aio NNG异步I/O对象，用于处理HTTP响应
 * @param req 模板实例化请求数据
 * @return int 成功返回0，失败返回错误码
 */
static inline int send_template_inst_req(nng_aio *                     aio,
                                         neu_json_template_inst_req_t *req)
{
    neu_plugin_t *plugin = neu_rest_get_plugin();

    if (strlen(req->name) >= NEU_TEMPLATE_NAME_LEN) {
        return NEU_ERR_TEMPLATE_NAME_TOO_LONG;
    }

    if (strlen(req->node) >= NEU_NODE_NAME_LEN) {
        return NEU_ERR_PLUGIN_NAME_TOO_LONG;
    }

    neu_req_inst_template_t cmd = { 0 };
    strncpy(cmd.tmpl, req->name, sizeof(cmd.tmpl));
    strncpy(cmd.node, req->node, sizeof(cmd.node));

    neu_reqresp_head_t header = {
        .ctx  = aio,
        .type = NEU_REQ_INST_TEMPLATE,
    };

    if (0 != neu_plugin_op(plugin, header, &cmd)) {
        return NEU_ERR_IS_BUSY;
    }

    return 0;
}

/**
 * @brief 处理实例化模板的HTTP请求
 *
 * 解析请求并实例化指定模板
 *
 * @param aio NNG异步I/O对象，用于处理HTTP请求和响应
 */
void handle_instantiate_template(nng_aio *aio)
{
    NEU_PROCESS_HTTP_REQUEST_VALIDATE_JWT(
        aio, neu_json_template_inst_req_t, neu_json_decode_template_inst_req, {
            int ret = send_template_inst_req(aio, req);
            if (0 != ret) {
                NEU_JSON_RESPONSE_ERROR(
                    ret, { neu_http_response(aio, ret, result_error); });
            }
        })
}

/**
 * @brief 发送批量模板实例化请求
 *
 * 解析并验证批量模板实例化请求参数，然后发送请求
 *
 * @param aio NNG异步I/O对象，用于处理HTTP响应
 * @param req 批量模板实例化请求数据
 * @return int 成功返回0，失败返回错误码
 */
static inline int send_template_insts_req(nng_aio *                      aio,
                                          neu_json_template_insts_req_t *req)
{
    neu_plugin_t *plugin = neu_rest_get_plugin();

    for (int i = 0; i < req->len; ++i) {
        if (strlen(req->insts[i].name) >= NEU_TEMPLATE_NAME_LEN) {
            return NEU_ERR_TEMPLATE_NAME_TOO_LONG;
        }

        if (strlen(req->insts[i].node) >= NEU_NODE_NAME_LEN) {
            return NEU_ERR_PLUGIN_NAME_TOO_LONG;
        }
    }

    neu_req_inst_templates_t cmd = {
        .n_inst = req->len,
        .insts  = (neu_req_inst_templates_info_t *) req->insts,
    };

    neu_reqresp_head_t header = {
        .ctx  = aio,
        .type = NEU_REQ_INST_TEMPLATES,
    };

    if (0 != neu_plugin_op(plugin, header, &cmd)) {
        return NEU_ERR_IS_BUSY;
    }

    req->len   = 0;
    req->insts = NULL; // ownership moved
    return 0;
}

/**
 * @brief 处理批量实例化模板的HTTP请求
 *
 * 解析请求并批量实例化指定模板
 *
 * @param aio NNG异步I/O对象，用于处理HTTP请求和响应
 */
void handle_instantiate_templates(nng_aio *aio)
{
    NEU_PROCESS_HTTP_REQUEST_VALIDATE_JWT(
        aio, neu_json_template_insts_req_t, neu_json_decode_template_insts_req,
        {
            int ret = send_template_insts_req(aio, req);
            if (0 != ret) {
                NEU_JSON_RESPONSE_ERROR(
                    ret, { neu_http_response(aio, ret, result_error); });
            }
        })
}

/**
 * @brief 处理获取模板组信息的HTTP请求
 *
 * 解析请求并获取指定模板的组信息
 *
 * @param aio NNG异步I/O对象，用于处理HTTP请求和响应
 */
void handle_get_template_group(nng_aio *aio)
{
    neu_plugin_t *plugin = neu_rest_get_plugin();

    NEU_VALIDATE_JWT(aio);

    neu_reqresp_head_t header = {
        .ctx  = aio,
        .type = NEU_REQ_GET_TEMPLATE_GROUP,
    };

    neu_req_get_template_group_t cmd = { 0 };
    int ret = neu_http_get_param_str(aio, "name", cmd.tmpl, sizeof(cmd.tmpl));
    // required parameter
    if (ret <= 0 || sizeof(cmd.tmpl) <= (size_t) ret) {
        NEU_JSON_RESPONSE_ERROR(NEU_ERR_PARAM_IS_WRONG, {
            neu_http_response(aio, NEU_ERR_PARAM_IS_WRONG, result_error);
        });
        return;
    }

    ret = neu_plugin_op(plugin, header, &cmd);
    if (ret != 0) {
        NEU_JSON_RESPONSE_ERROR(NEU_ERR_IS_BUSY, {
            neu_http_response(aio, NEU_ERR_IS_BUSY, result_error);
        });
    }
}
