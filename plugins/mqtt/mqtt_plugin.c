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

#include "utils/asprintf.h"
#include "utils/time.h"

#include "mqtt_config.h"
#include "mqtt_handle.h"
#include "mqtt_plugin.h"

const neu_plugin_module_t neu_plugin_module;

/**
 * @brief MQTT连接成功回调函数
 *
 * 当MQTT客户端成功连接到服务器时调用此函数
 *
 * @param data 插件实例指针
 */
static void connect_cb(void *data)
{
    neu_plugin_t *plugin = data;
    plugin->common.link_state =
        NEU_NODE_LINK_STATE_CONNECTED; // 更新连接状态为已连接
    plog_notice(plugin, "plugin `%s` connected", neu_plugin_module.module_name);
}

/**
 * @brief MQTT断开连接回调函数
 *
 * 当MQTT客户端与服务器断开连接时调用此函数
 *
 * @param data 插件实例指针
 */
static void disconnect_cb(void *data)
{
    neu_plugin_t *plugin = data;
    plugin->common.link_state =
        NEU_NODE_LINK_STATE_DISCONNECTED; // 更新连接状态为已断开
    plog_notice(plugin, "plugin `%s` disconnected",
                neu_plugin_module.module_name);
}

/**
 * @brief 创建并初始化MQTT插件实例
 *
 * 分配和初始化MQTT插件实例所需的内存
 *
 * @return 成功返回插件实例指针，失败返回NULL
 */
static neu_plugin_t *mqtt_plugin_open(void)
{
    neu_plugin_t *plugin = (neu_plugin_t *) calloc(1, sizeof(neu_plugin_t));
    neu_plugin_common_init(&plugin->common); // 初始化插件通用部分
    return plugin;
}

/**
 * @brief 释放MQTT插件实例
 *
 * 释放插件实例占用的资源
 *
 * @param plugin 插件实例指针
 * @return 总是返回NEU_ERR_SUCCESS表示成功
 */
static int mqtt_plugin_close(neu_plugin_t *plugin)
{
    const char *name = neu_plugin_module.module_name;
    plog_notice(plugin, "success to free plugin:%s", name);

    free(plugin);
    return NEU_ERR_SUCCESS;
}

/**
 * @brief 初始化MQTT插件
 *
 * 执行插件初始化操作，注册指标监控等
 *
 * @param plugin 插件实例指针
 * @param load 是否是加载插件时初始化
 * @return 成功返回NEU_ERR_SUCCESS，失败返回错误代码
 */
static int mqtt_plugin_init(neu_plugin_t *plugin, bool load)
{
    (void) load; // 未使用的参数
    plog_notice(plugin, "initialize plugin `%s` success",
                neu_plugin_module.module_name);

    // 注册缓存消息数量指标，用于监控
    neu_adapter_register_metric_cb_t register_metric =
        plugin->common.adapter_callbacks->register_metric;
    register_metric(plugin->common.adapter, NEU_METRIC_CACHED_MSGS_NUM,
                    NEU_METRIC_CACHED_MSGS_NUM_HELP,
                    NEU_METRIC_CACHED_MSGS_NUM_TYPE, 0);
    return NEU_ERR_SUCCESS;
}

/**
 * @brief 反初始化MQTT插件
 *
 * 释放插件使用的资源，关闭MQTT连接，清理配置
 *
 * @param plugin 插件实例指针
 * @return 成功返回NEU_ERR_SUCCESS，失败返回错误代码
 */
static int mqtt_plugin_uninit(neu_plugin_t *plugin)
{
    // 释放配置资源
    mqtt_config_fini(&plugin->config);

    // 关闭并释放MQTT客户端
    if (plugin->client) {
        neu_mqtt_client_close(plugin->client);
        neu_mqtt_client_free(plugin->client);
        plugin->client = NULL;
    }

    // 释放主题字符串
    free(plugin->read_req_topic);
    plugin->read_req_topic = NULL;
    free(plugin->read_resp_topic);
    plugin->read_resp_topic = NULL;

    // 释放路由表
    route_tbl_free(plugin->route_tbl);

    plog_notice(plugin, "uninitialize plugin `%s` success",
                neu_plugin_module.module_name);
    return NEU_ERR_SUCCESS;
}

/**
 * @brief 配置MQTT客户端参数
 *
 * 根据配置信息设置MQTT客户端的连接参数、回调函数等
 *
 * @param plugin 插件实例指针
 * @param client MQTT客户端实例
 * @param config MQTT配置信息
 * @return 成功返回0，失败返回-1
 */
