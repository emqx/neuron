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
#include "message.h"
#include "neu_adapter.h"
#include "neu_datatag_manager.h"
#include "neu_log.h"
#include "neu_manager.h"
#include "neu_panic.h"
#include "neu_vector.h"
#include "plugin_manager.h"

typedef struct adapter_reg_entity {
    adapter_id_t           adapter_id;
    neu_adapter_t *        adapter;
    neu_datatag_manager_t *datatag_manager;
    nng_cv *               cv;
    nng_pipe               adapter_pipe;
    uint32_t               bind_count;
} adapter_reg_entity_t;

typedef struct manager_bind_info {
    nng_mtx *  mtx;
    nng_cv *   cv;
    nng_socket mng_sock;
    uint32_t   bind_count;
    uint32_t   expect_bind_count;
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

// definition for plugin lib names
#if defined(__APPLE__)

#ifdef NEU_HAS_SAMPLE_ADAPTER
#define SAMPLE_DRV_PLUGIN_LIB_NAME "libplugin-sample-drv.dylib"
#define SAMPLE_APP_PLUGIN_LIB_NAME "libplugin-sample-app.dylib"
#endif
#define WEBSERVER_PLUGIN_LIB_NAME "libplugin-webserver-proxy.dylib"
#define MQTT_PLUGIN_LIB_NAME "libplugin-mqtt.dylib"
#define MODBUS_TCP_PLUGIN_LIB_NAME "libplugin-modbus-tcp.dylib"
#define OPCUA_PLUGIN_LIB_NAME "libplugin-opcua.dylib"

#else

#ifdef NEU_HAS_SAMPLE_ADAPTER
#define SAMPLE_DRV_PLUGIN_LIB_NAME "libplugin-sample-drv.so"
#define SAMPLE_APP_PLUGIN_LIB_NAME "libplugin-sample-app.so"
#endif
#define WEBSERVER_PLUGIN_LIB_NAME "libplugin-webserver-proxy.so"
#define MQTT_PLUGIN_LIB_NAME "libplugin-mqtt.so"
#define MODBUS_TCP_PLUGIN_LIB_NAME "libplugin-modbus-tcp.so"
#define OPCUA_PLUGIN_LIB_NAME "libplugin-opcua.so"

#endif

// definition for plugin names
#ifdef NEU_HAS_SAMPLE_ADAPTER
#define SAMPLE_DRV_PLUGIN_NAME "sample-drv-plugin"
#define SAMPLE_APP_PLUGIN_NAME "sample-app-plugin"
#endif
#define WEBSERVER_PLUGIN_NAME "webserver-plugin-proxy"
#define MQTT_PLUGIN_NAME "mqtt-plugin"
#define MODBUS_TCP_PLUGIN_NAME "modbus-tcp-plugin"
#define OPCUA_PLUGIN_NAME "opcua-plugin"

// definition for adapter names
#ifdef NEU_HAS_SAMPLE_ADAPTER
#define SAMPLE_DRV_ADAPTER_NAME "sample-driver-adapter"
#define SAMPLE_APP_ADAPTER_NAME "sample-app-adapter"
#endif
#define DEFAULT_DASHBOARD_ADAPTER_NAME "default-dashboard-adapter"
#define WEBSERVER_ADAPTER_NAME "webserver-adapter"
#define MQTT_ADAPTER_NAME "mqtt-adapter"
#define MODBUS_TCP_ADAPTER_NAME "modbus-tcp-adapter"
#define OPCUA_ADAPTER_NAME "opcua-adapter"

#define ADAPTER_NAME_MAX_LEN 90
#define GRP_CONFIG_NAME_MAX_LEN 90
#define PLUGIN_LIB_NAME_MAX_LEN 90

typedef struct adapter_reg_cmd {
    adapter_type_e adapter_type;
    const char *   adapter_name;
    const char *   plugin_name;
    // optional value, it is nothing when set to 0
    plugin_id_t plugin_id;
} adapter_reg_cmd_t;

static const adapter_reg_cmd_t default_adapter_reg_cmds[] = {
#ifdef NEU_HAS_SAMPLE_ADAPTER
    {
        .adapter_type = ADAPTER_TYPE_DRIVER,
        .adapter_name = SAMPLE_DRV_ADAPTER_NAME,
        .plugin_name  = SAMPLE_DRV_PLUGIN_NAME,
        .plugin_id    = { 0 } // The plugin_id is nothing
    },
    {
        .adapter_type = ADAPTER_TYPE_APP,
        .adapter_name = SAMPLE_APP_ADAPTER_NAME,
        .plugin_name  = SAMPLE_APP_PLUGIN_NAME,
        .plugin_id    = { 0 } // The plugin_id is nothing
    },
#endif

    {
        .adapter_type = ADAPTER_TYPE_MQTT,
        .adapter_name = MQTT_ADAPTER_NAME,
        .plugin_name  = MQTT_PLUGIN_NAME,
        .plugin_id    = { 0 } // The plugin_id is nothing
    },
    {
        .adapter_type = ADAPTER_TYPE_DRIVER,
        .adapter_name = "modbus-tcp-adapter",
        .plugin_name  = MODBUS_TCP_PLUGIN_NAME,
        .plugin_id    = { 0 } // The plugin_id is nothing
    },
    {
        .adapter_type = ADAPTER_TYPE_WEBSERVER,
        .adapter_name = DEFAULT_DASHBOARD_ADAPTER_NAME,
        .plugin_name  = DEFAULT_DASHBOARD_PLUGIN_NAME,
        .plugin_id    = { 0 } // The plugin_id is nothing
    },
    /*
    {   // for remote customize webserver by user, the plugin is a proxy
        .adapter_type    = ADAPTER_TYPE_WEBSERVER,
        .adapter_name    = WEBSERVER_ADAPTER_NAME,
        .plugin_name     = WEBSERVER_PLUGIN_NAME,
        .plugin_id       = { 0 }    // The plugin_id is nothing
    },
    */
    {
        .adapter_type = ADAPTER_TYPE_DRIVER,
        .adapter_name = OPCUA_ADAPTER_NAME,
        .plugin_name  = OPCUA_PLUGIN_NAME,
        .plugin_id    = { 0 } // The plugin_id is nothing
    },
};
#define DEFAULT_ADAPTER_ADD_INFO_SIZE \
    (sizeof(default_adapter_reg_cmds) / sizeof(default_adapter_reg_cmds[0]))

static const char *default_plugin_lib_names[] = {
#ifdef NEU_HAS_SAMPLE_ADAPTER
    SAMPLE_DRV_PLUGIN_LIB_NAME, SAMPLE_APP_PLUGIN_LIB_NAME,
#endif

    MQTT_PLUGIN_LIB_NAME,       MODBUS_TCP_PLUGIN_LIB_NAME,
    OPCUA_PLUGIN_LIB_NAME,
    /*
    WEBSERVER_PLUGIN_LIB_NAME,
    MODBUS_PLUGIN_LIB_NAME,
     */
};
#define DEFAULT_PLUGIN_COUNT \
    (sizeof(default_plugin_lib_names) / sizeof(default_plugin_lib_names[0]))

// clang-format off
static const plugin_reg_param_t default_plugin_infos[] = {
#ifdef NEU_HAS_SAMPLE_ADAPTER
    {
        .plugin_kind     = PLUGIN_KIND_SYSTEM,
        .adapter_type    = ADAPTER_TYPE_DRIVER,
        .plugin_name     = SAMPLE_DRV_PLUGIN_NAME,
        .plugin_lib_name = SAMPLE_DRV_PLUGIN_LIB_NAME
    },
    {
        .plugin_kind     = PLUGIN_KIND_SYSTEM,
        .adapter_type    = ADAPTER_TYPE_APP,
        .plugin_name     = SAMPLE_APP_PLUGIN_NAME,
        .plugin_lib_name = SAMPLE_APP_PLUGIN_LIB_NAME
    },
#endif

    {
        .plugin_kind     = PLUGIN_KIND_SYSTEM,
        .adapter_type    = ADAPTER_TYPE_MQTT,
        .plugin_name     = MQTT_PLUGIN_NAME,
        .plugin_lib_name = MQTT_PLUGIN_LIB_NAME
    },
    {
        .plugin_kind     = PLUGIN_KIND_SYSTEM,
        .adapter_type    = ADAPTER_TYPE_DRIVER,
        .plugin_name     = MODBUS_TCP_PLUGIN_NAME,
        .plugin_lib_name = MODBUS_TCP_PLUGIN_LIB_NAME
    },
    {
        .plugin_kind     = PLUGIN_KIND_STATIC,
        .adapter_type    = ADAPTER_TYPE_WEBSERVER,
        .plugin_name     = DEFAULT_DASHBOARD_PLUGIN_NAME,
        .plugin_lib_name = DEFAULT_DASHBOARD_PLUGIN_LIB_NAME
    },
    /*
    {
        .plugin_kind     = PLUGIN_KIND_SYSTEM,
        .adapter_type    = ADAPTER_TYPE_WEBSERVER,
        .plugin_name     = WEBSERVER_PLUGIN_NAME,
        .plugin_lib_name = WEBSERVER_PLUGIN_LIB_NAME
    },
    */
    {
        .plugin_kind     = PLUGIN_KIND_SYSTEM,
        .adapter_type    = ADAPTER_TYPE_DRIVER,
        .plugin_name     = OPCUA_PLUGIN_NAME,
        .plugin_lib_name = OPCUA_PLUGIN_LIB_NAME
    },
};
// clang-format on
#define DEFAULT_PLUGIN_INFO_SIZE \
    (sizeof(default_plugin_infos) / sizeof(default_plugin_infos[0]))

typedef struct config_add_cmd {
    char *               config_name;
    char *               src_adapter_name;
    char *               sub_adapter_name;
    uint32_t             read_interval;
    neu_taggrp_config_t *grp_config;
} config_add_cmd_t;

static config_add_cmd_t default_config_add_cmds[] = {
#ifdef NEU_HAS_SAMPLE_ADAPTER
    {
        .config_name      = "config_drv_sample",
        .src_adapter_name = SAMPLE_DRV_ADAPTER_NAME,
        .sub_adapter_name = SAMPLE_APP_ADAPTER_NAME,
        .read_interval    = 2000,
        .grp_config       = NULL,
    },
    {
        .config_name      = "config_app_sample",
        .src_adapter_name = SAMPLE_APP_ADAPTER_NAME,
        .sub_adapter_name = SAMPLE_DRV_ADAPTER_NAME,
        .read_interval    = 2000,
        .grp_config       = NULL,
    },
    {
        .config_name      = "config_mqtt_sample",
        .src_adapter_name = SAMPLE_DRV_ADAPTER_NAME,
        .sub_adapter_name = MQTT_ADAPTER_NAME,
        .read_interval    = 2000,
        .grp_config       = NULL,
    },
    {
        .config_name      = "config_modbus_tcp_sample",
        .src_adapter_name = SAMPLE_APP_ADAPTER_NAME,
        .sub_adapter_name = MODBUS_TCP_ADAPTER_NAME,
        .read_interval    = 2000,
        .grp_config       = NULL,
    },
    {
        .config_name      = "config_modbus_tcp_sample_2",
        .src_adapter_name = MODBUS_TCP_ADAPTER_NAME,
        .sub_adapter_name = MQTT_ADAPTER_NAME,
        .read_interval    = 2000,
        .grp_config       = NULL,
    },
    {
        .config_name      = "config_opcua_sample",
        .src_adapter_name = OPCUA_ADAPTER_NAME,
        .sub_adapter_name = MQTT_ADAPTER_NAME,
        .read_interval    = 2000,
        .grp_config       = NULL,
    },
#endif
};
#define DEFAULT_GROUP_CONFIG_COUNT \
    (sizeof(default_config_add_cmds) / sizeof(default_config_add_cmds[0]))

static int init_bind_info(manager_bind_info_t *mng_bind_info)
{
    int rv;

    if (mng_bind_info == NULL) {
        return (-1);
    }

    rv = nng_mtx_alloc(&mng_bind_info->mtx);
    if (rv != 0) {
        neu_panic("Failed to initialize mutex in manager_bind_info");
    }

    rv = nng_cv_alloc(&mng_bind_info->cv, mng_bind_info->mtx);
    if (rv != 0) {
        neu_panic("Failed to initialize condition(cv) in manager_bind_info");
    }

    mng_bind_info->bind_count        = 0;
    mng_bind_info->expect_bind_count = 0;
    return 0;
}

static int uninit_bind_info(manager_bind_info_t *mng_bind_info)
{
    if (mng_bind_info->bind_count > 0) {
        log_warn("It has some bound adapter in manager");
    }

    nng_cv_free(mng_bind_info->cv);
    nng_mtx_free(mng_bind_info->mtx);
    mng_bind_info->bind_count        = 0;
    mng_bind_info->expect_bind_count = 0;
    return 0;
}

static bool match_id_reg_adapter(const void *key, const void *item)
{
    return *(adapter_id_t *) key == ((adapter_reg_entity_t *) item)->adapter_id;
}

static bool match_name_reg_adapter(const void *key, const void *item)
{
    neu_adapter_t *adapter;

    adapter = ((adapter_reg_entity_t *) item)->adapter;
    return strcmp((const char *) key, neu_adapter_get_name(adapter)) == 0;
}

// Return SIZE_MAX if can't find a adapter
static size_t find_reg_adapter_index_by_id(vector_t *adapters, adapter_id_t id)
{
    return vector_find_index(adapters, &id, match_id_reg_adapter);
}

static adapter_reg_entity_t *find_reg_adapter_by_id(vector_t *   adapters,
                                                    adapter_id_t id)
{
    return vector_find_item(adapters, &id, match_id_reg_adapter);
}

static adapter_reg_entity_t *find_reg_adapter_by_name(vector_t *  adapters,
                                                      const char *name)
{
    return vector_find_item(adapters, name, match_name_reg_adapter);
}

static adapter_id_t manager_new_adapter_id(neu_manager_t *manager)
{
    adapter_id_t adapter_id;

    nng_mtx_lock(manager->adapters_mtx);
    adapter_id = manager->new_adapter_id++;
    nng_mtx_unlock(manager->adapters_mtx);
    return adapter_id;
}
neu_node_id_t neu_manager_adapter_id_to_node_id(neu_manager_t *manager,
                                                adapter_id_t   adapter_id)
{
    (void) manager;
    return adapter_id;
}
adapter_id_t neu_manager_adapter_id_from_node_id(neu_manager_t *manager,
                                                 neu_node_id_t  node_id)
{
    (void) manager;
    return node_id;
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
        nng_cv_wake(reg_entity->cv);
        nng_cv_wake(manager->bind_info.cv);
        nng_mtx_unlock(manager->bind_info.mtx);
        log_info("The manager bind the adapter(%s) with pipe(%d)",
                 neu_adapter_get_name(adapter), p);
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
        reg_entity->bind_count   = 0;
        manager->bind_info.bind_count--;
        manager->bind_info.expect_bind_count--;
        nng_mtx_unlock(manager->bind_info.mtx);
        log_info("The manager unbind the adapter(%s)",
                 neu_adapter_get_name(adapter));
    }

    return;
}

