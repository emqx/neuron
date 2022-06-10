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

#include "errcodes.h"
#include "neuron.h"

#include "plugin_ekuiper.h"
#include "read_write.h"

#define EKUIPER_PLUGIN_URL "ipc:///tmp/neuron-ekuiper.ipc"

const neu_plugin_module_t neu_plugin_module;

static neu_plugin_t *ekuiper_plugin_open(neu_adapter_t *            adapter,
                                         const adapter_callbacks_t *callbacks)
{
    neu_plugin_t *plugin;

    if (adapter == NULL || callbacks == NULL) {
        zlog_error(neuron, "Open plugin with NULL adapter or callbacks");
        return NULL;
    }

    plugin = (neu_plugin_t *) malloc(sizeof(neu_plugin_t));
    if (plugin == NULL) {
        zlog_error(neuron, "Failed to allocate plugin %s",
                   neu_plugin_module.module_name);
        return NULL;
    }

    neu_plugin_common_init(&plugin->common);
    plugin->common.adapter           = adapter;
    plugin->common.adapter_callbacks = callbacks;
    plugin->common.link_state        = NEU_PLUGIN_LINK_STATE_DISCONNECTED;

    zlog_info(neuron, "Success to create plugin: %s",
              neu_plugin_module.module_name);
    return plugin;
}

static int ekuiper_plugin_close(neu_plugin_t *plugin)
{
    int rv = 0;

    free(plugin);
    zlog_info(neuron, "Success to free plugin: %s",
              neu_plugin_module.module_name);
    return rv;
}

static void pipe_add_cb(nng_pipe p, nng_pipe_ev ev, void *arg)
{
    (void) p;
    (void) ev;
    neu_plugin_t *plugin = arg;
    atomic_store(&plugin->connected, true);
    plugin->common.link_state = NEU_PLUGIN_LINK_STATE_CONNECTED;
}

static void pipe_rm_cb(nng_pipe p, nng_pipe_ev ev, void *arg)
{
    (void) p;
    (void) ev;
    neu_plugin_t *plugin = arg;
    atomic_store(&plugin->connected, false);
    plugin->common.link_state = NEU_PLUGIN_LINK_STATE_DISCONNECTED;
}

static int ekuiper_plugin_init(neu_plugin_t *plugin)
{
    int      rv       = 0;
    nng_aio *recv_aio = NULL;

    atomic_init(&plugin->connected, false);

    rv = nng_aio_alloc(&recv_aio, recv_data_callback, plugin);
    if (rv < 0) {
        zlog_error(neuron, "cannot allocate recv_aio: %s", nng_strerror(rv));
        return rv;
    }

    plugin->recv_aio = recv_aio;

    zlog_info(neuron, "Initialized plugin: %s", neu_plugin_module.module_name);
    return rv;
}

static int ekuiper_plugin_uninit(neu_plugin_t *plugin)
{
    int rv = 0;

    nng_aio_free(plugin->recv_aio);

    zlog_info(neuron, "Uninitialize plugin: %s", neu_plugin_module.module_name);
    return rv;
}

static int ekuiper_plugin_start(neu_plugin_t *plugin)
{
    int rv = 0;

    rv = nng_pair0_open(&plugin->sock);
    if (rv != 0) {
        zlog_error(neuron, "nng_pair0_open: %s", nng_strerror(rv));
        return NEU_ERR_FAILURE;
    }

    nng_pipe_notify(plugin->sock, NNG_PIPE_EV_ADD_POST, pipe_add_cb, plugin);
    nng_pipe_notify(plugin->sock, NNG_PIPE_EV_REM_POST, pipe_rm_cb, plugin);

    if ((rv = nng_listen(plugin->sock, EKUIPER_PLUGIN_URL, NULL, 0)) != 0) {
        zlog_error(neuron, "nng_listen: %s", nng_strerror(rv));
        return NEU_ERR_FAILURE;
    }

    nng_recv_aio(plugin->sock, plugin->recv_aio);

    return NEU_ERR_SUCCESS;
}

static int ekuiper_plugin_stop(neu_plugin_t *plugin)
{
    nng_close(plugin->sock);
    return NEU_ERR_SUCCESS;
}

static int ekuiper_plugin_config(neu_plugin_t *plugin, neu_config_t *configs)
{
    int rv = 0;

    (void) plugin;
    (void) configs;

    zlog_info(neuron, "config plugin: %s", neu_plugin_module.module_name);
    return rv;
}

static int ekuiper_plugin_request(neu_plugin_t *      plugin,
                                  neu_reqresp_head_t *header, void *data)
{
    zlog_info(neuron, "send request to plugin: %s",
              neu_plugin_module.module_name);

    if (plugin->common.link_state != NEU_PLUGIN_LINK_STATE_CONNECTED) {
        zlog_info(neuron, "plugin: %s not connected",
                  neu_plugin_module.module_name);
        return -1;
    }

    switch (header->type) {
    case NEU_RESP_ERROR: {
        neu_resp_error_t *error = (neu_resp_error_t *) data;
        zlog_info(neuron, "plugin: %s receive resp errcode: %d",
                  neu_plugin_module.module_name, error->error);
        break;
    }
    case NEU_REQRESP_TRANS_DATA: {
        neu_reqresp_trans_data_t *trans_data = data;
        send_data(plugin, trans_data);
        break;
    }
    default:
        assert(false);
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
    .config  = ekuiper_plugin_config,
    .request = ekuiper_plugin_request,
};

const neu_plugin_module_t neu_plugin_module = {
    .version      = NEURON_PLUGIN_VER_1_0,
    .module_name  = "ekuiper",
    .module_descr = "Neuron and LF Edge eKuiper integration plugin",
    .intf_funs    = &plugin_intf_funs,
    .kind         = NEU_PLUGIN_KIND_SYSTEM,
    .type         = NEU_NA_TYPE_APP,
};
