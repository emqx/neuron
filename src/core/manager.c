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
#include <stdlib.h>

#include <nng/nng.h>
#include <nng/protocol/pair1/pair.h>
#include <nng/supplemental/util/platform.h>

#include "event/event.h"
#include "persist/persist.h"
#include "utils/log.h"

#include "adapter.h"
#include "adapter/driver/driver_internal.h"

#include "message.h"
#include "node_manager.h"
#include "pluginx_manager.h"
#include "subscribex.h"

#include "manager.h"

struct neu_manager {
    nng_mtx *       mtx;
    nng_socket      socket;
    neu_events_t *  events;
    neu_event_io_t *loop;

    neu_plugin_manager_t *plugin_manager;
    neu_node_manager_t *  node_manager;
    neu_subscribe_mgr_t * subscribe_manager;

    nng_pipe persist_pipe;
};

// definition for adapter names
#define DEFAULT_DASHBOARD_ADAPTER_NAME DEFAULT_DASHBOARD_PLUGIN_NAME
#define DEFAULT_PERSIST_ADAPTER_NAME DEFAULT_DUMMY_PLUGIN_NAME

static const char *const url = "inproc://neu_manager";

static int manager_loop(enum neu_event_io_type type, int fd, void *usr_data);
inline static void forward_msg(neu_manager_t *manager, nng_msg *msg,
                               nng_pipe pipe);
static void start_static_adapter(neu_manager_t *manager, const char *name);
static void stop_static_adapter(neu_manager_t *manager, const char *name);

neu_manager_t *neu_manager_create()
{
    int                  rv      = 0;
    neu_manager_t *      manager = calloc(1, sizeof(neu_manager_t));
    neu_event_io_param_t param   = {
        .usr_data = (void *) manager,
        .cb       = manager_loop,
    };

    manager->events            = neu_event_new();
    manager->plugin_manager    = neu_plugin_manager_create();
    manager->node_manager      = neu_node_manager_create();
    manager->subscribe_manager = neu_subscribe_manager_create();

    nng_mtx_alloc(&manager->mtx);
    nng_pair1_open_poly(&manager->socket);

    rv = nng_listen(manager->socket, url, NULL, 0);
    assert(rv == 0);

    nng_socket_get_int(manager->socket, NNG_OPT_RECVFD, &param.fd);
    manager->loop = neu_event_add_io(manager->events, param);

    // load static
    // load persistence
    start_static_adapter(manager, DEFAULT_DASHBOARD_PLUGIN_NAME);
    start_static_adapter(manager, DEFAULT_DUMMY_PLUGIN_NAME);

    return manager;
}

void neu_manager_destroy(neu_manager_t *manager)
{
    stop_static_adapter(manager, DEFAULT_DASHBOARD_PLUGIN_NAME);
    stop_static_adapter(manager, DEFAULT_DUMMY_PLUGIN_NAME);

    nng_close(manager->socket);
    neu_event_del_io(manager->events, manager->loop);
    neu_event_close(manager->events);

    neu_subscribe_manager_destroy(manager->subscribe_manager);
    neu_node_manager_destroy(manager->node_manager);
    neu_plugin_manager_destroy(manager->plugin_manager);
    nng_mtx_free(manager->mtx);
    free(manager);
    nlog_warn("manager exit");
}

const char *neu_manager_get_url(neu_manager_t *manager)
{
    (void) manager;
    return url;
}

int neu_manager_add_node(neu_manager_t *manager, const char *node_name,
                         const char *plugin_name)
{
    neu_adapter_t *       adapter      = NULL;
    neu_plugin_instance_t instance     = { 0 };
    neu_adapter_info_t    adapter_info = {
        .name = node_name,
    };
    neu_plugin_lib_info_t info = { 0 };
    int                   ret =
        neu_plugin_manager_find(manager->plugin_manager, plugin_name, &info);

    if (ret != 0) {
        return NEU_ERR_LIBRARY_NOT_FOUND;
    }

    adapter = neu_node_manager_find(manager->node_manager, node_name);
    if (adapter != NULL) {
        return NEU_ERR_NODE_EXIST;
    }

    ret = neu_plugin_manager_create_instance(manager->plugin_manager,
                                             plugin_name, &instance);
    adapter_info.handle = instance.handle;
    adapter_info.module = instance.module;

    adapter     = neu_adapter_create(&adapter_info, manager);
    adapter->id = neu_node_manager_add(manager->node_manager, adapter);
    neu_adapter_init(adapter);

    return NEU_ERR_SUCCESS;
}

