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
#include <stdlib.h>

#include <neuron.h>

#include "errcodes.h"

#include "c1.h"

static neu_plugin_t *driver_open(void);

static int driver_close(neu_plugin_t *plugin);
static int driver_init(neu_plugin_t *plugin, bool load);
static int driver_uninit(neu_plugin_t *plugin);
static int driver_start(neu_plugin_t *plugin);
static int driver_stop(neu_plugin_t *plugin);
static int driver_config(neu_plugin_t *plugin, const char *config);
static int driver_request(neu_plugin_t *plugin, neu_reqresp_head_t *head,
                          void *data);

static int driver_tag_validator(const neu_datatag_t *tag);
static int driver_validate_tag(neu_plugin_t *plugin, neu_datatag_t *tag);
static int driver_group_timer(neu_plugin_t *plugin, neu_plugin_group_t *group);
static int driver_write(neu_plugin_t *plugin, void *req, neu_datatag_t *tag,
                        neu_value_u value);
static int driver_write_tags(neu_plugin_t *plugin, void *req, UT_array *tags);

static const neu_plugin_intf_funs_t plugin_intf_funs = {
    .open    = driver_open,
    .close   = driver_close,
    .init    = driver_init,
    .uninit  = driver_uninit,
    .start   = driver_start,
    .stop    = driver_stop,
    .setting = driver_config,
    .request = driver_request,

    .driver.validate_tag  = driver_validate_tag,
    .driver.group_timer   = driver_group_timer,
    .driver.write_tag     = driver_write,
    .driver.tag_validator = driver_tag_validator,
    .driver.write_tags    = driver_write_tags,
    .driver.add_tags      = NULL,
    .driver.load_tags     = NULL,
    .driver.del_tags      = NULL,
};

const neu_plugin_module_t neu_plugin_module = {
    .version         = NEU_VERSION(2, 10, 99),
    .schema          = "c1",
    .module_name     = "c1",
    .module_descr    = "c1 v2",
    .module_descr_zh = "c1 v2",
    .intf_funs       = &plugin_intf_funs,
    .kind            = NEU_PLUGIN_KIND_CUSTOM,
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

static int driver_init(neu_plugin_t *plugin, bool load)
{
    (void) load;
    (void) plugin;

    return 0;
}

static int driver_uninit(neu_plugin_t *plugin)
{
    (void) plugin;
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
    (void) plugin;
    (void) config;
    return 0;
}

static int driver_request(neu_plugin_t *plugin, neu_reqresp_head_t *head,
                          void *data)
{
    (void) data;
    switch (head->type) {
    default:
        plog_warn(plugin, "unhandle msg type: %d", head->type);
        break;
    }
    return 0;
}

static int driver_tag_validator(const neu_datatag_t *tag)
{
    (void) tag;
    return 0;
}

static int driver_validate_tag(neu_plugin_t *plugin, neu_datatag_t *tag)
{
    (void) plugin;
    (void) tag;
    return 0;
}

static int driver_group_timer(neu_plugin_t *plugin, neu_plugin_group_t *group)
{
    (void) plugin;
    (void) group;
    return 0;
}

static int driver_write(neu_plugin_t *plugin, void *req, neu_datatag_t *tag,
                        neu_value_u value)
{
    (void) plugin;
    (void) req;
    (void) tag;
    (void) value;
    return 0;
}

static int driver_write_tags(neu_plugin_t *plugin, void *req, UT_array *tags)
{
    (void) plugin;
    (void) req;
    (void) tags;
    return 0;
}
