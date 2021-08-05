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
#include "neu_vector.h"
#include "plugin_manager.h"

#define DEFAULT_ADAPTER_REG_COUNT 8

typedef struct adapter_reg_entity {
    adapter_id_t   adapter_id;
    neu_adapter_t *adapter;
    nng_pipe       adapter_pipe;
    int            bind_count;
} adapter_reg_entity_t;

typedef struct manager_bind_info {
    nng_mtx *  mtx;
    nng_cv *   cv;
    nng_socket mng_sock;
    int        bind_count;
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
    plugin_manager_t *  plugin_manager;
    nng_mtx *           adapters_mtx;
    vector_t            reg_adapters;
};

static const char *const manager_url = "inproc://neu_manager";

#define NEU_HAS_SAMPLE_ADAPTER 1

#if defined(__APPLE__)

#ifdef NEU_HAS_SAMPLE_ADAPTER
#define SAMPLE_PLUGIN_LIB_NAME "libplugin-sample.dylib"
#endif
#define WEBSERVER_PLUGIN_LIB_NAME "libplugin-webserver-proxy.dylib"
#define MQTT_PLUGIN_LIB_NAME "libplugin-mqtt.dylib"
#define MODBUS_PLUGIN_LIB_NAME "libplugin-modbus.dylib"

#else

#ifdef NEU_HAS_SAMPLE_ADAPTER
#define SAMPLE_PLUGIN_LIB_NAME "libplugin-sample.so"
#endif
#define WEBSERVER_PLUGIN_LIB_NAME "libplugin-webserver-proxy.so"
#define MQTT_PLUGIN_LIB_NAME "libplugin-mqtt.so"
#define MODBUS_PLUGIN_LIB_NAME "libplugin-modbus.so"

#endif

#ifdef NEU_HAS_SAMPLE_ADAPTER
#define SAMPLE_PLUGIN_NAME "sample-plugin"
#endif
#define WEBSERVER_PLUGIN_NAME "webserver-plugin-proxy"
#define MQTT_PLUGIN_NAME "mqtt-plugin"
#define MODBUS_PLUGIN_NAME "modbus-plugin"

#define ADAPTER_NAME_MAX_LEN 50
#define PLUGIN_LIB_NAME_MAX_LEN 50

typedef struct adapter_reg_param {
    adapter_type_e adapter_type;
    const char *   adapter_name;
    const char *   plugin_name;
    // optional value, it is nothing when set to 0
    plugin_id_t plugin_id;
} adapter_reg_param_t;

static const adapter_reg_param_t default_adapter_reg_params[] = {
#ifdef NEU_HAS_SAMPLE_ADAPTER
    {
        .adapter_type = ADAPTER_TYPE_DRIVER,
        .adapter_name = "sample-adapter",
        .plugin_name  = SAMPLE_PLUGIN_NAME,
        .plugin_id    = { 0 } // The plugin_id is nothing
    },
#endif
    /*
    {
        .adapter_type    = ADAPTER_TYPE_WEBSERVER,
        .adapter_name    = "builtin-webserver",
        .plugin_name     = NULL,    // The builtin webserver has'nt plugin
        .plugin_id       = { 0 }    // The plugin_id is nothing
    },
    {   // for remote customize webserver by user, the plugin is a proxy
        .adapter_type    = ADAPTER_TYPE_WEBSERVER,
        .adapter_name    = "webserver-adapter",
        .plugin_name     = WEBSERVER_PLUGIN_NAME,
        .plugin_id       = { 0 }    // The plugin_id is nothing
    },
    {
        .adapter_type    = ADAPTER_TYPE_MQTT,
        .adapter_name    = "mqtt-adapter",
        .plugin_name     = MQTT_PLUGIN_NAME,
        .plugin_id       = { 0 }    // The plugin_id is nothing
    },
    {
        .adapter_type    = ADAPTER_TYPE_DRIVER,
        .adapter_name    = "modbus-adapter",
        .plugin_name     = MODBUS_PLUGIN_NAME,
        .plugin_id       = { 0 }    // The plugin_id is nothing
    },
    */
};
#define DEFAULT_ADAPTER_ADD_INFO_SIZE \
    (sizeof(default_adapter_reg_params) / sizeof(default_adapter_reg_params[0]))

