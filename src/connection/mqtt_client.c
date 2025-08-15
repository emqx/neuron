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
#include <inttypes.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <arpa/inet.h>

#define NNG_SUPP_TLS 1
#define NNG_SUPP_SQLITE 1
#include <nng/mqtt/mqtt_client.h>
#include <nng/nng.h>
#include <nng/supplemental/tls/tls.h>
#include <nng/supplemental/util/platform.h>

#include "connection/mqtt_client.h"
#include "event/event.h"
#include "utils/asprintf.h"
#include "utils/time.h"
#include "utils/utextend.h"
#include "utils/uthash.h"
#include "utils/utlist.h"
#include "utils/zlog.h"

#define log(level, ...)                               \
    do {                                              \
        if (client->log) {                            \
            zlog_##level(client->log, ##__VA_ARGS__); \
        }                                             \
    } while (0)

#define return_failure_if_open()                                           \
    do {                                                                   \
        if (client->open) {                                                \
            nng_mtx_unlock(client->mtx);                                   \
            log(warn,                                                      \
                "mqtt client already open, setting will not take effect"); \
            return -1;                                                     \
        }                                                                  \
    } while (0)

/**
 * @brief MQTT订阅信息结构体
 *
 * 存储每个MQTT主题订阅的相关信息
 */
typedef struct {
    size_t                         ref;   /**< 引用计数 */
    bool                           ack;   /**< 是否已收到订阅确认 */
    neu_mqtt_qos_e                 qos;   /**< 服务质量等级 */
    char                          *topic; /**< 订阅的主题 */
    neu_mqtt_client_subscribe_cb_t cb;    /**< 消息到达回调函数 */
    void                          *data;  /**< 用户自定义数据 */
    UT_hash_handle                 hh;    /**< uthash哈希处理结构 */
} subscription_t;

/**
 * @brief MQTT客户端任务类型枚举
 */
typedef enum {
    TASK_PUB,   /**< 发布任务 */
    TASK_SUB,   /**< 订阅任务 */
    TASK_UNSUB, /**< 取消订阅任务 */
    TASK_RECV,  /**< 接收任务 */
} task_kind_e;

#define TASK_UNION_FIELDS                     \
    struct {                                  \
        neu_mqtt_client_publish_cb_t cb;      \
        neu_mqtt_qos_e               qos;     \
        char                        *topic;   \
        uint8_t                     *payload; \
        uint32_t                     len;     \
        void                        *data;    \
    } pub;                                    \
    subscription_t *sub;                      \
    struct {                                  \
        subscription_t *sub;                  \
    } recv

typedef union {
    TASK_UNION_FIELDS;
} task_union;

/**
 * @brief MQTT客户端任务结构体
 *
 * 表示MQTT客户端需要执行的一个操作(发布、订阅、接收等)
 */
typedef struct task_s {
    task_kind_e kind; /**< 任务类型 */
    nng_aio    *aio;  /**< NNG异步I/O对象 */
    union {
        TASK_UNION_FIELDS; /**< 根据任务类型不同包含的数据 */
    };
    struct task_s *prev; /**< 双向链表前一个节点 */
    struct task_s *next; /**< 双向链表后一个节点 */
} task_t;

/**
 * @brief MQTT客户端结构体
 *
 * Neuron MQTT客户端的主要数据结构，管理连接和操作
 */
struct neu_mqtt_client_s {
    nng_socket                      sock;       /**< NNG MQTT套接字 */
    nng_mtx                        *mtx;        /**< 互斥锁，保护客户端状态 */
    neu_events_t                   *events;     /**< 事件系统引用 */
    neu_event_timer_t              *timer;      /**< 重连定时器 */
    neu_mqtt_version_e              version;    /**< MQTT协议版本 */
    char                           *host;       /**< MQTT服务器主机名/IP */
    uint16_t                        port;       /**< MQTT服务器端口 */
    char                           *url;        /**< 连接URL */
    nng_tls_config                 *tls_cfg;    /**< TLS配置(如果使用) */
    nng_msg                        *conn_msg;   /**< 连接消息 */
    nng_duration                    retry;      /**< 重试间隔(毫秒) */
    bool                            open;       /**< 是否已打开 */
    bool                            connected;  /**< 是否已连接 */
    neu_mqtt_client_connection_cb_t connect_cb; /**< 连接成功回调 */
    void                           *connect_cb_data;    /**< 连接回调用户数据 */
    neu_mqtt_client_connection_cb_t disconnect_cb;      /**< 连接断开回调 */
    void                           *disconnect_cb_data; /**< 断开回调用户数据 */
    nng_mqtt_sqlite_option         *sqlite_cfg;         /**< SQLite持久化配置 */
    bool                            receiving;          /**< 是否正在接收消息 */
    nng_aio                        *recv_aio;           /**< 接收异步I/O对象 */
    subscription_t                 *subscriptions;      /**< 主题订阅哈希表 */
    size_t                          suback_count;       /**< 订阅确认计数 */
    size_t                          task_count;         /**< 当前任务数量 */
    size_t                          task_limit;         /**< 任务数量限制 */
    task_t                         *task_free_list;     /**< 空闲任务链表 */
    zlog_category_t                *log;                /**< 日志类别 */
};

/**
 * @brief 创建新的任务对象
 * @param client MQTT客户端对象指针
 * @return 成功返回任务对象指针，失败返回NULL
 */
static inline task_t *task_new(neu_mqtt_client_t *client);

/**
 * @brief 释放任务对象资源
 * @param task 要释放的任务对象指针
 */
static inline void task_free(task_t *task);

/**
 * @brief 释放任务链表中所有任务
 * @param tasks 任务链表头指针
 */
static inline void tasks_free(task_t *tasks);

/**
 * @brief NNG异步I/O回调函数
 * @param arg 用户参数(任务对象指针)
 */
static void task_cb(void *arg);

/**
 * @brief 处理发布任务
 * @param task 任务对象指针
 * @param client MQTT客户端对象指针
 */
static void task_handle_pub(task_t *task, neu_mqtt_client_t *client);

/**
 * @brief 处理订阅任务
 * @param task 任务对象指针
 * @param client MQTT客户端对象指针
 */
static void task_handle_sub(task_t *task, neu_mqtt_client_t *client);

/**
 * @brief 处理取消订阅任务
 * @param task 任务对象指针
 * @param client MQTT客户端对象指针
 */
static void task_handle_unsub(task_t *task, neu_mqtt_client_t *client);

/**
 * @brief 处理接收任务
 * @param task 任务对象指针
 * @param client MQTT客户端对象指针
 */
