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
    if (neu_http_get_param_str(aio, "ndriver", cmd.ndriver,
                               sizeof(cmd.ndriver)) <= 0 ||
        strlen(cmd.ndriver) >= NEU_NODE_NAME_LEN) {
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
