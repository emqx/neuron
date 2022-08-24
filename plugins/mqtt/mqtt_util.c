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

#include <openssl/rand.h>

#include <json/json.h>
#include <json/neu_json_param.h>

#include "mqtt_plugin.h"
#include "mqtt_util.h"

int uuid_v4(char *buffer)
{
    union {
        struct {
            uint32_t time_low;
            uint16_t time_mid;
            uint16_t time_hi_and_version;
            uint8_t  clk_seq_hi_res;
            uint8_t  clk_seq_low;
            uint8_t  node[6];
        };
        uint8_t __rnd[16];
    } uuid;

    int rc              = RAND_bytes(uuid.__rnd, sizeof(uuid));
    uuid.clk_seq_hi_res = (uint8_t)((uuid.clk_seq_hi_res & 0x3F) | 0x80);
    uuid.time_hi_and_version =
        (uint16_t)((uuid.time_hi_and_version & 0x0FFF) | 0x4000);

    snprintf(buffer, 38, "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
             uuid.time_low, uuid.time_mid, uuid.time_hi_and_version,
             uuid.clk_seq_hi_res, uuid.clk_seq_low, uuid.node[0], uuid.node[1],
             uuid.node[2], uuid.node[3], uuid.node[4], uuid.node[5]);

    return rc;
}

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
    (void) plugin;
    if (NULL == config || NULL == option) {
        return 1;
    }

    int             ret       = 0;
    char *          error     = NULL;
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
    char uuid[38] = { 0 };
    int  rc       = uuid_v4(uuid);
    if (1 != rc) {
        return 2;
    }

    option->clientid = strdup(uuid);

    // upload-topic, required
    ret = neu_parse_param(config, &error, 1, &upload);
    if (0 != ret) {
        free(error);
        free(upload.v.val_str);
        return 3;
    } else {
        if (0 == strlen(upload.v.val_str)) {
            free(upload.v.val_str);
            return 3;
        }

        option->upload_topic = upload.v.val_str;
    }

    // heartbeat-topic, required
    ret = neu_parse_param(config, &error, 1, &heartbeat);
    if (0 != ret) {
        free(error);
        free(heartbeat.v.val_str);
        return 4;
    } else {
        if (0 == strlen(heartbeat.v.val_str)) {
            free(heartbeat.v.val_str);
            return 4;
        }

        option->heartbeat_topic = heartbeat.v.val_str;
    }

    // format, optional
    ret = neu_parse_param(config, &error, 1, &format);
    if (0 != ret) {
        free(error);
        option->format = 0;
    } else {
        option->format = format.v.val_int;
    }

    // ssl, required
    ret = neu_parse_param(config, &error, 1, &ssl);
    if (0 != ret) {
        free(error);
        return 5;
    } else {
        if (false == ssl.v.val_bool) {
            option->connection = strdup("tcp://");
        } else {
            option->connection = strdup("ssl://");

            // ca, required if ssl enabled
            ret = neu_parse_param(config, &error, 1, &ca);
            if (0 != ret) {
                free(error);
                free(ca.v.val_str);
                return 6;
            } else {
                option->ca = ca.v.val_str;
            }

            // cert, optional
            ret = neu_parse_param(config, &error, 1, &cert);
            if (0 != ret) {
                free(error);
                free(cert.v.val_str);
            } else {
                option->cert = cert.v.val_str;

                // key, required if cert enable
                ret = neu_parse_param(config, &error, 1, &key);
                if (0 != ret) {
                    free(error);
                    free(key.v.val_str);
                    return 7;
                } else {
                    option->key = key.v.val_str;
                }

                // keypass, optional
                ret = neu_parse_param(config, &error, 1, &keypass);
                if (0 != ret) {
                    free(error);
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
        free(error);
        free(host.v.val_str);
        return 8;
    } else {
        option->host = host.v.val_str;
    }

    // port, required
    ret = neu_parse_param(config, &error, 1, &port);
    if (0 != ret) {
        free(error);
        return 9;
    } else {
        int32_t p    = (int) port.v.val_int;
        option->port = calloc(10, sizeof(char));
        snprintf(option->port, 10, "%d", p);
        option->password = password.v.val_str;
    }

    // username, optional
    ret = neu_parse_param(config, &error, 1, &username);
    if (0 != ret) {
        free(error);
        free(username.v.val_str);
    } else {
        option->username = username.v.val_str;
    }

    // password, optional
    ret = neu_parse_param(config, &error, 1, &password);
    if (0 != ret) {
        free(error);
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

static const char *mqtt_option_error_strs[] = {
    [0]  = "success",
    [1]  = "config or option is null",
    [2]  = "generate uuid failed",
    [3]  = "upload topic field not set",
    [4]  = "heartbeat topic field not set",
    [5]  = "ssl field not set",
    [6]  = "ca field not set when login with tls",
    [7]  = "key field not set when login with tls with cert",
    [8]  = "host field not set",
    [9]  = "port field not set",
    [10] = "unknow",
};

const char *mqtt_option_error(int error)
{
    if (0 <= error && 9 >= error) {
        return mqtt_option_error_strs[error];
    } else {
        return mqtt_option_error_strs[10];
    }
}