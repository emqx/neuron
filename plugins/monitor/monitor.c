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

/**
 * monitor.c - 监控插件主实现文件
 *
 * 该文件实现了Neuron系统的监控插件，主要功能包括：
 * 1. 通过MQTT协议发送系统监控数据
 * 2. 定时发送心跳消息监测节点状态
 * 3. 处理各类系统事件并发送相应通知
 * 4. 提供系统状态监控和告警功能
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "errcodes.h"
#include "event/event.h"
#include "monitor.h"
#include "mqtt_handle.h"
#include "utils/http_handler.h"
#include "utils/log.h"

extern const neu_plugin_module_t neu_plugin_module;

/**
 * 监控插件全局实例指针
 * 用于在其他组件中访问监控插件实例
 */
static struct neu_plugin *g_monitor_plugin_;

/**
 * MQTT连接成功回调函数
 *
 * @param data 插件实例指针
 */
static void connect_cb(void *data)
{
    neu_plugin_t *plugin      = data;
    plugin->common.link_state = NEU_NODE_LINK_STATE_CONNECTED;
    plog_notice(plugin, "plugin `%s` connected", neu_plugin_module.module_name);
}

/**
 * MQTT连接断开回调函数
 *
 * @param data 插件实例指针
 */
static void disconnect_cb(void *data)
{
    neu_plugin_t *plugin      = data;
    plugin->common.link_state = NEU_NODE_LINK_STATE_DISCONNECTED;
    plog_notice(plugin, "plugin `%s` disconnected",
                neu_plugin_module.module_name);
}

/**
 * 配置MQTT客户端参数
 *
 * @param plugin 监控插件实例
 * @param client MQTT客户端实例
 * @param config 监控配置信息
 * @return 0成功，-1失败
 */
static int config_mqtt_client(neu_plugin_t *plugin, neu_mqtt_client_t *client,
                              const monitor_config_t *config)
{
    int rv = 0;

    if (NULL == client) {
        return 0;
    }

    // 尽早设置日志类别以便于调试
    rv = neu_mqtt_client_set_zlog_category(client, plugin->common.log);
    if (0 != rv) {
        plog_error(plugin, "neu_mqtt_client_set_zlog_category fail");
        return -1;
    }

    // 设置MQTT服务器地址和端口
    rv = neu_mqtt_client_set_addr(client, config->host, config->port);
    if (0 != rv) {
        plog_error(plugin, "neu_mqtt_client_set_host fail");
        return -1;
    }

    // 设置MQTT客户端ID
    rv = neu_mqtt_client_set_id(client, config->client_id);
    if (0 != rv) {
        plog_error(plugin, "neu_mqtt_client_set_id fail");
        return -1;
    }

    // 设置连接成功回调函数
    rv = neu_mqtt_client_set_connect_cb(client, connect_cb, plugin);
    if (0 != rv) {
        plog_error(plugin, "neu_mqtt_client_set_connect_cb fail");
        return -1;
    }

    // 设置连接断开回调函数
    rv = neu_mqtt_client_set_disconnect_cb(client, disconnect_cb, plugin);
    if (0 != rv) {
        plog_error(plugin, "neu_mqtt_client_set_disconnect_cb fail");
        return -1;
    }

    // 如果配置了用户名，设置认证信息
    if (NULL != config->username) {
        rv = neu_mqtt_client_set_user(client, config->username,
                                      config->password);
        if (0 != rv) {
            plog_error(plugin, "neu_mqtt_client_set_user fail");
        }
    }

    // 设置TLS/SSL加密参数
    rv = neu_mqtt_client_set_tls(client, config->ssl, config->ca, config->cert,
                                 config->key, config->keypass);
    if (0 != rv) {
        plog_error(plugin, "neu_mqtt_client_set_tsl fail");
        return -1;
    }

    return rv;
}

/**
 * 心跳定时器回调函数
 *
 * 定时请求获取所有节点状态信息，用于监控节点健康状况
 *
 * @param data 插件实例指针
 * @return 操作结果
 */
static int heartbeat_timer_cb(void *data)
{
    neu_plugin_t *plugin = data;

    neu_reqresp_head_t header = {
        .type = NEU_REQ_GET_NODES_STATE, // 请求获取所有节点状态
        .ctx  = plugin,
    };
    neu_req_get_nodes_state_t cmd = {};

    return neu_plugin_op(plugin, header, &cmd);
}

/**
 * 停止心跳定时器
 *
 * @param plugin 监控插件实例
 */
