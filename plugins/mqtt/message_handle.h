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

#ifndef NEURON_PLUGIN_MESSAGE_HANDLE
#define NEURON_PLUGIN_MESSAGE_HANDLE

#ifdef __cplusplus
extern "C" {
#endif

#include <neuron.h>

#include "option.h"
#include "paho_client.h"
#include "parser/neu_json_add_tag.h"
#include "parser/neu_json_get_tag.h"
#include "parser/neu_json_read.h"
#include "parser/neu_json_tag_group.h"
#include "parser/neu_json_write.h"

void message_handle_set_plugin_common(neu_plugin_common_t *common);
void message_handle_set_paho_client(paho_client_t *paho);
void message_handle_set_event_id(uint32_t *event_id);

void message_handle_init_tags();

void message_handle_read(struct neu_parse_read_req *req);
void message_handle_read_result(neu_variable_t *variable);
void message_handle_write_handle(struct neu_parse_write_req *write_req);
void message_handle_get_tag_list_handle(struct neu_parse_get_tags_req *req);
void message_handle_add_tag_handle(struct neu_parse_add_tags_req *req);
void message_handle_read_tag_group_list_handle(
    struct neu_paser_read_tag_group_list_req *req);

#ifdef __cplusplus
}
#endif
#endif