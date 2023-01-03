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

#include "file_req.h"

static neu_plugin_t *driver_open(void);

static int driver_close(neu_plugin_t *plugin);
static int driver_init(neu_plugin_t *plugin);
static int driver_uninit(neu_plugin_t *plugin);
static int driver_start(neu_plugin_t *plugin);
static int driver_stop(neu_plugin_t *plugin);
static int driver_config(neu_plugin_t *plugin, const char *config);
static int driver_request(neu_plugin_t *plugin, neu_reqresp_head_t *head,
                          void *data);

static int driver_group_timer(neu_plugin_t *plugin, neu_plugin_group_t *group);
static int driver_validate_tag(neu_plugin_t *plugin, neu_datatag_t *tag);
static int driver_write(neu_plugin_t *plugin, void *req, neu_datatag_t *tag,
                        neu_value_u value);

static const neu_plugin_intf_funs_t plugin_intf_funs = {
    .open    = driver_open,
    .close   = driver_close,
    .init    = driver_init,
    .uninit  = driver_uninit,
    .start   = driver_start,
    .stop    = driver_stop,
    .setting = driver_config,
    .request = driver_request,

    .driver.validate_tag = driver_validate_tag,
    .driver.group_timer  = driver_group_timer,
    .driver.write_tag    = driver_write,
};

const neu_plugin_module_t neu_plugin_module = {
    .version         = NEURON_PLUGIN_VER_1_0,
    .schema          = "file.json",
    .module_name     = "file",
    .module_descr    = "The plugin is used to read and write files.",
    .module_descr_zh = "该插件用于读写文件。",
    .intf_funs       = &plugin_intf_funs,
    .kind            = NEU_PLUGIN_KIND_SYSTEM,
    .type            = NEU_NA_TYPE_DRIVER,
    .display         = true,
    .single          = false,
};

static neu_plugin_t *driver_open(void)
{
    neu_plugin_t *plugin = calloc(1, sizeof(neu_plugin_t));

    neu_plugin_common_init(&plugin->common);

    return plugin;
}

static int driver_close(neu_plugin_t *plugin)
{
    free(plugin);

    return 0;
}

static int driver_init(neu_plugin_t *plugin)
{
    plugin->events            = neu_event_new();
    plugin->common.link_state = NEU_NODE_LINK_STATE_CONNECTED;

    return 0;
}

static int driver_uninit(neu_plugin_t *plugin)
{
    neu_event_close(plugin->events);
    plog_info(plugin, "node; file uininit ok");

    return 0;
}

static int driver_start(neu_plugin_t *plugin)
{
    (void) plugin;

    return 0;
}

static int driver_stop(neu_plugin_t *plugin)
{
    (void) plugin;

    return 0;
}

static int driver_config(neu_plugin_t *plugin, const char *config)
{
    char *          err_param   = NULL;
    neu_json_elem_t file_length = { .name = "file_length", .t = NEU_JSON_INT };

    int ret = neu_parse_param((char *) config, &err_param, 1, &file_length);
    if (ret != 0) {
        plog_warn(plugin, "config:%s, decode error: %s", config, err_param);
        free(err_param);
        return -1;
    }

    plugin->file_length = file_length.v.val_int;

    return 0;
}

static int driver_request(neu_plugin_t *plugin, neu_reqresp_head_t *head,
                          void *data)
{
    (void) plugin;
    (void) head;
    (void) data;

    return 0;
}

static int driver_validate_tag(neu_plugin_t *plugin, neu_datatag_t *tag)
{
    plog_debug(plugin, "validate tag success, name:%s, address:%s,type:%d ",
               tag->name, tag->address, tag->type);

    return 0;
}

static int driver_group_timer(neu_plugin_t *plugin, neu_plugin_group_t *group)
{
    return file_group_timer(plugin, group);
}

static int driver_write(neu_plugin_t *plugin, void *req, neu_datatag_t *tag,
                        neu_value_u value)
{
    return file_write(plugin, req, tag, value);
}
