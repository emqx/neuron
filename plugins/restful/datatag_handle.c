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
            if (strlen(req->node) >= NEU_NODE_NAME_LEN) {
                CHECK_NODE_NAME_LENGTH_ERR;
            } else if (strlen(req->group) >= NEU_GROUP_NAME_LEN) {
                CHECK_GROUP_NAME_LENGTH_ERR;
            } else {
                int                ret    = 0;
                neu_reqresp_head_t header = { 0 };
                neu_req_add_tag_t  cmd    = { 0 };
                header.otel_trace_type    = NEU_OTEL_TRACE_TYPE_REST_COMM;

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
                    cmd.tags[i].bias      = req->tags[i].bias;
                    cmd.tags[i].address   = strdup(req->tags[i].address);
                    cmd.tags[i].name      = strdup(req->tags[i].name);
                    if (req->tags[i].description != NULL) {
                        cmd.tags[i].description =
                            strdup(req->tags[i].description);
                    } else {
                        cmd.tags[i].description = strdup("");
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

    NEU_JSON_RESPONSE_ERROR(resp->error,
                            { neu_http_response(aio, resp->error, result); });
    free(result);
}

void handle_add_gtags(nng_aio *aio)
{
    neu_plugin_t *plugin = neu_rest_get_plugin();

    NEU_PROCESS_HTTP_REQUEST_VALIDATE_JWT(
        aio, neu_json_add_gtags_req_t, neu_json_decode_add_gtags_req, {
            int                ret    = 0;
            neu_reqresp_head_t header = { 0 };
            neu_req_add_gtag_t cmd    = { 0 };
            int                err_type;
            header.ctx             = aio;
            header.type            = NEU_REQ_ADD_GTAG;
            header.otel_trace_type = NEU_OTEL_TRACE_TYPE_REST_COMM;

            if (strlen(req->node) >= NEU_NODE_NAME_LEN) {
                err_type = NEU_ERR_NODE_NAME_TOO_LONG;
                goto error;
            }

            for (int i = 0; i < req->n_group; i++) {
                if (strlen(req->groups[i].group) >= NEU_GROUP_NAME_LEN) {
                    err_type = NEU_ERR_GROUP_NAME_TOO_LONG;
                    goto error;
                }
                if (req->groups[i].interval < NEU_DEFAULT_GROUP_INTERVAL) {
                    err_type = NEU_ERR_GROUP_PARAMETER_INVALID;
                    goto error;
                }
            }

            strcpy(cmd.driver, req->node);
            cmd.n_group = req->n_group;
            cmd.groups  = calloc(req->n_group, sizeof(neu_gdatatag_t));

            for (int i = 0; i < req->n_group; i++) {
                strcpy(cmd.groups[i].group, req->groups[i].group);
                cmd.groups[i].n_tag    = req->groups[i].n_tag;
                cmd.groups[i].interval = req->groups[i].interval;
                cmd.groups[i].tags =
                    calloc(cmd.groups[i].n_tag, sizeof(neu_datatag_t));

                for (int j = 0; j < req->groups[i].n_tag; j++) {
                    cmd.groups[i].tags[j].attribute =
                        req->groups[i].tags[j].attribute;
                    cmd.groups[i].tags[j].type = req->groups[i].tags[j].type;
                    cmd.groups[i].tags[j].precision =
                        req->groups[i].tags[j].precision;
                    cmd.groups[i].tags[j].decimal =
                        req->groups[i].tags[j].decimal;
                    cmd.groups[i].tags[j].bias = req->groups[i].tags[j].bias;
                    cmd.groups[i].tags[j].address =
                        strdup(req->groups[i].tags[j].address);
                    cmd.groups[i].tags[j].name =
                        strdup(req->groups[i].tags[j].name);
                    if (req->groups[i].tags[j].description != NULL) {
                        cmd.groups[i].tags[j].description =
                            strdup(req->groups[i].tags[j].description);
                    } else {
                        cmd.groups[i].tags[j].description = strdup("");
                    }
                }
            }

            ret = neu_plugin_op(plugin, header, &cmd);
            if (ret != 0) {
                NEU_JSON_RESPONSE_ERROR(NEU_ERR_IS_BUSY, {
                    neu_http_response(aio, NEU_ERR_IS_BUSY, result_error);
                });
            }
            goto success;
        error:
            NEU_JSON_RESPONSE_ERROR(
                err_type, { neu_http_response(aio, err_type, result_error); });
        success:;
        })
}

void handle_add_gtags_resp(nng_aio *aio, neu_resp_add_tag_t *resp)
{
    neu_json_add_gtag_res_t res    = { 0 };
    char *                  result = NULL;
    res.error                      = resp->error;
    res.index                      = resp->index;

    neu_json_encode_by_fn(&res, neu_json_encode_au_gtags_resp, &result);
    NEU_JSON_RESPONSE_ERROR(resp->error,
                            { neu_http_response(aio, resp->error, result); });
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
            int                err_type;

            header.ctx             = aio;
            header.type            = NEU_REQ_DEL_TAG;
            header.otel_trace_type = NEU_OTEL_TRACE_TYPE_REST_COMM;

            if (strlen(req->node) >= NEU_NODE_NAME_LEN) {
                err_type = NEU_ERR_NODE_NAME_TOO_LONG;
                goto error;
            }

            if (strlen(req->group) >= NEU_GROUP_NAME_LEN) {
                err_type = NEU_ERR_GROUP_NAME_TOO_LONG;
                goto error;
            }

            strcpy(cmd.driver, req->node);
            strcpy(cmd.group, req->group);
            cmd.n_tag = req->n_tags;

            for (int i = 0; i < req->n_tags; i++) {
                if (strlen(req->tags[i]) >= NEU_TAG_ADDRESS_LEN) {
                    err_type = NEU_ERR_TAG_NAME_TOO_LONG;
                    goto error;
                }
            }

            cmd.tags = calloc(req->n_tags, sizeof(char *));

            for (int i = 0; i < req->n_tags; i++) {
                cmd.tags[i] = strdup(req->tags[i]);
            }

            ret = neu_plugin_op(plugin, header, &cmd);
            if (ret != 0) {
                NEU_JSON_RESPONSE_ERROR(NEU_ERR_IS_BUSY, {
                    neu_http_response(aio, NEU_ERR_IS_BUSY, result_error);
                });
            }
            goto success;

        error:
            NEU_JSON_RESPONSE_ERROR(
                err_type, { neu_http_response(aio, err_type, result_error); });
        success:;
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

            header.ctx             = aio;
            header.type            = NEU_REQ_UPDATE_TAG;
            header.otel_trace_type = NEU_OTEL_TRACE_TYPE_REST_COMM;

            strcpy(cmd.driver, req->node);
            strcpy(cmd.group, req->group);
            cmd.n_tag = req->n_tag;
            cmd.tags  = calloc(req->n_tag, sizeof(neu_datatag_t));

            for (int i = 0; i < req->n_tag; i++) {
                cmd.tags[i].attribute = req->tags[i].attribute;
                cmd.tags[i].type      = req->tags[i].type;
                cmd.tags[i].precision = req->tags[i].precision;
                cmd.tags[i].decimal   = req->tags[i].decimal;
                cmd.tags[i].bias      = req->tags[i].bias;
                cmd.tags[i].address   = strdup(req->tags[i].address);
                cmd.tags[i].name      = strdup(req->tags[i].name);
                if (req->tags[i].description != NULL) {
                    cmd.tags[i].description = strdup(req->tags[i].description);
                } else {
                    cmd.tags[i].description = strdup("");
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
        .ctx             = aio,
        .type            = NEU_REQ_GET_TAG,
        .otel_trace_type = NEU_OTEL_TRACE_TYPE_REST_COMM,
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
        tags_res.tags[index].bias        = tag->bias;
        tags_res.tags[index].t           = NEU_JSON_UNDEFINE;
    }

    neu_json_encode_by_fn(&tags_res, neu_json_encode_get_tags_resp, &result);

    neu_http_ok(aio, result);

    free(result);
    free(tags_res.tags);
    utarray_free(tags->tags);
}