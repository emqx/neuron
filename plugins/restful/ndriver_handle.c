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

#include "define.h"
#include "plugin.h"
#include "utils/log.h"
#include "json/neu_json_error.h"
#include "json/neu_json_fn.h"

#include "adapter.h"
#include "handle.h"
#include "utils/http.h"

#include "parser/neu_json_ndriver.h"

static int send_ndriver_map_req(nng_aio *aio, neu_json_ndriver_map_t *req,
                                neu_reqresp_type_e type)
{
    neu_plugin_t *plugin = neu_rest_get_plugin();

    if (strlen(req->ndriver) >= NEU_NODE_NAME_LEN ||
        strlen(req->driver) >= NEU_NODE_NAME_LEN ||
        strlen(req->group) >= NEU_GROUP_NAME_LEN) {
        return NEU_ERR_NODE_NAME_TOO_LONG;
    }

    int                ret    = 0;
    neu_reqresp_head_t header = {
        .ctx  = aio,
        .type = type,
    };

    neu_req_ndriver_map_t cmd = { 0 };
    strcpy(cmd.ndriver, req->ndriver);
    strcpy(cmd.driver, req->driver);
    strcpy(cmd.group, req->group);

    ret = neu_plugin_op(plugin, header, &cmd);
    if (ret != 0) {
        return NEU_ERR_IS_BUSY;
    }

    return 0;
}

void handle_add_ndriver_map(nng_aio *aio)
{
    NEU_PROCESS_HTTP_REQUEST_VALIDATE_JWT(
        aio, neu_json_ndriver_map_t, neu_json_decode_ndriver_map, {
            int ret = send_ndriver_map_req(aio, req, NEU_REQ_ADD_NDRIVER_MAP);
            if (ret != 0) {
                NEU_JSON_RESPONSE_ERROR(
                    ret, { neu_http_response(aio, ret, result_error); });
            }
        })
}

void handle_del_ndriver_map(nng_aio *aio)
{
    NEU_PROCESS_HTTP_REQUEST_VALIDATE_JWT(
        aio, neu_json_ndriver_map_t, neu_json_decode_ndriver_map, {
            int ret = send_ndriver_map_req(aio, req, NEU_REQ_DEL_NDRIVER_MAP);
            if (ret != 0) {
                NEU_JSON_RESPONSE_ERROR(
                    ret, { neu_http_response(aio, ret, result_error); });
            }
        })
}

void handle_get_ndriver_maps(nng_aio *aio)
{
    int           ret    = 0;
    neu_plugin_t *plugin = neu_rest_get_plugin();

    NEU_VALIDATE_JWT(aio);

    neu_req_get_ndriver_maps_t cmd = { 0 };
    ret = neu_http_get_param_str(aio, "ndriver", cmd.ndriver,
                                 sizeof(cmd.ndriver));
    if (ret <= 0 || (size_t) ret >= sizeof(cmd.ndriver)) {
        NEU_JSON_RESPONSE_ERROR(NEU_ERR_PARAM_IS_WRONG, {
            neu_http_response(aio, error_code.error, result_error);
        })
        return;
    }

    neu_reqresp_head_t header = {
        .ctx  = aio,
        .type = NEU_REQ_GET_NDRIVER_MAPS,
    };

    ret = neu_plugin_op(plugin, header, &cmd);
    if (ret != 0) {
        NEU_JSON_RESPONSE_ERROR(NEU_ERR_IS_BUSY, {
            neu_http_response(aio, NEU_ERR_IS_BUSY, result_error);
        });
    }
}

void handle_get_ndriver_maps_resp(nng_aio *                    aio,
                                  neu_resp_get_ndriver_maps_t *groups)
{
    neu_json_ndriver_map_group_array_t group_arr = { 0 };

    group_arr.n_group = utarray_len(groups->groups);
    group_arr.groups  = calloc(group_arr.n_group, sizeof(*group_arr.groups));

    int index = 0;
    utarray_foreach(groups->groups, neu_resp_ndriver_map_info_t *, group)
    {
        group_arr.groups[index].driver = group->driver;
        group_arr.groups[index].group  = group->group;
        ++index;
    }

    char *result = NULL;
    neu_json_encode_by_fn(&group_arr, neu_json_encode_get_ndriver_maps_resp,
                          &result);
    if (result) {
        neu_http_ok(aio, result);
    } else {
        NEU_JSON_RESPONSE_ERROR(NEU_ERR_EINTERNAL, {
            neu_http_response(aio, NEU_ERR_EINTERNAL, result_error);
        });
    }

    free(result);
    free(group_arr.groups);
    utarray_free(groups->groups);
}

static inline int
send_update_ndriver_tag_param_req(nng_aio *                                aio,
                                  neu_json_update_ndriver_tag_param_req_t *req)
{
    int           ret    = 0;
    neu_plugin_t *plugin = neu_rest_get_plugin();

    neu_reqresp_head_t header = {
        .ctx  = aio,
        .type = NEU_REQ_UPDATE_NDRIVER_TAG_PARAM,
    };

    neu_req_update_ndriver_tag_param_t cmd = { 0 };
    cmd.n_tag                              = req->tags.len;
    cmd.tags = calloc(cmd.n_tag, sizeof(cmd.tags[0]));
    if (NULL == cmd.tags) {
        return NEU_ERR_EINTERNAL;
    }

    strcpy(cmd.ndriver, req->ndriver);
    strcpy(cmd.driver, req->driver);
    strcpy(cmd.group, req->group);

    for (int i = 0; i < req->tags.len; i++) {
        cmd.tags[i].name         = req->tags.data[i].name;
        req->tags.data[i].name   = NULL; // ownership moved
        cmd.tags[i].params       = req->tags.data[i].params;
        req->tags.data[i].params = NULL; // ownership moved
    }

    if (0 != neu_plugin_op(plugin, header, &cmd)) {
        ret = NEU_ERR_IS_BUSY;
        neu_req_update_ndriver_tag_param_fini(&cmd);
    }

    return ret;
}

