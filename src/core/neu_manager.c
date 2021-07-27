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

#include "adapter/adapter_internal.h"
#include "neu_adapter.h"
#include "neu_log.h"
#include "neu_manager.h"
#include "neu_panic.h"

typedef struct adapter_bind_info {
    uint32_t       adapter_id;
    neu_adapter_t *adapter;
    nng_pipe       adapter_pipe;
    int            bind_count;
} adapter_bind_info_t;

// TODO: The array of bound adapters should not be fixed
#define MAX_BIND_ADAPTERS 100
static adapter_bind_info_t g_bind_adapters[MAX_BIND_ADAPTERS];

typedef struct manager_bind_info {
    nng_mtx *            mtx;
    nng_cv *             cv;
    nng_pipe             mng_pipe;
    nng_socket           mng_sock;
    int                  bind_count;
    adapter_bind_info_t *bind_adapters;
} manager_bind_info_t;

typedef enum manager_state {
    MANAGER_STATE_NULL,
    MANAGER_STATE_IDLE,
    MANAGER_STATE_RUNNING,
} manager_state_e;

struct neu_manager {
    const char *        listen_url;
    nng_mtx *           mtx;
    manager_state_e     state;
    bool                stop;
    nng_thread *        thrd;
    uint32_t            new_adapter_id;
    manager_bind_info_t bind_info;
};

static const char *const manager_url = "inproc://neu_manager";

#define NEU_HAS_SAMPLE_ADAPTER 1
#ifdef NEU_HAS_SAMPLE_ADAPTER
#if defined(__APPLE__)
#define SAMPLE_PLUGIN_LIB_NAME "libsample-plugin.dylib"
#else
#define SAMPLE_PLUGIN_LIB_NAME "libsample-plugin.so"
#endif

static neu_adapter_info_t sample_adapter_info = { .type = ADAPTER_TYPE_DRIVER,
    .name                                               = "sample-adapter",
    .plugin_lib_name = SAMPLE_PLUGIN_LIB_NAME };

neu_adapter_t *sample_adapter;
#endif

#define NEU_HAS_MQTT_ADAPTER 1
#ifdef NEU_HAS_MQTT_ADAPTER
#if defined(__APPLE__)
#define MQTT_PLUGIN_LIB_NAME "libmqtt-plugin.dylib"
#else
#define MQTT_PLUGIN_LIB_NAME "libmqtt-plugin.so"
#endif

static neu_adapter_info_t mqtt_adapter_info = { .type = ADAPTER_TYPE_MQTT,
    .name                                             = "mqtt-adapter",
    .plugin_lib_name                                  = MQTT_PLUGIN_LIB_NAME };

neu_adapter_t *mqtt_adapter;
#endif

#define ADAPTER_NAME_MAX_LEN 50
#define PLUGIN_LIB_NAME_MAX_LEN 50

typedef struct adapter_add_cmd {
    adapter_type_e type;
    char           name[ADAPTER_NAME_MAX_LEN + 1];
    char           plugin_lib_name[PLUGIN_LIB_NAME_MAX_LEN + 1];
} adapter_add_cmd_t;

static const adapter_add_cmd_t builtin_adapter_add_cmds[] = {
#ifdef NEU_HAS_SAMPLE_ADAPTER
    { .type              = ADAPTER_TYPE_DRIVER,
        .name            = "sample-adapter",
        .plugin_lib_name = SAMPLE_PLUGIN_LIB_NAME },
#endif
    /*
        {
            .type             = ADAPTER_TYPE_WEBSERVER,
            .name             = "webserver-adapter",
            .plugin_lib_name = WEBSERVER_PLUGIN_LIB_NAME
        },
        {
            .type             = ADAPTER_TYPE_MQTT,
            .name             = "mqtt-adapter",
            .plugin_lib_name = MQTT_PLUGIN_LIB_NAME
        },
        {
            .type             = ADAPTER_TYPE_DRIVER,
            .name             = "modbus-adapter",
            .plugin_lib_name = MODBUS_PLUGIN_LIB_NAME
        },
    */
};

static int init_bind_info(manager_bind_info_t *mng_bind_info)
{
    int rv, rv1;

    if (mng_bind_info == NULL) {
        return (-1);
    }

    rv  = nng_mtx_alloc(&mng_bind_info->mtx);
    rv1 = nng_cv_alloc(&mng_bind_info->cv, mng_bind_info->mtx);
    if (rv || rv1) {
        neu_panic("Failed to initialize mutex and cv in manager_bind_info");
    }

    mng_bind_info->bind_count    = 0;
    mng_bind_info->bind_adapters = &g_bind_adapters[0];

    return 0;
}

