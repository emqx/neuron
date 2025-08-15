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
 * mqtt_handle.c - MQTT插件消息处理实现
 *
 * 该文件实现了MQTT插件的数据处理和消息发布功能，包括：
 * 1. 将标签值转换为不同格式的JSON消息
 * 2. 处理和发布数据点
 * 3. 处理订阅的写请求
 * 4. 管理MQTT消息的发布和应答
 */

#include "connection/mqtt_client.h"
#include "errcodes.h"
#include "utils/asprintf.h"
#include "version.h"
#include "json/neu_json_mqtt.h"
#include "json/neu_json_rw.h"

#include "mqtt_handle.h"
#include "mqtt_plugin.h"

/**
 * 将标签值转换为JSON格式
 *
 * @param tags 标签数组
 * @param json 要填充的JSON结构
 * @return 成功返回0，失败返回非0值
 */
static int tag_values_to_json(UT_array *tags, neu_json_read_resp_t *json)
{
    int index = 0;

    // 如果标签数组为空，直接返回成功
    if (0 == utarray_len(tags)) {
        return 0;
    }

    // 分配JSON标签数组内存
    json->n_tag = utarray_len(tags);
    json->tags  = (neu_json_read_resp_tag_t *) calloc(
        json->n_tag, sizeof(neu_json_read_resp_tag_t));
    if (NULL == json->tags) {
        return -1;
    }

    // 遍历标签数组，将每个标签值转换为JSON格式
    utarray_foreach(tags, neu_resp_tag_value_meta_t *, tag_value)
    {
        neu_tag_value_to_json(tag_value, &json->tags[index]);
        index += 1;
    }

    return 0;
}

/**
 * 生成上传数据的JSON字符串
 *
 * @param plugin 插件实例
 * @param data 要上传的数据
 * @param format 上传格式（值或标签格式）
 * @return 生成的JSON字符串，失败返回NULL
 */
static char *generate_upload_json(neu_plugin_t             *plugin,
                                  neu_reqresp_trans_data_t *data,
                                  mqtt_upload_format_e      format)
{
    char *json_str = NULL;
    // 创建JSON头部，包含组名、节点名和时间戳
    neu_json_read_periodic_t header = { .group     = (char *) data->group,
                                        .node      = (char *) data->driver,
                                        .timestamp = global_timestamp };
    neu_json_read_resp_t     json   = { 0 };

    // 将标签值转换为JSON格式
    if (0 != tag_values_to_json(data->tags, &json)) {
        plog_error(plugin, "tag_values_to_json fail");
        return NULL;
    }

    if (MQTT_UPLOAD_FORMAT_VALUES == format) { // 值格式（简化格式）
        neu_json_encode_with_mqtt(&json, neu_json_encode_read_resp1, &header,
                                  neu_json_encode_read_periodic_resp,
                                  &json_str);
    } else if (MQTT_UPLOAD_FORMAT_TAGS == format) { // 标签格式（完整格式）
        neu_json_encode_with_mqtt(&json, neu_json_encode_read_resp, &header,
                                  neu_json_encode_read_periodic_resp,
                                  &json_str);
    } else {
        plog_warn(plugin, "invalid upload format: %d", format);
    }

    // 释放已分配的内存
    for (int i = 0; i < json.n_tag; i++) {
        if (json.tags[i].n_meta > 0) {
            free(json.tags[i].metas);
        }
    }

    if (json.tags) {
        free(json.tags);
    }
    return json_str;
}

/**
 * 生成读取响应的JSON字符串
 *
 * @param plugin 插件实例
 * @param mqtt MQTT消息结构
 * @param data 读取组数据
 * @return 生成的JSON字符串，失败返回NULL
 */
