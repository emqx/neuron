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

#include "neuron.h"
#include "json/neu_json_fn.h"
#include "json/neu_json_rw.h"

#include "json_rw.h"
#include "read_write.h"

void send_data(neu_plugin_t *plugin, neu_reqresp_trans_data_t *trans_data)
{
    int rv = 0;

    char *           json_str = NULL;
    json_read_resp_t resp     = {
        .plugin     = plugin,
        .trans_data = trans_data,
    };
    rv = neu_json_encode_by_fn(&resp, json_encode_read_resp, &json_str);
    if (0 != rv) {
        plog_error(plugin, "fail encode trans data to json");
        return;
    }

    nng_msg *msg      = NULL;
    size_t   json_len = strlen(json_str);
    rv                = nng_msg_alloc(&msg, json_len);
    if (0 != rv) {
        plog_error(plugin, "nng cannot allocate msg");
        free(json_str);
        return;
    }

    memcpy(nng_msg_body(msg), json_str, json_len); // no null byte
    plog_debug(plugin, ">> %s", json_str);
    free(json_str);
    rv = nng_sendmsg(plugin->sock, msg,
                     NNG_FLAG_NONBLOCK); // TODO: use aio to send message
    if (0 == rv) {
        NEU_PLUGIN_UPDATE_METRIC(plugin, NEU_METRIC_SEND_MSGS_TOTAL, 1, NULL);
        NEU_PLUGIN_UPDATE_METRIC(plugin, NEU_METRIC_SEND_BYTES_5S, json_len,
                                 NULL);
        NEU_PLUGIN_UPDATE_METRIC(plugin, NEU_METRIC_SEND_BYTES_30S, json_len,
                                 NULL);
        NEU_PLUGIN_UPDATE_METRIC(plugin, NEU_METRIC_SEND_BYTES_60S, json_len,
                                 NULL);
    } else {
        plog_error(plugin, "nng cannot send msg: %s", nng_strerror(rv));
        nng_msg_free(msg);
        NEU_PLUGIN_UPDATE_METRIC(plugin, NEU_METRIC_SEND_MSG_ERRORS_TOTAL, 1,
                                 NULL);
    }
}

void recv_data_callback(void *arg)
{
    int               rv       = 0;
    neu_plugin_t *    plugin   = arg;
    nng_msg *         msg      = NULL;
    size_t            json_len = 0;
    char *            json_str = NULL;
    json_write_req_t *req      = NULL;

    rv = nng_aio_result(plugin->recv_aio);
    if (0 != rv) {
        plog_error(plugin, "nng_recv error: %s", nng_strerror(rv));
        nng_mtx_lock(plugin->mtx);
        plugin->receiving = false;
        nng_mtx_unlock(plugin->mtx);
        return;
    }

    msg      = nng_aio_get_msg(plugin->recv_aio);
    json_str = nng_msg_body(msg);
    json_len = nng_msg_len(msg);
    plog_debug(plugin, "<< %.*s", (int) json_len, json_str);
    NEU_PLUGIN_UPDATE_METRIC(plugin, NEU_METRIC_RECV_MSGS_TOTAL, 1, NULL);
    NEU_PLUGIN_UPDATE_METRIC(plugin, NEU_METRIC_RECV_BYTES_5S, json_len, NULL);
    NEU_PLUGIN_UPDATE_METRIC(plugin, NEU_METRIC_RECV_BYTES_30S, json_len, NULL);
    NEU_PLUGIN_UPDATE_METRIC(plugin, NEU_METRIC_RECV_BYTES_60S, json_len, NULL);
    NEU_PLUGIN_UPDATE_METRIC(plugin, NEU_METRIC_RECV_MSGS_5S, 1, NULL);
    NEU_PLUGIN_UPDATE_METRIC(plugin, NEU_METRIC_RECV_MSGS_30S, 1, NULL);
    NEU_PLUGIN_UPDATE_METRIC(plugin, NEU_METRIC_RECV_MSGS_60S, 1, NULL);
    if (json_decode_write_req(json_str, json_len, &req) < 0) {
        plog_error(plugin, "fail decode write request json: %.*s",
                   (int) nng_msg_len(msg), json_str);
        goto recv_data_callback_end;
    }

    rv = write_data(plugin, req);
    if (0 != rv) {
        plog_error(plugin, "failed to write data");
        goto recv_data_callback_end;
    }

recv_data_callback_end:
    nng_msg_free(msg);
    json_decode_write_req_free(req);
    nng_recv_aio(plugin->sock, plugin->recv_aio);
}

int write_data(neu_plugin_t *plugin, json_write_req_t *write_req)
{
    int                 ret    = 0;
    neu_reqresp_head_t  header = { 0 };
    neu_req_write_tag_t cmd    = { 0 };

    header.ctx  = NULL;
    header.type = NEU_REQ_WRITE_TAG;

    cmd.driver = write_req->node_name;
    cmd.group  = write_req->group_name;
    cmd.tag    = write_req->tag_name;

    switch (write_req->t) {
    case NEU_JSON_INT:
        cmd.value.type      = NEU_TYPE_INT64;
        cmd.value.value.u64 = write_req->value.val_int;
        break;
    case NEU_JSON_STR:
        cmd.value.type = NEU_TYPE_STRING;
        strcpy(cmd.value.value.str, write_req->value.val_str);
        break;
    case NEU_JSON_DOUBLE:
        cmd.value.type      = NEU_TYPE_DOUBLE;
        cmd.value.value.d64 = write_req->value.val_double;
        break;
    case NEU_JSON_BOOL:
        cmd.value.type          = NEU_TYPE_BOOL;
        cmd.value.value.boolean = write_req->value.val_bool;
        break;
    case NEU_JSON_BYTES:
        cmd.value.type               = NEU_TYPE_BYTES;
        cmd.value.value.bytes.length = write_req->value.val_bytes.length;
        memcpy(cmd.value.value.bytes.bytes, write_req->value.val_bytes.bytes,
               write_req->value.val_bytes.length);
        break;
    default:
        assert(false);
        break;
    }

    ret = neu_plugin_op(plugin, header, &cmd);
    if (0 == ret) {
        write_req->node_name  = NULL; // ownership moved
        write_req->group_name = NULL; // ownership moved
        write_req->tag_name   = NULL; // ownership moved
    }
    return ret;
}
