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

#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <mqtt.h>
#include <neuron.h>

#include "mqtt_c_client.h"

#define UNUSED(x) (void) (x)
#define INTERVAL 100000U
#define SEND_BUF_SIZE 1024 * 1024
#define RECV_BUF_SIZE 8192

struct reconnect_state {
    const char *     hostname;
    const char *     port;
    const char *     client_id;
    const char *     topic;
    mqtt_c_client_t *client;
    uint8_t *        send_buf;
    size_t           send_buf_sz;
    uint8_t *        recv_buf;
    size_t           recv_buf_sz;
};

struct mqtt_c_client {
    const neu_mqtt_option_t *option;
    neu_list                 subscribe_list;
    struct mqtt_client       mqtt;
    struct reconnect_state   state;
    bool                     running;
    SSL_CTX *                ssl_ctx;
    BIO *                    sock_fd;
    uint8_t                  send_buf[SEND_BUF_SIZE];
    uint8_t                  recv_buf[RECV_BUF_SIZE];
    pthread_t                client_daemon;
    pthread_mutex_t          mutex;
    pthread_mutex_t          working_mutex;
    bool                     working;
    void *                   user_data;
};

struct subscribe_tuple {
    neu_list_node        node;
    char *               topic;
    int                  qos;
    neu_subscribe_handle handle;
    bool                 subscribed;
    mqtt_c_client_t *    client;
};

static void ssl_ctx_uninit(SSL_CTX *ssl_ctx)
{
    if (NULL != ssl_ctx) {
        SSL_CTX_free(ssl_ctx);
    }
}

static SSL_CTX *ssl_ctx_init(const char *ca_file, const char *ca_path)
{
    SSL_CTX *ssl_ctx = NULL;
    ssl_ctx          = SSL_CTX_new(SSLv23_client_method());
    if (NULL == ssl_ctx) {
        log_error("Failed to create ssl ctx");
        return NULL;
    }

    if (!SSL_CTX_load_verify_locations(ssl_ctx, ca_file, ca_path)) {
        log_error("Failed to load certificate");
        ssl_ctx_uninit(ssl_ctx);
        return NULL;
    }

    return ssl_ctx;
}

static BIO *bio_socket_tcp_create(const char *addr, const char *port)
{
    BIO *bio = BIO_new_connect(addr);
    BIO_set_nbio(bio, 1);
    BIO_set_conn_port(bio, port);
    return bio;
}

