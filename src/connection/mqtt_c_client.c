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

#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <mqtt.h>
#include <neuron.h>

#include "mqtt_c_client.h"
#include "utils/log.h"

#define UNUSED(x) (void) (x)
#define INTERVAL 100000U
#define SEND_BUF_SIZE 1024 * 1024
#define RECV_BUF_SIZE 8192

struct reconnect_state {
    char *           hostname;
    char *           port;
    char *           client_id;
    char *           connection;
    char *           username;
    char *           password;
    int              keepalive;
    int              clean;
    char *           will_topic;
    char *           will_payload;
    int              will_payload_len;
    char *           ca_file;
    char *           ca_path;
    char *           cert_file;
    char *           key_file;
    char *           keypass;
    mqtt_c_client_t *client;
    uint8_t          send_buf[SEND_BUF_SIZE];
    uint8_t          recv_buf[RECV_BUF_SIZE];
    size_t           send_buf_sz;
    size_t           recv_buf_sz;
    X509 *           cert;
    EVP_PKEY *       key;
    SSL_CTX *        ssl_ctx;
    BIO *            sock_fd;
    int              sock_state; // sock state, value same as mqtt.error
    bool             running;    // refresher stop flag
    bool             working;    // publish & publish callback working flag
    pthread_mutex_t  sock_state_mutex;
    pthread_mutex_t  running_mutex;
    pthread_mutex_t  working_mutex;
};

struct subscribe_tuple {
    char *               topic;
    int                  qos;
    neu_subscribe_handle handle;
    mqtt_c_client_t *    client;
};

struct mqtt_c_client {
    const neu_mqtt_option_t *option;
    struct mqtt_client       mqtt;
    struct reconnect_state   state;
    pthread_t                daemon;
    void *                   user_data;
    state_update             state_update_func;
    zlog_category_t *        log;
    UT_array *               array;
};

static neu_err_code_e client_destroy(mqtt_c_client_t *client);

static neu_err_code_e client_subscribe_send(mqtt_c_client_t *       client,
                                            struct subscribe_tuple *tuple);

static X509 *ssl_ctx_load_cert(const char *b64, const char *password)
{
    unsigned char *bin = NULL;
    int            len = 0;
    bin                = neu_decode64(&len, b64);
    if (NULL == bin) {
        return NULL;
    }

    X509 *cert = NULL;
    BIO * bio  = NULL;
    bio        = BIO_new_mem_buf((void *) bin, len);
    cert       = PEM_read_bio_X509(bio, NULL, 0, (void *) password);
    BIO_free_all(bio);
    free(bin);
    return cert;
}

static EVP_PKEY *ssl_ctx_load_key(const char *b64, const char *password)
{
    unsigned char *bin = NULL;
    int            len = 0;
    bin                = neu_decode64(&len, b64);
    if (NULL == bin) {
        return NULL;
    }

    EVP_PKEY *key = NULL;
    BIO *     bio = NULL;
    bio           = BIO_new_mem_buf((void *) bin, len);
    key           = PEM_read_bio_PrivateKey(bio, NULL, 0, (void *) password);
    BIO_free_all(bio);
    free(bin);
    return key;
}

bool ssl_ctx_load_ca(SSL_CTX *ctx, const char *b64)
{
    unsigned char *bin = NULL;
    int            len = 0;
    bin                = neu_decode64(&len, b64);
    if (NULL == bin) {
        return false;
    }

    BIO *cbio = BIO_new_mem_buf((void *) bin, len);
    if (!cbio) {
        free(bin);
        return false;
    }

    X509_STORE *cts = SSL_CTX_get_cert_store(ctx);
    if (!cts) {
        free(bin);
        return false;
    }

    X509_INFO *itmp          = NULL;
    int        i             = 0;
    int        count         = 0;
    STACK_OF(X509_INFO) *inf = PEM_X509_INFO_read_bio(cbio, NULL, NULL, NULL);

    if (!inf) {
        BIO_free(cbio);
        free(bin);
        return false;
    }

    for (i = 0; i < sk_X509_INFO_num(inf); i++) {
        itmp = sk_X509_INFO_value(inf, i);
        if (itmp->x509) {
            X509_STORE_add_cert(cts, itmp->x509);
            count++;
        }

        if (itmp->crl) {
            X509_STORE_add_crl(cts, itmp->crl);
            count++;
        }
    }

    sk_X509_INFO_pop_free(inf, X509_INFO_free);
    BIO_free(cbio);
    free(bin);
    return true;
}