static char *generate_read_resp_json(neu_plugin_t          *plugin,
                                     neu_json_mqtt_t       *mqtt,
                                     neu_resp_read_group_t *data)
{
    char                *json_str = NULL;
    neu_json_read_resp_t json     = { 0 };

    // 将标签值转换为JSON格式
    if (0 != tag_values_to_json(data->tags, &json)) {
        plog_error(plugin, "tag_values_to_json fail");
        return NULL;
    }

    // 生成包含MQTT消息的JSON字符串
    neu_json_encode_with_mqtt(&json, neu_json_encode_read_resp, mqtt,
                              neu_json_encode_mqtt_resp, &json_str);

    // 释放临时分配的内存
    if (json.tags) {
        free(json.tags);
    }
    return json_str;
}

/**
 * 生成写入响应的JSON字符串
 *
 * @param plugin 插件实例
 * @param mqtt MQTT消息结构
 * @param data 错误数据
 * @return 生成的JSON字符串，失败返回NULL
 */
static char *generate_write_resp_json(neu_plugin_t     *plugin,
                                      neu_json_mqtt_t  *mqtt,
                                      neu_resp_error_t *data)
{
    (void) plugin;

    // 创建错误响应结构
    neu_json_error_resp_t error    = { .error = data->error };
    char                 *json_str = NULL;

    // 生成包含MQTT消息的JSON字符串
    neu_json_encode_with_mqtt(&error, neu_json_encode_error_resp, mqtt,
                              neu_json_encode_mqtt_resp, &json_str);

    return json_str;
}

/**
 * 发送读取请求
 *
 * @param plugin 插件实例
 * @param mqtt MQTT消息结构
 * @param req 读取请求参数
 * @return 操作结果，成功返回0，失败返回非0值
 */
static inline int send_read_req(neu_plugin_t *plugin, neu_json_mqtt_t *mqtt,
                                neu_json_read_req_t *req)
{
    plog_notice(plugin, "read uuid:%s, group:%s, node:%s", mqtt->uuid,
                req->group, req->node);

    // 准备请求头和命令结构
    neu_reqresp_head_t header = { 0 };
    header.ctx                = mqtt;
    header.type               = NEU_REQ_READ_GROUP;
    neu_req_read_group_t cmd  = { 0 };
    // 填充读取组命令结构
    cmd.driver = req->node;  // 驱动节点名
    cmd.group  = req->group; // 组名
    cmd.sync   = req->sync;  // 同步标志
    req->node  = NULL;       // 所有权转移
    req->group = NULL;       // 所有权转移

    // 执行请求操作
    if (0 != neu_plugin_op(plugin, header, &cmd)) {
        neu_req_read_group_fini(&cmd);
        plog_error(plugin, "neu_plugin_op(NEU_REQ_READ_GROUP) fail");
        return -1;
    }

    return 0;
}

/**
 * @brief 将JSON格式的值转换为Neuron内部的标签值格式
 *
 * 根据JSON值的类型，将其转换为对应的neu_dvalue_t类型，支持整数、字符串、
 * 浮点数、布尔值和字节数组等多种数据类型
 *
 * @param req JSON值指针
 * @param t JSON值的类型
 * @param value 目标neu_dvalue_t结构体指针
 * @return 成功返回0，失败返回-1
 */
static int json_value_to_tag_value(union neu_json_value *req,
                                   enum neu_json_type t, neu_dvalue_t *value)
{
    switch (t) {
    case NEU_JSON_INT: // 整数类型
        value->type      = NEU_TYPE_INT64;
        value->value.u64 = req->val_int;
        break;
    case NEU_JSON_STR: // 字符串类型
        value->type = NEU_TYPE_STRING;
        strncpy(value->value.str, req->val_str, sizeof(value->value.str));
        break;
    case NEU_JSON_DOUBLE: // 双精度浮点类型
        value->type      = NEU_TYPE_DOUBLE;
        value->value.d64 = req->val_double;
        break;
    case NEU_JSON_BOOL: // 布尔类型
        value->type          = NEU_TYPE_BOOL;
        value->value.boolean = req->val_bool;
        break;
    case NEU_JSON_BYTES: // 字节数组类型
        value->type               = NEU_TYPE_BYTES;
        value->value.bytes.length = req->val_bytes.length;
        memcpy(value->value.bytes.bytes, req->val_bytes.bytes,
               req->val_bytes.length);
        break;
    default:
        return -1;
    }
    return 0;
}