static int uninit_bind_info(manager_bind_info_t *mng_bind_info)
{
    if (mng_bind_info->bind_count > 0) {
        log_warn("It has some bound adapter in manager");
    }

    nng_cv_free(mng_bind_info->cv);
    nng_mtx_free(mng_bind_info->mtx);
    mng_bind_info->bind_count = 0;
    return 0;
}

static int manager_add_config(neu_manager_t *manager)
{
    int rv = 0;

    return rv;
}

static uint32_t manager_get_adapter_id(neu_manager_t *manager)
{
    uint32_t adapter_id;

    adapter_id = manager->new_adapter_id++;
    return adapter_id;
}

static void manager_bind_adapter(nng_pipe p, nng_pipe_ev ev, void *arg)
{
    int            rv = 0;
    neu_adapter_t *adapter;

    adapter = (neu_adapter_t *) arg;
    log_info(
        "The manager will bind adapter(%s)", neu_adapter_get_name(adapter));

    return;
}

static void manager_unbind_adapter(nng_pipe p, nng_pipe_ev ev, void *arg)
{
    int            rv = 0;
    neu_adapter_t *adapter;

    adapter = (neu_adapter_t *) arg;
    log_info(
        "The manager will unbind adapter(%s)", neu_adapter_get_name(adapter));

    return;
}

static int manager_add_adapter(neu_manager_t *manager, adapter_add_cmd_t *cmd)
{
    int rv = 0;

    return rv;
}

static int manager_remove_adapter(
    neu_manager_t *manager, neu_adapter_t *adapter)
{
    int rv = 0;

    return rv;
}

static int manager_start_adapter(neu_manager_t *manager, neu_adapter_t *adapter)
{
    int rv = 0;

    rv = nng_pipe_notify(neu_adapter_get_sock(adapter), NNG_PIPE_EV_ADD_POST,
        manager_bind_adapter, adapter);
    rv = nng_pipe_notify(neu_adapter_get_sock(adapter), NNG_PIPE_EV_REM_POST,
        manager_unbind_adapter, adapter);
    neu_adapter_start(adapter, manager);
    return rv;
}

static int manager_stop_adapter(neu_manager_t *manager, neu_adapter_t *adapter)
{
    int      rv = 0;
    size_t   msg_size;
    nng_msg *msg;

    msg_size = msg_inplace_data_get_size(sizeof(uint32_t));
    rv       = nng_msg_alloc(&msg, msg_size);
    if (rv != 0) {
        neu_panic("Can't allocate msg for stop adapter(%s)",
            neu_adapter_get_name(adapter));
    }

    void *     buf_ptr;
    message_t *pay_msg;
    pay_msg = (message_t *) nng_msg_body(msg);
    msg_inplace_data_init(pay_msg, MSG_CMD_EXIT_LOOP, sizeof(uint32_t));
    buf_ptr                           = msg_get_buf_ptr(pay_msg);
    *(uint32_t *) buf_ptr             = 0; // exit_code is 0
    manager_bind_info_t *manager_bind = &manager->bind_info;
    nng_sendmsg(manager_bind->mng_sock, msg, 0);
    neu_adapter_stop(adapter, manager);
    return rv;
}

