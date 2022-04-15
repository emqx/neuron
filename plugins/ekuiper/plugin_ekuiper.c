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
        log_error("Open plugin with NULL adapter or callbacks");
        return NULL;
    }

    plugin = (neu_plugin_t *) malloc(sizeof(neu_plugin_t));
    if (plugin == NULL) {
        log_error("Failed to allocate plugin %s",
                  neu_plugin_module.module_name);
        return NULL;
    }

    neu_plugin_common_init(&plugin->common);
    plugin->common.adapter           = adapter;
    plugin->common.adapter_callbacks = callbacks;
    plugin->common.link_state        = NEU_PLUGIN_LINK_STATE_DISCONNECTED;

    log_info("Success to create plugin: %s", neu_plugin_module.module_name);
    return plugin;
}

static int ekuiper_plugin_close(neu_plugin_t *plugin)
{
    int rv = 0;

    free(plugin);
    log_info("Success to free plugin: %s", neu_plugin_module.module_name);
    return rv;
}

static int ekuiper_plugin_init(neu_plugin_t *plugin)
{
    int      rv       = 0;
    nng_aio *recv_aio = NULL;

    rv = nng_aio_alloc(&recv_aio, recv_data_callback, plugin);
    if (rv < 0) {
        log_error("cannot allocate recv_aio: %s", nng_strerror(rv));
        return rv;
    }

    plugin->recv_aio = recv_aio;

    log_info("Initialized plugin: %s", neu_plugin_module.module_name);
    return rv;
}

static int ekuiper_plugin_uninit(neu_plugin_t *plugin)
{
    int rv = 0;

    nng_aio_free(plugin->recv_aio);

    log_info("Uninitialize plugin: %s", neu_plugin_module.module_name);
    return rv;
}

static int ekuiper_plugin_start(neu_plugin_t *plugin)
{
    int rv = 0;

    rv = nng_pair0_open(&plugin->sock);
    if (rv != 0) {
        log_error("nng_pair0_open: %s", nng_strerror(rv));
        return NEU_ERR_FAILURE;
    }

    if ((rv = nng_listen(plugin->sock, EKUIPER_PLUGIN_URL, NULL, 0)) != 0) {
        log_error("nng_listen: %s", nng_strerror(rv));
        return NEU_ERR_FAILURE;
    }

    nng_recv_aio(plugin->sock, plugin->recv_aio);

    plugin->common.link_state = NEU_PLUGIN_LINK_STATE_CONNECTED;
    return NEU_ERR_SUCCESS;
}

static int ekuiper_plugin_stop(neu_plugin_t *plugin)
{
    nng_close(plugin->sock);
    plugin->common.link_state = NEU_PLUGIN_LINK_STATE_DISCONNECTED;
    return NEU_ERR_SUCCESS;
}

static int ekuiper_plugin_config(neu_plugin_t *plugin, neu_config_t *configs)
{
    int rv = 0;

    (void) plugin;
    (void) configs;

    log_info("config plugin: %s", neu_plugin_module.module_name);
    return rv;
}

static int ekuiper_plugin_request(neu_plugin_t *plugin, neu_request_t *req)
{
    int rv = 0;

    if (plugin == NULL || req == NULL) {
        log_warn("The plugin pointer or request is NULL");
        return (-1);
    }

    log_info("send request to plugin: %s", neu_plugin_module.module_name);
    const adapter_callbacks_t *adapter_callbacks;
    adapter_callbacks = plugin->common.adapter_callbacks;
    (void) adapter_callbacks;

    switch (req->req_type) {
    case NEU_REQRESP_TRANS_DATA: {
        send_data(plugin, req);
        break;
    }

    default:
        break;
    }
    return rv;
}

static int ekuiper_plugin_event_reply(neu_plugin_t *     plugin,
                                      neu_event_reply_t *reply)
{
    int rv = 0;

    (void) plugin;
    (void) reply;

    log_info("reply event to plugin: %s", neu_plugin_module.module_name);
    return rv;
}

static int ekuiper_plugin_validate_tag(neu_plugin_t *plugin, neu_datatag_t *tag)
{
    (void) plugin;
    (void) tag;

    return 0;
}

static const neu_plugin_intf_funs_t plugin_intf_funs = {
    .open         = ekuiper_plugin_open,
    .close        = ekuiper_plugin_close,
    .init         = ekuiper_plugin_init,
    .uninit       = ekuiper_plugin_uninit,
    .start        = ekuiper_plugin_start,
    .stop         = ekuiper_plugin_stop,
    .config       = ekuiper_plugin_config,
    .request      = ekuiper_plugin_request,
    .validate_tag = ekuiper_plugin_validate_tag,
    .event_reply  = ekuiper_plugin_event_reply
};

const neu_plugin_module_t neu_plugin_module = {
    .version      = NEURON_PLUGIN_VER_1_0,
    .module_name  = "ekuiper",
    .module_descr = "Neuron and LF Edge eKuiper integration plugin",
    .intf_funs    = &plugin_intf_funs,
    .kind         = PLUGIN_KIND_SYSTEM,
    .type         = NEU_NODE_TYPE_APP,
};