static void task_handle_recv(task_t *task, neu_mqtt_client_t *client);

/**
 * @brief 创建新的订阅对象
 * @param client MQTT客户端对象指针
 * @param qos 服务质量等级
 * @param topic 订阅的主题
 * @param cb 接收消息回调函数
 * @param data 用户自定义数据
 * @return 成功返回订阅对象指针，失败返回NULL
 */
static subscription_t *subscription_new(neu_mqtt_client_t *client,
                                        neu_mqtt_qos_e qos, const char *topic,
                                        neu_mqtt_client_subscribe_cb_t cb,
                                        void                          *data);

/**
 * @brief 释放订阅对象资源
 * @param subscription 要释放的订阅对象指针
 */
static inline void subscription_free(subscription_t *subscription);

/**
 * @brief 增加订阅对象引用计数
 * @param subscription 订阅对象指针
 * @return 返回订阅对象指针
 */
static inline subscription_t *subscription_ref(subscription_t *subscription);

/**
 * @brief 释放订阅哈希表中所有订阅对象
 * @param subscriptions 订阅哈希表头指针
 */
static inline void subscriptions_free(subscription_t *subscriptions);

/**
 * @brief MQTT消息接收回调函数
 * @param arg 用户参数(MQTT客户端对象指针)
 */
static void recv_cb(void *arg);

/**
 * @brief 重新订阅回调函数
 * @param data 用户数据(MQTT客户端对象指针)
 * @return 成功返回0，失败返回错误码
 */
static int resub_cb(void *data);

/**
 * @brief 连接断开回调函数
 * @param p NNG管道对象
 * @param ev NNG管道事件
 * @param arg 用户参数(MQTT客户端对象指针)
 */
static void disconnect_cb(nng_pipe p, nng_pipe_ev ev, void *arg);

/**
 * @brief 连接建立回调函数
 * @param p NNG管道对象
 * @param ev NNG管道事件
 * @param arg 用户参数(MQTT客户端对象指针)
 */
static void connect_cb(nng_pipe p, nng_pipe_ev ev, void *arg);

/**
 * @brief 从客户端分配一个任务对象
 * @param client MQTT客户端对象指针
 * @return 成功返回任务对象指针，失败返回NULL
 */
static inline task_t *client_alloc_task(neu_mqtt_client_t *client);

/**
 * @brief 释放任务对象回客户端的空闲列表
 * @param client MQTT客户端对象指针
 * @param task 要释放的任务对象指针
 */
static inline void client_free_task(neu_mqtt_client_t *client, task_t *task);

/**
 * @brief 获取客户端空闲任务列表长度
 * @param client MQTT客户端对象指针
 * @return 空闲任务列表长度
 */
static inline size_t client_task_free_list_len(neu_mqtt_client_t *client);

/**
 * @brief 向客户端添加订阅
 * @param client MQTT客户端对象指针
 * @param sub 订阅对象指针
 */
static inline void client_add_subscription(neu_mqtt_client_t *client,
                                           subscription_t    *sub);

/**
 * @brief 发送订阅消息
 * @param client MQTT客户端对象指针
 * @param subscription 订阅对象指针
 * @return 成功返回0，失败返回错误码
 */
static int client_send_sub_msg(neu_mqtt_client_t *client,
                               subscription_t    *subscription);

/**
 * @brief 启动客户端接收
 * @param client MQTT客户端对象指针
 */
static inline void client_start_recv(neu_mqtt_client_t *client);

/**
 * @brief 启动重连定时器
 * @param client MQTT客户端对象指针
 * @return 成功返回0，失败返回错误码
 */
static inline int client_start_timer(neu_mqtt_client_t *client);

/**
 * @brief 构建连接URL
 * @param client MQTT客户端对象指针
 * @return 成功返回0，失败返回错误码
 */
static inline int client_make_url(neu_mqtt_client_t *client);

/**
 * @brief 将Neuron MQTT版本枚举转换为NNG MQTT版本值
 *
 * @param v Neuron MQTT版本枚举
 * @return NNG MQTT版本值
 */
static inline uint8_t neu_mqtt_version_to_nng_mqtt_version(neu_mqtt_version_e v)
{
    switch (v) {
    case NEU_MQTT_VERSION_V31:
        return MQTT_PROTOCOL_VERSION_v31;
    case NEU_MQTT_VERSION_V311:
        return MQTT_PROTOCOL_VERSION_v311;
    case NEU_MQTT_VERSION_V5:
        return MQTT_PROTOCOL_VERSION_v5;
    default:
        assert(!"logic error, invalid mqtt version");
    }
}

/**
 * @brief 创建新的任务对象
 *
 * 分配内存并初始化任务对象的异步I/O
 *
 * @param client MQTT客户端对象指针
 * @return 成功返回任务对象指针，失败返回NULL
 */
static inline task_t *task_new(neu_mqtt_client_t *client)
{
    /* 分配任务对象内存并初始化为0 */
    task_t *task = calloc(1, sizeof(*task));
    if (NULL == task) {
        log(error, "calloc task fail");
        return NULL;
    }

    /* 创建NNG异步I/O对象 */
    int rv = 0;
    if ((rv = nng_aio_alloc(&task->aio, task_cb, task)) != 0) {
        log(error, "nng_aio_alloc fail: %s", nng_strerror(rv));
        free(task);
        return NULL;
    }

    /* 设置客户端对象作为异步I/O的输入参数 */
    nng_aio_set_input(task->aio, 0, client);

    return task;
}

/**
 * @brief 释放任务对象资源
 *
 * 释放异步I/O对象和任务对象本身的内存
 *
 * @param task 要释放的任务对象指针
 */
static inline void task_free(task_t *task)
{
    // NanoSDK quirks: calling nng_aio_stop will block if the aio is in use
    // nng_aio_stop(task->aio);
    nng_aio_free(task->aio);
    free(task);
}

/**
 * @brief 释放任务链表中所有任务
 *
 * 遍历链表并释放每个任务对象
 *
 * @param tasks 任务链表头指针
 */
static inline void tasks_free(task_t *tasks)
{
    task_t *task = NULL, *tmp = NULL;
    /* 安全遍历链表，删除并释放每个节点 */
    DL_FOREACH_SAFE(tasks, task, tmp)
    {
        DL_DELETE(tasks, task);
        task_free(task);
    }
}

/**
 * @brief NNG异步I/O回调函数
 *
 * 当异步操作完成时被调用，根据任务类型执行相应的处理
 *
 * @param arg 用户参数(任务对象指针)
 */
