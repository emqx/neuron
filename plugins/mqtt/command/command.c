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
#include <stdio.h>
#include <stdlib.h>

#include "heartbeat.h"
#include "read_write.h"
#include "utils/log.h"

#include "../mqtt.h"
#include "command.h"

// Read tags with group config
static int read_request(mqtt_response_t *response, neu_plugin_t *plugin,
                        neu_json_mqtt_t *mqtt, char *json_str)
{
    UNUSED(response);

    neu_json_read_req_t *json = NULL;
    int                  rc   = neu_json_decode_read_req(json_str, &json);
    if (0 == rc) {
        command_rw_read_once_request(plugin, mqtt, json);
        neu_json_decode_read_req_free(json);
    }

    return rc;
}

// Write tags with group config
static int write_request(mqtt_response_t *response, neu_plugin_t *plugin,
                         neu_json_mqtt_t *mqtt, char *json_str)
{
    UNUSED(response);

    neu_json_write_req_t *req = NULL;
    int                   rc  = neu_json_decode_write_req(json_str, &req);
    if (0 == rc) {
        command_rw_write_request(plugin, mqtt, req);
        neu_json_decode_write_req_free(req);
    }

    return rc;
}

void command_response_handle(mqtt_response_t *response)
{
    neu_plugin_t *plugin = (neu_plugin_t *) response->plugin;

    if (NULL == response->topic_name || NULL == response->payload) {
        return;
    }

    char *json_str = malloc(response->len + 1);
    if (NULL == json_str) {
        return;
    }

    memset(json_str, 0x00, response->len + 1);
    memcpy(json_str, response->payload, response->len);

    neu_json_mqtt_t *mqtt = NULL;
    int              rc   = neu_json_decode_mqtt_req(json_str, &mqtt);

    if (0 != rc) {
        // char *                ret_str = NULL;
        // neu_json_error_resp_t error   = { .error = NEU_ERR_BODY_IS_WRONG };
        // neu_json_encode_with_mqtt(&error, neu_json_encode_error_resp, mqtt,
        //                           neu_json_encode_mqtt_resp, &ret_str);

        plog_error(plugin, "json parsing mqtt failed");

        free(json_str);
        return;
    }

    switch (response->type) {
    case TOPIC_TYPE_READ: {
        rc = read_request(response, plugin, mqtt, json_str);
        if (0 != rc) {
            neu_json_decode_mqtt_req_free(mqtt);
        }
        break;
    }

    case TOPIC_TYPE_WRITE: {
        rc = write_request(response, plugin, mqtt, json_str);
        if (0 != rc) {
            neu_json_decode_mqtt_req_free(mqtt);
        }
        break;
    }
    default:
        plog_error(plugin, "invalid topic type");
        break;
    }

    free(json_str);
}

char *command_read_once_response(neu_plugin_t *plugin, neu_reqresp_head_t *head,
                                 neu_resp_read_group_t *data, int format)
{
    return command_rw_read_once_response(plugin, head, data, format);
}

char *command_read_periodic_response(neu_plugin_t *            plugin,
                                     neu_reqresp_trans_data_t *data, int format)
{
    return command_rw_read_periodic_response(plugin, data, format);
}

char *command_write_response(neu_plugin_t *plugin, neu_reqresp_head_t *head,
                             neu_resp_error_t *data)
{
    return command_rw_write_response(plugin, head, data);
}

char *command_heartbeat_response(neu_plugin_t *plugin, UT_array *states)
{
    return command_heartbeat_generate(plugin, states);
}