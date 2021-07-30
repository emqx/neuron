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

#include <dlfcn.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <nng/nng.h>
#include <nng/protocol/pair1/pair.h>
#include <nng/supplemental/util/platform.h>

#include "adapter_internal.h"
#include "core/neu_manager.h"
#include "neu_adapter.h"
#include "neu_log.h"
#include "neu_panic.h"
#include "neu_plugin.h"

typedef enum adapter_state {
    ADAPTER_STATE_NULL,
    ADAPTER_STATE_IDLE,
    ADAPTER_STATE_STARTING,
    ADAPTER_STATE_RUNNING,
    ADAPTER_STATE_STOPPING,
} adapter_state_e;

struct neu_adapter {
    adapter_id_t         id;
    adapter_type_e       type;
    nng_mtx *            mtx;
    adapter_state_e      state;
    bool                 stop;
    char *               name;
    neu_manager_t *      manager;
    nng_pipe             pipe;
    nng_socket           sock;
    nng_thread *         thrd;
    uint32_t             new_req_id;
    plugin_id_t          plugin_id;
    char *               plugin_lib_name;
    void *               plugin_lib; // handle of dynamic lib
    neu_plugin_module_t *plugin_module;
    neu_plugin_t *       plugin;
    adapter_callbacks_t  cb_funs;
};

static void *load_plugin_library(char *                plugin_lib_name,
                                 neu_plugin_module_t **plugin_module)
{
    void *lib_handle;

    lib_handle = dlopen(plugin_lib_name, RTLD_NOW);
    if (lib_handle == NULL) {
        log_error("Failed to open dynamic library %s: %s", plugin_lib_name,
                  strerror(errno));

        return NULL;
    }

    void *module_info;
    module_info = dlsym(lib_handle, "neu_plugin_module");
    if (module_info == NULL) {
        dlclose(lib_handle);
        log_error("Failed to get neu_plugin_module from %s", plugin_lib_name);
        return NULL;
    }

    *plugin_module = (neu_plugin_module_t *) module_info;
    return lib_handle;
}

static int unload_plugin_library(void *lib_handle)
{
    int rv = 0;

    dlclose(lib_handle);
    return rv;
}

static uint32_t adapter_get_req_id(neu_adapter_t *adapter)
{
    uint32_t req_id;

    req_id = adapter->new_req_id++;
    return req_id;
}

static void adapter_loop(void *arg)
{
    int            rv;
    neu_adapter_t *adapter;

    adapter = (neu_adapter_t *) arg;
    const char *manager_url;
    manager_url = neu_manager_get_url(adapter->manager);
    rv          = nng_dial(adapter->sock, manager_url, NULL, 0);
    if (rv != 0) {
        neu_panic("The adapter can't dial to %s", manager_url);
    }

    nng_mtx_lock(adapter->mtx);
    adapter->state = ADAPTER_STATE_IDLE;
    nng_mtx_unlock(adapter->mtx);

    const char *adapter_str = "adapter started";
    nng_msg *   out_msg;
    size_t      msg_size;
    msg_size = msg_inplace_data_get_size(strlen(adapter_str) + 1);
    rv       = nng_msg_alloc(&out_msg, msg_size);
    if (rv == 0) {
        message_t *msg_ptr;
        char *     buf_ptr;
        msg_ptr = (message_t *) nng_msg_body(out_msg);
        msg_inplace_data_init(msg_ptr, MSG_EVENT_STATUS_STRING, msg_size);
        buf_ptr = msg_get_buf_ptr(msg_ptr);
        memcpy(buf_ptr, adapter_str, strlen(adapter_str));
        buf_ptr[strlen(adapter_str)] = 0;
        nng_sendmsg(adapter->sock, out_msg, 0);
    }

    while (1) {
        nng_msg *msg;

        nng_mtx_lock(adapter->mtx);
        if (adapter->stop) {
            adapter->state = ADAPTER_STATE_NULL;
            nng_mtx_unlock(adapter->mtx);
            log_info("Exit loop of the adapter(%s)", adapter->name);
            break;
        }
        nng_mtx_unlock(adapter->mtx);

        rv = nng_recvmsg(adapter->sock, &msg, 0);
        if (rv != 0) {
            log_warn("Manage pipe no message received");
            continue;
        }

        message_t *pay_msg;
        pay_msg = nng_msg_body(msg);
        switch (msg_get_type(pay_msg)) {
        case MSG_CONFIG_INFO_STRING: {
            char *buf_ptr;
            buf_ptr = msg_get_buf_ptr(pay_msg);
            log_info("Received string: %s", buf_ptr);
            break;
        }

        case MSG_CMD_START_READ: {
            const neu_plugin_intf_funs_t *intf_funs;
            neu_request_t                 req;
            uint32_t                      req_code;
            if (adapter->plugin_module) {
                intf_funs    = adapter->plugin_module->intf_funs;
                req.req_id   = adapter_get_req_id(adapter);
                req.req_type = NEU_REQRESP_READ;
                req.buf_len  = sizeof(uint32_t);
                req_code     = 1;
                req.buf      = (char *) &req_code;
                intf_funs->request(adapter->plugin, &req);
            }
            break;
        }

        case MSG_CMD_STOP_READ: {
            break;
        }

        case MSG_CMD_EXIT_LOOP: {
            uint32_t exit_code;

            exit_code = *(uint32_t *) msg_get_buf_ptr(pay_msg);
            log_info("adapter(%s) exit loop by exit_code=%d", adapter->name,
                     exit_code);
            nng_mtx_lock(adapter->mtx);
            adapter->state = ADAPTER_STATE_NULL;
            adapter->stop  = true;
            nng_mtx_unlock(adapter->mtx);
            break;
        }

        default:
            log_warn("Receive a not supported message(type: %d)",
                     msg_get_type(pay_msg));
            break;
        }

        nng_msg_free(msg);
    }

    return;
}