/**
 * @brief 发送写入单个标签值的请求
 *
 * 通过MQTT插件将收到的写入请求转发给相应的驱动程序
 *
 * @param plugin 插件实例
 * @param mqtt MQTT上下文信息
 * @param req 写入请求参数
 * @return 成功返回0，失败返回-1
 */
static int send_write_tag_req(neu_plugin_t *plugin, neu_json_mqtt_t *mqtt,
                              neu_json_write_req_t *req)
{
    plog_notice(plugin, "write tag uuid:%s, group:%s, node:%s", mqtt->uuid,
                req->group, req->node);

    neu_reqresp_head_t  header = { 0 };
    neu_req_write_tag_t cmd    = { 0 };

    header.ctx  = mqtt;              // 设置上下文为MQTT会话信息
    header.type = NEU_REQ_WRITE_TAG; // 请求类型为写入单个标签

    // 配置写入参数
    cmd.driver = req->node;
    cmd.group  = req->group;
    cmd.tag    = req->tag;

    // 将JSON值转换为Neuron内部标签值格式
    if (0 != json_value_to_tag_value(&req->value, req->t, &cmd.value)) {
        plog_error(plugin, "invalid tag value type: %d", req->t);
        return -1;
    }

    // 执行写入操作
    if (0 != neu_plugin_op(plugin, header, &cmd)) {
        plog_error(plugin, "neu_plugin_op(NEU_REQ_WRITE_TAG) fail");
        return -1;
    }

    req->node  = NULL; // ownership moved
    req->group = NULL; // ownership moved
    req->tag   = NULL; // ownership moved
    return 0;
}

/**
 * @brief 发送批量写入多个标签值的请求
 *
 * 通过MQTT插件将收到的批量写入请求转发给相应的驱动程序，处理多个标签值的同时写入
 *
 * @param plugin 插件实例
 * @param mqtt MQTT上下文信息
 * @param req 批量写入请求参数
 * @return 成功返回0，失败返回-1
 */
static int send_write_tags_req(neu_plugin_t *plugin, neu_json_mqtt_t *mqtt,
                               neu_json_write_tags_req_t *req)
{
    plog_notice(plugin, "write tags uuid:%s, group:%s, node:%s", mqtt->uuid,
                req->group, req->node);

    // 验证所有字符串类型的值长度是否合法
    for (int i = 0; i < req->n_tag; i++) {
        if (req->tags[i].t == NEU_JSON_STR) {
            if (strlen(req->tags[i].value.val_str) >= NEU_VALUE_SIZE) {
                return -1;
            }
        }
    }

    // 初始化请求头
    neu_reqresp_head_t header = {
        .ctx  = mqtt,
        .type = NEU_REQ_WRITE_TAGS,
    };

    // 配置批量写入命令
    neu_req_write_tags_t cmd = { 0 };
    cmd.driver               = req->node;
    cmd.group                = req->group;
    cmd.n_tag                = req->n_tag;
    cmd.tags                 = calloc(cmd.n_tag, sizeof(neu_resp_tag_value_t));
    if (NULL == cmd.tags) {
        return -1;
    }

    // 转换每个标签值到内部格式
    for (int i = 0; i < cmd.n_tag; i++) {
        strcpy(cmd.tags[i].tag, req->tags[i].tag);
        if (0 !=
            json_value_to_tag_value(&req->tags[i].value, req->tags[i].t,
                                    &cmd.tags[i].value)) {
            plog_error(plugin, "invalid tag value type: %d", req->tags[i].t);
            free(cmd.tags);
            return -1;
        }
    }

    // 执行批量写入操作
    if (0 != neu_plugin_op(plugin, header, &cmd)) {
        plog_error(plugin, "neu_plugin_op(NEU_REQ_WRITE_TAGS) fail");
        free(cmd.tags);
        return -1;
    }

    // 传递所有权，避免double free
    req->node  = NULL; // 所有权转移
    req->group = NULL; // 所有权转移

    return 0;
}

