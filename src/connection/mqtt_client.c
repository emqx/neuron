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
#include "errcodes.h"
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

typedef struct {
    size_t                         ref;
    bool                           ack;
    neu_mqtt_qos_e                 qos;
    char *                         topic;
    neu_mqtt_client_subscribe_cb_t cb;
    void *                         data;
    UT_hash_handle                 hh;
} subscription_t;

typedef enum {
    TASK_PUB,
    TASK_SUB,
    TASK_UNSUB,
    TASK_RECV,
} task_kind_e;

#define TASK_UNION_FIELDS                     \
    struct {                                  \
        neu_mqtt_client_publish_cb_t cb;      \
        neu_mqtt_qos_e               qos;     \
        char *                       topic;   \
        uint8_t *                    payload; \
        uint32_t                     len;     \
        void *                       data;    \
    } pub;                                    \
    subscription_t *sub;                      \
    struct {                                  \
        subscription_t *sub;                  \
    } recv

typedef union {
    TASK_UNION_FIELDS;
} task_union;

typedef struct task_s {
    task_kind_e kind;
    nng_aio *   aio;
    union {
        TASK_UNION_FIELDS;
    };
    struct task_s *prev;
    struct task_s *next;
} task_t;

struct neu_mqtt_client_s {
    nng_socket                      sock;
    nng_mtx *                       mtx;
    neu_events_t *                  events;
    neu_event_timer_t *             timer;
    neu_mqtt_version_e              version;
    char *                          host;
    uint16_t                        port;
    char *                          url;
    nng_tls_config *                tls_cfg;
    nng_msg *                       conn_msg;
    nng_duration                    retry;
    bool                            open;
    bool                            connected;
    neu_mqtt_client_connection_cb_t connect_cb;
    void *                          connect_cb_data;
    neu_mqtt_client_connection_cb_t disconnect_cb;
    void *                          disconnect_cb_data;
    nng_mqtt_sqlite_option *        sqlite_cfg;
    char *                          db;
    bool                            receiving;
    nng_aio *                       recv_aio;
    subscription_t *                subscriptions;
    size_t                          suback_count;
    size_t                          task_count;
    size_t                          task_limit;
    task_t *                        task_free_list;
    zlog_category_t *               log;
};

static inline task_t *task_new(neu_mqtt_client_t *client);
static inline void    task_free(task_t *task);
static inline void    tasks_free(task_t *tasks);
static void           task_cb(void *arg);
static void           task_handle_pub(task_t *task, neu_mqtt_client_t *client);
static void           task_handle_sub(task_t *task, neu_mqtt_client_t *client);
static void task_handle_unsub(task_t *task, neu_mqtt_client_t *client);
static void task_handle_recv(task_t *task, neu_mqtt_client_t *client);

static subscription_t *       subscription_new(neu_mqtt_client_t *client,
                                               neu_mqtt_qos_e qos, const char *topic,
                                               neu_mqtt_client_subscribe_cb_t cb,
                                               void *                         data);
static inline void            subscription_free(subscription_t *subscription);
static inline subscription_t *subscription_ref(subscription_t *subscription);
static inline void            subscriptions_free(subscription_t *subscriptions);

static void recv_cb(void *arg);
static int  resub_cb(void *data);
static void disconnect_cb(nng_pipe p, nng_pipe_ev ev, void *arg);
static void connect_cb(nng_pipe p, nng_pipe_ev ev, void *arg);

static inline task_t *client_alloc_task(neu_mqtt_client_t *client);
static inline void    client_free_task(neu_mqtt_client_t *client, task_t *task);
static inline size_t  client_task_free_list_len(neu_mqtt_client_t *client);
static inline void    client_add_subscription(neu_mqtt_client_t *client,
                                              subscription_t *   sub);
static int            client_send_sub_msg(neu_mqtt_client_t *client,
                                          subscription_t *   subscription);
static inline void    client_start_recv(neu_mqtt_client_t *client);
static inline int     client_start_timer(neu_mqtt_client_t *client);
static inline int     client_make_url(neu_mqtt_client_t *client);

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

