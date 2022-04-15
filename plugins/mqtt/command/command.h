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

#ifndef NEURON_PLUGIN_MQTT_COMMAND
#define NEURON_PLUGIN_MQTT_COMMAND

#ifdef __cplusplus
extern "C" {
#endif

#include <neuron.h>

#include "common.h"

enum topic_type {
    TOPIC_TYPE_PING = 0,
    TOPIC_TYPE_NODE,
    TOPIC_TYPE_GCONFIG,
    TOPIC_TYPE_TAGS,
    TOPIC_TYPE_PLUGIN,
    TOPIC_TYPE_SUBSCRIBE,
    TOPIC_TYPE_READ,
    TOPIC_TYPE_WRITE,
    TOPIC_TYPE_TTYS,
    TOPIC_TYPE_SCHEMA,
    TOPIC_TYPE_SETTING,
    TOPIC_TYPE_CTR,
    TOPIC_TYPE_STATE,
    TOPIC_TYPE_UPLOAD,
};

typedef void (*context_add_callback)(neu_plugin_t *plugin, uint32_t req_id,
                                     neu_json_mqtt_t *parse_head, char *result,
                                     void *pair, bool ready);

typedef struct {
    const char *         topic_name;
    size_t               topic_len;
    void *               payload;
    size_t               len;
    void *               plugin;
    void *               topic_pair;
    enum topic_type      type;
    context_add_callback context_add;
} mqtt_response_t;

void  command_response_handle(mqtt_response_t *response);
char *command_read_once_response(neu_plugin_t *plugin, uint32_t node_id,
                                 neu_json_mqtt_t *parse_header,
                                 neu_data_val_t * resp_val);
char *command_read_periodic_response(neu_plugin_t *plugin, uint64_t sender,
                                     const char *         node_name,
                                     neu_taggrp_config_t *config,
                                     neu_data_val_t *     resp_val,
                                     int                  upload_format);
char *command_write_response(neu_json_mqtt_t *parse_header,
                             neu_data_val_t * resp_val);

#ifdef __cplusplus
}
#endif
#endif