static void ssl_ctx_init(struct reconnect_state *state)
{
    assert(NULL != state);

    const char *ca_file   = state->ca_file;
    const char *cert_file = state->cert_file;
    const char *key_file  = state->key_file;

    assert(NULL != ca_file && 0 < strlen(ca_file));

    SSL_load_error_strings();
    ERR_load_BIO_strings();
    ERR_load_crypto_strings();
    OpenSSL_add_all_algorithms();
    SSL_library_init();
    state->ssl_ctx = SSL_CTX_new(SSLv23_client_method());

    mqtt_c_client_t *client = state->client;

    // setup cert
    if (NULL != cert_file && 0 < strlen(cert_file)) {
        state->cert = ssl_ctx_load_cert(cert_file, NULL);
        if (NULL != state->cert) {
            if (0 >= SSL_CTX_use_certificate(state->ssl_ctx, state->cert)) {
                if (NULL != client->log) {
                    zlog_error(client->log, "failed to use certificate");
                }
            }
        } else {
            if (NULL != client->log) {
                zlog_error(client->log, "failed to load certificate");
            }
        }
    }

    // setup key
    if (NULL != key_file && 0 < strlen(key_file)) {
        const char *keypass = state->keypass;
        state->key          = ssl_ctx_load_key(key_file, keypass);
        if (NULL != state->key) {
            if (0 >= SSL_CTX_use_PrivateKey(state->ssl_ctx, state->key)) {
                if (NULL != client->log) {
                    zlog_error(client->log, "failed to use privatekey");
                }
            }
        } else {
            if (NULL != client->log) {
                zlog_error(client->log, "failed to load privatekey");
            }
        }
    }

    if (NULL != state->cert && NULL != state->key) {
        if (!SSL_CTX_check_private_key(state->ssl_ctx)) {
            if (NULL != client->log) {
                zlog_error(client->log, "failed to check privatekey");
            }
        }
    }

    // setup ca from base64
    if (!ssl_ctx_load_ca(state->ssl_ctx, ca_file)) {
        if (NULL != client->log) {
            zlog_error(client->log, "failed to load ca");
        }
    }
}

static void ssl_ctx_uninit(struct reconnect_state *state)
{
    assert(NULL != state);

    if (NULL != state->ssl_ctx) {
        SSL_CTX_free(state->ssl_ctx);
    }

    if (NULL != state->cert) {
        X509_free(state->cert);
    }

    if (NULL != state->key) {
        EVP_PKEY_free(state->key);
    }
}

static BIO *bio_tcp_create(const char *addr, const char *port)
{
    assert(NULL != addr && 0 < strlen(addr));
    assert(NULL != port && 0 < strlen(port));

    BIO *bio = BIO_new_connect(addr);
    if (NULL == bio) {
        return NULL;
    }

    BIO_set_nbio(bio, 1);
    BIO_set_conn_port(bio, port);
    return bio;
}

static BIO *bio_ssl_create(SSL_CTX *ssl_ctx, const char *addr, const char *port)
{
    assert(NULL != ssl_ctx);
    assert(NULL != addr && 0 < strlen(addr));
    assert(NULL != port && 0 < strlen(port));

    SSL *ssl = NULL;
    BIO *bio = BIO_new_ssl_connect(ssl_ctx);
    if (NULL == bio) {
        return NULL;
    }

    BIO_get_ssl(bio, &ssl);
    SSL_set_mode(ssl, SSL_MODE_AUTO_RETRY);
    BIO_set_conn_hostname(bio, addr);
    BIO_set_nbio(bio, 1);
    BIO_set_conn_port(bio, port);
    return bio;
}

