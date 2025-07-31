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

#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#include "utils/log.h"

#include "adapter.h"
#include "adapter_internal.h"
#include "base/msg_internal.h"
#include "driver/driver_internal.h"
#include "errcodes.h"
#include "persist/persist.h"
#include "plugin.h"
#include "storage.h"

/**
 * 适配器消费者线程函数，从消息队列中获取消息并处理
 *
 * @param arg 适配器指针(neu_adapter_t *)
 * @return 线程返回值(void *)
 */
static void *adapter_consumer(void *arg);

/**
 * 适配器传输数据回调函数，处理来自数据传输套接字的消息
 *
 * @param type 事件IO类型
 * @param fd 文件描述符
 * @param usr_data 用户数据(适配器指针)
 * @return 执行结果(int)，0表示成功
 */
static int adapter_trans_data(enum neu_event_io_type type, int fd,
                              void *usr_data);

/**
 * 适配器主循环回调函数，处理来自控制套接字的消息
 *
 * @param type 事件IO类型
 * @param fd 文件描述符
 * @param usr_data 用户数据(适配器指针)
 * @return 执行结果(int)，0表示成功
 */
static int adapter_loop(enum neu_event_io_type type, int fd, void *usr_data);

/**
 * 适配器命令处理函数，向指定目标发送命令消息
 *
 * @param adapter 适配器指针
 * @param header 请求/响应头
 * @param data 消息数据
 * @return 执行结果(int)，0表示成功
 */
static int adapter_command(neu_adapter_t *adapter, neu_reqresp_head_t header,
                           void *data);

/**
 * 适配器响应处理函数，向请求发送者发送响应消息
 *
 * @param adapter 适配器指针
 * @param header 请求/响应头指针
 * @param data 响应数据
 * @return 执行结果(int)，0表示成功
 */
static int adapter_response(neu_adapter_t *adapter, neu_reqresp_head_t *header,
                            void *data);

/**
 * 适配器定向响应函数，向指定地址发送响应消息
 *
 * @param adapter 适配器指针
 * @param header 请求/响应头指针
 * @param data 响应数据
 * @param dst 目标地址
 * @return 执行结果(int)，0表示成功
 */
static int adapter_responseto(neu_adapter_t *     adapter,
                              neu_reqresp_head_t *header, void *data,
                              struct sockaddr_in dst);

/**
 * 注册适配器指标函数
 *
 * @param adapter 适配器指针
 * @param name 指标名称
 * @param help 指标帮助信息
 * @param type 指标类型
 * @param init 初始值
 * @return 执行结果(int)，0表示成功
 */
static int adapter_register_metric(neu_adapter_t *adapter, const char *name,
                                   const char *help, neu_metric_type_e type,
                                   uint64_t init);

/**
 * 更新适配器指标函数
 *
 * @param adapter 适配器指针
 * @param metric_name 指标名称
 * @param n 指标新值
 * @param group 组名(可选)
 * @return 执行结果(int)，0表示成功
 */
static int adapter_update_metric(neu_adapter_t *adapter,
                                 const char *metric_name, uint64_t n,
                                 const char *group);

/**
 * 发送响应消息的内联函数
 *
 * @param adapter 适配器指针
 * @param header 请求/响应头指针
 * @param data 响应数据
 */
inline static void reply(neu_adapter_t *adapter, neu_reqresp_head_t *header,
                         void *data);

/**
 * 向监控模块发送通知的内联函数
 *
 * @param adapter 适配器指针
 * @param event 事件类型
 * @param data 事件数据
 */
inline static void notify_monitor(neu_adapter_t *    adapter,
                                  neu_reqresp_type_e event, void *data);

static const adapter_callbacks_t callback_funs = {
    .command         = adapter_command,
    .response        = adapter_response,
    .responseto      = adapter_responseto,
    .register_metric = adapter_register_metric,
    .update_metric   = adapter_update_metric,
};

static __thread int create_adapter_error = 0;

