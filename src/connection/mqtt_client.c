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

#define NNG_SUPP_TLS 1
#define NNG_SUPP_SQLITE 1
#include <nng/mqtt/mqtt_client.h>
#include <nng/nng.h>
#include <nng/supplemental/tls/tls.h>
#include <nng/supplemental/util/platform.h>

#include "connection/mqtt_client.h"
#include "utils/asprintf.h"
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

#define check_opened()                                                       \
    do {                                                                     \
        if (client->opened) {                                                \
            log(warn,                                                        \
                "mqtt client already opened, setting will not take effect"); \
            return -1;                                                       \
        }                                                                    \
    } while (0)

typedef struct {
    neu_mqtt_qos_e                 qos;
    char *                         topic;
    neu_mqtt_client_subscribe_cb_t cb;
    void *                         data;
    UT_hash_handle                 hh;
} subscription_t;

typedef enum {
    TASK_SEND,
    TASK_RECV,
} task_kind_e;

typedef struct task_s {
    task_kind_e    kind;
    nng_aio *      aio;
    void *         data;
    struct task_s *prev;
    struct task_s *next;
} task_t;

struct neu_mqtt_client_s {
    nng_socket              sock;
    neu_mqtt_version_e      version;
    char *                  host;
    uint16_t                port;
    char *                  url;
    nng_tls_config *        tls_cfg;
    nng_msg *               conn_msg;
    bool                    opened;
    bool                    connected;
    neu_mqtt_client_cb_t    connect_cb;
    void *                  connect_cb_data;
    neu_mqtt_client_cb_t    disconnect_cb;
    void *                  disconnect_cb_data;
    nng_mqtt_sqlite_option *sqlite_cfg;
    bool                    receiving;
    nng_aio *               recv_aio;
    subscription_t *        subscriptions;
    size_t                  task_count;
    size_t                  task_limit;
    task_t *                task_free_list;
    zlog_category_t *       log;
};

static inline task_t *task_new(neu_mqtt_client_t *client, task_kind_e kind,
                               void *data);
static inline void    task_free(task_t *task);
static inline void    tasks_free(task_t *tasks);
static void           task_cb(void *arg);
static void           task_handle_send(task_t *task, neu_mqtt_client_t *client);
static void           task_handle_recv(task_t *task, neu_mqtt_client_t *client);

static subscription_t *subscription_new(neu_mqtt_client_t *            client,
                                        const char *                   topic,
                                        neu_mqtt_client_subscribe_cb_t cb,
                                        void *                         data);
static inline void     subscription_free(subscription_t *subscription);
static inline void     subscriptions_free(subscription_t *subscriptions);

static void recv_cb(void *arg);
static void disconnect_cb(nng_pipe p, nng_pipe_ev ev, void *arg);
static void connect_cb(nng_pipe p, nng_pipe_ev ev, void *arg);

static inline task_t *client_alloc_task(neu_mqtt_client_t *client,
                                        task_kind_e kind, void *data);
static inline void    client_free_task(neu_mqtt_client_t *client, task_t *task);
static inline void    client_add_subscription(neu_mqtt_client_t *client,
                                              subscription_t *   sub);
static inline int     client_send_msg(neu_mqtt_client_t *client, nng_msg *msg);
static int            client_send_sub_msg(neu_mqtt_client_t *client,
                                          subscription_t *   subscription);
static inline void    client_start_recv(neu_mqtt_client_t *client);
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

static inline task_t *task_new(neu_mqtt_client_t *client, task_kind_e kind,
                               void *data)
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
    task->kind = kind;
    task->data = data;

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

    if (TASK_SEND == task->kind) {
        task_handle_send(task, client);
    } else if (TASK_RECV == task->kind) {
        task_handle_recv(task, client);
    } else {
        log(error, "unexpected task kind:%d", task->kind);
        assert(!"logic error, task kind not exhausted");
    }

    client_free_task(client, task);
}

