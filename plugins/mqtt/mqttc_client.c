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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <mqtt.h>
#include <neuron.h>

#include "mqttc_client.h"

#define UNUSED(x) (void) (x)
#define INTERVAL 100000U
#define SEND_BUF_SIZE 2048
#define RECV_BUF_SIZE 2048

struct reconnect_state {
    const char *    hostname;
    const char *    port;
    const char *    client_id;
    const char *    topic;
    mqttc_client_t *client;
    uint8_t *       send_buf;
    size_t          send_buf_sz;
    uint8_t *       recv_buf;
    size_t          recv_buf_sz;
};

struct mqttc_client {
    option_t *             option;
    neu_list               subscribe_list;
    struct mqtt_client     mqtt;
    struct reconnect_state state;
    bool                   running;
    SSL_CTX *              ssl_ctx;
    BIO *                  sock_fd;
    uint8_t                send_buf[SEND_BUF_SIZE];
    uint8_t                recv_buf[RECV_BUF_SIZE];
    pthread_t              client_daemon;
    pthread_mutex_t        mutex;
    void *                 user_data;
};

struct subscribe_tuple {
    neu_list_node    node;
    char *           topic;
    int              qos;
    subscribe_handle handle;
    bool             subscribed;
    mqttc_client_t * client;
};

static BIO *create_bio_socket_tcp(const char *addr, const char *port)
{
    BIO *bio = BIO_new_connect(addr);
    BIO_set_nbio(bio, 1);
    BIO_set_conn_port(bio, port);
    return bio;
}

static BIO *create_bio_socket_ssl(SSL_CTX *ssl_ctx, const char *addr,
                                  const char *port)
{
    SSL *ssl;
    BIO *bio = BIO_new_ssl_connect(ssl_ctx);
    BIO_get_ssl(bio, &ssl);
    SSL_set_mode(ssl, SSL_MODE_AUTO_RETRY);
    BIO_set_conn_hostname(bio, addr);
    BIO_set_nbio(bio, 1);
    BIO_set_conn_port(bio, port);

    if (SSL_get_verify_result(ssl) != X509_V_OK) {
        log_error("error: x509 certificate verification failed");
    }
    return bio;
}

static void destroy_bio_socket(BIO *bio)
{
    if (NULL != bio) {
        BIO_free_all(bio);
    }
}

// static void connect_bio_socket(BIO *bio)
// {
//     int start_time = time(NULL);
//     while (BIO_do_connect(bio) == 0 && (int) time(NULL) - start_time < 10)
//         ;

//     if (BIO_do_connect(bio) <= 0) {
//         log_error("Failed to open socket: BIO_do_connect returned <= 0");
//     }
// }

static void connect_bio_socket(BIO *bio)
{
    int start_time    = time(NULL);
    int do_connect_rv = BIO_do_connect(bio);
    while (do_connect_rv <= 0 && BIO_should_retry(bio) &&
           (int) time(NULL) - start_time < 10) {
        do_connect_rv = BIO_do_connect(bio);
    }
    if (do_connect_rv <= 0) {
        // log_error("error: %s", ERR_reason_error_string(ERR_get_error()));
        return;
    }
}

client_error_e mqttc_client_subscribe_send(mqttc_client_t *        client,
                                           struct subscribe_tuple *tuple);

static void subscribe_all_topic(mqttc_client_t *client)
{
    struct subscribe_tuple *item;
    NEU_LIST_FOREACH(&client->subscribe_list, item)
    {
        mqttc_client_subscribe_send(client, item);
    }
}

static void reconnect_mqtt(struct mqtt_client *mqtt, void **p_reconnect_state)
{
    struct reconnect_state *state =
        *((struct reconnect_state **) p_reconnect_state);
    mqttc_client_t *client = state->client;
    option_t *      option = client->option;

    // If not first connection
    // if (mqtt->error != MQTT_ERROR_INITIAL_RECONNECT) { }

    destroy_bio_socket(client->sock_fd);

    if (0 == strcmp(option->connection, "tcp://")) {
        client->sock_fd = create_bio_socket_tcp(option->host, option->port);
    }

    if (0 == strcmp(option->connection, "ssl://")) {
        client->sock_fd =
            create_bio_socket_ssl(client->ssl_ctx, option->host, option->port);
    }

    connect_bio_socket(client->sock_fd);

    mqtt_reinit(mqtt, client->sock_fd, state->send_buf, state->send_buf_sz,
                state->recv_buf, state->recv_buf_sz);

    const char *client_id     = state->client_id;
    uint8_t     connect_flags = MQTT_CONNECT_RESERVED;
    if (1 == option->clean_session) {
        connect_flags = MQTT_CONNECT_CLEAN_SESSION;
    }
    enum MQTTErrors rc = mqtt_connect(
        mqtt, client_id, NULL, NULL, 0, option->username, option->password,
        connect_flags, option->keepalive_interval);
    if (MQTT_OK != rc) {
        return;
    }

    subscribe_all_topic(client);
}

