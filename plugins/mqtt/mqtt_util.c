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

#include "mqtt_util.h"
#include "schema.h"
#include "json/json.h"
#include <config.h>
#include <neuron.h>

#define MQTT_PLUGIN_NAME "mqtt-plugin"
#define SCHEMA_FILE "plugin_param_schema.json"

struct node_setting {
    uint32_t node_id;
    char *   req_topic;
    char *   res_topic;
    char *   client_id;
    bool     ssl;
    char *   host;
    int      port;
    char *   username;
    char *   password;
    char *   ca_path;
    char *   ca_file;
};

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

static int decode_node_setting(const char *         json_str,
                               struct node_setting *setting)
{
    json_error_t error;
    json_t *     root = json_loads(json_str, 0, &error);

    if (NULL == root) {
        log_debug("json error, column:%d, line:%d, pos:%d, %s, %s",
                  error.column, error.line, error.position, error.source,
                  error.text);
        return -1;
    }

    json_t *child = json_object_get(root, "params");
    if (NULL == child) {
        json_decref(root);
        return -2;
    }

    // setting->req_topic
    json_t *param = json_object_get(child, "req-topic");
    if (NULL == param) {
        setting->req_topic = NULL;
    }
    if (json_is_string(param)) {
        setting->req_topic = strdup(json_string_value(param));
        json_decref(param);
    }

    // setting->res_topic
    param = json_object_get(child, "res-topic");
    if (NULL == param) {
        setting->res_topic = NULL;
    }
    if (json_is_string(param)) {
        setting->res_topic = strdup(json_string_value(param));
        json_decref(param);
    }

    // setting->res_topic
    param = json_object_get(child, "client-id");
    if (NULL == param) {
        setting->client_id = NULL;
    }
    if (json_is_string(param)) {
        setting->client_id = strdup(json_string_value(param));
        json_decref(param);
    }

    // setting->ssl
    param = json_object_get(child, "ssl");
    if (NULL == param) {
        setting->ssl = false;
    }
    if (json_is_boolean(param)) {
        setting->ssl = json_boolean_value(param);
        json_decref(param);
    }

    // setting->host
    param = json_object_get(child, "host");
    if (NULL == param) {
        setting->host = NULL;
    }
    if (json_is_string(param)) {
        setting->host = strdup(json_string_value(param));
        json_decref(param);
    }

    // setting->port
    param = json_object_get(child, "port");
    if (NULL == param) {
        setting->port = 1883;
    }
    if (json_is_integer(param)) {
        setting->port = json_integer_value(param);
        json_decref(param);
    }

    // setting->username
    param = json_object_get(child, "username");
    if (NULL == param) {
        setting->username = NULL;
    }
    if (json_is_string(param)) {
        setting->username = strdup(json_string_value(param));
        json_decref(param);
    }

    // setting->password
    param = json_object_get(child, "password");
    if (NULL == param) {
        setting->password = NULL;
    }
    if (json_is_string(param)) {
        setting->password = strdup(json_string_value(param));
        json_decref(param);
    }

    // setting->ca_path
    param = json_object_get(child, "ca-path");
    if (NULL == param) {
        setting->ca_path = NULL;
    }
    if (json_is_string(param)) {
        setting->ca_path = strdup(json_string_value(param));
        json_decref(param);
    }

    param = json_object_get(child, "ca-file");
    if (NULL == param) {
        setting->ca_file = NULL;
    }
    if (json_is_string(param)) {
        setting->ca_file = strdup(json_string_value(param));
        json_decref(param);
    }

    json_decref(child);
    json_decref(root);
    return 0;
}

static int valid_node_setting(const char *file, const char *plugin_name,
                              struct node_setting *setting)
{
    char  buf[40960] = { 0 };
    FILE *fp         = fopen(file, "r");
    fread(buf, 1, sizeof(buf), fp);
    fclose(fp);

    neu_schema_valid_t *valid = neu_schema_load(buf, (char *) plugin_name);
    if (NULL == valid) {
        return -1;
    }

    int rc = neu_schema_valid_param_string(valid, setting->req_topic,
                                           (char *) "req-topic");
    if (0 != rc) {
        neu_schema_free(valid);
        return -2;
    }

    rc = neu_schema_valid_param_string(valid, setting->res_topic,
                                       (char *) "res-topic");
    if (0 != rc) {
        neu_schema_free(valid);
        return -3;
    }

    rc = neu_schema_valid_param_string(valid, setting->client_id,
                                       (char *) "client-id");
    if (0 != rc) {
        neu_schema_free(valid);
        return -10;
    }

    rc = neu_schema_valid_param_string(valid, setting->host, (char *) "host");
    if (0 != rc) {
        neu_schema_free(valid);
        return -4;
    }

    rc = neu_schema_valid_param_int(valid, setting->port, (char *) "port");
    if (0 != rc) {
        neu_schema_free(valid);
        return -5;
    }

    rc = neu_schema_valid_param_string(valid, setting->username,
                                       (char *) "username");
    if (0 != rc) {
        neu_schema_free(valid);
        return -6;
    }

    rc = neu_schema_valid_param_string(valid, setting->password,
                                       (char *) "password");
    if (0 != rc) {
        neu_schema_free(valid);
        return -7;
    }

    rc = neu_schema_valid_param_string(valid, setting->ca_file,
                                       (char *) "ca-file");
    if (0 != rc) {
        neu_schema_free(valid);
        return -8;
    }

    rc = neu_schema_valid_param_string(valid, setting->ca_path,
                                       (char *) "ca-path");
    if (0 != rc) {
        neu_schema_free(valid);
        return -9;
    }

    neu_schema_free(valid);
    return 0;
}

int mqtt_option_init(neu_config_t *config, neu_mqtt_option_t *option)
{
    if (NULL == config || NULL == option) {
        return -1;
    }

    struct node_setting setting = { 0 };
    int rc = decode_node_setting((char *) config->buf, &setting);
    if (0 != rc) {
        return -2;
    }

    rc = valid_node_setting(SCHEMA_FILE, MQTT_PLUGIN_NAME, &setting);

    // MQTT option
    option->clientid      = NULL; // Use random id
    option->MQTT_version  = 4;    // Version 3.1.1
    option->topic         = setting.req_topic;
    option->respons_topic = setting.res_topic;
    option->qos           = 0;

    if (false == setting.ssl) {
        option->connection = strdup("tcp://");
    } else {
        option->connection = strdup("ssl://");
    }

    option->host = setting.host;
    option->port = calloc(10, sizeof(char));
    if (NULL != option->port) {
        snprintf(option->port, 10, "%d", setting.port);
    }

    option->clientid = setting.client_id;
    option->username = setting.username;
    option->password = setting.password;
    option->ca_path  = setting.ca_path;
    option->ca_file  = setting.ca_file;

    if (0 == strcmp(option->connection, "ssl://") && NULL == option->ca_file) {
        return -6;
    }

    if (0 == strcmp(option->connection, "ssl://")) { }

    option->keepalive_interval = 20;
    option->clean_session      = 1;
    return 0;
}
