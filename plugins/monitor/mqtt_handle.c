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
 * mqtt_handle.c - 监控插件MQTT处理实现
 *
 * 该文件实现了监控插件通过MQTT协议发送监控数据和系统事件的功能：
 * 1. 处理节点状态变化并通过MQTT发送状态更新
 * 2. 处理系统事件（节点、组、标签的增删改）并通过MQTT发送事件通知
 * 3. 将各类系统信息转换为JSON格式的MQTT消息
 * 4. 管理MQTT消息的主题和发布流程
 */

#include "connection/mqtt_client.h"
#include "errcodes.h"
#include "version.h"
#include "json/neu_json_mqtt.h"
#include "json/neu_json_rw.h"

#include "parser/neu_json_group_config.h"
#include "parser/neu_json_node.h"
#include "parser/neu_json_tag.h"

#include "monitor.h"
#include "mqtt_handle.h"

/**
 * 生成节点状态心跳JSON消息
 *
 * 将节点状态信息转换为JSON格式的MQTT消息
 *
 * @param plugin 监控插件实例
 * @param states 节点状态数组
 * @return 生成的JSON字符串，失败返回NULL
 */
static char *generate_heartbeat_json(neu_plugin_t *plugin, UT_array *states)
{
    (void) plugin;
    char *version = NEURON_VERSION;
    // 创建JSON消息头部，包含版本和时间戳
    neu_json_states_head_t header   = { .version   = version,
                                        .timpstamp = global_timestamp };
    neu_json_states_t      json     = { 0 };
    char                  *json_str = NULL;

    // 分配节点状态数组
    json.n_state = utarray_len(states);
    json.states  = calloc(json.n_state, sizeof(neu_json_node_state_t));
    if (NULL == json.states) {
        return NULL;
    }

    // 遍历所有节点状态并填充JSON结构体
    utarray_foreach(states, neu_nodes_state_t *, state)
    {
        int index                  = utarray_eltidx(states, state);
        json.states[index].node    = state->node;          // 节点名称
        json.states[index].link    = state->state.link;    // 连接状态
        json.states[index].running = state->state.running; // 运行状态
    }

    // 将结构体编码为JSON字符串
    neu_json_encode_with_mqtt(&json, neu_json_encode_states_resp, &header,
                              neu_json_encode_state_header_resp, &json_str);

    // 释放临时分配的内存
    free(json.states);
    return json_str;
}

/**
 * 生成事件JSON消息
 *
 * 根据事件类型生成对应的JSON消息内容和MQTT主题
 *
 * @param plugin 监控插件实例
 * @param event 事件类型
 * @param data 事件数据
 * @param topic_p 返回的MQTT主题
 * @return 生成的JSON字符串，失败返回NULL
 */