static void publish_callback(void **                       p_reconnect_state,
                             struct mqtt_response_publish *published)
{
    char *topic_name = (char *) malloc(published->topic_name_size + 1);
    memcpy(topic_name, published->topic_name, published->topic_name_size);
    topic_name[published->topic_name_size] = '\0';

    log_info("Received publish('%s'): %s", topic_name,
             (const char *) published->application_message);

    struct reconnect_state *state =
        *((struct reconnect_state **) p_reconnect_state);
    mqttc_client_t *        client = state->client;
    struct subscribe_tuple *item;
    NEU_LIST_FOREACH(&client->subscribe_list, item)
    {
        if (0 == strcmp(item->topic, topic_name) && NULL != item->handle) {
            item->handle(topic_name, strlen(topic_name),
                         (void *) published->application_message,
                         published->application_message_size,
                         client->user_data);
        }
    }

    free(topic_name);
}

static void *mqtt_refresher(void *context)
{
    mqttc_client_t *    client = (mqttc_client_t *) context;
    struct mqtt_client *mqtt   = &client->mqtt;

    bool run_flag = true;
    while (1) {
        pthread_mutex_lock(&client->mutex);
        run_flag = client->running;
        pthread_mutex_unlock(&client->mutex);

        if (!run_flag) {
            break;
        }

        mqtt_sync((struct mqtt_client *) mqtt);
        usleep(INTERVAL);
    }

    return NULL;
}

mqttc_client_t *mqttc_client_create(option_t *option, void *context)
{
    mqttc_client_t *client = malloc(sizeof(mqttc_client_t));
    if (NULL == client) {
        log_error("Failed to malloc client memory");
        return NULL;
    }
    memset(client, 0, sizeof(mqttc_client_t));

    pthread_mutex_init(&client->mutex, NULL);
    client->option    = option;
    client->running   = true;
    client->user_data = context;

    struct reconnect_state *state = &client->state;
    state->hostname               = client->option->host;
    state->port                   = client->option->port;
    state->client_id              = client->option->clientid;
    state->topic                  = client->option->topic;
    state->client                 = client;
    state->send_buf               = client->send_buf;
    state->send_buf_sz            = sizeof(client->send_buf);
    state->recv_buf               = client->recv_buf;
    state->recv_buf_sz            = sizeof(client->recv_buf);

    client->mqtt.publish_response_callback_state = state;

    NEU_LIST_INIT(&client->subscribe_list, struct subscribe_tuple, node);
    return client;
}

client_error_e mqttc_client_init(mqttc_client_t *client)
{
    SSL_load_error_strings();
    ERR_load_BIO_strings();
    OpenSSL_add_all_algorithms();
    SSL_library_init();

    BIO *     bio     = NULL;
    SSL_CTX * ssl_ctx = NULL;
    option_t *option  = client->option;
    // if (0 == strcmp(option->connection, "tcp://")) { }

    if (0 == strcmp(option->connection, "ssl://")) {
        ssl_ctx = ssl_ctx_init(option->cafile, option->capath);
    }

    client->ssl_ctx = ssl_ctx;
    client->sock_fd = bio;
    return ClientSuccess;
}

client_error_e mqttc_client_connect(mqttc_client_t *client)
{
    if (NULL == client) {
        log_error("The client is not initialized");
        return ClientIsNULL;
    }

    mqtt_init_reconnect(&client->mqtt, reconnect_mqtt, &client->state,
                        publish_callback);

    if (pthread_create(&client->client_daemon, NULL, mqtt_refresher, client)) {
        log_error("Failed to start client daemon.");
        return ClientConnectFailure;
    }

    log_info("Successfully start the request of connect.");
    return ClientSuccess;
}

client_error_e mqttc_client_open(option_t *option, void *context,
                                 mqttc_client_t **p_client)
{
    mqttc_client_t *client = mqttc_client_create(option, context);
    if (NULL == client) {
        return ClientIsNULL;
    }

    client_error_e rc = mqttc_client_init(client);
    rc                = mqttc_client_connect(client);
    *p_client         = client;
    return rc;
}

client_error_e mqttc_client_is_connected(mqttc_client_t *client)
{
    if (NULL == client) {
        return ClientIsNULL;
    }

    if (1 != client->mqtt.error) {
        return ClientConnectFailure;
    }

    return ClientSuccess;
}

client_error_e mqttc_client_subscribe_destroy(struct subscribe_tuple *tuple)
{
    if (NULL != tuple) {
        if (NULL != tuple->topic) {
            free(tuple->topic);
        }
        free(tuple);
    }

    return ClientSuccess;
}

