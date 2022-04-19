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

#define NEURON_LOG_LABEL "neu_manager"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <nng/nng.h>
#include <nng/protocol/pair1/pair.h>
#include <nng/supplemental/util/platform.h>

#include "adapter.h"
#include "errcodes.h"
#include "log.h"
#include "message.h"
#include "neu_datatag_manager.h"
#include "neu_manager.h"
#include "neu_trans_buf.h"
#include "neu_vector.h"
#include "panic.h"
#include "persist/persist.h"
#include "plugin_manager.h"
#include "subscribe.h"

typedef struct adapter_reg_entity {
    neu_adapter_id_t       adapter_id;
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
    MANAGER_STATE_READY,
    MANAGER_STATE_RUNNING,
} manager_state_e;

struct neu_manager {
    const char *        listen_url;
    nng_mtx *           mtx;
    nng_cv *            cv;
    manager_state_e     state;
    bool                stop;
    nng_thread *        thrd;
    uint32_t            new_adapter_id;
    manager_bind_info_t bind_info;
    plugin_manager_t *  plugin_manager;
    nng_mtx *           adapters_mtx;
    nng_cv *            adapters_cv;
    bool                is_adapters_freezed;
    vector_t            reg_adapters;
};

static const char *const manager_url = "inproc://neu_manager";

// definition for plugin names
#define WEBSERVER_PLUGIN_NAME "webserver-plugin-proxy"

// definition for adapter names
#define DEFAULT_DASHBOARD_ADAPTER_NAME "default-dashboard-adapter"
#define DEFAULT_PERSIST_ADAPTER_NAME "default-persist-adapter"

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

static const adapter_reg_cmd_t static_adapter_reg_cmds[] = {
    {
        .adapter_type = ADAPTER_TYPE_WEBSERVER,
        .adapter_name = DEFAULT_DASHBOARD_ADAPTER_NAME,
        .plugin_name  = DEFAULT_DASHBOARD_PLUGIN_NAME,
        .plugin_id    = { 0 } // The plugin_id is nothing
    },
    {
        .adapter_type = ADAPTER_TYPE_FUNCTIONAL,
        .adapter_name = DEFAULT_PERSIST_ADAPTER_NAME,
        .plugin_name  = DEFAULT_DUMMY_PLUGIN_NAME,
        .plugin_id    = { 0 } // The plugin_id is nothing
    },
};
#define STATIC_ADAPTER_ADD_INFO_SIZE \
    (sizeof(static_adapter_reg_cmds) / sizeof(static_adapter_reg_cmds[0]))

// clang-format off
static const plugin_reg_param_t static_plugin_infos[] = {
    {
        .plugin_kind     = PLUGIN_KIND_STATIC,
        .adapter_type    = ADAPTER_TYPE_WEBSERVER,
        .plugin_name     = DEFAULT_DASHBOARD_PLUGIN_NAME,
        .plugin_lib_name = DEFAULT_DASHBOARD_PLUGIN_LIB_NAME
    },
    {
        .plugin_kind     = PLUGIN_KIND_STATIC,
        .adapter_type    = ADAPTER_TYPE_FUNCTIONAL,
        .plugin_name     = DEFAULT_DUMMY_PLUGIN_NAME,
        .plugin_lib_name = DEFAULT_DUMMY_PLUGIN_LIB_NAME
    },
};
// clang-format on
#define STATIC_PLUGIN_INFO_SIZE \
    (sizeof(static_plugin_infos) / sizeof(static_plugin_infos[0]))

typedef struct config_add_cmd {
    char *               config_name;
    char *               src_adapter_name;
    char *               sub_adapter_name;
    uint32_t             read_interval;
    neu_taggrp_config_t *grp_config;
} config_add_cmd_t;

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
    return *(neu_adapter_id_t *) key ==
        ((adapter_reg_entity_t *) item)->adapter_id;
}

static bool match_name_reg_adapter(const void *key, const void *item)
{
    neu_adapter_t *adapter;

    adapter = ((adapter_reg_entity_t *) item)->adapter;
    if (adapter == NULL) {
        log_warn("The adapter had been removed");
        return false;
    }
    return strcmp((const char *) key, neu_adapter_get_name(adapter)) == 0;
}

static bool match_pipe_reg_adapter(const void *key, const void *item)
{
    return (*(nng_pipe *) key).id ==
        ((adapter_reg_entity_t *) item)->adapter_pipe.id;
}

// Return SIZE_MAX if can't find a adapter
static size_t find_reg_adapter_index_by_id(vector_t *       adapters,
                                           neu_adapter_id_t id)
{
    return vector_find_index(adapters, &id, match_id_reg_adapter);
}

static adapter_reg_entity_t *find_reg_adapter_by_id(vector_t *       adapters,
                                                    neu_adapter_id_t id)
{
    return vector_find_item(adapters, &id, match_id_reg_adapter);
}

static adapter_reg_entity_t *find_reg_adapter_by_name(vector_t *  adapters,
                                                      const char *name)
{
    return vector_find_item(adapters, name, match_name_reg_adapter);
}

static adapter_reg_entity_t *find_reg_adapter_by_pipe(vector_t *adapters,
                                                      nng_pipe  p)
{
    return vector_find_item(adapters, &p, match_pipe_reg_adapter);
}

static bool is_static_adapter(const char *adapter_name)
{
    for (size_t i = 0; i < STATIC_ADAPTER_ADD_INFO_SIZE; ++i) {
        if (0 ==
            strcmp(static_adapter_reg_cmds[i].adapter_name, adapter_name)) {
            return true;
        }
    }
    return false;
}

static bool is_static_plugin(const char *plugin_name)
{
    for (size_t i = 0; i < STATIC_PLUGIN_INFO_SIZE; ++i) {
        if (0 == strcmp(static_plugin_infos[i].plugin_name, plugin_name)) {
            return true;
        }
    }
    return false;
}

static neu_adapter_id_t manager_new_adapter_id(neu_manager_t *manager)
{
    neu_adapter_id_t adapter_id;

    nng_mtx_lock(manager->adapters_mtx);
    adapter_id = manager->new_adapter_id++;
    nng_mtx_unlock(manager->adapters_mtx);
    return adapter_id;
}

static inline int manager_send_msg_to_pipe(neu_manager_t *manager,
                                           nng_pipe pipe, msg_type_e msg_type,
                                           void *buf, size_t size)
{
    nng_msg *msg = nng_msg_inplace_from_buf(msg_type, buf, size);
    if (NULL == msg) {
        return NEU_ERR_ENOMEM;
    }
    nng_msg_set_pipe(msg, pipe);
    int rv = nng_sendmsg(manager->bind_info.mng_sock, msg, 0);
    if (0 != rv) {
        nng_msg_free(msg);
    }
    return rv;
}

neu_node_id_t neu_manager_adapter_id_to_node_id(neu_manager_t *  manager,
                                                neu_adapter_id_t adapter_id)
{
    (void) manager;
    return adapter_id;
}
neu_adapter_id_t neu_manager_adapter_id_from_node_id(neu_manager_t *manager,
                                                     neu_node_id_t  node_id)
{
    (void) manager;
    return node_id;
}

static void manager_bind_adapter(nng_pipe p, nng_pipe_ev ev, void *arg)
{
    neu_adapter_id_t      adapter_id;
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
    neu_adapter_t *       cur_adapter;
    neu_manager_t *       manager;
    adapter_reg_entity_t *reg_entity;

    (void) ev;

    /* The parameter adapter is a last registered adapter, not current unbind
     * adapter, so we can not use adapter_id of this adapter to find registred
     * adapter entity. We must use pipe to find current unbind adapter.
     */
    manager = (neu_manager_t *) arg;
    nng_mtx_lock(manager->adapters_mtx);
    reg_entity = find_reg_adapter_by_pipe(&manager->reg_adapters, p);
    nng_mtx_unlock(manager->adapters_mtx);
    if (reg_entity != NULL) {
        cur_adapter = reg_entity->adapter;
        log_info("The manager unbind the adapter(%s)",
                 neu_adapter_get_name(cur_adapter));
        nng_mtx_lock(manager->bind_info.mtx);
        reg_entity->bind_count = 0;
        manager->bind_info.bind_count--;
        manager->bind_info.expect_bind_count--;
        nng_cv_wake(reg_entity->cv);
        nng_mtx_unlock(manager->bind_info.mtx);
    }

    return;
}

