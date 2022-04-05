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

#ifndef NEURON_PLUGIN_MQTT
#define NEURON_PLUGIN_MQTT

#ifdef __cplusplus
extern "C" {
#endif

#include <neuron.h>

#define UNUSED(x) (void) (x)
#define INTERVAL 100000U
#define TIMEOUT 3000U

#define TOPIC_PING_REQ "neuron/%s/ping"
#define TOPIC_NODE_REQ "neuron/%s/node/req"
#define TOPIC_GCONFIG_REQ "neuron/%s/gconfig/req"
#define TOPIC_TAGS_REQ "neuron/%s/tags/req"
#define TOPIC_PLUGIN_REQ "neuron/%s/plugin/req"
#define TOPIC_SUBSCRIBE_REQ "neuron/%s/subscribe/req"
#define TOPIC_READ_REQ "neuron/%s/read/req"
#define TOPIC_WRITE_REQ "neuron/%s/write/req"
#define TOPIC_TTYS_REQ "neuron/%s/ttys/req"
#define TOPIC_SCHEMA_REQ "neuron/%s/schema/plugin/req"
#define TOPIC_SETTING_REQ "neuron/%s/node/setting/req"
#define TOPIC_CTR_REQ "neuron/%s/node/ctl/req"
#define TOPIC_STATE_REQ "neuron/%s/node/state/req"

#define TOPIC_STATUS_RES "neuron/%s/status"
#define TOPIC_NODE_RES "neuron/%s/node/resp"
#define TOPIC_GCONFIG_RES "neuron/%s/gconfig/resp"
#define TOPIC_TAGS_RES "neuron/%s/tags/resp"
#define TOPIC_PLUGIN_RES "neuron/%s/plugin/resp"
#define TOPIC_SUBSCRIBE_RES "neuron/%s/subscribe/resp"
#define TOPIC_READ_RES "neuron/%s/read/resp"
#define TOPIC_WRITE_RES "neuron/%s/write/resp"
#define TOPIC_TTYS_RES "neuron/%s/ttys/resp"
#define TOPIC_SCHEMA_RES "neuron/%s/schema/plugin/resp"
#define TOPIC_SETTING_RES "neuron/%s/node/setting/resp"
#define TOPIC_CTR_RES "neuron/%s/node/ctl/resp"
#define TOPIC_STATE_RES "neuron/%s/node/state/resp"
#define TOPIC_UPLOAD_RES "neuron/%s/upload"

#define QOS0 0
#define QOS1 1
#define QOS2 2

#ifdef __cplusplus
}
#endif
#endif