static void task_cb(void *arg)
{
    task_t            *task   = arg;
    nng_aio           *aio    = task->aio;
    neu_mqtt_client_t *client = nng_aio_get_input(aio, 0);

    if (TASK_PUB == task->kind) {
        task_handle_pub(task, client);
    } else if (TASK_SUB == task->kind) {
        task_handle_sub(task, client);
    } else if (TASK_UNSUB == task->kind) {
        task_handle_unsub(task, client);
    } else if (TASK_RECV == task->kind) {
        task_handle_recv(task, client);
    } else {
        log(error, "unexpected task kind:%d", task->kind);
        assert(!"logic error, task kind not exhausted");
    }

    nng_mtx_lock(client->mtx);
    client_free_task(client, task);
    nng_mtx_unlock(client->mtx);
}

static void task_handle_pub(task_t *task, neu_mqtt_client_t *client)
{
    nng_aio *aio = task->aio;

    int rv = 0;
    if (0 != (rv = nng_aio_result(aio))) {
        nng_msg *msg = nng_aio_get_msg(aio);
        nng_msg_free(msg);
        log(error, "send PUBLISH error: %s", nng_strerror(rv));
        log(error, "pub [%s, QoS%d] fail", task->pub.topic, task->pub.qos);
    } else {
        log(debug, "pub [%s, QoS%d] %" PRIu32 " bytes", task->pub.topic,
            task->pub.qos, task->pub.len);
    }

    if (task->pub.cb) {
        task->pub.cb(rv, task->pub.qos, task->pub.topic, task->pub.payload,
                     task->pub.len, task->pub.data);
    }
    return;
}

static void task_handle_sub(task_t *task, neu_mqtt_client_t *client)
{
    int      rv  = 0;
    uint32_t n   = 0;
    uint8_t *rc  = NULL;
    nng_msg *msg = NULL;
    nng_aio *aio = task->aio;

    if (0 != (rv = nng_aio_result(aio))) {
        log(error, "send SUBSCRIBE error: %s", nng_strerror(rv));
        goto end;
    }

    msg = nng_aio_get_msg(aio);
    if (NULL == msg) {
        rv = -1;
        goto end;
    }

    nng_mqtt_packet_type pt = nng_mqtt_msg_get_packet_type(msg);
    if (NNG_MQTT_SUBACK != pt) {
        log(error, "recv packet type:%d, expect SUBACK packet", pt);
        rv = -1;
        goto end;
    }

    rc = nng_mqtt_msg_get_suback_return_codes(msg, &n);
    log(debug, "recv SUBACK return_code:%d", *rc);
    rv             = 0x80 == *rc ? -1 : 0;
    task->sub->ack = true;
    client->suback_count += 1;

end:
    // TODO: handle retry subscribe on failure
    log(notice, "sub [%s, QoS%d], %s", task->sub->topic, task->sub->qos,
        (0 == rv) ? "OK" : "FAIL");
    nng_msg_free(msg);
    return;
}

static void task_handle_unsub(task_t *task, neu_mqtt_client_t *client)
{
    int      rv  = 0;
    nng_msg *msg = NULL;
    nng_aio *aio = task->aio;

    if (0 != (rv = nng_aio_result(aio))) {
        log(error, "send UNSUBSCRIBE error: %s", nng_strerror(rv));
        goto end;
    }

    msg = nng_aio_get_msg(aio);
    if (NULL == msg) {
        rv = -1;
        goto end;
    }

    nng_mqtt_packet_type pt = nng_mqtt_msg_get_packet_type(msg);
    if (NNG_MQTT_UNSUBACK != pt) {
        log(error, "recv packet type:%d, expect UNSUBACK packet", pt);
        rv = -1;
        goto end;
    }

end:
    if (task->sub) {
        log(notice, "unsub [%s, QoS%d], %s", task->sub->topic, task->sub->qos,
            (0 == rv) ? "OK" : "FAIL");
    } else {
        log(notice, "unsub %s", (0 == rv) ? "OK" : "FAIL");
    }
    nng_msg_free(msg);
    return;
}

static void task_handle_recv(task_t *task, neu_mqtt_client_t *client)
{
    (void) client;

    nng_msg *msg = nng_aio_get_msg(task->aio);
    uint32_t payload_len;
    uint8_t *payload = nng_mqtt_msg_get_publish_payload(msg, &payload_len);

    task->recv.sub->cb(task->recv.sub->qos, task->recv.sub->topic, payload,
                       payload_len, task->recv.sub->data);

    nng_msg_free(msg);
}

static subscription_t *subscription_new(neu_mqtt_client_t *client,
                                        neu_mqtt_qos_e qos, const char *topic,
                                        neu_mqtt_client_subscribe_cb_t cb,
                                        void                          *data)
{
    (void) client;
    subscription_t *subscription = NULL;

    subscription = calloc(1, sizeof(*subscription));
    if (NULL == subscription) {
        log(error, "calloc subscription fail");
        return NULL;
    }

    subscription->topic = strdup(topic);
    if (NULL == subscription->topic) {
        log(error, "strdup subscription topic fail");
        free(subscription);
        return NULL;
    }

    subscription->ref  = 1;
    subscription->qos  = qos;
    subscription->cb   = cb;
    subscription->data = data;

    return subscription;
}

static inline void subscription_free(subscription_t *subscription)
{
    if (subscription && (0 == --subscription->ref)) {
        free(subscription->topic);
        free(subscription);
    }
}

static inline subscription_t *subscription_ref(subscription_t *subscription)
{
    ++subscription->ref;
    return subscription;
}

static inline void subscriptions_free(subscription_t *subscriptions)
{
    subscription_t *sub = NULL, *tmp = NULL;
    HASH_ITER(hh, subscriptions, sub, tmp)
    {
        HASH_DEL(subscriptions, sub);
        subscription_free(sub);
    }
}

static int resub_cb(void *data)
{
    neu_mqtt_client_t *client = data;
    subscription_t    *sub    = NULL;

    nng_mtx_lock(client->mtx);
    if (client->connected &&
        (client->suback_count != HASH_COUNT(client->subscriptions))) {
        // resend subscribe messages if necessary
        HASH_LOOP(hh, client->subscriptions, sub)
        {
            if (!sub->ack && client_send_sub_msg(client, sub) < 0) {
                log(error, "client_send_sub_msg fail");
            }
        }
    }
    nng_mtx_unlock(client->mtx);

    return 0;
}

