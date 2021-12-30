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

#include <neuron.h>
#include <nng/mqtt/mqtt_client.h>
#include <nng/nng.h>

#include "mqtt_nng_client.h"

#define UNUSED(x) (void) (x)
#define MAX_URL_LEN 256

struct mqtt_nng_client {
    const option_t *option;
    void *          user_data;
    neu_list        subscribe_list;
    char *          client_id;
    nng_socket      sock;
    char *          url;
    nng_mqtt_cb     callback;
};

struct subscribe_tuple {
    neu_list_node      node;
    char *             topic;
    int                qos;
    subscribe_handle   handle;
    bool               subscribed;
    mqtt_nng_client_t *client;
};

static void client_on_connected(void *arg, nng_msg *ack_msg)
{
    uint8_t status = nng_mqtt_msg_get_connack_return_code(ack_msg);
    log_info("connected status:%d", status);
    nng_msg_free(ack_msg);
}

static void client_on_disconnected(void *arg, nng_msg *msg)
{
    (void) arg;
    log_info("disconnect mqtt");
    nng_msg_free(msg);
}

static mqtt_nng_client_t *client_create(const option_t *option, void *context)
{
    mqtt_nng_client_t *client = malloc(sizeof(mqtt_nng_client_t));
    if (NULL == client) {
        return NULL;
    }

    memset(client, 0, sizeof(mqtt_nng_client_t));
    client->option    = option;
    client->user_data = context;
    client->client_id = client->option->clientid;

    char url[MAX_URL_LEN] = { '\0' };
    snprintf(url, MAX_URL_LEN, "mqtt-tcp://%s:%s", option->host, option->port);
    client->url = strdup(url);

    client->callback.name            = "neuron_client_cb";
    client->callback.on_connected    = client_on_connected;
    client->callback.on_disconnected = client_on_disconnected;
    client->callback.connect_arg     = client;
    client->callback.disconn_arg     = client;

    NEU_LIST_INIT(&client->subscribe_list, struct subscribe_tuple, node);
    return client;
}

static client_error_e client_init(mqtt_nng_client_t *client)
{
    if (0 != nng_mqtt_client_open(&client->sock)) {
        return MQTTC_INIT_FAILURE;
    }

    return MQTTC_SUCCESS;
}

static client_error_e client_connect(mqtt_nng_client_t *client)
{
    nng_msg *connect_msg;
    nng_mqtt_msg_alloc(&connect_msg, 0);
    nng_mqtt_msg_set_packet_type(connect_msg, NNG_MQTT_CONNECT);
    nng_mqtt_msg_set_connect_proto_version(connect_msg,
                                           client->option->MQTT_version);
    nng_mqtt_msg_set_connect_keep_alive(connect_msg, client->option->keepalive);

    if (NULL != client->option->username && NULL != client->option->password) {
        nng_mqtt_msg_set_connect_user_name(connect_msg,
                                           client->option->username);
        nng_mqtt_msg_set_connect_password(connect_msg,
                                          client->option->password);
    }

    if (NULL != client->option->will_topic) {
        nng_mqtt_msg_set_connect_will_msg(connect_msg,
                                          client->option->will_payload,
                                          strlen(client->option->will_payload));
        nng_mqtt_msg_set_connect_will_topic(connect_msg,
                                            client->option->will_topic);
    }

    nng_mqtt_msg_set_connect_client_id(connect_msg, client->client_id);
    nng_mqtt_msg_set_connect_clean_session(connect_msg, true);

    if (1 == client->option->verbose) {
        uint8_t buff[1024] = { 0 };
        nng_mqtt_msg_dump(connect_msg, buff, sizeof(buff), true);
        log_info("%s", buff);
    }

    nng_dialer dialer;
    if (0 != nng_dialer_create(&dialer, client->sock, client->url)) {
        return MQTTC_CONNECT_FAILURE;
    }

    nng_dialer_set_ptr(dialer, NNG_OPT_MQTT_CONNMSG, connect_msg);
    nng_dialer_set_cb(dialer, &client->callback);
    nng_dialer_start(dialer, NNG_FLAG_NONBLOCK);
    return MQTTC_SUCCESS;
}

client_error_e mqtt_nng_client_open(mqtt_nng_client_t **p_client,
                                    const option_t *option, void *context)
{
    mqtt_nng_client_t *client = client_create(option, context);
    if (NULL == client) {
        return MQTTC_IS_NULL;
    }

    client_error_e rc = client_init(client);
    rc                = client_connect(client);
    *p_client         = client;
    return rc;
}

client_error_e mqtt_nng_client_is_connected(mqtt_nng_client_t *client)
{
    return MQTTC_SUCCESS;
}

client_error_e mqtt_nng_client_subscribe(mqtt_nng_client_t *client,
                                         const char *topic, const int qos,
                                         subscribe_handle handle)
{
    return MQTTC_SUCCESS;
}

client_error_e mqtt_nng_client_unsubscribe(mqtt_nng_client_t *client,
                                           const char *       topic)
{
    return MQTTC_SUCCESS;
}

client_error_e mqtt_nng_client_publish(mqtt_nng_client_t *client,
                                       const char *topic, int qos,
                                       unsigned char *payload, size_t len)
{
    return MQTTC_SUCCESS;
}

static client_error_e mqtt_nng_client_disconnect(mqtt_nng_client_t *client)
{
    return MQTTC_SUCCESS;
}

static client_error_e mqtt_nng_client_destroy(mqtt_nng_client_t *client)
{
    if (NULL != client->url) {
        free(client->url);
    }

    free(client);
    return MQTTC_SUCCESS;
}

client_error_e mqtt_nng_client_close(mqtt_nng_client_t *client)
{
    if (NULL == client) {
        return MQTTC_IS_NULL;
    }

    client_error_e rc = mqtt_nng_client_disconnect(client);
    rc                = mqtt_nng_client_destroy(client);
    return rc;
}