static char *generate_event_json(neu_plugin_t *plugin, neu_reqresp_type_e event,
                                 void *data, char **topic_p)
{
    char *json_str = NULL;
    // 联合体用于不同事件类型的JSON编码
    union {
        neu_json_add_node_req_t            add_node;     // 添加节点
        neu_json_del_node_req_t            del_node;     // 删除节点
        neu_json_node_ctl_req_t            node_ctl;     // 节点控制
        neu_json_node_setting_req_t        node_setting; // 节点设置
        neu_json_add_group_config_req_t    add_grp;      // 添加组
        neu_json_del_group_config_req_t    del_grp;      // 删除组
        neu_json_update_group_config_req_t update_grp;   // 更新组
        neu_json_add_tags_req_t            add_tags;     // 添加标签
        neu_json_del_tags_req_t            del_tags;     // 删除标签
    } json_req = {};

    // 根据事件类型处理不同的事件数据
    switch (event) {
    case NEU_REQ_ADD_NODE_EVENT: {
        // 处理添加节点事件
        neu_req_add_node_t *add_node = data;
        json_req.add_node.name       = add_node->node;   // 节点名称
        json_req.add_node.plugin     = add_node->plugin; // 插件名称
        *topic_p = plugin->config->node_add_topic;       // 设置MQTT主题
        // 编码为JSON字符串
        neu_json_encode_by_fn(&json_req, neu_json_encode_add_node_req,
                              &json_str);
        break;
    }
    case NEU_REQ_DEL_NODE_EVENT: {
        neu_req_del_node_t *del_node = data;
        json_req.del_node.name       = del_node->node;
        *topic_p                     = plugin->config->node_del_topic;
        neu_json_encode_by_fn(&json_req, neu_json_encode_del_node_req,
                              &json_str);
        break;
    }
    case NEU_REQ_NODE_CTL_EVENT: {
        neu_req_node_ctl_t *node_ctl = data;
        json_req.node_ctl.node       = node_ctl->node;
        json_req.node_ctl.cmd        = node_ctl->ctl;
        *topic_p                     = plugin->config->node_ctl_topic;
        neu_json_encode_by_fn(&json_req, neu_json_encode_node_ctl_req,
                              &json_str);
        break;
    }
    case NEU_REQ_NODE_SETTING_EVENT: {
        neu_req_node_setting_t *node_setting = data;
        json_req.node_setting.node           = node_setting->node;
        json_req.node_setting.setting        = node_setting->setting;
        *topic_p = plugin->config->node_setting_topic;
        neu_json_encode_by_fn(&json_req, neu_json_encode_node_setting_req,
                              &json_str);
        break;
    }
    case NEU_REQ_ADD_GROUP_EVENT: {
        neu_req_add_group_t *add_grp = data;
        json_req.add_grp.node        = add_grp->driver;
        json_req.add_grp.group       = add_grp->group;
        json_req.add_grp.interval    = add_grp->interval;
        *topic_p                     = plugin->config->group_add_topic;
        neu_json_encode_by_fn(&json_req, neu_json_encode_add_group_config_req,
                              &json_str);
        break;
    }
    case NEU_REQ_DEL_GROUP_EVENT: {
        neu_req_del_group_t *del_grp = data;
        json_req.del_grp.node        = del_grp->driver;
        json_req.del_grp.group       = del_grp->group;
        *topic_p                     = plugin->config->group_del_topic;
        neu_json_encode_by_fn(&json_req, neu_json_encode_del_group_config_req,
                              &json_str);
        break;
    }
    case NEU_REQ_UPDATE_GROUP_EVENT: {
        neu_req_update_group_t *update_grp = data;
        json_req.update_grp.node           = update_grp->driver;
        json_req.update_grp.group          = update_grp->group;
        json_req.update_grp.new_name       = update_grp->new_name;
        json_req.update_grp.set_interval =
            update_grp->interval >= NEU_GROUP_INTERVAL_LIMIT;
        json_req.update_grp.interval = update_grp->interval;
        *topic_p                     = plugin->config->group_update_topic;
        neu_json_encode_by_fn(
            &json_req, neu_json_encode_update_group_config_req, &json_str);
        break;
    }
    case NEU_REQ_ADD_TAG_EVENT:
    case NEU_REQ_UPDATE_TAG_EVENT: {
        neu_req_add_tag_t *add_tag = data;
        json_req.add_tags.node     = add_tag->driver;
        json_req.add_tags.group    = add_tag->group;
        json_req.add_tags.n_tag    = add_tag->n_tag;
        json_req.add_tags.tags =
            calloc(add_tag->n_tag, sizeof(*json_req.add_tags.tags));
        if (NULL == json_req.add_tags.tags) {
            plog_error(plugin, "calloc fail");
            break;
        }
        for (uint16_t i = 0; i < add_tag->n_tag; ++i) {
            json_req.add_tags.tags[i].type      = add_tag->tags[i].type;
            json_req.add_tags.tags[i].name      = add_tag->tags[i].name;
            json_req.add_tags.tags[i].attribute = add_tag->tags[i].attribute;
            json_req.add_tags.tags[i].address   = add_tag->tags[i].address;
            json_req.add_tags.tags[i].decimal   = add_tag->tags[i].decimal;
            json_req.add_tags.tags[i].precision = add_tag->tags[i].precision;
            json_req.add_tags.tags[i].description =
                add_tag->tags[i].description;
            if (neu_tag_attribute_test(&add_tag->tags[i],
                                       NEU_ATTRIBUTE_STATIC)) {
                neu_tag_get_static_value_json(&add_tag->tags[i],
                                              &json_req.add_tags.tags[i].t,
                                              &json_req.add_tags.tags[i].value);
            } else {
                json_req.add_tags.tags[i].t = NEU_JSON_UNDEFINE;
            }
        }
        *topic_p = NEU_REQ_ADD_TAG_EVENT == event
            ? plugin->config->tag_add_topic
            : plugin->config->tag_update_topic;
        neu_json_encode_by_fn(&json_req, neu_json_encode_add_tags_req,
                              &json_str);

        free(json_req.add_tags.tags);
        break;
    }
    case NEU_REQ_DEL_TAG_EVENT: {
        neu_req_del_tag_t *del_tag = data;
        json_req.del_tags.node     = del_tag->driver;
        json_req.del_tags.group    = del_tag->group;
        json_req.del_tags.n_tags   = del_tag->n_tag;
        json_req.del_tags.tags =
            calloc(del_tag->n_tag, sizeof(*json_req.del_tags.tags));
        if (NULL == json_req.del_tags.tags) {
            plog_error(plugin, "calloc fail");
            break;
        }
        for (uint16_t i = 0; i < del_tag->n_tag; ++i) {
            json_req.del_tags.tags[i] = del_tag->tags[i];
        }
        *topic_p = plugin->config->tag_del_topic;
        neu_json_encode_by_fn(&json_req, neu_json_encode_del_tags_req,
                              &json_str);

        free(json_req.del_tags.tags);
        break;
    }
    default: {
        plog_error(plugin, "unsupported event:%s",
                   neu_reqresp_type_string(event));
        return NULL;
    }
    }

    return json_str;
}