static void connect_cb(nng_pipe p, nng_pipe_ev ev, void *arg)
{
    (void) p;
    (void) ev;
    neu_mqtt_client_t              *client = arg;
    neu_mqtt_client_connection_cb_t cb     = NULL;
    void                           *data   = NULL;
    int                             reason = 0;

    nng_pipe_get_int(p, NNG_OPT_MQTT_CONNECT_REASON, &reason);
    log(notice, "mqtt client connected, reason: %d", reason);

    nng_mtx_lock(client->mtx);
    // start receiving
    client_start_recv(client);

    // mark as connected
    client->connected = true;
    cb                = client->connect_cb;
    data              = client->connect_cb_data;
    nng_mtx_unlock(client->mtx);

    if (cb) {
        cb(data);
    }
}

static void disconnect_cb(nng_pipe p, nng_pipe_ev ev, void *arg)
{
    (void) p;
    (void) ev;
    neu_mqtt_client_t              *client = arg;
    subscription_t                 *sub    = NULL;
    neu_mqtt_client_connection_cb_t cb     = NULL;
    void                           *data   = NULL;
    int                             reason = 0;

    nng_pipe_get_int(p, NNG_OPT_MQTT_DISCONNECT_REASON, &reason);
    log(notice, "mqtt client disconnected, reason: %d", reason);

    nng_mtx_lock(client->mtx);
    client->connected = false;
    cb                = client->disconnect_cb;
    data              = client->disconnect_cb_data;
    HASH_LOOP(hh, client->subscriptions, sub)
    {
        sub->ack = false;
    }
    client->suback_count = 0;
    nng_mtx_unlock(client->mtx);

    if (cb) {
        cb(data);
    }
}

static void recv_cb(void *arg)
{
    int                rv           = 0;
    subscription_t    *subscription = arg;
    neu_mqtt_client_t *client       = arg;
    nng_aio           *aio          = client->recv_aio;

    if (0 != (rv = nng_aio_result(aio))) {
        log(error, "mqtt client recv error: %s", nng_strerror(rv));
        if (NNG_EAGAIN == rv) {
            nng_recv_aio(client->sock, aio);
        } else {
            nng_mtx_lock(client->mtx);
            client->receiving = false;
            nng_mtx_unlock(client->mtx);
        }
        return;
    }

    nng_msg *msg = nng_aio_get_msg(aio);
    nng_aio_set_msg(aio, NULL);

    if (NULL == msg) {
        log(error, "recv NULL msg");
        goto end;
    }

    nng_mqtt_packet_type pt = nng_mqtt_msg_get_packet_type(msg);
    if (NNG_MQTT_PUBLISH != pt) {
        log(error, "recv packet type:%d, expect PUBLISH packet", pt);
        goto end;
    }

    uint32_t    payload_len;
    uint8_t    *payload = nng_mqtt_msg_get_publish_payload(msg, &payload_len);
    uint32_t    topic_len;
    const char *topic = nng_mqtt_msg_get_publish_topic(msg, &topic_len);
    uint8_t     qos   = nng_mqtt_msg_get_publish_qos(msg);

    (void) payload;
    log(debug, "recv [%.*s, QoS%d] %" PRIu32 " bytes", (unsigned) topic_len,
        topic, (int) qos, payload_len);

    // find matching subscription
    // NOTE: does not support wildcards
    nng_mtx_lock(client->mtx);
    HASH_FIND(hh, client->subscriptions, topic, topic_len, subscription);
    if (NULL != subscription) {
        task_t *task = client_alloc_task(client);
        if (NULL != task) {
            task->kind     = TASK_RECV;
            task->recv.sub = subscription_ref(subscription);
            nng_aio_set_msg(task->aio, msg);
            msg = NULL;
            nng_aio_finish(task->aio, 0);
        } else {
            log(error, "client_alloc_task fail");
        }
    } else {
        log(warn, "[%.*s] no subscription found", (unsigned) topic_len, topic);
    }
    nng_mtx_unlock(client->mtx);

end:
    nng_msg_free(msg);
    nng_recv_aio(client->sock, aio);
}

static inline task_t *client_alloc_task(neu_mqtt_client_t *client)
{
    task_t *task = client->task_free_list;

    if (NULL != task) {
        DL_DELETE(client->task_free_list, task);
        // may try to reduce task queue length adaptive to workload
        return task;
    }

    if (client->task_count >= client->task_limit) {
        log(warn, "reach task limit: %zu", client->task_count);
        return NULL;
    }

    task = task_new(client);
    if (NULL == task) {
        log(error, "task_new fail");
        return NULL;
    }

    ++client->task_count;

    // the task queue length should be a small number due to reuse
    // log info every time the task queue length increments by 32
    if (0 == (client->task_count & (32 - 1))) {
        log(debug, "task queue length: %zu", client->task_count);
    }

    return task;
}

static inline void client_free_task(neu_mqtt_client_t *client, task_t *task)
{
    nng_aio_set_msg(task->aio, NULL);

    if (TASK_SUB == task->kind || TASK_UNSUB == task->kind) {
        subscription_free(task->sub);
    } else if (TASK_RECV == task->kind) {
        subscription_free(task->recv.sub);
    }

    memset(&task->pub, 0, sizeof(task_union));
    // prepend in the hope to help locality
    DL_PREPEND(client->task_free_list, task);
}

static inline size_t client_task_free_list_len(neu_mqtt_client_t *client)
{
    size_t  count = 0;
    task_t *elt   = NULL;
    // count by iterating the list,
    // this won't be a problem as a short free list is expected
    DL_COUNT(client->task_free_list, elt, count);
    return count;
}

static inline void client_add_subscription(neu_mqtt_client_t *client,
                                           subscription_t    *sub)
{
    subscription_t *old = NULL;

    HASH_FIND_STR(client->subscriptions, sub->topic, old);
    if (old) {
        HASH_DEL(client->subscriptions, old);
        subscription_free(old);
    }
    HASH_ADD_STR(client->subscriptions, topic, sub);
}

static inline void client_del_subscription(neu_mqtt_client_t *client,
                                           subscription_t    *sub)
{
    HASH_DEL(client->subscriptions, sub);
    if (sub->ack) {
        client->suback_count -= 1;
    }
    subscription_free(sub);
}

