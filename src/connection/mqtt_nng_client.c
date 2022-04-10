/**
 * NEURON IIoT System for Industry 4.0
 * Copyright (C) 2020-2021 EMQ Technologies Co., Ltd All rights reserved.
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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/queue.h>
#include <unistd.h>

#include <neuron.h>
#include <nng/mqtt/mqtt_client.h>
#include <nng/nng.h>
#include <nng/supplemental/tls/tls.h>
#include <nng/supplemental/util/platform.h>

#include "mqtt_nng_client.h"

#define UNUSED(x) (void) (x)
#define MAX_URL_LEN 256

struct subscribe_tuple {
    char *               topic;
    int                  qos;
    neu_subscribe_handle handle;
    bool                 subscribed;
    mqtt_nng_client_t *  client;
    TAILQ_ENTRY(subscribe_tuple) entry;
};

struct mqtt_nng_client {
    const neu_mqtt_option_t *option;
    void *                   user_data;
    nng_thread *             daemon;
    nng_mtx *                mtx;
    bool                     running;
    char *                   client_id;
    nng_socket               sock;
    nng_dialer               dialer;
    char *                   url;
    nng_mqtt_cb              callback;
    nng_msg *                connect_msg;
    int                      connected;
    char *                   ca;
    char *                   cert;
    char *                   key;
    char *                   keypass;
    nng_tls_config *         config;
    TAILQ_HEAD(, subscribe_tuple) head;
};

struct string {
    size_t         length;
    unsigned char *data;
};

static neu_err_code_e client_subscribe_send(mqtt_nng_client_t *     client,
                                            struct subscribe_tuple *tuple);

static void client_on_connected(void *arg, nng_msg *ack_msg)
{
    mqtt_nng_client_t *client = (mqtt_nng_client_t *) arg;
    uint8_t            status = nng_mqtt_msg_get_connack_return_code(ack_msg);
    client->connected         = (0 == status) ? 1 : 0;

    log_info("connected status:%d", status);
    nng_msg_free(ack_msg);

    if (1 == client->connected) {
        struct subscribe_tuple *tuple = NULL;
        TAILQ_FOREACH(tuple, &client->head, entry)
        {
            client_subscribe_send(client, tuple);
        }
    }
}

static void client_on_disconnected(void *arg, nng_msg *msg)
{
    mqtt_nng_client_t *client = (mqtt_nng_client_t *) arg;
    client->connected         = 0;
    log_info("disconnect mqtt");
    nng_msg_free(msg);

    bool run_flag = true;
    nng_mtx_lock(client->mtx);
    run_flag = client->running;
    nng_mtx_unlock(client->mtx);

    if (!run_flag) {
        log_info("exit");
        return;
    }
}

static neu_err_code_e client_send(mqtt_nng_client_t *client, nng_msg *msg)
{
    if (1 != client->connected) {
        nng_msg_free(msg);
        return NEU_ERR_MQTT_CONNECT_FAILURE;
    }

    nng_mqtt_packet_type type = nng_mqtt_msg_get_packet_type(msg);
    int                  ret  = nng_sendmsg(client->sock, msg, 0);
    nng_msg_free(msg);

    if (0 != ret) {
        neu_err_code_e error = NEU_ERR_MQTT_FAILURE;
        char *         flag  = "Unknow";
        switch (type) {
        case NNG_MQTT_SUBSCRIBE:
            error = NEU_ERR_MQTT_SUBSCRIBE_FAILURE;
            flag  = "Subscribe";
            break;
        case NNG_MQTT_UNSUBSCRIBE:
            error = NEU_ERR_MQTT_UNSUBSCRIBE_FAILURE;
            flag  = "Unsubscribe";
            break;
        case NNG_MQTT_PUBLISH:
            error = NEU_ERR_MQTT_PUBLISH_FAILURE;
            flag  = "Publish";
            break;

        default:
            break;
        }

        log_error("%s msg send error:%d", flag, ret);
        return error;
    }

    return NEU_ERR_SUCCESS;
}

static void client_refresher(void *context)
{
    mqtt_nng_client_t *client   = (mqtt_nng_client_t *) context;
    bool               run_flag = true;

    while (1) {
        nng_mtx_lock(client->mtx);
        run_flag = client->running;
        nng_mtx_unlock(client->mtx);

        if (!run_flag) {
            break;
        }

        if (1 != client->connected) {
            nng_msleep(1000);
            continue;
        }

        const char *topic       = NULL;
        uint32_t    topic_len   = 0;
        uint8_t *   payload     = NULL;
        uint32_t    payload_len = 0;
        nng_msg *   msg         = NULL;

        int ret = nng_recvmsg(client->sock, &msg, 0);
        if (0 != ret) {
            log_error("Received error:%d", ret);
            continue;
        }

        if (1 == client->option->verbose) {
            uint8_t buff[1024] = { 0 };
            nng_mqtt_msg_dump(msg, buff, sizeof(buff), true);
            log_info("%s", buff);
        }

        // only receive publish messages
        if (NNG_MQTT_PUBLISH != nng_mqtt_msg_get_packet_type(msg)) {
            nng_msg_free(msg);
            continue;
        }

        payload = nng_mqtt_msg_get_publish_payload(msg, &payload_len);
        topic   = nng_mqtt_msg_get_publish_topic(msg, &topic_len);

        char *topic_name = (char *) malloc(topic_len + 1);
        memcpy(topic_name, topic, topic_len);
        topic_name[topic_len] = '\0';

        log_info("Received: topic:%s,test", topic_name);

        struct subscribe_tuple *item = NULL;
        TAILQ_FOREACH(item, &client->head, entry)
        {
            if (0 == strcmp(item->topic, topic_name) && NULL != item->handle) {
                item->handle(topic_name, strlen(topic_name), (void *) payload,
                             payload_len, client->user_data);
            }
        }

        free(topic_name);
        nng_msg_free(msg);
    }
}

static struct string client_file_load(const char *const path)
{
    struct string string = { 0 };
    FILE *        fp     = fopen(path, "rb");
    if (!fp) {
        errno = 0;
        return string;
    }

    fseek(fp, 0, SEEK_END);
    string.length = (size_t) ftell(fp);
    string.data   = malloc((string.length + 1) * sizeof(unsigned char));
    if (NULL != string.data) {
        string.data[string.length] = '\0';
        fseek(fp, 0, SEEK_SET);
        size_t read =
            fread(string.data, sizeof(unsigned char), string.length, fp);
        if (read != string.length) {
            free(string.data);
            string.data   = NULL;
            string.length = 0;
        }
    } else {
        string.length = 0;
    }

    fclose(fp);
    return string;
}

static mqtt_nng_client_t *client_create(const neu_mqtt_option_t *option,
                                        void *                   context)
{
    mqtt_nng_client_t *client = malloc(sizeof(mqtt_nng_client_t));
    if (NULL == client) {
        return NULL;
    }

    memset(client, 0, sizeof(mqtt_nng_client_t));
    client->option    = option;
    client->user_data = context;
    client->client_id = client->option->clientid;
    client->connected = 0;

    nng_mtx_alloc(&client->mtx);
    client->running = true;

    if (NULL != client->option->cert) {
        struct string str = client_file_load(client->option->cert);
        client->cert      = (char *) str.data;
    }

    if (NULL != client->option->key) {
        struct string str = client_file_load(client->option->key);
        client->key       = (char *) str.data;
    }

    if (NULL != client->option->keypass) {
        client->keypass = strdup(client->option->keypass);
    }

    if (NULL != client->option->ca_file) {
        struct string str = client_file_load(client->option->ca_file);
        client->ca        = (char *) str.data;
    }

    client->callback.name            = "neuron_client";
    client->callback.on_connected    = client_on_connected;
    client->callback.on_disconnected = client_on_disconnected;
    client->callback.connect_arg     = client;
    client->callback.disconn_arg     = client;

    TAILQ_INIT(&client->head);
    return client;
}

static int client_tls(mqtt_nng_client_t *client)
{
    int ret = nng_tls_config_alloc(&client->config, NNG_TLS_MODE_CLIENT);
    if (0 != ret) {
        return ret;
    }

    if (NULL != client->cert && NULL != client->key) {
        nng_tls_config_auth_mode(client->config, NNG_TLS_AUTH_MODE_REQUIRED);
        if (0 !=
            nng_tls_config_own_cert(client->config, client->cert, client->key,
                                    client->keypass)) {
            return -1;
        }
    } else {
        nng_tls_config_auth_mode(client->config, NNG_TLS_AUTH_MODE_NONE);
    }

    if (NULL != client->ca) {
        if (0 != nng_tls_config_ca_chain(client->config, client->ca, NULL)) {
            return -1;
        }
    }

    nng_dialer_set_ptr(client->dialer, NNG_OPT_TLS_CONFIG, client->config);
    return 0;
}

static neu_err_code_e client_connection_init(mqtt_nng_client_t *client)
{
    if (0 != nng_mqtt_client_open(&client->sock)) {
        return NEU_ERR_MQTT_INIT_FAILURE;
    }

    char url[MAX_URL_LEN] = { '\0' };
    if (0 == strcmp(client->option->connection, "ssl://")) {
        snprintf(url, MAX_URL_LEN, "tls+mqtt-tcp://%s:%s", client->option->host,
                 client->option->port);
    }

    if (0 == strcmp(client->option->connection, "tcp://")) {
        snprintf(url, MAX_URL_LEN, "mqtt-tcp://%s:%s", client->option->host,
                 client->option->port);
    }

    client->url = strdup(url);
    if (0 != nng_dialer_create(&client->dialer, client->sock, client->url)) {
        return NEU_ERR_MQTT_CONNECT_FAILURE;
    }

    if (0 == strcmp(client->option->connection, "ssl://")) {
        client_tls(client);
    }

    return NEU_ERR_SUCCESS;
}

static void client_connect(mqtt_nng_client_t *client)
{
    nng_mqtt_msg_alloc(&client->connect_msg, 0);
    nng_mqtt_msg_set_packet_type(client->connect_msg, NNG_MQTT_CONNECT);
    nng_mqtt_msg_set_connect_proto_version(client->connect_msg,
                                           client->option->MQTT_version);
    nng_mqtt_msg_set_connect_keep_alive(client->connect_msg,
                                        client->option->keepalive);

    if (NULL != client->option->username && NULL != client->option->password) {
        nng_mqtt_msg_set_connect_user_name(client->connect_msg,
                                           client->option->username);
        nng_mqtt_msg_set_connect_password(client->connect_msg,
                                          client->option->password);
    }

    if (NULL != client->option->will_topic) {
        nng_mqtt_msg_set_connect_will_msg(
            client->connect_msg, (uint8_t *) client->option->will_payload,
            strlen(client->option->will_payload));
        nng_mqtt_msg_set_connect_will_topic(client->connect_msg,
                                            client->option->will_topic);
    }

    nng_mqtt_msg_set_connect_client_id(client->connect_msg, client->client_id);
    nng_mqtt_msg_set_connect_clean_session(client->connect_msg, true);

    if (1 == client->option->verbose) {
        uint8_t buff[1024] = { 0 };
        nng_mqtt_msg_dump(client->connect_msg, buff, sizeof(buff), true);
        log_info("%s", buff);
    }

    // Connect msg would be free when client disconnected
    nng_dialer_set_ptr(client->dialer, NNG_OPT_MQTT_CONNMSG,
                       client->connect_msg);
    nng_dialer_set_cb(client->dialer, &client->callback);
    nng_dialer_start(client->dialer, NNG_FLAG_NONBLOCK);
}

static void client_routine(mqtt_nng_client_t *client)
{
    nng_thread_create(&client->daemon, client_refresher, client);
}

neu_err_code_e mqtt_nng_client_open(mqtt_nng_client_t **     p_client,
                                    const neu_mqtt_option_t *option,
                                    void *                   context)
{
    if (NULL == option) {
        return NEU_ERR_MQTT_OPTION_IS_NULL;
    }

    mqtt_nng_client_t *client = client_create(option, context);
    if (NULL == client) {
        return NEU_ERR_MQTT_IS_NULL;
    }

    neu_err_code_e rc = client_connection_init(client);
    if (NEU_ERR_SUCCESS != rc) {
        return rc;
    }

    client_connect(client);
    client_routine(client);
    *p_client = client;
    return NEU_ERR_SUCCESS;
}

neu_err_code_e mqtt_nng_client_is_connected(mqtt_nng_client_t *client)
{
    if (NULL == client) {
        return NEU_ERR_MQTT_IS_NULL;
    }

    return (1 == client->connected) ? NEU_ERR_SUCCESS
                                    : NEU_ERR_MQTT_CONNECT_FAILURE;
}

static struct subscribe_tuple *
client_subscribe_create(mqtt_nng_client_t *client, const char *topic,
                        const int qos, const neu_subscribe_handle handle)
{
    struct subscribe_tuple *tuple =
        (struct subscribe_tuple *) malloc(sizeof(struct subscribe_tuple));
    if (NULL == tuple) {
        return NULL;
    }

    tuple->topic      = strdup(topic);
    tuple->qos        = qos;
    tuple->handle     = handle;
    tuple->subscribed = false;
    tuple->client     = client;
    return tuple;
}

static void client_subscribe_destroy(struct subscribe_tuple *tuple)
{
    if (NULL != tuple) {
        if (NULL != tuple->topic) {
            free(tuple->topic);
        }
        free(tuple);
    }
}

static neu_err_code_e client_subscribe_send(mqtt_nng_client_t *     client,
                                            struct subscribe_tuple *tuple)
{
    nng_msg *subscribe_msg = NULL;
    nng_mqtt_msg_alloc(&subscribe_msg, 0);
    nng_mqtt_msg_set_packet_type(subscribe_msg, NNG_MQTT_SUBSCRIBE);

    nng_mqtt_topic_qos subscription = { .qos   = tuple->qos,
                                        .topic = {
                                            .buf    = (uint8_t *) tuple->topic,
                                            .length = strlen(tuple->topic) } };

    nng_mqtt_msg_set_subscribe_topics(subscribe_msg, &subscription, 1);

    if (1 == client->option->verbose) {
        uint8_t buff[1024] = { 0 };
        nng_mqtt_msg_dump(subscribe_msg, buff, sizeof(buff), true);
        log_info("%s", buff);
    }

    return client_send(client, subscribe_msg);
}

static void client_subscribe_add(mqtt_nng_client_t *     client,
                                 struct subscribe_tuple *tuple)
{
    TAILQ_INSERT_TAIL(&client->head, tuple, entry);
}

neu_err_code_e mqtt_nng_client_subscribe(mqtt_nng_client_t *client,
                                         const char *topic, const int qos,
                                         neu_subscribe_handle handle)
{
    if (NULL == client) {
        return NEU_ERR_MQTT_IS_NULL;
    }

    if (NULL == topic || 0 == strlen(topic)) {
        return NEU_ERR_MQTT_TOPIC_EMPTY;
    }

    struct subscribe_tuple *tuple =
        client_subscribe_create(client, topic, qos, handle);
    if (NULL == tuple) {
        return NEU_ERR_MQTT_SUBSCRIBE_FAILURE;
    }

    client_subscribe_add(client, tuple);
    neu_err_code_e error = client_subscribe_send(client, tuple);
    if (NEU_ERR_SUCCESS != error) {
        return NEU_ERR_MQTT_SUBSCRIBE_FAILURE;
    }

    log_info("Subscribing to topic %s for using QoS%d", topic, qos);
    return NEU_ERR_SUCCESS;
}

static struct subscribe_tuple *
client_unsubscribe_create(mqtt_nng_client_t *client, const char *topic)
{
    struct subscribe_tuple *item = NULL;
    TAILQ_FOREACH(item, &client->head, entry)
    {
        if (0 == strcmp(item->topic, topic)) {
            return item;
        }
    }

    log_error("Cant find topic %s", topic);
    return NULL;
}

static neu_err_code_e client_unsubscribe_send(mqtt_nng_client_t *     client,
                                              struct subscribe_tuple *tuple)
{
    nng_msg *unsubscribe_msg = NULL;
    nng_mqtt_msg_alloc(&unsubscribe_msg, 0);
    nng_mqtt_msg_set_packet_type(unsubscribe_msg, NNG_MQTT_UNSUBSCRIBE);

    nng_mqtt_topic topic = { .buf    = (uint8_t *) tuple->topic,
                             .length = strlen(tuple->topic) };
    nng_mqtt_msg_set_unsubscribe_topics(unsubscribe_msg, &topic, 1);

    if (1 == client->option->verbose) {
        uint8_t buff[1024] = { 0 };
        nng_mqtt_msg_dump(unsubscribe_msg, buff, sizeof(buff), true);
        log_info("%s", buff);
    }

    return client_send(client, unsubscribe_msg);
}

static void client_unsubscribe_remove(mqtt_nng_client_t *     client,
                                      struct subscribe_tuple *tuple)
{
    TAILQ_REMOVE(&client->head, tuple, entry);
    client_subscribe_destroy(tuple);
}

neu_err_code_e mqtt_nng_client_unsubscribe(mqtt_nng_client_t *client,
                                           const char *       topic)
{
    if (NULL == client) {
        return NEU_ERR_MQTT_IS_NULL;
    }

    if (NULL == topic || 0 == strlen(topic)) {
        return NEU_ERR_MQTT_TOPIC_EMPTY;
    }

    struct subscribe_tuple *tuple = client_unsubscribe_create(client, topic);
    if (NULL == tuple) {
        return NEU_ERR_MQTT_UNSUBSCRIBE_FAILURE;
    }

    client_unsubscribe_send(client, tuple);
    client_unsubscribe_remove(client, tuple);
    return NEU_ERR_SUCCESS;
}

neu_err_code_e mqtt_nng_client_publish(mqtt_nng_client_t *client,
                                       const char *topic, int qos,
                                       unsigned char *payload, size_t len)
{
    if (NULL == client) {
        return NEU_ERR_MQTT_IS_NULL;
    }

    if (NULL == topic || 0 == strlen(topic)) {
        return NEU_ERR_MQTT_TOPIC_EMPTY;
    }

    nng_msg *publish_msg = NULL;
    nng_mqtt_msg_alloc(&publish_msg, 0);
    nng_mqtt_msg_set_packet_type(publish_msg, NNG_MQTT_PUBLISH);
    nng_mqtt_msg_set_publish_dup(publish_msg, 0);
    nng_mqtt_msg_set_publish_qos(publish_msg, qos);
    nng_mqtt_msg_set_publish_retain(publish_msg, 0);
    nng_mqtt_msg_set_publish_payload(publish_msg, (uint8_t *) payload, len);
    nng_mqtt_msg_set_publish_topic(publish_msg, topic);

    if (1 == client->option->verbose) {
        uint8_t buff[1024] = { 0 };
        nng_mqtt_msg_dump(publish_msg, buff, sizeof(buff), true);
        log_info("%s", buff);
    }

    return client_send(client, publish_msg);
}

static void client_disconnect(mqtt_nng_client_t *client)
{
    while (!TAILQ_EMPTY(&client->head)) {
        struct subscribe_tuple *tuple = TAILQ_FIRST(&client->head);
        if (NULL != tuple) {
            mqtt_nng_client_unsubscribe(client, tuple->topic);
        }
    }

    nng_mtx_lock(client->mtx);
    client->running = false;
    nng_mtx_unlock(client->mtx);

    log_debug("nng socket close...");
    nng_close(client->sock);
    log_debug("nng socket closed");
    nng_thread_destroy(client->daemon);
    log_debug("nng thread destroy");
}

static void client_destroy(mqtt_nng_client_t *client)
{
    nng_mtx_free(client->mtx);
    nng_msg_free(client->connect_msg);

    if (NULL != client->url) {
        free(client->url);
    }

    if (NULL != client->cert) {
        free(client->cert);
    }

    if (NULL != client->key) {
        free(client->key);
    }

    if (NULL != client->keypass) {
        free(client->keypass);
    }

    if (NULL != client->ca) {
        free(client->ca);
    }

    if (NULL != client->config) {
        nng_tls_config_free(client->config);
    }

    free(client);
}

neu_err_code_e mqtt_nng_client_suspend(mqtt_nng_client_t *client)
{
    // TODO: suspend logic
    return NEU_ERR_SUCCESS;
}

neu_err_code_e mqtt_nng_client_continue(mqtt_nng_client_t *client)
{
    // TODO: continue logic
    return NEU_ERR_SUCCESS;
}

neu_err_code_e mqtt_nng_client_close(mqtt_nng_client_t *client)
{
    if (NULL == client) {
        return NEU_ERR_MQTT_IS_NULL;
    }

    client_disconnect(client);
    client_destroy(client);
    return NEU_ERR_SUCCESS;
}
