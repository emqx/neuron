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

#include "neuron.h"

#include "plugin_ekuiper.h"
#include "read_write.h"

#define EKUIPER_PLUGIN_URL "ipc:///tmp/neuron-ekuiper.ipc"

const neu_plugin_module_t neu_plugin_module;

static neu_plugin_t *ekuiper_plugin_open(void)
{
    neu_plugin_t *plugin = calloc(1, sizeof(neu_plugin_t));

    neu_plugin_common_init(&plugin->common);

    zlog_info(neuron, "success to create plugin: %s",
              neu_plugin_module.module_name);
    return plugin;
}

static int ekuiper_plugin_close(neu_plugin_t *plugin)
{
    int rv = 0;

    free(plugin);
    zlog_info(neuron, "success to free plugin: %s",
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
}

static int ekuiper_plugin_init(neu_plugin_t *plugin)
{
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
        return rv;
    }

    plugin->recv_aio = recv_aio;

    plog_info(plugin, "plugin initialized");
    return rv;
}

static int ekuiper_plugin_uninit(neu_plugin_t *plugin)
{
    int rv = 0;

    nng_aio_free(plugin->recv_aio);
    nng_mtx_free(plugin->mtx);

    plog_info(plugin, "plugin uninitialized");
    return rv;
}

static int ekuiper_plugin_start(neu_plugin_t *plugin)
{
    int rv = 0;

    rv = nng_pair0_open(&plugin->sock);
    if (rv != 0) {
        plog_error(plugin, "nng_pair0_open: %s", nng_strerror(rv));
        return NEU_ERR_EINTERNAL;
    }

    nng_pipe_notify(plugin->sock, NNG_PIPE_EV_ADD_POST, pipe_add_cb, plugin);
    nng_pipe_notify(plugin->sock, NNG_PIPE_EV_REM_POST, pipe_rm_cb, plugin);

    if ((rv = nng_listen(plugin->sock, EKUIPER_PLUGIN_URL, NULL, 0)) != 0) {
        plog_error(plugin, "nng_listen: %s", nng_strerror(rv));
        return NEU_ERR_EINTERNAL;
    }

    nng_recv_aio(plugin->sock, plugin->recv_aio);
    plog_notice(plugin, "start successfully");

    return NEU_ERR_SUCCESS;
}

static int ekuiper_plugin_stop(neu_plugin_t *plugin)
{
    nng_close(plugin->sock);
    plog_notice(plugin, "stop successfully");
    return NEU_ERR_SUCCESS;
}

static int ekuiper_plugin_config(neu_plugin_t *plugin, const char *configs)
{
    int rv = 0;

    (void) plugin;
    (void) configs;

    plog_info(plugin, "config success");
    return rv;
}

static int ekuiper_plugin_request(neu_plugin_t *      plugin,
                                  neu_reqresp_head_t *header, void *data)
{
    bool disconnected = false;

    plog_info(plugin, "handling request type: %d", header->type);

    nng_mtx_lock(plugin->mtx);
    disconnected =
        NEU_NODE_LINK_STATE_DISCONNECTED == plugin->common.link_state;
    nng_mtx_unlock(plugin->mtx);

    if (disconnected) {
        plog_notice(plugin, "not connected");
        return -1;
    }

    switch (header->type) {
    case NEU_RESP_ERROR: {
        neu_resp_error_t *error = (neu_resp_error_t *) data;
        plog_info(plugin, "receive resp errcode: %d", error->error);
        break;
    }
    case NEU_REQRESP_TRANS_DATA: {
        neu_reqresp_trans_data_t *trans_data = data;
        send_data(plugin, trans_data);
        break;
    }
    case NEU_RESP_APP_SUBSCRIBE_GROUP: {
        neu_resp_app_subscribe_group_t *sub =
            (neu_resp_app_subscribe_group_t *) data;

        utarray_free(sub->tags);
        break;
    }
    default:
        plog_notice(plugin, "unsupported request type: %d", header->type);
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
    .version      = NEURON_PLUGIN_VER_1_0,
    .module_name  = "ekuiper",
    .module_descr = "Neuron and LF Edge eKuiper integration plugin",
    .intf_funs    = &plugin_intf_funs,
    .kind         = NEU_PLUGIN_KIND_SYSTEM,
    .type         = NEU_NA_TYPE_APP,
};
