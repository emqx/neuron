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

#include "parser/neu_json_tag.h"
#include "plugin.h"
#include "utils/log.h"
#include "json/neu_json_error.h"
#include "json/neu_json_fn.h"

#include "handle.h"
#include "tag.h"
#include "utils/http.h"

#include "datatag_handle.h"

void handle_add_tags(nng_aio *aio)
{
    neu_plugin_t *plugin = neu_rest_get_plugin();

    NEU_PROCESS_HTTP_REQUEST_VALIDATE_JWT(
        aio, neu_json_add_tags_req_t, neu_json_decode_add_tags_req, {
            if (strlen(req->group) >= NEU_GROUP_NAME_LEN) {
                NEU_JSON_RESPONSE_ERROR(NEU_ERR_GROUP_NAME_TOO_LONG, {
                    http_response(aio, NEU_ERR_GROUP_NAME_TOO_LONG,
                                  result_error);
                });
            } else {
                int                ret    = 0;
                neu_reqresp_head_t header = { 0 };
                neu_req_add_tag_t  cmd    = { 0 };

                header.ctx  = aio;
                header.type = NEU_REQ_ADD_TAG;
                strcpy(cmd.driver, req->node);
                strcpy(cmd.group, req->group);
                cmd.n_tag = req->n_tag;
                cmd.tags  = calloc(req->n_tag, sizeof(neu_datatag_t));

                for (int i = 0; i < req->n_tag; i++) {
                    cmd.tags[i].attribute = req->tags[i].attribute;
                    cmd.tags[i].type      = req->tags[i].type;
                    cmd.tags[i].precision = req->tags[i].precision;
                    cmd.tags[i].decimal   = req->tags[i].decimal;
                    cmd.tags[i].address   = strdup(req->tags[i].address);
                    cmd.tags[i].name      = strdup(req->tags[i].name);
                    if (req->tags[i].description != NULL) {
                        cmd.tags[i].description =
                            strdup(req->tags[i].description);
                    } else {
                        cmd.tags[i].description = strdup("");
                    }
                    if (NEU_ATTRIBUTE_STATIC & req->tags[i].attribute) {
                        neu_tag_set_static_value_json(
                            &cmd.tags[i], req->tags[i].t, &req->tags[i].value);
                    }
                }

                ret = neu_plugin_op(plugin, header, &cmd);
                if (ret != 0) {
                    NEU_JSON_RESPONSE_ERROR(NEU_ERR_IS_BUSY, {
                        neu_http_response(aio, NEU_ERR_IS_BUSY, result_error);
                    });
                }
            }
        })
}

void handle_add_tags_resp(nng_aio *aio, neu_resp_add_tag_t *resp)
{
    neu_json_add_tag_res_t res    = { 0 };
    char *                 result = NULL;

    res.error = resp->error;
    res.index = resp->index;

    neu_json_encode_by_fn(&res, neu_json_encode_au_tags_resp, &result);

    neu_http_ok(aio, result);
    free(result);
}

void handle_del_tags(nng_aio *aio)
{
    neu_plugin_t *plugin = neu_rest_get_plugin();

    NEU_PROCESS_HTTP_REQUEST_VALIDATE_JWT(
        aio, neu_json_del_tags_req_t, neu_json_decode_del_tags_req, {
            int                ret    = 0;
            neu_reqresp_head_t header = { 0 };
            neu_req_del_tag_t  cmd    = { 0 };

            header.ctx  = aio;
            header.type = NEU_REQ_DEL_TAG;
            strcpy(cmd.driver, req->node);
            strcpy(cmd.group, req->group);
            cmd.n_tag = req->n_tags;
            cmd.tags  = calloc(req->n_tags, sizeof(char *));

            for (int i = 0; i < req->n_tags; i++) {
                cmd.tags[i] = strdup(req->tags[i]);
            }

            ret = neu_plugin_op(plugin, header, &cmd);
            if (ret != 0) {
                NEU_JSON_RESPONSE_ERROR(NEU_ERR_IS_BUSY, {
                    neu_http_response(aio, NEU_ERR_IS_BUSY, result_error);
                });
            }
        })
}