#define REGISTER_METRIC(adapter, name, init) \
    adapter_register_metric(adapter, name, name##_HELP, name##_TYPE, init);

#define REGISTER_DRIVER_METRICS(adapter)                     \
    REGISTER_METRIC(adapter, NEU_METRIC_LINK_STATE,          \
                    NEU_NODE_LINK_STATE_DISCONNECTED);       \
    REGISTER_METRIC(adapter, NEU_METRIC_RUNNING_STATE,       \
                    NEU_NODE_RUNNING_STATE_INIT);            \
    REGISTER_METRIC(adapter, NEU_METRIC_LAST_RTT_MS,         \
                    NEU_METRIC_LAST_RTT_MS_MAX);             \
    REGISTER_METRIC(adapter, NEU_METRIC_SEND_BYTES, 0);      \
    REGISTER_METRIC(adapter, NEU_METRIC_RECV_BYTES, 0);      \
    REGISTER_METRIC(adapter, NEU_METRIC_TAGS_TOTAL, 0);      \
    REGISTER_METRIC(adapter, NEU_METRIC_TAG_READS_TOTAL, 0); \
    REGISTER_METRIC(adapter, NEU_METRIC_TAG_READ_ERRORS_TOTAL, 0);

#define REGISTER_APP_METRICS(adapter)                              \
    REGISTER_METRIC(adapter, NEU_METRIC_LINK_STATE,                \
                    NEU_NODE_LINK_STATE_DISCONNECTED);             \
    REGISTER_METRIC(adapter, NEU_METRIC_RUNNING_STATE,             \
                    NEU_NODE_RUNNING_STATE_INIT);                  \
    REGISTER_METRIC(adapter, NEU_METRIC_SEND_MSGS_TOTAL, 0);       \
    REGISTER_METRIC(adapter, NEU_METRIC_SEND_MSG_ERRORS_TOTAL, 0); \
    REGISTER_METRIC(adapter, NEU_METRIC_RECV_MSGS_TOTAL, 0);

/**
 * 获取适配器创建过程中的错误码
 *
 * @return 错误码
 */
int neu_adapter_error()
{
    return create_adapter_error;
}

/**
 * 设置适配器创建过程中的错误码
 *
 * @param error 错误码
 */
void neu_adapter_set_error(int error)
{
    create_adapter_error = error;
}

static void *adapter_consumer(void *arg)
{
    neu_adapter_t *adapter = (neu_adapter_t *) arg;
    while (1) {
        neu_msg_t *         msg    = NULL;
        uint32_t            n      = adapter_msg_q_pop(adapter->msg_q, &msg);
        neu_reqresp_head_t *header = neu_msg_get_header(msg);

        nlog_debug("adapter(%s) recv msg from: %s %p, type: %s, %u",
                   adapter->name, header->sender, header->ctx,
                   neu_reqresp_type_string(header->type), n);
        adapter->module->intf_funs->request(
            adapter->plugin, (neu_reqresp_head_t *) header, &header[1]);
        neu_trans_data_free((neu_reqresp_trans_data_t *) &header[1]);
        neu_msg_free(msg);
    }

    return NULL;
}

/**
 * 获取节点对应的日志类别
 *
 * @param node 节点名称
 * @return 日志类别指针
 */
static inline zlog_category_t *get_log_category(const char *node)
{
    char name[NEU_NODE_NAME_LEN] = { 0 };
    // replace path separators
    for (int i = 0; node[i]; ++i) {
        name[i] = ('/' == node[i] || '\\' == node[i]) ? '_' : node[i];
    }
    return zlog_get_category(name);
}

/**
 * 创建适配器
 *
 * @param info 适配器信息
 * @param load 是否加载现有配置
 * @return 适配器指针，失败时返回NULL
 */
neu_adapter_t *neu_adapter_create(neu_adapter_info_t *info, bool load)
{
    int                  rv      = 0;
    int                  init_rv = 0;
    neu_adapter_t *      adapter = NULL;
    neu_event_io_param_t param   = { 0 };

    switch (info->module->type) {
    case NEU_NA_TYPE_DRIVER:
        adapter = (neu_adapter_t *) neu_adapter_driver_create();
        break;
    case NEU_NA_TYPE_NDRIVER:
    case NEU_NA_TYPE_APP:
        adapter = calloc(1, sizeof(neu_adapter_t));
        break;
    }

    adapter->control_fd =
        socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK, IPPROTO_UDP);
    if (adapter->control_fd <= 0) {
        free(adapter);
        return NULL;
    }
    adapter->trans_data_fd =
        socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK, IPPROTO_UDP);
    if (adapter->trans_data_fd <= 0) {
        close(adapter->control_fd);
        free(adapter);
        return NULL;
    }

    adapter->name                    = strdup(info->name);
    adapter->events                  = neu_event_new();
    adapter->state                   = NEU_NODE_RUNNING_STATE_INIT;
    adapter->handle                  = info->handle;
    adapter->cb_funs.command         = callback_funs.command;
    adapter->cb_funs.response        = callback_funs.response;
    adapter->cb_funs.responseto      = callback_funs.responseto;
    adapter->cb_funs.register_metric = callback_funs.register_metric;
    adapter->cb_funs.update_metric   = callback_funs.update_metric;
    adapter->module                  = info->module;
    adapter->timestamp_lev           = 0;
    adapter->trans_data_port         = 0;
    adapter->log_level               = ZLOG_LEVEL_NOTICE;

    struct sockaddr_in remote = {
        .sin_family      = AF_INET,
        .sin_port        = htons(7788),
        .sin_addr.s_addr = inet_addr("127.0.0.1"),
    };
    rv = connect(adapter->control_fd, (struct sockaddr *) &remote,
                 sizeof(struct sockaddr_in));
    assert(rv == 0);

    switch (info->module->type) {
    case NEU_NA_TYPE_DRIVER:
        if (adapter->module->display) {
            REGISTER_DRIVER_METRICS(adapter);
        }
        neu_adapter_driver_init((neu_adapter_driver_t *) adapter);
        break;
    case NEU_NA_TYPE_NDRIVER:
    case NEU_NA_TYPE_APP: {
        adapter->msg_q = adapter_msg_q_new(adapter->name, 1024);
        pthread_create(&adapter->consumer_tid, NULL, adapter_consumer,
                       (void *) adapter);
        while (true) {
            uint16_t           port  = neu_manager_get_port();
            struct sockaddr_in local = {
                .sin_family      = AF_INET,
                .sin_port        = htons(port),
                .sin_addr.s_addr = inet_addr("127.0.0.1"),
            };
            if (bind(adapter->trans_data_fd, (struct sockaddr *) &local,
                     sizeof(struct sockaddr_in)) == 0) {
                adapter->trans_data_port = port;
                break;
            }
        }

        param.usr_data = (void *) adapter;
        param.cb       = adapter_trans_data;
        param.fd       = adapter->trans_data_fd;

        adapter->trans_data_io = neu_event_add_io(adapter->events, param);

        if (adapter->module->display) {
            REGISTER_APP_METRICS(adapter);
        }

        break;
    }
    }

    adapter->plugin = adapter->module->intf_funs->open();
    assert(adapter->plugin != NULL);
    assert(neu_plugin_common_check(adapter->plugin));
    neu_plugin_common_t *common = neu_plugin_to_plugin_common(adapter->plugin);
    common->adapter             = adapter;
    common->adapter_callbacks   = &adapter->cb_funs;
    common->link_state          = NEU_NODE_LINK_STATE_DISCONNECTED;
    common->log                 = get_log_category(adapter->name);
    strcpy(common->name, adapter->name);

    zlog_level_switch(common->log, default_log_level);

    init_rv = adapter->module->intf_funs->init(adapter->plugin, load);

    if (adapter_load_setting(adapter->name, &adapter->setting) == 0) {
        if (adapter->module->intf_funs->setting(adapter->plugin,
                                                adapter->setting) == 0) {
            adapter->state = NEU_NODE_RUNNING_STATE_READY;
        } else {
            free(adapter->setting);
            adapter->setting = NULL;
        }
    }

    if (info->module->type == NEU_NA_TYPE_DRIVER) {
        adapter_load_group_and_tag((neu_adapter_driver_t *) adapter);
    }

    param.fd       = adapter->control_fd;
    param.usr_data = (void *) adapter;
    param.cb       = adapter_loop;

    adapter->control_io = neu_event_add_io(adapter->events, param);

    adapter_storage_state(adapter->name, adapter->state);

    if (init_rv != 0) {
        nlog_warn("Failed to init adapter: %s", adapter->name);
        neu_adapter_set_error(init_rv);

        if (adapter->module->type == NEU_NA_TYPE_DRIVER) {
            neu_adapter_driver_destroy((neu_adapter_driver_t *) adapter);
        } else {
            neu_event_del_io(adapter->events, adapter->trans_data_io);
        }
        neu_event_del_io(adapter->events, adapter->control_io);

        neu_adapter_destroy(adapter);
        return NULL;
    } else {
        neu_adapter_set_error(0);
        return adapter;
    }
}

/**
 * 获取适配器传输数据端口
 *
 * @param adapter 适配器指针
 * @return 端口号
 */
uint16_t neu_adapter_trans_data_port(neu_adapter_t *adapter)
{
    return adapter->trans_data_port;
}

/**
 * 重命名适配器
 *
 * @param adapter 适配器指针
 * @param new_name 新名称
 * @return 执行结果，0表示成功，非0表示错误码
 */
int neu_adapter_rename(neu_adapter_t *adapter, const char *new_name)
{
    char *name     = strdup(new_name);
    char *old_name = strdup(adapter->name);
    if (NULL == name) {
        return NEU_ERR_EINTERNAL;
    }

    zlog_category_t *log = get_log_category(name);
    if (NULL == log) {
        free(name);
        free(old_name);
        return NEU_ERR_EINTERNAL;
    }

    if (NEU_NA_TYPE_DRIVER == adapter->module->type) {
        neu_adapter_driver_stop_group_timer((neu_adapter_driver_t *) adapter);
    }

    // fix metrics
    if (adapter->metrics) {
        neu_metrics_del_node(adapter);
    }
    free(adapter->name);
    adapter->name = name;
    if (adapter->metrics) {
        adapter->metrics->name = name;
        neu_metrics_add_node(adapter);
    }

    // fix log
    neu_plugin_common_t *common = neu_plugin_to_plugin_common(adapter->plugin);
    common->log                 = log;
    strcpy(common->name, adapter->name);
    zlog_level_switch(common->log, default_log_level);

    if (NEU_NA_TYPE_DRIVER == adapter->module->type) {
        neu_adapter_driver_start_group_timer((neu_adapter_driver_t *) adapter);
    }

    remove_logs(old_name);
    free(old_name);

    return 0;
}

/**
 * 初始化适配器并通知管理器
 *
 * @param adapter 适配器指针
 * @param state 初始运行状态
 */
