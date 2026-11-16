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

#ifndef NEURON_PLUGIN_MQTT_V2_WRITE_H
#define NEURON_PLUGIN_MQTT_V2_WRITE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

#include <jansson.h>

#include "type.h"

typedef struct {
    char *       tag;
    neu_dvalue_t value;
    int          error;
} mqtt_v2_write_value_t;

typedef struct {
    json_t *               id;      // echoed in the response, may be NULL
    char *                 version; // echoed in the response, may be NULL
    char *                 group;   // group of all the values, may be NULL
    size_t                 n_value;
    mqtt_v2_write_value_t *values;
} mqtt_v2_write_req_t;

/**
 * Decode a write request according to its `version`, a request without
 * `version` is in the neuron format.
 *
 * Returns 0 on success. On failure the returned error code is the reason, and
 * the fields decoded so far are kept for the response, with the per tag
 * reasons in `values[i].error`.
 */
int  mqtt_v2_write_req_decode(const uint8_t *payload, uint32_t len,
                              mqtt_v2_write_req_t *req);
void mqtt_v2_write_req_fini(mqtt_v2_write_req_t *req);

// sets `error` on every value that does not have a reason of its own
void mqtt_v2_write_values_fail(mqtt_v2_write_value_t *values, size_t n_value,
                               int error);

char *mqtt_v2_write_resp_encode(json_t *id, const char *version, int error,
                                const mqtt_v2_write_value_t *values,
                                size_t                       n_value);

#ifdef __cplusplus
}
#endif

#endif
