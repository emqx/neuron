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

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "../mqtt.h"
#include "heartbeat.h"
#include "utils/log.h"
#include "version.h"

char *command_heartbeat_generate(neu_plugin_t *plugin, UT_array *states)
{
    char *                 version  = NEURON_VERSION;
    neu_json_states_head_t header   = { .version   = version,
                                      .timpstamp = plugin->common.timestamp };
    neu_json_states_t      json     = { 0 };
    char *                 json_str = NULL;
    json.n_state                    = utarray_len(states);
    if (0 < json.n_state) {
        json.states = calloc(json.n_state, sizeof(neu_json_node_state_t));
    }

    for (int i = 0; i < json.n_state; i++) {
        struct node_state **nsp = utarray_eltptr(states, (unsigned int) i);
        struct node_state * ns  = (*nsp);
        json.states[i].node     = ns->node;
        json.states[i].link     = ns->link;
        json.states[i].running  = ns->running;
    }

    neu_json_encode_with_mqtt(&json, neu_json_encode_states_resp, &header,
                              neu_json_encode_state_header_resp, &json_str);
    if (0 < json.n_state) {
        free(json.states);
    }

    return json_str;
}