void neu_adapter_init(neu_adapter_t *adapter, neu_node_running_state_e state)
{
    neu_req_node_init_t init = { 0 };
    init.state               = state;
    strcpy(init.node, adapter->name);

    neu_msg_t *msg = neu_msg_new(NEU_REQ_NODE_INIT, NULL, &init);
    if (NULL == msg) {
        nlog_error("failed alloc msg for %s", adapter->name);
        return;
    }
    neu_reqresp_head_t *header = neu_msg_get_header(msg);
    strcpy(header->sender, adapter->name);
    strcpy(header->receiver, "manager");

    int ret = neu_send_msg(adapter->control_fd, msg);
    if (0 != ret) {
        nlog_error("%s failed to send init msg to manager, ret: %d, errno: %d",
                   adapter->name, ret, errno);
        neu_msg_free(msg);
    }
}

/**
 * 获取适配器类型
 *
 * @param adapter 适配器指针
 * @return 适配器类型
 */
neu_node_type_e neu_adapter_get_type(neu_adapter_t *adapter)
{
    return adapter->module->type;
}

/**
 * 获取适配器标签缓存类型
 *
 * @param adapter 适配器指针
 * @return 标签缓存类型
 */
neu_tag_cache_type_e neu_adapter_get_tag_cache_type(neu_adapter_t *adapter)
{
    return adapter->module->cache_type;
}

static int adapter_register_metric(neu_adapter_t *adapter, const char *name,
                                   const char *help, neu_metric_type_e type,
                                   uint64_t init)
{
    if (NULL == adapter->metrics) {
        adapter->metrics =
            neu_node_metrics_new(adapter, adapter->module->type, adapter->name);
        if (NULL == adapter->metrics) {
            return -1;
        }
        neu_metrics_add_node(adapter);
    }

    if (0 >
        neu_node_metrics_add(adapter->metrics, NULL, name, help, type, init)) {
        return -1;
    }

    return 0;
}

static int adapter_update_metric(neu_adapter_t *adapter,
                                 const char *metric_name, uint64_t n,
                                 const char *group)
{
    if (NULL == adapter->metrics) {
        return -1;
    }

    return neu_node_metrics_update(adapter->metrics, group, metric_name, n);
}

static int adapter_command(neu_adapter_t *adapter, neu_reqresp_head_t header,
                           void *data)
{
    int ret = 0;

    neu_msg_t *msg = neu_msg_new(header.type, header.ctx, data);
    if (NULL == msg) {
        return NEU_ERR_EINTERNAL;
    }
    neu_reqresp_head_t *pheader = neu_msg_get_header(msg);

    strcpy(pheader->sender, adapter->name);
    switch (pheader->type) {
    case NEU_REQ_READ_GROUP: {
        neu_req_read_group_t *cmd = (neu_req_read_group_t *) data;
        strcpy(pheader->receiver, cmd->driver);
        break;
    }
    case NEU_REQ_WRITE_TAG: {
        neu_req_write_tag_t *cmd = (neu_req_write_tag_t *) data;
        strcpy(pheader->receiver, cmd->driver);
        break;
    }
    case NEU_REQ_WRITE_TAGS: {
        neu_req_write_tags_t *cmd = (neu_req_write_tags_t *) data;
        strcpy(pheader->receiver, cmd->driver);
        break;
    }
    case NEU_REQ_WRITE_GTAGS: {
        neu_req_write_gtags_t *cmd = (neu_req_write_gtags_t *) data;
        strcpy(pheader->receiver, cmd->driver);
        break;
    }
    case NEU_REQ_DEL_NODE: {
        neu_req_del_node_t *cmd = (neu_req_del_node_t *) data;
        strcpy(pheader->receiver, cmd->node);
        break;
    }
    case NEU_REQ_UPDATE_GROUP:
    case NEU_REQ_GET_GROUP:
    case NEU_REQ_DEL_GROUP:
    case NEU_REQ_ADD_GROUP: {
        neu_req_add_group_t *cmd = (neu_req_add_group_t *) data;
        strcpy(pheader->receiver, cmd->driver);
        break;
    }
    case NEU_REQ_GET_TAG:
    case NEU_REQ_UPDATE_TAG:
    case NEU_REQ_DEL_TAG:
    case NEU_REQ_ADD_TAG: {
        neu_req_add_tag_t *cmd = (neu_req_add_tag_t *) data;
        strcpy(pheader->receiver, cmd->driver);
        break;
    }
    case NEU_REQ_UPDATE_NODE:
    case NEU_REQ_NODE_CTL:
    case NEU_REQ_GET_NODE_STATE:
    case NEU_REQ_GET_NODE_SETTING:
    case NEU_REQ_NODE_SETTING: {
        neu_req_node_setting_t *cmd = (neu_req_node_setting_t *) data;
        strcpy(pheader->receiver, cmd->node);
        break;
    }
    case NEU_REQ_GET_DRIVER_GROUP:
    case NEU_REQ_GET_SUB_DRIVER_TAGS: {
        strcpy(pheader->receiver, "manager");
        break;
    }
    case NEU_REQRESP_NODE_DELETED: {
        neu_reqresp_node_deleted_t *cmd = (neu_reqresp_node_deleted_t *) data;
        strcpy(pheader->receiver, cmd->node);
        break;
    }
    case NEU_REQ_UPDATE_NDRIVER_TAG_PARAM:
    case NEU_REQ_UPDATE_NDRIVER_TAG_INFO:
    case NEU_REQ_GET_NDRIVER_TAGS: {
        neu_req_get_ndriver_tags_t *cmd = (neu_req_get_ndriver_tags_t *) data;
        strcpy(pheader->receiver, cmd->ndriver);
        break;
    }
    case NEU_REQ_UPDATE_LOG_LEVEL: {
        neu_req_update_log_level_t *cmd = (neu_req_update_log_level_t *) data;
        strcpy(pheader->receiver, cmd->node);
        break;
    }
    default:
        break;
    }

    ret = neu_send_msg(adapter->control_fd, msg);
    if (0 != ret) {
        nlog_error(
            "adapter: %s send %d command %s failed, ret: %d-%d, errno: %s(%d)",
            adapter->name, adapter->control_fd,
            neu_reqresp_type_string(pheader->type), ret, pheader->len,
            strerror(errno), errno);
        neu_msg_free(msg);
        return -1;
    } else {
        return 0;
    }
}

static int adapter_response(neu_adapter_t *adapter, neu_reqresp_head_t *header,
                            void *data)
{
    assert(header->type != NEU_REQRESP_TRANS_DATA);
    neu_msg_exchange(header);

    neu_msg_gen(header, data);
    neu_msg_t *msg = (neu_msg_t *) header;
    int        ret = neu_send_msg(adapter->control_fd, msg);
    if (0 != ret) {
        nlog_error("adapter: %s send response %s failed, ret: %d, errno: %d",
                   adapter->name, neu_reqresp_type_string(header->type), ret,
                   errno);
        neu_msg_free(msg);
    }

    return ret;
}

static int adapter_responseto(neu_adapter_t *     adapter,
                              neu_reqresp_head_t *header, void *data,
                              struct sockaddr_in dst)
{
    assert(header->type == NEU_REQRESP_TRANS_DATA);

    neu_msg_t *msg = neu_msg_new(header->type, header->ctx, data);
    if (NULL == msg) {
        return NEU_ERR_EINTERNAL;
    }
    neu_reqresp_head_t *pheader = neu_msg_get_header(msg);
    strcpy(pheader->sender, adapter->name);

