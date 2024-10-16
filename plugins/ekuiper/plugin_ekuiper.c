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

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include "errcodes.h"
#include "neuron.h"
#include "otel/otel_manager.h"
#include "utils/asprintf.h"

#include "plugin_ekuiper.h"
#include "read_write.h"

#define EKUIPER_PLUGIN_URL "tcp://127.0.0.1:7081"

const neu_plugin_module_t neu_plugin_module;

static neu_plugin_t *ekuiper_plugin_open(void)
{
    neu_plugin_t *plugin = calloc(1, sizeof(neu_plugin_t));

    neu_plugin_common_init(&plugin->common);

    zlog_notice(neuron, "success to create plugin: %s",
                neu_plugin_module.module_name);
    return plugin;
}

static int ekuiper_plugin_close(neu_plugin_t *plugin)
{
    int rv = 0;

    free(plugin);
    zlog_notice(neuron, "success to free plugin: %s",
                neu_plugin_module.module_name);
    return rv;
}

static void pipe_add_cb(nng_pipe p, nng_pipe_ev ev, void *arg)
{
    (void) p;
    (void) ev;
    neu_plugin_t *plugin = arg;
    nng_mtx_lock(plugin->mtx);
    plugin->common.link_state = NEU_NODE_LINK_STATE_CONNECTED;
    nng_mtx_unlock(plugin->mtx);
}

static void pipe_rm_cb(nng_pipe p, nng_pipe_ev ev, void *arg)
{
    (void) p;
    (void) ev;
    neu_plugin_t *plugin = arg;
    nng_mtx_lock(plugin->mtx);
    plugin->common.link_state = NEU_NODE_LINK_STATE_DISCONNECTED;
    nng_mtx_unlock(plugin->mtx);

    NEU_PLUGIN_UPDATE_METRIC(plugin, NEU_METRIC_DISCONNECTION_60S, 1, NULL);
    NEU_PLUGIN_UPDATE_METRIC(plugin, NEU_METRIC_DISCONNECTION_600S, 1, NULL);
    NEU_PLUGIN_UPDATE_METRIC(plugin, NEU_METRIC_DISCONNECTION_1800S, 1, NULL);
}

static int ekuiper_plugin_init(neu_plugin_t *plugin, bool load)
{
    (void) load;
    int      rv       = 0;
    nng_aio *recv_aio = NULL;

    plugin->mtx = NULL;
    rv          = nng_mtx_alloc(&plugin->mtx);
    if (0 != rv) {
        plog_error(plugin, "cannot allocate nng_mtx");
        return rv;
    }

    rv = nng_aio_alloc(&recv_aio, recv_data_callback, plugin);
    if (rv < 0) {
        plog_error(plugin, "cannot allocate recv_aio: %s", nng_strerror(rv));
        nng_mtx_free(plugin->mtx);
        plugin->mtx = NULL;
        return rv;
    }

    plugin->recv_aio = recv_aio;

    NEU_PLUGIN_REGISTER_METRIC(plugin, NEU_METRIC_TRANS_DATA_5S, 5000);
    NEU_PLUGIN_REGISTER_METRIC(plugin, NEU_METRIC_TRANS_DATA_30S, 30000);
    NEU_PLUGIN_REGISTER_METRIC(plugin, NEU_METRIC_TRANS_DATA_60S, 60000);
    NEU_PLUGIN_REGISTER_METRIC(plugin, NEU_METRIC_SEND_BYTES_5S, 5000);
    NEU_PLUGIN_REGISTER_METRIC(plugin, NEU_METRIC_SEND_BYTES_30S, 30000);
    NEU_PLUGIN_REGISTER_METRIC(plugin, NEU_METRIC_SEND_BYTES_60S, 60000);
    NEU_PLUGIN_REGISTER_METRIC(plugin, NEU_METRIC_RECV_BYTES_5S, 5000);
    NEU_PLUGIN_REGISTER_METRIC(plugin, NEU_METRIC_RECV_BYTES_30S, 30000);
    NEU_PLUGIN_REGISTER_METRIC(plugin, NEU_METRIC_RECV_BYTES_60S, 60000);
    NEU_PLUGIN_REGISTER_METRIC(plugin, NEU_METRIC_RECV_MSGS_5S, 5000);
    NEU_PLUGIN_REGISTER_METRIC(plugin, NEU_METRIC_RECV_MSGS_30S, 30000);
    NEU_PLUGIN_REGISTER_METRIC(plugin, NEU_METRIC_RECV_MSGS_60S, 60000);
    NEU_PLUGIN_REGISTER_METRIC(plugin, NEU_METRIC_DISCONNECTION_60S, 60000);
    NEU_PLUGIN_REGISTER_METRIC(plugin, NEU_METRIC_DISCONNECTION_600S, 600000);
    NEU_PLUGIN_REGISTER_METRIC(plugin, NEU_METRIC_DISCONNECTION_1800S, 1800000);

    plog_notice(plugin, "plugin initialized");
    return rv;
}