bool neu_mqtt_topic_filter_is_valid(const char *topic_filter)
{
    if (NULL == topic_filter || '\0' == *topic_filter ||
        strlen(topic_filter) > 65535) {
        return false;
    }

    char c;
    while ((c = *topic_filter++)) {
        switch (c) {
        case '#':
            // '#' must be last character
            return '\0' == *topic_filter;
        case '+':
            // '+' must occupy an entire level
            if (*topic_filter && '/' != *topic_filter) {
                return false;
            }
            break;
        default:
            for (; '/' != c; c = *topic_filter++) {
                if ('#' == c || '+' == c) {
                    return false;
                } else if ('\0' == c) {
                    return true;
                }
            }
        }
    }

    return true;
}

static bool topic_filter_is_match(const char *topic_filter,
                                  const char *topic_name,
                                  uint32_t    topic_name_len)
{
    // The Server MUST NOT match Topic Filters starting with a wildcard
    // character (# or +) with Topic Names beginning with a $ character
    // [MQTT-4.7.2-1].
    if ('$' == topic_name[0]) {
        if ('$' != topic_filter[0]) {
            return false;
        }
    } else if (0 == strcmp("#", topic_filter)) {
        return true;
    }

    uint32_t i = 0;
    while (true) {
        switch (*topic_filter) {
        case '#':
            return true;
        case '+':
            while (i < topic_name_len && '/' != topic_name[i]) {
                ++i;
            };
            ++topic_filter;
            break;
        case '/':
            if (i < topic_name_len && '/' == topic_name[i]) {
                ++i;
            } else if ('#' != topic_filter[1]) { // '#' include parent level
                return false;
            }
            ++topic_filter;
            break;
        case '\0':
            return i == topic_name_len;
        default:
            for (; *topic_filter && '/' != *topic_filter; ++topic_filter, ++i) {
                if (!(i < topic_name_len && *topic_filter == topic_name[i])) {
                    return false;
                }
            }
        }
    }

    return true;
}

bool neu_mqtt_topic_filter_is_match(const char *topic_filter,
                                    const char *topic_name)
{
    return topic_filter_is_match(topic_filter, topic_name, strlen(topic_name));
}

static inline task_t *task_new(neu_mqtt_client_t *client)
{
    task_t *task = calloc(1, sizeof(*task));
    if (NULL == task) {
        log(error, "calloc task fail");
        return NULL;
    }

    int rv = 0;
    if ((rv = nng_aio_alloc(&task->aio, task_cb, task)) != 0) {
        log(error, "nng_aio_alloc fail: %s", nng_strerror(rv));
        free(task);
        return NULL;
    }

    nng_aio_set_input(task->aio, 0, client);

    return task;
}

static inline void task_free(task_t *task)
{
    // NanoSDK quirks: calling nng_aio_stop will block if the aio is in use
    // nng_aio_stop(task->aio);
    nng_aio_free(task->aio);
    free(task);
}

static inline void tasks_free(task_t *tasks)
{
    task_t *task = NULL, *tmp = NULL;
    DL_FOREACH_SAFE(tasks, task, tmp)
    {
        DL_DELETE(tasks, task);
        task_free(task);
    }
}

static void task_cb(void *arg)
{
    task_t *           task   = arg;
    nng_aio *          aio    = task->aio;
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
                                        void *                         data)
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

static inline subscription_t *
subscription_find_match(subscription_t *subscriptions, const char *topic_name,
                        uint32_t topic_name_len)
{
    subscription_t *sub = NULL, *tmp = NULL;
    HASH_FIND(hh, subscriptions, topic_name, topic_name_len, sub);
    if (NULL == sub) {
        // subscription wildcard matching
        // TODO: linear scanning is enough for current usages,
        //       but may pose performance issues in the future
        HASH_ITER(hh, subscriptions, sub, tmp)
        {
            if (topic_filter_is_match(sub->topic, topic_name, topic_name_len)) {
                return sub;
            }
        }
    }
    return sub;
}