static inline void stop_heartbeart_timer(neu_plugin_t *plugin)
{
    if (plugin->heartbeat_timer) {
        neu_event_del_timer(plugin->events, plugin->heartbeat_timer);
        plugin->heartbeat_timer = NULL;
        plog_notice(plugin, "heartbeat timer stopped");
    }
}

/**
 * 启动心跳定时器
 *
 * 根据配置的时间间隔，定时发送获取节点状态请求
 *
 * @param plugin 监控插件实例
 * @param interval 心跳间隔（秒）
 * @return 0成功，非零错误码
 */
static int start_hearbeat_timer(neu_plugin_t *plugin, uint64_t interval)
{
    neu_event_timer_t *timer = NULL;

    // 如果间隔为0，表示禁用心跳
    if (0 == interval) {
        plog_info(plugin, "heartbeat disabled");
        goto end;
    }

    // 如果事件循环不存在，创建一个
    if (NULL == plugin->events) {
        plugin->events = neu_event_new();
        if (NULL == plugin->events) {
            plog_error(plugin, "neu_event_new fail");
            return NEU_ERR_EINTERNAL;
        }
    }

    // 配置定时器参数
    neu_event_timer_param_t param = {
        .second      = interval,
        .millisecond = 0,
        .cb          = heartbeat_timer_cb,
        .usr_data    = plugin,
    };

    // 添加定时器
    timer = neu_event_add_timer(plugin->events, param);
    if (NULL == timer) {
        plog_error(plugin, "neu_event_add_timer fail");
        return NEU_ERR_EINTERNAL;
    }

end:
    // 如果已存在定时器，先删除旧的
    if (plugin->heartbeat_timer) {
        neu_event_del_timer(plugin->events, plugin->heartbeat_timer);
    }
    plugin->heartbeat_timer = timer;

    plog_notice(plugin, "start_hearbeat_timer interval: %" PRIu64, interval);

    return 0;
}

/**
 * 清理定时器和MQTT客户端资源
 *
 * @param plugin 监控插件实例
 */
static void uninit_timers_and_mqtt(neu_plugin_t *plugin)
{
    // 停止心跳定时器
    stop_heartbeart_timer(plugin);

    // 释放事件循环
    if (NULL != plugin->events) {
        neu_event_close(plugin->events);
        plugin->events = NULL;
    }

    // 释放配置
    if (NULL != plugin->config) {
        monitor_config_fini(plugin->config);
        free(plugin->config);
        plugin->config = NULL;
    }

    // 释放MQTT客户端
    if (NULL != plugin->mqtt_client) {
        neu_mqtt_client_close(plugin->mqtt_client);
        neu_mqtt_client_free(plugin->mqtt_client);
        plugin->mqtt_client = NULL;
    }
}

/**
 * 监控插件打开函数
 *
 * 创建并初始化插件实例
 *
 * @return 创建的插件实例指针，失败返回NULL
 */
static neu_plugin_t *monitor_plugin_open(void)
{
    neu_plugin_t *plugin = calloc(1, sizeof(neu_plugin_t));
    if (NULL == plugin) {
        return NULL;
    }

    // 初始化插件通用部分
    neu_plugin_common_init(&plugin->common);

    // 设置全局插件实例指针
    g_monitor_plugin_ = plugin;
    return plugin;
}

/**
 * 监控插件关闭函数
 *
 * 释放插件资源
 *
 * @param plugin 监控插件实例
 * @return 操作结果
 */
static int monitor_plugin_close(neu_plugin_t *plugin)
{
    int rv = 0;

    plog_notice(plugin, "Success to free plugin: %s",
                neu_plugin_module.module_name);
    free(plugin);

    return rv;
}

/**
 * 监控插件初始化函数
 *
 * @param plugin 监控插件实例
 * @param load 是否为加载时初始化
 * @return 操作结果
 */
static int monitor_plugin_init(neu_plugin_t *plugin, bool load)
{
    int rv = 0;
    (void) plugin;
    (void) load;

    plog_notice(plugin, "Initialize plugin: %s", neu_plugin_module.module_name);
    return rv;
}

/**
 * 监控插件反初始化函数
 *
 * 清理插件资源
 *
 * @param plugin 监控插件实例
 * @return 操作结果
 */
static int monitor_plugin_uninit(neu_plugin_t *plugin)
{
    int rv = 0;

    // 清理定时器和MQTT客户端资源
    uninit_timers_and_mqtt(plugin);

    plog_notice(plugin, "Uninitialize plugin: %s",
                neu_plugin_module.module_name);
    return rv;
}