static void bio_destroy(BIO *bio)
{
    if (NULL != bio) {
        BIO_free_all(bio);
    }
}

static void bio_init(struct reconnect_state *state)
{
    if (0 == strcmp(state->connection, "tcp://")) {
        state->sock_fd = bio_tcp_create(state->hostname, state->port);
    }

    if (0 == strcmp(state->connection, "ssl://")) {
        state->sock_fd =
            bio_ssl_create(state->ssl_ctx, state->hostname, state->port);
    }
}

static void bio_uninit(struct reconnect_state *state)
{
    bio_destroy(state->sock_fd);
}

static int bio_connect(struct reconnect_state *state)
{
    assert(NULL != state && NULL != state->sock_fd);

    int start_time    = time(NULL);
    int do_connect_rv = BIO_do_connect(state->sock_fd);
    while (do_connect_rv <= 0 && BIO_should_retry(state->sock_fd) &&
           (int) time(NULL) - start_time < 10) {
        do_connect_rv = BIO_do_connect(state->sock_fd);
    }

    if (do_connect_rv <= 0) {
        return -1;
    }

    return 0;
}

static void bio_update_connection(struct reconnect_state *state)
{
    bio_uninit(state);
    bio_init(state);
    bio_connect(state);
}

static void reconnect_state_uninit(struct reconnect_state *state)
{
    assert(NULL != state);

    bio_uninit(state);
    ssl_ctx_uninit(state);

    if (NULL != state->hostname) {
        free(state->hostname);
    }

    if (NULL != state->port) {
        free(state->port);
    }

    if (NULL != state->client_id) {
        free(state->client_id);
    }

    if (NULL != state->connection) {
        free(state->connection);
    }

    if (NULL != state->username) {
        free(state->username);
    }

    if (NULL != state->password) {
        free(state->password);
    }

    if (NULL != state->ca_file) {
        free(state->ca_file);
    }

    if (NULL != state->ca_path) {
        free(state->ca_path);
    }

    if (NULL != state->cert_file) {
        free(state->cert_file);
    }

    if (NULL != state->key_file) {
        free(state->key_file);
    }

    pthread_mutex_destroy(&state->sock_state_mutex);
    pthread_mutex_destroy(&state->running_mutex);
    pthread_mutex_destroy(&state->working_mutex);

    if (NULL != state->will_topic) {
        free(state->will_topic);
    }

    if (NULL != state->will_payload) {
        free(state->will_payload);
    }

    if (NULL != state->keypass) {
        free(state->keypass);
    }
}

static void subscribe_on_reconnect(mqtt_c_client_t *client)
{
    assert(NULL != client);

    for (struct subscribe_tuple **p =
             (struct subscribe_tuple **) utarray_front(client->array);
         NULL != p;
         p = (struct subscribe_tuple **) utarray_next(client->array, p)) {
        neu_err_code_e error = client_subscribe_send(client, (*p));
        if (NEU_ERR_SUCCESS != error) {
            if (NULL != client->log) {
                zlog_error(client->log, "send subscribe %s error:%d",
                           (*p)->topic, error);
            }
        }

        if (NULL != client->log) {
            zlog_error(client->log, "send subscribe %s success:%d", (*p)->topic,
                       error);
        }
    }
}

enum MQTTErrors inspector_callback(struct mqtt_client *mqtt)
{
    struct reconnect_state *state = mqtt->reconnect_state;
    if (0 == strcmp(state->connection, "ssl://")) {
        SSL *ssl = NULL;
        BIO_get_ssl(state->sock_fd, &ssl);
        if (X509_V_OK != SSL_get_verify_result(ssl)) {
            mqtt->error = MQTT_ERROR_SOCKET_ERROR;
        }
    }