/**
 * @brief MQTT消息发布回调函数
 *
 * 当消息发布完成后（无论成功或失败）调用此回调函数，用于更新相关指标并清理资源
 *
 * @param errcode 错误码，0表示成功
 * @param qos MQTT服务质量级别
 * @param topic 发布的主题
 * @param payload 消息负载
 * @param len 消息长度
 * @param data 用户数据（插件指针）
 */
static void publish_cb(int errcode, neu_mqtt_qos_e qos, char *topic,
                       uint8_t *payload, uint32_t len, void *data)
{
    (void) qos;   // 未使用的参数
    (void) topic; // 未使用的参数
    (void) len;   // 未使用的参数

    neu_plugin_t *plugin = data;

    // 获取指标更新回调函数
    neu_adapter_update_metric_cb_t update_metric =
        plugin->common.adapter_callbacks->update_metric;

    // 根据发布结果更新相应指标
    if (0 == errcode) {
        // 发布成功，增加成功计数
        update_metric(plugin->common.adapter, NEU_METRIC_SEND_MSGS_TOTAL, 1,
                      NULL);
    } else {
        // 发布失败，增加错误计数
        update_metric(plugin->common.adapter, NEU_METRIC_SEND_MSG_ERRORS_TOTAL,
                      1, NULL);
    }

    // 释放消息负载内存
    free(payload);
}

/**
 * @brief 发布MQTT消息
 *
 * 将消息发布到指定的MQTT主题
 *
 * @param plugin 插件实例
 * @param qos MQTT服务质量级别
 * @param topic 发布的主题
 * @param payload 消息内容
 * @param payload_len 消息长度
 * @return 成功返回0，失败返回-1
 */
static inline int publish(neu_plugin_t *plugin, neu_mqtt_qos_e qos, char *topic,
                          char *payload, size_t payload_len)
{
    // 获取指标更新回调函数
    neu_adapter_update_metric_cb_t update_metric =
        plugin->common.adapter_callbacks->update_metric;

    // 调用MQTT客户端API发布消息
    int rv =
        neu_mqtt_client_publish(plugin->client, qos, topic, (uint8_t *) payload,
                                (uint32_t) payload_len, plugin, publish_cb);
    if (0 != rv) {
        // 发布失败处理
        plog_error(plugin, "pub [%s, QoS%d] fail", topic, qos);
        // 增加错误计数
        update_metric(plugin->common.adapter, NEU_METRIC_SEND_MSG_ERRORS_TOTAL,
                      1, NULL);
        // 清理资源
        free(payload);
        rv = NEU_ERR_MQTT_PUBLISH_FAILURE;
    }

    return rv;
}

/**
 * @brief 处理MQTT写请求消息
 *
 * 解析MQTT接收到的写入请求，根据类型（单个或批量）转发给相应的处理函数
 *
 * @param qos MQTT服务质量级别
 * @param topic 接收消息的主题
 * @param payload 消息负载
 * @param len 消息长度
 * @param data 用户数据（插件指针）
 */