    int ret = neu_send_msg_to(adapter->control_fd, &dst, msg);
    if (0 != ret) {
        nlog_error("adapter: %s send responseto %s failed, ret: %d, errno: %d",
                   adapter->name, neu_reqresp_type_string(header->type), ret,
                   errno);
        neu_msg_free(msg);
    }

    return ret;
}

static int adapter_trans_data(enum neu_event_io_type type, int fd,
                              void *usr_data)
{
    neu_adapter_t *adapter = (neu_adapter_t *) usr_data;
    if (type != NEU_EVENT_IO_READ) {
        nlog_warn("adapter: %s recv close, exit loop, fd: %d", adapter->name,
                  fd);
        return 0;
    }

    neu_msg_t *msg = NULL;
    int        rv  = neu_recv_msg(adapter->trans_data_fd, &msg);
    if (0 != rv) {
        nlog_warn("adapter: %s recv trans data failed, ret: %d, errno: %s(%d)",
                  adapter->name, rv, strerror(errno), errno);
        return 0;
    }

    neu_reqresp_head_t *header = neu_msg_get_header(msg);

    nlog_debug("adapter(%s) recv msg from: %s %p, type: %s", adapter->name,
               header->sender, header->ctx,
               neu_reqresp_type_string(header->type));

    if (header->type != NEU_REQRESP_TRANS_DATA &&
        header->type != NEU_RESP_ERROR) {
        nlog_warn("adapter: %s recv msg type error, type: %s", adapter->name,
                  neu_reqresp_type_string(header->type));
        neu_msg_free(msg);
        return 0;
    }

    if (header->type == NEU_REQRESP_TRANS_DATA) {
        if (adapter_msg_q_push(adapter->msg_q, msg) < 0) {
            nlog_warn("adapter: %s trans data msg q is full, drop msg",
                      adapter->name);
            neu_trans_data_free((neu_reqresp_trans_data_t *) &header[1]);
            neu_msg_free(msg);
        }
        return 0;
    }

    adapter->module->intf_funs->request(
        adapter->plugin, (neu_reqresp_head_t *) header, &header[1]);
    neu_msg_free(msg);
    return 0;
}

