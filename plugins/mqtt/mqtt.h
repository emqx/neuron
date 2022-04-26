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
#define TOPIC_READ_REQ "neuron/%s/read/req"
#define TOPIC_WRITE_REQ "neuron/%s/write/req"

#define TOPIC_STATUS_RES "neuron/%s/status"
#define TOPIC_READ_RES "neuron/%s/read/resp"
#define TOPIC_WRITE_RES "neuron/%s/write/resp"
#define TOPIC_UPLOAD_RES "neuron/%s/upload"

#define QOS0 0
#define QOS1 1
#define QOS2 2

#ifdef __cplusplus
}
#endif
#endif