static BIO *bio_socket_ssl_create(SSL_CTX *ssl_ctx, const char *addr,
                                  const char *port)
{
    SSL *ssl = NULL;
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

static void bio_socket_destroy(BIO *bio)
{
    if (NULL != bio) {
        BIO_free_all(bio);
    }
}

static void bio_socket_connect(BIO *bio)
{
    int start_time    = time(NULL);
    int do_connect_rv = BIO_do_connect(bio);
    while (do_connect_rv <= 0 && BIO_should_retry(bio) &&
           (int) time(NULL) - start_time < 10) {
        do_connect_rv = BIO_do_connect(bio);
    }
    if (do_connect_rv <= 0) {
        return;
    }
}

static neu_err_code_e client_subscribe_send(mqtt_c_client_t *       client,
                                            struct subscribe_tuple *tuple);

static void subscribe_all_topic(mqtt_c_client_t *client)
{
    struct subscribe_tuple *item = NULL;
    NEU_LIST_FOREACH(&client->subscribe_list, item)
    {
        client_subscribe_send(client, item);
    }
}

static void client_reconnect(struct mqtt_client *mqtt, void **p_reconnect_state)
{
    struct reconnect_state *state =
        *((struct reconnect_state **) p_reconnect_state);
    mqtt_c_client_t *        client = state->client;
    const neu_mqtt_option_t *option = client->option;

    // If not first connection
    // if (mqtt->error != MQTT_ERROR_INITIAL_RECONNECT) { }

    bio_socket_destroy(client->sock_fd);

    if (0 == strcmp(option->connection, "tcp://")) {
        client->sock_fd = bio_socket_tcp_create(option->host, option->port);
    }

    if (0 == strcmp(option->connection, "ssl://")) {
        client->sock_fd =
            bio_socket_ssl_create(client->ssl_ctx, option->host, option->port);
    }

    bio_socket_connect(client->sock_fd);

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
    struct reconnect_state *state =
        *((struct reconnect_state **) p_reconnect_state);
    mqtt_c_client_t *client = state->client;

    pthread_mutex_lock(&client->working_mutex);
    if (!client->working) {
        pthread_mutex_unlock(&client->working_mutex);
        return;
    }
    pthread_mutex_unlock(&client->working_mutex);

    char *topic_name = (char *) malloc(published->topic_name_size + 1);
    memcpy(topic_name, published->topic_name, published->topic_name_size);
    topic_name[published->topic_name_size] = '\0';

    log_info("Received publish('%s'): %s", topic_name,
             (const char *) published->application_message);

    struct subscribe_tuple *item = NULL;
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

static void *client_refresher(void *context)
{
    mqtt_c_client_t *   client = (mqtt_c_client_t *) context;
    struct mqtt_client *mqtt   = &client->mqtt;

    while (1) {
        pthread_mutex_lock(&client->mutex);
        if (!client->running) {
            pthread_mutex_unlock(&client->mutex);
            break;
        }
        pthread_mutex_unlock(&client->mutex);

        mqtt_sync((struct mqtt_client *) mqtt);
        usleep(INTERVAL);
    }

    return NULL;
}

static mqtt_c_client_t *client_create(const neu_mqtt_option_t *option,
                                      void *                   context)
{
    mqtt_c_client_t *client = malloc(sizeof(mqtt_c_client_t));
    if (NULL == client) {
        log_error("Failed to malloc client memory");
        return NULL;
    }
    memset(client, 0, sizeof(mqtt_c_client_t));

    pthread_mutex_init(&client->mutex, NULL);
    pthread_mutex_init(&client->working_mutex, NULL);
    client->option    = option;
    client->running   = true;
    client->user_data = context;
    client->working   = true;

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

static neu_err_code_e client_init(mqtt_c_client_t *client)
{
    SSL_load_error_strings();
    ERR_load_BIO_strings();
    OpenSSL_add_all_algorithms();
    SSL_library_init();

    BIO *                    bio     = NULL;
    SSL_CTX *                ssl_ctx = NULL;
    const neu_mqtt_option_t *option  = client->option;
    // if (0 == strcmp(option->connection, "tcp://")) { }

    if (0 == strcmp(option->connection, "ssl://")) {
        ssl_ctx = ssl_ctx_init(option->ca_file, option->ca_path);
    }

    client->ssl_ctx = ssl_ctx;
    client->sock_fd = bio;
    return NEU_ERR_SUCCESS;
}

static neu_err_code_e client_connect(mqtt_c_client_t *client)
{
    if (NULL == client) {
        log_error("The client is not initialized");
        return NEU_ERR_MQTT_IS_NULL;
    }

    mqtt_init_reconnect(&client->mqtt, client_reconnect, &client->state,
                        publish_callback);

    if (pthread_create(&client->client_daemon, NULL, client_refresher,
                       client)) {
        log_error("Failed to start client daemon.");
        return NEU_ERR_MQTT_CONNECT_FAILURE;
    }

    log_info("Successfully start the request of connect.");
    return NEU_ERR_SUCCESS;
}

neu_err_code_e mqtt_c_client_open(mqtt_c_client_t **       p_client,
                                  const neu_mqtt_option_t *option,
                                  void *                   context)
{
    mqtt_c_client_t *client = client_create(option, context);
    if (NULL == client) {
        return NEU_ERR_MQTT_IS_NULL;
    }

    neu_err_code_e rc = client_init(client);
    rc                = client_connect(client);
    *p_client         = client;
    return rc;
}

neu_err_code_e mqtt_c_client_is_connected(mqtt_c_client_t *client)
{
    if (NULL == client) {
        return NEU_ERR_MQTT_IS_NULL;
    }

    if (1 != client->mqtt.error) {
        return NEU_ERR_MQTT_CONNECT_FAILURE;
    }

    return NEU_ERR_SUCCESS;
}

static neu_err_code_e client_subscribe_destroy(struct subscribe_tuple *tuple)
{
    if (NULL != tuple) {
        if (NULL != tuple->topic) {
            free(tuple->topic);
        }
        free(tuple);
    }

    return NEU_ERR_SUCCESS;
}

static neu_err_code_e client_disconnect(mqtt_c_client_t *client)
{
    while (!neu_list_empty(&client->subscribe_list)) {
        struct subscribe_tuple *tuple = NULL;
        tuple                         = neu_list_first(&client->subscribe_list);
        if (NULL != tuple) {
            mqtt_c_client_unsubscribe(client, tuple->topic);
            neu_list_remove(&client->subscribe_list, tuple);
            client_subscribe_destroy(tuple);
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

    return NEU_ERR_SUCCESS;
}

static neu_err_code_e client_destroy(mqtt_c_client_t *client)
{
    pthread_mutex_destroy(&client->mutex);
    pthread_mutex_destroy(&client->working_mutex);
    free(client);
    return NEU_ERR_SUCCESS;
}

static struct subscribe_tuple *
client_subscribe_create(mqtt_c_client_t *client, const char *topic,
                        const int qos, const neu_subscribe_handle handle)
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

static neu_err_code_e client_subscribe_send(mqtt_c_client_t *       client,
                                            struct subscribe_tuple *tuple)
{
    neu_err_code_e error = mqtt_c_client_is_connected(client);
    if (0 != error) {
        log_error("Failed to start send message, return code %d", error);
        return error;
    }

    int rc = mqtt_subscribe(&client->mqtt, tuple->topic, tuple->qos);
    if (1 != rc) {
        log_error("Failed to start subscribe, return code %d", rc);
        return NEU_ERR_MQTT_SUBSCRIBE_FAILURE;
    }

    return NEU_ERR_SUCCESS;
}

static neu_err_code_e client_subscribe_add(mqtt_c_client_t *       client,
                                           struct subscribe_tuple *tuple)
{
    NEU_LIST_NODE_INIT(&tuple->node);
    neu_list_append(&client->subscribe_list, tuple);
    return NEU_ERR_SUCCESS;
}

neu_err_code_e mqtt_c_client_subscribe(mqtt_c_client_t *client,
                                       const char *topic, const int qos,
                                       neu_subscribe_handle handle)
{
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
client_unsubscribe_create(mqtt_c_client_t *client, const char *topic)
{
    struct subscribe_tuple *item = NULL;
    NEU_LIST_FOREACH(&client->subscribe_list, item)
    {
        if (0 == strcmp(item->topic, topic)) {
            return item;
        }
    }

    log_error("Cant find topic %s", topic);
    return NULL;
}

static neu_err_code_e client_unsubscribe_send(mqtt_c_client_t *       client,
                                              struct subscribe_tuple *tuple)
{
    int rc = mqtt_unsubscribe(&client->mqtt, tuple->topic);
    if (1 != rc) {
        return NEU_ERR_MQTT_UNSUBSCRIBE_FAILURE;
    }

    return NEU_ERR_SUCCESS;
}

neu_err_code_e mqtt_c_client_unsubscribe(mqtt_c_client_t *client,
                                         const char *     topic)
{
    if (NULL == client) {
        return NEU_ERR_MQTT_IS_NULL;
    }

    struct subscribe_tuple *tuple = client_unsubscribe_create(client, topic);
    if (NULL == tuple) {
        return NEU_ERR_MQTT_UNSUBSCRIBE_FAILURE;
    }

    client_unsubscribe_send(client, tuple);
    return NEU_ERR_SUCCESS;
}

neu_err_code_e mqtt_c_client_publish(mqtt_c_client_t *client, const char *topic,
                                     int qos, unsigned char *payload,
                                     size_t len)
{
    if (SEND_BUF_SIZE < len) {
        return NEU_ERR_MQTT_PUBLISH_OVER_LENGTH;
    }

    neu_err_code_e error = mqtt_c_client_is_connected(client);
    if (0 != error) {
        log_error("Failed to start send message, return code %d", error);
        return error;
    }

    pthread_mutex_lock(&client->working_mutex);
    if (!client->working) {
        pthread_mutex_unlock(&client->working_mutex);
        return NEU_ERR_MQTT_SUSPENDED;
    }
    pthread_mutex_unlock(&client->working_mutex);

    error = mqtt_publish(&client->mqtt, topic, payload, len, qos);
    if (1 != error) {
        log_error("Failed to start send message, return code %d", error);
        return NEU_ERR_MQTT_PUBLISH_FAILURE;
    }

    log_info("Publish to topic %s for using QoS%d", topic, qos);
    return NEU_ERR_SUCCESS;
}

neu_err_code_e mqtt_c_client_suspend(mqtt_c_client_t *client)
{
    pthread_mutex_lock(&client->working_mutex);
    client->working = false;
    pthread_mutex_unlock(&client->working_mutex);
    return NEU_ERR_SUCCESS;
}

neu_err_code_e mqtt_c_client_continue(mqtt_c_client_t *client)
{
    pthread_mutex_lock(&client->working_mutex);
    client->working = true;
    pthread_mutex_unlock(&client->working_mutex);
    return NEU_ERR_SUCCESS;
}

neu_err_code_e mqtt_c_client_close(mqtt_c_client_t *client)
{
    if (NULL == client) {
        return NEU_ERR_MQTT_IS_NULL;
    }

    pthread_mutex_lock(&client->mutex);
    client->running = false;
    pthread_mutex_unlock(&client->mutex);

    pthread_join(client->client_daemon, NULL);
    neu_err_code_e rc = client_disconnect(client);
    rc                = client_destroy(client);
    return rc;
}