/**
 * 监控插件配置函数
 *
 * 解析并应用插件设置
 *
 * @param plugin 监控插件实例
 * @param setting JSON格式的配置字符串
 * @return 操作结果，0成功，非零错误码
 */
static int monitor_plugin_config(neu_plugin_t *plugin, const char *setting)
{
    int              rv          = 0;
    const char      *plugin_name = neu_plugin_module.module_name;
    monitor_config_t config      = { 0 };

    // 解析配置
    rv = monitor_config_parse(plugin, setting, &config);
    if (0 != rv) {
        plog_error(plugin, "monitor_config_parse fail");
        return NEU_ERR_NODE_SETTING_INVALID;
    }

    // 如果配置不存在，分配内存
    if (NULL == plugin->config) {
        plugin->config = calloc(1, sizeof(*plugin->config));
        if (NULL == plugin->config) {
            plog_error(plugin, "calloc monitor config fail");
            rv = NEU_ERR_EINTERNAL;
            goto error;
        }
    }

    // 如果MQTT客户端不存在，创建一个
    if (NULL == plugin->mqtt_client) {
        plugin->mqtt_client = neu_mqtt_client_new(NEU_MQTT_VERSION_V311);
        if (NULL == plugin->mqtt_client) {
            plog_error(plugin, "neu_mqtt_client_new fail");
            rv = NEU_ERR_EINTERNAL;
            goto error;
        }
    } else if (neu_mqtt_client_is_open(plugin->mqtt_client)) {
        // 如果MQTT客户端已经打开，先关闭它
        rv = neu_mqtt_client_close(plugin->mqtt_client);
        if (0 != rv) {
            plog_error(plugin, "neu_mqtt_client_close fail");
            rv = NEU_ERR_EINTERNAL;
            goto error;
        }
    }

    // 配置MQTT客户端
    rv = config_mqtt_client(plugin, plugin->mqtt_client, &config);
    if (0 != rv) {
        rv = NEU_ERR_MQTT_INIT_FAILURE;
        goto error;
    }

    // 如果插件已启动，打开MQTT连接
    if (plugin->started && 0 != neu_mqtt_client_open(plugin->mqtt_client)) {
        plog_error(plugin, "neu_mqtt_client_open fail");
        rv = NEU_ERR_MQTT_CONNECT_FAILURE;
        goto error;
    }

    // 如果插件已启动，启动心跳定时器
    if (plugin->started &&
        0 != start_hearbeat_timer(plugin, config.heartbeat_interval)) {
        plog_error(plugin, "start_hearbeat_timer fail");
        rv = NEU_ERR_EINTERNAL;
        goto error;
    }

    // 更新配置
    monitor_config_fini(plugin->config);
    memmove(plugin->config, &config, sizeof(config));
    // 注意：config已移动，不要调用monitor_config_fini

    plog_notice(plugin, "config plugin `%s` success", plugin_name);
    return 0;

error:
    plog_error(plugin, "config plugin `%s` fail", plugin_name);
    monitor_config_fini(&config);
    return rv;
}

/**
 * 监控插件请求处理函数
 *
 * 处理各类系统消息和事件
 *
 * @param plugin 监控插件实例
 * @param header 消息头
 * @param data 消息数据
 * @return 操作结果
 */
static int monitor_plugin_request(neu_plugin_t       *plugin,
                                  neu_reqresp_head_t *header, void *data)
{
    (void) plugin;

    switch (header->type) {
    case NEU_RESP_ERROR: {
        // 处理错误响应
        neu_resp_error_t *resp_err = (neu_resp_error_t *) data;
        nlog_warn("recv error code: %d", resp_err->error);
        break;
    }
    case NEU_RESP_GET_NODES_STATE: {
        // 处理节点状态响应
        handle_nodes_state(header->ctx, (neu_resp_get_nodes_state_t *) data);
        break;
    }
    // 处理各类系统事件
    case NEU_REQ_ADD_NODE_EVENT:     // 添加节点事件
    case NEU_REQ_DEL_NODE_EVENT:     // 删除节点事件
    case NEU_REQ_NODE_CTL_EVENT:     // 节点控制事件
    case NEU_REQ_NODE_SETTING_EVENT: // 节点设置事件
    case NEU_REQ_ADD_GROUP_EVENT:    // 添加组事件
    case NEU_REQ_DEL_GROUP_EVENT:    // 删除组事件
    case NEU_REQ_UPDATE_GROUP_EVENT: // 更新组事件
    case NEU_REQ_ADD_TAG_EVENT:      // 添加标签事件
    case NEU_REQ_DEL_TAG_EVENT:      // 删除标签事件
    case NEU_REQ_UPDATE_TAG_EVENT: { // 更新标签事件
        handle_events(plugin, header->type, data);
        break;
    }
    default:
        nlog_warn("recv unsupported msg: %s",
                  neu_reqresp_type_string(header->type));
        break;
    }
    return 0;
}

