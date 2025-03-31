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

#ifndef NEURON_PLUGIN_MQTT_HANDLE_H
#define NEURON_PLUGIN_MQTT_HANDLE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "datalayers_plugin.h"

void db_write_task_consumer(neu_plugin_t *plugin);
void task_queue_init(task_queue_t *queue);
void tasks_free(task_queue_t *queue);

int handle_trans_data(neu_plugin_t *            plugin,
                      neu_reqresp_trans_data_t *trans_data);

int handle_subscribe_group(neu_plugin_t *plugin, neu_req_subscribe_t *sub_info);
int handle_unsubscribe_group(neu_plugin_t *         plugin,
                             neu_req_unsubscribe_t *unsub_info);

int handle_update_group(neu_plugin_t *plugin, neu_req_update_group_t *req);
int handle_del_group(neu_plugin_t *plugin, neu_req_del_group_t *cmd);

int handle_update_driver(neu_plugin_t *plugin, neu_req_update_node_t *req);
int handle_del_driver(neu_plugin_t *plugin, neu_reqresp_node_deleted_t *req);
#ifdef __cplusplus
}
#endif

#endif