// The output parameter p_adapter hold a new adapter
static adapter_id_t manager_reg_adapter(neu_manager_t *    manager,
                                        adapter_reg_cmd_t *reg_param,
                                        neu_adapter_t **   p_adapter)
{
    int                rv;
    neu_adapter_t *    adapter;
    neu_adapter_info_t adapter_info;
    plugin_reg_info_t  plugin_reg_info;

    if (reg_param->plugin_id.id_val != 0) {
        rv = plugin_manager_get_reg_info(
            manager->plugin_manager, reg_param->plugin_id, &plugin_reg_info);
    } else {
        rv = plugin_manager_get_reg_info_by_name(
            manager->plugin_manager, reg_param->plugin_name, &plugin_reg_info);
    }
    if (rv != 0) {
        log_error("Can't find plugin: %s", reg_param->plugin_name);
        return 0;
    }

    adapter_reg_entity_t *same_reg_entity;
    same_reg_entity = find_reg_adapter_by_name(&manager->reg_adapters,
                                               reg_param->adapter_name);
    if (same_reg_entity != NULL) {
        log_warn("A adapter with same name has already been registered");
        return 0;
    }

    adapter_info.id              = manager_new_adapter_id(manager);
    adapter_info.type            = reg_param->adapter_type;
    adapter_info.name            = reg_param->adapter_name;
    adapter_info.plugin_id       = plugin_reg_info.plugin_id;
    adapter_info.plugin_kind     = plugin_reg_info.plugin_kind;
    adapter_info.plugin_lib_name = plugin_reg_info.plugin_lib_name;
    adapter                      = neu_adapter_create(&adapter_info, manager);
    if (adapter == NULL) {
        return 0;
    }

    neu_datatag_manager_t *datatag_manager;
    datatag_manager = neu_datatag_mng_create(adapter);
    if (datatag_manager == NULL) {
        neu_adapter_destroy(adapter);
        return 0;
    }

    adapter_reg_entity_t reg_entity;
    manager_bind_info_t *mng_bind_info;
    mng_bind_info = &manager->bind_info;
    rv            = nng_cv_alloc(&reg_entity.cv, mng_bind_info->mtx);
    if (rv != 0) {
        log_error("Failed to new cv for register adapter");
        neu_datatag_mng_destroy(datatag_manager);
        neu_adapter_destroy(adapter);
        return 0;
    }

    reg_entity.adapter_id      = adapter_info.id;
    reg_entity.adapter         = adapter;
    reg_entity.datatag_manager = datatag_manager;
    reg_entity.bind_count      = 0;
    nng_mtx_lock(manager->adapters_mtx);
    vector_push_back(&manager->reg_adapters, &reg_entity);
    nng_mtx_unlock(manager->adapters_mtx);
    if (p_adapter != NULL) {
        *p_adapter = adapter;
    }
    log_debug("register adapter id: %d, type: %d, name: %s",
              reg_entity.adapter_id, adapter_info.type, adapter_info.name);
    return reg_entity.adapter_id;
}