static int adapter_loop(enum neu_event_io_type type, int fd, void *usr_data)
{
    neu_adapter_t *adapter = (neu_adapter_t *) usr_data;

    if (type != NEU_EVENT_IO_READ) {
        nlog_warn("adapter: %s recv close, exit loop, fd: %d", adapter->name,
                  fd);
        return 0;
    }

    neu_msg_t *msg = NULL;
    int        rv  = neu_recv_msg(adapter->control_fd, &msg);
    if (0 != rv) {
        nlog_warn("adapter: %s recv failed, ret: %d, errno: %s(%d)",
                  adapter->name, rv, strerror(errno), errno);
        return 0;
    }

    neu_reqresp_head_t *header = neu_msg_get_header(msg);

    nlog_info("adapter(%s) recv msg from: %s %p, type: %s", adapter->name,
              header->sender, header->ctx,
              neu_reqresp_type_string(header->type));

    switch (header->type) {
    case NEU_REQ_SUBSCRIBE_GROUP: {
        neu_req_subscribe_t *cmd = (neu_req_subscribe_t *) &header[1];
        if (adapter->module->type == NEU_NA_TYPE_DRIVER) {
            neu_adapter_driver_subscribe((neu_adapter_driver_t *) adapter, cmd);
        } else {
            adapter->module->intf_funs->request(
                adapter->plugin, (neu_reqresp_head_t *) header, &header[1]);
        }
        neu_msg_free(msg);
        break;
    }
    case NEU_REQ_UNSUBSCRIBE_GROUP: {
        neu_req_unsubscribe_t *cmd = (neu_req_unsubscribe_t *) &header[1];
        if (adapter->module->type == NEU_NA_TYPE_DRIVER) {
            neu_adapter_driver_unsubscribe((neu_adapter_driver_t *) adapter,
                                           cmd);
        } else {
            adapter->module->intf_funs->request(
                adapter->plugin, (neu_reqresp_head_t *) header, &header[1]);
        }
        neu_msg_free(msg);
        break;
    }
    case NEU_REQ_UPDATE_SUBSCRIBE_GROUP:
    case NEU_RESP_GET_DRIVER_GROUP:
    case NEU_REQRESP_NODE_DELETED:
    case NEU_RESP_GET_SUB_DRIVER_TAGS:
    case NEU_REQ_UPDATE_NODE:
    case NEU_RESP_GET_NODE_STATE:
    case NEU_RESP_GET_NODES_STATE:
    case NEU_RESP_GET_NODE_SETTING:
    case NEU_REQ_UPDATE_GROUP:
    case NEU_RESP_GET_SUBSCRIBE_GROUP:
    case NEU_RESP_ADD_TAG:
    case NEU_RESP_ADD_TEMPLATE_TAG:
    case NEU_RESP_UPDATE_TAG:
    case NEU_RESP_UPDATE_TEMPLATE_TAG:
    case NEU_RESP_GET_TAG:
    case NEU_RESP_GET_TEMPLATE_TAG:
    case NEU_RESP_GET_NODE:
    case NEU_RESP_GET_PLUGIN:
    case NEU_RESP_GET_TEMPLATE:
    case NEU_RESP_GET_TEMPLATES:
    case NEU_RESP_GET_NDRIVER_MAPS:
    case NEU_RESP_GET_NDRIVER_TAGS:
    case NEU_RESP_GET_GROUP:
    case NEU_RESP_ERROR:
    case NEU_REQRESP_NODES_STATE:
    case NEU_REQ_ADD_NODE_EVENT:
    case NEU_REQ_DEL_NODE_EVENT:
    case NEU_REQ_NODE_CTL_EVENT:
    case NEU_REQ_NODE_SETTING_EVENT:
    case NEU_REQ_ADD_GROUP_EVENT:
    case NEU_REQ_DEL_GROUP_EVENT:
    case NEU_REQ_UPDATE_GROUP_EVENT:
    case NEU_REQ_ADD_TAG_EVENT:
    case NEU_REQ_DEL_TAG_EVENT:
    case NEU_REQ_UPDATE_TAG_EVENT:
        adapter->module->intf_funs->request(
            adapter->plugin, (neu_reqresp_head_t *) header, &header[1]);
        neu_msg_free(msg);
        break;
    case NEU_RESP_READ_GROUP:
        adapter->module->intf_funs->request(
            adapter->plugin, (neu_reqresp_head_t *) header, &header[1]);
        neu_resp_read_free((neu_resp_read_group_t *) &header[1]);
        neu_msg_free(msg);
        break;
    case NEU_REQ_READ_GROUP: {
        neu_resp_error_t error = { 0 };

        if (adapter->module->type == NEU_NA_TYPE_DRIVER) {
            neu_adapter_driver_read_group((neu_adapter_driver_t *) adapter,
                                          header);
        } else {
            neu_req_read_group_fini((neu_req_read_group_t *) &header[1]);
            error.error  = NEU_ERR_GROUP_NOT_ALLOW;
            header->type = NEU_RESP_ERROR;
            neu_msg_exchange(header);
            reply(adapter, header, &error);
        }

        break;
    }
    case NEU_REQ_WRITE_TAG: {
        neu_resp_error_t error = { 0 };

        if (adapter->module->type == NEU_NA_TYPE_DRIVER) {
            neu_adapter_driver_write_tag((neu_adapter_driver_t *) adapter,
                                         header);
        } else {
            neu_req_write_tag_fini((neu_req_write_tag_t *) &header[1]);
            error.error  = NEU_ERR_GROUP_NOT_ALLOW;
            header->type = NEU_RESP_ERROR;
            neu_msg_exchange(header);
            reply(adapter, header, &error);
        }

        break;
    }
    case NEU_REQ_WRITE_TAGS: {
        neu_resp_error_t error = { 0 };

        if (adapter->module->type != NEU_NA_TYPE_DRIVER) {
            neu_req_write_tags_fini((neu_req_write_tags_t *) &header[1]);
            error.error  = NEU_ERR_GROUP_NOT_ALLOW;
            header->type = NEU_RESP_ERROR;
            neu_msg_exchange(header);
            reply(adapter, header, &error);
        } else {
            neu_adapter_driver_write_tags((neu_adapter_driver_t *) adapter,
                                          header);
        }
        break;
    }
    case NEU_REQ_WRITE_GTAGS: {
        neu_resp_error_t error = { 0 };

        if (adapter->module->type != NEU_NA_TYPE_DRIVER) {
            neu_req_write_gtags_fini((neu_req_write_gtags_t *) &header[1]);
            error.error  = NEU_ERR_GROUP_NOT_ALLOW;
            header->type = NEU_RESP_ERROR;
            neu_msg_exchange(header);
            reply(adapter, header, &error);
        } else {
            neu_adapter_driver_write_gtags((neu_adapter_driver_t *) adapter,
                                           header);
        }
        break;
    }
    case NEU_REQ_NODE_SETTING: {
        neu_req_node_setting_t *cmd   = (neu_req_node_setting_t *) &header[1];
        neu_resp_error_t        error = { 0 };

        error.error = neu_adapter_set_setting(adapter, cmd->setting);
        if (error.error == NEU_ERR_SUCCESS) {
            adapter_storage_setting(adapter->name, cmd->setting);
            // ownership of `cmd->setting` transfer
            notify_monitor(adapter, NEU_REQ_NODE_SETTING_EVENT, cmd);
        } else {
            free(cmd->setting);
        }

        header->type = NEU_RESP_ERROR;
        neu_msg_exchange(header);
        reply(adapter, header, &error);
        break;
    }
    case NEU_REQ_GET_NODE_SETTING: {
        neu_resp_get_node_setting_t resp  = { 0 };
        neu_resp_error_t            error = { 0 };

        neu_msg_exchange(header);
        error.error = neu_adapter_get_setting(adapter, &resp.setting);
        if (error.error != NEU_ERR_SUCCESS) {
            header->type = NEU_RESP_ERROR;
            reply(adapter, header, &error);
        } else {
            header->type = NEU_RESP_GET_NODE_SETTING;
            strcpy(resp.node, adapter->name);
            reply(adapter, header, &resp);
        }
        break;
    }
    case NEU_REQ_GET_NODE_STATE: {
        neu_resp_get_node_state_t *resp =
            (neu_resp_get_node_state_t *) &header[1];

        if (NULL != adapter->metrics) {
            pthread_mutex_lock(&adapter->metrics->lock);
            neu_metric_entry_t *e = NULL;
            HASH_FIND_STR(adapter->metrics->entries, NEU_METRIC_LAST_RTT_MS, e);
            resp->rtt = NULL != e ? e->value : 0;
            pthread_mutex_unlock(&adapter->metrics->lock);
        }
        resp->state  = neu_adapter_get_state(adapter);
        header->type = NEU_RESP_GET_NODE_STATE;
        neu_msg_exchange(header);
        reply(adapter, header, resp);
        break;
    }
    case NEU_REQ_GET_GROUP: {
        neu_msg_exchange(header);

        if (adapter->module->type == NEU_NA_TYPE_DRIVER) {
            neu_resp_get_group_t resp = {
                .groups = neu_adapter_driver_get_group(
                    (neu_adapter_driver_t *) adapter)
            };
            header->type = NEU_RESP_GET_GROUP;
            reply(adapter, header, &resp);
        } else {
            neu_resp_error_t error = { .error = NEU_ERR_GROUP_NOT_ALLOW };

            header->type = NEU_RESP_ERROR;
            reply(adapter, header, &error);
        }
        break;
    }
    case NEU_REQ_GET_TAG: {
        neu_req_get_tag_t *cmd   = (neu_req_get_tag_t *) &header[1];
        neu_resp_error_t   error = { .error = 0 };
        UT_array *         tags  = NULL;

        if (adapter->module->type == NEU_NA_TYPE_DRIVER) {
            error.error = neu_adapter_driver_query_tag(
                (neu_adapter_driver_t *) adapter, cmd->group, cmd->name, &tags);
        } else {
            error.error = NEU_ERR_GROUP_NOT_ALLOW;
        }

        neu_msg_exchange(header);
        if (error.error != NEU_ERR_SUCCESS) {
            header->type = NEU_RESP_ERROR;
            reply(adapter, header, &error);
        } else {
            neu_resp_get_tag_t resp = { .tags = tags };

            header->type = NEU_RESP_GET_TAG;
            reply(adapter, header, &resp);
        }

        break;
    }
    case NEU_REQ_ADD_GROUP: {
        neu_req_add_group_t *cmd   = (neu_req_add_group_t *) &header[1];
        neu_resp_error_t     error = { 0 };

        if (cmd->interval < NEU_GROUP_INTERVAL_LIMIT) {
            error.error = NEU_ERR_GROUP_PARAMETER_INVALID;
        } else {
            if (adapter->module->type == NEU_NA_TYPE_DRIVER) {
                error.error = neu_adapter_driver_add_group(
                    (neu_adapter_driver_t *) adapter, cmd->group,
                    cmd->interval);
            } else {
                error.error = NEU_ERR_GROUP_NOT_ALLOW;
            }
        }

        if (error.error == NEU_ERR_SUCCESS) {
            adapter_storage_add_group(adapter->name, cmd->group, cmd->interval);
            notify_monitor(adapter, NEU_REQ_ADD_GROUP_EVENT, cmd);
        }

        neu_msg_exchange(header);
        header->type = NEU_RESP_ERROR;
        reply(adapter, header, &error);
        break;
    }
    case NEU_REQ_UPDATE_DRIVER_GROUP: {
        neu_req_update_group_t *cmd  = (neu_req_update_group_t *) &header[1];
        neu_resp_update_group_t resp = { 0 };

        if (adapter->module->type == NEU_NA_TYPE_DRIVER) {
            resp.error = neu_adapter_driver_update_group(
                (neu_adapter_driver_t *) adapter, cmd->group, cmd->new_name,
                cmd->interval);
        } else {
            resp.error = NEU_ERR_GROUP_NOT_ALLOW;
        }

        if (resp.error == NEU_ERR_SUCCESS) {
            adapter_storage_update_group(adapter->name, cmd->group,
                                         cmd->new_name, cmd->interval);
            notify_monitor(adapter, NEU_REQ_UPDATE_GROUP_EVENT, cmd);
        }

        strcpy(resp.driver, cmd->driver);
        strcpy(resp.group, cmd->group);
        strcpy(resp.new_name, cmd->new_name);
        header->type = NEU_RESP_UPDATE_DRIVER_GROUP;
        neu_msg_exchange(header);
        reply(adapter, header, &resp);
        break;
    }
    case NEU_REQ_DEL_GROUP: {
        neu_req_del_group_t *cmd   = (neu_req_del_group_t *) &header[1];
        neu_resp_error_t     error = { 0 };

        if (adapter->module->type == NEU_NA_TYPE_DRIVER) {
            error.error = neu_adapter_driver_del_group(
                (neu_adapter_driver_t *) adapter, cmd->group);
        } else {
            adapter->module->intf_funs->request(
                adapter->plugin, (neu_reqresp_head_t *) header, &header[1]);
            neu_msg_free(msg);
            break;
        }

        if (error.error == NEU_ERR_SUCCESS) {
            adapter_storage_del_group(cmd->driver, cmd->group);
            notify_monitor(adapter, NEU_REQ_DEL_GROUP_EVENT, cmd);
        }

        neu_msg_exchange(header);
        header->type = NEU_RESP_ERROR;
        reply(adapter, header, &error);
        break;
    }
    case NEU_REQ_NODE_CTL: {
        neu_req_node_ctl_t *cmd   = (neu_req_node_ctl_t *) &header[1];
        neu_resp_error_t    error = { 0 };

        switch (cmd->ctl) {
        case NEU_ADAPTER_CTL_START:
            error.error = neu_adapter_start(adapter);
            break;
        case NEU_ADAPTER_CTL_STOP:
            error.error = neu_adapter_stop(adapter);
            break;
        }

        if (0 == error.error) {
            notify_monitor(adapter, NEU_REQ_NODE_CTL_EVENT, cmd);
        }

        neu_msg_exchange(header);
        header->type = NEU_RESP_ERROR;
        reply(adapter, header, &error);
        break;
    }
    case NEU_REQ_NODE_RENAME: {
        neu_req_node_rename_t *cmd  = (neu_req_node_rename_t *) &header[1];
        neu_resp_node_rename_t resp = { 0 };
        resp.error = neu_adapter_rename(adapter, cmd->new_name);
        strcpy(header->receiver, header->sender);
        strcpy(header->sender, cmd->new_name);
        strcpy(resp.node, cmd->node);
        strcpy(resp.new_name, cmd->new_name);
        header->type = NEU_RESP_NODE_RENAME;
        reply(adapter, header, &resp);
        break;
    }
    case NEU_REQ_DEL_TAG: {
        neu_req_del_tag_t *cmd   = (neu_req_del_tag_t *) &header[1];
        neu_resp_error_t   error = { 0 };

        if (adapter->module->type == NEU_NA_TYPE_DRIVER) {
            for (int i = 0; i < cmd->n_tag; i++) {
                int ret = neu_adapter_driver_del_tag(
                    (neu_adapter_driver_t *) adapter, cmd->group, cmd->tags[i]);
                if (0 == ret) {
                    adapter_storage_del_tag(cmd->driver, cmd->group,
                                            cmd->tags[i]);
                }
            }
        } else {
            error.error = NEU_ERR_GROUP_NOT_ALLOW;
        }

        if (0 == error.error) {
            notify_monitor(adapter, NEU_REQ_DEL_TAG_EVENT, cmd);
        } else {
            for (uint16_t i = 0; i < cmd->n_tag; i++) {
                free(cmd->tags[i]);
            }
            free(cmd->tags);
        }

        neu_msg_exchange(header);
        header->type = NEU_RESP_ERROR;
        reply(adapter, header, &error);
        break;
    }
    case NEU_REQ_ADD_TAG: {
        neu_req_add_tag_t *cmd  = (neu_req_add_tag_t *) &header[1];
        neu_resp_add_tag_t resp = { 0 };

        if (adapter->module->type == NEU_NA_TYPE_DRIVER) {
            for (int i = 0; i < cmd->n_tag; i++) {
                int ret = neu_adapter_driver_validate_tag(
                    (neu_adapter_driver_t *) adapter, cmd->group,
                    &cmd->tags[i]);
                if (ret == 0) {
                    resp.index += 1;
                } else {
                    resp.error = ret;
                    break;
                }
            }
        } else {
            resp.error = NEU_ERR_GROUP_NOT_ALLOW;
        }

        if (resp.index > 0) {
            int ret = neu_adapter_driver_try_add_tag(
                (neu_adapter_driver_t *) adapter, cmd->group, cmd->tags,
                resp.index);
            if (ret != 0) {
                resp.index = 0;
                resp.error = ret;
            }
        }

        for (int i = 0; i < resp.index; i++) {
            int ret = neu_adapter_driver_add_tag(
                (neu_adapter_driver_t *) adapter, cmd->group, &cmd->tags[i]);
            if (ret != 0) {
                neu_adapter_driver_try_del_tag((neu_adapter_driver_t *) adapter,
                                               resp.index - i);
                resp.index = i;
                resp.error = ret;
                break;
            }
        }

        for (uint16_t i = resp.index; i < cmd->n_tag; i++) {
            neu_tag_fini(&cmd->tags[i]);
        }

        if (resp.index) {
            // we have added some tags, try to persist
            adapter_storage_add_tags(cmd->driver, cmd->group, cmd->tags,
                                     resp.index);
            cmd->n_tag = resp.index;
            notify_monitor(adapter, NEU_REQ_ADD_TAG_EVENT, cmd);
        } else {
            free(cmd->tags);
        }

        neu_msg_exchange(header);
        header->type = NEU_RESP_ADD_TAG;
        reply(adapter, header, &resp);
        break;
    }
    case NEU_REQ_UPDATE_TAG: {
        neu_req_update_tag_t *cmd  = (neu_req_update_tag_t *) &header[1];
        neu_resp_update_tag_t resp = { 0 };

        if (adapter->module->type == NEU_NA_TYPE_DRIVER) {
            for (int i = 0; i < cmd->n_tag; i++) {
                int ret = neu_adapter_driver_update_tag(
                    (neu_adapter_driver_t *) adapter, cmd->group,
                    &cmd->tags[i]);
                if (ret == 0) {
                    adapter_storage_update_tag(cmd->driver, cmd->group,
                                               &cmd->tags[i]);

                    resp.index += 1;
                } else {
                    resp.error = ret;
                    break;
                }
            }
        } else {
            resp.error = NEU_ERR_GROUP_NOT_ALLOW;
        }

        for (uint16_t i = resp.index; i < cmd->n_tag; i++) {
            neu_tag_fini(&cmd->tags[i]);
        }
        if (resp.index > 0) {
            cmd->n_tag = resp.index;
            notify_monitor(adapter, NEU_REQ_UPDATE_TAG_EVENT, cmd);
        } else {
            free(cmd->tags);
        }

        neu_msg_exchange(header);
        header->type = NEU_RESP_UPDATE_TAG;
        reply(adapter, header, &resp);
        break;
    }
    case NEU_REQ_NODE_UNINIT: {
        neu_req_node_uninit_t *cmd = (neu_req_node_uninit_t *) &header[1];
        char                   name[NEU_NODE_NAME_LEN]     = { 0 };
        char                   receiver[NEU_NODE_NAME_LEN] = { 0 };

        neu_adapter_uninit(adapter);

        header->type = NEU_RESP_NODE_UNINIT;
        neu_msg_exchange(header);
        strcpy(header->sender, adapter->name);
        strcpy(cmd->node, adapter->name);

        neu_msg_gen(header, cmd);

        strcpy(name, adapter->name);
        strcpy(receiver, header->receiver);

        int ret = neu_send_msg(adapter->control_fd, msg);
        if (0 != ret) {
            nlog_error("%s %d send uninit msg to %s error: %s(%d)", name,
                       adapter->control_fd, receiver, strerror(errno), errno);
            neu_msg_free(msg);
        } else {
            nlog_notice("%s send uninit msg to %s failed", name, receiver);
        }
        break;
    }
    case NEU_REQ_ADD_NDRIVER_MAP: {
        neu_msg_free(msg);
        break;
    }
    case NEU_REQ_DEL_NDRIVER_MAP: {
        neu_msg_free(msg);
        break;
    }
    case NEU_REQ_UPDATE_NDRIVER_TAG_PARAM: {
        neu_req_update_ndriver_tag_param_t *cmd =
            (neu_req_update_ndriver_tag_param_t *) &header[1];
        neu_resp_update_tag_t resp = { 0 };

        for (uint16_t i = 0; i < cmd->n_tag; i++) {
            // TODO
            resp.index += 1;
        }

        neu_req_update_ndriver_tag_param_fini(cmd);

        neu_msg_exchange(header);
        header->type = NEU_RESP_UPDATE_TAG;
        reply(adapter, header, &resp);
        break;
    }
    case NEU_REQ_UPDATE_NDRIVER_TAG_INFO: {
        neu_req_update_ndriver_tag_info_t *cmd =
            (neu_req_update_ndriver_tag_info_t *) &header[1];
        neu_resp_update_tag_t resp = { 0 };

        for (uint16_t i = 0; i < cmd->n_tag; i++) {
            // TODO
            resp.index += 1;
        }

        neu_req_update_ndriver_tag_info_fini(cmd);

        neu_msg_exchange(header);
        header->type = NEU_RESP_UPDATE_TAG;
        reply(adapter, header, &resp);
        break;
    }
    case NEU_REQ_GET_NDRIVER_TAGS: {
        neu_req_get_ndriver_tags_t *cmd =
            (neu_req_get_ndriver_tags_t *) &header[1];
        neu_resp_error_t error = { .error = 0 };
        UT_array *       tags  = NULL;

        // TODO
        (void) cmd;
        utarray_new(tags, neu_ndriver_tag_get_icd());

        neu_msg_exchange(header);
        if (error.error != NEU_ERR_SUCCESS) {
            header->type = NEU_RESP_ERROR;
            reply(adapter, header, &error);
        } else {
            neu_resp_get_tag_t resp = { .tags = tags };
            header->type            = NEU_RESP_GET_NDRIVER_TAGS;
            reply(adapter, header, &resp);
        }

        break;
    }
    case NEU_REQ_UPDATE_LOG_LEVEL: {
        neu_req_update_log_level_t *cmd =
            (neu_req_update_log_level_t *) &header[1];
        neu_resp_error_t error = { 0 };

        adapter->log_level = cmd->log_level;
        zlog_level_switch(neu_plugin_to_plugin_common(adapter->plugin)->log,
                          cmd->log_level);
        neu_msg_exchange(header);
        header->type = NEU_RESP_ERROR;
        reply(adapter, header, &error);

        break;
    }

    default:
        nlog_warn("adapter: %s recv msg type error, type: %s", adapter->name,
                  neu_reqresp_type_string(header->type));
        assert(false);
        break;
    }

    return 0;
}