    return mqtt->error;
}

static void client_reconnect(struct mqtt_client *mqtt, void **p_reconnect_state)
{
    assert(NULL != mqtt);
    assert(NULL != p_reconnect_state);

    struct reconnect_state *state =
        *((struct reconnect_state **) p_reconnect_state);
    mqtt_c_client_t *client  = state->client;
    mqtt->inspector_callback = inspector_callback;

    bio_update_connection(state);

    mqtt_reinit(mqtt, state->sock_fd, state->send_buf, state->send_buf_sz,
                state->recv_buf, state->recv_buf_sz);

    const char *client_id     = state->client_id;
    uint8_t     connect_flags = MQTT_CONNECT_RESERVED;
    if (1 == state->clean) {
        connect_flags = MQTT_CONNECT_CLEAN_SESSION;
    }

    const char *will_topic   = state->will_topic;
    const char *will_payload = state->will_payload;
    size_t      payload_len  = state->will_payload_len;

    enum MQTTErrors rc = mqtt_connect(
        mqtt, client_id, will_topic, will_payload, payload_len, state->username,
        state->password, connect_flags, state->keepalive);

    if (MQTT_OK != rc) {
        return;
    }

    subscribe_on_reconnect(client);
}

static void publish_callback(void **                       p_reconnect_state,
                             struct mqtt_response_publish *published)
{
    assert(NULL != p_reconnect_state);
    assert(NULL != published);

    struct reconnect_state *state =
        *((struct reconnect_state **) p_reconnect_state);
    mqtt_c_client_t *client = state->client;

    pthread_mutex_lock(&state->working_mutex);
    if (!state->working) {
        pthread_mutex_unlock(&state->working_mutex);
        return;
    }
    pthread_mutex_unlock(&state->working_mutex);

    char *topic_name = (char *) malloc(published->topic_name_size + 1);
    if (NULL == topic_name) {
        if (NULL != client->log) {
            zlog_error(client->log, "topic name alloc error");
        }
        return;
    }

    memcpy(topic_name, published->topic_name, published->topic_name_size);
    topic_name[published->topic_name_size] = '\0';

    nlog_debug("received publish('%s'): %s", topic_name,
               (const char *) published->application_message);

    for (struct subscribe_tuple **p =
             (struct subscribe_tuple **) utarray_front(client->array);
         NULL != p;
         p = (struct subscribe_tuple **) utarray_next(client->array, p)) {
        if (0 == strcmp((*p)->topic, topic_name) && NULL != (*p)->handle) {
            (*p)->handle(topic_name, strlen(topic_name),
                         (void *) published->application_message,
                         published->application_message_size,
                         client->user_data);
        }
    }

    free(topic_name);
}

neu_err_code_e mqtt_c_client_is_connected(mqtt_c_client_t *client);

static void *client_refresher(void *context)
{
    assert(NULL != context);
    int error_count = 0;

    mqtt_c_client_t *       client = (mqtt_c_client_t *) context;
    struct mqtt_client *    mqtt   = &client->mqtt;
    struct reconnect_state *state  = mqtt->reconnect_state;

    usleep(500); // Waiting for caller initialization to complete

    while (1) {
        pthread_mutex_lock(&state->running_mutex);
        if (!state->running) {
            pthread_mutex_unlock(&state->running_mutex);
            break;
        }
        pthread_mutex_unlock(&state->running_mutex);

        enum MQTTErrors error = mqtt_sync((struct mqtt_client *) mqtt);
        pthread_mutex_lock(&state->sock_state_mutex);
        state->sock_state = error;
        pthread_mutex_unlock(&state->sock_state_mutex);
        usleep(INTERVAL);

        if (NULL != client->state_update_func) {
            int rc = mqtt_c_client_is_connected(client);
            client->state_update_func(client->user_data, rc);

            if (0 != rc && error_count % 100 == 0) {
                zlog_error(client->log, "connect to %s:%s erorr, retry...",
                           client->state.hostname, client->state.port);
            }
        }
        error_count++;
    }

    reconnect_state_uninit(state);
    client_destroy(client);
    return NULL;
}

