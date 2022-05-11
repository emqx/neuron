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

#include <jansson.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util.h"
#include <config.h>
#include <json/json.h>
#include <json/neu_json_param.h>
#include <neuron.h>

void mqtt_option_uninit(neu_mqtt_option_t *option)
{
    if (NULL != option->clientid) {
        free(option->clientid);
        option->clientid = NULL;
    }

    if (NULL != option->upload_topic) {
        free(option->upload_topic);
        option->upload_topic = NULL;
    }

    if (NULL != option->connection) {
        free(option->connection);
        option->connection = NULL;
    }

    if (NULL != option->host) {
        free(option->host);
        option->host = NULL;
    }

    if (NULL != option->port) {
        free(option->port);
        option->port = NULL;
    }

    if (NULL != option->username) {
        free(option->username);
        option->username = NULL;
    }

    if (NULL != option->password) {
        free(option->password);
        option->password = NULL;
    }

    if (NULL != option->ca) {
        free(option->ca);
        option->ca = NULL;
    }

    if (NULL != option->cert) {
        free(option->cert);
        option->cert = NULL;
    }

    if (NULL != option->key) {
        free(option->key);
        option->key = NULL;
    }
}

int mqtt_option_init(neu_config_t *config, neu_mqtt_option_t *option)
{
    if (NULL == config || NULL == option) {
        return -1;
    }

    int             ret      = 0;
    char *          error    = NULL;
    neu_json_elem_t id       = { .name = "client-id", .t = NEU_JSON_STR };
    neu_json_elem_t upload   = { .name = "upload-topic", .t = NEU_JSON_STR };
    neu_json_elem_t format   = { .name = "format", .t = NEU_JSON_INT };
    neu_json_elem_t ssl      = { .name = "ssl", .t = NEU_JSON_BOOL };
    neu_json_elem_t host     = { .name = "host", .t = NEU_JSON_STR };
    neu_json_elem_t port     = { .name = "port", .t = NEU_JSON_INT };
    neu_json_elem_t username = { .name = "username", .t = NEU_JSON_STR };
    neu_json_elem_t password = { .name = "password", .t = NEU_JSON_STR };
    neu_json_elem_t ca       = { .name = "ca", .t = NEU_JSON_STR };
    neu_json_elem_t cert     = { .name = "cert", .t = NEU_JSON_STR };
    neu_json_elem_t key      = { .name = "key", .t = NEU_JSON_STR };

    ret = neu_parse_param(config->buf, &error, 11, &id, &upload, &format, &ssl,
                          &host, &port, &username, &password, &ca, &cert, &key);
    if (0 != ret) {
        return ret;
    }

    if (false == ssl.v.val_bool) {
        option->connection = strdup("tcp://");
    } else {
        option->connection = strdup("ssl://");
    }

    if (0 == strcmp(option->connection, "ssl://")) {
        if (0 == strlen(ca.v.val_str)) {
            free(ca.v.val_str);
            return -1;
        }
    }

    // MQTT option
    option->MQTT_version = 4; // Version 3.1.1
    option->host         = host.v.val_str;
    int32_t p            = (int) port.v.val_int;
    option->port         = calloc(10, sizeof(char));
    snprintf(option->port, 10, "%d", p);
    option->clientid           = id.v.val_str;
    option->upload_topic       = upload.v.val_str;
    option->format             = format.v.val_int;
    option->username           = username.v.val_str;
    option->password           = password.v.val_str;
    option->ca                 = ca.v.val_str;
    option->cert               = cert.v.val_str;
    option->key                = key.v.val_str;
    option->keepalive_interval = 30;
    option->clean_session      = 1;
    return 0;
}
