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
#include <errno.h>
#include <stdlib.h>

#include <neuron.h>

#include "errcodes.h"

struct neu_plugin {
    neu_plugin_common_t common;

    // neu_events_t *events;

    // int file_length;
};

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
    .driver.group_sync    = NULL,
    .driver.write_tag     = driver_write,
    .driver.tag_validator = driver_tag_validator,
    .driver.write_tags    = NULL,
    .driver.test_read_tag = NULL,
    .driver.add_tags      = NULL,
    .driver.load_tags     = NULL,
    .driver.del_tags      = NULL,
};

const neu_plugin_module_t neu_plugin_module = {
    .version         = NEURON_PLUGIN_VER_1_0,
    .schema          = "file",
    .module_name     = "File",
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

static int driver_init(neu_plugin_t *plugin, bool load)
{
    (void) load;
    plog_notice(plugin, "%s init success", plugin->common.name);
    return 0;
}

static int driver_uninit(neu_plugin_t *plugin)
{
    plog_notice(plugin, "%s uninit success", plugin->common.name);

    return 0;
}

static int driver_start(neu_plugin_t *plugin)
{
    neu_adapter_update_metric_cb_t update_metric =
        plugin->common.adapter_callbacks->update_metric;
    update_metric(plugin->common.adapter, NEU_METRIC_LAST_RTT_MS, 1, NULL);

    plugin->common.link_state = NEU_NODE_LINK_STATE_CONNECTED;
    plog_notice(plugin, "start `%s` plugin success", plugin->common.name);

    return 0;
}

static int driver_stop(neu_plugin_t *plugin)
{
    neu_adapter_update_metric_cb_t update_metric =
        plugin->common.adapter_callbacks->update_metric;
    update_metric(plugin->common.adapter, NEU_METRIC_LAST_RTT_MS, 9999, NULL);

    plugin->common.link_state = NEU_NODE_LINK_STATE_DISCONNECTED;
    plog_notice(plugin, "stop `%s` plugin success", plugin->common.name);

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
    (void) plugin;
    (void) head;
    (void) data;
    return 0;
}

static int driver_tag_validator(const neu_datatag_t *tag)
{
    (void) tag;
    return 0;
}

static int driver_validate_tag(neu_plugin_t *plugin, neu_datatag_t *tag)
{
    if (tag->type != NEU_TYPE_STRING) {
        return NEU_ERR_TAG_TYPE_NOT_SUPPORT;
    }

    plog_notice(plugin, "validate tag success, name:%s, address:%s, type:%d ",
                tag->name, tag->address, tag->type);

    return 0;
}

static int driver_group_timer(neu_plugin_t *plugin, neu_plugin_group_t *group)
{
    neu_adapter_update_metric_cb_t update_metric =
        plugin->common.adapter_callbacks->update_metric;

    utarray_foreach(group->tags, neu_datatag_t *, tag)
    {
        neu_dvalue_t dvalue = { 0 };

        FILE *fp = fopen(tag->address, "r");
        if (fp == NULL) {
            dvalue.type = NEU_TYPE_ERROR;
            if (errno == ENOENT) {
                dvalue.value.i32 = NEU_ERR_FILE_NOT_EXIST;
            } else {
                dvalue.value.i32 = NEU_ERR_FILE_OPEN_FAILURE;
            }
        } else {
            char *buf = calloc(1, 1024);
            int   ret = fread(buf, 1024, 1, fp);
            if (ret < 0) {
                dvalue.type      = NEU_TYPE_ERROR;
                dvalue.value.i32 = NEU_ERR_FILE_READ_FAILURE;
            } else if (ret < NEU_VALUE_SIZE) {
                dvalue.type = NEU_TYPE_STRING;
                strcpy(dvalue.value.str, buf);
            } else {
                dvalue.type             = NEU_TYPE_PTR;
                dvalue.value.ptr.type   = NEU_TYPE_STRING;
                dvalue.value.ptr.length = ret;
                dvalue.value.ptr.ptr    = (uint8_t *) strdup(buf);
            }

            fclose(fp);
            free(buf);
        }

        plugin->common.adapter_callbacks->driver.update(
            plugin->common.adapter, group->group_name, tag->name, dvalue);
    }

    update_metric(plugin->common.adapter, NEU_METRIC_SEND_BYTES, 0, NULL);
    update_metric(plugin->common.adapter, NEU_METRIC_RECV_BYTES, 0, NULL);
    update_metric(plugin->common.adapter, NEU_METRIC_LAST_RTT_MS, 1, NULL);
    update_metric(plugin->common.adapter, NEU_METRIC_GROUP_LAST_SEND_MSGS, 1,
                  group->group_name);
    return 0;
}

static int driver_write(neu_plugin_t *plugin, void *req, neu_datatag_t *tag,
                        neu_value_u value)
{
    FILE *fp = fopen(tag->address, "w");
    if (fp == NULL) {
        if (errno == ENOENT) {
            plugin->common.adapter_callbacks->driver.write_response(
                plugin->common.adapter, req, NEU_ERR_FILE_NOT_EXIST);
        } else {
            plugin->common.adapter_callbacks->driver.write_response(
                plugin->common.adapter, req, NEU_ERR_FILE_OPEN_FAILURE);
        }
    } else {
        if (tag->type == NEU_TYPE_STRING) {
            size_t ret = fwrite(value.str, 1, strlen(value.str), fp);
            if (ret != strlen(value.str)) {
                plugin->common.adapter_callbacks->driver.write_response(
                    plugin->common.adapter, req, NEU_ERR_FILE_WRITE_FAILURE);
            }
        } else {
            plugin->common.adapter_callbacks->driver.write_response(
                plugin->common.adapter, req, NEU_ERR_PLUGIN_TAG_TYPE_MISMATCH);
        }

        fclose(fp);
        plugin->common.adapter_callbacks->driver.write_response(
            plugin->common.adapter, req, 0);
    }
    return 0;
}