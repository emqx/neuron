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

#include "open62541_option.h"
#include "schema/schema.h"
#include "json/json.h"
#include <config.h>

#define BUF_SIZE 40960
#define OPCUA_PLUGIN_NAME "opcua-plugin"
#define SCHEMA_FILE "plugin_param_schema.json"
#define CONFIG_FILE "./neuron.yaml"
#define CONFIG_NODE "opcua"

struct node_setting {
    char *host;
    int   port;
    char *username;
    char *password;
};

int opcua_option_init(option_t *option)
{
    if (NULL == option) {
        return -1;
    }

    memset(option, 0, sizeof(option_t));

    char *host = neu_config_get_value(CONFIG_FILE, 2, CONFIG_NODE, "host");
    char *port = neu_config_get_value(CONFIG_FILE, 2, CONFIG_NODE, "port");
    char *username =
        neu_config_get_value(CONFIG_FILE, 2, CONFIG_NODE, "username");
    char *password =
        neu_config_get_value(CONFIG_FILE, 2, CONFIG_NODE, "password");

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
        free(username);
    }

    if (NULL == password) {
        option->password = NULL;
    } else {
        option->password = strdup(password);
        free(password);
    }

    return 0;
}

void opcua_option_uninit(option_t *option)
{
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
}

static int decode_node_setting(const char *         json_str,
                               struct node_setting *setting)
{
    json_error_t error = { 0 };
    json_t *     root  = json_loads(json_str, 0, &error);

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

    // setting->host
    json_t *param = json_object_get(child, "host");
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
        setting->port = 4840;
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
    if (json_is_boolean(param)) {
        setting->username = strdup(json_string_value(param));
        json_decref(param);
    }

    // setting->password
    param = json_object_get(child, "password");
    if (NULL == param) {
        setting->host = NULL;
    }
    if (json_is_string(param)) {
        setting->password = strdup(json_string_value(param));
        json_decref(param);
    }

    json_decref(child);
    json_decref(root);
    return 0;
}

int valid_node_setting(const char *file, const char *plugin_name,
                       struct node_setting *setting)
{
    char  buf[BUF_SIZE] = { 0 };
    FILE *fp            = fopen(file, "r");
    fread(buf, 1, sizeof(buf), fp);
    fclose(fp);

    neu_schema_valid_t *valid = neu_schema_load(buf, (char *) plugin_name);
    if (NULL == valid) {
        return -1;
    }

    int rc =
        neu_schema_valid_param_string(valid, setting->host, (char *) "host");
    if (0 != rc) {
        neu_schema_free(valid);
        return -2;
    }

    rc = neu_schema_valid_param_int(valid, setting->port, (char *) "port");
    if (0 != rc) {
        neu_schema_free(valid);
        return -3;
    }

    rc = neu_schema_valid_param_string(valid, setting->username,
                                       (char *) "username");
    if (0 != rc) {
        neu_schema_free(valid);
        return -4;
    }

    rc = neu_schema_valid_param_string(valid, setting->password,
                                       (char *) "password");
    if (0 != rc) {
        neu_schema_free(valid);
        return -5;
    }

    neu_schema_free(valid);
    return 0;
}

int opcua_option_init_by_config(neu_config_t *config, option_t *option)
{
    if (NULL == config || NULL == option) {
        return -1;
    }

    memset(option, 0, sizeof(option_t));

    struct node_setting setting = { 0 };
    int rc = decode_node_setting((char *) config->buf, &setting);
    if (0 != rc) {
        return -2;
    }

    // OPCUA option
    option->host = setting.host;
    option->port = calloc(10, sizeof(char));
    if (NULL != option->port) {
        snprintf(option->port, 10, "%d", setting.port);
    }

    option->username = setting.username;
    option->password = setting.password;
    return 0;
}