static int config_mqtt_client(neu_plugin_t *plugin, neu_mqtt_client_t *client,
                              const mqtt_config_t *config)
{
    int rv = 0;

    if (NULL == client) {
        return 0;
    }

    // 尽早设置日志类别，便于调试
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

    // 设置断开连接回调函数
    rv = neu_mqtt_client_set_disconnect_cb(client, disconnect_cb, plugin);
    if (0 != rv) {
        plog_error(plugin, "neu_mqtt_client_set_disconnect_cb fail");
        return -1;
    }

    // 设置消息缓存大小（内存和磁盘）
    rv = neu_mqtt_client_set_cache_size(client, config->cache_mem_size,
                                        config->cache_disk_size);
    if (0 != rv) {
        plog_error(plugin, "neu_mqtt_client_set_msg_cache_limit fail");
        return -1;
    }

    // 设置缓存同步间隔
    rv = neu_mqtt_client_set_cache_sync_interval(client,
                                                 config->cache_sync_interval);
    if (0 != rv) {
        plog_error(plugin, "neu_mqtt_client_set_msg_cache_sync_interval fail");
        return -1;
    }

    // 设置用户名和密码（如果提供）
    if (NULL != config->username) {
        rv = neu_mqtt_client_set_user(client, config->username,
                                      config->password);
        if (0 != rv) {
            plog_error(plugin, "neu_mqtt_client_set_user fail");
        }
    }

    // 设置TLS/SSL参数（如果启用）
    rv = neu_mqtt_client_set_tls(client, config->ssl, config->ca, config->cert,
                                 config->key, config->keypass);
    if (0 != rv) {
        plog_error(plugin, "neu_mqtt_client_set_tsl fail");
        return -1;
    }

    return rv;
}

/**
 * @brief 创建MQTT主题
 *
 * 根据插件名称生成标准格式的读请求和读响应主题
 *
 * @param plugin 插件实例指针
 * @return 成功返回0，失败返回-1
 */
static int create_topic(neu_plugin_t *plugin)
{
    if (plugin->read_req_topic) {
        // 主题已创建
        return 0;
    }

    // 创建读请求主题：/neuron/插件名/read/req
    neu_asprintf(&plugin->read_req_topic, "/neuron/%s/read/req",
                 plugin->common.name);
    if (NULL == plugin->read_req_topic) {
        return -1;
    }

    // 创建读响应主题：/neuron/插件名/read/resp
    neu_asprintf(&plugin->read_resp_topic, "/neuron/%s/read/resp",
                 plugin->common.name);
    if (NULL == plugin->read_resp_topic) {
        free(plugin->read_req_topic);
        plugin->read_req_topic = NULL;
        return -1;
    }

    return 0;
}

/**
 * @brief 订阅MQTT主题
 *
 * 订阅读取和写入请求主题，设置相应的处理回调函数
 *
 * @param plugin 插件实例指针
 * @param config MQTT配置信息
 * @return 成功返回0，失败返回错误代码
 */
static int subscribe(neu_plugin_t *plugin, const mqtt_config_t *config)
{
    // 创建主题（如果还未创建）
    if (0 != create_topic(plugin)) {
        plog_error(plugin, "create topics fail");
        return NEU_ERR_EINTERNAL;
    }

    // 订阅读取请求主题
    if (0 !=
        neu_mqtt_client_subscribe(plugin->client, config->qos,
                                  plugin->read_req_topic, plugin,
                                  handle_read_req)) {
        plog_error(plugin, "subscribe [%s] fail", plugin->read_req_topic);
        return NEU_ERR_MQTT_SUBSCRIBE_FAILURE;
    }

    // 订阅写入请求主题
    if (0 !=
        neu_mqtt_client_subscribe(plugin->client, config->qos,
                                  config->write_req_topic, plugin,
                                  handle_write_req)) {
        plog_error(plugin, "subscribe [%s] fail",
                   plugin->config.write_req_topic);
        return NEU_ERR_MQTT_SUBSCRIBE_FAILURE;
    }

    return 0;
}

/**
 * @brief 取消订阅MQTT主题
 *
 * 取消订阅之前订阅的主题
 *
 * @param plugin 插件实例指针
 * @param config MQTT配置信息
 * @return 总是返回0表示成功
 */
static int unsubscribe(neu_plugin_t *plugin, const mqtt_config_t *config)
{
    // 取消订阅读请求和写请求主题
    neu_mqtt_client_unsubscribe(plugin->client, plugin->read_req_topic);
    neu_mqtt_client_unsubscribe(plugin->client, config->write_req_topic);
    neu_msleep(100); // 等待消息处理完成
    return 0;
}

