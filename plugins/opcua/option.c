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

#include "option.h"
#include <config.h>
#include <neuron.h>

#define CONFIG_FILE "./neuron.yaml"
#define CONFIG_NODE "opcua"

int opcua_option_init(option_t *option)
{
    char *default_cert_file =
        neu_config_get_value(CONFIG_FILE, 2, CONFIG_NODE, "default_cert_file");
    char *default_key_file =
        neu_config_get_value(CONFIG_FILE, 2, CONFIG_NODE, "default_key_file");
    char *host = neu_config_get_value(CONFIG_FILE, 2, CONFIG_NODE, "host");
    char *port = neu_config_get_value(CONFIG_FILE, 2, CONFIG_NODE, "port");
    char *username =
        neu_config_get_value(CONFIG_FILE, 2, CONFIG_NODE, "username");
    char *password =
        neu_config_get_value(CONFIG_FILE, 2, CONFIG_NODE, "password");
    char *cert_file =
        neu_config_get_value(CONFIG_FILE, 2, CONFIG_NODE, "cert_file");
    char *key_file =
        neu_config_get_value(CONFIG_FILE, 2, CONFIG_NODE, "key_file");

    // OPC-UA option
    if (NULL == default_cert_file) {
        option->default_cert_file = NULL;
    } else {
        option->default_cert_file = strdup(default_cert_file);
        free(default_cert_file);
    }

    if (NULL == default_key_file) {
        option->default_key_file = NULL;
    } else {
        option->default_key_file = strdup(default_key_file);
        free(default_key_file);
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
        free(username);
    }

    if (NULL == password) {
        option->password = NULL;
    } else {
        option->password = strdup(password);
        free(password);
    }

    if (NULL == cert_file) {
        option->cert_file = NULL;
    } else {
        option->cert_file = strdup(cert_file);
        free(cert_file);
    }

    if (NULL == key_file) {
        option->key_file = NULL;
    } else {
        option->key_file = strdup(key_file);
        free(key_file);
    }

    return 0;
}

void opcua_option_uninit(option_t *option)
{
    if (NULL != option->default_cert_file) {
        free(option->default_cert_file);
    }

    if (NULL != option->default_key_file) {
        free(option->default_key_file);
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

    if (NULL != option->cert_file) {
        free(option->cert_file);
    }

    if (NULL != option->key_file) {
        free(option->key_file);
    }
}