/**
 * 销毁适配器
 *
 * @param adapter 适配器指针
 */
void neu_adapter_destroy(neu_adapter_t *adapter)
{
    nlog_notice("adapter %s destroy", adapter->name);
    close(adapter->control_fd);
    close(adapter->trans_data_fd);

    adapter->module->intf_funs->close(adapter->plugin);

    if (NULL != adapter->metrics) {
        neu_metrics_del_node(adapter);
        neu_node_metrics_free(adapter->metrics);
    }

    if (adapter->consumer_tid != 0) {
        pthread_cancel(adapter->consumer_tid);
    }
    if (adapter->msg_q != NULL) {
        adapter_msg_q_free(adapter->msg_q);
    }

    char *setting = NULL;
    if (adapter_load_setting(adapter->name, &setting) != 0) {
        remove_logs(adapter->name);
    } else {
        free(setting);
    }

    if (adapter->name != NULL) {
        free(adapter->name);
    }
    if (NULL != adapter->setting) {
        free(adapter->setting);
    }

    neu_event_close(adapter->events);
    free(adapter);
}

/**
 * 反初始化适配器
 *
 * @param adapter 适配器指针
 * @return 执行结果(int)，0表示成功
 */
int neu_adapter_uninit(neu_adapter_t *adapter)
{
    if (adapter->module->type == NEU_NA_TYPE_DRIVER) {
        neu_adapter_driver_uninit((neu_adapter_driver_t *) adapter);
    }
    adapter->module->intf_funs->uninit(adapter->plugin);

    neu_event_del_io(adapter->events, adapter->control_io);

    if (adapter->module->type == NEU_NA_TYPE_DRIVER) {
        neu_adapter_driver_destroy((neu_adapter_driver_t *) adapter);
    }

    nlog_notice("Stop the adapter(%s)", adapter->name);
    return 0;
}