static void task_handle_send(task_t *task, neu_mqtt_client_t *client)
{
    nng_aio *aio = task->aio;

    int rv = 0;
    if (0 != (rv = nng_aio_result(aio))) {
        log(error, "send error: %s", nng_strerror(rv));
        return;
    }

    nng_msg *msg = nng_aio_get_msg(aio);
    if (NULL == msg) {
        return;
    }

    nng_mqtt_packet_type pt = nng_mqtt_msg_get_packet_type(msg);
    switch (pt) {
    case NNG_MQTT_SUBACK: {
        uint32_t n  = 0;
        uint8_t *rc = nng_mqtt_msg_get_suback_return_codes(msg, &n);
        if (0 == *rc) {
            log(info, "suback return_code:%d, OK", *rc);
        } else {
            log(error, "suback return_code:%d, FAIL", *rc);
        }
        nng_msg_free(msg);
        break;
    }
    case NNG_MQTT_UNSUBACK: {
        nng_msg_free(msg);
        break;
    }
    default:
        // NanoSDK quirks: do not free the msg
        // log(info, "ignore packet type: %d", pt);
        break;
    }

    return;
}

static void task_handle_recv(task_t *task, neu_mqtt_client_t *client)
{
    subscription_t *subscription = task->data;
    nng_msg *       msg          = nng_aio_get_msg(task->aio);

    if (NULL == msg) {
        log(error, "recv NULL msg");
        return;
    }

    nng_mqtt_packet_type pt = nng_mqtt_msg_get_packet_type(msg);
    if (NNG_MQTT_PUBLISH != pt) {
        log(error, "recv packet type:%d, expect PUBLISH packet", pt);
        return;
    }

    uint32_t payload_len;
    uint8_t *payload = nng_mqtt_msg_get_publish_payload(msg, &payload_len);

    subscription->cb(payload, payload_len, subscription->data);

    nng_msg_free(msg);
}

static subscription_t *subscription_new(neu_mqtt_client_t *            client,
                                        const char *                   topic,
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

    subscription->cb   = cb;
    subscription->data = data;

    return subscription;
}