static int client_send_sub_msg(neu_mqtt_client_t *client,
                               subscription_t    *subscription)
{
    int      rv      = 0;
    nng_msg *sub_msg = NULL;

    if ((rv = nng_mqtt_msg_alloc(&sub_msg, 0)) != 0) {
        log(error, "nng_mqtt_msg_alloc fail: %s", nng_strerror(rv));
        return -1;
    }

    nng_mqtt_topic_qos topic_qos[] = {
        { .qos   = subscription->qos,
          .topic = { .buf    = (uint8_t *) subscription->topic,
                     .length = strlen(subscription->topic) } },
    };

    size_t topic_qos_count = sizeof(topic_qos) / sizeof(nng_mqtt_topic_qos);

    nng_mqtt_msg_set_packet_type(sub_msg, NNG_MQTT_SUBSCRIBE);
    nng_mqtt_msg_set_subscribe_topics(sub_msg, topic_qos, topic_qos_count);

    task_t *task = client_alloc_task(client);
    if (NULL == task) {
        log(error, "client_alloc_task fail");
        nng_msg_free(sub_msg);
        return -1;
    }

    task->kind = TASK_SUB;
    task->sub  = subscription_ref(subscription);
    nng_aio_set_msg(task->aio, sub_msg);
    nng_send_aio(client->sock, task->aio);

    return 0;
}

static int client_send_unsub_msg(neu_mqtt_client_t *client,
                                 subscription_t    *subscription)
{
    int      rv      = 0;
    nng_msg *sub_msg = NULL;

    if ((rv = nng_mqtt_msg_alloc(&sub_msg, 0)) != 0) {
        log(error, "nng_mqtt_msg_alloc fail: %s", nng_strerror(rv));
        return -1;
    }

    nng_mqtt_topic mqtt_topic = { .buf    = (uint8_t *) subscription->topic,
                                  .length = strlen(subscription->topic) };

    nng_mqtt_msg_set_packet_type(sub_msg, NNG_MQTT_UNSUBSCRIBE);
    nng_mqtt_msg_set_unsubscribe_topics(sub_msg, &mqtt_topic, 1);

    task_t *task = client_alloc_task(client);
    if (NULL == task) {
        log(error, "client_alloc_task fail");
        nng_msg_free(sub_msg);
        return -1;
    }

    task->kind = TASK_UNSUB;
    task->sub  = subscription ? subscription_ref(subscription) : NULL;
    nng_aio_set_msg(task->aio, sub_msg);
    nng_send_aio(client->sock, task->aio);

    return 0;
}

static inline void client_start_recv(neu_mqtt_client_t *client)
{
    if (client->recv_aio && !client->receiving) {
        nng_recv_aio(client->sock, client->recv_aio);
        client->receiving = true;
    }
}

static inline int client_start_timer(neu_mqtt_client_t *client)
{
    neu_events_t      *events = NULL;
    neu_event_timer_t *timer  = NULL;

    if (client->events) {
        // timer already started
        return 0;
    }

    events = neu_event_new();
    if (NULL == events) {
        return -1;
    }

    neu_event_timer_param_t param = {
        .second      = 1,
        .millisecond = 500,
        .cb          = resub_cb,
        .usr_data    = client,
    };

    timer = neu_event_add_timer(events, param);
    if (NULL == timer) {
        neu_event_close(events);
        return -1;
    }

    client->events = events;
    client->timer  = timer;
    return 0;
}

static inline int client_make_url(neu_mqtt_client_t *client)
{
    char       *url = NULL;
    const char *fmt = NULL;

    if (client->tls_cfg) {
        fmt = "tls+mqtt-tcp://%s:%" PRIu16;
    } else {
        fmt = "mqtt-tcp://%s:%" PRIu16;
    }

    // cppcheck false alarm
    // cppcheck-suppress wrongPrintfScanfArgNum
    size_t n = snprintf(NULL, 0, fmt, client->host, client->port);
    url      = realloc(client->url, n + 1);
    if (NULL == url) {
        log(error, "neu_asprintf address url fail");
        return -1;
    }

    snprintf(url, n + 1, fmt, client->host, client->port);
    client->url = url;
    return 0;
}

static inline nng_msg *alloc_conn_msg(neu_mqtt_client_t *client,
                                      neu_mqtt_version_e version)
{
    int      rv  = 0;
    nng_msg *msg = NULL;

    if (0 != (rv = nng_mqtt_msg_alloc(&msg, 0))) {
        log(error, "nng_mqtt_msg_alloc fail: %s", nng_strerror(rv));
        return NULL;
    }

    nng_mqtt_msg_set_packet_type(msg, NNG_MQTT_CONNECT);
    nng_mqtt_msg_set_connect_keep_alive(msg, 60);
    nng_mqtt_msg_set_connect_proto_version(
        msg, neu_mqtt_version_to_nng_mqtt_version(version));
    nng_mqtt_msg_set_connect_clean_session(msg, true);

    return msg;
}

static inline nng_tls_config *alloc_tls_config(neu_mqtt_client_t *client)
{
    int             rv  = 0;
    nng_tls_config *cfg = NULL;

    if ((rv = nng_tls_config_alloc(&cfg, NNG_TLS_MODE_CLIENT)) != 0) {
        log(error, "nng_tls_config_alloc fail: %s", nng_strerror(rv));
        return NULL;
    }

    return cfg;
}

static inline nng_mqtt_sqlite_option *
alloc_sqlite_config(neu_mqtt_client_t *client)
{
    int                     rv;
    char                   *db  = NULL;
    nng_mqtt_sqlite_option *cfg = NULL;
    const mqtt_buf          client_id =
        nng_mqtt_msg_get_connect_client_id(client->conn_msg);

    if (NULL == client_id.buf || 0 == client_id.length) {
        log(error, "nng_mqtt_msg_get_connect_client_id fail");
        return NULL;
    }

    neu_asprintf(&db, "neuron-mqtt-client-%.*s.db", (unsigned) client_id.length,
                 (char *) client_id.buf);
    if (NULL == db) {
        log(error, "neu_asprintf nano-mqtt cilent db fail");
        return NULL;
    }

    if ((rv = nng_mqtt_alloc_sqlite_opt(&cfg)) != 0) {
        log(error, "nng_mqtt_alloc_sqlite_opt fail: %s", nng_strerror(rv));
        free(db);
        return NULL;
    }

    nng_mqtt_set_sqlite_enable(cfg, true);
    nng_mqtt_set_sqlite_flush_threshold(cfg, 256);
    nng_mqtt_set_sqlite_max_rows(cfg, 256);
    nng_mqtt_set_sqlite_db_dir(cfg, "persistence/");
    nng_mqtt_sqlite_db_init(cfg, db, client->version);

    free(db);
    return cfg;
}

/**
 * @brief 创建新的MQTT客户端对象
 *
 * @param version MQTT协议版本
 * @return 成功返回MQTT客户端对象指针，失败返回NULL
 */
