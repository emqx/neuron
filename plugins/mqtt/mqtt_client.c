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
#include <string.h>

#include "mqtt_client.h"
#include <config.h>
#include <neuron.h>

#define CONFIG_FILE "./neuron.yaml"
#define CONFIG_NODE "mqtt"

void ssl_ctx_uninit(SSL_CTX *ssl_ctx)
{
    if (NULL != ssl_ctx) {
        SSL_CTX_free(ssl_ctx);
    }
}

SSL_CTX *ssl_ctx_init(const char *ca_file, const char *ca_path)
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

int mqtt_option_init(option_t *option)
{
    char *clientid = neu_config_get_value(CONFIG_FILE, 2, CONFIG_NODE, "id");
    char *mqtt_version =
        neu_config_get_value(CONFIG_FILE, 2, CONFIG_NODE, "mqtt_version");
    char *topic = neu_config_get_value(CONFIG_FILE, 2, CONFIG_NODE, "topic");
    char *qos   = neu_config_get_value(CONFIG_FILE, 2, CONFIG_NODE, "qos");
    char *keepalive_interval =
        neu_config_get_value(CONFIG_FILE, 2, CONFIG_NODE, "keepalive_interval");
    char *clean_session =
        neu_config_get_value(CONFIG_FILE, 2, CONFIG_NODE, "clean_session");
    char *connection = neu_config_get_value(CONFIG_FILE, 3, CONFIG_NODE,
                                            "broker", "connection");
    char *host =
        neu_config_get_value(CONFIG_FILE, 3, CONFIG_NODE, "broker", "host");
    char *port =
        neu_config_get_value(CONFIG_FILE, 3, CONFIG_NODE, "broker", "port");
    char *username =
        neu_config_get_value(CONFIG_FILE, 3, CONFIG_NODE, "broker", "username");
    char *password =
        neu_config_get_value(CONFIG_FILE, 3, CONFIG_NODE, "broker", "password");
    char *ca_path =
        neu_config_get_value(CONFIG_FILE, 3, CONFIG_NODE, "broker", "ca_path");
    char *ca_file =
        neu_config_get_value(CONFIG_FILE, 3, CONFIG_NODE, "broker", "ca_file");

    // MQTT option
    if (NULL == clientid) {
        option->clientid = NULL; // Use random id
    } else {
        option->clientid = strdup(clientid);
        free(clientid);
    }

    if (NULL == mqtt_version) {
        option->MQTT_version = 4; // Version 3.1.1
    } else {
        option->MQTT_version = atoi(mqtt_version);
        free(mqtt_version);
    }

    if (NULL == topic) {
        return -1;
    } else {
        option->topic = strdup(topic);
        free(topic);
    }

    if (NULL == qos) {
        option->qos = 0;
    } else {
        option->qos = atoi(qos);
        free(qos);
    }

    if (NULL == connection) {
        option->connection = strdup("tcp://");
    } else {
        option->connection = strdup(connection);
        free(connection);
    }

    if (NULL == host) {
        return -2;
    } else {
        option->host = strdup(host);
        free(host);
    }

    if (NULL == port) {
        return -3;
    } else {
        option->port = strdup(port);
        free(port);
    }

    if (NULL == username) {
        option->username = NULL;
    } else {
        option->username = strdup(username);
    }

    if (NULL == password) {
        option->password = NULL;
    } else {
        option->password = strdup(password);
    }

    if (NULL == ca_path) {
        option->capath = NULL;
    } else {
        option->capath = strdup(ca_path);
    }

    if (NULL == ca_file) {
        option->cafile = NULL;
    } else {
        option->cafile = strdup(ca_file);
    }

    if (0 == strcmp(option->connection, "ssl://") && NULL == option->cafile) {
        return -2;
    }

    if (0 == strcmp(option->connection, "ssl://")) {
        SSL_CTX *ssl_ctx = NULL;
        ssl_ctx          = ssl_ctx_init(option->cafile, option->capath);
        if (NULL == ssl_ctx) {
            return -3;
        }

        ssl_ctx_uninit(ssl_ctx);
    }

    if (NULL == keepalive_interval) {
        option->keepalive_interval = 20;
    } else {
        option->keepalive_interval = atoi(keepalive_interval);
        free(keepalive_interval);
    }

    if (NULL == clean_session) {
        option->clean_session = 1;
    } else {
        option->clean_session = atoi(clean_session);
        free(clean_session);
    }

    return 0;
}

void mqtt_option_uninit(option_t *option)
{
    if (NULL != option->clientid) {
        free(option->clientid);
    }

    if (NULL != option->topic) {
        free(option->topic);
    }

    if (NULL != option->connection) {
        free(option->connection);
    }

    if (NULL != option->host) {
        free(option->host);
    }

    if (NULL != option->port) {
        free(option->port);
    }

    if (NULL != option->username) {
        free(option->username);
    }

    if (NULL != option->password) {
        free(option->password);
    }

    if (NULL != option->capath) {
        free(option->capath);
    }

    if (NULL != option->cafile) {
        free(option->cafile);
    }
}