// The output parameter p_adapter hold a new adapter
static neu_err_code_e manager_reg_adapter(neu_manager_t *    manager,
                                          adapter_reg_cmd_t *reg_param,
                                          neu_adapter_t **   p_adapter,
                                          neu_adapter_id_t * adapter_id)
{
    int                rv = NEU_ERR_SUCCESS;
    neu_adapter_t *    adapter;
    neu_adapter_info_t adapter_info;
    plugin_reg_info_t  plugin_reg_info;

    nng_mtx_lock(manager->mtx);
    if (manager->state == MANAGER_STATE_NULL) {
        return NEU_ERR_ESTATE;
    }
    nng_mtx_unlock(manager->mtx);
    if (reg_param->plugin_id.id_val != 0) {
        rv = plugin_manager_get_reg_info(
            manager->plugin_manager, reg_param->plugin_id, &plugin_reg_info);
    } else {
        rv = plugin_manager_get_reg_info_by_name(
            manager->plugin_manager, reg_param->plugin_name, &plugin_reg_info);
    }
    if (rv != 0) {
        log_error("Can't find plugin: %s", reg_param->plugin_name);
        return NEU_ERR_LIBRARY_NOT_FOUND;
    }

    adapter_reg_entity_t *same_reg_entity;
    nng_mtx_lock(manager->adapters_mtx);
    same_reg_entity = find_reg_adapter_by_name(&manager->reg_adapters,
                                               reg_param->adapter_name);
    nng_mtx_unlock(manager->adapters_mtx);
    if (same_reg_entity != NULL) {
        log_warn("A adapter with same name has already been registered");
        return NEU_ERR_NODE_EXIST;
    }

    adapter_info.id              = manager_new_adapter_id(manager);
    adapter_info.name            = reg_param->adapter_name;
    adapter_info.type            = plugin_reg_info.adapter_type;
    adapter_info.plugin_id       = plugin_reg_info.plugin_id;
    adapter_info.plugin_kind     = plugin_reg_info.plugin_kind;
    adapter_info.plugin_lib_name = plugin_reg_info.plugin_lib_name;
    adapter                      = neu_adapter_create(&adapter_info, manager);
    if (adapter == NULL) {
        return NEU_ERR_EINTERNAL;
    }

    neu_datatag_manager_t *datatag_manager;
    datatag_manager = neu_datatag_mng_create(adapter);
    if (datatag_manager == NULL) {
        neu_adapter_destroy(adapter);
        return NEU_ERR_EINTERNAL;
    }

    adapter_reg_entity_t reg_entity;
    manager_bind_info_t *mng_bind_info;
    mng_bind_info = &manager->bind_info;
    rv            = nng_cv_alloc(&reg_entity.cv, mng_bind_info->mtx);
    if (rv != 0) {
        log_error("Failed to new cv for register adapter");
        neu_datatag_mng_destroy(datatag_manager);
        neu_adapter_destroy(adapter);
        return NEU_ERR_EINTERNAL;
    }

    reg_entity.adapter_id      = adapter_info.id;
    reg_entity.adapter         = adapter;
    reg_entity.datatag_manager = datatag_manager;
    reg_entity.bind_count      = 0;
    nng_mtx_lock(manager->adapters_mtx);
    while (manager->is_adapters_freezed) {
        nng_cv_wait(manager->adapters_cv);
    }
    vector_push_back(&manager->reg_adapters, &reg_entity);
    nng_mtx_unlock(manager->adapters_mtx);
    if (p_adapter != NULL) {
        *p_adapter = adapter;
    }

    log_debug("register adapter id: %d, type: %d, name: %s",
              reg_entity.adapter_id, adapter_info.type, adapter_info.name);

    *adapter_id = reg_entity.adapter_id;
    return rv;
}

static int manager_unreg_adapter(neu_manager_t *manager, neu_adapter_id_t id,
                                 bool need_erase)
{
    size_t                index;
    vector_t *            reg_adapters;
    adapter_reg_entity_t *reg_entity;
    manager_bind_info_t * bind_info;
    neu_adapter_t *       adapter;

    reg_adapters = &manager->reg_adapters;

    nng_mtx_lock(manager->adapters_mtx);
    index = find_reg_adapter_index_by_id(reg_adapters, id);
    if (index == SIZE_MAX) {
        nng_mtx_unlock(manager->adapters_mtx);
        return NEU_ERR_NODE_NOT_EXIST;
    }
    manager->is_adapters_freezed = true;
    reg_entity = (adapter_reg_entity_t *) vector_get(reg_adapters, index);
    nng_mtx_unlock(manager->adapters_mtx);

    // Do not lock adapters_mtx when waiting unbind adapter
    bind_info = &manager->bind_info;
    nng_mtx_lock(bind_info->mtx);
    // wait manager unbind the adapter
    while (reg_entity->bind_count != 0) {
        nng_cv_wait(reg_entity->cv);
    }
    nng_mtx_unlock(bind_info->mtx);

    nng_cv_free(reg_entity->cv);
    neu_datatag_mng_destroy(reg_entity->datatag_manager);
    adapter = reg_entity->adapter;

    nng_mtx_lock(manager->adapters_mtx);
    if (need_erase) {
        vector_erase(reg_adapters, index);
    } else {
        memset(reg_entity, 0, sizeof(adapter_reg_entity_t));
    }
    manager->is_adapters_freezed = false;
    nng_cv_wake(manager->adapters_cv);
    nng_mtx_unlock(manager->adapters_mtx);
    if (adapter != NULL) {
        neu_adapter_destroy(adapter);
    }

    return NEU_ERR_SUCCESS;
}

int neu_manager_init_main_adapter(neu_manager_t * manager,
                                  bind_notify_fun bind_adapter,
                                  bind_notify_fun unbind_adapter)
{
    int rv = 0;

    manager_bind_info_t *bind_info;

    bind_info = &manager->bind_info;
    nng_mtx_lock(bind_info->mtx);
    while (bind_info->bind_count != bind_info->expect_bind_count) {
        nng_cv_wait(bind_info->cv);
    }
    nng_mtx_unlock(bind_info->mtx);

    rv = nng_pipe_notify(manager->bind_info.mng_sock, NNG_PIPE_EV_ADD_POST,
                         bind_adapter, manager);
    rv = nng_pipe_notify(manager->bind_info.mng_sock, NNG_PIPE_EV_REM_POST,
                         unbind_adapter, manager);
    return rv;
}

int neu_manager_init_adapter(neu_manager_t *manager, neu_adapter_t *adapter)
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
                         manager_unbind_adapter, manager);
    neu_adapter_init(adapter);
    return rv;
}

int neu_manager_uninit_adapter(neu_manager_t *manager, neu_adapter_t *adapter)
{
    int rv = 0;

    nng_pipe              msg_pipe = { 0 };
    adapter_reg_entity_t *reg_entity;
    neu_adapter_id_t      adapter_id;

    adapter_id = neu_adapter_get_id(adapter);
    nng_mtx_lock(manager->adapters_mtx);
    reg_entity = find_reg_adapter_by_id(&manager->reg_adapters, adapter_id);
    if (reg_entity != NULL) {
        msg_pipe = reg_entity->adapter_pipe;
    }
    nng_mtx_unlock(manager->adapters_mtx);

    if (msg_pipe.id == 0) {
        log_warn("The adapter(%s) had been unbound",
                 neu_adapter_get_name(adapter));
        goto stop_adapter;
    }

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
    nng_msg_set_pipe(msg, msg_pipe);
    nng_sendmsg(manager_bind->mng_sock, msg, 0);

stop_adapter:
    neu_adapter_uninit(adapter);
    return rv;
}

