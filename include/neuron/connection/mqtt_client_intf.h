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

#ifndef NEURON_NEU_MQTT_INTERFACE
#define NEURON_NEU_MQTT_INTERFACE

#ifdef __cplusplus
extern "C" {
#endif

#include "neuron.h"
#include <stddef.h>
#include <stdint.h>

typedef struct {
    /* debug app options */
    int verbose;
    /* MQTT options */
    int   MQTT_version; // 3-3_1, 4-3_3_1, 5-5
    char *clientid;
    int   qos;
    int   retained;
    char *username;
    char *password;
    char *host;
    char *port;
    char *connection; // tcp://, ssl://, ws://, wss://
    int   keepalive;
    int   keepalive_interval;
    int   clean_session;
    /* will options */
    char *will_topic;
    char *will_payload;
    int   will_qos;
    int   will_retain;
    /* TLS options */
    char *ca;
    char *cert;
    char *key;
    char *keypass;

    /*application*/
    int   format;
    char *upload_topic;
} neu_mqtt_option_t;

typedef void *neu_mqtt_client_t;

typedef void (*neu_subscribe_handle)(const char *topic_name, size_t topic_len,
                                     void *payload, const size_t len,
                                     void *context);

neu_err_code_e neu_mqtt_client_open(neu_mqtt_client_t *      p_client,
                                    const neu_mqtt_option_t *option,
                                    void *                   context);

neu_err_code_e neu_mqtt_client_is_connected(neu_mqtt_client_t client);

neu_err_code_e neu_mqtt_client_subscribe(neu_mqtt_client_t client,
                                         const char *topic, const int qos,
                                         neu_subscribe_handle handle);

neu_err_code_e neu_mqtt_client_unsubscribe(neu_mqtt_client_t client,
                                           const char *      topic);

neu_err_code_e neu_mqtt_client_publish(neu_mqtt_client_t client,
                                       const char *topic, int qos,
                                       unsigned char *payload, size_t len);

neu_err_code_e neu_mqtt_client_suspend(neu_mqtt_client_t client);

neu_err_code_e neu_mqtt_client_continue(neu_mqtt_client_t client);

neu_err_code_e neu_mqtt_client_close(neu_mqtt_client_t client);

#ifdef __cplusplus
}
#endif
#endif