static int resub_cb(void *data)
{
    neu_mqtt_client_t *client = data;
    subscription_t *   sub    = NULL;

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
    neu_mqtt_client_t *             client = arg;
    neu_mqtt_client_connection_cb_t cb     = NULL;
    void *                          data   = NULL;
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
    neu_mqtt_client_t *             client = arg;
    subscription_t *                sub    = NULL;
    neu_mqtt_client_connection_cb_t cb     = NULL;
    void *                          data   = NULL;
    int                             reason = 0;

    nng_pipe_get_int(p, NNG_OPT_MQTT_DISCONNECT_REASON, &reason);
    log(notice, "mqtt client disconnected, reason: %d", reason);

    nng_mtx_lock(client->mtx);
    client->connected = false;
    cb                = client->disconnect_cb;
    data              = client->disconnect_cb_data;
    HASH_LOOP(hh, client->subscriptions, sub) { sub->ack = false; }
    client->suback_count = 0;
    nng_mtx_unlock(client->mtx);

    if (cb) {
        cb(data);
    }
}

static void recv_cb(void *arg)
{
    int                rv           = 0;
    subscription_t *   subscription = arg;
    neu_mqtt_client_t *client       = arg;
    nng_aio *          aio          = client->recv_aio;

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
    uint8_t *   payload = nng_mqtt_msg_get_publish_payload(msg, &payload_len);
    uint32_t    topic_len;
    const char *topic = nng_mqtt_msg_get_publish_topic(msg, &topic_len);
    uint8_t     qos   = nng_mqtt_msg_get_publish_qos(msg);

    (void) payload;
    log(debug, "recv [%.*s, QoS%d] %" PRIu32 " bytes", (unsigned) topic_len,
        topic, (int) qos, payload_len);

    nng_mtx_lock(client->mtx);
    subscription =
        subscription_find_match(client->subscriptions, topic, topic_len);
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
                                           subscription_t *   sub)
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
                                           subscription_t *   sub)
{
    HASH_DEL(client->subscriptions, sub);
    if (sub->ack) {
        client->suback_count -= 1;
    }
    subscription_free(sub);
}

static int client_send_sub_msg(neu_mqtt_client_t *client,
                               subscription_t *   subscription)
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
                                 subscription_t *   subscription)
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
    neu_events_t *     events = NULL;
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
    char *      url = NULL;
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
    nng_mqtt_sqlite_option *cfg = NULL;

    if ((rv = nng_mqtt_alloc_sqlite_opt(&cfg)) != 0) {
        log(error, "nng_mqtt_alloc_sqlite_opt fail: %s", nng_strerror(rv));
        return NULL;
    }

    nng_mqtt_set_sqlite_enable(cfg, true);
    nng_mqtt_set_sqlite_flush_threshold(cfg, 256);
    nng_mqtt_set_sqlite_max_rows(cfg, 256);
    nng_mqtt_set_sqlite_db_dir(cfg, "persistence/");

    return cfg;
}

neu_mqtt_client_t *neu_mqtt_client_new(neu_mqtt_version_e version)
{
    neu_mqtt_client_t *client = calloc(1, sizeof(*client));
    if (NULL == client) {
        return NULL;
    }

    if (0 != nng_mtx_alloc(&client->mtx)) {
        free(client);
        return NULL;
    }

    client->conn_msg = alloc_conn_msg(client, version);
    if (NULL == client->conn_msg) {
        nng_mtx_free(client->mtx);
        free(client);
        return NULL;
    }

    client->version    = version;
    client->retry      = NEU_MQTT_CACHE_SYNC_INTERVAL_DEFAULT;
    client->task_limit = 1024;

    return client;
}

neu_mqtt_client_t *neu_mqtt_client_from_addr(const char *host, uint16_t port,
                                             neu_mqtt_version_e version)
{
    neu_mqtt_client_t *client = neu_mqtt_client_new(version);
    if (NULL == client) {
        return NULL;
    }

    if (0 != neu_mqtt_client_set_addr(client, host, port)) {
        neu_mqtt_client_free(client);
        return NULL;
    }

    return client;
}

void neu_mqtt_client_free(neu_mqtt_client_t *client)
{
    if (client) {
        if (client->tls_cfg) {
            nng_tls_config_free(client->tls_cfg);
        }
        if (client->sqlite_cfg) {
            nng_mqtt_free_sqlite_opt(client->sqlite_cfg);
        }
        nng_aio_free(client->recv_aio);
        subscriptions_free(client->subscriptions);
        tasks_free(client->task_free_list);
        nng_msg_free(client->conn_msg);
        free(client->db);
        free(client->url);
        free(client->host);
        nng_mtx_free(client->mtx);
        free(client);
    }
}

