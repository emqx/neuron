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

#include "plugin.h"
#include "utils/log.h"
#include "json/neu_json_fn.h"
#include "json/neu_json_rw.h"

#include "handle.h"
#include "utils/http.h"

#include "rw_handle.h"

/**
 * @brief 处理读取请求的HTTP处理函数
 *
 * 此函数处理来自客户端的读取请求，验证JWT令牌，解析读取命令，
 * 并将命令发送给插件进行处理。
 *
 * @param aio NNG异步I/O对象，包含HTTP请求和响应信息
 */
void handle_read(nng_aio *aio)
{
    neu_plugin_t *plugin = neu_rest_get_plugin();

    NEU_PROCESS_HTTP_REQUEST_VALIDATE_JWT(
        aio, neu_json_read_req_t, neu_json_decode_read_req, {
            int                  ret    = 0;
            neu_reqresp_head_t   header = { 0 };
            neu_req_read_group_t cmd    = { 0 };

            header.ctx  = aio;
            header.type = NEU_REQ_READ_GROUP;

            cmd.driver = req->node;
            cmd.group  = req->group;
            cmd.sync   = req->sync;
            req->node  = NULL;
            req->group = NULL;
            ret        = neu_plugin_op(plugin, header, &cmd);
            if (ret != 0) {
                neu_req_read_group_fini(&cmd);
                NEU_JSON_RESPONSE_ERROR(NEU_ERR_IS_BUSY, {
                    neu_http_response(aio, NEU_ERR_IS_BUSY, result_error);
                });
            }
        })
}

/**
 * @brief 处理写入请求的HTTP处理函数
 *
 * 此函数处理来自客户端的标签写入请求，验证JWT令牌，解析写入命令，
 * 检查值的大小是否合法，并将命令发送给插件进行处理。
 *
 * @param aio NNG异步I/O对象，包含HTTP请求和响应信息
 */
void handle_write(nng_aio *aio)
{
    neu_plugin_t *plugin = neu_rest_get_plugin();

    NEU_PROCESS_HTTP_REQUEST_VALIDATE_JWT(
        aio, neu_json_write_req_t, neu_json_decode_write_req, {
            neu_reqresp_head_t  header = { 0 };
            neu_req_write_tag_t cmd    = { 0 };

            if (req->t == NEU_JSON_STR &&
                strlen(req->value.val_str) >= NEU_VALUE_SIZE) {
                NEU_JSON_RESPONSE_ERROR(NEU_ERR_STRING_TOO_LONG, {
                    neu_http_response(aio, NEU_ERR_STRING_TOO_LONG,
                                      result_error);
                });
                return;
            }

            if (req->t == NEU_JSON_BYTES &&
                req->value.val_bytes.length > NEU_VALUE_SIZE) {
                NEU_JSON_RESPONSE_ERROR(NEU_ERR_STRING_TOO_LONG, {
                    neu_http_response(aio, NEU_ERR_STRING_TOO_LONG,
                                      result_error);
                });
                return;
            }

            nng_http_req *nng_req = nng_aio_get_input(aio, 0);
            nlog_notice("<%p> req %s %s", aio, nng_http_req_get_method(nng_req),
                        nng_http_req_get_uri(nng_req));

            header.ctx  = aio;
            header.type = NEU_REQ_WRITE_TAG;

            cmd.driver = req->node;
            cmd.group  = req->group;
            cmd.tag    = req->tag;
            req->node  = NULL; // ownership moved
            req->group = NULL; // ownership moved
            req->tag   = NULL; // ownership moved

            switch (req->t) {
            case NEU_JSON_INT:
                cmd.value.type      = NEU_TYPE_INT64;
                cmd.value.value.u64 = req->value.val_int;
                break;
            case NEU_JSON_STR:
                cmd.value.type = NEU_TYPE_STRING;
                strcpy(cmd.value.value.str, req->value.val_str);
                break;
            case NEU_JSON_BYTES:
                cmd.value.type               = NEU_TYPE_BYTES;
                cmd.value.value.bytes.length = req->value.val_bytes.length;
                memcpy(cmd.value.value.bytes.bytes, req->value.val_bytes.bytes,
                       req->value.val_bytes.length);
                break;
            case NEU_JSON_DOUBLE:
                cmd.value.type      = NEU_TYPE_DOUBLE;
                cmd.value.value.d64 = req->value.val_double;
                break;
            case NEU_JSON_BOOL:
                cmd.value.type          = NEU_TYPE_BOOL;
                cmd.value.value.boolean = req->value.val_bool;
                break;
            default:
                assert(false);
                break;
            }

            int ret = neu_plugin_op(plugin, header, &cmd);
            if (ret != 0) {
                neu_req_write_tag_fini(&cmd);
                NEU_JSON_RESPONSE_ERROR(NEU_ERR_IS_BUSY, {
                    neu_http_response(aio, NEU_ERR_IS_BUSY, result_error);
                });
            }
        })
}