static int adapter_response(neu_adapter_t *adapter, neu_response_t *resp)
{
    int rv = 0;

    if (adapter == NULL || resp == NULL) {
        log_warn("The adapter or response is NULL");
        return (-1);
    }

    log_info("Get response from plugin");
    switch (resp->resp_type) {
    case NEU_REQRESP_MOVE_BUF: {
        core_databuf_t *databuf;
        databuf = core_databuf_new_with_buf(resp->buf, resp->buf_len);
        // for debug
        log_debug("Get respose buf: %s", core_databuf_get_ptr(databuf));

        nng_msg *msg;
        size_t   msg_size;
        msg_size = msg_external_data_get_size();
        rv       = nng_msg_alloc(&msg, msg_size);
        if (rv == 0) {
            message_t *pay_msg;

            pay_msg = (message_t *) nng_msg_body(msg);
            msg_external_data_init(pay_msg, MSG_DATA_NEURON_DATABUF, databuf);
            nng_sendmsg(adapter->sock, msg, 0);
        }
        core_databuf_put(databuf);
        break;
    }

    default:
        break;
    }
    return rv;
}

static int adapter_event_notify(neu_adapter_t *     adapter,
                                neu_event_notify_t *event)
{
    int rv = 0;

    log_info("Get event notify from plugin");
    return rv;
}

static const adapter_callbacks_t callback_funs = { .response = adapter_response,
                                                   .event_notify =
                                                       adapter_event_notify };

