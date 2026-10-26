/**
 * NEURON IIoT System for Industry 4.0
 * Copyright (C) 2020-2026 EMQ Technologies Co., Ltd All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 **/

#ifndef NEURON_PLUGIN_THINGSNEURAL_HANDLE_H
#define NEURON_PLUGIN_THINGSNEURAL_HANDLE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "neuron.h"

int tn_subscribe_downlink(neu_plugin_t *plugin);
int tn_handle_trans_data(neu_plugin_t *plugin, neu_reqresp_trans_data_t *data);
int tn_handle_set_payload(neu_plugin_t *plugin, const char *payload);
int tn_handle_subscribe_group(neu_plugin_t *plugin, neu_req_subscribe_t *sub);
int tn_handle_update_subscribe(neu_plugin_t *plugin, neu_req_subscribe_t *sub);
int tn_handle_unsubscribe_group(neu_plugin_t *         plugin,
                                neu_req_unsubscribe_t *unsub);
int tn_handle_update_group(neu_plugin_t *plugin, neu_req_update_group_t *req);
int tn_handle_del_group(neu_plugin_t *plugin, neu_req_del_group_t *req);
int tn_handle_update_driver(neu_plugin_t *plugin, neu_req_update_node_t *req);
int tn_handle_del_driver(neu_plugin_t *plugin, neu_reqresp_node_deleted_t *req);

#ifdef __cplusplus
}
#endif

#endif