/**
 * @brief 处理多标签写入请求的HTTP处理函数
 *
 * 此函数处理来自客户端的多标签写入请求，验证JWT令牌，解析写入命令，
 * 检查字符串值的大小是否合法，并将命令发送给插件进行处理。
 *
 * @param aio NNG异步I/O对象，包含HTTP请求和响应信息
 */
void handle_write_tags(nng_aio *aio)
{
    neu_plugin_t *plugin = neu_rest_get_plugin();

    NEU_PROCESS_HTTP_REQUEST_VALIDATE_JWT(
        aio, neu_json_write_tags_req_t, neu_json_decode_write_tags_req, {
            neu_reqresp_head_t   header = { 0 };
            neu_req_write_tags_t cmd    = { 0 };

            for (int i = 0; i < req->n_tag; i++) {
                if (req->tags[i].t == NEU_JSON_STR) {
                    if (strlen(req->tags[i].value.val_str) >= NEU_VALUE_SIZE) {
                        NEU_JSON_RESPONSE_ERROR(NEU_ERR_STRING_TOO_LONG, {
                            neu_http_response(aio, NEU_ERR_STRING_TOO_LONG,
                                              result_error);
                        });
                        return;
                    }
                }
            }

            nng_http_req *nng_req = nng_aio_get_input(aio, 0);
            nlog_notice("<%p> req %s %s", aio, nng_http_req_get_method(nng_req),
                        nng_http_req_get_uri(nng_req));
            header.ctx  = aio;
            header.type = NEU_REQ_WRITE_TAGS;

            cmd.driver = req->node;
            cmd.group  = req->group;
            cmd.n_tag  = req->n_tag;
            cmd.tags   = calloc(cmd.n_tag, sizeof(neu_resp_tag_value_t));
            req->node  = NULL; // ownership moved
            req->group = NULL; // ownership moved

            for (int i = 0; i < cmd.n_tag; i++) {
                strcpy(cmd.tags[i].tag, req->tags[i].tag);
                switch (req->tags[i].t) {
                case NEU_JSON_INT:
                    cmd.tags[i].value.type      = NEU_TYPE_INT64;
                    cmd.tags[i].value.value.i64 = req->tags[i].value.val_int;
                    break;
                case NEU_JSON_STR:
                    cmd.tags[i].value.type = NEU_TYPE_STRING;
                    strcpy(cmd.tags[i].value.value.str,
                           req->tags[i].value.val_str);
                    break;
                case NEU_JSON_DOUBLE:
                    cmd.tags[i].value.type      = NEU_TYPE_DOUBLE;
                    cmd.tags[i].value.value.d64 = req->tags[i].value.val_double;
                    break;
                case NEU_JSON_BOOL:
                    cmd.tags[i].value.type = NEU_TYPE_BOOL;
                    cmd.tags[i].value.value.boolean =
                        req->tags[i].value.val_bool;
                    break;
                case NEU_JSON_BYTES:
                    cmd.tags[i].value.type = NEU_TYPE_BYTES;
                    cmd.tags[i].value.value.bytes.length =
                        req->tags[i].value.val_bytes.length;
                    memcpy(cmd.tags[i].value.value.bytes.bytes,
                           req->tags[i].value.val_bytes.bytes,
                           req->tags[i].value.val_bytes.length);
                    break;
                default:
                    assert(false);
                    break;
                }
            }

            int ret = neu_plugin_op(plugin, header, &cmd);
            if (ret != 0) {
                neu_req_write_tags_fini(&cmd);
                NEU_JSON_RESPONSE_ERROR(NEU_ERR_IS_BUSY, {
                    neu_http_response(aio, NEU_ERR_IS_BUSY, result_error);
                });
            }
        })
}

/**
 * @brief 将JSON请求转换为内部命令结构
 *
 * 此函数将解析后的JSON请求转换为内部命令结构，处理内存分配和值类型转换。
 *
 * @param req 解析后的JSON请求结构
 * @param cmd 输出的内部命令结构
 */