bool neu_mqtt_client_is_open(neu_mqtt_client_t *client)
{
    bool open = false;

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

int neu_mqtt_client_set_will_msg(neu_mqtt_client_t *client, const char *topic,
                                 uint8_t *msg, uint32_t len, bool retain,
                                 uint8_t qos)
{
    if (NULL == topic || NULL == msg || 0 == len) {
        return -1;
    }

    nng_mtx_lock(client->mtx);
    return_failure_if_open();

    nng_mqtt_msg_set_connect_will_topic(client->conn_msg, topic);
    nng_mqtt_msg_set_connect_will_msg(client->conn_msg, msg, len);
    nng_mqtt_msg_set_connect_will_retain(client->conn_msg, retain);
    nng_mqtt_msg_set_connect_will_qos(client->conn_msg, qos);
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

int neu_mqtt_client_set_connect_cb(neu_mqtt_client_t *             client,
                                   neu_mqtt_client_connection_cb_t cb,
                                   void *                          data)
{
    nng_mtx_lock(client->mtx);
    return_failure_if_open();

    client->connect_cb      = cb;
    client->connect_cb_data = data;
    nng_mtx_unlock(client->mtx);

    return 0;
}

int neu_mqtt_client_set_disconnect_cb(neu_mqtt_client_t *             client,
                                      neu_mqtt_client_connection_cb_t cb,
                                      void *                          data)
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

static void rand_str(char *dst, size_t len)
{
    char alphabet[] = "0123456789"
                      "abcdefghijklmnopqrstuvwxyz"
                      "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

    while (len-- > 1) {
        size_t i = (double) rand() / RAND_MAX * ((sizeof alphabet) - 1);
        *dst++   = alphabet[i];
    }
    *dst = '\0';
}

static int neu_mqtt_client_set_cache_db(neu_mqtt_client_t *client)
{
    char str[8] = { 0 };
    rand_str(str, sizeof(str));

    char *db = NULL;
    neu_asprintf(&db, "neuron-mqtt-client-%s.db", str);
    if (NULL == db) {
        log(error, "neu_asprintf nano-mqtt cilent db fail");
        return NEU_ERR_EINTERNAL;
    }

    if (client->db && 0 == strcmp(client->db, db)) {
        free(db);
        return 0;
    }

    neu_mqtt_client_remove_cache_db(client);

    free(client->db);
    client->db = db;

    if (client->sqlite_cfg) {
        nng_mqtt_sqlite_db_init(client->sqlite_cfg, db, client->version);
    }

    log(notice, "set cache db %s", db);

    return 0;
}

void neu_mqtt_client_remove_cache_db(neu_mqtt_client_t *client)
{
    if (NULL == client->db) {
        return;
    }

    const char *dir       = "persistence";
    int         path_size = strlen(dir) + strlen(client->db) + 6;
    char *      path      = calloc(path_size, 1);
    if (NULL == path) {
        return;
    }

    snprintf(path, path_size, "%s/%s", dir, client->db);
    log(notice, "rm cache db %s", path);
    remove(path);
    snprintf(path, path_size, "%s/%s-shm", dir, client->db);
    log(notice, "rm cache db %s", path);
    remove(path);
    snprintf(path, path_size, "%s/%s-wal", dir, client->db);
    log(notice, "rm cache db %s", path);
    remove(path);

    free(path);
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
            neu_mqtt_client_remove_cache_db(client);
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

    if (0 != (rv = neu_mqtt_client_set_cache_db(client))) {
        return rv;
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
                                      zlog_category_t *  cat)
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
    neu_events_t *     events = NULL;
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

int neu_mqtt_client_publish(neu_mqtt_client_t *client, neu_mqtt_qos_e qos,
                            char *topic, uint8_t *payload, uint32_t len,
                            void *data, neu_mqtt_client_publish_cb_t cb)
{
    int      rv      = 0;
    nng_msg *pub_msg = NULL;
    task_t * task    = NULL;

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

    if (!neu_mqtt_topic_filter_is_valid(topic)) {
        return NEU_ERR_MQTT_SUBSCRIBE_FAILURE;
    }

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
