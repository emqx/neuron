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
#include <stdlib.h>
#include <string.h>

#define NNG_SUPP_SQLITE 1
#include <nng/mqtt/mqtt_client.h>
#include <nng/nng.h>
#include <nng/supplemental/tls/tls.h>
#include <nng/supplemental/util/platform.h>

#include "connection/mqtt_client.h"
#include "utils/asprintf.h"
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

struct neu_mqtt_client_s {
    nng_socket              sock;
    char *                  url;
    neu_mqtt_version_e      version;
    nng_tls_config *        tls_cfg;
    nng_msg *               conn_msg;
    bool                    opened;
    bool                    connected;
    neu_mqtt_client_cb_t    connect_cb;
    void *                  connect_cb_data;
    neu_mqtt_client_cb_t    disconnect_cb;
    void *                  disconnect_cb_data;
    nng_mqtt_sqlite_option *sqlite_cfg;
    zlog_category_t *       log;
};

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

static void connect_cb(nng_pipe p, nng_pipe_ev ev, void *arg)
{
    (void) p;
    (void) ev;
    neu_mqtt_client_t *client = arg;

    log(info, "mqtt client connected");
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

static nng_msg *alloc_conn_msg(neu_mqtt_client_t *client,
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
    nng_mqtt_sqlite_db_init(cfg, "mqtt_client.db", client->version);

    return cfg;
}

neu_mqtt_client_t *neu_mqtt_client_new(const char *host, uint16_t port,
                                       neu_mqtt_version_e version)
{
    char *url = NULL;

    neu_asprintf(&url, "mqtt-tcp://%s:%" PRIu16, host, port);
    if (NULL == url) {
        return NULL;
    }

    neu_mqtt_client_t *client = calloc(1, sizeof(*client));
    if (NULL == client) {
        free(url);
        return NULL;
    }

    client->conn_msg = alloc_conn_msg(client, version);
    if (NULL == client->conn_msg) {
        free(url);
        return NULL;
    }

    client->url     = url;
    client->version = version;

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
        nng_msg_free(client->conn_msg);
        free(client->url);
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

    if (client->tls_cfg) {
        cfg = client->tls_cfg;
    } else {
        cfg = alloc_tls_config(client);
        if (NULL == cfg) {
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

    if (ca != NULL) {
        if ((rv = nng_tls_config_ca_chain(cfg, ca, NULL)) != 0) {
            log(error, "nng_tls_config_ca_chain fail: %s", nng_strerror(rv));
            return -1;
        }
    }

    return 0;
}

int neu_mqtt_client_set_msg_cache_limit(neu_mqtt_client_t *client,
                                        size_t             cache_limit)
{
    check_opened();

    if (0 == cache_limit) {
        return 0;
    }

    if (NULL == client->sqlite_cfg) {
        client->sqlite_cfg = alloc_sqlite_config(client);
        if (NULL == client->sqlite_cfg) {
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

    if (client->sqlite_cfg &&
        (rv = nng_socket_set_ptr(client->sock, NNG_OPT_MQTT_SQLITE,
                                 client->sqlite_cfg))) {
        log(error, "nng_socket_set_ptr(NNG_OPT_MQTT_SQLITE) fail: %s",
            nng_strerror(rv));
        return -1;
    }

    nng_dialer dialer;
    if ((rv = nng_dialer_create(&dialer, client->sock, client->url)) != 0) {
        log(error, "nng_dialer_create fail: %s", nng_strerror(rv));
        return -1;
    }

    if (client->tls_cfg &&
        0 !=
            (rv = nng_dialer_set_ptr(dialer, NNG_OPT_TLS_CONFIG,
                                     client->tls_cfg))) {
        log(error, "nng_dialter_set_ptr(NNG_OPT_TLS_CONFIG) fail: %s",
            nng_strerror(rv));
        return -1;
    }

    nng_mqtt_set_connect_cb(client->sock, connect_cb, client);
    nng_mqtt_set_disconnect_cb(client->sock, disconnect_cb, client);

    nng_dialer_set_ptr(dialer, NNG_OPT_MQTT_CONNMSG, client->conn_msg);
    nng_dialer_start(dialer, NNG_FLAG_NONBLOCK);

    client->opened = true;

    return 0;
}

int neu_mqtt_client_close(neu_mqtt_client_t *client)
{
    int rv = 0;

    if (!client->opened) {
        return rv;
    }

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

    int rv = nng_sendmsg(client->sock, pub_msg, NNG_FLAG_NONBLOCK);
    if (0 != rv) {
        log(error, "nng_sendmsg fail: %s", nng_strerror(rv));
        nng_msg_free(pub_msg);
        return -1;
    }

    log(debug, "pub [%s, QoS%d] %zu bytes", topic, qos, len);

    return 0;
}

int neu_mqtt_client_subscribe(neu_mqtt_client_t *client, neu_mqtt_qos_e qos,
                              const char *topic, neu_mqtt_client_cb_t cb)
{
    int      rv      = 0;
    nng_msg *sub_msg = NULL;

    if ((rv = nng_mqtt_msg_alloc(&sub_msg, 0)) != 0) {
        log(error, "nng_mqtt_msg_alloc fail: %s", nng_strerror(rv));
        return -1;
    }

    nng_mqtt_topic_qos topic_qos[] = {
        { .qos   = qos,
          .topic = { .buf = (uint8_t *) topic, .length = strlen(topic) } },
    };

    size_t topic_qos_count = sizeof(topic_qos) / sizeof(nng_mqtt_topic_qos);

    nng_mqtt_msg_set_packet_type(sub_msg, NNG_MQTT_SUBSCRIBE);
    nng_mqtt_msg_set_subscribe_topics(sub_msg, topic_qos, topic_qos_count);

    rv = nng_sendmsg(client->sock, sub_msg, NNG_FLAG_NONBLOCK);
    if (0 != rv) {
        log(error, "nng_sendmsg fail: %s", nng_strerror(rv));
        return -1;
    }

    (void) cb; // TODO: handle callback on subscribed topic

    return 0;
}
