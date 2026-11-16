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

#ifndef NEURON_PLUGIN_MQTT_V2_HANDLE_H
#define NEURON_PLUGIN_MQTT_V2_HANDLE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "mqtt_v2_plugin.h"

#define MQTT_V2_WRITE_CTX_MAGIC 0x4d515432

// functions suffixed with `_locked` require `plugin->mtx`
int  mqtt_v2_set_topics_locked(neu_plugin_t *plugin);
void mqtt_v2_subscribe_all_locked(neu_plugin_t *plugin);
// retries the write topics failed to subscribe on the connected client
void mqtt_v2_subscribe_missing_locked(neu_plugin_t *plugin);
// the subscriptions went away with the previous client
void mqtt_v2_forget_subscriptions_locked(neu_plugin_t *plugin);
void mqtt_v2_publish_app_state_locked(neu_plugin_t *plugin, bool online);
// clears the retained states under the current topics, now if connected or
// else on the next connection, returns whether anything was published
bool mqtt_v2_retire_topics_locked(neu_plugin_t *plugin);
// to be called once connected
void mqtt_v2_connected_locked(neu_plugin_t *plugin);
void mqtt_v2_disconnected_locked(neu_plugin_t *plugin);
// frees the device table and the pending writes
void mqtt_v2_devices_free_locked(neu_plugin_t *plugin);

int mqtt_v2_request_nodes_state(neu_plugin_t *plugin);
int mqtt_v2_request_tags(neu_plugin_t *plugin);

int mqtt_v2_handle_trans_data(neu_plugin_t *            plugin,
                              neu_reqresp_trans_data_t *data);
int mqtt_v2_handle_subscribe(neu_plugin_t *plugin, neu_req_subscribe_t *req,
                             bool update);
int mqtt_v2_handle_unsubscribe(neu_plugin_t *plugin, const char *driver,
                               const char *group);
int mqtt_v2_handle_update_group(neu_plugin_t *          plugin,
                                neu_req_update_group_t *req);
int mqtt_v2_handle_update_driver(neu_plugin_t *         plugin,
                                 neu_req_update_node_t *req);
int mqtt_v2_handle_del_driver(neu_plugin_t *plugin, const char *driver);
int mqtt_v2_handle_nodes_state(neu_plugin_t *              plugin,
                               neu_resp_get_nodes_state_t *resp);
int mqtt_v2_handle_sub_tags(neu_plugin_t *                  plugin,
                            neu_resp_get_sub_driver_tags_t *resp);
// `tags` is the per tag result of NEU_RESP_WRITE_TAGS, NULL for NEU_RESP_ERROR
int mqtt_v2_handle_write_resp(neu_plugin_t *plugin, mqtt_v2_write_ctx_t *ctx,
                              int error, UT_array *tags);

#ifdef __cplusplus
}
#endif

#endif
