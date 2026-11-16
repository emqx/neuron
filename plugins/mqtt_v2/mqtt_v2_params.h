/**
 * NEURON IIoT System for Industry 4.0
 * Copyright (C) 2020-2026 EMQ Technologies Co., Ltd All rights reserved.
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

#ifndef NEURON_PLUGIN_MQTT_V2_PARAMS_H
#define NEURON_PLUGIN_MQTT_V2_PARAMS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <string.h>

#include <jansson.h>

#include "errcodes.h"

// shared with the manager, which validates subscription parameters
// synchronously before the subscription is persisted
#define MQTT_V2_MODULE_NAME "MQTT V2"

#define MQTT_V2_TOPIC_MAX_LEN 255

// `vars` is a NULL terminated list of the variable names allowed in `tmpl`
static inline bool mqtt_v2_topic_tmpl_valid(const char *       tmpl,
                                            const char *const *vars)
{
    size_t len = strlen(tmpl);

    if (0 == len || MQTT_V2_TOPIC_MAX_LEN < len) {
        return false;
    }

    // topics starting with `$` are reserved by brokers, e.g. `$SYS`
    if ('$' == tmpl[0] && '{' != tmpl[1]) {
        return false;
    }

    for (const char *p = tmpl; '\0' != *p; ++p) {
        if ('+' == *p || '#' == *p) {
            return false;
        }

        if ('$' != p[0] || '{' != p[1]) {
            continue;
        }

        const char *end = strchr(p + 2, '}');
        if (NULL == end) {
            return false;
        }

        size_t n     = end - (p + 2);
        bool   known = false;
        for (size_t i = 0; NULL != vars[i]; ++i) {
            if (strlen(vars[i]) == n && 0 == strncmp(p + 2, vars[i], n)) {
                known = true;
                break;
            }
        }
        if (!known) {
            return false;
        }
        p = end;
    }

    return true;
}

static inline bool mqtt_v2_sub_topic_valid(const char *topic)
{
    static const char *vars[] = { "app", "device", "group", NULL };
    return mqtt_v2_topic_tmpl_valid(topic, vars);
}

// `params` is the subscription parameter string `{"params": {...}}`,
// with the optional `topic` field, an empty one selects the default topic
static inline int mqtt_v2_params_check(const char *params)
{
    int     rv   = NEU_ERR_SUCCESS;
    json_t *root = NULL;

    if (NULL == params) {
        return NEU_ERR_SUCCESS;
    }

    root = json_loads(params, 0, NULL);
    if (!json_is_object(root)) {
        json_decref(root);
        return NEU_ERR_GROUP_PARAMETER_INVALID;
    }

    json_t *object = json_object_get(root, "params");
    if (NULL != object && !json_is_null(object)) {
        json_t *topic = json_object_get(object, "topic");

        if (!json_is_object(object)) {
            rv = NEU_ERR_GROUP_PARAMETER_INVALID;
        } else if (NULL != topic && !json_is_null(topic) &&
                   (!json_is_string(topic) ||
                    (0 < json_string_length(topic) &&
                     !mqtt_v2_sub_topic_valid(json_string_value(topic))))) {
            rv = NEU_ERR_GROUP_PARAMETER_INVALID;
        }
    }

    json_decref(root);
    return rv;
}

#ifdef __cplusplus
}
#endif

#endif