/**
 * 监控插件启动函数
 *
 * 打开MQTT连接并启动心跳定时器
 *
 * @param plugin 监控插件实例
 * @return 操作结果
 */
static int monitor_plugin_start(neu_plugin_t *plugin)
{
    int         rv          = 0;
    const char *plugin_name = neu_plugin_module.module_name;

    // 检查MQTT客户端是否存在
    if (NULL == plugin->mqtt_client) {
        plog_notice(plugin, "mqtt client is NULL");
        goto end;
    }

    // 打开MQTT连接
    if (0 != neu_mqtt_client_open(plugin->mqtt_client)) {
        plog_error(plugin, "neu_mqtt_client_open fail");
        rv = NEU_ERR_MQTT_CONNECT_FAILURE;
        goto end;
    }

    // 启动心跳定时器
    if (0 != start_hearbeat_timer(plugin, plugin->config->heartbeat_interval)) {
        plog_error(plugin, "start_hearbeat_timer fail");
        neu_mqtt_client_close(plugin->mqtt_client);
        rv = NEU_ERR_EINTERNAL;
        goto end;
    }

end:
    if (0 == rv) {
        plugin->started = true;
        plog_notice(plugin, "start plugin `%s` success", plugin_name);
    } else {
        plog_error(plugin, "start plugin `%s` failed, error %d", plugin_name,
                   rv);
    }

    return 0;
}

/**
 * 监控插件停止函数
 *
 * 关闭MQTT连接并停止心跳定时器
 *
 * @param plugin 监控插件实例
 * @return 操作结果
 */
static int monitor_plugin_stop(neu_plugin_t *plugin)
{
    // 关闭MQTT连接
    if (plugin->mqtt_client) {
        neu_mqtt_client_close(plugin->mqtt_client);
        plog_notice(plugin, "mqtt client closed");
    }

    // 停止心跳定时器
    stop_heartbeart_timer(plugin);
    plugin->started = false;
    plog_notice(plugin, "stop plugin `%s` success",
                neu_plugin_module.module_name);
    return 0;
}

/**
 * 监控插件接口函数表
 */
static const neu_plugin_intf_funs_t plugin_intf_funs = {
    .open    = monitor_plugin_open,    // 打开插件
    .close   = monitor_plugin_close,   // 关闭插件
    .init    = monitor_plugin_init,    // 初始化插件
    .uninit  = monitor_plugin_uninit,  // 反初始化插件
    .start   = monitor_plugin_start,   // 启动插件
    .stop    = monitor_plugin_stop,    // 停止插件
    .setting = monitor_plugin_config,  // 配置插件
    .request = monitor_plugin_request, // 处理请求
};

#define DEFAULT_MONITOR_PLUGIN_DESCR \
    "Builtin plugin for Neuron monitoring and alerting"
#define DEFAULT_MONITOR_PLUGIN_DESCR_ZH "内置监控与告警插件"

/**
 * 监控插件模块定义
 */
const neu_plugin_module_t neu_plugin_module = {
    .version         = NEURON_PLUGIN_VER_1_0,           // 插件版本
    .schema          = "monitor",                       // 插件模式
    .module_name     = "Monitor",                       // 模块名称
    .module_descr    = DEFAULT_MONITOR_PLUGIN_DESCR,    // 模块描述（英文）
    .module_descr_zh = DEFAULT_MONITOR_PLUGIN_DESCR_ZH, // 模块描述（中文）
    .intf_funs       = &plugin_intf_funs,               // 接口函数表
    .kind            = NEU_PLUGIN_KIND_SYSTEM,          // 插件类型：系统插件
    .type            = NEU_NA_TYPE_APP,                 // 应用类型
    .display         = true,                            // 是否显示
    .single          = false,                           // 是否单例
    .single_name     = "monitor",                       // 单例名称
};

/**
 * 获取监控插件实例
 *
 * @return 监控插件实例指针
 */
neu_plugin_t *neu_monitor_get_plugin()
{
    return g_monitor_plugin_;
}
