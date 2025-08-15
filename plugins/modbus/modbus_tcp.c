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

/**
 * modbus_tcp.c
 * 本文件实现了 Modbus TCP 协议插件的驱动接口，包括：
 * - 初始化和配置
 * - 启动和停止
 * - 标签校验
 * - 分组定时器处理
 * - 写入操作
 * 支持 TCP 和 UDP 通信方式，使用 Modbus TCP 协议格式。
 */

/**
 * 创建驱动插件
 * @return 插件对象指针
 */
static neu_plugin_t *driver_open(void);

/**
 * 关闭驱动插件
 * @param plugin 插件对象
 * @return 处理结果
 */
static int driver_close(neu_plugin_t *plugin);

/**
 * 初始化驱动插件
 * @param plugin 插件对象
 * @param load 是否加载配置
 * @return 处理结果
 */
static int driver_init(neu_plugin_t *plugin, bool load);

/**
 * 反初始化驱动插件
 * @param plugin 插件对象
 * @return 处理结果
 */
static int driver_uninit(neu_plugin_t *plugin);

/**
 * 启动驱动插件
 * @param plugin 插件对象
 * @return 处理结果
 */
static int driver_start(neu_plugin_t *plugin);

/**
 * 停止驱动插件
 * @param plugin 插件对象
 * @return 处理结果
 */
static int driver_stop(neu_plugin_t *plugin);

/**
 * 配置驱动插件
 * @param plugin 插件对象
 * @param config 配置字符串
 * @return 处理结果
 */
static int driver_config(neu_plugin_t *plugin, const char *config);

/**
 * 处理请求
 * @param plugin 插件对象
 * @param head 请求头
 * @param data 请求数据
 * @return 处理结果
 */
static int driver_request(neu_plugin_t *plugin, neu_reqresp_head_t *head,
                          void *data);

/**
 * 验证标签格式
 * @param tag 标签
 * @return 验证结果
 */
static int driver_tag_validator(const neu_datatag_t *tag);

/**
 * 验证标签
 * @param plugin 插件对象
 * @param tag 标签
 * @return 验证结果
 */
static int driver_validate_tag(neu_plugin_t *plugin, neu_datatag_t *tag);

/**
 * 分组定时器处理
 * @param plugin 插件对象
 * @param group 分组
 * @return 处理结果
 */
static int driver_group_timer(neu_plugin_t *plugin, neu_plugin_group_t *group);

/**
 * 写入单个标签
 * @param plugin 插件对象
 * @param req 请求上下文
 * @param tag 标签
 * @param value 写入值
 * @return 处理结果
 */
static int driver_write(neu_plugin_t *plugin, void *req, neu_datatag_t *tag,
                        neu_value_u value);

/**
 * 批量写入标签
 * @param plugin 插件对象
 * @param req 请求上下文
 * @param tags 标签数组
 * @return 处理结果
 */
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
    .schema      = "modbus-tcp",
    .module_name = "Modbus TCP",
    .module_descr =
        "This plugin is used to connect devices using the modbus TCP protocol "
        "Users can choose to connect as a client or a server. Support TCP and "
        "UDP communication methods.",
    .module_descr_zh =
        "该插件用于连接使用 modbus tcp 协议的设备。"
        "用户可选择作为客户端连接，或是服务端连接,支持 TCP 和 UDP 通信方式。",
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
    char            *err_param = NULL;
    neu_json_elem_t  port      = { .name = "port", .t = NEU_JSON_INT };
    neu_json_elem_t  timeout   = { .name = "timeout", .t = NEU_JSON_INT };
    neu_json_elem_t  host      = { .name      = "host",
                                   .t         = NEU_JSON_STR,
                                   .v.val_str = NULL };
    neu_json_elem_t  interval  = { .name = "interval", .t = NEU_JSON_INT };
    neu_json_elem_t  mode  = { .name = "connection_mode", .t = NEU_JSON_INT };
    neu_conn_param_t param = { 0 };
    neu_json_elem_t  max_retries = { .name = "max_retries", .t = NEU_JSON_INT };
    neu_json_elem_t  retry_interval = { .name = "retry_interval",
                                        .t    = NEU_JSON_INT };
    neu_json_elem_t  endianess_64   = { .name = "endianess_64",
                                        .t    = NEU_JSON_INT };

    ret = neu_parse_param((char *) config, &err_param, 5, &port, &host, &mode,
                          &timeout, &interval);

    if (ret != 0) {
        plog_error(plugin, "config: %s, decode error: %s", config, err_param);
        free(err_param);
        if (host.v.val_str != NULL) {
            free(host.v.val_str);
        }
        return -1;
    }

    if (timeout.v.val_int <= 0) {
        plog_error(plugin, "config: %s, set timeout error: %s", config,
                   err_param);
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
    plugin->interval       = interval.v.val_int;
    plugin->max_retries    = max_retries.v.val_int;
    plugin->retry_interval = retry_interval.v.val_int;
    plugin->endianess_64   = endianess_64.v.val_int;

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