void handle_write_req(neu_mqtt_qos_e qos, const char *topic,
                      const uint8_t *payload, uint32_t len, void *data)
{
    int               rv     = 0;
    neu_plugin_t     *plugin = data;
    neu_json_write_t *req    = NULL;

    (void) qos;   // 未使用的参数
    (void) topic; // 未使用的参数

    // 更新接收消息计数器
    neu_adapter_update_metric_cb_t update_metric =
        plugin->common.adapter_callbacks->update_metric;
    update_metric(plugin->common.adapter, NEU_METRIC_RECV_MSGS_TOTAL, 1, NULL);

    // 为JSON字符串分配内存并复制数据
    char *json_str = malloc(len + 1);
    if (NULL == json_str) {
        return;
    }

    memcpy(json_str, payload, len);
    json_str[len] = '\0';

    // 解析MQTT请求头
    neu_json_mqtt_t *mqtt = NULL;
    rv                    = neu_json_decode_mqtt_req(json_str, &mqtt);
    if (0 != rv) {
        plog_error(plugin, "neu_json_decode_mqtt_req failed");
        free(json_str);
        return;
    }

    // 解析写入请求内容
    rv = neu_json_decode_write(json_str, &req);
    if (0 != rv) {
        plog_error(plugin, "neu_json_decode_write fail");
        neu_json_decode_mqtt_req_free(mqtt);
        free(json_str);
        return;
    }

    // 根据请求类型选择处理函数
    if (req->singular) {
        // 单个标签写入
        rv = send_write_tag_req(plugin, mqtt, &req->single);
    } else {
        // 批量标签写入
        rv = send_write_tags_req(plugin, mqtt, &req->plural);
    }
    if (0 != rv) {
        neu_json_decode_mqtt_req_free(mqtt);
    }

    // 清理资源
    neu_json_decode_write_free(req);
    free(json_str);
}

/**
 * @brief 处理写操作的响应并发送结果
 *
 * 生成写操作响应的JSON数据并发布到MQTT主题
 *
 * @param plugin 插件实例
 * @param mqtt_json MQTT请求上下文
 * @param data 响应错误数据
 * @return 成功返回0，失败返回错误代码
 */
int handle_write_response(neu_plugin_t *plugin, neu_json_mqtt_t *mqtt_json,
                          neu_resp_error_t *data)
{
    int   rv       = 0;
    char *json_str = NULL;

    // 检查MQTT客户端是否可用
    if (NULL == plugin->client) {
        rv = NEU_ERR_MQTT_IS_NULL;
        goto end;
    }

    // 检查MQTT连接状态和缓存配置
    if (0 == plugin->config.cache &&
        !neu_mqtt_client_is_connected(plugin->client)) {
        // 缓存已禁用且MQTT已断开连接，无法处理
        rv = NEU_ERR_MQTT_FAILURE;
        goto end;
    }

    // 生成写操作响应的JSON数据
    json_str = generate_write_resp_json(plugin, mqtt_json, data);
    if (NULL == json_str) {
        plog_error(plugin, "generate write resp json fail, uuid:%s",
                   mqtt_json->uuid);
        rv = NEU_ERR_EINTERNAL;
        goto end;
    }

    // 发布响应到指定主题
    char          *topic = plugin->config.write_resp_topic;
    neu_mqtt_qos_e qos   = plugin->config.qos;
    rv       = publish(plugin, qos, topic, json_str, strlen(json_str));
    json_str = NULL; // 所有权已转移给publish函数

end:
    // 清理资源
    neu_json_decode_mqtt_req_free(mqtt_json);
    return rv;
}

/**
 * @brief 处理MQTT读取请求消息
 *
 * 解析MQTT接收到的读取请求，根据请求内容向驱动发送读取命令
 *
 * @param qos MQTT服务质量级别
 * @param topic 接收消息的主题
 * @param payload 消息负载
 * @param len 消息长度
 * @param data 用户数据（插件指针）
 */
