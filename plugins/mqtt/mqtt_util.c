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

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <json/json.h>
#include <json/neu_json_param.h>

#include "mqtt_plugin.h"
#include "mqtt_util.h"

void mqtt_option_uninit(neu_plugin_t *plugin, neu_mqtt_option_t *option)
{
    (void) plugin;

    if (NULL != option->clientid) {
        free(option->clientid);
        option->clientid = NULL;
    }

    if (NULL != option->upload_topic) {
        free(option->upload_topic);
        option->upload_topic = NULL;
    }

    if (NULL != option->heartbeat_topic) {
        free(option->heartbeat_topic);
        option->heartbeat_topic = NULL;
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

    if (NULL != option->keypass) {
        free(option->keypass);
        option->keypass = NULL;
    }
}

int mqtt_option_init(neu_plugin_t *plugin, char *config,
                     neu_mqtt_option_t *option)
{
    if (NULL == config || NULL == option) {
        return -1;
    }

    int             ret       = 0;
    char *          error     = NULL;
    neu_json_elem_t id        = { .name = "client-id", .t = NEU_JSON_STR };
    neu_json_elem_t upload    = { .name = "upload-topic", .t = NEU_JSON_STR };
    neu_json_elem_t heartbeat = { .name = "heartbeat-topic",
                                  .t    = NEU_JSON_STR };
    neu_json_elem_t format    = { .name = "format", .t = NEU_JSON_INT };
    neu_json_elem_t ssl       = { .name = "ssl", .t = NEU_JSON_BOOL };
    neu_json_elem_t host      = { .name = "host", .t = NEU_JSON_STR };
    neu_json_elem_t port      = { .name = "port", .t = NEU_JSON_INT };
    neu_json_elem_t username  = { .name = "username", .t = NEU_JSON_STR };
    neu_json_elem_t password  = { .name = "password", .t = NEU_JSON_STR };
    neu_json_elem_t ca        = { .name = "ca", .t = NEU_JSON_STR };
    neu_json_elem_t cert      = { .name = "cert", .t = NEU_JSON_STR };
    neu_json_elem_t key       = { .name = "key", .t = NEU_JSON_STR };
    neu_json_elem_t keypass   = { .name = "keypass", .t = NEU_JSON_STR };

    // client-id, required
    ret = neu_parse_param(config, &error, 1, &id);
    if (0 != ret) {
        plog_error(plugin, "can't decode client-id from json");
        free(id.v.val_str);
        return -1;
    } else {
        option->clientid = id.v.val_str;
    }

    // upload-topic, optional
    ret = neu_parse_param(config, &error, 1, &upload);
    if (0 != ret) {
        free(upload.v.val_str);
    } else {
        option->upload_topic = upload.v.val_str;
    }

    // heartbeat-topic, optional
    ret = neu_parse_param(config, &error, 1, &heartbeat);
    if (0 != ret) {
        free(heartbeat.v.val_str);
    } else {
        option->heartbeat_topic = heartbeat.v.val_str;
    }

    // format, required
    ret = neu_parse_param(config, &error, 1, &format);
    if (0 != ret) {
        option->format = 0;
    } else {
        option->format = format.v.val_int;
    }

    // ssl, required
    ret = neu_parse_param(config, &error, 1, &ssl);
    if (0 != ret) {
        plog_error(plugin, "can't decode ssl from json");
        return -2;
    } else {
        if (false == ssl.v.val_bool) {
            option->connection = strdup("tcp://");
        } else {
            option->connection = strdup("ssl://");

            // ca, required if ssl enabled
            ret = neu_parse_param(config, &error, 1, &ca);
            if (0 != ret) {
                plog_error(plugin, "can't decode ca from json");
                free(ca.v.val_str);
                return -3;
            } else {
                option->ca = ca.v.val_str;
            }

            // cert, optional
            ret = neu_parse_param(config, &error, 1, &cert);
            if (0 == ret) {
                free(cert.v.val_str);
            } else {
                option->cert = cert.v.val_str;

                // key, required if cert enable
                ret = neu_parse_param(config, &error, 1, &key);
                if (0 != ret) {
                    plog_error(plugin, "can't decode key from json");
                    free(key.v.val_str);
                    return -4;
                } else {
                    option->key = key.v.val_str;
                }

                // keypass, optional
                ret = neu_parse_param(config, &error, 1, &keypass);
                if (0 != ret) {
                    free(keypass.v.val_str);
                } else {
                    option->keypass = keypass.v.val_str;
                }
            }
        }
    }

    // host, required
    ret = neu_parse_param(config, &error, 1, &host);
    if (0 != ret) {
        plog_error(plugin, "can't decode host from json");
        free(host.v.val_str);
        return -5;
    } else {
        option->host = host.v.val_str;
    }

    // port, required
    ret = neu_parse_param(config, &error, 1, &port);
    if (0 != ret) {
        plog_error(plugin, "can't decode port from json");
        return -6;
    } else {
        int32_t p    = (int) port.v.val_int;
        option->port = calloc(10, sizeof(char));
        snprintf(option->port, 10, "%d", p);
        option->password = password.v.val_str;
    }

    // username, optional
    ret = neu_parse_param(config, &error, 1, &username);
    if (0 != ret) {
        free(username.v.val_str);
    } else {
        option->username = username.v.val_str;
    }

    // password, optional
    ret = neu_parse_param(config, &error, 1, &password);
    if (0 != ret) {
        free(password.v.val_str);
    } else {
        option->password = password.v.val_str;
    }

    // MQTT option
    option->MQTT_version       = 4; // Version 3.1.1
    option->keepalive_interval = 30;
    option->clean_session      = 1;
    option->will_topic         = NULL;
    option->will_payload       = NULL;

    return 0;
}