static inline void subscription_free(subscription_t *subscription)
{
    if (subscription) {
        free(subscription->topic);
        free(subscription);
    }
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

static void connect_cb(nng_pipe p, nng_pipe_ev ev, void *arg)
{
    (void) p;
    (void) ev;
    neu_mqtt_client_t *client = arg;

    log(info, "mqtt client connected");

    // send subscribe messages for all subscriptions
    subscription_t *sub;
    HASH_LOOP(hh, client->subscriptions, sub)
    {
        if (client_send_sub_msg(client, sub) < 0) {
            log(error, "client_send_sub_msg fail");
        }
    }

    // start receiving
    client_start_recv(client);

    // mark as connected
    client->connected = true;

    if (client->connect_cb) {
        client->connect_cb(client->connect_cb_data);
    }
}

static void disconnect_cb(nng_pipe p, nng_pipe_ev ev, void *arg)
{
    (void) p;
    (void) ev;
    neu_mqtt_client_t *client = arg;

    log(info, "mqtt client disconnected");
    client->connected = false;

    if (client->disconnect_cb) {
        client->disconnect_cb(client->connect_cb_data);
    }
}

static void recv_cb(void *arg)
{
    int                rv           = 0;
    subscription_t *   subscription = arg;
    neu_mqtt_client_t *client       = arg;
    nng_aio *          aio          = client->recv_aio;

    if (0 != (rv = nng_aio_result(aio))) {
        log(error, "mqtt client recv error: %s, aio:%p", nng_strerror(rv), aio);
        if (NNG_EAGAIN == rv) {
            nng_recv_aio(client->sock, aio);
        } else {
            client->receiving = false;
        }
        return;
    }

    nng_msg *msg = nng_aio_get_msg(aio);
    nng_aio_set_msg(aio, NULL);

    uint32_t    payload_len;
    uint8_t *   payload = nng_mqtt_msg_get_publish_payload(msg, &payload_len);
    uint32_t    topic_len;
    const char *topic = nng_mqtt_msg_get_publish_topic(msg, &topic_len);
    uint8_t     qos   = nng_mqtt_msg_get_publish_qos(msg);

    (void) payload;
    log(info, "recv [%.*s, QoS%d] %" PRIu32 " bytes", (unsigned) topic_len,
        topic, (int) qos, payload_len);

    // find matching subscription
    // NOTE: does not support wildcards
    HASH_FIND(hh, client->subscriptions, topic, topic_len, subscription);
    if (NULL != subscription) {
        task_t *task = client_alloc_task(client, TASK_RECV, subscription);
        if (NULL != task) {
            nng_aio_set_msg(task->aio, msg);
            msg = NULL;
            nng_aio_finish(task->aio, 0);
        } else {
            log(error, "client_alloc_task fail");
        }
    } else {
        log(warn, "[%.*s] no subscription found", (unsigned) topic_len, topic);
    }

    nng_msg_free(msg);
    nng_recv_aio(client->sock, aio);
}

static inline task_t *client_alloc_task(neu_mqtt_client_t *client,
                                        task_kind_e kind, void *data)
{
    task_t *task = client->task_free_list;

    if (NULL != task) {
        DL_DELETE(client->task_free_list, task);
        task->kind = kind;
        task->data = data;
        // may try to reduce task queue length adaptive to workload
        return task;
    }

    if (client->task_count >= client->task_limit) {
        log(warn, "reach task limit: %zu", client->task_count);
        return NULL;
    }

    task = task_new(client, kind, data);
    if (NULL == task) {
        log(error, "task_new fail");
        return NULL;
    }

    ++client->task_count;

    // the task queue length should be a small number due to reuse
    // log info every time the task queue length increments by 32
    if (0 == (client->task_count & (32 - 1))) {
        log(info, "task queue length: %zu", client->task_count);
    }

    return task;
}

static inline void client_free_task(neu_mqtt_client_t *client, task_t *task)
{
    nng_aio_set_msg(task->aio, NULL);
    task->data = NULL;
    // prepend in the hope to help locality
    DL_PREPEND(client->task_free_list, task);
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

static inline int client_send_msg(neu_mqtt_client_t *client, nng_msg *msg)
{
    task_t *task = client_alloc_task(client, TASK_SEND, NULL);
    if (NULL == task) {
        log(error, "client_alloc_task fail");
        return -1;
    }

    nng_aio_set_msg(task->aio, msg);
    nng_send_aio(client->sock, task->aio);

    return 0;
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

    rv = client_send_msg(client, sub_msg);
    if (0 != rv) {
        log(error, "client_send_msg fail");
        nng_msg_free(sub_msg);
        return -1;
    }

    log(info, "sub [%s, QoS%d]", subscription->topic, subscription->qos);

    return 0;
}

static inline void client_start_recv(neu_mqtt_client_t *client)
{
    if (client->recv_aio && !client->receiving) {
        nng_recv_aio(client->sock, client->recv_aio);
        client->receiving = true;
    }
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
    char *                  db  = NULL;
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

neu_mqtt_client_t *neu_mqtt_client_new(neu_mqtt_version_e version)
{
    neu_mqtt_client_t *client = calloc(1, sizeof(*client));
    if (NULL == client) {
        return NULL;
    }

    client->conn_msg = alloc_conn_msg(client, version);
    if (NULL == client->conn_msg) {
        return NULL;
    }

    client->version    = version;
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
        free(client->url);
        free(client->host);
        free(client);
    }
}

bool neu_mqtt_client_is_opened(neu_mqtt_client_t *client)
{
    return client->opened;
}

bool neu_mqtt_client_is_connected(neu_mqtt_client_t *client)
{
    return client->connected;
}

int neu_mqtt_client_set_addr(neu_mqtt_client_t *client, const char *host,
                             uint16_t port)
{
    if (client->host && 0 == strcmp(client->host, host)) {
        client->port = port;
        return 0;
    }

    char *h = strdup(host);
    if (NULL == h) {
        log(error, "strdup host fail");
        return -1;
    }

    free(client->host);
    client->host = h;
    client->port = port;
    return 0;
}

int neu_mqtt_client_set_id(neu_mqtt_client_t *client, const char *id)
{
    check_opened();

    if (NULL == id || 0 == strlen(id)) {
        return -1;
    }

    nng_mqtt_msg_set_connect_client_id(client->conn_msg, id);
    return 0;
}

int neu_mqtt_client_set_user(neu_mqtt_client_t *client, const char *username,
                             const char *password)
{
    check_opened();

    nng_mqtt_msg_set_connect_user_name(client->conn_msg, username);
    nng_mqtt_msg_set_connect_password(client->conn_msg, password);
    return 0;
}

int neu_mqtt_client_set_connect_cb(neu_mqtt_client_t *  client,
                                   neu_mqtt_client_cb_t cb, void *data)
{
    check_opened();

    client->connect_cb      = cb;
    client->connect_cb_data = data;
    return 0;
}

int neu_mqtt_client_set_disconnect_cb(neu_mqtt_client_t *  client,
                                      neu_mqtt_client_cb_t cb, void *data)
{
    check_opened();

    client->disconnect_cb      = cb;
    client->disconnect_cb_data = data;
    return 0;
}

int neu_mqtt_client_set_tls(neu_mqtt_client_t *client, const char *ca,
                            const char *cert, const char *key,
                            const char *keypass)
{
    int             rv  = 0;
    nng_tls_config *cfg = NULL;

    check_opened();

    if (NULL == ca) {
        // disable tls
        log(info, "tls disabled");
        if (client->tls_cfg) {
            nng_tls_config_free(client->tls_cfg);
            client->tls_cfg = NULL;
        }
        return 0;
    }

    if (client->tls_cfg) {
        cfg = client->tls_cfg;
    } else {
        cfg = alloc_tls_config(client);
        if (NULL == cfg) {
            log(error, "alloc_tls_config fail");
            return -1;
        }
        client->tls_cfg = cfg;
    }

    if (cert != NULL && key != NULL) {
        if ((rv = nng_tls_config_auth_mode(cfg, NNG_TLS_AUTH_MODE_REQUIRED)) !=
            0) {
            log(error, "nng_tls_config_auth_mode fail: %s", nng_strerror(rv));
            return -1;
        }
        if ((rv = nng_tls_config_own_cert(cfg, cert, key, keypass)) != 0) {
            log(error, "nng_tls_config_own_cert fail: %s", nng_strerror(rv));
            return -1;
        }
    } else {
        if ((rv = nng_tls_config_auth_mode(cfg, NNG_TLS_AUTH_MODE_NONE)) != 0) {
            log(error, "nng_tls_config_auth_mode fail: %s", nng_strerror(rv));
            return -1;
        }
    }

    if ((rv = nng_tls_config_ca_chain(cfg, ca, NULL)) != 0) {
        log(error, "nng_tls_config_ca_chain fail: %s", nng_strerror(rv));
        return -1;
    }

    return 0;
}

int neu_mqtt_client_set_msg_cache_limit(neu_mqtt_client_t *client,
                                        size_t             cache_limit)
{
    check_opened();

    if (0 == cache_limit) {
        // disable cache
        log(info, "cache disabled");
        if (client->sqlite_cfg) {
            nng_mqtt_free_sqlite_opt(client->sqlite_cfg);
            client->sqlite_cfg = NULL;
        }
        return 0;
    }

    if (NULL == client->sqlite_cfg) {
        client->sqlite_cfg = alloc_sqlite_config(client);
        if (NULL == client->sqlite_cfg) {
            log(error, "alloc_sqlite_config fail");
            return -1;
        }
    }

    nng_mqtt_set_sqlite_max_rows(client->sqlite_cfg, cache_limit);

    return 0;
}

int neu_mqtt_client_set_zlog_category(neu_mqtt_client_t *client,
                                      zlog_category_t *  cat)
{
    check_opened();

    client->log = cat;
    return 0;
}

int neu_mqtt_client_open(neu_mqtt_client_t *client)
{
    int rv = 0;

    if (client->opened) {
        return rv;
    }

    if (NEU_MQTT_VERSION_V5 == client->version &&
        (rv = nng_mqttv5_client_open(&client->sock)) != 0) {
        log(error, "nng_mqttv5_client_open fail: %s", nng_strerror(rv));
        return -1;
    } else if ((rv = nng_mqtt_client_open(&client->sock)) != 0) {
        log(error, "nng_mqtt_client_open fail: %s", nng_strerror(rv));
        return -1;
    }

    if (0 != client_make_url(client)) {
        log(error, "client_make_url fail");
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

    client->opened = true;

    return 0;

error:
    nng_close(client->sock);
    return -1;
}

int neu_mqtt_client_close(neu_mqtt_client_t *client)
{
    int rv = 0;

    if (!client->opened) {
        return rv;
    }

    // NanoSDK quirks: calling nng_aio_stop will block if the aio is in use
    // nng_aio_stop(client->recv_aio);

    rv = nng_close(client->sock);
    if (0 != rv) {
        log(error, "nng_close: %s", nng_strerror(rv));
        return -1;
    }

    client->opened = false;

    return 0;
}

int neu_mqtt_client_publish(neu_mqtt_client_t *client, neu_mqtt_qos_e qos,
                            const char *topic, const char *payload, size_t len)
{
    nng_msg *pub_msg;
    nng_mqtt_msg_alloc(&pub_msg, 0);

    nng_mqtt_msg_set_packet_type(pub_msg, NNG_MQTT_PUBLISH);
    nng_mqtt_msg_set_publish_topic(pub_msg, topic);
    nng_mqtt_msg_set_publish_payload(pub_msg, (uint8_t *) payload, len);
    nng_mqtt_msg_set_publish_qos(pub_msg, qos);

    int rv = client_send_msg(client, pub_msg);
    if (0 != rv) {
        log(error, "client_send_msg fail");
        nng_msg_free(pub_msg);
        return -1;
    }

    log(info, "pub [%s, QoS%d] %zu bytes", topic, qos, len);

    return 0;
}

int neu_mqtt_client_subscribe(neu_mqtt_client_t *client, neu_mqtt_qos_e qos,
                              const char *                   topic,
                              neu_mqtt_client_subscribe_cb_t cb, void *data)
{
    int             rv           = 0;
    subscription_t *subscription = NULL;

    if (NULL == client->recv_aio) {
        if ((rv = nng_aio_alloc(&client->recv_aio, recv_cb, client)) != 0) {
            log(error, "nng_aio_alloc fail: %s", nng_strerror(rv));
            return -1;
        }
        if (client->connected) {
            // connect_cb already fired, then start receiving
            client_start_recv(client);
        }
    }

    subscription = subscription_new(client, topic, cb, data);
    if (NULL == subscription) {
        log(error, "subscription_new fail");
        goto error;
    }
    subscription->qos = qos;

    if (client->connected) {
        // connect_cb already fired, then send a subscribe message
        if (client_send_sub_msg(client, subscription) < 0) {
            log(error, "client_send_sub_msg fail");
            goto error;
        }
    }

    client_add_subscription(client, subscription);

    return 0;

error:
    subscription_free(subscription);
    return -1;
}
