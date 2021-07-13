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

#ifndef NEURON_PLUGIN_H
#define NEURON_PLUGIN_H

#include "neu_adapter.h"

#define NEURON_PLUGIN_VER_1_0	100
#define NEURON_PLUGIN_VER_2_0	200

typedef enum neu_plugin_state {
	NEURON_PLUGIN_STATE_NULL,
	NEURON_PLUGIN_STATE_IDLE,
	NEURON_PLUGIN_STATE_READY,
} neu_plugin_state_e;

typedef struct neu_plugin_common {
	neu_plugin_state_e state;
	neu_adapter_t*	   adapter;
	const adapter_callbacks_t* adapter_callbacks;
} neu_plugin_common_t;

typedef struct neu_plugin neu_plugin_t;

typedef struct neu_plugin_intf_funs {
	neu_plugin_t* (*open)(neu_adapter_t* adapter,
						  const adapter_callbacks_t* callbacks);
	int (*close)(neu_plugin_t* plugin);
	int (*init)(neu_plugin_t* plugin);
	int (*uninit)(neu_plugin_t* plugin);
	int (*config)(neu_plugin_t* plugin, neu_config_t* configs);
	int (*request)(neu_plugin_t* plugin,
				   neu_request_t* req);
	int (*event_reply)(neu_plugin_t* plugin,
					   neu_event_reply_t* reply);
} neu_plugin_intf_funs_t;

typedef struct neu_plugin_module {
	const uint32_t version;
	const char*    module_name;
	const char*    module_descr;
	const neu_plugin_intf_funs_t* intf_funs;
} neu_plugin_module_t;

#endif