static int manager_unreg_adapter(neu_manager_t *manager, adapter_id_t id,
                                 bool need_erase)
{
    int                    rv = 0;
    size_t                 index;
    vector_t *             reg_adapters;
    nng_cv *               cv;
    neu_adapter_t *        adapter;
    neu_datatag_manager_t *datatag_mng;

    adapter      = NULL;
    datatag_mng  = NULL;
    cv           = NULL;
    reg_adapters = &manager->reg_adapters;

    nng_mtx_lock(manager->adapters_mtx);
    index = find_reg_adapter_index_by_id(reg_adapters, id);
    if (index != SIZE_MAX) {
        adapter_reg_entity_t *reg_entity;

        reg_entity  = (adapter_reg_entity_t *) vector_get(reg_adapters, index);
        adapter     = reg_entity->adapter;
        datatag_mng = reg_entity->datatag_manager;
        cv          = reg_entity->cv;
        if (need_erase) {
            vector_erase(reg_adapters, index);
        }
    }
    nng_mtx_unlock(manager->adapters_mtx);

    if (cv != NULL) {
        nng_cv_free(cv);
    }
    if (datatag_mng != NULL) {
        neu_datatag_mng_destroy(datatag_mng);
    }

    if (adapter != NULL) {
        neu_adapter_destroy(adapter);
    }
    return rv;
}

static int manager_start_adapter(neu_manager_t *manager, neu_adapter_t *adapter)
{
    int rv = 0;

    manager_bind_info_t *bind_info;

    bind_info = &manager->bind_info;
    nng_mtx_lock(bind_info->mtx);
    while (bind_info->bind_count != bind_info->expect_bind_count) {
        nng_cv_wait(bind_info->cv);
    }

    // A new adapter will to be bind
    bind_info->expect_bind_count++;
    nng_mtx_unlock(bind_info->mtx);

    rv = nng_pipe_notify(manager->bind_info.mng_sock, NNG_PIPE_EV_ADD_POST,
                         manager_bind_adapter, adapter);
    rv = nng_pipe_notify(manager->bind_info.mng_sock, NNG_PIPE_EV_REM_POST,
                         manager_unbind_adapter, adapter);
    neu_adapter_start(adapter);
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
    neu_adapter_stop(adapter);
    return rv;
}

