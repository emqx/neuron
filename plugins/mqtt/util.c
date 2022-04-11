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

#define MQTT_CERT_PATH "config"
#define MQTT_CA_FILE "mqtt_ca.crt"
#define MQTT_CERT_FILE "mqtt_cert.crt"
#define MQTT_KEY_FILE "mqtt_key.key"

void mqtt_option_uninit(neu_mqtt_option_t *option)
{
    if (NULL != option->clientid) {
        free(option->clientid);
        option->clientid = NULL;
    }

    if (NULL != option->topic) {
        free(option->topic);
        option->topic = NULL;
    }

    if (NULL != option->respons_topic) {
        free(option->respons_topic);
        option->respons_topic = NULL;
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

    if (NULL != option->ca_path) {
        free(option->ca_path);
        option->ca_path = NULL;
    }

    if (NULL != option->ca_file) {
        free(option->ca_file);
        option->ca_file = NULL;
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

static int b64_file_store(const char *in, const char *file_name)
{
    unsigned char *result  = NULL;
    int            out_len = 0;
    result                 = neu_decode64(&out_len, in);
    if (NULL == result) {
        return -1;
    }

    FILE *fp = fopen(file_name, "w+");
    if (NULL == fp) {
        free(result);
        return -1;
    }

    size_t write_len = fwrite(result, sizeof(unsigned char), out_len, fp);
    if (0 == write_len) {
        free(result);
        fclose(fp);
        return -1;
    }

    free(result);
    fclose(fp);
    return 0;
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
    neu_json_elem_t ca = { .name = "ca", .t = NEU_JSON_STR, .v.val_str = NULL };
    neu_json_elem_t cert = { .name      = "cert",
                             .t         = NEU_JSON_STR,
                             .v.val_str = NULL };
    neu_json_elem_t key  = { .name      = "key",
                            .t         = NEU_JSON_STR,
                            .v.val_str = NULL };

    ret = neu_parse_param(config->buf, &error, 9, &id, &ssl, &host, &port,
                          &username, &password, &ca, &cert, &key);
    if (0 != ret) {
        return ret;
    }

    char ca_path[PATH_MAX + 1]   = { 0 };
    char ca_file[PATH_MAX + 1]   = { 0 };
    char cert_file[PATH_MAX + 1] = { 0 };
    char key_file[PATH_MAX + 1]  = { 0 };

    // MQTT option
    option->clientid     = NULL; // Use random id
    option->MQTT_version = 4;    // Version 3.1.1

    if (false == ssl.v.val_bool) {
        option->connection = strdup("tcp://");
    } else {
        option->connection = strdup("ssl://");
    }

    if (0 == strcmp(option->connection, "ssl://")) {
        if (NULL == ca.v.val_str) {
            return -1;
        }

        if (0 == strlen(ca.v.val_str)) {
            free(ca.v.val_str);
            return -1;
        }

        char workdir[PATH_MAX + 1] = { 0 };
        getcwd(workdir, sizeof(workdir));
        sprintf(ca_file, "%s/%s/%s", workdir, MQTT_CERT_PATH, MQTT_CA_FILE);
        sprintf(ca_path, "%s/%s", workdir, MQTT_CERT_PATH);

        ret = b64_file_store(ca.v.val_str, ca_file);
        free(ca.v.val_str);
        ca.v.val_str = NULL;

        if (0 != ret) {
            return -1;
        }
    }

    if (0 == strcmp(option->connection, "ssl://")) {
        char workdir[PATH_MAX + 1] = { 0 };
        getcwd(workdir, sizeof(workdir));

        if (NULL != cert.v.val_str && 0 < strlen(cert.v.val_str)) {
            sprintf(cert_file, "%s/%s/%s", workdir, MQTT_CERT_PATH,
                    MQTT_CERT_FILE);

            ret = b64_file_store(cert.v.val_str, cert_file);
            free(cert.v.val_str);
            cert.v.val_str = NULL;

            if (0 != ret) {
                return -1;
            }
        }

        if (NULL != key.v.val_str && 0 < strlen(key.v.val_str)) {
            sprintf(key_file, "%s/%s/%s", workdir, MQTT_CERT_PATH,
                    MQTT_KEY_FILE);

            ret = b64_file_store(key.v.val_str, key_file);
            free(key.v.val_str);
            key.v.val_str = NULL;

            if (0 != ret) {
                return -1;
            }
        }
    }

    option->host = host.v.val_str;
    int32_t p    = (int) port.v.val_int;
    option->port = calloc(10, sizeof(char));
    snprintf(option->port, 10, "%d", p);
    option->clientid           = id.v.val_str;
    option->username           = username.v.val_str;
    option->password           = password.v.val_str;
    option->ca_path            = strdup(ca_path);
    option->ca_file            = strdup(ca_file);
    option->cert               = strdup(cert_file);
    option->key                = strdup(key_file);
    option->keepalive_interval = 30;
    option->clean_session      = 1;

    if (NULL != ca.v.val_str) {
        free(ca.v.val_str);
    }

    if (NULL != cert.v.val_str) {
        free(cert.v.val_str);
    }

    if (NULL != key.v.val_str) {
        free(key.v.val_str);
    }

    return 0;
}