neu_adapter_t *neu_adapter_create(neu_adapter_info_t *info)
{
    neu_adapter_t *adapter;

    adapter = malloc(sizeof(neu_adapter_t));
    if (adapter == NULL) {
        log_error("Out of memeory for create adapter");
        return NULL;
    }

    int rv;
    adapter->state = ADAPTER_STATE_NULL;
    adapter->stop  = false;
    if ((rv = nng_mtx_alloc(&adapter->mtx)) != 0) {
        log_error("Can't allocate mutex for adapter");
        free(adapter);
        return NULL;
    }

    adapter->id         = info->id;
    adapter->type       = info->type;
    adapter->name       = strdup(info->name);
    adapter->new_req_id = 0;
    adapter->plugin_id  = info->plugin_id;
    if (adapter->plugin_id.id_val == 0) {
        adapter->plugin_lib_name = NULL;
        adapter->plugin_lib      = NULL;
        adapter->plugin_module   = NULL;
        adapter->plugin          = NULL;
        log_info("Create a adapter without plugin");
        goto open_pipe;
    }

    adapter->plugin_lib_name = strdup(info->plugin_lib_name);
    if (adapter->name == NULL || adapter->plugin_lib_name == NULL) {
        if (adapter->name != NULL) {
            free(adapter->name);
        }
        if (adapter->plugin_lib_name != NULL) {
            free(adapter->plugin_lib_name);
        }
        nng_mtx_free(adapter->mtx);
        free(adapter);
        log_error("Failed duplicate string for create adapter");
        return NULL;
    }

    void *               handle;
    neu_plugin_module_t *plugin_module;
    handle = load_plugin_library(adapter->plugin_lib_name, &plugin_module);
    if (handle == NULL) {
        neu_panic("Can't to load library(%s) for plugin(%s)",
                  adapter->plugin_lib_name, adapter->name);
    }

    neu_plugin_t *plugin;
    adapter->plugin_lib    = handle;
    adapter->plugin_module = plugin_module;
    plugin = plugin_module->intf_funs->open(adapter, &callback_funs);
    if (plugin == NULL) {
        neu_panic("Can't to open plugin(%s)", plugin_module->module_name);
    }
    adapter->plugin = plugin;

open_pipe:
    rv = nng_pair1_open(&adapter->sock);
    if (rv != 0) {
        neu_panic("The adapter(%s) can't open pipe", adapter->name);
    }

    return adapter;
}

void neu_adapter_destroy(neu_adapter_t *adapter)
{
    if (adapter == NULL) {
        neu_panic("Destroied adapter is NULL");
    }

    nng_close(adapter->sock);
    if (adapter->plugin_module != NULL) {
        adapter->plugin_module->intf_funs->close(adapter->plugin);
    }
    if (adapter->plugin_lib != NULL) {
        unload_plugin_library(adapter->plugin_lib);
    }
    if (adapter->name != NULL) {
        free(adapter->name);
    }
    if (adapter->plugin_lib_name != NULL) {
        free(adapter->plugin_lib_name);
    }
    nng_mtx_free(adapter->mtx);
    free(adapter);
    return;
}

int neu_adapter_start(neu_adapter_t *adapter, neu_manager_t *manager)
{
    int rv = 0;

    if (adapter == NULL || manager == NULL) {
        log_error("Start adapter with NULL adapter or manager");
        return (-1);
    }

    if (adapter->plugin_module != NULL) {
        const neu_plugin_intf_funs_t *intf_funs;
        intf_funs = adapter->plugin_module->intf_funs;
        intf_funs->init(adapter->plugin);
    }

    adapter->manager = manager;
    nng_thread_create(&adapter->thrd, adapter_loop, adapter);
    return rv;
}

int neu_adapter_stop(neu_adapter_t *adapter, neu_manager_t *manager)
{
    int rv = 0;

    if (adapter == NULL || manager == NULL) {
        log_error("Stop adapter with NULL adapter or manager");
        return -1;
    }

    log_info("Stop the adapter(%s)", adapter->name);
    nng_mtx_lock(adapter->mtx);
    adapter->stop = true;
    nng_mtx_unlock(adapter->mtx);
    nng_thread_destroy(adapter->thrd);

    if (adapter->plugin_module != NULL) {
        const neu_plugin_intf_funs_t *intf_funs;
        intf_funs = adapter->plugin_module->intf_funs;
        intf_funs->uninit(adapter->plugin);
    }

    return rv;
}

const char *neu_adapter_get_name(neu_adapter_t *adapter)
{
    if (adapter == NULL) {
        return NULL;
    }

    return (const char *) adapter->name;
}

neu_manager_t *neu_adapter_get_manager(neu_adapter_t *adapter)
{
    if (adapter == NULL) {
        return NULL;
    }

    return (neu_manager_t *) adapter->manager;
}

nng_socket neu_adapter_get_sock(neu_adapter_t *adapter)
{
    if (adapter == NULL) {
        nng_socket sock;

        sock.id = 0;
        return sock;
    }

    return adapter->sock;
}

adapter_id_t neu_adapter_get_id(neu_adapter_t *adapter)
{
    if (adapter == NULL) {
        return 0;
    }

    return adapter->id;
}