/**
 * 启动适配器
 *
 * @param adapter 适配器指针
 * @return 执行结果(int)，0表示成功
 */
int neu_adapter_start(neu_adapter_t *adapter)
{
    const neu_plugin_intf_funs_t *intf_funs = adapter->module->intf_funs;
    neu_err_code_e                error     = NEU_ERR_SUCCESS;

    switch (adapter->state) {
    case NEU_NODE_RUNNING_STATE_INIT:
        error = NEU_ERR_NODE_NOT_READY;
        break;
    case NEU_NODE_RUNNING_STATE_RUNNING:
        error = NEU_ERR_NODE_IS_RUNNING;
        break;
    case NEU_NODE_RUNNING_STATE_READY:
    case NEU_NODE_RUNNING_STATE_STOPPED:
        break;
    }

    if (error != NEU_ERR_SUCCESS) {
        return error;
    }

    error = intf_funs->start(adapter->plugin);
    if (error == NEU_ERR_SUCCESS) {
        adapter->state = NEU_NODE_RUNNING_STATE_RUNNING;
        adapter_storage_state(adapter->name, adapter->state);
        if (NEU_NA_TYPE_DRIVER == neu_adapter_get_type(adapter)) {
            neu_adapter_driver_start_group_timer(
                (neu_adapter_driver_t *) adapter);
        }
    }

    return error;
}

/**
 * 启动单个适配器
 *
 * @param adapter 适配器指针
 * @return 执行结果(int)，0表示成功
 */
int neu_adapter_start_single(neu_adapter_t *adapter)
{
    const neu_plugin_intf_funs_t *intf_funs = adapter->module->intf_funs;

    adapter->state = NEU_NODE_RUNNING_STATE_RUNNING;
    return intf_funs->start(adapter->plugin);
}

/**
 * 停止适配器
 *
 * @param adapter 适配器指针
 * @return 执行结果(int)，0表示成功
 */