static mqtt_c_client_t *client_create(const neu_mqtt_option_t *option,
                                      void *                   context)
{
    mqtt_c_client_t *client = calloc(1, sizeof(mqtt_c_client_t));
    if (NULL == client) {
        zlog_error(option->log, "mqtt c client alloc error");
        return NULL;
    }

    client->option            = option;
    client->user_data         = context;
    client->state_update_func = client->option->state_update_func;
    client->log               = client->option->log;
    UT_icd tuple_icd = { sizeof(struct subscribe_tuple *), NULL, NULL, NULL };
    utarray_new(client->array, &tuple_icd);

    struct reconnect_state *state = &client->state;
    state->hostname               = strdup(client->option->host);
    state->port                   = strdup(client->option->port);
    state->client_id              = strdup(client->option->clientid);
    state->connection             = strdup(client->option->connection);
    state->username               = strdup(client->option->username);
    state->password               = strdup(client->option->password);
    state->keepalive              = client->option->keepalive_interval;
    state->clean                  = client->option->clean_session;
    state->ca_file                = strdup(client->option->ca);
    state->cert_file              = strdup(client->option->cert);
    state->key_file               = strdup(client->option->key);
    state->client                 = client;
    state->send_buf_sz            = sizeof(state->send_buf);
    state->recv_buf_sz            = sizeof(state->recv_buf);
    state->cert                   = NULL;
    state->key                    = NULL;
    state->ssl_ctx                = NULL;
    state->sock_fd                = NULL;
    state->running                = true;
    state->working                = true;
    pthread_mutex_init(&state->sock_state_mutex, NULL);
    pthread_mutex_init(&state->running_mutex, NULL);
    pthread_mutex_init(&state->working_mutex, NULL);

    if (NULL != client->option->will_topic) {
        state->will_topic = strdup(option->will_topic);
        if (NULL != client->option->will_payload) {
            state->will_payload     = strdup(client->option->will_payload);
            state->will_payload_len = strlen(state->will_payload);
        }
    }

    if (NULL != client->option->keypass &&
        0 < strlen(client->option->keypass)) {
        state->keypass = strdup(client->option->keypass);
    } else {
        state->keypass = NULL;
    }

    client->mqtt.publish_response_callback_state = state;
    return client;
}

static neu_err_code_e client_init(mqtt_c_client_t *client)
{
    const neu_mqtt_option_t *option = client->option;
    if (0 == strcmp(option->connection, "ssl://")) {
        ssl_ctx_init(&client->state);
        if (NULL == client->state.ssl_ctx) {
            return NEU_ERR_MQTT_FAILURE;
        }
    }

    bio_init(&client->state);
    if (NULL == client->state.sock_fd) {
        return NEU_ERR_MQTT_FAILURE;
    }

    return NEU_ERR_SUCCESS;
}

static neu_err_code_e client_connect(mqtt_c_client_t *client)
{
    mqtt_init_reconnect(&client->mqtt, client_reconnect, &client->state,
                        publish_callback);

    if (0 != pthread_create(&client->daemon, NULL, client_refresher, client)) {
        if (NULL != client->log) {
            zlog_error(client->log, "pthread create error");
        }

        return NEU_ERR_MQTT_CONNECT_FAILURE;
    }

    return NEU_ERR_SUCCESS;
}

neu_err_code_e mqtt_c_client_open(mqtt_c_client_t **       p_client,
                                  const neu_mqtt_option_t *option,
                                  void *                   context)
{
    assert(NULL != p_client);
    assert(NULL != option);

    mqtt_c_client_t *client = client_create(option, context);
    if (NULL == client) {
        *p_client = NULL;
        return NEU_ERR_MQTT_IS_NULL;
    }

    neu_err_code_e rc = client_init(client);
    if (NEU_ERR_SUCCESS != rc) {
        client_destroy(client);
        *p_client = NULL;
        return rc;
    }

    rc = client_connect(client);
    if (NEU_ERR_SUCCESS != rc) {
        client_destroy(client);
        *p_client = NULL;
        return rc;
    }

    *p_client = client;
    return rc;
}

