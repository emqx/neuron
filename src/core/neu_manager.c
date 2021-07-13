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

#include <nng/nng.h>
#include <nng/protocol/pair1/pair.h>
#include <nng/supplemental/util/platform.h>

#include "neu_log.h"
#include "neu_panic.h"
#include "neu_adapter.h"
#include "adapter/adapter_internal.h"
#include "neu_manager.h"

typedef struct adapter_bind_info {
	uint32_t adapter_id;
	nng_pipe adapter_pipe;
	int      adapter_sock;
	int      bind_count;
} adapter_bind_info_t;

// TODO: The array of bound adapters should not be fixed
#define MAX_BIND_ADAPTERS  100
static adapter_bind_info_t g_bind_adapters[MAX_BIND_ADAPTERS];

typedef struct manager_bind_info {
	nng_mtx*    mtx;
	nng_cv *    cv;
	nng_pipe    mng_pipe;
	nng_socket  mng_sock;
	int         bind_count;
	adapter_bind_info_t* bind_adapters;
} manager_bind_info_t;

struct neu_manager {
	const char*			listen_url;
	bool				stop;
	nng_thread*			thrd;
	uint32_t			new_adapter_id;
	manager_bind_info_t bind_info;
};

static const char* const manager_url = "inproc://neu_manager";

#define NEU_HAS_SAMPLE_ADAPTER	1
#ifdef NEU_HAS_SAMPLE_ADAPTER
	#if defined(__APPLE__)
	#define SAMPLE_PLUGIN_LIB_NAME	"libsample-plugin.dylib"
	#else
	#define SAMPLE_PLUGIN_LIB_NAME  "libsample-plugin.so"
	#endif

static neu_adapter_info_t sample_adapter_info = {
	.type			 = ADAPTER_TYPE_DRIVER,
	.name			 = "sample-adapter",
	.plugin_lib_name = SAMPLE_PLUGIN_LIB_NAME
};

neu_adapter_t* sample_adapter;
#endif

static int init_bind_info(manager_bind_info_t* mng_bind_info)
{
	int rv, rv1;

	if (mng_bind_info == NULL) {
		return (-1);
	}

	rv = nng_mtx_alloc(&mng_bind_info->mtx);
	rv1 = nng_cv_alloc(&mng_bind_info->cv, mng_bind_info->mtx);
	if (rv || rv1) {
		neu_panic("Failed to initialize mutex and cv in manager_bind_info");
	}

	mng_bind_info->bind_count = 0;
	mng_bind_info->bind_adapters = &g_bind_adapters[0];

	return 0;
}

static int uninit_bind_info(manager_bind_info_t* mng_bind_info)
{
	if (mng_bind_info->bind_count > 0) {
		log_warn("It has some bound adapter in manager");
	}

	nng_cv_free(mng_bind_info->cv);
	nng_mtx_free(mng_bind_info->mtx);
	mng_bind_info->bind_count = 0;
	return 0;
}

static int manager_add_config(neu_manager_t* manager)
{
	int rv = 0;

	return rv;
}

static int manager_add_adapter(neu_manager_t* manager, neu_adapter_t* adapter)
{
	int rv = 0;

	return rv;
}

static int manager_remove_adapter(neu_manager_t* manager, neu_adapter_t* adapter)
{
	int rv = 0;

	return rv;
}

static void manager_loop(void* arg)
{
	int rv;
	neu_manager_t* manager;

	manager = (neu_manager_t*) arg;
	manager_bind_info_t* manager_bind = &manager->bind_info;
	rv = nng_pair1_open_poly(&manager_bind->mng_sock);
	if (rv != 0) {
		neu_panic("Can't open the manager pipe");
	}

	rv = nng_listen(manager_bind->mng_sock, manager->listen_url, NULL, 0);
	if (rv != 0) {
		neu_panic("Neuron manager can't listen on %s", manager->listen_url);
	}

#ifdef NEU_HAS_SAMPLE_ADAPTER
	sample_adapter_info.id = manager->new_adapter_id;
	manager->new_adapter_id++;
	sample_adapter = neu_adapter_create(&sample_adapter_info);
	neu_adapter_start(sample_adapter, manager);
#endif

	while (1) {
		nng_msg* msg;

		rv = nng_recvmsg(manager_bind->mng_sock, &msg, 0);
		if (rv != 0) {
			if (manager->stop) {
				break;
			} else {
				log_warn("Manage pipe no message received");
				continue;
			}
		}

		message_t* neu_msg;
		neu_msg = nng_msg_body(msg);
		switch (msg_get_type(neu_msg)) {
			case MSG_EVENT_STATUS_STRING:
			{
				char* buf_ptr;
				buf_ptr = msg_get_buf_ptr(neu_msg);
				log_info("Recieve string: %s", buf_ptr);

				const char* adapter_str = "manager recv reply";
				nng_msg* out_msg;
				size_t msg_size;
				msg_size = msg_inplace_data_get_size(strlen(adapter_str) + 1);
				rv = nng_msg_alloc(&out_msg, msg_size);
				if (rv == 0) {
					void* msg_ptr;
					char* buf_ptr;
					msg_ptr = nng_msg_body(out_msg);
					msg_inplace_data_init(msg_ptr, MSG_CONFIG_INFO_STRING,
							msg_size);
					buf_ptr = msg_get_buf_ptr(msg_ptr);
					memcpy(buf_ptr, adapter_str, strlen(adapter_str));
					buf_ptr[strlen(adapter_str)] = 0;
					nng_sendmsg(manager_bind->mng_sock, out_msg, 0);
				}
				break;
			}

			default:
				log_warn("Receive a not handled message(type: %d)", msg_get_type(neu_msg));
				break;
		}

		nng_msg_free(msg);
	}

#ifdef NEU_HAS_SAMPLE_ADAPTER
	neu_adapter_stop(sample_adapter, manager);
	neu_adapter_destroy(sample_adapter);
#endif
	return;
}

neu_manager_t* neu_manager_create()
{
	neu_manager_t* manager;

	manager = malloc(sizeof(neu_manager_t));
	if (manager == NULL) {
		log_error("Out of memeory for create neuron manager");
		return NULL;
	}

	manager->stop		= false;
	manager->listen_url = manager_url;
	manager->new_adapter_id = 1;
	init_bind_info(&manager->bind_info);
	nng_thread_create(&manager->thrd, manager_loop, manager);
	return manager;
}

void neu_manager_destroy(neu_manager_t* manager)
{
	nng_thread_destroy(manager->thrd);
	uninit_bind_info(&manager->bind_info);
	free(manager);
	return;
}

int neu_manager_bind_adapter(neu_manager_t* manager, neu_adapter_t* adapter)
{
	int rv = 0;

	if (adapter == NULL) {
		log_warn("The adapter is NULL that want to bind");
		return (-1);
	}

	return rv;
}

int neu_manager_unbind_adapter(neu_manager_t* manager, neu_adapter_t* adapter)
{
	int rv = 0;

	if (adapter == NULL) {
		log_warn("The adapter is NULL that want to unbind");
		return (-1);
	}

	return rv;
}

const char* neu_manager_get_url(neu_manager_t* manager)
{
	if (manager == NULL) {
		return NULL;
	}

	return manager->listen_url;
}