void handle_read_req(neu_mqtt_qos_e qos, const char *topic,
                     const uint8_t *payload, uint32_t len, void *data)
{
    int                  rv     = 0;
    neu_plugin_t        *plugin = data;
    neu_json_read_req_t *req    = NULL;

    (void) qos;   // 未使用的参数
    (void) topic; // 未使用的参数

    // 更新接收消息计数器
    neu_adapter_update_metric_cb_t update_metric =
        plugin->common.adapter_callbacks->update_metric;
    update_metric(plugin->common.adapter, NEU_METRIC_RECV_MSGS_TOTAL, 1, NULL);

    // 为JSON字符串分配内存并复制数据
    char *json_str = malloc(len + 1);
    if (NULL == json_str) {
        return;
    }

    memcpy(json_str, payload, len);
    json_str[len] = '\0';

    neu_json_mqtt_t *mqtt = NULL;
    rv                    = neu_json_decode_mqtt_req(json_str, &mqtt);
    if (0 != rv) {
        plog_error(plugin, "neu_json_decode_mqtt_req failed");
        free(json_str);
        return;
    }

    rv = neu_json_decode_read_req(json_str, &req);
    if (0 != rv) {
        plog_error(plugin, "neu_json_decode_read_req fail");
        neu_json_decode_mqtt_req_free(mqtt);
        free(json_str);
        return;
    }

    rv = send_read_req(plugin, mqtt, req);
    if (0 != rv) {
        neu_json_decode_mqtt_req_free(mqtt);
    }

    neu_json_decode_read_req_free(req);
    free(json_str);
}

int handle_read_response(neu_plugin_t *plugin, neu_json_mqtt_t *mqtt_json,
                         neu_resp_read_group_t *data)
{
    int   rv       = 0;
    char *json_str = NULL;

    if (NULL == plugin->client) {
        rv = NEU_ERR_MQTT_IS_NULL;
        goto end;
    }

    if (0 == plugin->config.cache &&
        !neu_mqtt_client_is_connected(plugin->client)) {
        // cache disable and we are disconnected
        rv = NEU_ERR_MQTT_FAILURE;
        goto end;
    }

    json_str = generate_read_resp_json(plugin, mqtt_json, data);
    if (NULL == json_str) {
        plog_error(plugin, "generate read resp json fail");
        rv = NEU_ERR_EINTERNAL;
        goto end;
    }

    char          *topic = plugin->read_resp_topic;
    neu_mqtt_qos_e qos   = plugin->config.qos;
    rv       = publish(plugin, qos, topic, json_str, strlen(json_str));
    json_str = NULL;

end:
    neu_json_decode_mqtt_req_free(mqtt_json);
    return rv;
}

int handle_trans_data(neu_plugin_t             *plugin,
                      neu_reqresp_trans_data_t *trans_data)
{
    int rv = 0;

    if (NULL == plugin->client) {
        return NEU_ERR_MQTT_IS_NULL;
    }

    if (0 == plugin->config.cache &&
        !neu_mqtt_client_is_connected(plugin->client)) {
        // cache disable and we are disconnected
        return NEU_ERR_MQTT_FAILURE;
    }

    const route_entry_t *route = route_tbl_get(
        &plugin->route_tbl, trans_data->driver, trans_data->group);
    if (NULL == route) {
        plog_error(plugin, "no route for driver:%s group:%s",
                   trans_data->driver, trans_data->group);
        return NEU_ERR_GROUP_NOT_SUBSCRIBE;
    }

    char *json_str =
        generate_upload_json(plugin, trans_data, plugin->config.format);
    if (NULL == json_str) {
        plog_error(plugin, "generate upload json fail");
        return NEU_ERR_EINTERNAL;
    }

    char          *topic = route->topic;
    neu_mqtt_qos_e qos   = plugin->config.qos;
    rv       = publish(plugin, qos, topic, json_str, strlen(json_str));
    json_str = NULL;

    return rv;
}

static inline char *default_upload_topic(neu_req_subscribe_t *info)
{
    char *t = NULL;
    neu_asprintf(&t, "/neuron/%s/%s/%s", info->app, info->driver, info->group);
    return t;
}