/**
 * MQTT消息发布回调函数
 *
 * 在消息发布完成后更新指标统计
 *
 * @param errcode 错误码，0表示成功
 * @param qos 服务质量等级
 * @param topic 消息主题
 * @param payload 消息内容
 * @param len 消息长度
 * @param data 用户数据（插件实例）
 */
static void publish_cb(int errcode, neu_mqtt_qos_e qos, char *topic,
                       uint8_t *payload, uint32_t len, void *data)
{
    (void) qos;
    (void) topic;
    (void) len;

    neu_plugin_t *plugin = data;

    // 获取指标更新回调函数
    neu_adapter_update_metric_cb_t update_metric =
        plugin->common.adapter_callbacks->update_metric;

    // 更新消息发送统计指标
    if (0 == errcode) {
        // 发送成功，增加成功计数
        update_metric(plugin->common.adapter, NEU_METRIC_SEND_MSGS_TOTAL, 1,
                      NULL);
    } else {
        // 发送失败，增加错误计数
        update_metric(plugin->common.adapter, NEU_METRIC_SEND_MSG_ERRORS_TOTAL,
                      1, NULL);
    }

    // 释放消息内容内存
    free(payload);
}

/**
 * 发布MQTT消息
 *
 * @param plugin 监控插件实例
 * @param qos 服务质量等级
 * @param topic 消息主题
 * @param payload 消息内容
 * @param payload_len 消息长度
 * @return 0成功，非零错误码
 */