static void manager_loop(void *arg)
{
    int            rv;
    neu_manager_t *manager;

    manager                           = (neu_manager_t *) arg;
    manager_bind_info_t *manager_bind = &manager->bind_info;
    rv = nng_pair1_open_poly(&manager_bind->mng_sock);
    if (rv != 0) {
        neu_panic("Can't open the manager pipe");
    }

    rv = nng_listen(manager_bind->mng_sock, manager->listen_url, NULL, 0);
    if (rv != 0) {
        neu_panic("Neuron manager can't listen on %s", manager->listen_url);
    }

    nng_mtx_lock(manager->mtx);
    manager->state = MANAGER_STATE_RUNNING;
    nng_mtx_unlock(manager->mtx);

#ifdef NEU_HAS_SAMPLE_ADAPTER
    sample_adapter_info.id = manager_get_adapter_id(manager);
    sample_adapter         = neu_adapter_create(&sample_adapter_info);
    manager_start_adapter(manager, sample_adapter);
#endif

#ifdef NEU_HAS_MQTT_ADAPTER
    mqtt_adapter_info.id = manager_get_adapter_id(manager);
    mqtt_adapter         = neu_adapter_create(&mqtt_adapter_info);
    manager_start_adapter(manager, mqtt_adapter);
#endif

    while (1) {
        nng_msg *msg;

        nng_mtx_lock(manager->mtx);
        if (manager->stop) {
            manager->state = MANAGER_STATE_NULL;
            nng_mtx_unlock(manager->mtx);
            log_info("Exit loop of the manager");
            break;
        }
        nng_mtx_unlock(manager->mtx);

        rv = nng_recvmsg(manager_bind->mng_sock, &msg, 0);
        if (rv != 0) {
            log_warn("Manage pipe no message received");
            continue;
        }

        message_t *pay_msg;
        pay_msg = nng_msg_body(msg);
        switch (msg_get_type(pay_msg)) {
        case MSG_EVENT_STATUS_STRING: {
            char *buf_ptr;
            buf_ptr = msg_get_buf_ptr(pay_msg);
            log_info("Recieve string: %s", buf_ptr);

            const char *adapter_str = "manager recv reply";
            nng_msg *   out_msg;
            size_t      msg_size;
            msg_size = msg_inplace_data_get_size(strlen(adapter_str) + 1);
            rv       = nng_msg_alloc(&out_msg, msg_size);
            if (rv == 0) {
                message_t *msg_ptr;
                char *     buf_ptr;
                msg_ptr = (message_t *) nng_msg_body(out_msg);
                msg_inplace_data_init(
                    msg_ptr, MSG_CONFIG_INFO_STRING, strlen(adapter_str) + 1);
                buf_ptr = msg_get_buf_ptr(msg_ptr);
                memcpy(buf_ptr, adapter_str, strlen(adapter_str));
                buf_ptr[strlen(adapter_str)] = 0;
                nng_sendmsg(manager_bind->mng_sock, out_msg, 0);
            }

            // for debug
            usleep(1000);
            nng_msg *read_msg;
            msg_size = msg_inplace_data_get_size(sizeof(uint32_t));
            rv       = nng_msg_alloc(&read_msg, msg_size);
            if (rv == 0) {
                message_t *msg_ptr;
                void *     buf_ptr;

                msg_ptr = (message_t *) nng_msg_body(read_msg);
                msg_inplace_data_init(
                    msg_ptr, MSG_CMD_START_READ, sizeof(uint32_t));
                buf_ptr               = msg_get_buf_ptr(msg_ptr);
                *(uint32_t *) buf_ptr = 3;
                nng_sendmsg(manager_bind->mng_sock, read_msg, 0);
            }
            break;
        }

        case MSG_CMD_EXIT_LOOP: {
            uint32_t exit_code;

            exit_code = *(uint32_t *) msg_get_buf_ptr(pay_msg);
            log_info("manager exit loop by exit_code=%d", exit_code);
            nng_mtx_lock(manager->mtx);
            manager->state = MANAGER_STATE_NULL;
            manager->stop  = true;
            nng_mtx_unlock(manager->mtx);
            break;
        }

        case MSG_DATA_NEURON_DATABUF: {
            core_databuf_t *databuf;

            databuf = (core_databuf_t *) msg_get_buf_ptr(pay_msg);

            log_info("dispatch databuf to subscribes");
            // TODO: dispatch the databuf to subsribes

            core_databuf_put(databuf);
            break;
        }

        default:
            log_warn("Receive a not supported message(type: %d)",
                msg_get_type(pay_msg));
            break;
        }

        nng_msg_free(msg);
    }

#ifdef NEU_HAS_SAMPLE_ADAPTER
    manager_stop_adapter(manager, sample_adapter);
    neu_adapter_destroy(sample_adapter);
#endif
    nng_close(manager_bind->mng_sock);
    return;
}

neu_manager_t *neu_manager_create()
{
    neu_manager_t *manager;

    manager = malloc(sizeof(neu_manager_t));
    if (manager == NULL) {
        log_error("Out of memeory for create neuron manager");
        return NULL;
    }

    manager->state          = MANAGER_STATE_NULL;
    manager->stop           = false;
    manager->listen_url     = manager_url;
    manager->new_adapter_id = 1;
    int rv;
    if ((rv = nng_mtx_alloc(&manager->mtx)) != 0) {
        log_error("Can't allocate mutex for manager");
        free(manager);
        return NULL;
    }

    init_bind_info(&manager->bind_info);
    nng_thread_create(&manager->thrd, manager_loop, manager);
    return manager;
}

void neu_manager_destroy(neu_manager_t *manager)
{
    nng_thread_destroy(manager->thrd);
    nng_mtx_free(manager->mtx);
    uninit_bind_info(&manager->bind_info);
    free(manager);
    return;
}

const char *neu_manager_get_url(neu_manager_t *manager)
{
    if (manager == NULL) {
        return NULL;
    }

    return manager->listen_url;
}