neu_mqtt_client_t *neu_mqtt_client_new(neu_mqtt_version_e version)
{
    /* 分配客户端对象内存并初始化为0 */
    neu_mqtt_client_t *client = calloc(1, sizeof(*client));
    if (NULL == client) {
        return NULL;
    }

    /* 创建互斥锁 */
    if (0 != nng_mtx_alloc(&client->mtx)) {
        free(client);
        return NULL;
    }

    /* 创建连接消息 */
    client->conn_msg = alloc_conn_msg(client, version);
    if (NULL == client->conn_msg) {
        nng_mtx_free(client->mtx);
        free(client);
        return NULL;
    }

    /* 设置客户端参数默认值 */
    client->version    = version;
    client->retry      = NEU_MQTT_CACHE_SYNC_INTERVAL_DEFAULT;
    client->task_limit = 1024;

    return client;
}

/**
 * @brief 从服务器地址和端口创建MQTT客户端
 *
 * @param host 服务器主机名或IP地址
 * @param port 服务器端口
 * @param version MQTT协议版本
 * @return 成功返回MQTT客户端对象指针，失败返回NULL
 */
neu_mqtt_client_t *neu_mqtt_client_from_addr(const char *host, uint16_t port,
                                             neu_mqtt_version_e version)
{
    /* 创建新的MQTT客户端 */
    neu_mqtt_client_t *client = neu_mqtt_client_new(version);
    if (NULL == client) {
        return NULL;
    }

    /* 设置服务器地址 */
    if (0 != neu_mqtt_client_set_addr(client, host, port)) {
        neu_mqtt_client_free(client);
        return NULL;
    }

    return client;
}

/**
 * @brief 释放MQTT客户端对象资源
 *
 * 释放所有分配的资源，包括TLS配置、SQLite配置、订阅、任务等
 *
 * @param client MQTT客户端对象指针
 */
void neu_mqtt_client_free(neu_mqtt_client_t *client)
{
    if (client) {
        /* 释放TLS配置 */
        if (client->tls_cfg) {
            nng_tls_config_free(client->tls_cfg);
        }
        /* 释放SQLite持久化配置 */
        if (client->sqlite_cfg) {
            nng_mqtt_free_sqlite_opt(client->sqlite_cfg);
        }
        /* 释放接收异步I/O */
        nng_aio_free(client->recv_aio);
        /* 释放所有订阅 */
        subscriptions_free(client->subscriptions);
        /* 释放任务空闲列表 */
        tasks_free(client->task_free_list);
        /* 释放连接消息 */
        nng_msg_free(client->conn_msg);
        /* 释放URL和主机名 */
        free(client->url);
        free(client->host);
        /* 释放互斥锁 */
        nng_mtx_free(client->mtx);
        /* 释放客户端对象本身 */
        free(client);
    }
}

/**
 * @brief 检查MQTT客户端是否已打开
 *
 * @param client MQTT客户端对象指针
 * @return 如果客户端已打开返回true，否则返回false
 */
bool neu_mqtt_client_is_open(neu_mqtt_client_t *client)
{
    bool open = false;

    /* 线程安全地访问客户端状态 */
    nng_mtx_lock(client->mtx);
    open = client->open;
    nng_mtx_unlock(client->mtx);

    return open;
}

bool neu_mqtt_client_is_connected(neu_mqtt_client_t *client)
{
    bool connected = false;

    nng_mtx_lock(client->mtx);
    connected = client->connected;
    nng_mtx_unlock(client->mtx);

    return connected;
}

size_t neu_mqtt_client_get_cached_msgs_num(neu_mqtt_client_t *client)
{
    size_t num = 0;

    nng_mtx_lock(client->mtx);
    if (NULL != client->sqlite_cfg) {
        num = nng_mqtt_sqlite_db_get_cached_size(client->sqlite_cfg);
    }
    nng_mtx_unlock(client->mtx);

    return num;
}

int neu_mqtt_client_set_addr(neu_mqtt_client_t *client, const char *host,
                             uint16_t port)
{
    int rv = 0;

    nng_mtx_lock(client->mtx);
    return_failure_if_open();

    if (client->host && 0 == strcmp(client->host, host)) {
        client->port = port;
        goto end;
    }

    char *h = strdup(host);
    if (NULL == h) {
        log(error, "strdup host fail");
        rv = -1;
        goto end;
    }

    free(client->host);
    client->host = h;
    client->port = port;

end:
    nng_mtx_unlock(client->mtx);
    return rv;
}

int neu_mqtt_client_set_id(neu_mqtt_client_t *client, const char *id)
{
    if (NULL == id || 0 == strlen(id)) {
        return -1;
    }

    nng_mtx_lock(client->mtx);
    return_failure_if_open();

    nng_mqtt_msg_set_connect_client_id(client->conn_msg, id);
    nng_mtx_unlock(client->mtx);

    return 0;
}

int neu_mqtt_client_set_user(neu_mqtt_client_t *client, const char *username,
                             const char *password)
{
    nng_mtx_lock(client->mtx);
    return_failure_if_open();

    nng_mqtt_msg_set_connect_user_name(client->conn_msg, username);
    nng_mqtt_msg_set_connect_password(client->conn_msg, password);
    nng_mtx_unlock(client->mtx);

    return 0;
}

int neu_mqtt_client_set_connect_cb(neu_mqtt_client_t              *client,
                                   neu_mqtt_client_connection_cb_t cb,
                                   void                           *data)
{
    nng_mtx_lock(client->mtx);
    return_failure_if_open();

    client->connect_cb      = cb;
    client->connect_cb_data = data;
    nng_mtx_unlock(client->mtx);

    return 0;
}

int neu_mqtt_client_set_disconnect_cb(neu_mqtt_client_t              *client,
                                      neu_mqtt_client_connection_cb_t cb,
                                      void                           *data)
{
    nng_mtx_lock(client->mtx);
    return_failure_if_open();

    client->disconnect_cb      = cb;
    client->disconnect_cb_data = data;
    nng_mtx_unlock(client->mtx);

    return 0;
}