static int ekuiper_plugin_uninit(neu_plugin_t *plugin)
{
    int rv = 0;

    nng_close(plugin->sock);
    nng_aio_free(plugin->recv_aio);
    nng_mtx_free(plugin->mtx);
    free(plugin->host);
    free(plugin->url);

    plog_notice(plugin, "plugin uninitialized");
    return rv;
}

static inline int start(neu_plugin_t *plugin, const char *url)
{
    int rv = 0;

    rv = nng_pair0_open(&plugin->sock);
    if (rv != 0) {
        plog_error(plugin, "nng_pair0_open: %s", nng_strerror(rv));
        return NEU_ERR_EINTERNAL;
    }

    nng_pipe_notify(plugin->sock, NNG_PIPE_EV_ADD_POST, pipe_add_cb, plugin);
    nng_pipe_notify(plugin->sock, NNG_PIPE_EV_REM_POST, pipe_rm_cb, plugin);
    nng_socket_set_int(plugin->sock, NNG_OPT_SENDBUF, 2048);
    nng_socket_set_int(plugin->sock, NNG_OPT_RECVBUF, 2048);

    if ((rv = nng_listen(plugin->sock, url, NULL, 0)) != 0) {
        plog_error(plugin, "nng_listen: %s", nng_strerror(rv));
        nng_close(plugin->sock);
        if (NNG_EADDRINVAL == rv) {
            rv = NEU_ERR_IP_ADDRESS_INVALID;
        } else if (NNG_EADDRINUSE == rv) {
            rv = NEU_ERR_IP_ADDRESS_IN_USE;
        } else {
            rv = NEU_ERR_EINTERNAL;
        }
        return rv;
    }

    nng_mtx_lock(plugin->mtx);
    while (plugin->receiving) {
        nng_mtx_unlock(plugin->mtx);
        nng_msleep(10);
        nng_mtx_lock(plugin->mtx);
    }
    plugin->receiving = true;
    nng_recv_aio(plugin->sock, plugin->recv_aio);
    nng_mtx_unlock(plugin->mtx);

    return NEU_ERR_SUCCESS;
}

static int ekuiper_plugin_start(neu_plugin_t *plugin)
{
    int   rv  = 0;
    char *url = plugin->url ? plugin->url : EKUIPER_PLUGIN_URL; // default url

    rv = start(plugin, url);
    if (rv != 0) {
        return rv;
    }

    plugin->started = true;
    plog_notice(plugin, "start successfully");

    return NEU_ERR_SUCCESS;
}

static inline void stop(neu_plugin_t *plugin)
{
    nng_close(plugin->sock);
}

static int ekuiper_plugin_stop(neu_plugin_t *plugin)
{
    stop(plugin);
    plugin->started = false;
    plog_notice(plugin, "stop successfully");
    return NEU_ERR_SUCCESS;
}

