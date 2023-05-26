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

#include "modbus_point.h"
#include "modbus_req.h"
#include "modbus_stack.h"

static neu_plugin_t *driver_open(void);

static int driver_close(neu_plugin_t *plugin);
static int driver_init(neu_plugin_t *plugin);
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
    .driver.write_tag     = driver_write,
    .driver.tag_validator = driver_tag_validator,
};

const neu_plugin_module_t neu_plugin_module = {
    .version     = NEURON_PLUGIN_VER_1_0,
    .schema      = "modbus-qh-tcp",
    .module_name = "Modbus TCP QH",
    .module_descr =
        "The modbus-qh-tcp plugin is an optimized version of modbus tcp, which "
        "expands the packet description field to two bytes, limiting the "
        "maximum length of Modbus tcp packets to 64KB",
    .module_descr_zh =
        "该插件是 modbus-tcp 插件的优化版，数据包描述字段扩展为两个字节,限制 "
        "Modbus TCP 数据包的最大长度为 64KB。",
    .intf_funs  = &plugin_intf_funs,
    .kind       = NEU_PLUGIN_KIND_SYSTEM,
    .type       = NEU_NA_TYPE_DRIVER,
    .display    = true,
    .single     = false,
    .timer_type = NEU_EVENT_TIMER_NOBLOCK,
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
    plugin->protocol = MODBUS_PROTOCOL_TCP;
    plugin->events   = neu_event_new();
    plugin->stack    = modbus_stack_create((void *) plugin, MODBUS_PROTOCOL_TCP,
                                        modbus_send_msg, modbus_value_handle,
                                        modbus_write_resp);

    plog_notice(plugin, "%s init success", plugin->common.name);
    return 0;
}

static int driver_uninit(neu_plugin_t *plugin)
{
    plog_notice(plugin, "%s uninit start", plugin->common.name);
    if (plugin->conn != NULL) {
        neu_conn_destory(plugin->conn);
    }

    if (plugin->stack) {
        modbus_stack_destroy(plugin->stack);
    }

    neu_event_close(plugin->events);

    plog_notice(plugin, "%s uninit success", plugin->common.name);

    return 0;
}

static int driver_start(neu_plugin_t *plugin)
{
    neu_conn_start(plugin->conn);
    plog_notice(plugin, "%s start success", plugin->common.name);
    return 0;
}

static int driver_stop(neu_plugin_t *plugin)
{
    neu_conn_stop(plugin->conn);
    plog_notice(plugin, "%s stop success", plugin->common.name);
    return 0;
}

static int driver_config(neu_plugin_t *plugin, const char *config)
{
    int              ret       = 0;
    char *           err_param = NULL;
    neu_json_elem_t  port      = { .name = "port", .t = NEU_JSON_INT };
    neu_json_elem_t  timeout   = { .name = "timeout", .t = NEU_JSON_INT };
    neu_json_elem_t  host      = { .name = "host", .t = NEU_JSON_STR };
    neu_json_elem_t  mode  = { .name = "connection_mode", .t = NEU_JSON_INT };
    neu_conn_param_t param = { 0 };

    ret = neu_parse_param((char *) config, &err_param, 4, &port, &host, &mode,
                          &timeout);

    if (ret != 0) {
        plog_error(plugin, "config: %s, decode error: %s", config, err_param);
        free(err_param);
        return -1;
    }

    param.log = plugin->common.log;
    if (mode.v.val_int == 1) {
        param.type                           = NEU_CONN_TCP_SERVER;
        param.params.tcp_server.ip           = host.v.val_str;
        param.params.tcp_server.port         = port.v.val_int;
        param.params.tcp_server.start_listen = modbus_tcp_server_listen;
        param.params.tcp_server.stop_listen  = modbus_tcp_server_stop;
        param.params.tcp_server.timeout      = timeout.v.val_int;
        param.params.tcp_server.max_link     = 1;
        plugin->is_server                    = true;
    } else {
        param.type                      = NEU_CONN_TCP_CLIENT;
        param.params.tcp_client.ip      = host.v.val_str;
        param.params.tcp_client.port    = port.v.val_int;
        param.params.tcp_client.timeout = timeout.v.val_int;
        plugin->is_server               = false;
    }
    plog_notice(plugin,
                "config: host: %s, port: %" PRId64 ", mode: %" PRId64 "",
                host.v.val_str, port.v.val_int, mode.v.val_int);

    if (plugin->conn != NULL) {
        plugin->conn = neu_conn_reconfig(plugin->conn, &param);
    } else {
        plugin->common.link_state = NEU_NODE_LINK_STATE_DISCONNECTED;
        plugin->conn =
            neu_conn_new(&param, (void *) plugin, modbus_conn_connected,
                         modbus_conn_disconnected);
    }

    free(host.v.val_str);
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
    modbus_point_t point = { 0 };
    return modbus_tag_to_point(tag, &point);
}

static int driver_validate_tag(neu_plugin_t *plugin, neu_datatag_t *tag)
{
    modbus_point_t point = { 0 };

    int ret = modbus_tag_to_point(tag, &point);
    if (ret == 0) {
        plog_notice(
            plugin,
            "validate tag success, name: %s, address: %s, type: %d, slave id: "
            "%d, start address: %d, n register: %d, area: %s",
            tag->name, tag->address, tag->type, point.slave_id,
            point.start_address, point.n_register,
            modbus_area_to_str(point.area));
    } else {
        plog_error(
            plugin,
            "validate tag error, name: %s, address: %s, type: %d, slave id: "
            "%d, start address: %d, n register: %d, area: %s",
            tag->name, tag->address, tag->type, point.slave_id,
            point.start_address, point.n_register,
            modbus_area_to_str(point.area));
    }

    return ret;
}

static int driver_group_timer(neu_plugin_t *plugin, neu_plugin_group_t *group)
{
    return modbus_group_timer(plugin, group, 0xffff);
}

static int driver_write(neu_plugin_t *plugin, void *req, neu_datatag_t *tag,
                        neu_value_u value)
{
    return modbus_write(plugin, req, tag, value);
}