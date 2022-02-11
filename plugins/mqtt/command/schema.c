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

#include <errno.h>
#include <jansson.h>
#include <stdlib.h>

#include "schema.h"

static char *schema_inject(char *buf, neu_json_mqtt_t *mqtt)
{
    json_error_t error;
    json_t *     schema = json_loads(buf, 0, &error);

    if (NULL == schema) {
        log_debug("json error, column:%d, line:%d, pos:%d, %s, %s",
                  error.column, error.line, error.position, error.source,
                  error.text);
        return NULL;
    }

    json_t *root = json_object();
    json_object_set(root, "schema", schema);
    json_object_set(root, "uuid", json_string(mqtt->uuid));
    json_object_set(root, "command", json_string(mqtt->command));
    char *ret_str = json_dumps(root, 4096);
    return ret_str;
}

char *command_schema_get(neu_plugin_t *plugin, neu_json_mqtt_t *mqtt)
{
    UNUSED(plugin);
    char  buf[4096] = { 0 };
    FILE *fp        = fopen("./plugin_param_schema.json", "r");

    if (fp == NULL) {
        log_info("open ./plugin_param_schema.json error: %d", errno);

        char *                result = NULL;
        neu_json_error_resp_t error  = { 0 };
        error.error                  = NEU_ERR_FAILURE;
        neu_json_encode_with_mqtt(&error, neu_json_encode_error_resp, mqtt,
                                  neu_json_encode_mqtt_resp, &result);
        return result;
    }

    fread(buf, 1, sizeof(buf), fp);
    fclose(fp);

    char *ret_str = schema_inject(buf, mqtt);
    return ret_str;
}