int neu_manager_del_node(neu_manager_t *manager, const char *node_name)
{
    neu_adapter_t *adapter =
        neu_node_manager_find(manager->node_manager, node_name);

    if (adapter == NULL) {
        return NEU_ERR_NODE_NOT_EXIST;
    }

    neu_adapter_uninit(adapter);
    neu_adapter_destroy(adapter);

    neu_subscribe_manager_remove(manager->subscribe_manager, node_name, NULL);
    neu_node_manager_del(manager->node_manager, node_name);
    return NEU_ERR_SUCCESS;
}

UT_array *neu_manager_get_nodes(neu_manager_t *manager, neu_node_type_e type)
{
    return neu_node_manager_get(manager->node_manager, type);
}

int neu_manager_subscribe(neu_manager_t *manager, const char *app,
                          const char *driver, const char *group)
{
    (void) manager;
    (void) app;
    (void) driver;
    (void) group;

    return 0;
}

int neu_manager_unsubscribe(neu_manager_t *manager, const char *app,
                            const char *driver, const char *group)
{
    (void) manager;
    (void) app;
    (void) driver;
    (void) group;

    return 0;
}

int neu_manager_add_group(neu_manager_t *manager, const char *driver,
                          const char *group, uint32_t interval)
{
    if (interval < NEU_GROUP_INTERVAL_LIMIT) {
        return NEU_ERR_GROUP_PARAMETER_INVALID;
    }

    if (strlen(group) > NEU_GROUP_NAME_LEN) {
        return NEU_ERR_GROUP_PARAMETER_INVALID;
    }

    neu_adapter_t *adapter =
        neu_node_manager_find(manager->node_manager, driver);
    if (adapter == NULL) {
        return NEU_ERR_NODE_NOT_EXIST;
    }

    if (adapter->plugin_info.module->type != NEU_NA_TYPE_DRIVER) {
        return NEU_ERR_GROUP_NOT_ALLOW;
    }

    return neu_adapter_driver_add_group((neu_adapter_driver_t *) adapter, group,
                                        interval);
}

int neu_manager_del_group(neu_manager_t *manager, const char *driver,
                          const char *group)
{
    neu_adapter_t *adapter =
        neu_node_manager_find(manager->node_manager, driver);
    if (adapter == NULL) {
        return NEU_ERR_NODE_NOT_EXIST;
    }

    if (adapter->plugin_info.module->type != NEU_NA_TYPE_DRIVER) {
        return NEU_ERR_GROUP_NOT_ALLOW;
    }

    neu_subscribe_manager_remove(manager->subscribe_manager, driver, group);
    return neu_adapter_driver_del_group((neu_adapter_driver_t *) adapter,
                                        group);
}

int neu_manager_update_group(neu_manager_t *manager, const char *driver,
                             const char *group, uint32_t interval)
{
    if (interval < NEU_GROUP_INTERVAL_LIMIT) {
        return NEU_ERR_GROUP_PARAMETER_INVALID;
    }
    (void) manager;
    (void) driver;
    (void) group;
    (void) interval;

    return 0;
}

int neu_manager_get_group(neu_manager_t *manager, const char *driver,
                          UT_array **groups)
{
    neu_adapter_t *adapter =
        neu_node_manager_find(manager->node_manager, driver);

    if (adapter == NULL) {
        return NEU_ERR_NODE_NOT_EXIST;
    }

    if (adapter->plugin_info.module->type != NEU_NA_TYPE_DRIVER) {
        return NEU_ERR_GROUP_NOT_ALLOW;
    }

    *groups = neu_adapter_driver_get_group((neu_adapter_driver_t *) adapter);

    return NEU_ERR_SUCCESS;
}

int neu_manager_add_plugin(neu_manager_t *manager, const char *plugin_library)
{
    return neu_plugin_manager_add(manager->plugin_manager, plugin_library);
}

int neu_manager_del_plugin(neu_manager_t *manager, const char *plugin)
{
    neu_plugin_manager_del(manager->plugin_manager, plugin);
    return 0;
}

UT_array *neu_manager_get_plugins(neu_manager_t *manager)
{
    return neu_plugin_manager_get(manager->plugin_manager);
}

int neu_manager_start_node(neu_manager_t *manager, const char *node)
{
    (void) manager;
    (void) node;

    return 0;
}

int neu_manager_stop_node(neu_manager_t *manager, const char *node)
{
    (void) manager;
    (void) node;

    return 0;
}

int neu_manager_get_adapter_info(neu_manager_t *manager, const char *name,
                                 neu_persist_adapter_info_t *info)
{
    neu_adapter_t *adapter = neu_node_manager_find(manager->node_manager, name);

    if (adapter != NULL) {
        info->name        = strdup(name);
        info->type        = adapter->plugin_info.module->type;
        info->plugin_name = strdup(adapter->plugin_info.module->module_name);
        info->state       = adapter->state;
        return 0;
    }

    return -1;
}