static int parse_config(neu_plugin_t *plugin, const char *setting,
                        char **host_p, uint16_t *port_p)
{
    char *          err_param = NULL;
    neu_json_elem_t host      = { .name = "host", .t = NEU_JSON_STR };
    neu_json_elem_t port      = { .name = "port", .t = NEU_JSON_INT };

    if (0 != neu_parse_param(setting, &err_param, 2, &host, &port)) {
        plog_error(plugin, "parsing setting fail, key: `%s`", err_param);
        goto error;
    }

    // host, required
    if (0 == strlen(host.v.val_str)) {
        plog_error(plugin, "setting invalid host: `%s`", host.v.val_str);
        goto error;
    }

    struct in_addr addr;
    if (0 == inet_aton(host.v.val_str, &addr)) {
        plog_error(plugin, "inet_aton fail: %s", host.v.val_str);
        goto error;
    }

    // port, required
    if (0 == port.v.val_int || port.v.val_int > 65535) {
        plog_error(plugin, "setting invalid port: %" PRIi64, port.v.val_int);
        goto error;
    }

    *host_p = host.v.val_str;
    *port_p = port.v.val_int;

    plog_notice(plugin, "config host:%s port:%" PRIu16, *host_p, *port_p);

    return 0;

error:
    free(err_param);
    free(host.v.val_str);
    return -1;
}

static int ekuiper_plugin_config(neu_plugin_t *plugin, const char *setting)
{
    int      rv   = 0;
    char *   url  = NULL;
    char *   host = NULL;
    uint16_t port = 0;

    if (0 != parse_config(plugin, setting, &host, &port)) {
        rv = NEU_ERR_NODE_SETTING_INVALID;
        goto error;
    }

    neu_asprintf(&url, "tcp://%s:%" PRIu16, host, port);
    if (NULL == url) {
        plog_error(plugin, "create url fail");
        rv = NEU_ERR_EINTERNAL;
        goto error;
    }

    if (plugin->started) {
        stop(plugin);
    }

    // check we could start the plugin with the new setting
    if (0 != (rv = start(plugin, url))) {
        // recover with old setting
        if (plugin->started && 0 != start(plugin, plugin->url)) {
            plog_warn(plugin, "restart host:%s port:%" PRIu16 " fail",
                      plugin->host, plugin->port);
        }
        goto error;
    }

    if (!plugin->started) {
        stop(plugin);
    }

    plog_notice(plugin, "config success");

    free(plugin->host);
    free(plugin->url);
    plugin->host = host;
    plugin->port = port;
    plugin->url  = url;

    return rv;

error:
    free(url);
    free(host);
    plog_error(plugin, "config failure");
    return rv;
}

