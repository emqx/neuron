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
