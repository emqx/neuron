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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <nng/nng.h>

#include "monitor.h"
#include "utils/log.h"

#define neu_plugin_module default_monitor_plugin_module

struct neu_plugin {
    neu_plugin_common_t common;
};

static neu_plugin_t *monitor_plugin_open(void)
{
    neu_plugin_t *plugin = calloc(1, sizeof(neu_plugin_t));

    neu_plugin_common_init(&plugin->common);

    return plugin;
}

static int monitor_plugin_close(neu_plugin_t *plugin)
{
    int rv = 0;

    free(plugin);
    nlog_info("Success to free plugin: %s", neu_plugin_module.module_name);

    return rv;
}

static int monitor_plugin_init(neu_plugin_t *plugin)
{
    int rv = 0;

    (void) plugin;

    nlog_info("Initialize plugin: %s", neu_plugin_module.module_name);
    return rv;
}

static int monitor_plugin_uninit(neu_plugin_t *plugin)
{
    int rv = 0;

    (void) plugin;

    nlog_info("Uninitialize plugin: %s", neu_plugin_module.module_name);
    return rv;
}

static int monitor_plugin_config(neu_plugin_t *plugin, const char *configs)
{
    int rv = 0;

    (void) plugin;
    (void) configs;

    nlog_info("config plugin: %s", neu_plugin_module.module_name);
    return rv;
}

static int monitor_plugin_request(neu_plugin_t *      plugin,
                                  neu_reqresp_head_t *header, void *data)
{
    (void) plugin;

    switch (header->type) {
    case NEU_RESP_ERROR: {
        neu_resp_error_t *resp_err = (neu_resp_error_t *) data;
        nlog_warn("recv error code: %d", resp_err->error);
        break;
    }
    default:
        nlog_fatal("recv unhandle msg: %s",
                   neu_reqresp_type_string(header->type));
        assert(false);
        break;
    }
    return 0;
}

static int monitor_plugin_start(neu_plugin_t *plugin)
{
    (void) plugin;
    return 0;
}

static int monitor_plugin_stop(neu_plugin_t *plugin)
{
    (void) plugin;
    return 0;
}

static const neu_plugin_intf_funs_t plugin_intf_funs = {
    .open    = monitor_plugin_open,
    .close   = monitor_plugin_close,
    .init    = monitor_plugin_init,
    .uninit  = monitor_plugin_uninit,
    .start   = monitor_plugin_start,
    .stop    = monitor_plugin_stop,
    .setting = monitor_plugin_config,
    .request = monitor_plugin_request,
};

#define DEFAULT_MONITOR_PLUGIN_DESCR \
    "Builtin plugin for Neuron monitoring and alerting"

const neu_plugin_module_t default_monitor_plugin_module = {
    .version      = NEURON_PLUGIN_VER_1_0,
    .module_name  = "neuron-default-monitor",
    .module_descr = DEFAULT_MONITOR_PLUGIN_DESCR,
    .intf_funs    = &plugin_intf_funs,
    .kind         = NEU_PLUGIN_KIND_SYSTEM,
    .type         = NEU_NA_TYPE_APP,
};