/**
 * @brief 配置MQTT插件
 *
 * 解析设置字符串并应用MQTT配置
 *
 * @param plugin 插件实例指针
 * @param setting JSON格式的配置字符串
 * @return 成功返回0，失败返回错误代码
 */
static int mqtt_plugin_config(neu_plugin_t *plugin, const char *setting)
{
    int           rv          = 0;
    const char   *plugin_name = neu_plugin_module.module_name;
    mqtt_config_t config      = { 0 };
    bool          started     = false;

    // 解析配置字符串
    rv = mqtt_config_parse(plugin, setting, &config);
    if (0 != rv) {
        plog_error(plugin, "neu_mqtt_config_parse fail");
        return NEU_ERR_NODE_SETTING_INVALID;
    }

    if (NULL == plugin->client) {
        // 首次配置，创建MQTT客户端
        plugin->client = neu_mqtt_client_new(NEU_MQTT_VERSION_V311);
        if (NULL == plugin->client) {
            plog_error(plugin, "neu_mqtt_client_new fail");
            rv = NEU_ERR_EINTERNAL;
            goto error;
        }
    } else if (neu_mqtt_client_is_open(plugin->client)) {
        // 客户端已存在且已连接，需要先关闭
        started = true;
        unsubscribe(plugin, &plugin->config);
        rv = neu_mqtt_client_close(plugin->client);
        if (0 != rv) {
            plog_error(plugin, "neu_mqtt_client_close fail");
            rv = NEU_ERR_EINTERNAL;
            goto error;
        }
    }

    // 配置MQTT客户端参数
    rv = config_mqtt_client(plugin, plugin->client, &config);
    if (0 != rv) {
        rv = NEU_ERR_MQTT_INIT_FAILURE;
        goto error;
    }

    if (started) {
        // 如果之前已启动，则重新连接并订阅主题
        if (0 != neu_mqtt_client_open(plugin->client)) {
            plog_error(plugin, "neu_mqtt_client_open fail");
            rv = NEU_ERR_MQTT_CONNECT_FAILURE;
            goto error;
        }
        if (0 != (rv = subscribe(plugin, &config))) {
            goto error;
        }
    }

    if (plugin->config.host) {
        // 已存在旧配置，释放旧配置
        mqtt_config_fini(&plugin->config);
    }
    // 保存新配置
    memmove(&plugin->config, &config, sizeof(config));

    plog_notice(plugin, "config plugin `%s` success", plugin_name);
    return 0;

error:
    plog_error(plugin, "config plugin `%s` fail", plugin_name);
    mqtt_config_fini(&config);
    return rv;
}

/**
 * @brief 启动MQTT插件
 *
 * 连接MQTT服务器并订阅主题
 *
 * @param plugin 插件实例指针
 * @return 成功返回0，失败返回错误代码
 */
static int mqtt_plugin_start(neu_plugin_t *plugin)
{
    int         rv          = 0;
    const char *plugin_name = neu_plugin_module.module_name;

    // 检查客户端是否已初始化
    if (NULL == plugin->client) {
        plog_error(plugin, "mqtt client is NULL due to init failure");
        rv = NEU_ERR_MQTT_IS_NULL;
        goto end;
    }

    // 连接MQTT服务器
    if (0 != neu_mqtt_client_open(plugin->client)) {
        plog_error(plugin, "neu_mqtt_client_open fail");
        rv = NEU_ERR_MQTT_CONNECT_FAILURE;
        goto end;
    }

    // 订阅相关主题
    rv = subscribe(plugin, &plugin->config);

end:
    if (0 == rv) {
        plog_notice(plugin, "start plugin `%s` success", plugin_name);
    } else {
        plog_error(plugin, "start plugin `%s` failed, error %d", plugin_name,
                   rv);
        // 失败时关闭连接
        neu_mqtt_client_close(plugin->client);
    }
    return rv;
}

/**
 * @brief 停止MQTT插件
 *
 * 取消订阅并断开MQTT连接
 *
 * @param plugin 插件实例指针
 * @return 总是返回NEU_ERR_SUCCESS表示成功
 */
static int mqtt_plugin_stop(neu_plugin_t *plugin)
{
    if (plugin->client) {
        // 取消订阅并关闭连接
        unsubscribe(plugin, &plugin->config);
        neu_mqtt_client_close(plugin->client);
        plog_notice(plugin, "mqtt client closed");
    }

    plog_notice(plugin, "stop plugin `%s` success",
                neu_plugin_module.module_name);
    return NEU_ERR_SUCCESS;
}