static const char *default_plugin_lib_names[] = {
#ifdef NEU_HAS_SAMPLE_ADAPTER
    SAMPLE_PLUGIN_LIB_NAME,
#endif
    /*
    WEBSERVER_PLUGIN_LIB_NAME,
    MQTT_PLUGIN_LIB_NAME,
    MODBUS_PLUGIN_LIB_NAME,
     */
};
#define DEFAULT_PLUGIN_COUNT \
    (sizeof(default_plugin_lib_names) / sizeof(default_plugin_lib_names[0]))

// clang-format off
static const plugin_reg_param_t system_plugin_infos[] = {
#ifdef NEU_HAS_SAMPLE_ADAPTER
    {
        .plugin_kind     = PLUGIN_KIND_SYSTEM,
        .adapter_type    = ADAPTER_TYPE_DRIVER,
        .plugin_name     = SAMPLE_PLUGIN_NAME,
        .plugin_lib_name = SAMPLE_PLUGIN_LIB_NAME
    },
#endif
    /*
    {
        .plugin_kind     = PLUGIN_KIND_SYSTEM,
        .adapter_type    = ADAPTER_TYPE_WEBSERVER,
        .plugin_name     = WEBSERVER_PLUGIN_NAME,
        .plugin_lib_name = WEBSERVER_PLUGIN_LIB_NAME
    },
    {
        .plugin_kind     = PLUGIN_KIND_SYSTEM,
        .adapter_type    = ADAPTER_TYPE_MQTT,
        .plugin_name     = MQTT_PLUGIN_NAME,
        .plugin_lib_name = MQTT_PLUGIN_LIB_NAME
    },
    {
        .plugin_kind     = PLUGIN_KIND_SYSTEM,
        .adapter_type    = ADAPTER_TYPE_DRIVER,
        .plugin_name     = MODBUS_PLUGIN_NAME,
        .plugin_lib_name = MODBUS_PLUGIN_LIB_NAME
    },
    */
};
// clang-format on
#define SYSTEM_PLUGIN_INFO_SIZE \
    (sizeof(system_plugin_infos) / sizeof(system_plugin_infos[0]))