UT_array *neu_manager_get_plugin_info(neu_manager_t *manager)
{
    UT_array *plugins_info = NULL;

    utarray_new(plugins_info, &ut_ptr_icd);
    UT_array *plugins = neu_plugin_manager_get(manager->plugin_manager);

    utarray_foreach(plugins, neu_plugin_lib_info_t *, p)
    {
        char *library = strdup(p->lib_name);
        utarray_push_back(plugins_info, &library);
    }
    utarray_free(plugins);

    return plugins_info;
}

static int manager_loop(enum neu_event_io_type type, int fd, void *usr_data)
{
    neu_manager_t *manager = (neu_manager_t *) usr_data;
    int            rv      = 0;
    nng_msg *      msg     = NULL;
    message_t *    pay_msg = NULL;
    void *         msg_ptr = NULL;

    if (type == NEU_EVENT_IO_CLOSED || type == NEU_EVENT_IO_HUP) {
        nlog_warn("manager socket(%d) recv closed or hup %d.", fd, type);
        return 0;
    }

    rv = nng_recvmsg(manager->socket, &msg, NNG_FLAG_NONBLOCK);
    if (rv != 0) {
        nlog_warn("manager recv msg error: %d", rv);
        return 0;
    }

    pay_msg = nng_msg_body(msg);
    msg_ptr = msg_get_buf_ptr(pay_msg);
    switch (msg_get_type(pay_msg)) {
    case MSG_EVENT_NODE_PING: {
        char *   node_name = (char *) msg_ptr;
        nng_pipe pipe      = nng_msg_get_pipe(msg);

        neu_node_manager_update(manager->node_manager, node_name, pipe);
        nlog_notice("bind node %s to pipe(%d)", node_name, pipe.id);
        if (strcmp(DEFAULT_DUMMY_PLUGIN_NAME, node_name) == 0) {
            nng_msg *out_msg  = NULL;
            size_t   msg_size = msg_inplace_data_get_size(0);

            nng_msg_alloc(&out_msg, msg_size);
            msg_ptr = (message_t *) nng_msg_body(out_msg);
            msg_inplace_data_init(msg_ptr, MSG_CMD_PERSISTENCE_LOAD, 0);

            manager->persist_pipe = pipe;
            nng_msg_set_pipe(out_msg, manager->persist_pipe);

            nng_sendmsg(manager->socket, out_msg, 0);
            nlog_info("send persistence load to pipe: %d", pipe.id);
        }
        break;
    }
    case MSG_CMD_READ_DATA: {
        read_data_cmd_t *read_cmd = (read_data_cmd_t *) msg_ptr;
        nng_pipe         dst_pipe = neu_node_manager_get_pipe_by_id(
            manager->node_manager, read_cmd->dst_node_id);

        forward_msg(manager, msg, dst_pipe);

        nlog_info("forward read request to driver pipe: %d", dst_pipe.id);
        break;
    }
    case MSG_DATA_READ_RESP: {
        read_data_resp_t *read_res = (read_data_resp_t *) msg_ptr;
        nng_pipe          dst_pipe = neu_node_manager_get_pipe_by_id(
            manager->node_manager, read_res->recver_id);

        forward_msg(manager, msg, dst_pipe);

        nlog_info("forward read response to driver pipe: %d", dst_pipe.id);
        break;
    }
    case MSG_CMD_WRITE_DATA: {
        write_data_cmd_t *write_cmd = (write_data_cmd_t *) msg_ptr;
        nng_pipe          dst_pipe  = neu_node_manager_get_pipe_by_id(
            manager->node_manager, write_cmd->dst_node_id);

        forward_msg(manager, msg, dst_pipe);

        nlog_info("forward write request to driver pipe: %d", dst_pipe.id);
        break;
    }
    case MSG_DATA_WRITE_RESP: {
        write_data_resp_t *write_res = (write_data_resp_t *) msg_ptr;
        nng_pipe           dst_pipe  = neu_node_manager_get_pipe_by_id(
            manager->node_manager, write_res->recver_id);

        forward_msg(manager, msg, dst_pipe);

        nlog_info("forward write response to driver pipe: %d", dst_pipe.id);
        break;
    }

    case MSG_EVENT_ADD_NODE:
    case MSG_EVENT_UPDATE_NODE:
    case MSG_EVENT_DEL_NODE:
    case MSG_EVENT_SET_NODE_SETTING:
    case MSG_EVENT_ADD_GRP_CONFIG:
    case MSG_EVENT_UPDATE_GRP_CONFIG:
    case MSG_EVENT_DEL_GRP_CONFIG:
    case MSG_EVENT_ADD_TAGS:
    case MSG_EVENT_DEL_TAGS:
    case MSG_EVENT_UPDATE_TAGS:
    case MSG_EVENT_ADD_PLUGIN:
    case MSG_EVENT_UPDATE_PLUGIN:
    case MSG_EVENT_DEL_PLUGIN: {
        forward_msg(manager, msg, manager->persist_pipe);
        nlog_info("forward event %d to %s pipe: %d", msg_get_type(pay_msg),
                  DEFAULT_PERSIST_ADAPTER_NAME, manager->persist_pipe.id);
        break;
    }
    case MSG_EVENT_UPDATE_LICENSE: {
        UT_array *pipes = neu_node_manager_get_pipes(manager->node_manager,
                                                     NEU_NA_TYPE_DRIVER);

        utarray_foreach(pipes, nng_pipe *, pipe)
        {
            forward_msg(manager, msg, *pipe);
            nlog_info("forward license update to pipe: %d", pipe->id);
        }
        utarray_free(pipes);

        break;
    }
    case MSG_CMD_SUBSCRIBE_NODE: {
        subscribe_node_cmd_t *subscribe = (subscribe_node_cmd_t *) msg_ptr;
        nng_pipe              app_pipe =
            neu_node_manager_get_pipe(manager->node_manager, subscribe->app);

        neu_subscribe_manager_sub(manager->subscribe_manager, subscribe->driver,
                                  subscribe->app, subscribe->group, app_pipe);

        break;
    }
    case MSG_CMD_UNSUBSCRIBE_NODE: {
        unsubscribe_node_cmd_t *unsubscribe =
            (unsubscribe_node_cmd_t *) msg_ptr;

        neu_subscribe_manager_unsub(manager->subscribe_manager,
                                    unsubscribe->driver, unsubscribe->app,
                                    unsubscribe->group);
        break;
    }
    case MSG_DATA_NEURON_TRANS_DATA: {
        neuron_trans_data_t *trans = (neuron_trans_data_t *) msg_ptr;
        UT_array *           apps  = neu_subscribe_manager_find(
            manager->subscribe_manager, trans->node_name, trans->group);

        utarray_foreach(apps, neu_app_subscribe_t *, app)
        {
            forward_msg(manager, msg, app->pipe);
            nlog_debug("forward trans data to pipe: %d", app->pipe.id);
        }

        utarray_free(apps);
        break;
    }
    default:
        nlog_warn("receive a not supported msg type: %d",
                  msg_get_type(pay_msg));
        break;
    }
    nng_msg_free(msg);

    return 0;
}