int neu_manager_start_adapter(neu_adapter_t *adapter)
{
    return neu_adapter_start(adapter);
}

int neu_manager_stop_adapter(neu_adapter_t *adapter)
{
    return neu_adapter_stop(adapter);
}

// Call this function before start manager loop, so it don't need lock
static int register_static_plugins(neu_manager_t *manager)
{
    int                rv = 0;
    plugin_id_t        plugin_id;
    plugin_reg_param_t reg_param;

    for (uint32_t j = 0; j < STATIC_PLUGIN_INFO_SIZE; j++) {
        plugin_id.id_val = 0;
        reg_param        = static_plugin_infos[j];
        rv = plugin_manager_reg_plugin(manager->plugin_manager, &reg_param,
                                       &plugin_id);
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
static void reg_and_start_static_adapters(neu_manager_t *manager)
{
    uint32_t         i;
    neu_adapter_id_t id;
    neu_adapter_t *  p_adapter;

    for (i = 0; i < STATIC_ADAPTER_ADD_INFO_SIZE; i++) {
        p_adapter = NULL;
        manager_reg_adapter(manager,
                            (adapter_reg_cmd_t *) &static_adapter_reg_cmds[i],
                            &p_adapter, &id);
        if (id != 0 && p_adapter != NULL) {
            neu_manager_init_adapter(manager, p_adapter);
            neu_manager_start_adapter(p_adapter);
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
            neu_manager_stop_adapter(adapter);
            neu_manager_uninit_adapter(manager, adapter);
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

static int add_grp_config_to_adapter(adapter_reg_entity_t *reg_entity,
                                     neu_taggrp_config_t * grp_config)
{
    int rv = NEU_ERR_SUCCESS;

    neu_datatag_manager_t *datatag_mng;
    datatag_mng = reg_entity->datatag_manager;
    rv          = neu_datatag_mng_add_grp_config(datatag_mng, grp_config);
    if (rv != 0) {
        log_error("Failed to add datatag group config: %s",
                  neu_taggrp_cfg_get_name(grp_config));
        return rv;
    }
    return rv;
}

static int dispatch_databuf_to_adapters(neu_manager_t *      manager,
                                        neuron_trans_data_t *neu_trans_data)
{
    int       rv = 0;
    vector_t *sub_pipes;

    manager_bind_info_t *manager_bind = &manager->bind_info;
    assert(neu_trans_data->grp_config != NULL);
    sub_pipes =
        (vector_t *) neu_taggrp_cfg_ref_subpipes(neu_trans_data->grp_config);

    neu_trans_buf_t *trans_buf;
    trans_buf = &neu_trans_data->trans_buf;
    log_trace("dispatch databuf to %d subscribes in sub_pipes",
              sub_pipes->size);
    VECTOR_FOR_EACH(sub_pipes, iter)
    {
        size_t   msg_size;
        nng_msg *out_msg;
        nng_pipe msg_pipe;

        msg_pipe = *(nng_pipe *) iterator_get(&iter);
        msg_size = msg_inplace_data_get_size(sizeof(neuron_trans_data_t));
        rv       = nng_msg_alloc(&out_msg, msg_size);
        if (rv == 0) {
            message_t *          msg_ptr;
            neuron_trans_data_t *out_neu_trans_data;
            msg_ptr = (message_t *) nng_msg_body(out_msg);
            msg_inplace_data_init(msg_ptr, MSG_DATA_NEURON_TRANS_DATA,
                                  sizeof(neuron_trans_data_t));
            out_neu_trans_data = msg_get_buf_ptr(msg_ptr);
            out_neu_trans_data->grp_config =
                (neu_taggrp_config_t *) neu_taggrp_cfg_ref(
                    neu_trans_data->grp_config);
            out_neu_trans_data->sender_id = neu_trans_data->sender_id;
            out_neu_trans_data->node_name = neu_trans_data->node_name;
            neu_trans_buf_copy(&out_neu_trans_data->trans_buf, trans_buf);
            nng_msg_set_pipe(out_msg, msg_pipe);
            log_trace("Forward databuf to pipe: %d", msg_pipe);
            nng_sendmsg(manager_bind->mng_sock, out_msg, 0);
        }
    }

    if (neu_trans_data->grp_config == NULL) {
        vector_free(sub_pipes);
    }
    neu_taggrp_cfg_free(neu_trans_data->grp_config);
    neu_trans_buf_uninit(trans_buf);
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
    manager->state = MANAGER_STATE_READY;
    nng_cv_wake(manager->cv);
    nng_mtx_unlock(manager->mtx);

    // Wait the main process connect to manager
    nng_mtx_lock(manager->mtx);
    while (manager->state != MANAGER_STATE_RUNNING) {
        nng_cv_wait(manager->cv);
    }
    nng_mtx_unlock(manager->mtx);

    log_info("Register and start all static adapters");
    register_static_plugins(manager);
    reg_and_start_static_adapters(manager);

    nng_mtx_lock(manager->adapters_mtx);
    adapter_reg_entity_t *persist_reg_entity = find_reg_adapter_by_name(
        &manager->reg_adapters, DEFAULT_PERSIST_ADAPTER_NAME);
    nng_mtx_unlock(manager->adapters_mtx);
    nng_pipe persist_pipe = persist_reg_entity->adapter_pipe;

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
            if (0 ==
                strncmp(DEFAULT_PERSIST_ADAPTER_NAME, buf_ptr,
                        msg_get_buf_len(pay_msg))) {
                msg_size           = msg_inplace_data_get_size(0);
                rv                 = nng_msg_alloc(&out_msg, msg_size);
                message_t *msg_ptr = (message_t *) nng_msg_body(out_msg);
                msg_inplace_data_init(msg_ptr, MSG_CMD_PERSISTENCE_LOAD, 0);
                nng_msg_set_pipe(out_msg, msg_pipe);
                persist_pipe = msg_pipe;
                log_info("Send persistence load to pipe: %d", msg_pipe);
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
            neu_adapter_id_t      adapter_id;
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
            neu_adapter_id_t      adapter_id;
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
            neu_adapter_id_t      adapter_id;
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
            neu_adapter_id_t      adapter_id;
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

        case MSG_EVENT_ADD_NODE:
            // fall through

        case MSG_EVENT_UPDATE_NODE:
            // fall through

        case MSG_EVENT_DEL_NODE:
            // fall through

        case MSG_EVENT_SET_NODE_SETTING:

        case MSG_EVENT_ADD_GRP_CONFIG:
            // fall through

        case MSG_EVENT_UPDATE_GRP_CONFIG:
            // fall through

        case MSG_EVENT_DEL_GRP_CONFIG:
            // fall through

        case MSG_EVENT_ADD_TAGS:
            // fall through

        case MSG_EVENT_DEL_TAGS:
            // fall through

        case MSG_EVENT_UPDATE_TAGS:
            // fall through

        case MSG_EVENT_ADD_PLUGIN:
            // fall through

        case MSG_EVENT_UPDATE_PLUGIN:
            // fall through

        case MSG_EVENT_DEL_PLUGIN: {
            msg_type_e msg_type = msg_get_type(pay_msg);
            void *     buf      = msg_get_buf_ptr(pay_msg);
            size_t     size     = msg_get_buf_len(pay_msg);
            rv = manager_send_msg_to_pipe(manager, persist_pipe, msg_type, buf,
                                          size);
            if (0 == rv) {
                log_info("Forward event %d to %s pipe: %d", msg_type,
                         DEFAULT_PERSIST_ADAPTER_NAME, persist_pipe);
            }
            break;
        }

        case MSG_EVENT_UPDATE_LICENSE: {
            // broadcast license update event
            adapter_reg_entity_t *reg_entity = NULL;
            nng_pipe              src_pipe   = nng_msg_get_pipe(msg);
            VECTOR_FOR_EACH(&manager->reg_adapters, iter)
            {
                reg_entity        = iterator_get(&iter);
                nng_pipe dst_pipe = reg_entity->adapter_pipe;

                if (src_pipe.id == dst_pipe.id) {
                    // filter out the originator
                    continue;
                }
                rv = manager_send_msg_to_pipe(
                    manager, dst_pipe, MSG_EVENT_UPDATE_LICENSE, NULL, 0);
                if (0 == rv) {
                    log_info("Forward license update event to pipe: %d",
                             dst_pipe);
                } else {
                    log_error("Fail send license update event to pipe: %d",
                              dst_pipe);
                }
            }
            break;
        }

        case MSG_CMD_SUBSCRIBE_NODE: {
            subscribe_node_cmd_t *cmd_ptr = msg_get_buf_ptr(pay_msg);
            rv = neu_manager_subscribe_node(manager, cmd_ptr);
            if (0 != rv) {
                break;
            }

            neu_node_id_t node_id  = cmd_ptr->dst_node_id;
            msg_type_e    msg_type = MSG_EVENT_SUBSCRIBE_NODE;
            rv = manager_send_msg_to_pipe(manager, persist_pipe, msg_type,
                                          &node_id, sizeof(node_id));
            if (0 == rv) {
                log_info("Forward event %d to %s pipe: %d", msg_type,
                         DEFAULT_PERSIST_ADAPTER_NAME, persist_pipe);
            }
            break;
        }

        case MSG_CMD_UNSUBSCRIBE_NODE: {
            unsubscribe_node_cmd_t *cmd_ptr = msg_get_buf_ptr(pay_msg);
            rv = neu_manager_unsubscribe_node(manager, cmd_ptr);
            if (0 != rv) {
                break;
            }

            neu_node_id_t node_id  = cmd_ptr->dst_node_id;
            msg_type_e    msg_type = MSG_EVENT_UNSUBSCRIBE_NODE;
            rv = manager_send_msg_to_pipe(manager, persist_pipe, msg_type,
                                          &node_id, sizeof(node_id));
            if (0 == rv) {
                log_info("Forward event %d to %s pipe: %d", msg_type,
                         DEFAULT_PERSIST_ADAPTER_NAME, persist_pipe);
            }
            break;
        }

        case MSG_EVENT_GROUP_CONFIG_CHANGED: {
            size_t           msg_size;
            nng_msg *        out_msg;
            nng_pipe         msg_pipe;
            neu_adapter_id_t adapter_id;

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

        case MSG_DATA_NEURON_TRANS_DATA: {
            neuron_trans_data_t *neu_trans_data;

            neu_trans_data = (neuron_trans_data_t *) msg_get_buf_ptr(pay_msg);
            rv = dispatch_databuf_to_adapters(manager, neu_trans_data);
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

    manager->state               = MANAGER_STATE_NULL;
    manager->stop                = false;
    manager->listen_url          = manager_url;
    manager->new_adapter_id      = 1;
    manager->is_adapters_freezed = false;

    int rv, rv1;
    rv  = nng_mtx_alloc(&manager->mtx);
    rv1 = nng_mtx_alloc(&manager->adapters_mtx);
    if (rv != 0 || rv1 != 0) {
        neu_panic("Can't allocate mutex for manager");
    }

    rv  = nng_cv_alloc(&manager->cv, manager->mtx);
    rv1 = nng_cv_alloc(&manager->adapters_cv, manager->adapters_mtx);
    if (rv != 0 || rv1 != 0) {
        neu_panic("Failed to initialize condition(cv) for manager"
                  "and vector of adapters");
    }

    rv = vector_init(&manager->reg_adapters, 8, sizeof(adapter_reg_entity_t));
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

    nng_cv_free(manager->adapters_cv);
    nng_cv_free(manager->cv);
    nng_mtx_free(manager->adapters_mtx);
    nng_mtx_free(manager->mtx);
    free(manager);
    return;
}

void neu_manager_wait_ready(neu_manager_t *manager)
{
    if (manager == NULL) {
        return;
    }

    nng_mtx_lock(manager->mtx);
    while (manager->state != MANAGER_STATE_READY) {
        nng_cv_wait(manager->cv);
    }
    nng_mtx_unlock(manager->mtx);
    return;
}

void neu_manager_trigger_running(neu_manager_t *manager)
{
    if (manager == NULL) {
        return;
    }

    nng_mtx_lock(manager->mtx);
    manager->state = MANAGER_STATE_RUNNING;
    nng_cv_wake(manager->cv);
    nng_mtx_unlock(manager->mtx);
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

    case NEU_NODE_TYPE_APP:
        adapter_type = ADAPTER_TYPE_APP;
        break;

    case NEU_NODE_TYPE_DRIVERX:
        adapter_type = ADAPTER_TYPE_DRIVERX;
        break;

    case NEU_NODE_TYPE_FUNCTIONAL:
        adapter_type = ADAPTER_TYPE_FUNCTIONAL;
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

    case ADAPTER_TYPE_DRIVERX:
        node_type = NEU_NODE_TYPE_DRIVERX;
        break;

    case ADAPTER_TYPE_APP:
        node_type = NEU_NODE_TYPE_APP;
        break;
    case ADAPTER_TYPE_FUNCTIONAL:
        node_type = NEU_NODE_TYPE_FUNCTIONAL;
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
    neu_adapter_id_t  adapter_id;
    neu_adapter_t *   adapter = NULL;
    adapter_reg_cmd_t reg_cmd;

    reg_cmd.adapter_type = adapter_type_from_node_type(cmd->node_type);
    reg_cmd.adapter_name = cmd->adapter_name;
    reg_cmd.plugin_name  = cmd->plugin_name;
    reg_cmd.plugin_id    = cmd->plugin_id;
    rv = manager_reg_adapter(manager, &reg_cmd, &adapter, &adapter_id);
    if (rv == NEU_ERR_SUCCESS) {
        if (NULL != p_node_id) {
            *p_node_id = neu_manager_adapter_id_to_node_id(manager, adapter_id);
        }
        rv = neu_manager_init_adapter(manager, adapter);
    }
    return rv;
}

// NOTE: Do NOT expose this function in header files, use for persistence only!
int neu_manager_update_node_id(neu_manager_t *manager, neu_node_id_t node_id,
                               neu_node_id_t new_node_id)
{
    int                   rv = 0;
    neu_adapter_id_t      adapter_id;
    neu_adapter_id_t      new_adapter_id;
    adapter_reg_entity_t *reg_entity;

    // declaration for neu_adapter_set_id since it is not exposed
    neu_adapter_id_t neu_adapter_set_id(neu_adapter_t * adapter,
                                        neu_adapter_id_t id);

    if (0 == new_node_id) {
        return NEU_ERR_EINVAL;
    }

    adapter_id     = neu_manager_adapter_id_from_node_id(manager, node_id);
    new_adapter_id = neu_manager_adapter_id_from_node_id(manager, new_node_id);

    nng_mtx_lock(manager->adapters_mtx);
    reg_entity = find_reg_adapter_by_id(&manager->reg_adapters, new_adapter_id);
    if (NULL != reg_entity) {
        rv = NEU_ERR_NODE_EXIST;
    } else {
        reg_entity = find_reg_adapter_by_id(&manager->reg_adapters, adapter_id);
        if (reg_entity != NULL) {
            reg_entity->adapter_id = new_adapter_id;
            neu_adapter_set_id(reg_entity->adapter, new_adapter_id);
            if (new_adapter_id >= manager->new_adapter_id) {
                // update new available adapter id
                manager->new_adapter_id = new_adapter_id + 1;
            }
        } else {
            rv = NEU_ERR_NODE_NOT_EXIST;
        }
    }
    nng_mtx_unlock(manager->adapters_mtx);

    return rv;
}

int neu_manager_del_node(neu_manager_t *manager, neu_node_id_t node_id)
{
    int                   rv = 0;
    neu_adapter_id_t      adapter_id;
    neu_adapter_t *       adapter = NULL;
    adapter_reg_entity_t *reg_entity;
    vector_t              group_config;

    vector_init(&group_config, 1, sizeof(neu_taggrp_config_t *));

    if (neu_manager_get_grp_configs(manager, node_id, &group_config) == 0) {
        VECTOR_FOR_EACH(&group_config, iter)
        {
            neu_taggrp_config_t *config =
                *(neu_taggrp_config_t **) iterator_get(&iter);
            if (neu_taggrp_cfg_get_subpipes(config)->size != 0) {
                rv = NEU_ERR_GRP_CONFIG_IN_USE;
                break;
            }
        }
    }
    vector_uninit(&group_config);

    if (rv != 0) {
        return rv;
    }

    adapter_id = neu_manager_adapter_id_from_node_id(manager, node_id);

    nng_mtx_lock(manager->adapters_mtx);
    reg_entity = find_reg_adapter_by_id(&manager->reg_adapters, adapter_id);
    if (reg_entity != NULL) {
        adapter = reg_entity->adapter;
    }
    nng_mtx_unlock(manager->adapters_mtx);

    neu_manager_uninit_adapter(manager, adapter);

    rv = manager_unreg_adapter(manager, adapter_id, true);
    return rv;
}

int neu_manager_subscribe_node(neu_manager_t *       manager,
                               subscribe_node_cmd_t *cmd)
{
    int rv = 0;

    nng_mtx_lock(manager->adapters_mtx);
    neu_adapter_id_t src_adapter_id =
        neu_manager_adapter_id_from_node_id(manager, cmd->src_node_id);
    neu_adapter_id_t sub_adapter_id =
        neu_manager_adapter_id_from_node_id(manager, cmd->dst_node_id);
    adapter_reg_entity_t *src_reg_entity =
        find_reg_adapter_by_id(&manager->reg_adapters, src_adapter_id);
    adapter_reg_entity_t *sub_reg_entity =
        find_reg_adapter_by_id(&manager->reg_adapters, sub_adapter_id);
    neu_adapter_add_sub_grp_config(sub_reg_entity->adapter, cmd->src_node_id,
                                   cmd->grp_config);
    nng_mtx_unlock(manager->adapters_mtx);

    int need_forward =
        sub_grp_config_with_pipe(cmd->grp_config, sub_reg_entity->adapter_pipe);
    if (0 == need_forward) {
        return rv;
    }

    size_t   msg_size = msg_inplace_data_get_size(sizeof(*cmd));
    nng_msg *out_msg  = NULL;
    rv                = nng_msg_alloc(&out_msg, msg_size);
    if (rv == 0) {
        message_t *           msg_ptr;
        subscribe_node_cmd_t *out_cmd_ptr;
        msg_ptr = (message_t *) nng_msg_body(out_msg);
        msg_inplace_data_init(msg_ptr, MSG_CMD_SUBSCRIBE_NODE,
                              sizeof(subscribe_node_cmd_t));
        out_cmd_ptr = msg_get_buf_ptr(msg_ptr);
        memcpy(out_cmd_ptr, cmd, sizeof(subscribe_node_cmd_t));
        nng_msg_set_pipe(out_msg, src_reg_entity->adapter_pipe);
        log_info("Forward subscribe driver command to driver pipe: %d",
                 src_reg_entity->adapter_pipe);
        nng_sendmsg(manager->bind_info.mng_sock, out_msg, 0);
    }

    return rv;
}

int neu_manager_unsubscribe_node(neu_manager_t *         manager,
                                 unsubscribe_node_cmd_t *cmd)
{
    int rv = 0;

    nng_mtx_lock(manager->adapters_mtx);
    neu_adapter_id_t src_adapter_id =
        neu_manager_adapter_id_from_node_id(manager, cmd->src_node_id);
    neu_adapter_id_t sub_adapter_id =
        neu_manager_adapter_id_from_node_id(manager, cmd->dst_node_id);
    adapter_reg_entity_t *src_reg_entity =
        find_reg_adapter_by_id(&manager->reg_adapters, src_adapter_id);
    adapter_reg_entity_t *sub_reg_entity =
        find_reg_adapter_by_id(&manager->reg_adapters, sub_adapter_id);
    neu_adapter_del_sub_grp_config(sub_reg_entity->adapter, cmd->src_node_id,
                                   cmd->grp_config);
    nng_mtx_unlock(manager->adapters_mtx);

    int need_forward = unsub_grp_config_with_pipe(cmd->grp_config,
                                                  sub_reg_entity->adapter_pipe);
    if (0 == need_forward) {
        return rv;
    }

    size_t msg_size = msg_inplace_data_get_size(sizeof(unsubscribe_node_cmd_t));
    nng_msg *out_msg = NULL;
    rv               = nng_msg_alloc(&out_msg, msg_size);
    if (rv == 0) {
        message_t *             msg_ptr;
        unsubscribe_node_cmd_t *out_cmd_ptr;
        msg_ptr = (message_t *) nng_msg_body(out_msg);
        msg_inplace_data_init(msg_ptr, MSG_CMD_UNSUBSCRIBE_NODE,
                              sizeof(unsubscribe_node_cmd_t));
        out_cmd_ptr = msg_get_buf_ptr(msg_ptr);
        memcpy(out_cmd_ptr, cmd, sizeof(unsubscribe_node_cmd_t));
        nng_msg_set_pipe(out_msg, src_reg_entity->adapter_pipe);
        log_info("Forward unsubscribe driver command to driver pipe: %d",
                 src_reg_entity->adapter_pipe);
        nng_sendmsg(manager->bind_info.mng_sock, out_msg, 0);
    }

    return rv;
}

static bool adapter_match_node_type(neu_adapter_t * adapter,
                                    neu_node_type_e node_type)
{
    switch (node_type) {
    case NEU_NODE_TYPE_WEBSERVER:
        return ADAPTER_TYPE_WEBSERVER == neu_adapter_get_type(adapter);
    case NEU_NODE_TYPE_DRIVER:
        return ADAPTER_TYPE_DRIVER == neu_adapter_get_type(adapter);
    case NEU_NODE_TYPE_MQTT:
        return ADAPTER_TYPE_MQTT == neu_adapter_get_type(adapter);
    case NEU_NODE_TYPE_DRIVERX:
        return ADAPTER_TYPE_DRIVERX == neu_adapter_get_type(adapter);
    case NEU_NODE_TYPE_APP:
        return ADAPTER_TYPE_APP == neu_adapter_get_type(adapter);
    case NEU_NODE_TYPE_FUNCTIONAL:
        return ADAPTER_TYPE_FUNCTIONAL == neu_adapter_get_type(adapter);
    default:
        return false;
    }
}

int neu_manager_get_nodes(neu_manager_t *manager, neu_node_type_e node_type,
                          vector_t *result_nodes)
{
    int                   rv = NEU_ERR_SUCCESS;
    adapter_reg_entity_t *reg_entity;

    if (manager == NULL || result_nodes == NULL) {
        log_error("get nodes with NULL manager or result_nodes");
        return NEU_ERR_EINTERNAL;
    }

    nng_mtx_lock(manager->adapters_mtx);
    VECTOR_FOR_EACH(&manager->reg_adapters, iter)
    {
        reg_entity = (adapter_reg_entity_t *) iterator_get(&iter);
        if (adapter_match_node_type(reg_entity->adapter, node_type)) {
            neu_node_info_t  node_info;
            neu_adapter_id_t adapter_id;
            const char *     adapter_name;

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

int neu_manager_get_node_name_by_id(neu_manager_t *manager,
                                    neu_node_id_t node_id, char **name)
{
    int                   rv = 0;
    neu_adapter_id_t      adapter_id;
    adapter_reg_entity_t *reg_entity;

    if (NULL == name) {
        return NEU_ERR_EINVAL;
    }

    adapter_id = neu_manager_adapter_id_from_node_id(manager, node_id);

    nng_mtx_lock(manager->adapters_mtx);
    reg_entity = find_reg_adapter_by_id(&manager->reg_adapters, adapter_id);
    if (reg_entity != NULL) {
        *name = strdup(neu_adapter_get_name(reg_entity->adapter));
        if (NULL == *name) {
            rv = NEU_ERR_ENOMEM;
        }
    } else {
        rv = NEU_ERR_NODE_NOT_EXIST;
    }
    nng_mtx_unlock(manager->adapters_mtx);

    return rv;
}

int neu_manager_get_node_id_by_name(neu_manager_t *manager, const char *name,
                                    neu_node_id_t *node_id_p)
{
    int                   rv = 0;
    adapter_reg_entity_t *reg_entity;

    if (NULL == manager || NULL == name || NULL == node_id_p) {
        return NEU_ERR_EINVAL;
    }

    nng_mtx_lock(manager->adapters_mtx);
    reg_entity = find_reg_adapter_by_name(&manager->reg_adapters, name);
    if (NULL != reg_entity) {
        *node_id_p = neu_adapter_get_id(reg_entity->adapter);
    } else {
        rv = NEU_ERR_NODE_NOT_EXIST;
    }
    nng_mtx_unlock(manager->adapters_mtx);

    return rv;
}

int neu_manager_add_grp_config(neu_manager_t *           manager,
                               neu_cmd_add_grp_config_t *cmd)
{
    int                   rv = NEU_ERR_SUCCESS;
    neu_adapter_id_t      adapter_id;
    adapter_reg_entity_t *reg_entity;

    if (manager == NULL || cmd == NULL) {
        log_error("Add group config with NULL manager or command");
        return NEU_ERR_EINTERNAL;
    }

    if (cmd->grp_config == NULL) {
        log_error("group config is NULL");
        return NEU_ERR_EINTERNAL;
    }

    nng_mtx_lock(manager->adapters_mtx);
    adapter_id = neu_manager_adapter_id_from_node_id(manager, cmd->node_id);
    reg_entity = find_reg_adapter_by_id(&manager->reg_adapters, adapter_id);
    if (reg_entity == NULL) {
        log_error("Can't find matched src registered adapter");
        rv = NEU_ERR_NODE_NOT_EXIST;
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
    int                   rv = NEU_ERR_SUCCESS;
    neu_adapter_id_t      adapter_id;
    adapter_reg_entity_t *reg_entity;

    if (manager == NULL || config_name == NULL) {
        log_error("Delete group config NULL manager or config_name");
        return NEU_ERR_EINTERNAL;
    }

    nng_mtx_lock(manager->adapters_mtx);
    adapter_id = neu_manager_adapter_id_from_node_id(manager, node_id);
    reg_entity = find_reg_adapter_by_id(&manager->reg_adapters, adapter_id);
    if (reg_entity == NULL) {
        log_error("Can't find matched registered adapter");
        rv = NEU_ERR_NODE_NOT_EXIST;
        goto del_grp_config_exit;
    }

    rv = neu_datatag_mng_del_grp_config(reg_entity->datatag_manager,
                                        config_name);
    if (rv != 0) {
        log_error("Failed to delete datatag group config: %s", config_name);
        rv = NEU_ERR_EINTERNAL;
    }
del_grp_config_exit:
    nng_mtx_unlock(manager->adapters_mtx);
    return rv;
}

int neu_manager_update_grp_config(neu_manager_t *              manager,
                                  neu_cmd_update_grp_config_t *cmd)
{
    int                   rv = NEU_ERR_SUCCESS;
    neu_adapter_id_t      adapter_id;
    adapter_reg_entity_t *reg_entity;

    if (manager == NULL || cmd == NULL) {
        log_error("Update group config with NULL manager or command");
        return NEU_ERR_EINTERNAL;
    }

    if (cmd->grp_config == NULL) {
        log_error("group config is NULL");
        return NEU_ERR_EINTERNAL;
    }

    nng_mtx_lock(manager->adapters_mtx);
    adapter_id = neu_manager_adapter_id_from_node_id(manager, cmd->node_id);
    reg_entity = find_reg_adapter_by_id(&manager->reg_adapters, adapter_id);
    if (reg_entity == NULL) {
        log_error("Can't find matched src or dst registered adapter");
        rv = NEU_ERR_NODE_NOT_EXIST;
        goto update_grp_config_exit;
    }

    rv = neu_datatag_mng_update_grp_config(reg_entity->datatag_manager,
                                           cmd->grp_config);
    if (rv != 0) {
        log_error("Failed to update datatag group config: %s",
                  neu_taggrp_cfg_get_name(cmd->grp_config));
        neu_taggrp_cfg_free(cmd->grp_config);
        rv = NEU_ERR_EINTERNAL;
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
    neu_adapter_id_t      adapter_id;

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

int neu_manager_get_persist_datatag_infos(neu_manager_t *manager,
                                          neu_node_id_t  node_id,
                                          const char *   grp_config_name,
                                          vector_t **    result)
{
    int                   rv = 0;
    adapter_reg_entity_t *reg_entity;
    neu_adapter_id_t      adapter_id;

    if (manager == NULL || result == NULL) {
        log_error("get persist datatag infos with NULL manager or result");
        return NEU_ERR_EINTERNAL;
    }

    nng_mtx_lock(manager->adapters_mtx);

    adapter_id = neu_manager_adapter_id_from_node_id(manager, node_id);
    reg_entity = find_reg_adapter_by_id(&manager->reg_adapters, adapter_id);
    if (NULL == reg_entity) {
        rv = NEU_ERR_NODE_NOT_EXIST;
        goto final;
    }

    neu_taggrp_config_t *grp_config = neu_datatag_mng_get_grp_config(
        reg_entity->datatag_manager, grp_config_name);
    if (NULL == grp_config) {
        rv = NEU_ERR_GRP_CONFIG_NOT_EXIST;
        goto final;
    }

    neu_datatag_table_t *tag_tbl =
        neu_datatag_mng_get_datatag_tbl(reg_entity->datatag_manager);

    vector_t *tag_ids = neu_taggrp_cfg_get_datatag_ids(grp_config);

    vector_t *datatag_infos =
        vector_new(tag_ids->size, sizeof(neu_persist_datatag_info_t));
    if (NULL == datatag_infos) {
        rv = NEU_ERR_ENOMEM;
        goto error_vector_new;
    }

    VECTOR_FOR_EACH(tag_ids, iter)
    {
        neu_datatag_id_t *id  = iterator_get(&iter);
        neu_datatag_t *   tag = neu_datatag_tbl_get(tag_tbl, *id);
        if (NULL == tag) {
            continue; // ignore not found tag
        }

        neu_persist_datatag_info_t datatag_info = {
            .type      = tag->type,
            .attribute = tag->attribute,
            .name      = strdup(tag->name),
            .address   = strdup(tag->addr_str),
            .id        = tag->id,

        };
        if (0 != vector_push_back(datatag_infos, &datatag_info)) {
            free(datatag_info.name);
            free(datatag_info.address);
            rv = NEU_ERR_ENOMEM;
            break;
        }
        if (NULL == datatag_info.name || NULL == datatag_info.address) {
            rv = NEU_ERR_ENOMEM;
            break;
        }
    }

    if (0 == rv) {
        *result = datatag_infos;
    } else {
        neu_persist_datatag_infos_free(datatag_infos);
    }

error_vector_new:
    neu_taggrp_cfg_free(grp_config);
final:
    nng_mtx_unlock(manager->adapters_mtx); // NOTE: big lock granularity
    return rv;
}

int neu_manager_add_plugin_lib(neu_manager_t *           manager,
                               neu_cmd_add_plugin_lib_t *cmd,
                               plugin_id_t *             p_plugin_id)
{
    int                  rv = 0;
    plugin_reg_param_t   reg_param;
    void *               handle      = NULL;
    neu_plugin_module_t *module_info = NULL;

    handle = load_plugin_library((char *) cmd->plugin_lib_name,
                                 PLUGIN_KIND_SYSTEM, &module_info);
    if (handle == NULL) {
        return NEU_ERR_LIBRARY_NOT_FOUND;
    }

    reg_param.plugin_kind     = module_info->kind;
    reg_param.adapter_type    = adapter_type_from_node_type(module_info->type);
    reg_param.plugin_lib_name = (char *) cmd->plugin_lib_name;
    reg_param.plugin_name     = module_info->module_name;

    unload_plugin_library(handle, PLUGIN_KIND_SYSTEM);

    if (reg_param.plugin_kind != PLUGIN_KIND_STATIC &&
        reg_param.plugin_kind != PLUGIN_KIND_SYSTEM &&
        reg_param.plugin_kind != PLUGIN_KIND_CUSTOM) {
        return NEU_ERR_LIBRARY_INFO_INVALID;
    }

    if (reg_param.adapter_type <= ADAPTER_TYPE_UNKNOW ||
        reg_param.adapter_type >= ADAPTER_TYPE_MAX) {
        return NEU_ERR_LIBRARY_INFO_INVALID;
    }

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

int neu_manager_get_persist_plugin_infos(neu_manager_t *manager,
                                         vector_t **    result)
{
    if (NULL == manager || NULL == result) {
        log_error("get persist plugin infos with NULL manager or result");
        return NEU_ERR_EINVAL;
    }

    vector_t *plugin_regs =
        plugin_manager_get_all_plugins(manager->plugin_manager);
    if (plugin_regs == NULL) {
        return NEU_ERR_ENOMEM;
    }

    size_t count = plugin_regs->size;
    if (count >= STATIC_PLUGIN_INFO_SIZE) {
        count -= STATIC_PLUGIN_INFO_SIZE;
    }
    vector_t *plugin_infos = vector_new(count, sizeof(char *));
    if (NULL == plugin_infos) {
        vector_free(plugin_regs);
        return NEU_ERR_ENOMEM;
    }

    char *             plugin_info     = NULL;
    plugin_reg_info_t *plugin_reg_info = NULL;
    VECTOR_FOR_EACH(plugin_regs, iter)
    {
        plugin_reg_info = (plugin_reg_info_t *) iterator_get(&iter);
        if (is_static_plugin(plugin_reg_info->plugin_name)) {
            continue;
        }
        plugin_info = strdup(plugin_reg_info->plugin_lib_name);
        if (0 != vector_push_back(plugin_infos, &plugin_info)) {
            neu_persist_plugin_infos_free(plugin_infos);
            vector_free(plugin_regs);
            return NEU_ERR_ENOMEM;
        }
    }

    *result = plugin_infos;
    vector_free(plugin_regs);
    return 0;
}

neu_datatag_table_t *neu_manager_get_datatag_tbl(neu_manager_t *manager,
                                                 neu_node_id_t  node_id)
{
    adapter_reg_entity_t *reg_entity;
    neu_datatag_table_t * tag_table;
    neu_adapter_id_t      adapter_id;

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

int neu_manager_adapter_set_setting(neu_manager_t *manager,
                                    neu_node_id_t node_id, const char *config)
{
    neu_adapter_id_t      adapter_id = { 0 };
    adapter_reg_entity_t *reg_entity = NULL;
    int                   ret        = 0;
    neu_config_t          neu_config = { .type = NEU_CONFIG_SETTING };

    nng_mtx_lock(manager->adapters_mtx);

    adapter_id = neu_manager_adapter_id_from_node_id(manager, node_id);
    reg_entity = find_reg_adapter_by_id(&manager->reg_adapters, adapter_id);

    if (reg_entity == NULL) {
        log_error("Can't find matched src registered adapter");
        nng_mtx_unlock(manager->adapters_mtx);
        return NEU_ERR_NODE_NOT_EXIST;
    }

    neu_config.buf_len = strlen(config);
    neu_config.buf     = strdup(config);

    ret = neu_adapter_set_setting(reg_entity->adapter, &neu_config);
    nng_mtx_unlock(manager->adapters_mtx);

    free(neu_config.buf);

    return ret;
}

const char *find_adapter_plugin_name(neu_manager_t *manager,
                                     neu_adapter_t *adapter)
{
    plugin_id_t plugin_id = neu_adapter_get_plugin_id(adapter);

    plugin_reg_info_t reg_info = {};
    plugin_manager_get_reg_info(manager->plugin_manager, plugin_id, &reg_info);

    return reg_info.plugin_name;
}

int neu_manager_get_persist_adapter_infos(neu_manager_t *manager,
                                          vector_t **    result)
{
    adapter_reg_entity_t *reg_entity    = NULL;
    vector_t *            adapter_infos = NULL;

    if (NULL == manager || NULL == result) {
        log_error("get persist adapter infos with NULL manager or NULL result");
        return NEU_ERR_EINVAL;
    }

    size_t count = manager->reg_adapters.size;
    if (count >= STATIC_ADAPTER_ADD_INFO_SIZE) {
        count -= STATIC_ADAPTER_ADD_INFO_SIZE;
    }
    adapter_infos = vector_new(count, sizeof(neu_persist_adapter_info_t));
    if (NULL == adapter_infos) {
        return NEU_ERR_ENOMEM;
    }

    nng_mtx_lock(manager->adapters_mtx);
    VECTOR_FOR_EACH(&manager->reg_adapters, iter)
    {
        reg_entity               = (adapter_reg_entity_t *) iterator_get(&iter);
        const char *adapter_name = neu_adapter_get_name(reg_entity->adapter);
        const char *plugin_name =
            find_adapter_plugin_name(manager, reg_entity->adapter);

        if (is_static_adapter(adapter_name)) {
            continue;
        }

        neu_persist_adapter_info_t adapter_info = {};
        adapter_info.id =
            neu_manager_adapter_id_to_node_id(manager, reg_entity->adapter_id);
        adapter_info.type  = neu_adapter_get_type(reg_entity->adapter);
        adapter_info.state = neu_adapter_get_state(reg_entity->adapter).running;
        adapter_info.name  = strdup(adapter_name);
        adapter_info.plugin_name = strdup(plugin_name);

        if (0 != vector_push_back(adapter_infos, &adapter_info)) {
            neu_persist_adapter_infos_free(adapter_infos);
            nng_mtx_unlock(manager->adapters_mtx);
            return NEU_ERR_ENOMEM;
        }
    }
    nng_mtx_unlock(manager->adapters_mtx);

    *result = adapter_infos;

    return NEU_ERR_SUCCESS;
}

int neu_manager_adapter_get_setting(neu_manager_t *manager,
                                    neu_node_id_t node_id, char **config)
{
    neu_adapter_id_t      adapter_id = { 0 };
    adapter_reg_entity_t *reg_entity = NULL;
    int                   ret        = 0;

    nng_mtx_lock(manager->adapters_mtx);

    adapter_id = neu_manager_adapter_id_from_node_id(manager, node_id);
    reg_entity = find_reg_adapter_by_id(&manager->reg_adapters, adapter_id);

    if (reg_entity == NULL) {
        log_error("Can't find matched src registered adapter");
        nng_mtx_unlock(manager->adapters_mtx);
        return NEU_ERR_NODE_NOT_EXIST;
    }

    ret = neu_adapter_get_setting(reg_entity->adapter, config);
    nng_mtx_unlock(manager->adapters_mtx);

    return ret;
}

int neu_manager_start_adapter_with_id(neu_manager_t *manager,
                                      neu_node_id_t  node_id)
{
    neu_adapter_id_t      adapter_id = { 0 };
    adapter_reg_entity_t *reg_entity = NULL;

    nng_mtx_lock(manager->adapters_mtx);

    adapter_id = neu_manager_adapter_id_from_node_id(manager, node_id);
    reg_entity = find_reg_adapter_by_id(&manager->reg_adapters, adapter_id);

    if (reg_entity == NULL) {
        log_error("Can't find matched src registered adapter");
        nng_mtx_unlock(manager->adapters_mtx);
        return NEU_ERR_NODE_NOT_EXIST;
    }

    // TODO: this should better be outside of lock
    neu_adapter_start(reg_entity->adapter);

    nng_mtx_unlock(manager->adapters_mtx);

    return NEU_ERR_SUCCESS;
}

int neu_manager_adapter_get_state(neu_manager_t *manager, neu_node_id_t node_id,
                                  neu_plugin_state_t *state)
{
    neu_adapter_id_t      adapter_id = { 0 };
    adapter_reg_entity_t *reg_entity = NULL;

    nng_mtx_lock(manager->adapters_mtx);

    adapter_id = neu_manager_adapter_id_from_node_id(manager, node_id);
    reg_entity = find_reg_adapter_by_id(&manager->reg_adapters, adapter_id);

    if (reg_entity == NULL) {
        log_error("Can't find matched src registered adapter");
        nng_mtx_unlock(manager->adapters_mtx);
        return NEU_ERR_NODE_NOT_EXIST;
    }

    *state = neu_adapter_get_state(reg_entity->adapter);
    nng_mtx_unlock(manager->adapters_mtx);

    return NEU_ERR_SUCCESS;
}

int neu_manager_adapter_ctl(neu_manager_t *manager, neu_node_id_t node_id,
                            neu_adapter_ctl_e ctl)
{
    neu_adapter_id_t      adapter_id = { 0 };
    adapter_reg_entity_t *reg_entity = NULL;
    neu_adapter_t *       adapter;
    int                   ret = NEU_ERR_SUCCESS;

    nng_mtx_lock(manager->adapters_mtx);

    adapter_id = neu_manager_adapter_id_from_node_id(manager, node_id);
    reg_entity = find_reg_adapter_by_id(&manager->reg_adapters, adapter_id);

    if (reg_entity == NULL) {
        nng_mtx_unlock(manager->adapters_mtx);
        log_error("Can't find matched src registered adapter");
        return NEU_ERR_NODE_NOT_EXIST;
    }

    adapter = reg_entity->adapter;
    nng_mtx_unlock(manager->adapters_mtx);

    switch (ctl) {
    case NEU_ADAPTER_CTL_START:
        ret = neu_manager_start_adapter(adapter);
        break;
    case NEU_ADAPTER_CTL_STOP:
        ret = neu_manager_stop_adapter(adapter);
        break;
    }

    return ret;
}

int neu_manager_adapter_get_sub_grp_configs(neu_manager_t *manager,
                                            neu_node_id_t  node_id,
                                            vector_t **    result_sgc)
{
    neu_adapter_id_t      adapter_id = { 0 };
    adapter_reg_entity_t *reg_entity = NULL;

    nng_mtx_lock(manager->adapters_mtx);

    adapter_id = neu_manager_adapter_id_from_node_id(manager, node_id);
    reg_entity = find_reg_adapter_by_id(&manager->reg_adapters, adapter_id);

    if (reg_entity == NULL) {
        log_error("Can't find matched src registered adapter");
        nng_mtx_unlock(manager->adapters_mtx);
        return -1;
    }

    *result_sgc = neu_adapter_get_sub_grp_configs(reg_entity->adapter);
    nng_mtx_unlock(manager->adapters_mtx);

    return 0;
}

int neu_manager_adapter_get_grp_config_ref_by_name(
    neu_manager_t *manager, neu_node_id_t node_id, const char *grp_config_name,
    neu_taggrp_config_t **p_grp_config)
{
    int rv = 0;

    if (NULL == manager || NULL == grp_config_name || NULL == p_grp_config) {
        log_error(
            "get nodes with NULL manager, grp_config_name or p_grp_config");
        return NEU_ERR_EINVAL;
    }

    nng_mtx_lock(manager->adapters_mtx);
    neu_adapter_id_t adapter_id =
        neu_manager_adapter_id_from_node_id(manager, node_id);
    adapter_reg_entity_t *reg_entity =
        find_reg_adapter_by_id(&manager->reg_adapters, adapter_id);
    if (NULL == reg_entity) {
        nng_mtx_unlock(manager->adapters_mtx);
        return NEU_ERR_NODE_NOT_EXIST;
    }

    neu_taggrp_config_t *grp_config =
        (neu_taggrp_config_t *) neu_datatag_mng_ref_grp_config(
            reg_entity->datatag_manager, grp_config_name);

    if (NULL != grp_config) {
        *p_grp_config = grp_config;
    } else {
        rv = NEU_ERR_GRP_CONFIG_NOT_EXIST;
    }

    nng_mtx_unlock(manager->adapters_mtx);
    return rv;
}

int neu_manager_get_persist_subscription_infos(neu_manager_t *manager,
                                               neu_node_id_t  node_id,
                                               vector_t **    result)
{
    int rv = 0;

    if (NULL == manager || NULL == result) {
        log_error("get persist subscription infos with NULL manager or result");
        return NEU_ERR_EINTERNAL;
    }

    neu_adapter_id_t      adapter_id = { 0 };
    adapter_reg_entity_t *reg_entity = NULL;

    nng_mtx_lock(manager->adapters_mtx);

    adapter_id = neu_manager_adapter_id_from_node_id(manager, node_id);
    reg_entity = find_reg_adapter_by_id(&manager->reg_adapters, adapter_id);

    if (reg_entity == NULL) {
        rv = NEU_ERR_NODE_NOT_EXIST;
        goto final;
    }

    vector_t *sub_grp_configs =
        neu_adapter_get_sub_grp_configs(reg_entity->adapter);
    if (NULL == sub_grp_configs) {
        rv = NEU_ERR_ENOMEM;
        goto final;
    }

    vector_t *sub_infos = vector_new(sub_grp_configs->capacity,
                                     sizeof(neu_persist_subscription_info_t));
    if (NULL == sub_infos) {
        rv = NEU_ERR_ENOMEM;
        goto error_new_vec;
    }

    VECTOR_FOR_EACH(sub_grp_configs, iter)
    {
        neu_sub_grp_config_t *sgc = iterator_get(&iter);
        neu_adapter_id_t      adapter_id =
            neu_manager_adapter_id_from_node_id(manager, sgc->node_id);
        adapter_reg_entity_t *src_reg_entity =
            find_reg_adapter_by_id(&manager->reg_adapters, adapter_id);
        if (NULL == src_reg_entity) {
            rv = NEU_ERR_NODE_NOT_EXIST;
            break;
        }

        char *group_config_name = strdup(sgc->group_config_name);
        char *src_adapter_name =
            strdup(neu_adapter_get_name(src_reg_entity->adapter));
        char *sub_adapter_name =
            strdup(neu_adapter_get_name(reg_entity->adapter));

        neu_persist_subscription_info_t sub_info = {
            .group_config_name = group_config_name,
            .src_adapter_name  = src_adapter_name,
            .sub_adapter_name  = sub_adapter_name, // TODO: we don't need these
            .read_interval     = 0,
        };

        if (0 != vector_push_back(sub_infos, &sub_info)) {
            free(group_config_name);
            free(src_adapter_name);
            free(sub_adapter_name);
            rv = NEU_ERR_ENOMEM;
            break;
        }
        if (NULL == group_config_name || NULL == src_adapter_name ||
            NULL == sub_adapter_name) {
            rv = NEU_ERR_ENOMEM;
            break;
        }
    }

    if (0 == rv) {
        *result = sub_infos;
    } else {
        neu_persist_subscription_infos_free(sub_infos);
    }

error_new_vec:
    vector_free(sub_grp_configs);
final:
    nng_mtx_unlock(manager->adapters_mtx); // TODO: reduce locking granularity

    return rv;
}

int neu_manager_adapter_validate_tag(neu_manager_t *manager,
                                     neu_node_id_t node_id, neu_datatag_t *tags)
{
    neu_adapter_id_t      adapter_id = { 0 };
    adapter_reg_entity_t *reg_entity = NULL;
    neu_adapter_t *       adapter;
    int                   ret = -1;

    nng_mtx_lock(manager->adapters_mtx);

    adapter_id = neu_manager_adapter_id_from_node_id(manager, node_id);
    reg_entity = find_reg_adapter_by_id(&manager->reg_adapters, adapter_id);

    if (reg_entity == NULL) {
        nng_mtx_unlock(manager->adapters_mtx);
        log_error("Can't find matched src registered adapter");
        return -1;
    }

    adapter = reg_entity->adapter;
    nng_mtx_unlock(manager->adapters_mtx);

    ret = neu_adapter_validate_tag(adapter, tags);

    return ret;
}
