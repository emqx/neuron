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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dlfcn.h>

#include <nng/nng.h>

#include "neu_log.h"
#include "neu_panic.h"
#include "core/neu_manager.h"
#include "neu_adapter.h"
#include "adapter_internal.h"
#include "neu_plugin.h"

typedef enum adapter_state {
	ADAPTER_STATE_NULL,
	ADAPTER_STATE_IDLE,
	ADAPTER_STATE_STARTING,
	ADAPTER_STATE_RUNNING,
	ADAPTER_STATE_STOPPING,
} adapter_state_e;

struct neu_adapter {
	uint32_t	    id;
	adapter_type_e	type;
	adapter_state_e state;
	char*			name;
	nng_pipe		pipe;
	char*			plugin_lib_name;
	void*           plugin_lib;		// handle of dynamic lib
	neu_plugin_module_t* plugin_module;
	neu_plugin_t*	plugin;
	adapter_callbacks_t  cb_funs;
};

static void* load_plugin_library(char* plugin_lib_name,
	   							 neu_plugin_module_t** plugin_module)
{
	void* lib_handle;

	lib_handle = dlopen(plugin_lib_name, RTLD_NOW);
	if (lib_handle == NULL) {
		return NULL;
	}

	void* module_info;
	module_info = dlsym(lib_handle, "neu_plugin_module");
	if (module_info == NULL) {
		dlclose(lib_handle);
		log_error("Failed to get neu_plugin_module from %s", plugin_lib_name);
		return NULL;
	}

	*plugin_module = (neu_plugin_module_t*) module_info;
	return lib_handle;
}

static int unload_plugin_library(void* lib_handle)
{
	int rv = 0;

	dlclose(lib_handle);
	return rv;
}

neu_adapter_t* neu_adapter_create(neu_adapter_info_t* info)
{
	neu_adapter_t* adapter;

	adapter = malloc(sizeof(neu_adapter_t));
	if (adapter == NULL) {
		log_error("Out of memeory for create adapter");
		return NULL;
	}

	adapter->id = info->id;
	adapter->type = info->type;
	adapter->name = strdup(info->name);
	adapter->plugin_lib_name = strdup(info->plugin_lib_name);
	adapter->state = ADAPTER_STATE_IDLE;

	if (  adapter->name == NULL
	   || adapter->plugin_lib_name == NULL) {
		if (adapter->name != NULL) {
			free(adapter->name);
		}
		if (adapter->plugin_lib_name != NULL) {
			free(adapter->plugin_lib_name);
		}
		free(adapter);
		log_error("Failed duplicate string for create adapter");
		return NULL;
	}

	void* handle;
	neu_plugin_module_t* plugin_module;
	handle = load_plugin_library(adapter->plugin_lib_name,
								 &plugin_module);
	if (handle == NULL) {
		neu_panic("Can't to load library(%s) for plugin(%s)",
				  adapter->plugin_lib_name, adapter->name);
	}

	adapter->plugin_lib = handle;
	adapter->plugin_module = plugin_module;
	neu_plugin_t* plugin;
	plugin_module = adapter->plugin_module;
	plugin = plugin_module->intf_funs->open(adapter);
	if (plugin == NULL) {
		neu_panic("Can't to open plugin(%s)", plugin_module->module_name);
	}

	adapter->plugin = plugin;
	return adapter;
}

void neu_adapter_destroy(neu_adapter_t* adapter)
{
	if (adapter == NULL) {
		neu_panic("Destroied adapter is NULL");
	}

	adapter->plugin_module->intf_funs->close(adapter->plugin);
	unload_plugin_library(adapter->plugin_lib);
	if (adapter->name != NULL) {
		free(adapter->name);
	}
	if (adapter->plugin_lib_name != NULL) {
		free(adapter->plugin_lib_name);
	}
	free(adapter);
	return;
}

int neu_adapter_start(neu_adapter_t* adapter, neu_manager_t* manager)
{
	int rv = 0;

	const neu_plugin_intf_funs_t* intf_funs;
	intf_funs = adapter->plugin_module->intf_funs;
	intf_funs->init(adapter->plugin);

	return rv;
}

int neu_adapter_stop(neu_adapter_t* adapter, neu_manager_t* manager)
{
	int rv = 0;

	const neu_plugin_intf_funs_t* intf_funs;
	intf_funs = adapter->plugin_module->intf_funs;
	intf_funs->uninit(adapter->plugin);

	return rv;
}

