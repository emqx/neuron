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

            strcpy(cmd.driver, req->node);
            strcpy(cmd.group, req->group);
            ret = neu_plugin_op(plugin, header, &cmd);
            if (ret != 0) {
                NEU_JSON_RESPONSE_ERROR(NEU_ERR_IS_BUSY, {
                    neu_http_response(aio, NEU_ERR_IS_BUSY, result_error);
                });
            }
        })
}

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

            header.ctx  = aio;
            header.type = NEU_REQ_WRITE_TAG;

            strcpy(cmd.driver, req->node);
            strcpy(cmd.group, req->group);
            strcpy(cmd.tag, req->tag);

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
                NEU_JSON_RESPONSE_ERROR(NEU_ERR_IS_BUSY, {
                    neu_http_response(aio, NEU_ERR_IS_BUSY, result_error);
                });
            }
        })
}

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

            header.ctx  = aio;
            header.type = NEU_REQ_WRITE_TAGS;

            strcpy(cmd.driver, req->node);
            strcpy(cmd.group, req->group);
            cmd.n_tag = req->n_tag;
            cmd.tags  = calloc(cmd.n_tag, sizeof(neu_resp_tag_value_t));

            for (int i = 0; i < cmd.n_tag; i++) {
                strcpy(cmd.tags[i].tag, req->tags[i].tag);
                switch (req->tags[i].t) {
                case NEU_JSON_INT:
                    cmd.tags[i].value.type      = NEU_TYPE_INT64;
                    cmd.tags[i].value.value.u64 = req->tags[i].value.val_int;
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
                NEU_JSON_RESPONSE_ERROR(NEU_ERR_IS_BUSY, {
                    neu_http_response(aio, NEU_ERR_IS_BUSY, result_error);
                });
            }
        })
}

void handle_read_resp(nng_aio *aio, neu_resp_read_group_t *resp)
{
    neu_json_read_resp_t api_res = { 0 };
    char *               result  = NULL;

    api_res.n_tag = resp->n_tag;
    api_res.tags  = calloc(api_res.n_tag, sizeof(neu_json_read_resp_tag_t));

    for (int i = 0; i < resp->n_tag; i++) {
        neu_resp_tag_value_meta_t *tag = &resp->tags[i];

        api_res.tags[i].name  = resp->tags[i].tag;
        api_res.tags[i].error = NEU_ERR_SUCCESS;

        for (int k = 0; k < NEU_TAG_META_SIZE; k++) {
            if (strlen(tag->metas[k].name) > 0) {
                api_res.tags[i].n_meta++;
            } else {
                break;
            }
        }

        if (api_res.tags[i].n_meta > 0) {
            api_res.tags[i].metas = (neu_json_tag_meta_t *) calloc(
                api_res.tags[i].n_meta, sizeof(neu_json_tag_meta_t));
        }

        neu_json_metas_to_json(tag->metas, NEU_TAG_META_SIZE, &api_res.tags[i]);

        switch (resp->tags[i].value.type) {
        case NEU_TYPE_INT8:
            api_res.tags[i].t             = NEU_JSON_INT;
            api_res.tags[i].value.val_int = resp->tags[i].value.value.i8;
            break;
        case NEU_TYPE_UINT8:
            api_res.tags[i].t             = NEU_JSON_INT;
            api_res.tags[i].value.val_int = resp->tags[i].value.value.u8;
            break;
        case NEU_TYPE_INT16:
            api_res.tags[i].t             = NEU_JSON_INT;
            api_res.tags[i].value.val_int = resp->tags[i].value.value.i16;
            break;
        case NEU_TYPE_WORD:
        case NEU_TYPE_UINT16:
            api_res.tags[i].t             = NEU_JSON_INT;
            api_res.tags[i].value.val_int = resp->tags[i].value.value.u16;
            break;
        case NEU_TYPE_INT32:
            api_res.tags[i].t             = NEU_JSON_INT;
            api_res.tags[i].value.val_int = resp->tags[i].value.value.i32;
            break;
        case NEU_TYPE_DWORD:
        case NEU_TYPE_UINT32:
            api_res.tags[i].t             = NEU_JSON_INT;
            api_res.tags[i].value.val_int = resp->tags[i].value.value.u32;
            break;
        case NEU_TYPE_INT64:
            api_res.tags[i].t             = NEU_JSON_INT;
            api_res.tags[i].value.val_int = resp->tags[i].value.value.i64;
            break;
        case NEU_TYPE_LWORD:
        case NEU_TYPE_UINT64:
            api_res.tags[i].t             = NEU_JSON_INT;
            api_res.tags[i].value.val_int = resp->tags[i].value.value.u64;
            break;
        case NEU_TYPE_FLOAT:
            api_res.tags[i].t               = NEU_JSON_FLOAT;
            api_res.tags[i].value.val_float = resp->tags[i].value.value.f32;
            api_res.tags[i].precision       = resp->tags[i].value.precision;
            break;
        case NEU_TYPE_DOUBLE:
            api_res.tags[i].t                = NEU_JSON_DOUBLE;
            api_res.tags[i].value.val_double = resp->tags[i].value.value.d64;
            api_res.tags[i].precision        = resp->tags[i].value.precision;
            break;
        case NEU_TYPE_BIT:
            api_res.tags[i].t             = NEU_JSON_BIT;
            api_res.tags[i].value.val_bit = resp->tags[i].value.value.u8;
            break;
        case NEU_TYPE_BOOL:
            api_res.tags[i].t              = NEU_JSON_BOOL;
            api_res.tags[i].value.val_bool = resp->tags[i].value.value.boolean;
            break;
        case NEU_TYPE_STRING:
            api_res.tags[i].t             = NEU_JSON_STR;
            api_res.tags[i].value.val_str = resp->tags[i].value.value.str;
            break;
        case NEU_TYPE_BYTES:
            api_res.tags[i].t = NEU_JSON_BYTES;
            api_res.tags[i].value.val_bytes.length =
                resp->tags[i].value.value.bytes.length;
            api_res.tags[i].value.val_bytes.bytes =
                resp->tags[i].value.value.bytes.bytes;
            break;
        case NEU_TYPE_ERROR:
            api_res.tags[i].t             = NEU_JSON_INT;
            api_res.tags[i].value.val_int = resp->tags[i].value.value.i32;
            api_res.tags[i].error         = resp->tags[i].value.value.i32;
            break;
        case NEU_TYPE_PTR:
            api_res.tags[i].t = NEU_JSON_STR;
            api_res.tags[i].value.val_str =
                (char *) resp->tags[i].value.value.ptr.ptr;
            break;
        default:
            break;
        }
    }

    neu_json_encode_by_fn(&api_res, neu_json_encode_read_resp, &result);
    for (int i = 0; i < resp->n_tag; i++) {
        if (resp->tags[i].value.type == NEU_TYPE_PTR) {
            free(resp->tags[i].value.value.ptr.ptr);
        }
        if (api_res.tags[i].n_meta > 0) {
            free(api_res.tags[i].metas);
        }
    }
    neu_http_ok(aio, result);
    free(api_res.tags);
    free(result);
    free(resp->tags);
}