neu_err_code_e mqtt_c_client_is_connected(mqtt_c_client_t *client)
{
    assert(NULL != client);

    struct mqtt_client *    mqtt  = &client->mqtt;
    struct reconnect_state *state = mqtt->reconnect_state;
    pthread_mutex_lock(&state->sock_state_mutex);
    if (MQTT_OK == state->sock_state) {
        if (MQTT_OK != mqtt->error) {
            pthread_mutex_unlock(&state->sock_state_mutex);
            return NEU_ERR_MQTT_CONNECT_FAILURE;
        }
    } else {
        pthread_mutex_unlock(&state->sock_state_mutex);
        return NEU_ERR_MQTT_CONNECT_FAILURE;
    }
    pthread_mutex_unlock(&state->sock_state_mutex);

    return NEU_ERR_SUCCESS;
}

static neu_err_code_e client_subscribe_destroy(struct subscribe_tuple *tuple)
{
    assert(NULL != tuple);

    if (NULL != tuple->topic) {
        free(tuple->topic);
    }

    free(tuple);
    return NEU_ERR_SUCCESS;
}

static neu_err_code_e client_disconnect(mqtt_c_client_t *client)
{
    neu_err_code_e error = mqtt_c_client_is_connected(client);
    if (NEU_ERR_SUCCESS == error) {
        mqtt_disconnect(&client->mqtt);
    }

    return NEU_ERR_SUCCESS;
}

static neu_err_code_e client_destroy(mqtt_c_client_t *client)
{
    for (struct subscribe_tuple **p =
             (struct subscribe_tuple **) utarray_front(client->array);
         NULL != p;
         p = (struct subscribe_tuple **) utarray_next(client->array, p)) {
        client_subscribe_destroy(*p);
    }

    utarray_free(client->array);
    free(client);
    return NEU_ERR_SUCCESS;
}

static struct subscribe_tuple *
client_subscribe_create(mqtt_c_client_t *client, const char *topic,
                        const int qos, const neu_subscribe_handle handle)
{
    struct subscribe_tuple *tuple = calloc(1, sizeof(struct subscribe_tuple));
    if (NULL == tuple) {
        if (NULL != client->log) {
            nlog_error("tuple alloc error");
        }

        return NULL;
    }

    tuple->topic  = strdup(topic);
    tuple->qos    = qos;
    tuple->handle = handle;
    tuple->client = client;
    return tuple;
}

static neu_err_code_e client_subscribe_send(mqtt_c_client_t *       client,
                                            struct subscribe_tuple *tuple)
{
    int rc = mqtt_subscribe(&client->mqtt, tuple->topic, tuple->qos);
    if (1 != rc) {
        return NEU_ERR_MQTT_SUBSCRIBE_FAILURE;
    }

    return NEU_ERR_SUCCESS;
}

static neu_err_code_e client_subscribe_add(mqtt_c_client_t *       client,
                                           struct subscribe_tuple *tuple)
{
    utarray_push_back(client->array, &tuple);
    return NEU_ERR_SUCCESS;
}