int neu_mqtt_client_set_tls(neu_mqtt_client_t *client, bool enabled,
                            const char *ca, const char *cert, const char *key,
                            const char *keypass)
{
    int             rv  = 0;
    nng_tls_config *cfg = NULL;

    nng_mtx_lock(client->mtx);
    return_failure_if_open();

    if (!enabled) {
        // disable tls
        log(debug, "tls disabled");
        if (client->tls_cfg) {
            nng_tls_config_free(client->tls_cfg);
            client->tls_cfg = NULL;
        }
        goto end;
    }

    if (NULL == client->host) {
        log(error, "no client host");
        rv = -1;
        goto end;
    }

    if (client->tls_cfg) {
        cfg = client->tls_cfg;
    } else {
        cfg = alloc_tls_config(client);
        if (NULL == cfg) {
            log(error, "alloc_tls_config fail");
            rv = -1;
            goto end;
        }
        client->tls_cfg = cfg;
    }

    // validate server name and enable SNI only when host is not an IP address
    struct sockaddr_in sa;
    if (0 == inet_pton(AF_INET, client->host, &sa.sin_addr) &&
        0 != (rv = nng_tls_config_server_name(client->tls_cfg, client->host))) {
        log(error, "nng_tls_config_server_name fail: %s", nng_strerror(rv));
        rv = -1;
        goto end;
    }

    if (cert != NULL && key != NULL) {
        if ((rv = nng_tls_config_auth_mode(cfg, NNG_TLS_AUTH_MODE_REQUIRED)) !=
            0) {
            log(error, "nng_tls_config_auth_mode fail: %s", nng_strerror(rv));
            rv = -1;
            goto end;
        }
        if ((rv = nng_tls_config_own_cert(cfg, cert, key, keypass)) != 0) {
            log(error, "nng_tls_config_own_cert fail: %s", nng_strerror(rv));
            rv = -1;
            goto end;
        }
    } else {
        if ((rv = nng_tls_config_auth_mode(cfg, NNG_TLS_AUTH_MODE_NONE)) != 0) {
            log(error, "nng_tls_config_auth_mode fail: %s", nng_strerror(rv));
            rv = -1;
            goto end;
        }
    }

    if (ca && (rv = nng_tls_config_ca_chain(cfg, ca, NULL)) != 0) {
        log(error, "nng_tls_config_ca_chain fail: %s", nng_strerror(rv));
        rv = -1;
    }

end:
    nng_mtx_unlock(client->mtx);
    return rv;
}

int neu_mqtt_client_set_cache_size(neu_mqtt_client_t *client,
                                   size_t mem_size_bytes, size_t db_size_bytes)
{
    int rv = 0;

    nng_mtx_lock(client->mtx);
    return_failure_if_open();

    if (0 == mem_size_bytes && 0 == db_size_bytes) {
        // disable cache
        log(debug, "cache disabled");
        if (client->sqlite_cfg) {
            nng_mqtt_free_sqlite_opt(client->sqlite_cfg);
            client->sqlite_cfg = NULL;
        }
        goto end;
    }

    if (NULL == client->sqlite_cfg) {
        client->sqlite_cfg = alloc_sqlite_config(client);
        if (NULL == client->sqlite_cfg) {
            log(error, "alloc_sqlite_config fail");
            rv = -1;
            goto end;
        }
    }

    // FIXME: until NanoSDK cache size limit feature out
    // Assume that,
    // 1. every upload message follows the below template
    //    '{"node":"","group":"","timestamp":1669708991767,"values":{"":0},"errors":{}}'
    // 2. every upload message has 50 tags in average
    // 3. every tag has 4 bytes data
    // then, estimated average message size is
    //    len(tmpl) + 128/2 + 128/2 + 50*(64/2+4) ~= 2000
    size_t avg_msg_size = 2000;
    nng_mqtt_set_sqlite_flush_threshold(client->sqlite_cfg,
                                        mem_size_bytes / avg_msg_size);
    nng_mqtt_set_sqlite_max_rows(client->sqlite_cfg,
                                 db_size_bytes / avg_msg_size);

end:
    nng_mtx_unlock(client->mtx);
    return rv;
}

int neu_mqtt_client_set_cache_sync_interval(neu_mqtt_client_t *client,
                                            uint32_t           interval)
{
    int rv = 0;

    nng_mtx_lock(client->mtx);
    return_failure_if_open();

    // sync interval should be within reasonable range
    if (NEU_MQTT_CACHE_SYNC_INTERVAL_MIN <= interval &&
        interval <= NEU_MQTT_CACHE_SYNC_INTERVAL_MAX) {
        client->retry = interval;
    } else {
        rv = -1;
        // client->retry = 100;
        // rv            = 0;
    }

    nng_mtx_unlock(client->mtx);
    return rv;
}

int neu_mqtt_client_set_zlog_category(neu_mqtt_client_t *client,
                                      zlog_category_t   *cat)
{
    nng_mtx_lock(client->mtx);
    return_failure_if_open();

    client->log = cat;
    nng_mtx_unlock(client->mtx);

    return 0;
}

int neu_mqtt_client_open(neu_mqtt_client_t *client)
{
    int rv = 0;

    nng_mtx_lock(client->mtx);
    if (client->open) {
        nng_mtx_unlock(client->mtx);
        return 0;
    }

    if (NEU_MQTT_VERSION_V5 == client->version &&
        (rv = nng_mqttv5_client_open(&client->sock)) != 0) {
        log(error, "nng_mqttv5_client_open fail: %s", nng_strerror(rv));
        nng_mtx_unlock(client->mtx);
        return -1;
    } else if ((rv = nng_mqtt_client_open(&client->sock)) != 0) {
        log(error, "nng_mqtt_client_open fail: %s", nng_strerror(rv));
        nng_mtx_unlock(client->mtx);
        return -1;
    }

    if (0 != client_make_url(client)) {
        log(error, "client_make_url fail");
        goto error;
    }

    if (0 != client_start_timer(client)) {
        log(error, "client_start_timer fail");
        goto error;
    }

    if ((rv = nng_socket_set_ms(client->sock, NNG_OPT_MQTT_RETRY_INTERVAL,
                                client->retry)) != 0) {
        log(error, "nng_socket_set_ms(%" PRIu32 ") fail: %s", client->retry,
            nng_strerror(rv));
        goto error;
    }

    if (client->sqlite_cfg &&
        (rv = nng_socket_set_ptr(client->sock, NNG_OPT_MQTT_SQLITE,
                                 client->sqlite_cfg))) {
        log(error, "nng_socket_set_ptr(NNG_OPT_MQTT_SQLITE) fail: %s",
            nng_strerror(rv));
        goto error;
    }

    nng_dialer dialer;
    if ((rv = nng_dialer_create(&dialer, client->sock, client->url)) != 0) {
        log(error, "nng_dialer_create fail: %s", nng_strerror(rv));
        goto error;
    }

    if (client->tls_cfg &&
        0 !=
            (rv = nng_dialer_set_ptr(dialer, NNG_OPT_TLS_CONFIG,
                                     client->tls_cfg))) {
        log(error, "nng_dialter_set_ptr(NNG_OPT_TLS_CONFIG) fail: %s",
            nng_strerror(rv));
        goto error;
    }

    nng_mqtt_set_connect_cb(client->sock, connect_cb, client);
    nng_mqtt_set_disconnect_cb(client->sock, disconnect_cb, client);

    if ((rv = nng_dialer_set_ptr(dialer, NNG_OPT_MQTT_CONNMSG,
                                 client->conn_msg)) != 0) {
        log(error, "nng_dialer_set_ptr fail: %s", nng_strerror(rv));
        goto error;
    }

    if ((rv = nng_dialer_start(dialer, NNG_FLAG_NONBLOCK)) != 0) {
        log(error, "nng_dialer_start fail: %s", nng_strerror(rv));
        goto error;
    }

    client->open = true;
    nng_mtx_unlock(client->mtx);

    return 0;

error:
    nng_mtx_unlock(client->mtx);
    if (client->events) {
        neu_event_del_timer(client->events, client->timer);
        neu_event_close(client->events);
        client->events = NULL;
        client->timer  = NULL;
    }
    nng_close(client->sock);
    return -1;
}

