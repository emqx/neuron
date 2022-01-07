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

#ifndef NEURON_PLUGIN_MQTTC_UTIL
#define NEURON_PLUGIN_MQTTC_UTIL

#ifdef __cplusplus
extern "C" {
#endif

#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/ssl.h>

#include <neuron.h>

typedef struct {
    /* debug app options */
    int   publisher; /* publisher app? */
    int   quiet;
    int   verbose;
    int   tracelevel;
    char *delimiter;
    int   maxdatalen;
    /* message options */
    char *message;
    char *filename;
    int   stdin_lines;
    int   stdlin_complete;
    int   null_message;
    /* MQTT options */
    int   MQTT_version; // 3-3_1, 4-3_3_1, 5-5
    char *topic;
    char *respons_topic;
    char *clientid;
    int   qos;
    int   retained;
    char *username;
    char *password;
    char *host;
    char *port;
    char *connection; // tcp://, ssl://, ws://, wss://
    int   keepalive;

    int keepalive_interval;
    int clean_session;
    /* will options */
    char *will_topic;
    char *will_payload;
    int   will_qos;
    int   will_retain;
    /* TLS options */
    int   insecure;
    char *capath;
    char *cert;
    char *cafile;
    char *key;
    char *keypass;
    char *ciphers;
    char *psk_identity;
    char *psk;
    /* MQTT V5 options */
    int message_expiry;
    struct {
        char *name;
        char *value;
    } user_property;
    /* websocket HTTP proxies */
    char *http_proxy;
    char *https_proxy;
} option_t;

typedef enum {
    MQTT_SUCCESS = 0,
    MQTT_NO_CERTFILESET,
    MQTT_CERTFILE_LOAD_FAILURE,
    MQTT_IS_NULL,
    MQTT_INIT_FAILURE,
    MQTT_CONNECT_FAILURE,
    MQTT_SUBSCRIBE_TIMEOUT,
    MQTT_SUBSCRIBE_LIST_INITIAL_FAILURE,
    MQTT_SUBSCRIBE_FAILURE,
    MQTT_SUBSCRIBE_ADD_REPEAT,
    MQTT_SUBSCRIBE_ADD_FAILURE,
    MQTT_UNSUBSCRIBE_FAILURE,
    MQTT_PUBLISH_FAILURE,
} client_error_e;

typedef void (*subscribe_handle)(const char *topic_name, size_t topic_len,
                                 void *payload, const size_t len,
                                 void *context);

int  mqtt_option_init(neu_config_t *config, option_t *option);
void mqtt_option_uninit(option_t *option);

#ifdef __cplusplus
}
#endif
#endif