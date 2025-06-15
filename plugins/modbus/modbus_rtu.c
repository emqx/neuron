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
    .version     = NEURON_PLUGIN_VER_1_0,
    .schema      = "modbus-rtu",
    .module_name = "Modbus RTU",
    .module_descr =
        "This plugin is used to connect devices using modbus rtu protocol. "
        "Support serial and TCP communication methods. Selecting "
        "client/server is also supported.",
    .module_descr_zh =
        "该插件用于连接使用 modbus rtu 协议的设备。支持串口, TCP "
        "两种通信方式。也支持选择客户端/服务端。",
    .intf_funs = &plugin_intf_funs,
    .kind      = NEU_PLUGIN_KIND_SYSTEM,
    .type      = NEU_NA_TYPE_DRIVER,
    .display   = true,
    .single    = false,
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
    plugin->protocol = MODBUS_PROTOCOL_RTU;
    plugin->events   = neu_event_new();
    plugin->stack    = modbus_stack_create((void *) plugin, MODBUS_PROTOCOL_RTU,
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
    neu_conn_param_t param     = { 0 };

    neu_json_elem_t link     = { .name = "link", .t = NEU_JSON_INT };
    neu_json_elem_t timeout  = { .name = "timeout", .t = NEU_JSON_INT };
    neu_json_elem_t interval = { .name = "interval", .t = NEU_JSON_INT };

    neu_json_elem_t mode = { .name = "connection_mode", .t = NEU_JSON_INT };
    neu_json_elem_t host = { .name = "host", .t = NEU_JSON_STR };
    neu_json_elem_t port = { .name = "port", .t = NEU_JSON_INT };

    neu_json_elem_t device = { .name = "device", .t = NEU_JSON_STR };
    neu_json_elem_t stop   = { .name = "stop", .t = NEU_JSON_INT };
    neu_json_elem_t parity = { .name = "parity", .t = NEU_JSON_INT };
    neu_json_elem_t baud   = { .name = "baud", .t = NEU_JSON_INT };
    neu_json_elem_t data   = { .name = "data", .t = NEU_JSON_INT };

    neu_json_elem_t max_retries = { .name = "max_retries", .t = NEU_JSON_INT };
    neu_json_elem_t retry_interval = { .name = "retry_interval",
                                       .t    = NEU_JSON_INT };

    neu_json_elem_t endianess_64 = { .name = "endianess_64",
                                     .t    = NEU_JSON_INT };

    ret = neu_parse_param((char *) config, &err_param, 3, &link, &timeout,
                          &interval);
    if (ret != 0) {
        plog_error(plugin, "config: %s, decode link & timeout error: %s",
                   config, err_param);
        free(err_param);
        return -1;
    }

    if (timeout.v.val_int <= 0) {
        plog_error(plugin, "config: %s, set timeout error: %s", config,
                   err_param);
        free(err_param);
        return -1;
    }

    plugin->interval = interval.v.val_int;
    if (link.v.val_int == 0) {
        ret = neu_parse_param((char *) config, &err_param, 5, &device, &stop,
                              &parity, &baud, &data);
    } else {
        ret = neu_parse_param((char *) config, &err_param, 3, &mode, &host,
                              &port);
    }

    if (ret != 0) {
        plog_error(plugin, "config: %s, decode error: %s", config, err_param);
        free(err_param);
        return -1;
    }

    ret = neu_parse_param((char *) config, &err_param, 2, &port, &max_retries,
                          &retry_interval);
    if (ret != 0) {
        free(err_param);
        max_retries.v.val_int    = 0;
        retry_interval.v.val_int = 0;
    }

    ret = neu_parse_param((char *) config, &err_param, 1, &endianess_64);
    if (ret != 0) {
        free(err_param);
        endianess_64.v.val_int = MODBUS_LL;
    }

    param.log              = plugin->common.log;
    plugin->max_retries    = max_retries.v.val_int;
    plugin->retry_interval = retry_interval.v.val_int;
    plugin->endianess_64   = endianess_64.v.val_int;

    if (link.v.val_int == 0) {
        param.type = NEU_CONN_TTY_CLIENT;

        param.params.tty_client.device  = device.v.val_str;
        param.params.tty_client.baud    = baud.v.val_int;
        param.params.tty_client.data    = data.v.val_int;
        param.params.tty_client.parity  = parity.v.val_int;
        param.params.tty_client.stop    = stop.v.val_int;
        param.params.tty_client.timeout = timeout.v.val_int;

        plugin->is_serial = true;
        plog_notice(plugin,
                    "config: device: %s, baud: %" PRId64 ", data: %" PRId64
                    ", parity: %" PRId64 ", stop: %" PRId64 "",
                    device.v.val_str, baud.v.val_int, data.v.val_int,
                    parity.v.val_int, stop.v.val_int);
    } else {
        if (mode.v.val_int == 1) {
            param.type                           = NEU_CONN_TCP_SERVER;
            param.params.tcp_server.ip           = host.v.val_str;
            param.params.tcp_server.port         = port.v.val_int;
            param.params.tcp_server.start_listen = modbus_tcp_server_listen;
            param.params.tcp_server.stop_listen  = modbus_tcp_server_stop;
            param.params.tcp_server.timeout      = timeout.v.val_int;
            param.params.tcp_server.max_link     = 1;
            plugin->is_server                    = true;
        }
        if (mode.v.val_int == 0) {
            param.type                      = NEU_CONN_TCP_CLIENT;
            param.params.tcp_client.ip      = host.v.val_str;
            param.params.tcp_client.port    = port.v.val_int;
            param.params.tcp_client.timeout = timeout.v.val_int;
            plugin->is_server               = false;
        }

        plog_notice(plugin,
                    "config: host: %s, port: %" PRId64 ", mode: %" PRId64 "",
                    host.v.val_str, port.v.val_int, mode.v.val_int);
    }

    if (plugin->conn != NULL) {
        plugin->conn = neu_conn_reconfig(plugin->conn, &param);
    } else {
        plugin->common.link_state = NEU_NODE_LINK_STATE_DISCONNECTED;
        plugin->conn =
            neu_conn_new(&param, (void *) plugin, modbus_conn_connected,
                         modbus_conn_disconnected);
    }

    if (link.v.val_int == 0) {
        free(device.v.val_str);
    } else {
        free(host.v.val_str);
    }

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
        plog_notice(plugin,
                    "validate tag success, name: %s, address: %s, type: %d, "
                    "slave id: "
                    "%d, start address: %d, n register: %d, area: %s",
                    tag->name, tag->address, tag->type, point.slave_id,
                    point.start_address, point.n_register,
                    modbus_area_to_str(point.area));
    } else {
        plog_error(plugin,
                   "validate tag error, name: %s, address: %s, type: %d, "
                   "slave id: "
                   "%d, start address: %d, n register: %d, area: %s",
                   tag->name, tag->address, tag->type, point.slave_id,
                   point.start_address, point.n_register,
                   modbus_area_to_str(point.area));
    }

    return ret;
}

static int driver_group_timer(neu_plugin_t *plugin, neu_plugin_group_t *group)
{
    return modbus_group_timer(plugin, group, 0xfa);
}

static int driver_write(neu_plugin_t *plugin, void *req, neu_datatag_t *tag,
                        neu_value_u value)
{
    return modbus_write_tag(plugin, req, tag, value);
}

static int driver_write_tags(neu_plugin_t *plugin, void *req, UT_array *tags)
{
    return modbus_write_tags(plugin, req, tags);
}