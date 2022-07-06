/**
 * NEURON IIoT System for Industry 4.0
 * Copyright (C) 2020-2022 EMQ Technologies Co., Ltd All rights reserved.
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
#include <unistd.h>

#include <nng/nng.h>
#include <nng/protocol/pair1/pair.h>
#include <nng/supplemental/util/platform.h>

#include "event/event.h"
#include "persist/persist.h"
#include "utils/log.h"

#include "adapter.h"
#include "adapter/adapter_internal.h"
#include "adapter/driver/driver_internal.h"
#include "errcodes.h"

#include "node_manager.h"
#include "plugin_manager.h"
#include "storage.h"
#include "subscribe.h"

#include "manager.h"
#include "manager_internal.h"

// definition for adapter names
#define DEFAULT_DASHBOARD_ADAPTER_NAME DEFAULT_DASHBOARD_PLUGIN_NAME

static const char *const url = "inproc://neu_manager";

static int manager_loop(enum neu_event_io_type type, int fd, void *usr_data);
inline static void reply(neu_manager_t *manager, neu_reqresp_head_t *header,
                         void *data);
inline static void forward_msg_dup(neu_manager_t *manager, nng_msg *msg,
                                   nng_pipe pipe);
inline static void forward_msg(neu_manager_t *manager, nng_msg *msg,
                               const char *ndoe);
static void start_static_adapter(neu_manager_t *manager, const char *name);

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
    manager->persister         = neu_persister_create("persistence");

    nng_pair1_open_poly(&manager->socket);

    rv = nng_listen(manager->socket, url, NULL, 0);
    assert(rv == 0);

    nng_socket_get_int(manager->socket, NNG_OPT_RECVFD, &param.fd);
    manager->loop = neu_event_add_io(manager->events, param);

    start_static_adapter(manager, DEFAULT_DASHBOARD_PLUGIN_NAME);

    if (manager_load_plugin(manager) != 0) {
        nlog_warn("load plugin error");
    } else {
        manager_load_node(manager);
        while (neu_node_manager_exist_uninit(manager->node_manager)) {
            usleep(1000 * 100);
        }

        manager_load_subscribe(manager);
    }

    return manager;
}

void neu_manager_destroy(neu_manager_t *manager)
{
    neu_reqresp_head_t  header     = { .type = NEU_REQ_NODE_UNINIT };
    neu_req_node_init_t uninit     = { 0 };
    nng_msg *           uninit_msg = NULL;
    UT_array *pipes = neu_node_manager_get_pipes_all(manager->node_manager);

    strcpy(header.sender, "manager");
    utarray_foreach(pipes, nng_pipe *, pipe)
    {
        uninit_msg = neu_msg_gen(&header, &uninit);
        nng_msg_set_pipe(uninit_msg, *pipe);

        if (nng_sendmsg(manager->socket, uninit_msg, 0) != 0) {
            nng_msg_free(uninit_msg);
            nlog_warn("manager -> %d uninit msg send fail", (*pipe).id);
        }
    }
    utarray_free(pipes);

    while (1) {
        usleep(1000 * 100);
        if (neu_node_manager_size(manager->node_manager) == 0) {
            break;
        }
    }

    neu_persister_destroy(manager->persister);
    neu_subscribe_manager_destroy(manager->subscribe_manager);
    neu_node_manager_destroy(manager->node_manager);
    neu_plugin_manager_destroy(manager->plugin_manager);

    nng_close(manager->socket);
    neu_event_del_io(manager->events, manager->loop);
    neu_event_close(manager->events);

    free(manager);
    nlog_warn("manager exit");
}

const char *neu_manager_get_url()
{
    return url;
}

static int manager_loop(enum neu_event_io_type type, int fd, void *usr_data)
{
    neu_manager_t *     manager = (neu_manager_t *) usr_data;
    int                 rv      = 0;
    nng_msg *           msg     = NULL;
    neu_reqresp_head_t *header  = NULL;

    if (type == NEU_EVENT_IO_CLOSED || type == NEU_EVENT_IO_HUP) {
        nlog_warn("manager socket(%d) recv closed or hup %d.", fd, type);
        return 0;
    }

    rv = nng_recvmsg(manager->socket, &msg, NNG_FLAG_NONBLOCK);
    if (rv != 0) {
        nlog_warn("manager recv msg error: %d", rv);
        return 0;
    }

    header = (neu_reqresp_head_t *) nng_msg_body(msg);

    nlog_info("manager recv msg from: %s, type: %s", header->sender,
              neu_reqresp_type_string(header->type));
    switch (header->type) {
    case NEU_REQRESP_TRANS_DATA: {
        neu_reqresp_trans_data_t *cmd = (neu_reqresp_trans_data_t *) &header[1];
        UT_array *apps = neu_subscribe_manager_find(manager->subscribe_manager,
                                                    cmd->driver, cmd->group);
        if (apps != NULL) {
            utarray_foreach(apps, neu_app_subscribe_t *, app)
            {
                forward_msg_dup(manager, msg, app->pipe);
                nlog_debug("forward trans data to pipe: %d", app->pipe.id);
            }
            utarray_free(apps);
            break;
        }
        break;
    }
    case NEU_REQ_UPDATE_LICENSE: {
        UT_array *pipes = neu_node_manager_get_pipes(manager->node_manager,
                                                     NEU_NA_TYPE_DRIVER);

        utarray_foreach(pipes, nng_pipe *, pipe)
        {
            forward_msg_dup(manager, msg, *pipe);
            nlog_info("forward license update to pipe: %d", pipe->id);
        }
        utarray_free(pipes);

        break;
    }
    case NEU_REQ_NODE_INIT: {
        neu_req_node_init_t *init = (neu_req_node_init_t *) &header[1];
        nng_pipe             pipe = nng_msg_get_pipe(msg);

        neu_node_manager_update(manager->node_manager, init->node, pipe);
        nlog_notice("bind node %s to pipe(%d)", init->node, pipe.id);
        break;
    }
    case NEU_REQ_ADD_PLUGIN: {
        neu_req_add_plugin_t *cmd = (neu_req_add_plugin_t *) &header[1];
        int              error = neu_manager_add_plugin(manager, cmd->library);
        neu_resp_error_t e     = { .error = error };

        if (error == NEU_ERR_SUCCESS) {
            manager_strorage_plugin(manager);
        }

        header->type = NEU_RESP_ERROR;
        strcpy(header->receiver, header->sender);
        reply(manager, header, &e);
        break;
    }
    case NEU_REQ_DEL_PLUGIN: {
        neu_req_del_plugin_t *cmd = (neu_req_del_plugin_t *) &header[1];
        int              error = neu_manager_del_plugin(manager, cmd->plugin);
        neu_resp_error_t e     = { .error = error };

        if (error == NEU_ERR_SUCCESS) {
            manager_strorage_plugin(manager);
        }

        header->type = NEU_RESP_ERROR;
        strcpy(header->receiver, header->sender);
        reply(manager, header, &e);
        break;
    }
    case NEU_REQ_GET_PLUGIN: {
        UT_array *            plugins = neu_manager_get_plugins(manager);
        neu_resp_get_plugin_t resp    = { .plugins = plugins };

        header->type = NEU_RESP_GET_PLUGIN;
        strcpy(header->receiver, header->sender);
        reply(manager, header, &resp);
        break;
    }
    case NEU_REQ_ADD_NODE: {
        neu_req_add_node_t *cmd = (neu_req_add_node_t *) &header[1];
        int error = neu_manager_add_node(manager, cmd->node, cmd->plugin);
        neu_resp_error_t e = { .error = error };

        if (error == NEU_ERR_SUCCESS) {
            manager_storage_add_node(manager, cmd->node);
        }

        header->type = NEU_RESP_ERROR;
        strcpy(header->receiver, header->sender);
        reply(manager, header, &e);
        break;
    }
    case NEU_REQ_DEL_NODE: {
        neu_req_del_node_t *cmd   = (neu_req_del_node_t *) &header[1];
        neu_resp_error_t    error = { 0 };

        strcpy(header->receiver, cmd->node);
        if (neu_node_manager_find(manager->node_manager, header->receiver) !=
            NULL) {
            manager_storage_del_node(manager, cmd->node);
            header->type = NEU_REQ_NODE_UNINIT;
            forward_msg(manager, msg, header->receiver);
        } else {
            error.error  = NEU_ERR_NODE_NOT_EXIST;
            header->type = NEU_RESP_ERROR;
            neu_msg_exchange(header);
            reply(manager, header, &error);
        }
        break;
    }
    case NEU_RESP_NODE_UNINIT: {
        neu_resp_node_uninit_t *cmd = (neu_resp_node_uninit_t *) &header[1];

        neu_manager_del_node(manager, cmd->node);
        if (strlen(header->receiver) > 0 &&
            strcmp(header->receiver, "manager") != 0) {
            neu_resp_error_t error = { 0 };
            header->type           = NEU_RESP_ERROR;
            reply(manager, header, &error);
        }
        break;
    }
    case NEU_REQ_GET_NODE: {
        neu_req_get_node_t *cmd   = (neu_req_get_node_t *) &header[1];
        UT_array *          nodes = neu_manager_get_nodes(manager, cmd->type);
        neu_resp_get_node_t resp  = { .nodes = nodes };

        header->type = NEU_RESP_GET_NODE;
        strcpy(header->receiver, header->sender);
        reply(manager, header, &resp);
        break;
    }
    case NEU_REQ_SUBSCRIBE_GROUP: {
        neu_req_subscribe_t *cmd   = (neu_req_subscribe_t *) &header[1];
        neu_resp_error_t     error = { 0 };

        error.error =
            neu_manager_subscribe(manager, cmd->app, cmd->driver, cmd->group);

        if (error.error == NEU_ERR_SUCCESS) {
            manager_storage_subscribe(manager, cmd->app);
            neu_manager_notify_app_sub(manager, cmd->app, cmd->driver,
                                       cmd->group);
        }

        header->type = NEU_RESP_ERROR;
        strcpy(header->receiver, header->sender);
        reply(manager, header, &error);
        break;
    }
    case NEU_REQ_UNSUBSCRIBE_GROUP: {
        neu_req_unsubscribe_t *cmd   = (neu_req_unsubscribe_t *) &header[1];
        neu_resp_error_t       error = { 0 };

        error.error =
            neu_manager_unsubscribe(manager, cmd->app, cmd->driver, cmd->group);

        if (error.error == NEU_ERR_SUCCESS) {
            manager_storage_subscribe(manager, cmd->app);
            neu_manager_notify_app_unsub(manager, cmd->app, cmd->driver,
                                         cmd->group);
        }

        header->type = NEU_RESP_ERROR;
        strcpy(header->receiver, header->sender);
        reply(manager, header, &error);
        break;
    }
    case NEU_REQ_GET_SUBSCRIBE_GROUP: {
        neu_req_get_subscribe_group_t *cmd =
            (neu_req_get_subscribe_group_t *) &header[1];
        UT_array *groups = neu_manager_get_sub_group(manager, cmd->app);
        neu_resp_get_subscribe_group_t resp = { .groups = groups };

        strcpy(header->receiver, header->sender);
        header->type = NEU_RESP_GET_SUBSCRIBE_GROUP;
        reply(manager, header, &resp);
        break;
    }

    case NEU_REQ_GET_NODE_SETTING:
    case NEU_REQ_READ_GROUP:
    case NEU_REQ_WRITE_TAG:
    case NEU_REQ_GET_NODE_STATE:
    case NEU_REQ_GET_GROUP:
    case NEU_REQ_GET_TAG:
    case NEU_REQ_NODE_CTL:
    case NEU_REQ_ADD_GROUP: {
        if (neu_node_manager_find(manager->node_manager, header->receiver) ==
            NULL) {
            neu_resp_error_t e = { .error = NEU_ERR_NODE_NOT_EXIST };
            header->type       = NEU_RESP_ERROR;
            neu_msg_exchange(header);
            reply(manager, header, &e);
        } else {
            forward_msg(manager, msg, header->receiver);
        }

        break;
    }

    case NEU_REQ_DEL_GROUP: {
        neu_req_del_group_t *cmd = (neu_req_del_group_t *) &header[1];

        if (neu_node_manager_find(manager->node_manager, header->receiver) ==
            NULL) {
            neu_resp_error_t e = { .error = NEU_ERR_NODE_NOT_EXIST };
            header->type       = NEU_RESP_ERROR;
            neu_msg_exchange(header);
            reply(manager, header, &e);
        } else {
            forward_msg(manager, msg, header->receiver);
            neu_manager_notify_app_unsub_update(manager, cmd->driver,
                                                cmd->group);
            neu_subscribe_manager_remove(manager->subscribe_manager,
                                         cmd->driver, cmd->group);
        }
        break;
    }
    case NEU_REQ_DEL_TAG: {
        neu_req_del_tag_t *cmd = (neu_req_del_tag_t *) &header[1];

        if (neu_node_manager_find(manager->node_manager, header->receiver) ==
            NULL) {
            neu_resp_error_t e = { .error = NEU_ERR_NODE_NOT_EXIST };
            header->type       = NEU_RESP_ERROR;
            neu_msg_exchange(header);
            reply(manager, header, &e);
            for (int i = 0; i < cmd->n_tag; i++) {
                free(cmd->tags[i]);
            }
            free(cmd->tags);
        } else {
            forward_msg(manager, msg, header->receiver);
            neu_manager_notify_app_sub_update(manager, cmd->driver, cmd->group);
        }

        break;
    }
    case NEU_REQ_UPDATE_TAG:
    case NEU_REQ_ADD_TAG: {
        neu_req_add_tag_t *cmd = (neu_req_add_tag_t *) &header[1];

        if (neu_node_manager_find(manager->node_manager, header->receiver) ==
            NULL) {
            neu_resp_error_t e = { .error = NEU_ERR_NODE_NOT_EXIST };
            header->type       = NEU_RESP_ERROR;
            neu_msg_exchange(header);
            reply(manager, header, &e);
            for (int i = 0; i < cmd->n_tag; i++) {
                free(cmd->tags[i].address);
                free(cmd->tags[i].name);
                free(cmd->tags[i].description);
            }
            free(cmd->tags);
        } else {
            forward_msg(manager, msg, header->receiver);
            neu_manager_notify_app_sub_update(manager, cmd->driver, cmd->group);
        }

        break;
    }
    case NEU_REQ_NODE_SETTING: {
        neu_req_node_setting_t *cmd = (neu_req_node_setting_t *) &header[1];

        if (neu_node_manager_find(manager->node_manager, header->receiver) ==
            NULL) {
            neu_resp_error_t e = { .error = NEU_ERR_NODE_NOT_EXIST };
            header->type       = NEU_RESP_ERROR;
            neu_msg_exchange(header);
            reply(manager, header, &e);
            free(cmd->setting);
        } else {
            forward_msg(manager, msg, header->receiver);
        }
        break;
    }

    case NEU_RESP_ADD_TAG:
    case NEU_RESP_UPDATE_TAG:
    case NEU_RESP_GET_TAG:
    case NEU_RESP_GET_GROUP:
    case NEU_RESP_GET_NODE_SETTING:
    case NEU_RESP_GET_NODE_STATE:
    case NEU_RESP_ERROR:
    case NEU_RESP_READ_GROUP:
    case NEU_RESP_APP_SUBSCRIBE_GROUP:
        forward_msg(manager, msg, header->receiver);
        break;
    default:
        assert(false);
        break;
    }

    nng_msg_free(msg);
    return 0;
}

inline static void forward_msg_dup(neu_manager_t *manager, nng_msg *msg,
                                   nng_pipe pipe)
{
    nng_msg *out_msg;

    nng_msg_dup(&out_msg, msg);
    nng_msg_set_pipe(out_msg, pipe);
    if (nng_sendmsg(manager->socket, out_msg, 0) == 0) {
        nlog_info("forward msg to pipe %d", pipe.id);
    } else {
        nng_msg_free(out_msg);
    }
}

inline static void forward_msg(neu_manager_t *manager, nng_msg *msg,
                               const char *node)
{
    nng_msg *out_msg;
    nng_pipe pipe = neu_node_manager_get_pipe(manager->node_manager, node);

    nng_msg_dup(&out_msg, msg);
    nng_msg_set_pipe(out_msg, pipe);
    if (nng_sendmsg(manager->socket, out_msg, 0) == 0) {
        nlog_info("forward msg to %s", node);
    } else {
        nng_msg_free(out_msg);
    }
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

    adapter = neu_adapter_create(&adapter_info, manager);
    neu_node_manager_add_static(manager->node_manager, adapter);
    neu_adapter_init(adapter);
    neu_adapter_start(adapter);
}

inline static void reply(neu_manager_t *manager, neu_reqresp_head_t *header,
                         void *data)
{
    nng_msg *msg = neu_msg_gen(header, data);
    nng_pipe pipe =
        neu_node_manager_get_pipe(manager->node_manager, header->receiver);

    nng_msg_set_pipe(msg, pipe);

    if (nng_sendmsg(manager->socket, msg, 0) != 0) {
        nng_msg_free(msg);
    }
}