void handle_update_tags(nng_aio *aio)
{
    neu_plugin_t *plugin = neu_rest_get_plugin();

    NEU_PROCESS_HTTP_REQUEST_VALIDATE_JWT(
        aio, neu_json_update_tags_req_t, neu_json_decode_update_tags_req, {
            int                ret    = 0;
            neu_reqresp_head_t header = { 0 };
            neu_req_add_tag_t  cmd    = { 0 };

            header.ctx  = aio;
            header.type = NEU_REQ_UPDATE_TAG;
            strcpy(cmd.driver, req->node);
            strcpy(cmd.group, req->group);
            cmd.n_tag = req->n_tag;
            cmd.tags  = calloc(req->n_tag, sizeof(neu_datatag_t));

            for (int i = 0; i < req->n_tag; i++) {
                cmd.tags[i].attribute = req->tags[i].attribute;
                cmd.tags[i].type      = req->tags[i].type;
                cmd.tags[i].precision = req->tags[i].precision;
                cmd.tags[i].decimal   = req->tags[i].decimal;
                cmd.tags[i].address   = strdup(req->tags[i].address);
                cmd.tags[i].name      = strdup(req->tags[i].name);
                if (req->tags[i].description != NULL) {
                    cmd.tags[i].description = strdup(req->tags[i].description);
                } else {
                    cmd.tags[i].description = strdup("");
                }
                if (NEU_ATTRIBUTE_STATIC & req->tags[i].attribute) {
                    neu_tag_set_static_value_json(&cmd.tags[i], req->tags[i].t,
                                                  &req->tags[i].value);
                }
            }

            ret = neu_plugin_op(plugin, header, &cmd);
            if (ret != 0) {
                NEU_JSON_RESPONSE_ERROR(NEU_ERR_IS_BUSY, {
                    neu_http_response(aio, NEU_ERR_IS_BUSY, result_error);
                });
            }
        })
}

void handle_update_tags_resp(nng_aio *aio, neu_resp_update_tag_t *resp)
{
    handle_add_tags_resp(aio, resp);
}

void handle_get_tags(nng_aio *aio)
{
    neu_plugin_t *     plugin                     = neu_rest_get_plugin();
    char               node[NEU_NODE_NAME_LEN]    = { 0 };
    char               group[NEU_GROUP_NAME_LEN]  = { 0 };
    char               tag_name[NEU_TAG_NAME_LEN] = { 0 };
    int                ret                        = 0;
    neu_req_get_tag_t  cmd                        = { 0 };
    neu_reqresp_head_t header                     = {
        .ctx  = aio,
        .type = NEU_REQ_GET_TAG,
    };

    NEU_VALIDATE_JWT(aio);

    if (neu_http_get_param_str(aio, "node", node, sizeof(node)) <= 0) {
        NEU_JSON_RESPONSE_ERROR(NEU_ERR_PARAM_IS_WRONG, {
            neu_http_response(aio, NEU_ERR_PARAM_IS_WRONG, result_error);
        })
        return;
    }
    if (neu_http_get_param_str(aio, "group", group, sizeof(group)) <= 0) {
        NEU_JSON_RESPONSE_ERROR(NEU_ERR_PARAM_IS_WRONG, {
            neu_http_response(aio, NEU_ERR_PARAM_IS_WRONG, result_error);
        })
        return;
    }

    if (neu_http_get_param_str(aio, "name", tag_name, sizeof(tag_name)) >= 0) {
        strcpy(cmd.name, tag_name);
    }

    strcpy(cmd.driver, node);
    strcpy(cmd.group, group);

    ret = neu_plugin_op(plugin, header, &cmd);
    if (ret != 0) {
        NEU_JSON_RESPONSE_ERROR(NEU_ERR_IS_BUSY, {
            neu_http_response(aio, NEU_ERR_IS_BUSY, result_error);
        });
    }
}

void handle_get_tags_resp(nng_aio *aio, neu_resp_get_tag_t *tags)
{
    neu_json_get_tags_resp_t tags_res = { 0 };
    char *                   result   = NULL;

    tags_res.n_tag = utarray_len(tags->tags);
    tags_res.tags  = calloc(tags_res.n_tag, sizeof(neu_json_tag_t));

    utarray_foreach(tags->tags, neu_datatag_t *, tag)
    {
        int index = utarray_eltidx(tags->tags, tag);

        tags_res.tags[index].name        = tag->name;
        tags_res.tags[index].address     = tag->address;
        tags_res.tags[index].description = tag->description;
        tags_res.tags[index].type        = tag->type;
        tags_res.tags[index].attribute   = tag->attribute;
        tags_res.tags[index].precision   = tag->precision;
        tags_res.tags[index].decimal     = tag->decimal;
        if (neu_tag_attribute_test(tag, NEU_ATTRIBUTE_STATIC)) {
            neu_tag_get_static_value_json(tag, &tags_res.tags[index].t,
                                          &tags_res.tags[index].value);
        } else {
            tags_res.tags[index].t = NEU_JSON_UNDEFINE;
        }
    }

    neu_json_encode_by_fn(&tags_res, neu_json_encode_get_tags_resp, &result);

    neu_http_ok(aio, result);

    free(result);
    free(tags_res.tags);
    utarray_free(tags->tags);
}