neu_err_code_e mqtt_c_client_subscribe(mqtt_c_client_t *client,
                                       const char *topic, const int qos,
                                       neu_subscribe_handle handle)
{
    assert(NULL != client);
    assert(NULL != topic && 0 < strlen(topic));

    for (struct subscribe_tuple **p =
             (struct subscribe_tuple **) utarray_front(client->array);
         NULL != p;
         p = (struct subscribe_tuple **) utarray_next(client->array, p)) {
        if (0 == strcmp((*p)->topic, topic)) {
            return NEU_ERR_SUCCESS;
        }
    }

    struct subscribe_tuple *tuple =
        client_subscribe_create(client, topic, qos, handle);
    if (NULL == tuple) {
        return NEU_ERR_MQTT_SUBSCRIBE_FAILURE;
    }

    client_subscribe_add(client, tuple);

    neu_err_code_e error = mqtt_c_client_is_connected(client);
    if (0 != error) {
        return error;
    }

    error = client_subscribe_send(client, tuple);
    if (NEU_ERR_SUCCESS != error) {
        return NEU_ERR_MQTT_SUBSCRIBE_FAILURE;
    }

    return NEU_ERR_SUCCESS;
}

static struct subscribe_tuple *
client_unsubscribe_create(mqtt_c_client_t *client, const char *topic)
{
    for (struct subscribe_tuple **p =
             (struct subscribe_tuple **) utarray_front(client->array);
         NULL != p;
         p = (struct subscribe_tuple **) utarray_next(client->array, p)) {
        if (0 == strcmp((*p)->topic, topic)) {
            return (*p);
        }
    }

    return NULL;
}

static neu_err_code_e client_unsubscribe_send(mqtt_c_client_t *       client,
                                              struct subscribe_tuple *tuple)
{
    neu_err_code_e error = mqtt_c_client_is_connected(client);
    if (0 != error) {
        return error;
    }

    int rc = mqtt_unsubscribe(&client->mqtt, tuple->topic);
    if (1 != rc) {
        return NEU_ERR_MQTT_UNSUBSCRIBE_FAILURE;
    }

    return NEU_ERR_SUCCESS;
}

neu_err_code_e mqtt_c_client_unsubscribe(mqtt_c_client_t *client,
                                         const char *     topic)
{
    assert(NULL != client);
    assert(NULL != topic && 0 < strlen(topic));

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
    assert(NULL != client);
    assert(NULL != topic && 0 < strlen(topic));

    // if (SEND_BUF_SIZE < len) {
    //     return NEU_ERR_MQTT_PUBLISH_OVER_LENGTH;
    // }

    neu_err_code_e error = mqtt_c_client_is_connected(client);
    if (0 != error) {
        return error;
    }

    pthread_mutex_lock(&client->state.working_mutex);
    if (!client->state.working) {
        pthread_mutex_unlock(&client->state.working_mutex);
        return NEU_ERR_MQTT_SUSPENDED;
    }
    pthread_mutex_unlock(&client->state.working_mutex);

    enum MQTTErrors err = mqtt_publish(&client->mqtt, topic, payload, len, qos);
    if (1 != err) {
        return NEU_ERR_MQTT_PUBLISH_FAILURE;
    }

    return NEU_ERR_SUCCESS;
}

neu_err_code_e mqtt_c_client_suspend(mqtt_c_client_t *client)
{
    assert(NULL != client);

    pthread_mutex_lock(&client->state.working_mutex);
    client->state.working = false;
    pthread_mutex_unlock(&client->state.working_mutex);
    return NEU_ERR_SUCCESS;
}

neu_err_code_e mqtt_c_client_continue(mqtt_c_client_t *client)
{
    assert(NULL != client);

    pthread_mutex_lock(&client->state.working_mutex);
    client->state.working = true;
    pthread_mutex_unlock(&client->state.working_mutex);
    return NEU_ERR_SUCCESS;
}

neu_err_code_e mqtt_c_client_close(mqtt_c_client_t *client)
{
    assert(NULL != client);

    mqtt_c_client_suspend(client);
    client_disconnect(client);

    client->state_update_func = NULL;
    client->user_data         = NULL;
    client->log               = NULL;
    pthread_mutex_lock(&client->state.running_mutex);
    client->state.running = false;
    pthread_mutex_unlock(&client->state.running_mutex);
    // pthread_join(client->daemon, NULL); // sync exit
    pthread_detach(client->daemon); // async exit
    return NEU_ERR_SUCCESS;
}