inline static void forward_msg(neu_manager_t *manager, nng_msg *msg,
                               nng_pipe pipe)
{
    nng_msg *  out_nmsg;
    message_t *in_msg, *out_msg;
    void *     out_msg_ptr;

    in_msg = (message_t *) nng_msg_body(msg);
    nng_msg_alloc(&out_nmsg, nng_msg_len(msg));
    out_msg = (message_t *) nng_msg_body(out_nmsg);

    msg_inplace_data_init(out_msg, msg_get_type(in_msg),
                          msg_get_buf_len(in_msg));

    out_msg_ptr = msg_get_buf_ptr(out_msg);
    memcpy(out_msg_ptr, msg_get_buf_ptr(in_msg), msg_get_buf_len(in_msg));

    nng_msg_set_pipe(out_nmsg, pipe);
    nng_sendmsg(manager->socket, out_nmsg, 0);
}

static void start_static_adapter(neu_manager_t *manager, const char *name)
{
    neu_adapter_t *       adapter      = NULL;
    neu_plugin_instance_t instance     = { 0 };
    neu_adapter_info_t    adapter_info = {
        .name = name,
    };

    neu_plugin_manager_load_static(manager->plugin_manager, name, &instance);
    adapter_info.handle = instance.handle;
    adapter_info.module = instance.module;

    adapter     = neu_adapter_create(&adapter_info, manager);
    adapter->id = neu_node_manager_add_static(manager->node_manager, adapter);
    neu_adapter_init(adapter);
    neu_adapter_start(adapter);
}

static void stop_static_adapter(neu_manager_t *manager, const char *name)
{
    neu_adapter_t *adapter = neu_node_manager_find(manager->node_manager, name);

    neu_adapter_uninit(adapter);
    neu_adapter_destroy(adapter);
    neu_node_manager_del(manager->node_manager, name);
}