void handle_put_ndriver_tag_param(nng_aio *aio)
{
    NEU_PROCESS_HTTP_REQUEST_VALIDATE_JWT(
        aio, neu_json_update_ndriver_tag_param_req_t,
        neu_json_decode_update_ndriver_tag_param_req, {
            int ret = send_update_ndriver_tag_param_req(aio, req);
            if (0 != ret) {
                NEU_JSON_RESPONSE_ERROR(
                    ret, { neu_http_response(aio, ret, result_error); });
            }
        })
}

static inline int
send_update_ndriver_tag_info_req(nng_aio *                               aio,
                                 neu_json_update_ndriver_tag_info_req_t *req)
{
    int           ret    = 0;
    neu_plugin_t *plugin = neu_rest_get_plugin();

    neu_reqresp_head_t header = {
        .ctx  = aio,
        .type = NEU_REQ_UPDATE_NDRIVER_TAG_INFO,
    };

    neu_req_update_ndriver_tag_info_t cmd = { 0 };
    cmd.n_tag                             = req->tags.len;
    cmd.tags = calloc(cmd.n_tag, sizeof(cmd.tags[0]));
    if (NULL == cmd.tags) {
        return NEU_ERR_EINTERNAL;
    }

    strcpy(cmd.ndriver, req->ndriver);
    strcpy(cmd.driver, req->driver);
    strcpy(cmd.group, req->group);

    for (int i = 0; i < req->tags.len; i++) {
        cmd.tags[i].name          = req->tags.data[i].name;
        req->tags.data[i].name    = NULL; // ownership moved
        cmd.tags[i].address       = req->tags.data[i].address;
        req->tags.data[i].address = NULL; // ownership moved
    }

    if (0 != neu_plugin_op(plugin, header, &cmd)) {
        ret = NEU_ERR_IS_BUSY;
        neu_req_update_ndriver_tag_info_fini(&cmd);
    }

    return ret;
}

void handle_put_ndriver_tag_info(nng_aio *aio)
{
    NEU_PROCESS_HTTP_REQUEST_VALIDATE_JWT(
        aio, neu_json_update_ndriver_tag_info_req_t,
        neu_json_decode_update_ndriver_tag_info_req, {
            int ret = send_update_ndriver_tag_info_req(aio, req);
            if (0 != ret) {
                NEU_JSON_RESPONSE_ERROR(
                    ret, { neu_http_response(aio, ret, result_error); });
            }
        })
}

void handle_get_ndriver_tags(nng_aio *aio)
{
    int           ret    = 0;
    neu_plugin_t *plugin = neu_rest_get_plugin();

    NEU_VALIDATE_JWT(aio);

    neu_req_get_ndriver_tags_t cmd = { 0 };

    ret = neu_http_get_param_str(aio, "ndriver", cmd.ndriver,
                                 sizeof(cmd.ndriver));
    if (ret <= 0 || (size_t) ret >= sizeof(cmd.ndriver)) {
        ret = NEU_ERR_PARAM_IS_WRONG;
        goto end;
    }

    ret = neu_http_get_param_str(aio, "driver", cmd.driver, sizeof(cmd.driver));
    if (ret <= 0 || (size_t) ret >= sizeof(cmd.driver)) {
        ret = NEU_ERR_PARAM_IS_WRONG;
        goto end;
    }

    ret = neu_http_get_param_str(aio, "group", cmd.group, sizeof(cmd.group));
    if (ret <= 0 || (size_t) ret >= sizeof(cmd.group)) {
        ret = NEU_ERR_PARAM_IS_WRONG;
        goto end;
    }

    neu_reqresp_head_t header = {
        .ctx  = aio,
        .type = NEU_REQ_GET_NDRIVER_TAGS,
    };

    if (0 != (ret = neu_plugin_op(plugin, header, &cmd))) {
        ret = NEU_ERR_IS_BUSY;
    }

end:
    if (0 != ret) {
        NEU_JSON_RESPONSE_ERROR(ret,
                                { neu_http_response(aio, ret, result_error); });
    }
}

void handle_get_ndriver_tags_resp(nng_aio *                    aio,
                                  neu_resp_get_ndriver_tags_t *tags)
{
    char *                       result   = NULL;
    neu_json_ndriver_tag_array_t tags_res = { 0 };

    tags_res.len  = utarray_len(tags->tags);
    tags_res.data = calloc(tags_res.len, sizeof(tags_res.data[0]));
    if (NULL == tags_res.data) {
        goto end;
    }

    for (int i = 0; i < tags_res.len; ++i) {
        neu_ndriver_tag_t *tag     = utarray_eltptr(tags->tags, (size_t) i);
        tags_res.data[i].name      = tag->name;
        tags_res.data[i].address   = tag->address;
        tags_res.data[i].attribute = tag->attribute;
        tags_res.data[i].type      = tag->type;
        tags_res.data[i].params    = tag->params;
    }

    neu_json_encode_by_fn(&tags_res, neu_json_encode_get_ndriver_tags_resp,
                          &result);

end:
    if (result) {
        neu_http_ok(aio, result);
    } else {
        NEU_JSON_RESPONSE_ERROR(NEU_ERR_EINTERNAL, {
            neu_http_response(aio, NEU_ERR_EINTERNAL, result_error);
        });
    }

    free(result);
    free(tags_res.data);
    utarray_free(tags->tags);
}