int handle_subscribe_group(neu_plugin_t *plugin, neu_req_subscribe_t *sub_info)
{
    int rv = 0;

    neu_json_elem_t topic = { .name = "topic", .t = NEU_JSON_STR };
    if (NULL == sub_info->params) {
        // no parameters, try default topic
        topic.v.val_str = default_upload_topic(sub_info);
        if (NULL == topic.v.val_str) {
            rv = NEU_ERR_EINTERNAL;
            goto end;
        }
    } else if (0 != neu_parse_param(sub_info->params, NULL, 1, &topic)) {
        plog_error(plugin, "parse `%s` for topic fail", sub_info->params);
        rv = NEU_ERR_GROUP_PARAMETER_INVALID;
        goto end;
    }

    rv = route_tbl_add_new(&plugin->route_tbl, sub_info->driver,
                           sub_info->group, topic.v.val_str);
    // topic.v.val_str ownership moved
    if (0 != rv) {
        plog_error(plugin, "route driver:%s group:%s fail, `%s`",
                   sub_info->driver, sub_info->group, sub_info->params);
        goto end;
    }

    plog_notice(plugin, "route driver:%s group:%s to topic:%s",
                sub_info->driver, sub_info->group, topic.v.val_str);

end:
    free(sub_info->params);
    return rv;
}

int handle_update_subscribe(neu_plugin_t *plugin, neu_req_subscribe_t *sub_info)
{
    int rv = 0;

    if (NULL == sub_info->params) {
        rv = NEU_ERR_GROUP_PARAMETER_INVALID;
        goto end;
    }

    neu_json_elem_t topic = { .name = "topic", .t = NEU_JSON_STR };
    if (0 != neu_parse_param(sub_info->params, NULL, 1, &topic)) {
        plog_error(plugin, "parse `%s` for topic fail", sub_info->params);
        rv = NEU_ERR_GROUP_PARAMETER_INVALID;
        goto end;
    }

    rv = route_tbl_update(&plugin->route_tbl, sub_info->driver, sub_info->group,
                          topic.v.val_str);
    // topic.v.val_str ownership moved
    if (0 != rv) {
        plog_error(plugin, "route driver:%s group:%s fail, `%s`",
                   sub_info->driver, sub_info->group, sub_info->params);
        goto end;
    }

    plog_notice(plugin, "route driver:%s group:%s to topic:%s",
                sub_info->driver, sub_info->group, topic.v.val_str);

end:
    free(sub_info->params);
    return rv;
}

int handle_unsubscribe_group(neu_plugin_t          *plugin,
                             neu_req_unsubscribe_t *unsub_info)
{
    route_tbl_del(&plugin->route_tbl, unsub_info->driver, unsub_info->group);
    plog_notice(plugin, "del route driver:%s group:%s", unsub_info->driver,
                unsub_info->group);
    return 0;
}

int handle_del_group(neu_plugin_t *plugin, neu_req_del_group_t *req)
{
    route_tbl_del(&plugin->route_tbl, req->driver, req->group);
    plog_notice(plugin, "del route driver:%s group:%s", req->driver,
                req->group);
    return 0;
}

int handle_update_group(neu_plugin_t *plugin, neu_req_update_group_t *req)
{
    route_tbl_update_group(&plugin->route_tbl, req->driver, req->group,
                           req->new_name);
    plog_notice(plugin, "update route driver:%s group:%s to %s", req->driver,
                req->group, req->new_name);
    return 0;
}

int handle_update_driver(neu_plugin_t *plugin, neu_req_update_node_t *req)
{
    route_tbl_update_driver(&plugin->route_tbl, req->node, req->new_name);
    plog_notice(plugin, "update route driver:%s to %s", req->node,
                req->new_name);
    return 0;
}

int handle_del_driver(neu_plugin_t *plugin, neu_reqresp_node_deleted_t *req)
{
    route_tbl_del_driver(&plugin->route_tbl, req->node);
    plog_notice(plugin, "delete route driver:%s", req->node);
    return 0;
}