int neu_mqtt_client_close(neu_mqtt_client_t *client)
{
    int                rv     = 0;
    neu_events_t      *events = NULL;
    neu_event_timer_t *timer  = NULL;

    nng_mtx_lock(client->mtx);
    if (!client->open) {
        nng_mtx_unlock(client->mtx);
        return 0;
    }
    events         = client->events;
    timer          = client->timer;
    client->events = NULL;
    client->timer  = NULL;
    nng_mtx_unlock(client->mtx);

    if (events) {
        neu_event_del_timer(events, timer);
        neu_event_close(events);
    }

    // NanoSDK quirks: calling nng_aio_stop will block if the aio is in use
    // nng_aio_stop(client->recv_aio);

    rv = nng_close(client->sock);
    if (0 != rv) {
        log(error, "nng_close: %s", nng_strerror(rv));
        return -1;
    }

    nng_mtx_lock(client->mtx);
    // wait for all tasks
    while (client_task_free_list_len(client) != client->task_count) {
        nng_mtx_unlock(client->mtx);
        neu_msleep(100);
        nng_mtx_lock(client->mtx);
    }
    client->open = false;
    nng_mtx_unlock(client->mtx);

    return 0;
}

/**
 * @brief 发布MQTT消息
 *
 * @param client MQTT客户端对象指针
 * @param qos 服务质量等级
 * @param topic 发布的主题
 * @param payload 消息负载数据
 * @param len 负载数据长度
 * @param data 用户自定义数据，会传递给回调函数
 * @param cb 发布完成回调函数
 * @return 成功返回0，失败返回-1
 */
int neu_mqtt_client_publish(neu_mqtt_client_t *client, neu_mqtt_qos_e qos,
                            char *topic, uint8_t *payload, uint32_t len,
                            void *data, neu_mqtt_client_publish_cb_t cb)
{
    int      rv      = 0;
    nng_msg *pub_msg = NULL;
    task_t  *task    = NULL;

    /* 分配MQTT消息 */
    if (0 != (rv = nng_mqtt_msg_alloc(&pub_msg, 0))) {
        log(error, "nng_mqtt_msg_alloc fail: %s", nng_strerror(rv));
        return -1;
    }

    if (0 != (rv = nng_mqtt_msg_set_publish_topic(pub_msg, topic))) {
        nng_msg_free(pub_msg);
        log(error, "nng_mqtt_msg_set_publish_topic fail: %s", nng_strerror(rv));
        return -1;
    }

    nng_mqtt_msg_set_packet_type(pub_msg, NNG_MQTT_PUBLISH);
    nng_mqtt_msg_set_publish_payload(pub_msg, (uint8_t *) payload, len);
    nng_mqtt_msg_set_publish_qos(pub_msg, qos);

    nng_mtx_lock(client->mtx);
    task = client_alloc_task(client);
    nng_mtx_unlock(client->mtx);

    if (NULL == task) {
        nng_msg_free(pub_msg);
        log(error, "client_alloc_task fail");
        return -1;
    }

    task->kind        = TASK_PUB;
    task->pub.cb      = cb;
    task->pub.qos     = qos;
    task->pub.topic   = topic;
    task->pub.payload = payload;
    task->pub.len     = len;
    task->pub.data    = data;
    nng_aio_set_msg(task->aio, pub_msg);
    nng_send_aio(client->sock, task->aio);

    return 0;
}

int neu_mqtt_client_subscribe(neu_mqtt_client_t *client, neu_mqtt_qos_e qos,
                              const char *topic, void *data,
                              neu_mqtt_client_subscribe_cb_t cb)
{
    int             rv           = 0;
    subscription_t *subscription = NULL;

    nng_mtx_lock(client->mtx);
    if (NULL == client->recv_aio) {
        if ((rv = nng_aio_alloc(&client->recv_aio, recv_cb, client)) != 0) {
            log(error, "nng_aio_alloc fail: %s", nng_strerror(rv));
            goto error;
        }
        if (client->connected) {
            // connect_cb already fired, then start receiving
            client_start_recv(client);
        }
    }

    subscription = subscription_new(client, qos, topic, cb, data);
    if (NULL == subscription) {
        log(error, "subscription_new fail");
        goto error;
    }

    if (client->connected) {
        // connect_cb already fired, then send a subscribe message
        if (client_send_sub_msg(client, subscription) < 0) {
            log(error, "client_send_sub_msg fail");
            goto error;
        }
    }

    client_add_subscription(client, subscription);
    nng_mtx_unlock(client->mtx);

    return 0;

error:
    nng_mtx_unlock(client->mtx);
    subscription_free(subscription);
    return -1;
}

int neu_mqtt_client_unsubscribe(neu_mqtt_client_t *client, const char *topic)
{
    int             rv           = 0;
    subscription_t *subscription = NULL;

    nng_mtx_lock(client->mtx);
    if (NULL == client->recv_aio) {
        if ((rv = nng_aio_alloc(&client->recv_aio, recv_cb, client)) != 0) {
            log(error, "nng_aio_alloc fail: %s", nng_strerror(rv));
            goto error;
        }
        if (client->connected) {
            // connect_cb already fired, then start receiving
            client_start_recv(client);
        }
    }

    HASH_FIND_STR(client->subscriptions, topic, subscription);

    if (client->connected) {
        // send a unsubscribe message even subscription is NULL
        if (client_send_unsub_msg(client, subscription) < 0) {
            log(error, "client_send_unsub_msg fail");
            goto error;
        }
    }

    if (subscription) {
        client_del_subscription(client, subscription);
    }

    nng_mtx_unlock(client->mtx);

    return 0;

error:
    nng_mtx_unlock(client->mtx);
    return -1;
}