static inline int publish(neu_plugin_t *plugin, neu_mqtt_qos_e qos, char *topic,
                          char *payload, size_t payload_len)
{
    // 获取指标更新回调函数
    neu_adapter_update_metric_cb_t update_metric =
        plugin->common.adapter_callbacks->update_metric;

    // 发布MQTT消息
    int rv = neu_mqtt_client_publish(
        plugin->mqtt_client, qos, topic, (uint8_t *) payload,
        (uint32_t) payload_len, plugin, publish_cb);
    if (0 != rv) {
        // 发布失败，记录错误并更新错误计数
        plog_error(plugin, "pub [%s, QoS%d] fail", topic, qos);
        update_metric(plugin->common.adapter, NEU_METRIC_SEND_MSG_ERRORS_TOTAL,
                      1, NULL);
        free(payload);
        rv = NEU_ERR_MQTT_PUBLISH_FAILURE;
    }

    return rv;
}

/**
 * 处理节点状态信息
 *
 * 将节点状态信息转换为JSON格式并通过MQTT发送
 *
 * @param plugin 监控插件实例
 * @param states 节点状态信息
 * @return 0成功，非零错误码
 */
int handle_nodes_state(neu_plugin_t *plugin, neu_reqresp_nodes_state_t *states)
{
    int   rv       = 0;
    char *json_str = NULL;

    // 检查MQTT客户端是否存在
    if (NULL == plugin->mqtt_client) {
        rv = NEU_ERR_MQTT_IS_NULL;
        goto end;
    }

    // 检查MQTT连接状态
    if (!neu_mqtt_client_is_connected(plugin->mqtt_client)) {
        // 缓存被禁用且我们已断开连接
        rv = NEU_ERR_MQTT_FAILURE;
        goto end;
    }

    // 生成节点状态JSON消息
    json_str = generate_heartbeat_json(plugin, states->states);
    if (NULL == json_str) {
        plog_error(plugin, "generate heartbeat json fail");
        rv = NEU_ERR_EINTERNAL;
        goto end;
    }

    char          *topic = plugin->config->heartbeat_topic;
    neu_mqtt_qos_e qos   = NEU_MQTT_QOS0;
    rv       = publish(plugin, qos, topic, json_str, strlen(json_str));
    json_str = NULL;

end:
    utarray_free(states->states);

    return rv;
}

/**
 * 处理系统事件
 *
 * 将系统事件转换为JSON格式并通过MQTT发送
 *
 * @param plugin 监控插件实例
 * @param event 事件类型
 * @param data 事件数据
 * @return 0成功，非零错误码
 */
int handle_events(neu_plugin_t *plugin, neu_reqresp_type_e event, void *data)
{
    int   rv       = 0;
    char *json_str = NULL;
    char *topic    = NULL;

    // 检查MQTT客户端是否存在
    if (NULL == plugin->mqtt_client) {
        rv = NEU_ERR_MQTT_IS_NULL;
        goto end;
    }

    // 检查MQTT连接状态
    if (!neu_mqtt_client_is_connected(plugin->mqtt_client)) {
        // 缓存被禁用且我们已断开连接
        rv = NEU_ERR_MQTT_FAILURE;
        goto end;
    }

    // 生成事件JSON消息和对应的主题
    json_str = generate_event_json(plugin, event, data, &topic);
    if (NULL == json_str) {
        plog_error(plugin, "generate event:%s json fail",
                   neu_reqresp_type_string(event));
        rv = NEU_ERR_EINTERNAL;
        goto end;
    }

    // 发布MQTT消息
    neu_mqtt_qos_e qos = NEU_MQTT_QOS0; // 使用QoS0服务质量
    rv       = publish(plugin, qos, topic, json_str, strlen(json_str));
    json_str = NULL;

end:
    // 根据事件类型清理资源
    if (NEU_REQ_NODE_SETTING_EVENT == event) {
        // 清理节点设置事件资源
        neu_req_node_setting_fini(data);
    } else if (NEU_REQ_ADD_TAG_EVENT == event ||
               NEU_REQ_UPDATE_TAG_EVENT == event) {
        // 清理添加/更新标签事件资源
        neu_req_add_tag_fini(data);
    } else if (NEU_REQ_DEL_TAG_EVENT == event) {
        // 清理删除标签事件资源
        neu_req_del_tag_fini(data);
    }
    return rv;
}