static int init_bind_info(manager_bind_info_t *mng_bind_info)
{
    int rv, rv1;

    if (mng_bind_info == NULL) {
        return (-1);
    }

    rv  = nng_mtx_alloc(&mng_bind_info->mtx);
    rv1 = nng_cv_alloc(&mng_bind_info->cv, mng_bind_info->mtx);
    if (rv != 0 || rv1 != 0) {
        neu_panic("Failed to initialize mutex and cv in manager_bind_info");
    }

    mng_bind_info->bind_count = 0;
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

// Return SIZE_MAX if can't find a adapter
static size_t find_reg_adapter_index_by_id(vector_t *adapters, adapter_id_t id)
{
    size_t                index = SIZE_MAX;
    adapter_reg_entity_t *reg_entity;

    VECTOR_FOR_EACH(adapters, iter)
    {
        reg_entity = (adapter_reg_entity_t *) iterator_get(&iter);
        if (reg_entity->adapter_id == id) {
            index = iterator_index(adapters, &iter);
            break;
        }
    }

    return index;
}

static adapter_reg_entity_t *find_reg_adapter_by_id(vector_t *   adapters,
                                                    adapter_id_t id)
{
    adapter_reg_entity_t *reg_entity;

    VECTOR_FOR_EACH(adapters, iter)
    {
        reg_entity = (adapter_reg_entity_t *) iterator_get(&iter);
        if (reg_entity->adapter_id == id) {
            return reg_entity;
        }
    }

    return NULL;
}

static uint32_t manager_get_adapter_id(neu_manager_t *manager)
{
    adapter_id_t adapter_id;

    nng_mtx_lock(manager->adapters_mtx);
    adapter_id = manager->new_adapter_id++;
    nng_mtx_unlock(manager->adapters_mtx);
    return adapter_id;
}

static int manager_add_config(neu_manager_t *manager)
{
    int rv = 0;

    (void) manager;

    return rv;
}

static void manager_bind_adapter(nng_pipe p, nng_pipe_ev ev, void *arg)
{
    adapter_id_t          adapter_id;
    neu_adapter_t *       adapter;
    neu_manager_t *       manager;
    adapter_reg_entity_t *reg_entity;

    (void) ev;

    adapter = (neu_adapter_t *) arg;
    manager = neu_adapter_get_manager(adapter);
    nng_mtx_lock(manager->adapters_mtx);
    adapter_id = neu_adapter_get_id(adapter);
    reg_entity = find_reg_adapter_by_id(&manager->reg_adapters, adapter_id);
    nng_mtx_unlock(manager->adapters_mtx);
    if (reg_entity != NULL) {
        nng_mtx_lock(manager->bind_info.mtx);
        reg_entity->adapter_pipe = p;
        reg_entity->bind_count   = 1;
        manager->bind_info.bind_count++;
        nng_mtx_unlock(manager->bind_info.mtx);
        log_info("The manager bind the adapter(%s)",
                 neu_adapter_get_name(adapter));
    }

    return;
}

static void manager_unbind_adapter(nng_pipe p, nng_pipe_ev ev, void *arg)
{
    adapter_id_t          adapter_id;
    neu_adapter_t *       adapter;
    neu_manager_t *       manager;
    adapter_reg_entity_t *reg_entity;

    (void) ev;

    adapter = (neu_adapter_t *) arg;
    manager = neu_adapter_get_manager(adapter);
    nng_mtx_lock(manager->adapters_mtx);
    adapter_id = neu_adapter_get_id(adapter);
    reg_entity = find_reg_adapter_by_id(&manager->reg_adapters, adapter_id);
    nng_mtx_unlock(manager->adapters_mtx);
    if (reg_entity != NULL) {
        manager = neu_adapter_get_manager(adapter);
        nng_mtx_lock(manager->bind_info.mtx);
        reg_entity->adapter_pipe = p;
        reg_entity->bind_count   = 1;
        manager->bind_info.bind_count++;
        nng_mtx_unlock(manager->bind_info.mtx);
        log_info("The manager unbind adapter(%s)",
                 neu_adapter_get_name(adapter));
    }

    return;
}

// The output parameter p_adapter hold a new adapter
static adapter_id_t manager_reg_adapter(neu_manager_t *      manager,
                                        adapter_reg_param_t *reg_param,
                                        neu_adapter_t **     p_adapter)
{
    adapter_id_t       adapter_id;
    neu_adapter_t *    adapter;
    neu_adapter_info_t adapter_info;
    plugin_reg_info_t  plugin_reg_info;

    if (reg_param->plugin_id.id_val != 0) {
        plugin_manager_get_reg_info(manager->plugin_manager,
                                    reg_param->plugin_id, &plugin_reg_info);
    } else {
        plugin_manager_get_reg_info_by_name(
            manager->plugin_manager, reg_param->plugin_name, &plugin_reg_info);
    }

    adapter_id                   = 0;
    adapter_info.id              = manager_get_adapter_id(manager);
    adapter_info.type            = reg_param->adapter_type;
    adapter_info.name            = reg_param->adapter_name;
    adapter_info.plugin_id       = plugin_reg_info.plugin_id;
    adapter_info.plugin_lib_name = plugin_reg_info.plugin_lib_name;
    adapter                      = neu_adapter_create(&adapter_info);
    if (adapter != NULL) {
        adapter_reg_entity_t reg_entity;

        adapter_id            = adapter_info.id;
        reg_entity.adapter_id = adapter_info.id;
        reg_entity.adapter    = adapter;
        reg_entity.bind_count = 0;
        nng_mtx_lock(manager->adapters_mtx);
        vector_push_back(&manager->reg_adapters, &reg_entity);
        nng_mtx_unlock(manager->adapters_mtx);
        if (p_adapter != NULL) {
            *p_adapter = adapter;
        }
    }
    return adapter_id;
}

static int manager_unreg_adapter(neu_manager_t *manager, adapter_id_t id)
{
    int            rv = 0;
    size_t         index;
    vector_t *     reg_adapters;
    neu_adapter_t *adapter;

    adapter      = NULL;
    reg_adapters = &manager->reg_adapters;

    nng_mtx_lock(manager->adapters_mtx);
    index = find_reg_adapter_index_by_id(reg_adapters, id);
    if (index != SIZE_MAX) {
        adapter_reg_entity_t *reg_entity;

        reg_entity = (adapter_reg_entity_t *) vector_get(reg_adapters, index);
        adapter    = reg_entity->adapter;
        vector_erase(reg_adapters, index);
    }
    nng_mtx_unlock(manager->adapters_mtx);

    if (adapter != NULL) {
        neu_adapter_destroy(adapter);
    }
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

// Call this function before start manager loop, so it don't need lock
static int register_default_plugins(neu_manager_t *manager)
{
    int                       rv = 0;
    uint                      i, j;
    plugin_id_t               plugin_id;
    const plugin_reg_param_t *reg_param;

    for (i = 0; i < DEFAULT_PLUGIN_COUNT; i++) {
        plugin_id.id_val = 0;
        for (j = 0; j < SYSTEM_PLUGIN_INFO_SIZE; j++) {
            if (strcmp(system_plugin_infos[j].plugin_lib_name,
                       default_plugin_lib_names[i]) == 0) {
                break;
            }
        }

        if (j < SYSTEM_PLUGIN_INFO_SIZE) {
            reg_param = &system_plugin_infos[j];
            plugin_id =
                plugin_manager_reg_plugin(manager->plugin_manager, reg_param);
            if (plugin_id.id_val == 0) {
                log_warn("Failed to register plugin: %s",
                         reg_param->plugin_name);
            }
        }
    }

    return rv;
}

// Call this function after quit manager loop, so it don't need lock
static void unregister_all_reg_plugins(neu_manager_t *manager)
{
    plugin_manager_unreg_all_plugins(manager->plugin_manager);
    return;
}

// Call this function before start manager loop, so it don't need lock
static void add_and_start_default_adapters(neu_manager_t *manager)
{
    uint           i;
    adapter_id_t   id;
    neu_adapter_t *p_adapter;

    for (i = 0; i < DEFAULT_ADAPTER_ADD_INFO_SIZE; i++) {
        id = manager_reg_adapter(
            manager, (adapter_reg_param_t *) &default_adapter_reg_params[i],
            &p_adapter);
        if (id != 0 && p_adapter != NULL) {
            manager_start_adapter(manager, p_adapter);
        }
    }
    return;
}

// Call this function after quit manager loop, so it don't need lock
static void stop_and_destroy_bind_adapters(neu_manager_t *manager)
{
    vector_t *            adapters;
    adapter_reg_entity_t *reg_entity;

    adapters = &manager->reg_adapters;
    VECTOR_FOR_EACH(adapters, iter)
    {
        neu_adapter_t *adapter;

        reg_entity = (adapter_reg_entity_t *) iterator_get(&iter);
        adapter    = reg_entity->adapter;
        if (adapter != NULL) {
            manager_stop_adapter(manager, adapter);
            manager_unreg_adapter(manager, reg_entity->adapter_id);
        }
    }
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

    register_default_plugins(manager);
    add_and_start_default_adapters(manager);
    log_info("Start message loop of neu_manager");
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
                msg_inplace_data_init(msg_ptr, MSG_CONFIG_INFO_STRING,
                                      strlen(adapter_str) + 1);
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
                msg_inplace_data_init(msg_ptr, MSG_CMD_START_READ,
                                      sizeof(uint32_t));
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

    log_info("End message loop of neu_manager");
    stop_and_destroy_bind_adapters(manager);
    unregister_all_reg_plugins(manager);
    nng_close(manager_bind->mng_sock);
    return;
}

neu_manager_t *neu_manager_create()
{
    neu_manager_t *manager;

    manager = malloc(sizeof(neu_manager_t));
    if (manager == NULL) {
        neu_panic("Out of memeory for create neuron manager");
    }

    manager->state          = MANAGER_STATE_NULL;
    manager->stop           = false;
    manager->listen_url     = manager_url;
    manager->new_adapter_id = 1;
    int rv, rv1;
    rv  = nng_mtx_alloc(&manager->mtx);
    rv1 = nng_mtx_alloc(&manager->adapters_mtx);
    if (rv != 0 || rv1 != 0) {
        neu_panic("Can't allocate mutex for manager");
    }

    rv = vector_setup(&manager->reg_adapters, DEFAULT_ADAPTER_REG_COUNT,
                      sizeof(adapter_reg_entity_t));
    if (rv != 0) {
        neu_panic("Failed to create vector of registered adapters");
    }

    init_bind_info(&manager->bind_info);
    manager->plugin_manager = plugin_manager_create();
    if (manager->plugin_manager == NULL) {
        neu_panic("Failed to create plugin manager");
    }

    nng_thread_create(&manager->thrd, manager_loop, manager);
    return manager;
}

void neu_manager_destroy(neu_manager_t *manager)
{
    nng_thread_destroy(manager->thrd);
    plugin_manager_destroy(manager->plugin_manager);
    uninit_bind_info(&manager->bind_info);
    vector_destroy(&manager->reg_adapters);
    nng_mtx_free(manager->adapters_mtx);
    nng_mtx_free(manager->mtx);
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
