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

#include <nng/nng.h>

#include "json/neu_json_fn.h"

#include "json_rw.h"
#include "read_write.h"

void send_data(neu_plugin_t *plugin, neu_request_t *req)
{
    int                  rv       = 0;
    neu_reqresp_data_t * neu_data = (neu_reqresp_data_t *) req->buf;
    neu_datatag_table_t *datatag_table =
        neu_system_get_datatags_table(plugin, req->sender_id);

    json_read_resp_t resp = {
        .sender_id            = req->sender_id,
        .sender_name          = req->node_name,
        .sender_datatag_table = datatag_table,
        .grp_config           = neu_data->grp_config,
        .data_val             = neu_data->data_val,
    };
    char *json_str = NULL;
    rv = neu_json_encode_by_fn(&resp, json_encode_read_resp, &json_str);
    if (0 != rv) {
        log_error("cannot encode read resp");
        return;
    }

    nng_msg *msg      = NULL;
    size_t   json_len = strlen(json_str);
    rv                = nng_msg_alloc(&msg, json_len);
    if (0 != rv) {
        log_error("nng cannot allocate msg");
        free(json_str);
        return;
    }

    memcpy(nng_msg_body(msg), json_str, json_len); // no null byte
    rv = nng_sendmsg(plugin->sock, msg, 0); // TODO: use aio to send message
    if (0 != rv) {
        log_error("nng cannot send msg");
        nng_msg_free(msg);
    }

    free(json_str);
}