static void trans(neu_json_write_gtags_req_t *req, neu_req_write_gtags_t *cmd)
{
    cmd->driver  = req->node;
    req->node    = NULL; // ownership moved
    cmd->n_group = req->n_group;
    cmd->groups  = calloc(cmd->n_group, sizeof(neu_req_gtag_group_t));
    for (int i = 0; i < cmd->n_group; i++) {
        cmd->groups[i].group = req->groups[i].group;
        req->groups[i].group = NULL; // ownership moved
        cmd->groups[i].n_tag = req->groups[i].n_tag;
        cmd->groups[i].tags =
            calloc(cmd->groups[i].n_tag, sizeof(neu_resp_tag_value_t));

        for (int k = 0; k < cmd->groups[i].n_tag; k++) {
            strcpy(cmd->groups[i].tags[k].tag, req->groups[i].tags[k].tag);

            switch (req->groups[i].tags[k].t) {
            case NEU_JSON_INT:
                cmd->groups[i].tags[k].value.type = NEU_TYPE_INT64;
                cmd->groups[i].tags[k].value.value.u64 =
                    req->groups[i].tags[k].value.val_int;
                break;
            case NEU_JSON_STR:
                cmd->groups[i].tags[k].value.type = NEU_TYPE_STRING;
                strcpy(cmd->groups[i].tags[k].value.value.str,
                       req->groups[i].tags[k].value.val_str);
                break;
            case NEU_JSON_DOUBLE:
                cmd->groups[i].tags[k].value.type = NEU_TYPE_DOUBLE;
                cmd->groups[i].tags[k].value.value.d64 =
                    req->groups[i].tags[k].value.val_double;
                break;
            case NEU_JSON_BOOL:
                cmd->groups[i].tags[k].value.type = NEU_TYPE_BOOL;
                cmd->groups[i].tags[k].value.value.boolean =
                    req->groups[i].tags[k].value.val_bool;
                break;
            case NEU_JSON_BYTES:
                cmd->groups[i].tags[k].value.type = NEU_TYPE_BYTES;
                cmd->groups[i].tags[k].value.value.bytes.length =
                    req->groups[i].tags[k].value.val_bytes.length;
                memcpy(cmd->groups[i].tags[k].value.value.bytes.bytes,
                       req->groups[i].tags[k].value.val_bytes.bytes,
                       req->groups[i].tags[k].value.val_bytes.length);
                break;
            default:
                assert(false);
                break;
            }
        }
    }
}

/**
 * @brief 处理多组标签写入请求的HTTP处理函数
 *
 * 此函数处理来自客户端的多组标签写入请求，验证JWT令牌，解析写入命令，
 * 并将命令发送给插件进行处理。
 *
 * @param aio NNG异步I/O对象，包含HTTP请求和响应信息
 */
void handle_write_gtags(nng_aio *aio)
{
    neu_plugin_t *plugin = neu_rest_get_plugin();

    NEU_PROCESS_HTTP_REQUEST_VALIDATE_JWT(
        aio, neu_json_write_gtags_req_t, neu_json_decode_write_gtags_req, {
            neu_reqresp_head_t    header = { 0 };
            neu_req_write_gtags_t cmd    = { 0 };

            nng_http_req *nng_req = nng_aio_get_input(aio, 0);
            nlog_notice("<%p> req %s %s", aio, nng_http_req_get_method(nng_req),
                        nng_http_req_get_uri(nng_req));
            header.ctx  = aio;
            header.type = NEU_REQ_WRITE_GTAGS;

            trans(req, &cmd);

            int ret = neu_plugin_op(plugin, header, &cmd);
            if (ret != 0) {
                neu_req_write_gtags_fini(&cmd);
                NEU_JSON_RESPONSE_ERROR(NEU_ERR_IS_BUSY, {
                    neu_http_response(aio, NEU_ERR_IS_BUSY, result_error);
                });
            }
        })
}

/**
 * @brief 处理读取响应的函数
 *
 * 此函数处理读取操作的响应，将内部响应结构转换为JSON格式，
 * 并发送HTTP响应给客户端。
 *
 * @param aio NNG异步I/O对象，用于发送HTTP响应
 * @param resp 内部读取响应结构，包含读取到的标签值
 */
void handle_read_resp(nng_aio *aio, neu_resp_read_group_t *resp)
{
    neu_json_read_resp_t api_res = { 0 };
    char *               result  = NULL;
    int                  index   = 0;

    api_res.n_tag = utarray_len(resp->tags);
    api_res.tags  = calloc(api_res.n_tag, sizeof(neu_json_read_resp_tag_t));

    utarray_foreach(resp->tags, neu_resp_tag_value_meta_t *, tag_value)
    {
        neu_tag_value_to_json(tag_value, &api_res.tags[index]);
        index += 1;
    }

    neu_json_encode_by_fn(&api_res, neu_json_encode_read_resp, &result);
    for (int i = 0; i < api_res.n_tag; i++) {
        if (api_res.tags[i].n_meta > 0) {
            free(api_res.tags[i].metas);
        }
    }
    neu_http_ok(aio, result);
    free(api_res.tags);
    free(result);
}
