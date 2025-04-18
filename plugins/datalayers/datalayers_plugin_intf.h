/**
 * NEURON IIoT System for Industry 4.0
 * Copyright (C) 2020-2024 EMQ Technologies Co., Ltd All rights reserved.
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

#ifndef NEURON_PLUGIN_DATALAYERS_INTF_H
#define NEURON_PLUGIN_DATALAYERS_INTF_H

#ifdef __cplusplus
extern "C" {
#endif

#include "neuron.h"
#include "plugin.h"

#define NEU_METRIC_CACHED_QUEUE_SIZE "cached_queue_size"
#define NEU_METRIC_CACHED_QUEUE_SIZE_TYPE NEU_METRIC_TYPE_GAUAGE
#define NEU_METRIC_CACHED_QUEUE_SIZE_HELP "Size of the cached message queue"

#define NEU_METRIC_MAX_CACHED_QUEUE_SIZE "max_cached_queue_size"
#define NEU_METRIC_MAX_CACHED_QUEUE_SIZE_TYPE NEU_METRIC_TYPE_GAUAGE
#define NEU_METRIC_MAX_CACHED_QUEUE_SIZE_HELP \
    "Max size of the cached message queue"

#define NEU_METRIC_DISCARDED_MSGS "discarded_msgs"
#define NEU_METRIC_DISCARDED_MSGS_TYPE NEU_METRIC_TYPE_COUNTER
#define NEU_METRIC_DISCARDED_MSGS_HELP \
    "Number of messages discarded when cache is full"

#define NEU_PLUGIN_REGISTER_CACHED_QUEUE_SIZE_METRIC(plugin) \
    NEU_PLUGIN_REGISTER_METRIC(plugin, NEU_METRIC_CACHED_QUEUE_SIZE, 0)

#define NEU_PLUGIN_REGISTER_MAX_CACHED_QUEUE_SIZE_METRIC(plugin) \
    NEU_PLUGIN_REGISTER_METRIC(plugin, NEU_METRIC_MAX_CACHED_QUEUE_SIZE, 0)

#define NEU_PLUGIN_REGISTER_DISCARDED_MSGS_METRIC(plugin) \
    NEU_PLUGIN_REGISTER_METRIC(plugin, NEU_METRIC_DISCARDED_MSGS, 0)

#define NEU_PLUGIN_UPDATE_CACHED_QUEUE_SIZE_METRIC(plugin, val) \
    NEU_PLUGIN_UPDATE_METRIC(plugin, NEU_METRIC_CACHED_QUEUE_SIZE, val, NULL)

#define NEU_PLUGIN_UPDATE_MAX_CACHED_QUEUE_SIZE_METRIC(plugin, val)         \
    NEU_PLUGIN_UPDATE_METRIC(plugin, NEU_METRIC_MAX_CACHED_QUEUE_SIZE, val, \
                             NULL)

#define NEU_PLUGIN_UPDATE_DISCARDED_MSGS_METRIC(plugin, val) \
    NEU_PLUGIN_UPDATE_METRIC(plugin, NEU_METRIC_DISCARDED_MSGS, val, NULL)

extern const neu_plugin_intf_funs_t datalayers_plugin_intf_funs;

neu_plugin_t *datalayers_plugin_open(void);
int           datalayers_plugin_close(neu_plugin_t *plugin);
int           datalayers_plugin_init(neu_plugin_t *plugin, bool load);
int           datalayers_plugin_uninit(neu_plugin_t *plugin);
int datalayers_plugin_config(neu_plugin_t *plugin, const char *setting);
int datalayers_plugin_start(neu_plugin_t *plugin);
int datalayers_plugin_stop(neu_plugin_t *plugin);
int datalayers_plugin_request(neu_plugin_t *plugin, neu_reqresp_head_t *head,
                              void *data);

#ifdef __cplusplus
}
#endif

#endif