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

#include "json/neu_json_rw.h"
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

void recv_data_callback(void *arg)
{
    int               rv       = 0;
    neu_plugin_t *    plugin   = arg;
    nng_msg *         msg      = NULL;
    char *            json_str = NULL;
    json_write_req_t *req      = NULL;

    rv = nng_aio_result(plugin->recv_aio);
    if (0 != rv) {
        log_error("receive error: %s", nng_strerror(rv));
        return;
    }

    msg      = nng_aio_get_msg(plugin->recv_aio);
    json_str = nng_msg_body(msg);
    // log_info("receive cmd: %s\n", json_str);
    if (json_decode_write_req(json_str, nng_msg_len(msg), &req) < 0) {
        log_error("cannot decode write request");
        goto recv_data_callback_end;
    }

    rv = write_data(plugin, req);
    if (0 != rv) {
        log_error("failed to write data");
        goto recv_data_callback_end;
    }

recv_data_callback_end:
    nng_msg_free(msg);
    json_decode_write_req_free(req);
    nng_recv_aio(plugin->sock, plugin->recv_aio);
}

int write_data(neu_plugin_t *plugin, json_write_req_t *write_req)
{
    uint32_t req_id = neu_plugin_get_event_id(plugin);
    return plugin_send_write_cmd_from_write_req(plugin, req_id, write_req);
}