// Call this function before start manager loop, so it don't need lock
static int register_default_plugins(neu_manager_t *manager)
{
    int                rv = 0;
    uint32_t           i, j;
    plugin_id_t        plugin_id;
    plugin_reg_param_t reg_param;
    int                path_len = 64;

    for (i = 0; i < DEFAULT_PLUGIN_COUNT; i++) {
        plugin_id.id_val = 0;
        for (j = 0; j < DEFAULT_PLUGIN_INFO_SIZE; j++) {
            if (strcmp(default_plugin_infos[j].plugin_lib_name,
                       default_plugin_lib_names[i]) == 0) {
                break;
            }
        }

        if (j < DEFAULT_PLUGIN_INFO_SIZE) {
            reg_param                 = default_plugin_infos[j];
            reg_param.plugin_lib_name = calloc(1, path_len);

            snprintf(reg_param.plugin_lib_name, path_len, "./%s",
                     default_plugin_infos[j].plugin_lib_name);
            rv = plugin_manager_reg_plugin(manager->plugin_manager, &reg_param,
                                           &plugin_id);
            free(reg_param.plugin_lib_name);
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
static void reg_and_start_default_adapters(neu_manager_t *manager)
{
    uint32_t       i;
    adapter_id_t   id;
    neu_adapter_t *p_adapter;

    for (i = 0; i < DEFAULT_ADAPTER_ADD_INFO_SIZE; i++) {
        p_adapter = NULL;
        id        = manager_reg_adapter(
            manager, (adapter_reg_cmd_t *) &default_adapter_reg_cmds[i],
            &p_adapter);
        if (id != 0 && p_adapter != NULL) {
            manager_start_adapter(manager, p_adapter);
        }
    }
    return;
}

// Call this function after quit manager loop, so it don't need lock
static void stop_and_unreg_bind_adapters(neu_manager_t *manager)
{
    vector_t *            reg_adapters;
    adapter_reg_entity_t *reg_entity;

    reg_adapters = &manager->reg_adapters;
    VECTOR_FOR_EACH(reg_adapters, iter)
    {
        neu_adapter_t *adapter;

        reg_entity = (adapter_reg_entity_t *) iterator_get(&iter);
        adapter    = reg_entity->adapter;
        if (adapter != NULL) {
            manager_stop_adapter(manager, adapter);
            manager_unreg_adapter(manager, reg_entity->adapter_id, false);
        }
    }
}

static bool pipe_equal(const void *a, const void *b)
{
    return *(uint32_t *) a == *(uint32_t *) b;
}

static int sub_grp_config_with_pipe(neu_taggrp_config_t *grp_config,
                                    nng_pipe             sub_pipe)

{
    int       rv = 0;
    vector_t *sub_pipes;

    sub_pipes = neu_taggrp_cfg_get_subpipes(grp_config);
    if (vector_has_elem(sub_pipes, &sub_pipe, pipe_equal)) {
        return 0;
    }

    rv = vector_push_back(sub_pipes, &sub_pipe);
    if (rv != 0) {
        log_error("Can't add pipe to vector of subscribe pipes");
        return -1;
    }

    // if we add first one pipe, then need forward subscribe message
    return sub_pipes->size == 1 ? 1 : 0;
}

static int sub_grp_config_with_adapter(neu_manager_t *       manager,
                                       neu_taggrp_config_t * grp_config,
                                       adapter_reg_entity_t *reg_entity)
{
    manager_bind_info_t *bind_info;

    bind_info = &manager->bind_info;
    nng_mtx_lock(bind_info->mtx);
    // wait manage bind the adapter
    while (reg_entity->bind_count == 0) {
        nng_cv_wait(reg_entity->cv);
    }
    nng_mtx_unlock(bind_info->mtx);

    return sub_grp_config_with_pipe(grp_config, reg_entity->adapter_pipe);
}

static int unsub_grp_config_with_pipe(neu_taggrp_config_t *grp_config,
                                      nng_pipe             sub_pipe)
{
    int       rv = 0;
    size_t    index;
    vector_t *sub_pipes;

    sub_pipes = neu_taggrp_cfg_get_subpipes(grp_config);
    index     = vector_find_index(sub_pipes, &sub_pipe, pipe_equal);
    if (index == SIZE_MAX) {
        return 0;
    }

    rv = vector_erase(sub_pipes, index);
    if (rv != 0) {
        log_error("Can't remove pipe from vector of subscribe pipes");
        return -1;
    }

    // if we remove last one pipe, then need forward unsubscribe message
    return sub_pipes->size == 0 ? 1 : 0;
}

static int unsub_grp_config_with_adapter(neu_manager_t *       manager,
                                         neu_taggrp_config_t * grp_config,
                                         adapter_reg_entity_t *reg_entity)
{
    manager_bind_info_t *bind_info;

    bind_info = &manager->bind_info;
    nng_mtx_lock(bind_info->mtx);
    // wait manage bind the adapter
    while (reg_entity->bind_count == 0) {
        nng_cv_wait(reg_entity->cv);
    }
    nng_mtx_unlock(bind_info->mtx);

    return unsub_grp_config_with_pipe(grp_config, reg_entity->adapter_pipe);
}

static int add_grp_config_to_adapter(adapter_reg_entity_t *reg_entity,
                                     neu_taggrp_config_t * grp_config)
{
    int rv = 0;

    neu_datatag_manager_t *datatag_mng;
    datatag_mng = reg_entity->datatag_manager;
    rv          = neu_datatag_mng_add_grp_config(datatag_mng, grp_config);
    if (rv != 0) {
        log_error("Failed to add datatag group config: %s",
                  neu_taggrp_cfg_get_name(grp_config));
        neu_taggrp_cfg_free(grp_config);
        return rv;
    }
    return rv;
}

/*
static int manager_add_config_by_name(neu_manager_t *   manager,
                                      config_add_cmd_t *cmd)
{
    int                   rv = 0;
    adapter_reg_entity_t *reg_entity;

    nng_mtx_lock(manager->adapters_mtx);
    reg_entity =
        find_reg_adapter_by_name(&manager->reg_adapters, cmd->src_adapter_name);
    if (reg_entity == NULL) {
        log_error("Can't find matched src registered adapter");
        rv = -1;
        goto add_config_by_name_exit;
    }

    rv = add_grp_config_to_adapter(reg_entity, cmd->grp_config);
add_config_by_name_exit:
    nng_mtx_unlock(manager->adapters_mtx);
    return rv;
}
*/

static void add_default_grp_configs(neu_manager_t *manager)
{
    uint32_t              i;
    config_add_cmd_t *    config_add_cmd;
    neu_taggrp_config_t * grp_config;
    adapter_reg_entity_t *src_reg_entity;
    adapter_reg_entity_t *sub_reg_entity;

    size_t   msg_size;
    nng_msg *out_msg;
    int      need_send;
    int      rv;

    for (i = 0; i < DEFAULT_GROUP_CONFIG_COUNT; i++) {
        config_add_cmd = &default_config_add_cmds[i];
        grp_config     = neu_taggrp_cfg_new(config_add_cmd->config_name);
        neu_taggrp_cfg_set_interval(grp_config, config_add_cmd->read_interval);
        config_add_cmd->grp_config = grp_config;
        src_reg_entity             = find_reg_adapter_by_name(
            &manager->reg_adapters, config_add_cmd->src_adapter_name);
        sub_reg_entity = find_reg_adapter_by_name(
            &manager->reg_adapters, config_add_cmd->sub_adapter_name);
        add_grp_config_to_adapter(src_reg_entity, grp_config);
        need_send =
            sub_grp_config_with_adapter(manager, grp_config, sub_reg_entity);

        /* Send the subscribe node message to driver
         */
        msg_size = msg_inplace_data_get_size(sizeof(subscribe_node_cmd_t));
        rv       = nng_msg_alloc(&out_msg, msg_size);
        if (rv == 0 && need_send == 1) {
            message_t *           msg_ptr;
            subscribe_node_cmd_t *out_cmd_ptr;
            msg_ptr = (message_t *) nng_msg_body(out_msg);
            msg_inplace_data_init(msg_ptr, MSG_CMD_SUBSCRIBE_NODE,
                                  sizeof(subscribe_node_cmd_t));
            out_cmd_ptr              = msg_get_buf_ptr(msg_ptr);
            out_cmd_ptr->grp_config  = grp_config;
            out_cmd_ptr->sender_id   = 0;
            out_cmd_ptr->dst_node_id = neu_manager_adapter_id_to_node_id(
                manager, src_reg_entity->adapter_id);
            nng_msg_set_pipe(out_msg, src_reg_entity->adapter_pipe);
            log_info("Send subscribe driver command to driver pipe: %d",
                     src_reg_entity->adapter_pipe);
            nng_sendmsg(manager->bind_info.mng_sock, out_msg, 0);
        }
    }
    return;
}

static void remove_default_grp_configs(neu_manager_t *manager)
{
    uint32_t              i;
    config_add_cmd_t *    config_add_cmd;
    neu_taggrp_config_t * grp_config;
    adapter_reg_entity_t *src_reg_entity;
    adapter_reg_entity_t *sub_reg_entity;

    for (i = 0; i < DEFAULT_GROUP_CONFIG_COUNT; i++) {
        config_add_cmd = &default_config_add_cmds[i];
        grp_config     = config_add_cmd->grp_config;

        nng_mtx_lock(manager->adapters_mtx);
        src_reg_entity = find_reg_adapter_by_name(
            &manager->reg_adapters, config_add_cmd->src_adapter_name);
        sub_reg_entity = find_reg_adapter_by_name(
            &manager->reg_adapters, config_add_cmd->sub_adapter_name);
        unsub_grp_config_with_adapter(manager, grp_config, sub_reg_entity);
        neu_datatag_mng_del_grp_config(src_reg_entity->datatag_manager,
                                       config_add_cmd->config_name);
        nng_mtx_unlock(manager->adapters_mtx);

        /* The neuron manager should be shutdown, so there is no need to
         * send the unsubscribe node message to driver.
         */
    }
    return;
}

static int dispatch_databuf_to_adapters(neu_manager_t *   manager,
                                        neuron_databuf_t *neu_databuf)
{
    int       rv = 0;
    vector_t *sub_pipes;

    manager_bind_info_t *manager_bind = &manager->bind_info;
    if (neu_databuf->grp_config == NULL) {
        nng_pipe              msg_pipe;
        adapter_reg_entity_t *reg_entity;

        sub_pipes = vector_new(DEFAULT_ADAPTER_REG_COUNT, sizeof(nng_pipe));

        nng_mtx_lock(manager->adapters_mtx);
        reg_entity = find_reg_adapter_by_name(&manager->reg_adapters,
                                              SAMPLE_APP_ADAPTER_NAME);
        msg_pipe   = reg_entity->adapter_pipe;
        vector_push_back(sub_pipes, &msg_pipe);

        reg_entity =
            find_reg_adapter_by_name(&manager->reg_adapters, MQTT_ADAPTER_NAME);
        msg_pipe = reg_entity->adapter_pipe;
        vector_push_back(sub_pipes, &msg_pipe);
        nng_mtx_unlock(manager->adapters_mtx);
    } else {
        sub_pipes =
            (vector_t *) neu_taggrp_cfg_ref_subpipes(neu_databuf->grp_config);
    }

    log_info("dispatch databuf to %d subscribes in sub_pipes", sub_pipes->size);
    VECTOR_FOR_EACH(sub_pipes, iter)
    {
        size_t   msg_size;
        nng_msg *out_msg;
        nng_pipe msg_pipe;

        msg_pipe = *(nng_pipe *) iterator_get(&iter);
        msg_size = msg_inplace_data_get_size(sizeof(neuron_databuf_t));
        rv       = nng_msg_alloc(&out_msg, msg_size);
        if (rv == 0) {
            message_t *       msg_ptr;
            neuron_databuf_t *out_neu_databuf;
            msg_ptr = (message_t *) nng_msg_body(out_msg);
            msg_inplace_data_init(msg_ptr, MSG_DATA_NEURON_DATABUF,
                                  sizeof(neuron_databuf_t));
            out_neu_databuf = msg_get_buf_ptr(msg_ptr);
            out_neu_databuf->grp_config =
                (neu_taggrp_config_t *) neu_taggrp_cfg_ref(
                    neu_databuf->grp_config);
            out_neu_databuf->databuf = core_databuf_get(neu_databuf->databuf);
            nng_msg_set_pipe(out_msg, msg_pipe);
            log_debug("Forward databuf to pipe: %d", msg_pipe);
            nng_sendmsg(manager_bind->mng_sock, out_msg, 0);
        }
    }

    if (neu_databuf->grp_config == NULL) {
        vector_free(sub_pipes);
    }
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

    register_default_plugins(manager);
    reg_and_start_default_adapters(manager);
    add_default_grp_configs(manager);
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
        case MSG_EVENT_NODE_PING: {
            char *buf_ptr;
            buf_ptr = msg_get_buf_ptr(pay_msg);
            log_debug("Recieve ping: %s", buf_ptr);

            const char *adapter_str = "manager recv reply";
            nng_msg *   out_msg;
            size_t      msg_size;
            nng_pipe    msg_pipe;
            msg_size = msg_inplace_data_get_size(strlen(adapter_str) + 1);
            msg_pipe = nng_msg_get_pipe(msg);
            rv       = nng_msg_alloc(&out_msg, msg_size);
            if (rv == 0) {
                message_t *msg_ptr;
                char *     buf_ptr;
                msg_ptr = (message_t *) nng_msg_body(out_msg);
                msg_inplace_data_init(msg_ptr, MSG_CMD_RESP_PONG,
                                      strlen(adapter_str) + 1);
                buf_ptr = msg_get_buf_ptr(msg_ptr);
                memcpy(buf_ptr, adapter_str, strlen(adapter_str));
                buf_ptr[strlen(adapter_str)] = 0;
                nng_msg_set_pipe(out_msg, msg_pipe);
                log_info("Reply pong to pipe: %d", msg_pipe);
                nng_sendmsg(manager_bind->mng_sock, out_msg, 0);
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

        case MSG_CMD_READ_DATA: {
            size_t                msg_size;
            nng_msg *             out_msg;
            nng_pipe              msg_pipe;
            adapter_id_t          adapter_id;
            read_data_cmd_t *     cmd_ptr;
            adapter_reg_entity_t *reg_entity;

            cmd_ptr = (read_data_cmd_t *) msg_get_buf_ptr(pay_msg);
            nng_mtx_lock(manager->adapters_mtx);
            adapter_id = neu_manager_adapter_id_from_node_id(
                manager, cmd_ptr->dst_node_id);
            reg_entity =
                find_reg_adapter_by_id(&manager->reg_adapters, adapter_id);
            msg_pipe = reg_entity->adapter_pipe;
            nng_mtx_unlock(manager->adapters_mtx);
            msg_size = msg_inplace_data_get_size(sizeof(read_data_cmd_t));
            rv       = nng_msg_alloc(&out_msg, msg_size);
            if (rv == 0) {
                message_t *      msg_ptr;
                read_data_cmd_t *out_cmd_ptr;
                msg_ptr = (message_t *) nng_msg_body(out_msg);
                msg_inplace_data_init(msg_ptr, MSG_CMD_READ_DATA,
                                      sizeof(read_data_cmd_t));
                out_cmd_ptr = msg_get_buf_ptr(msg_ptr);
                memcpy(out_cmd_ptr, cmd_ptr, sizeof(read_data_cmd_t));
                nng_msg_set_pipe(out_msg, msg_pipe);
                log_info("Forward read command to driver pipe: %d", msg_pipe);
                nng_sendmsg(manager_bind->mng_sock, out_msg, 0);
            }
            break;
        }

        case MSG_DATA_READ_RESP: {
            size_t                msg_size;
            nng_msg *             out_msg;
            nng_pipe              msg_pipe;
            adapter_id_t          adapter_id;
            read_data_resp_t *    cmd_ptr;
            adapter_reg_entity_t *reg_entity;

            cmd_ptr = (read_data_resp_t *) msg_get_buf_ptr(pay_msg);
            nng_mtx_lock(manager->adapters_mtx);
            adapter_id = neu_manager_adapter_id_from_node_id(
                manager, cmd_ptr->recver_id);
            reg_entity =
                find_reg_adapter_by_id(&manager->reg_adapters, adapter_id);
            msg_pipe = reg_entity->adapter_pipe;
            nng_mtx_unlock(manager->adapters_mtx);
            msg_size = msg_inplace_data_get_size(sizeof(read_data_resp_t));
            rv       = nng_msg_alloc(&out_msg, msg_size);
            if (rv == 0) {
                message_t *       msg_ptr;
                read_data_resp_t *out_cmd_ptr;
                msg_ptr = (message_t *) nng_msg_body(out_msg);
                msg_inplace_data_init(msg_ptr, MSG_DATA_READ_RESP,
                                      sizeof(read_data_resp_t));
                out_cmd_ptr = msg_get_buf_ptr(msg_ptr);
                memcpy(out_cmd_ptr, cmd_ptr, sizeof(read_data_resp_t));
                nng_msg_set_pipe(out_msg, msg_pipe);
                log_info("Forward read response to app pipe: %d", msg_pipe);
                nng_sendmsg(manager_bind->mng_sock, out_msg, 0);
            }
            break;
        }

        case MSG_CMD_WRITE_DATA: {
            size_t                msg_size;
            nng_msg *             out_msg;
            nng_pipe              msg_pipe;
            adapter_id_t          adapter_id;
            write_data_cmd_t *    cmd_ptr;
            adapter_reg_entity_t *reg_entity;

            cmd_ptr = (write_data_cmd_t *) msg_get_buf_ptr(pay_msg);
            nng_mtx_lock(manager->adapters_mtx);
            adapter_id = neu_manager_adapter_id_from_node_id(
                manager, cmd_ptr->dst_node_id);
            reg_entity =
                find_reg_adapter_by_id(&manager->reg_adapters, adapter_id);
            msg_pipe = reg_entity->adapter_pipe;
            nng_mtx_unlock(manager->adapters_mtx);
            msg_size = msg_inplace_data_get_size(sizeof(write_data_cmd_t));
            rv       = nng_msg_alloc(&out_msg, msg_size);
            if (rv == 0) {
                message_t *       msg_ptr;
                write_data_cmd_t *out_cmd_ptr;
                msg_ptr = (message_t *) nng_msg_body(out_msg);
                msg_inplace_data_init(msg_ptr, MSG_CMD_WRITE_DATA,
                                      sizeof(write_data_cmd_t));
                out_cmd_ptr = msg_get_buf_ptr(msg_ptr);
                memcpy(out_cmd_ptr, cmd_ptr, sizeof(write_data_cmd_t));
                nng_msg_set_pipe(out_msg, msg_pipe);
                log_info("Forward write command to driver pipe: %d", msg_pipe);
                nng_sendmsg(manager_bind->mng_sock, out_msg, 0);
            }
            break;
        }

        case MSG_DATA_WRITE_RESP: {
            size_t                msg_size;
            nng_msg *             out_msg;
            nng_pipe              msg_pipe;
            adapter_id_t          adapter_id;
            write_data_resp_t *   cmd_ptr;
            adapter_reg_entity_t *reg_entity;

            cmd_ptr = (write_data_resp_t *) msg_get_buf_ptr(pay_msg);
            nng_mtx_lock(manager->adapters_mtx);
            adapter_id = neu_manager_adapter_id_from_node_id(
                manager, cmd_ptr->recver_id);
            reg_entity =
                find_reg_adapter_by_id(&manager->reg_adapters, adapter_id);
            msg_pipe = reg_entity->adapter_pipe;
            nng_mtx_unlock(manager->adapters_mtx);
            msg_size = msg_inplace_data_get_size(sizeof(write_data_resp_t));
            rv       = nng_msg_alloc(&out_msg, msg_size);
            if (rv == 0) {
                message_t *        msg_ptr;
                write_data_resp_t *out_cmd_ptr;
                msg_ptr = (message_t *) nng_msg_body(out_msg);
                msg_inplace_data_init(msg_ptr, MSG_DATA_WRITE_RESP,
                                      sizeof(write_data_resp_t));
                out_cmd_ptr = msg_get_buf_ptr(msg_ptr);
                memcpy(out_cmd_ptr, cmd_ptr, sizeof(write_data_resp_t));
                nng_msg_set_pipe(out_msg, msg_pipe);
                log_info("Forward write response to app pipe: %d", msg_pipe);
                nng_sendmsg(manager_bind->mng_sock, out_msg, 0);
            }
            break;
        }

        case MSG_CMD_SUBSCRIBE_NODE: {
            size_t       msg_size;
            nng_msg *    out_msg;
            nng_pipe     msg_pipe;
            int          need_forward;
            adapter_id_t adapter_id;

            subscribe_node_cmd_t *cmd_ptr;
            adapter_reg_entity_t *reg_entity;

            cmd_ptr = (subscribe_node_cmd_t *) msg_get_buf_ptr(pay_msg);
            nng_mtx_lock(manager->adapters_mtx);
            adapter_id = neu_manager_adapter_id_from_node_id(
                manager, cmd_ptr->dst_node_id);
            reg_entity =
                find_reg_adapter_by_id(&manager->reg_adapters, adapter_id);
            msg_pipe = reg_entity->adapter_pipe;
            nng_mtx_unlock(manager->adapters_mtx);
            msg_size = msg_inplace_data_get_size(sizeof(subscribe_node_cmd_t));
            rv       = nng_msg_alloc(&out_msg, msg_size);
            need_forward = sub_grp_config_with_pipe(cmd_ptr->grp_config,
                                                    nng_msg_get_pipe(msg));
            if (rv == 0 && need_forward == 1) {
                message_t *           msg_ptr;
                subscribe_node_cmd_t *out_cmd_ptr;
                msg_ptr = (message_t *) nng_msg_body(out_msg);
                msg_inplace_data_init(msg_ptr, MSG_CMD_SUBSCRIBE_NODE,
                                      sizeof(subscribe_node_cmd_t));
                out_cmd_ptr = msg_get_buf_ptr(msg_ptr);
                memcpy(out_cmd_ptr, cmd_ptr, sizeof(subscribe_node_cmd_t));
                nng_msg_set_pipe(out_msg, msg_pipe);
                log_info("Forward subscribe driver command to driver pipe: %d",
                         msg_pipe);
                nng_sendmsg(manager_bind->mng_sock, out_msg, 0);
            }
            break;
        }

        case MSG_CMD_UNSUBSCRIBE_NODE: {
            size_t       msg_size;
            nng_msg *    out_msg;
            nng_pipe     msg_pipe;
            int          need_forward;
            adapter_id_t adapter_id;

            unsubscribe_node_cmd_t *cmd_ptr;
            adapter_reg_entity_t *  reg_entity;

            cmd_ptr = (unsubscribe_node_cmd_t *) msg_get_buf_ptr(pay_msg);
            nng_mtx_lock(manager->adapters_mtx);
            adapter_id = neu_manager_adapter_id_from_node_id(
                manager, cmd_ptr->dst_node_id);
            reg_entity =
                find_reg_adapter_by_id(&manager->reg_adapters, adapter_id);
            msg_pipe = reg_entity->adapter_pipe;
            nng_mtx_unlock(manager->adapters_mtx);
            msg_size =
                msg_inplace_data_get_size(sizeof(unsubscribe_node_cmd_t));
            rv           = nng_msg_alloc(&out_msg, msg_size);
            need_forward = unsub_grp_config_with_pipe(cmd_ptr->grp_config,
                                                      nng_msg_get_pipe(msg));
            if (rv == 0 || need_forward == 1) {
                message_t *             msg_ptr;
                unsubscribe_node_cmd_t *out_cmd_ptr;
                msg_ptr = (message_t *) nng_msg_body(out_msg);
                msg_inplace_data_init(msg_ptr, MSG_CMD_UNSUBSCRIBE_NODE,
                                      sizeof(unsubscribe_node_cmd_t));
                out_cmd_ptr = msg_get_buf_ptr(msg_ptr);
                memcpy(out_cmd_ptr, cmd_ptr, sizeof(unsubscribe_node_cmd_t));
                nng_msg_set_pipe(out_msg, msg_pipe);
                log_info(
                    "Forward unsubscribe driver command to driver pipe: %d",
                    msg_pipe);
                nng_sendmsg(manager_bind->mng_sock, out_msg, 0);
            }
            break;
        }

        case MSG_EVENT_GROUP_CONFIG_CHANGED: {
            size_t       msg_size;
            nng_msg *    out_msg;
            nng_pipe     msg_pipe;
            adapter_id_t adapter_id;

            grp_config_changed_event_t *event_ptr;
            adapter_reg_entity_t *      reg_entity;

            event_ptr = (grp_config_changed_event_t *) msg_get_buf_ptr(pay_msg);
            nng_mtx_lock(manager->adapters_mtx);
            adapter_id = neu_manager_adapter_id_from_node_id(
                manager, event_ptr->dst_node_id);
            reg_entity =
                find_reg_adapter_by_id(&manager->reg_adapters, adapter_id);
            msg_pipe = reg_entity->adapter_pipe;
            nng_mtx_unlock(manager->adapters_mtx);
            msg_size =
                msg_inplace_data_get_size(sizeof(grp_config_changed_cmd_t));
            rv = nng_msg_alloc(&out_msg, msg_size);
            if (rv == 0) {
                message_t *               msg_ptr;
                grp_config_changed_cmd_t *out_cmd_ptr;
                msg_ptr = (message_t *) nng_msg_body(out_msg);
                msg_inplace_data_init(msg_ptr, MSG_CMD_GROUP_CONFIG_CHANGED,
                                      sizeof(grp_config_changed_cmd_t));
                out_cmd_ptr              = msg_get_buf_ptr(msg_ptr);
                out_cmd_ptr->grp_config  = event_ptr->grp_config;
                out_cmd_ptr->sender_id   = event_ptr->sender_id;
                out_cmd_ptr->dst_node_id = event_ptr->dst_node_id;
                nng_msg_set_pipe(out_msg, msg_pipe);
                log_info(
                    "Forward group config changed command to driver pipe: %d",
                    msg_pipe);
                nng_sendmsg(manager_bind->mng_sock, out_msg, 0);
            }
            break;
        }

        case MSG_DATA_NEURON_DATABUF: {
            neuron_databuf_t *neu_databuf;

            neu_databuf = (neuron_databuf_t *) msg_get_buf_ptr(pay_msg);
            rv          = dispatch_databuf_to_adapters(manager, neu_databuf);

            neu_taggrp_cfg_free(neu_databuf->grp_config);
            core_databuf_put(neu_databuf->databuf);
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
    remove_default_grp_configs(manager);
    stop_and_unreg_bind_adapters(manager);
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

    rv = vector_init(&manager->reg_adapters, DEFAULT_ADAPTER_REG_COUNT,
                     sizeof(adapter_reg_entity_t));
    if (rv != 0) {
        neu_panic("Failed to initialize vector of registered adapters");
    }

    init_bind_info(&manager->bind_info);
    manager->plugin_manager = plugin_manager_create();
    if (manager->plugin_manager == NULL) {
        neu_panic("Failed to create plugin manager");
    }

    nng_thread_create(&manager->thrd, manager_loop, manager);
    return manager;
}

void neu_manager_stop(neu_manager_t *manager)
{
    nng_mtx_lock(manager->mtx);
    manager->stop = true;
    nng_mtx_unlock(manager->mtx);
}

void neu_manager_destroy(neu_manager_t *manager)
{
    nng_thread_destroy(manager->thrd);
    plugin_manager_destroy(manager->plugin_manager);
    uninit_bind_info(&manager->bind_info);
    vector_uninit(&manager->reg_adapters);

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

static adapter_type_e adapter_type_from_node_type(neu_node_type_e node_type)
{
    adapter_type_e adapter_type;

    switch (node_type) {
    case NEU_NODE_TYPE_DRIVER:
        adapter_type = ADAPTER_TYPE_DRIVER;
        break;

    case NEU_NODE_TYPE_WEBSERVER:
        adapter_type = ADAPTER_TYPE_WEBSERVER;
        break;

    case NEU_NODE_TYPE_MQTT:
        adapter_type = ADAPTER_TYPE_MQTT;
        break;

    case NEU_NODE_TYPE_STREAM_PROCESSOR:
        adapter_type = ADAPTER_TYPE_STREAM_PROCESSOR;
        break;

    case NEU_NODE_TYPE_APP:
        adapter_type = ADAPTER_TYPE_APP;
        break;

    default:
        adapter_type = ADAPTER_TYPE_UNKNOW;
        break;
    }

    return adapter_type;
}

static neu_node_type_e adapter_type_to_node_type(adapter_type_e adapter_type)
{
    neu_node_type_e node_type;

    switch (adapter_type) {
    case ADAPTER_TYPE_DRIVER:
        node_type = NEU_NODE_TYPE_DRIVER;
        break;

    case ADAPTER_TYPE_WEBSERVER:
        node_type = NEU_NODE_TYPE_WEBSERVER;
        break;

    case ADAPTER_TYPE_MQTT:
        node_type = NEU_NODE_TYPE_MQTT;
        break;

    case ADAPTER_TYPE_STREAM_PROCESSOR:
        node_type = NEU_NODE_TYPE_STREAM_PROCESSOR;
        break;

    case ADAPTER_TYPE_APP:
        node_type = NEU_NODE_TYPE_APP;
        break;

    default:
        node_type = NEU_NODE_TYPE_UNKNOW;
        break;
    }

    return node_type;
}

int neu_manager_add_node(neu_manager_t *manager, neu_cmd_add_node_t *cmd,
                         neu_node_id_t *p_node_id)
{
    int               rv = 0;
    adapter_id_t      adapter_id;
    adapter_reg_cmd_t reg_cmd;

    reg_cmd.adapter_type = adapter_type_from_node_type(cmd->node_type);
    reg_cmd.adapter_name = cmd->adapter_name;
    reg_cmd.plugin_name  = cmd->plugin_name;
    reg_cmd.plugin_id    = cmd->plugin_id;
    adapter_id           = manager_reg_adapter(manager, &reg_cmd, NULL);
    *p_node_id = neu_manager_adapter_id_to_node_id(manager, adapter_id);
    return rv;
}

int neu_manager_del_node(neu_manager_t *manager, neu_node_id_t node_id)
{
    int          rv = 0;
    adapter_id_t adapter_id;

    adapter_id = neu_manager_adapter_id_from_node_id(manager, node_id);
    rv         = manager_unreg_adapter(manager, adapter_id, true);
    return rv;
}

int neu_manager_update_node(neu_manager_t *manager, neu_cmd_update_node_t *cmd)
{
    int rv = 0;

    (void) manager;
    (void) cmd;
    return rv;
}

static bool adapter_match_node_type(neu_adapter_t * adapter,
                                    neu_node_type_e node_type)
{
    if (node_type == NEU_NODE_TYPE_UNKNOW) {
        return true;
    } else if (node_type == NEU_NODE_TYPE_DRIVER) {
        return ADAPTER_TYPE_DRIVER == neu_adapter_get_type(adapter);
    } else {
        return ADAPTER_TYPE_DRIVER != neu_adapter_get_type(adapter);
    }
}

int neu_manager_get_nodes(neu_manager_t *manager, neu_node_type_e node_type,
                          vector_t *result_nodes)
{
    int                   rv = 0;
    adapter_reg_entity_t *reg_entity;

    if (manager == NULL || result_nodes == NULL) {
        log_error("get nodes with NULL manager or result_nodes");
        return -1;
    }

    nng_mtx_lock(manager->adapters_mtx);
    VECTOR_FOR_EACH(&manager->reg_adapters, iter)
    {
        reg_entity = (adapter_reg_entity_t *) iterator_get(&iter);
        if (adapter_match_node_type(reg_entity->adapter, node_type)) {
            neu_node_info_t node_info;
            adapter_id_t    adapter_id;
            const char *    adapter_name;

            adapter_name = neu_adapter_get_name(reg_entity->adapter);
            adapter_id   = neu_adapter_get_id(reg_entity->adapter);
            node_info.node_id =
                neu_manager_adapter_id_to_node_id(manager, adapter_id);
            node_info.node_name = strdup(adapter_name);
            node_info.plugin_id =
                neu_adapter_get_plugin_id(reg_entity->adapter);
            vector_push_back(result_nodes, &node_info);
        }
    }
    nng_mtx_unlock(manager->adapters_mtx);

    return rv;
}

int neu_manager_add_grp_config(neu_manager_t *           manager,
                               neu_cmd_add_grp_config_t *cmd)
{
    int                   rv = 0;
    adapter_id_t          adapter_id;
    adapter_reg_entity_t *reg_entity;

    if (manager == NULL || cmd == NULL) {
        log_error("Add group config with NULL manager or command");
        return -1;
    }

    if (cmd->grp_config == NULL) {
        log_error("group config is NULL");
        return -1;
    }

    nng_mtx_lock(manager->adapters_mtx);
    adapter_id = neu_manager_adapter_id_from_node_id(manager, cmd->node_id);
    reg_entity = find_reg_adapter_by_id(&manager->reg_adapters, adapter_id);
    if (reg_entity == NULL) {
        log_error("Can't find matched src registered adapter");
        rv = -1;
        goto add_grp_config_exit;
    }

    rv = add_grp_config_to_adapter(reg_entity, cmd->grp_config);
add_grp_config_exit:
    nng_mtx_unlock(manager->adapters_mtx);
    return rv;
}

int neu_manager_del_grp_config(neu_manager_t *manager, neu_node_id_t node_id,
                               const char *config_name)
{
    int                   rv = 0;
    adapter_id_t          adapter_id;
    adapter_reg_entity_t *reg_entity;

    if (manager == NULL || config_name == NULL) {
        log_error("Delete group config NULL manager or config_name");
        return -1;
    }

    nng_mtx_lock(manager->adapters_mtx);
    adapter_id = neu_manager_adapter_id_from_node_id(manager, node_id);
    reg_entity = find_reg_adapter_by_id(&manager->reg_adapters, adapter_id);
    if (reg_entity == NULL) {
        log_error("Can't find matched registered adapter");
        rv = -1;
        goto del_grp_config_exit;
    }

    rv = neu_datatag_mng_del_grp_config(reg_entity->datatag_manager,
                                        config_name);
    if (rv != 0) {
        log_error("Failed to delete datatag group config: %s", config_name);
    }
del_grp_config_exit:
    nng_mtx_unlock(manager->adapters_mtx);
    return rv;
}

int neu_manager_update_grp_config(neu_manager_t *              manager,
                                  neu_cmd_update_grp_config_t *cmd)
{
    int                   rv = 0;
    adapter_id_t          adapter_id;
    adapter_reg_entity_t *reg_entity;

    if (manager == NULL || cmd == NULL) {
        log_error("Update group config with NULL manager or command");
        return -1;
    }

    if (cmd->grp_config == NULL) {
        log_error("group config is NULL");
        return -1;
    }

    nng_mtx_lock(manager->adapters_mtx);
    adapter_id = neu_manager_adapter_id_from_node_id(manager, cmd->node_id);
    reg_entity = find_reg_adapter_by_id(&manager->reg_adapters, adapter_id);
    if (reg_entity == NULL) {
        log_error("Can't find matched src or dst registered adapter");
        rv = -1;
        goto update_grp_config_exit;
    }

    rv = neu_datatag_mng_update_grp_config(reg_entity->datatag_manager,
                                           cmd->grp_config);
    if (rv != 0) {
        log_error("Failed to update datatag group config: %s",
                  neu_taggrp_cfg_get_name(cmd->grp_config));
        neu_taggrp_cfg_free(cmd->grp_config);
    }

update_grp_config_exit:
    nng_mtx_unlock(manager->adapters_mtx);
    return rv;
}

int neu_manager_get_grp_configs(neu_manager_t *manager, neu_node_id_t node_id,
                                vector_t *result_grp_configs)
{
    int                   rv = 0;
    adapter_reg_entity_t *reg_entity;
    adapter_id_t          adapter_id;

    if (manager == NULL || result_grp_configs == NULL) {
        log_error("get nodes with NULL manager or result_nodes");
        return -1;
    }

    nng_mtx_lock(manager->adapters_mtx);
    adapter_id = neu_manager_adapter_id_from_node_id(manager, node_id);
    VECTOR_FOR_EACH(&manager->reg_adapters, iter)
    {
        reg_entity = (adapter_reg_entity_t *) iterator_get(&iter);
        if (reg_entity->adapter_id == adapter_id) {
            rv = neu_datatag_mng_ref_all_grp_configs(
                reg_entity->datatag_manager, result_grp_configs);
        }
    }
    nng_mtx_unlock(manager->adapters_mtx);

    return rv;
}

int neu_manager_add_plugin_lib(neu_manager_t *           manager,
                               neu_cmd_add_plugin_lib_t *cmd,
                               plugin_id_t *             p_plugin_id)
{
    int                rv;
    plugin_reg_param_t reg_param;

    reg_param.plugin_kind     = cmd->plugin_kind;
    reg_param.adapter_type    = adapter_type_from_node_type(cmd->node_type);
    reg_param.plugin_name     = cmd->plugin_name;
    reg_param.plugin_lib_name = (char *) cmd->plugin_lib_name;
    rv = plugin_manager_reg_plugin(manager->plugin_manager, &reg_param,
                                   p_plugin_id);
    return rv;
}

int neu_manager_del_plugin_lib(neu_manager_t *manager, plugin_id_t plugin_id)
{
    int rv;

    rv = plugin_manager_unreg_plugin(manager->plugin_manager, plugin_id);
    return rv;
}

int neu_manager_update_plugin_lib(neu_manager_t *              manager,
                                  neu_cmd_update_plugin_lib_t *cmd)
{
    int                rv;
    plugin_reg_param_t reg_param;

    reg_param.plugin_kind     = cmd->plugin_kind;
    reg_param.adapter_type    = adapter_type_from_node_type(cmd->node_type);
    reg_param.plugin_name     = cmd->plugin_name;
    reg_param.plugin_lib_name = (char *) cmd->plugin_lib_name;
    rv = plugin_manager_update_plugin(manager->plugin_manager, &reg_param);
    return rv;
}

int neu_manager_get_plugin_libs(neu_manager_t *manager,
                                vector_t *     plugin_lib_infos)
{
    if (manager == NULL || plugin_lib_infos == NULL) {
        log_error("get plugin libs with NULL manager or plugin_lib_infos");
        return -1;
    }

    vector_t *         plugin_regs;
    plugin_reg_info_t *plugin_reg_info;
    plugin_lib_info_t  plugin_lib_info;
    plugin_regs = plugin_manager_get_all_plugins(manager->plugin_manager);
    if (plugin_regs == NULL) {
        return -1;
    }

    VECTOR_FOR_EACH(plugin_regs, iter)
    {
        adapter_type_e adapter_type;

        plugin_reg_info             = (plugin_reg_info_t *) iterator_get(&iter);
        adapter_type                = plugin_reg_info->adapter_type;
        plugin_lib_info.plugin_id   = plugin_reg_info->plugin_id;
        plugin_lib_info.plugin_kind = plugin_reg_info->plugin_kind;
        plugin_lib_info.node_type   = adapter_type_to_node_type(adapter_type);
        plugin_lib_info.plugin_name = plugin_reg_info->plugin_name;
        plugin_lib_info.plugin_lib_name = plugin_reg_info->plugin_lib_name;
        vector_push_back(plugin_lib_infos, &plugin_lib_info);
    }

    vector_free(plugin_regs);
    return 0;
}

neu_datatag_table_t *neu_manager_get_datatag_tbl(neu_manager_t *manager,
                                                 neu_node_id_t  node_id)
{
    adapter_reg_entity_t *reg_entity;
    neu_datatag_table_t * tag_table;
    adapter_id_t          adapter_id;

    if (manager == NULL) {
        log_error("get datatag table with NULL manager");
        return NULL;
    }

    tag_table = NULL;
    nng_mtx_lock(manager->adapters_mtx);
    adapter_id = neu_manager_adapter_id_from_node_id(manager, node_id);
    VECTOR_FOR_EACH(&manager->reg_adapters, iter)
    {
        reg_entity = (adapter_reg_entity_t *) iterator_get(&iter);
        if (reg_entity->adapter_id == adapter_id) {
            tag_table =
                neu_datatag_mng_get_datatag_tbl(reg_entity->datatag_manager);
        }
    }
    nng_mtx_unlock(manager->adapters_mtx);

    return tag_table;
}

int neu_manager_adapter_config_setting(neu_manager_t *manager,
                                       neu_node_id_t  node_id,
                                       const char *   config)
{
    adapter_id_t          adapter_id = { 0 };
    adapter_reg_entity_t *reg_entity = NULL;
    int                   ret        = 0;
    neu_config_t          neu_config = { .type = NEU_CONFIG_SETTING };

    nng_mtx_lock(manager->adapters_mtx);

    adapter_id = neu_manager_adapter_id_from_node_id(manager, node_id);
    reg_entity = find_reg_adapter_by_id(&manager->reg_adapters, adapter_id);

    if (reg_entity == NULL) {
        log_error("Can't find matched src registered adapter");
        nng_mtx_unlock(manager->adapters_mtx);
        return -1;
    }

    neu_config.buf_len = strlen(config);
    neu_config.buf     = strdup(config);

    ret = neu_adapter_config(reg_entity->adapter, &neu_config);
    nng_mtx_unlock(manager->adapters_mtx);

    free(neu_config.buf);

    return ret;
}