static int ekuiper_plugin_request(neu_plugin_t *      plugin,
                                  neu_reqresp_head_t *header, void *data)
{
    bool disconnected = false;

    plog_debug(plugin, "handling request type: %d", header->type);

    nng_mtx_lock(plugin->mtx);
    disconnected =
        NEU_NODE_LINK_STATE_DISCONNECTED == plugin->common.link_state;
    nng_mtx_unlock(plugin->mtx);

    neu_otel_trace_ctx trace = NULL;
    neu_otel_scope_ctx scope = NULL;
    if (neu_otel_control_is_started()) {
        trace = neu_otel_find_trace(header->ctx);
        if (trace) {
            scope = neu_otel_add_span(trace);
            neu_otel_scope_set_span_name(scope, "ekuiper response");
            char new_span_id[36] = { 0 };
            neu_otel_new_span_id(new_span_id);
            neu_otel_scope_set_span_id(scope, new_span_id);
            uint8_t *p_sp_id = neu_otel_scope_get_pre_span_id(scope);
            if (p_sp_id) {
                neu_otel_scope_set_parent_span_id2(scope, p_sp_id, 8);
            }
            neu_otel_scope_add_span_attr_int(scope, "thread id",
                                             (int64_t)(pthread_self()));
            neu_otel_scope_set_span_start_time(scope, neu_time_ns());
        }
    }

    switch (header->type) {
    case NEU_RESP_ERROR: {
        neu_resp_error_t *error = (neu_resp_error_t *) data;
        plog_debug(plugin, "receive resp errcode: %d", error->error);
        if (trace) {
            if (error->error != NEU_ERR_SUCCESS) {
                neu_otel_scope_set_status_code2(scope, NEU_OTEL_STATUS_ERROR,
                                                error->error);
            } else {
                neu_otel_scope_set_status_code2(scope, NEU_OTEL_STATUS_OK,
                                                error->error);
            }
            neu_otel_scope_set_span_end_time(scope, neu_time_ns());
            neu_otel_trace_set_final(trace);
        }
        if (header->ctx) {
            free(header->ctx);
        }
        break;
    }
    case NEU_REQRESP_TRANS_DATA: {
        neu_reqresp_trans_data_t *trans_data = data;

        if (plugin->started) {
            NEU_PLUGIN_UPDATE_METRIC(plugin, NEU_METRIC_TRANS_DATA_5S, 1, NULL);
            NEU_PLUGIN_UPDATE_METRIC(plugin, NEU_METRIC_TRANS_DATA_30S, 1,
                                     NULL);
            NEU_PLUGIN_UPDATE_METRIC(plugin, NEU_METRIC_TRANS_DATA_60S, 1,
                                     NULL);
        }

        if (disconnected) {
            plog_debug(plugin, "not connected");

            neu_otel_trace_ctx trans_trace = NULL;
            neu_otel_scope_ctx trans_scope = NULL;
            if (neu_otel_data_is_started() && trans_data->trace_ctx) {
                trans_trace = neu_otel_find_trace(trans_data->trace_ctx);
                if (trans_trace) {
                    char new_span_id[36] = { 0 };
                    neu_otel_new_span_id(new_span_id);
                    trans_scope = neu_otel_add_span2(
                        trans_trace, "ekuiper send", new_span_id);
                    neu_otel_scope_add_span_attr_int(trans_scope, "thread id",
                                                     (int64_t)(pthread_self()));
                    neu_otel_scope_set_status_code2(
                        trans_scope, NEU_OTEL_STATUS_ERROR,
                        NEU_ERR_PLUGIN_DISCONNECTED);
                    neu_otel_scope_set_span_start_time(trans_scope,
                                                       neu_time_ns());
                    neu_otel_scope_set_span_end_time(trans_scope,
                                                     neu_time_ns());
                    neu_otel_trace_set_final(trans_trace);
                }
            }

        } else {
            send_data(plugin, trans_data);
        }
        break;
    }
    case NEU_REQRESP_NODE_DELETED: {
        break;
    }
    case NEU_REQ_UPDATE_NODE: {
        break;
    }
    case NEU_REQ_UPDATE_GROUP: {
        break;
    }
    case NEU_REQ_DEL_GROUP:
        break;
    case NEU_REQ_SUBSCRIBE_GROUP:
    case NEU_REQ_UPDATE_SUBSCRIBE_GROUP: {
        neu_req_subscribe_t *sub_info = data;
        free(sub_info->params);
        break;
    }
    case NEU_REQ_UNSUBSCRIBE_GROUP:
        break;
    default:
        plog_warn(plugin, "unsupported request type: %d", header->type);
        break;
    }

    return 0;
}

static const neu_plugin_intf_funs_t plugin_intf_funs = {
    .open    = ekuiper_plugin_open,
    .close   = ekuiper_plugin_close,
    .init    = ekuiper_plugin_init,
    .uninit  = ekuiper_plugin_uninit,
    .start   = ekuiper_plugin_start,
    .stop    = ekuiper_plugin_stop,
    .setting = ekuiper_plugin_config,
    .request = ekuiper_plugin_request,
};

const neu_plugin_module_t neu_plugin_module = {
    .version         = NEURON_PLUGIN_VER_1_0,
    .schema          = "ekuiper",
    .module_name     = "eKuiper",
    .module_descr    = "LF Edge eKuiper integration plugin",
    .module_descr_zh = "LF Edge eKuiper 一体化插件",
    .intf_funs       = &plugin_intf_funs,
    .kind            = NEU_PLUGIN_KIND_SYSTEM,
    .type            = NEU_NA_TYPE_APP,
    .display         = true,
    .single          = false,
};