int neu_adapter_stop(neu_adapter_t *adapter)
{
    const neu_plugin_intf_funs_t *intf_funs = adapter->module->intf_funs;
    neu_err_code_e                error     = NEU_ERR_SUCCESS;

    switch (adapter->state) {
    case NEU_NODE_RUNNING_STATE_INIT:
    case NEU_NODE_RUNNING_STATE_READY:
        error = NEU_ERR_NODE_NOT_RUNNING;
        break;
    case NEU_NODE_RUNNING_STATE_STOPPED:
        error = NEU_ERR_NODE_IS_STOPED;
        break;
    case NEU_NODE_RUNNING_STATE_RUNNING:
        break;
    }

    if (error != NEU_ERR_SUCCESS) {
        return error;
    }

    error = intf_funs->stop(adapter->plugin);
    if (error == NEU_ERR_SUCCESS) {
        adapter->state = NEU_NODE_RUNNING_STATE_STOPPED;
        adapter_storage_state(adapter->name, adapter->state);
        if (NEU_NA_TYPE_DRIVER == neu_adapter_get_type(adapter)) {
            neu_adapter_driver_stop_group_timer(
                (neu_adapter_driver_t *) adapter);
        }
        neu_adapter_reset_metrics(adapter);
    }

    return error;
}

/**
 * 设置适配器配置
 *
 * @param adapter 适配器指针
 * @param setting 配置字符串
 * @return 执行结果(int)，0表示成功
 */
int neu_adapter_set_setting(neu_adapter_t *adapter, const char *setting)
{
    int rv = -1;

    const neu_plugin_intf_funs_t *intf_funs;

    intf_funs = adapter->module->intf_funs;
    rv        = intf_funs->setting(adapter->plugin, setting);
    if (rv == 0) {
        if (adapter->setting != NULL) {
            free(adapter->setting);
        }
        adapter->setting = strdup(setting);

        if (adapter->state == NEU_NODE_RUNNING_STATE_INIT) {
            adapter->state = NEU_NODE_RUNNING_STATE_READY;
            neu_adapter_start(adapter);
        }
    } else {
        rv = NEU_ERR_NODE_SETTING_INVALID;
    }

    return rv;
}

/**
 * 获取适配器配置
 *
 * @param adapter 适配器指针
 * @param config 配置字符串指针
 * @return 执行结果(int)，0表示成功
 */
int neu_adapter_get_setting(neu_adapter_t *adapter, char **config)
{
    if (adapter->setting != NULL) {
        *config = strdup(adapter->setting);
        return NEU_ERR_SUCCESS;
    }

    return NEU_ERR_NODE_SETTING_NOT_FOUND;
}

/**
 * 获取适配器状态
 *
 * @param adapter 适配器指针
 * @return 节点状态
 */
neu_node_state_t neu_adapter_get_state(neu_adapter_t *adapter)
{
    neu_node_state_t     state  = { 0 };
    neu_plugin_common_t *common = neu_plugin_to_plugin_common(adapter->plugin);

    state.link      = common->link_state;
    state.running   = adapter->state;
    state.log_level = adapter->log_level;

    return state;
}

/**
 * 验证适配器标签
 *
 * @param adapter 适配器指针
 * @param tag 数据标签指针
 * @return 执行结果(int)，0表示成功
 */
int neu_adapter_validate_tag(neu_adapter_t *adapter, neu_datatag_t *tag)
{
    const neu_plugin_intf_funs_t *intf_funs = adapter->module->intf_funs;
    neu_err_code_e                error     = NEU_ERR_SUCCESS;

    error = intf_funs->driver.validate_tag(adapter->plugin, tag);

    return error;
}

/**
 * 添加定时器
 *
 * @param adapter 适配器指针
 * @param param 定时器参数
 * @return 定时器指针
 */
neu_event_timer_t *neu_adapter_add_timer(neu_adapter_t *         adapter,
                                         neu_event_timer_param_t param)
{
    return neu_event_add_timer(adapter->events, param);
}

/**
 * 删除定时器
 *
 * @param adapter 适配器指针
 * @param timer 定时器指针
 */
void neu_adapter_del_timer(neu_adapter_t *adapter, neu_event_timer_t *timer)
{
    neu_event_del_timer(adapter->events, timer);
}

/**
 * 注册组指标
 *
 * @param adapter 适配器指针
 * @param group_name 组名
 * @param name 指标名称
 * @param help 指标帮助信息
 * @param type 指标类型
 * @param init 初始值
 * @return 执行结果(int)，0表示成功
 */
int neu_adapter_register_group_metric(neu_adapter_t *adapter,
                                      const char *group_name, const char *name,
                                      const char *help, neu_metric_type_e type,
                                      uint64_t init)
{
    if (NULL == adapter->metrics) {
        return -1;
    }

    return neu_node_metrics_add(adapter->metrics, group_name, name, help, type,
                                init);
}

/**
 * 更新组指标
 *
 * @param adapter 适配器指针
 * @param group_name 组名
 * @param metric_name 指标名称
 * @param n 指标新值
 * @return 执行结果(int)，0表示成功
 */
int neu_adapter_update_group_metric(neu_adapter_t *adapter,
                                    const char *   group_name,
                                    const char *metric_name, uint64_t n)
{
    if (NULL == adapter->metrics) {
        return -1;
    }

    return neu_node_metrics_update(adapter->metrics, group_name, metric_name,
                                   n);
}

/**
 * 更新组名
 *
 * @param adapter 适配器指针
 * @param group_name 旧组名
 * @param new_group_name 新组名
 * @return 执行结果(int)，0表示成功
 */
int neu_adapter_metric_update_group_name(neu_adapter_t *adapter,
                                         const char *   group_name,
                                         const char *   new_group_name)
{
    if (NULL == adapter->metrics) {
        return -1;
    }

    return neu_node_metrics_update_group(adapter->metrics, group_name,
                                         new_group_name);
}

/**
 * 删除组指标
 *
 * @param adapter 适配器指针
 * @param group_name 组名
 */
void neu_adapter_del_group_metrics(neu_adapter_t *adapter,
                                   const char *   group_name)
{
    if (NULL != adapter->metrics) {
        neu_node_metrics_del_group(adapter->metrics, group_name);
    }
}

/**
 * 发送响应消息的内联函数
 *
 * @param adapter 适配器指针
 * @param header 请求/响应头指针
 * @param data 响应数据
 */
inline static void reply(neu_adapter_t *adapter, neu_reqresp_head_t *header,
                         void *data)
{
    neu_msg_gen(header, data);
    int ret = neu_send_msg(adapter->control_fd, (neu_msg_t *) header);
    if (0 != ret) {
        nlog_warn("%s reply %s to %s, error: %s(%d)", header->sender,
                  neu_reqresp_type_string(header->type), header->receiver,
                  strerror(errno), errno);
    }
}

/**
 * 向监控模块发送通知的内联函数
 *
 * @param adapter 适配器指针
 * @param event 事件类型
 * @param data 事件数据
 */
inline static void notify_monitor(neu_adapter_t *    adapter,
                                  neu_reqresp_type_e event, void *data)
{
    neu_msg_t *msg = neu_msg_new(event, NULL, data);
    if (NULL == msg) {
        return;
    }
    neu_reqresp_head_t *header = neu_msg_get_header(msg);

    strcpy(header->receiver, "manager");
    strncpy(header->sender, adapter->name, NEU_NODE_NAME_LEN);

    int ret = neu_send_msg(adapter->control_fd, msg);
    if (0 != ret) {
        nlog_warn("notify %s of %s, error: %s(%d)", header->receiver,
                  neu_reqresp_type_string(header->type), strerror(errno),
                  errno);
        neu_msg_free(msg);
    }
}
