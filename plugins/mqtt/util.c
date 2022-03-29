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
    }

    if (NULL != option->topic) {
        free(option->topic);
    }

    if (NULL != option->respons_topic) {
        free(option->respons_topic);
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

    if (NULL != option->ca_path) {
        free(option->ca_path);
    }

    if (NULL != option->ca_file) {
        free(option->ca_file);
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
    neu_json_elem_t ssl      = { .name = "ssl", .t = NEU_JSON_BOOL };
    neu_json_elem_t host     = { .name = "host", .t = NEU_JSON_STR };
    neu_json_elem_t port     = { .name = "port", .t = NEU_JSON_INT };
    neu_json_elem_t username = { .name = "username", .t = NEU_JSON_STR };
    neu_json_elem_t password = { .name = "password", .t = NEU_JSON_STR };
    neu_json_elem_t ca_path  = { .name = "ca-path", .t = NEU_JSON_STR };
    neu_json_elem_t ca_file  = { .name = "ca-file", .t = NEU_JSON_STR };

    ret = neu_parse_param(config->buf, &error, 8, &id, &ssl, &host, &port,
                          &username, &password, &ca_path, &ca_file);
    if (0 != ret) {
        return ret;
    }

    // MQTT option
    option->clientid     = NULL; // Use random id
    option->MQTT_version = 4;    // Version 3.1.1

    if (false == ssl.v.val_bool) {
        option->connection = strdup("tcp://");
    } else {
        option->connection = strdup("ssl://");
    }

    option->host = host.v.val_str;
    int32_t p    = (int) port.v.val_int;
    option->port = calloc(10, sizeof(char));
    snprintf(option->port, 10, "%d", p);
    option->clientid = id.v.val_str;
    option->username = username.v.val_str;
    option->password = password.v.val_str;
    option->ca_path  = ca_path.v.val_str;
    option->ca_file  = ca_file.v.val_str;

    if (0 == strcmp(option->connection, "ssl://") && NULL == option->ca_file) {
        return -1;
    }

    if (0 == strcmp(option->connection, "ssl://")) { }

    option->keepalive_interval = 20;
    option->clean_session      = 1;
    return 0;
}