client_error_e mqttc_client_disconnect(mqttc_client_t *client)
{
    while (!neu_list_empty(&client->subscribe_list)) {
        struct subscribe_tuple *tuple = NULL;
        tuple                         = neu_list_first(&client->subscribe_list);
        if (NULL != tuple) {
            mqttc_client_unsubscribe(client, tuple->topic);
            neu_list_remove(&client->subscribe_list, tuple);
            mqttc_client_subscribe_destroy(tuple);
        }
    }

    if (NULL != client) {
        int rc = mqtt_disconnect(&client->mqtt);
        if (1 != rc) {
            log_error("Disconnect connection fail");
        }

        if (NULL != client->sock_fd) {
            BIO_free_all(client->sock_fd);
        }

        ssl_ctx_uninit(client->ssl_ctx);
    }

    return ClientSuccess;
}

client_error_e mqttc_client_destroy(mqttc_client_t *client)
{
    pthread_mutex_destroy(&client->mutex);
    free(client);
    return ClientSuccess;
}

struct subscribe_tuple *
mqttc_client_subscribe_create(mqttc_client_t *client, const char *topic,
                              const int qos, const subscribe_handle handle)
{
    struct subscribe_tuple *tuple =
        (struct subscribe_tuple *) malloc(sizeof(struct subscribe_tuple));

    if (NULL == tuple) {
        log_error("Failed to malloc client memory");
        return NULL;
    }

    tuple->topic      = strdup(topic);
    tuple->qos        = qos;
    tuple->handle     = handle;
    tuple->subscribed = false;
    tuple->client     = client;
    return tuple;
}

client_error_e mqttc_client_subscribe_send(mqttc_client_t *        client,
                                           struct subscribe_tuple *tuple)
{
    client_error_e error = mqttc_client_is_connected(client);
    if (0 != error) {
        log_error("Failed to start send message, return code %d", error);
        return error;
    }

    int rc = mqtt_subscribe(&client->mqtt, tuple->topic, tuple->qos);
    if (1 != rc) {
        log_error("Failed to start subscribe, return code %d", rc);
        return ClientSubscribeFailure;
    }

    return ClientSuccess;
}

client_error_e mqttc_client_subscribe_add(mqttc_client_t *        client,
                                          struct subscribe_tuple *tuple)
{
    NEU_LIST_NODE_INIT(&tuple->node);
    neu_list_append(&client->subscribe_list, tuple);
    return ClientSuccess;
}

client_error_e mqttc_client_subscribe(mqttc_client_t *client, const char *topic,
                                      const int qos, subscribe_handle handle)
{
    struct subscribe_tuple *tuple =
        mqttc_client_subscribe_create(client, topic, qos, handle);
    if (NULL == tuple) {
        return ClientSubscribeFailure;
    }

    mqttc_client_subscribe_add(client, tuple);
    client_error_e error = mqttc_client_subscribe_send(client, tuple);
    if (ClientSuccess != error) {
        return ClientSubscribeFailure;
    }

    log_info("Subscribing to topic %s for using QoS%d", topic, qos);
    return ClientSuccess;
}

struct subscribe_tuple *mqttc_client_unsubscribe_create(mqttc_client_t *client,
                                                        const char *    topic)
{
    struct subscribe_tuple *item;
    NEU_LIST_FOREACH(&client->subscribe_list, item)
    {
        if (0 == strcmp(item->topic, topic)) {
            return item;
        }
    }

    log_error("Cant find topic %s", topic);
    return NULL;
}

client_error_e mqttc_client_unsubscribe_send(mqttc_client_t *        client,
                                             struct subscribe_tuple *tuple)
{
    int rc = mqtt_unsubscribe(&client->mqtt, tuple->topic);
    if (1 != rc) {
        return ClientUnsubscribeFailure;
    }

    return ClientSuccess;
}

client_error_e mqttc_client_unsubscribe(mqttc_client_t *client,
                                        const char *    topic)
{
    if (NULL == client) {
        return ClientIsNULL;
    }

    struct subscribe_tuple *tuple =
        mqttc_client_unsubscribe_create(client, topic);
    if (NULL == tuple) {
        return ClientUnsubscribeFailure;
    }

    mqttc_client_unsubscribe_send(client, tuple);
    // mqttc_client_subscribe_remove(client, topic);
    return ClientSuccess;
}

client_error_e mqttc_client_publish(mqttc_client_t *client, const char *topic,
                                    int qos, unsigned char *payload, size_t len)
{
    client_error_e error = mqttc_client_is_connected(client);
    if (0 != error) {
        log_error("Failed to start send message, return code %d", error);
        return error;
    }

    error = mqtt_publish(&client->mqtt, topic, payload, len, qos);
    if (1 != error) {
        log_error("Failed to start send message, return code %d", error);
        return ClientPublishFailure;
    }

    log_info("Publish to topic %s for using QoS%d", topic, qos);
    return ClientSuccess;
}

client_error_e mqttc_client_close(mqttc_client_t *client)
{
    if (NULL == client) {
        return ClientIsNULL;
    }

    pthread_mutex_lock(&client->mutex);
    client->running = false;
    pthread_mutex_unlock(&client->mutex);

    pthread_join(client->client_daemon, NULL);
    client_error_e rc = mqttc_client_disconnect(client);
    rc                = mqttc_client_destroy(client);
    return rc;
}