/**
 * @brief 处理插件收到的请求
 *
 * 根据请求类型分发到不同的处理函数
 *
 * @param plugin 插件实例指针
 * @param head 请求头信息
 * @param data 请求数据
 * @return 处理结果错误码
 */
static int mqtt_plugin_request(neu_plugin_t *plugin, neu_reqresp_head_t *head,
                               void *data)
{
    neu_err_code_e error = NEU_ERR_SUCCESS;

    // 获取指标更新回调函数
    neu_adapter_update_metric_cb_t update_metric =
        plugin->common.adapter_callbacks->update_metric;

    // 每秒更新一次缓存消息数量指标
    if (NULL != plugin->client &&
        (global_timestamp - plugin->cache_metric_update_ts) >= 1000) {
        update_metric(plugin->common.adapter, NEU_METRIC_CACHED_MSGS_NUM,
                      neu_mqtt_client_get_cached_msgs_num(plugin->client),
                      NULL);
        plugin->cache_metric_update_ts = global_timestamp;
    }

    // 根据请求类型分发到不同的处理函数
    switch (head->type) {
    case NEU_RESP_ERROR: // 写操作响应
        error = handle_write_response(plugin, head->ctx, data);
        break;
    case NEU_RESP_READ_GROUP: // 读取组响应
        error = handle_read_response(plugin, head->ctx, data);
        break;
    case NEU_REQRESP_TRANS_DATA: { // 数据传输
        error = handle_trans_data(plugin, data);
        break;
    }
    case NEU_REQ_SUBSCRIBE_GROUP: // 订阅组
        error = handle_subscribe_group(plugin, data);
        break;
    case NEU_REQ_UPDATE_SUBSCRIBE_GROUP: // 更新订阅
        error = handle_update_subscribe(plugin, data);
        break;
    case NEU_REQ_UNSUBSCRIBE_GROUP: // 取消订阅组
        error = handle_unsubscribe_group(plugin, data);
        break;
    case NEU_REQ_UPDATE_GROUP: // 更新组
        error = handle_update_group(plugin, data);
        break;
    case NEU_REQ_DEL_GROUP: // 删除组
        error = handle_del_group(plugin, data);
        break;
    case NEU_REQ_UPDATE_NODE: // 更新节点
        error = handle_update_driver(plugin, data);
        break;
    case NEU_REQRESP_NODE_DELETED: // 节点已删除
        error = handle_del_driver(plugin, data);
        break;
    default: // 未知请求类型
        error = NEU_ERR_MQTT_FAILURE;
        break;
    }

    return error;
}

/**
 * @brief MQTT插件接口函数集合
 *
 * 定义插件的各种操作接口，包括打开、关闭、初始化、启动等
 */
static const neu_plugin_intf_funs_t plugin_intf_funs = {
    .open    = mqtt_plugin_open,    // 创建插件实例
    .close   = mqtt_plugin_close,   // 释放插件实例
    .init    = mqtt_plugin_init,    // 初始化插件
    .uninit  = mqtt_plugin_uninit,  // 反初始化插件
    .start   = mqtt_plugin_start,   // 启动插件
    .stop    = mqtt_plugin_stop,    // 停止插件
    .setting = mqtt_plugin_config,  // 配置插件
    .request = mqtt_plugin_request, // 处理请求
};

#define DESCRIPTION "Northbound MQTT plugin bases on NanoSDK."
#define DESCRIPTION_ZH "基于 NanoSDK 的北向应用 MQTT 插件"

/**
 * @brief MQTT插件模块定义
 *
 * 定义插件的基本信息，包括名称、版本、描述、类型等
 */
const neu_plugin_module_t neu_plugin_module = {
    .version         = NEURON_PLUGIN_VER_1_0,  // 插件版本
    .schema          = "mqtt",                 // 模式标识
    .module_name     = "MQTT",                 // 模块名称
    .module_descr    = DESCRIPTION,            // 英文描述
    .module_descr_zh = DESCRIPTION_ZH,         // 中文描述
    .intf_funs       = &plugin_intf_funs,      // 接口函数集
    .kind            = NEU_PLUGIN_KIND_SYSTEM, // 插件类型（系统插件）
    .type            = NEU_NA_TYPE_APP,        // 插件应用类型（北向应用）
    .display         = true,                   // 是否显示在UI中
    .single          = false,                  // 是否只能创建单个实例
};
