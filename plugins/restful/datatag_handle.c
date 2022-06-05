/**
 * NEURON IIoT System for Industry 4.0
 * Copyright (C) 2020-2021 EMQ Technologies Co., Ltd All rights reserved.
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
#include "json/neu_json_error.h"
#include "json/neu_json_fn.h"
#include "json/neu_json_tag.h"

#include "handle.h"
#include "http.h"
#include "tag.h"

#include "datatag_handle.h"

void handle_add_tags(nng_aio *aio)
{
    neu_plugin_t *plugin = neu_rest_get_plugin();

    REST_PROCESS_HTTP_REQUEST_VALIDATE_JWT(
        aio, neu_json_add_tags_req_t, neu_json_decode_add_tags_req, {
            char *                 result = NULL;
            neu_json_add_tag_res_t res    = { 0 };

            neu_datatag_t *tags = calloc(req->n_tag, sizeof(neu_datatag_t));
            for (int i = 0; i < req->n_tag; i++) {
                tags[i].attribute = req->tags[i].attribute;
                tags[i].type      = req->tags[i].type;
                tags[i].addr_str  = req->tags[i].address;
                tags[i].name      = req->tags[i].name;
            }

            res.error = neu_plugin_tag_add(plugin, req->node, req->group,
                                           req->n_tag, tags, &res.index);

            neu_json_encode_by_fn(&res, neu_json_encode_au_tags_resp, &result);

            http_ok(aio, result);
            if (res.index > 0) {
                neu_plugin_notify_event_add_tags(plugin, 0, req->node,
                                                 req->group);
            }

            free(result);
            free(tags);
        })
}

void handle_del_tags(nng_aio *aio)
{
    neu_plugin_t *plugin = neu_rest_get_plugin();

    REST_PROCESS_HTTP_REQUEST_VALIDATE_JWT(
        aio, neu_json_del_tags_req_t, neu_json_decode_del_tags_req, {
            int rv = neu_plugin_tag_del(plugin, req->node, req->group,
                                        req->n_tags, req->tags);
            neu_plugin_notify_event_del_tags(plugin, 0, req->node, req->group);
            NEU_JSON_RESPONSE_ERROR(
                rv, { http_response(aio, NEU_ERR_SUCCESS, result_error); })
        })
}

void handle_update_tags(nng_aio *aio)
{
    neu_plugin_t *plugin = neu_rest_get_plugin();

    REST_PROCESS_HTTP_REQUEST_VALIDATE_JWT(
        aio, neu_json_update_tags_req_t, neu_json_decode_update_tags_req, {
            char *                    result = NULL;
            neu_json_update_tag_res_t res    = { 0 };

            neu_datatag_t *tags = calloc(req->n_tag, sizeof(neu_datatag_t));
            for (int i = 0; i < req->n_tag; i++) {
                tags[i].attribute = req->tags[i].attribute;
                tags[i].type      = req->tags[i].type;
                tags[i].addr_str  = req->tags[i].address;
                tags[i].name      = req->tags[i].name;
            }

            res.error = neu_plugin_tag_update(plugin, req->node, req->group,
                                              req->n_tag, tags, &res.index);

            neu_json_encode_by_fn(&res, neu_json_encode_au_tags_resp, &result);

            http_ok(aio, result);
            if (res.index > 0) {
                neu_plugin_notify_event_update_tags(plugin, 0, req->node,
                                                    req->group);
            }

            free(result);
            free(tags);
        })
}

void handle_get_tags(nng_aio *aio)
{
    neu_plugin_t *plugin                    = neu_rest_get_plugin();
    char          node[NEU_NODE_NAME_LEN]   = { 0 };
    char          group[NEU_GROUP_NAME_LEN] = { 0 };
    UT_array *    tags                      = NULL;
    int           ret                       = 0;
    char *        result                    = NULL;

    VALIDATE_JWT(aio);

    if (http_get_param_str(aio, "node", node, sizeof(node)) <= 0) {
        NEU_JSON_RESPONSE_ERROR(NEU_ERR_PARAM_IS_WRONG, {
            http_response(aio, NEU_ERR_PARAM_IS_WRONG, result_error);
        })
        return;
    }
    if (http_get_param_str(aio, "group", group, sizeof(group)) <= 0) {
        NEU_JSON_RESPONSE_ERROR(NEU_ERR_PARAM_IS_WRONG, {
            http_response(aio, NEU_ERR_PARAM_IS_WRONG, result_error);
        })
        return;
    }

    ret = neu_plugin_tag_get(plugin, node, group, &tags);
    if (ret != 0) {
        NEU_JSON_RESPONSE_ERROR(ret, { http_response(aio, ret, result_error); })
        return;
    }

    neu_json_get_tags_resp_t tags_res = { 0 };
    int                      index    = 0;
    tags_res.n_tag                    = utarray_len(tags);
    tags_res.tags =
        calloc(tags_res.n_tag, sizeof(neu_json_get_tags_resp_tag_t));

    utarray_foreach(tags, neu_datatag_t *, tag)
    {
        tags_res.tags[index].name      = tag->name;
        tags_res.tags[index].address   = tag->addr_str;
        tags_res.tags[index].type      = tag->type;
        tags_res.tags[index].attribute = tag->attribute;
        index += 1;
    }

    neu_json_encode_by_fn(&tags_res, neu_json_encode_get_tags_resp, &result);

    http_ok(aio, result);

    free(result);
    free(tags_res.tags);
    utarray_free(tags);
}