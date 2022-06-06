/**
 * NEURON IIoT System for Industry 4.0
 * Copyright (C) 2020-2021 EMQ Technologies Co., Ltd All rights reserved.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <nng/nng.h>
#include <nng/supplemental/http/http.h>

#include "dummy.h"
#include "plugin.h"
#include "utils/log.h"

#define neu_plugin_module default_dummy_plugin_module

struct neu_plugin {
    neu_plugin_common_t common;
};

static neu_plugin_t *dummy_plugin_open(neu_adapter_t *            adapter,
                                       const adapter_callbacks_t *callbacks)
{
    neu_plugin_t *plugin;

    if (adapter == NULL || callbacks == NULL) {
        nlog_error("Open plugin with NULL adapter or callbacks");
        return NULL;
    }

    plugin = (neu_plugin_t *) malloc(sizeof(neu_plugin_t));
    if (plugin == NULL) {
        nlog_error("Failed to allocate plugin %s",
                   neu_plugin_module.module_name);
        return NULL;
    }

    neu_plugin_common_init(&plugin->common);
    plugin->common.adapter           = adapter;
    plugin->common.adapter_callbacks = callbacks;

    nlog_info("Success to create plugin: %s", neu_plugin_module.module_name);
    return plugin;
}

static int dummy_plugin_close(neu_plugin_t *plugin)
{
    free(plugin);
    nlog_info("Success to free plugin: %s", neu_plugin_module.module_name);
    return 0;
}

static int dummy_plugin_init(neu_plugin_t *plugin)
{
    int rv = 0;

    (void) plugin;

    nlog_info("Initialize plugin: %s", neu_plugin_module.module_name);
    return rv;
}

static int dummy_plugin_uninit(neu_plugin_t *plugin)
{
    int rv = 0;

    (void) plugin;

    nlog_info("Uninitialize plugin: %s", neu_plugin_module.module_name);
    return rv;
}

static int dummy_plugin_config(neu_plugin_t *plugin, neu_config_t *configs)
{
    int rv = 0;

    (void) plugin;
    (void) configs;

    nlog_info("config plugin: %s", neu_plugin_module.module_name);
    return rv;
}

static int dummy_plugin_request(neu_plugin_t *plugin, neu_reqresp_head_t *head,
                                void *data)
{
    (void) plugin;
    (void) head;
    (void) data;
    return 0;
}

static int dummy_plugin_start(neu_plugin_t *plugin)
{
    (void) plugin;
    return 0;
}

static int dummy_plugin_stop(neu_plugin_t *plugin)
{
    (void) plugin;
    return 0;
}

static const neu_plugin_intf_funs_t plugin_intf_funs = {
    .open    = dummy_plugin_open,
    .close   = dummy_plugin_close,
    .init    = dummy_plugin_init,
    .uninit  = dummy_plugin_uninit,
    .start   = dummy_plugin_start,
    .stop    = dummy_plugin_stop,
    .config  = dummy_plugin_config,
    .request = dummy_plugin_request,
};

#define DEFAULT_DUMMY_PLUGIN_DESCR "A plugin that does nothing"

const neu_plugin_module_t default_dummy_plugin_module = {
    .version      = NEURON_PLUGIN_VER_1_0,
    .module_name  = "neuron-default-dummy",
    .module_descr = DEFAULT_DUMMY_PLUGIN_DESCR,
    .intf_funs    = &plugin_intf_funs,
    .kind         = PLUGIN_KIND_STATIC,
    .type         = NEU_NA_TYPE_APP,
};
