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

#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <nng/nng.h>
#include <nng/protocol/pair1/pair.h>
#include <nng/supplemental/util/platform.h>

#include "adapter.h"
#include "adapter_internal.h"
#include "driver/driver_internal.h"
#include "persist/persist.h"
#include "plugin.h"
#include "plugin_info.h"
#include "utils/log.h"

#define to_node_id(adapter, id) id;

static neu_persister_t *g_persister_singleton = NULL;

static void persister_singleton_init()
{
    const char *persistence_dir = "persistence";

    neu_persister_t *persister = neu_persister_create(persistence_dir);
    if (NULL == persister) {
        nlog_error("can't create persister");
        exit(EXIT_FAILURE);
    }
    g_persister_singleton = persister;
}

// NOTE: this function is not thread safe!
static neu_persister_t *persister_singleton_get()
{
    if (NULL == g_persister_singleton) {
        persister_singleton_init();
    }
    return g_persister_singleton;
}

static int persister_singleton_load_plugins(neu_adapter_t *adapter)
{
    UT_array *       plugin_infos = NULL;
    neu_persister_t *persister    = persister_singleton_get();

    int rv = neu_persister_load_plugins(persister, &plugin_infos);
    if (rv != 0) {
        return rv;
    }

    utarray_foreach(plugin_infos, char **, name)
    {
        rv                    = neu_manager_add_plugin(adapter->manager, *name);
        const char *ok_or_err = (0 == rv) ? "success" : "fail";
        nlog_info("%s load plugin %s, lib:%s", adapter->name, ok_or_err, *name);
    }

    utarray_foreach(plugin_infos, char **, name) { free(*name); }
    utarray_free(plugin_infos);

    return rv;
}

static int persister_singleton_load_setting(neu_adapter_t *adapter,
                                            const char *   adapter_name)
{
    neu_persister_t *persister = persister_singleton_get();
    const char *     setting   = NULL;
    int              rv =
        neu_persister_load_adapter_setting(persister, adapter_name, &setting);
    if (0 != rv) {
        const char *fail_or_ignore = "fail";
        if (NEU_ERR_ENOENT == rv) {
            // ignore this error, no setting ever set
            rv             = 0;
            fail_or_ignore = "ignore";
        }
        nlog_info("%s %s load setting of %s", adapter->name, fail_or_ignore,
                  adapter_name);
        return rv;
    }

    rv = neu_manager_node_setting(adapter->manager, adapter_name, setting);
    const char *ok_or_err = (0 == rv) ? "success" : "fail";
    nlog_info("%s %s set setting of %s %s", adapter->name, ok_or_err,
              adapter_name, setting);

    free((char *) setting);

    return rv;
}

static int persister_singleton_load_datatags(neu_adapter_t *adapter,
                                             const char *   driver,
                                             const char *   group)
{
    neu_persister_t *persister = persister_singleton_get();
    UT_array *       tags      = NULL;
    int rv = neu_persister_load_datatags(persister, driver, group, &tags);
    if (0 != rv) {
        const char *fail_or_ignore = "fail";
        if (NEU_ERR_ENOENT == rv) {
            // ignore no datatags
            rv             = 0;
            fail_or_ignore = "ignore";
        }
        nlog_info("%s %s load datatags info of adapter:%s grp:%s",
                  adapter->name, fail_or_ignore, driver, group);
        return rv;
    }

    utarray_foreach(tags, neu_datatag_t *, tag)
    {
        uint16_t index = 0;
        neu_manager_add_tag(adapter->manager, driver, group, 1, tag, &index);
        (void) index;
    }

    neu_persist_datatag_infos_free(tags);

    return rv;
}

static int persister_singleton_load_grp_and_tags(neu_adapter_t *adapter,
                                                 const char *   adapter_name)
{
    UT_array *       group_config_infos = NULL;
    neu_persister_t *persister          = persister_singleton_get();

    int rv = neu_persister_load_group_configs(persister, adapter_name,
                                              &group_config_infos);
    if (0 != rv) {
        const char *fail_or_ignore = "fail";
        if (NEU_ERR_ENOENT == rv) {
            // ignore no group configs
            rv             = 0;
            fail_or_ignore = "ignore";
        }
        nlog_error("%s %s load grp config of %s", adapter->name, fail_or_ignore,
                   adapter_name);
        return rv;
    }

    utarray_foreach(group_config_infos, neu_persist_group_config_info_t *, p)
    {
        neu_manager_add_group(adapter->manager, p->adapter_name,
                              p->group_config_name, p->read_interval);

        rv = persister_singleton_load_datatags(adapter, p->adapter_name,
                                               p->group_config_name);
        if (0 != rv) {
            nlog_warn("%s load datatags of adapter:%s grp:%s fail, ignore",
                      adapter->name, p->adapter_name, p->group_config_name);
        }
    }

    neu_persist_group_config_infos_free(group_config_infos);
    return rv;
}

static int persister_singleton_load_subscriptions(neu_adapter_t *adapter,
                                                  const char *   adapter_name)
{
    int              rv        = 0;
    neu_persister_t *persister = persister_singleton_get();
    UT_array *       sub_infos = NULL;

    rv = neu_persister_load_subscriptions(persister, adapter_name, &sub_infos);
    if (0 != rv) {
        const char *fail_or_ignore = "fail";
        if (NEU_ERR_ENOENT == rv) {
            // ignore no subscriptions
            rv             = 0;
            fail_or_ignore = "ignore";
        }
        nlog_error("%s %s load subscription infos of %s", adapter->name,
                   fail_or_ignore, adapter_name);
        return rv;
    }

    utarray_foreach(sub_infos, neu_persist_subscription_info_t *, info)
    {
        rv = neu_manager_subscribe(adapter->manager, info->sub_adapter_name,
                                   info->src_adapter_name,
                                   info->group_config_name);
        const char *ok_or_err = (0 == rv) ? "success" : "fail";
        nlog_info("%s %s load subscription app:%s driver:%s grp:%s",
                  adapter->name, ok_or_err, adapter_name,
                  info->src_adapter_name, info->group_config_name);
    }

    neu_persist_subscription_infos_free(sub_infos);

    return rv;
}

static int persister_singleton_load_data(neu_adapter_t *adapter)
{
    UT_array *       adapter_infos = NULL;
    neu_persister_t *persister     = persister_singleton_get();

    nlog_info("%s start persistence loading", adapter->name);

    int rv = persister_singleton_load_plugins(adapter);
    if (0 != rv) {
        return rv;
    }

    rv = neu_persister_load_adapters(persister, &adapter_infos);
    if (0 != rv) {
        nlog_error("%s failed to load adapter infos", adapter->name);
        goto error_load_adapters;
    }

    // sort by adapter id
    // qsort(adapter_infos->data, adapter_infos->size,
    // sizeof(neu_persist_adapter_info_t), persist_adapter_info_cmp);

    utarray_foreach(adapter_infos, neu_persist_adapter_info_t *, adapter_info)
    {
        rv = neu_manager_add_node(adapter->manager, adapter_info->name,
                                  adapter_info->plugin_name);
        const char *ok_or_err = (0 == rv) ? "success" : "fail";
        nlog_info("%s load adapter %s type:%" PRId64 ", name:%s plugin:%s",
                  adapter->name, ok_or_err, adapter_info->type,
                  adapter_info->name, adapter_info->plugin_name);
        if (0 != rv) {
            continue;
        }

        persister_singleton_load_setting(adapter, adapter_info->name);
        persister_singleton_load_grp_and_tags(adapter, adapter_info->name);

        if (ADAPTER_STATE_RUNNING == adapter_info->state) {
            rv = neu_manager_node_ctl(adapter->manager, adapter_info->name,
                                      NEU_ADAPTER_CTL_START);

            if (0 != rv) {
                nlog_error("%s fail start adapter %s", adapter->name,
                           adapter_info->name);
            }
        }
    }

    utarray_foreach(adapter_infos, neu_persist_adapter_info_t *, adapter_info)
    {
        persister_singleton_load_subscriptions(adapter, adapter_info->name);
    }

    neu_persist_adapter_infos_free(adapter_infos);
error_load_adapters:
    return rv;
}

static int persister_singleton_handle_nodes(neu_adapter_t *    adapter,
                                            neu_reqresp_type_e event,
                                            const char *       node_name)
{
    int                        rv           = 0;
    neu_persister_t *          persister    = persister_singleton_get();
    neu_persist_adapter_info_t adapter_info = {};

    nlog_info("%s handling node event %d of %s", adapter->name, event,
              node_name);

    if (NEU_REQ_DEL_NODE == event) {
        rv = neu_persister_delete_adapter(persister, node_name);
        if (0 != rv) {
            nlog_error("%s failed to del adapter %s", adapter->name, node_name);
        }
        return rv;
    }

    // MSG_EVENT_ADD_NODE
    rv = neu_manager_get_adapter_info(adapter->manager, node_name,
                                      &adapter_info);
    if (0 != rv) {
        nlog_error("%s unable to get adapter:%s info", adapter->name,
                   node_name);
        return rv;
    }

    rv = neu_persister_store_adapter(persister, &adapter_info);
    if (0 != rv) {
        nlog_error("%s failed to store adapter info", adapter->name);
    }

    neu_persist_adapter_info_fini(&adapter_info);
    return rv;
}

static int persister_singleton_handle_plugins(neu_adapter_t *adapter)
{
    neu_persister_t *persister    = persister_singleton_get();
    UT_array *       plugin_infos = NULL;

    nlog_info("%s handling plugin event", adapter->name);

    plugin_infos = neu_manager_get_plugins(adapter->manager);

    // store the current set of plugin infos
    int rv = neu_persister_store_plugins(persister, plugin_infos);
    if (0 != rv) {
        nlog_error("%s failed to store plugins infos", adapter->name);
    }

    neu_persist_plugin_infos_free(plugin_infos);
    return rv;
}

static int persister_singleton_handle_grp_config(neu_adapter_t *    adapter,
                                                 neu_reqresp_type_e event,
                                                 char *node_name, char *group,
                                                 uint32_t interval)
{
    neu_persister_t *persister = persister_singleton_get();
    int              rv        = 0;

    nlog_info("%s handling grp config event %d", adapter->name, event);

    if (NEU_REQ_DEL_GROUP != event) {
        neu_persist_group_config_info_t info = {
            .group_config_name = group,
            .read_interval     = interval,
            .adapter_name      = node_name,
        };
        rv = neu_persister_store_group_config(persister, node_name, &info);
        if (0 != rv) {
            nlog_error("%s failed to store group config infos", node_name);
        }
    } else {
        rv = neu_persister_delete_group_config(persister, node_name, group);
        if (0 != rv) {
            nlog_error("%s failed to del group config infos", group);
        }
    }

    return rv;
}

static int persister_singleton_handle_datatags(neu_adapter_t *adapter,
                                               const char *   node,
                                               const char *   group)
{
    neu_persister_t *persister = persister_singleton_get();
    UT_array *       tags      = NULL;

    int rv = neu_manager_get_tag(adapter->manager, node, group, &tags);
    if (rv != 0) {
        nlog_warn("%s fail get tag infos", adapter->name);
        return rv;
    }

    rv = neu_persister_store_datatags(persister, node, group, tags);
    if (0 != rv) {
        nlog_error("fail store adapter:%s grp:%s datatag infos", node, group);
    }

    neu_persist_datatag_infos_free(tags);
    return rv;
}

static int persister_singleton_handle_subscriptions(neu_adapter_t *adapter,
                                                    const char *   app)
{
    neu_persister_t *persister = persister_singleton_get();

    UT_array *sub_infos = neu_manager_get_sub_group(adapter->manager, app);

    int rv = neu_persister_store_subscriptions(persister, app, sub_infos);
    if (0 != rv) {
        nlog_error("%s fail store adapter:%s subscription infos", app, app);
    }

    utarray_free(sub_infos);

    return rv;
}

neu_plugin_running_state_e
neu_adapter_state_to_plugin_state(neu_adapter_t *adapter)
{
    neu_plugin_running_state_e state;

    switch (adapter->state) {
    case ADAPTER_STATE_IDLE:
        state = NEU_PLUGIN_RUNNING_STATE_IDLE;
        break;

    case ADAPTER_STATE_INIT:
        state = NEU_PLUGIN_RUNNING_STATE_INIT;
        break;

    case ADAPTER_STATE_READY:
        state = NEU_PLUGIN_RUNNING_STATE_READY;
        break;

    case ADAPTER_STATE_RUNNING:
        state = NEU_PLUGIN_RUNNING_STATE_RUNNING;
        break;

    case ADAPTER_STATE_STOPPED:
        state = NEU_PLUGIN_RUNNING_STATE_STOPPED;
        break;

    default:
        state = NEU_PLUGIN_RUNNING_STATE_IDLE;
        break;
    }

    return state;
}

static int adapter_command(neu_adapter_t *adapter, neu_reqresp_head_t header,
                           void *data)
{
    int      ret = 0;
    nng_msg *msg = NULL;

    strcpy(header.sender, adapter->name);
    switch (header.type) {
    case NEU_REQ_READ_GROUP:
    case NEU_REQ_WRITE_TAG: {
        neu_req_read_group_t *cmd = (neu_req_read_group_t *) data;
        strcpy(header.receiver, cmd->driver);
        break;
    }
    default:
        break;
    }

    msg = neu_msg_gen(&header, data);

    ret = nng_sendmsg(adapter->nng.sock, msg, 0);
    if (ret != 0) {
        nng_msg_free(msg);
    }

    return ret;
}

static int adapter_response(neu_adapter_t *adapter, neu_reqresp_head_t *header,
                            void *data)
{
    switch (header->type) {
    case NEU_REQRESP_TRANS_DATA:
        strcpy(header->sender, adapter->name);
        break;
    default:
        neu_msg_exchange(header);
        break;
    }

    nng_msg *msg = neu_msg_gen(header, data);

    return nng_sendmsg(adapter->nng.sock, msg, 0);
}

// clang-format off
static const adapter_callbacks_t callback_funs = {
    .command      = adapter_command,
    .response     = adapter_response,
};
// clang-format on

neu_adapter_t *neu_adapter_create(neu_adapter_info_t *info,
                                  neu_manager_t *     manager)
{
    int            rv      = 0;
    neu_adapter_t *adapter = calloc(1, sizeof(neu_adapter_t));

    adapter->cb_funs = callback_funs;
    switch (info->module->type) {
    case NEU_NA_TYPE_DRIVER:
        adapter = (neu_adapter_t *) neu_adapter_driver_create(adapter);
        break;
    default:
        break;
    }

    adapter->name               = strdup(info->name);
    adapter->state              = ADAPTER_STATE_IDLE;
    adapter->manager            = manager;
    adapter->plugin_info.handle = info->handle;
    adapter->plugin_info.module = info->module;
    adapter->req_id             = 1;

    rv = nng_mtx_alloc(&adapter->nng.mtx);
    assert(rv == 0);
    rv = nng_mtx_alloc(&adapter->nng.sub_grp_mtx);
    assert(rv == 0);

    adapter->plugin_info.plugin = adapter->plugin_info.module->intf_funs->open(
        adapter, &adapter->cb_funs);
    assert(adapter->plugin_info.plugin != NULL);
    assert(neu_plugin_common_check(adapter->plugin_info.plugin));
    neu_plugin_common_t *common =
        neu_plugin_to_plugin_common(adapter->plugin_info.plugin);
    common->adapter           = adapter;
    common->adapter_callbacks = &adapter->cb_funs;
    common->link_state        = NEU_PLUGIN_LINK_STATE_DISCONNECTED;
    common->log               = zlog_get_category(adapter->name);

    adapter->events = neu_event_new();

    nlog_info("Success to create adapter: %s", adapter->name);

    return adapter;
}

void neu_adapter_destroy(neu_adapter_t *adapter)
{
    if (adapter->plugin_info.module->type == NEU_NA_TYPE_DRIVER) {
        neu_adapter_driver_destroy((neu_adapter_driver_t *) adapter);
    }

    adapter->plugin_info.module->intf_funs->close(adapter->plugin_info.plugin);
    if (adapter->name != NULL) {
        free(adapter->name);
    }
    if (NULL != adapter->setting.buf) {
        free(adapter->setting.buf);
    }

    nng_mtx_free(adapter->nng.mtx);
    nng_mtx_free(adapter->nng.sub_grp_mtx);
    neu_event_close(adapter->events);
    free(adapter);
}

int adapter_loop(enum neu_event_io_type type, int fd, void *usr_data)
{
    neu_adapter_t *     adapter = (neu_adapter_t *) usr_data;
    int                 rv      = 0;
    nng_msg *           msg     = NULL;
    neu_reqresp_head_t *header  = NULL;

    switch (type) {
    case NEU_EVENT_IO_READ:
        rv = nng_recvmsg(adapter->nng.sock, &msg, 0);
        if (rv == NNG_ECLOSED) {
            nlog_warn("nng socket closed.");
            return 0;
        }
        header = (neu_reqresp_head_t *) nng_msg_body(msg);
        switch (header->type) {
        case NEU_REQRESP_PERSISTENCE_LOAD:
            persister_singleton_load_data(adapter);
            break;
        case NEU_REQ_APP_SUBSCRIBE_GROUP:
        case NEU_REQ_APP_UNSUBSCRIBE_GROUP:
        case NEU_RESP_GET_NODE_STATE:
        case NEU_RESP_GET_NODE_SETTING:
        case NEU_REQRESP_TRANS_DATA:
        case NEU_RESP_READ_GROUP:
        case NEU_RESP_GET_SUBSCRIBE_GROUP:
        case NEU_RESP_ADD_TAG:
        case NEU_RESP_UPDATE_TAG:
        case NEU_RESP_GET_TAG:
        case NEU_RESP_GET_NODE:
        case NEU_RESP_GET_PLUGIN:
        case NEU_RESP_GET_GROUP:
        case NEU_RESP_ERROR:
            adapter->plugin_info.module->intf_funs->request(
                adapter->plugin_info.plugin, (neu_reqresp_head_t *) header,
                &header[1]);
            break;
        case NEU_REQ_ADD_PLUGIN:
        case NEU_REQ_DEL_PLUGIN:
            persister_singleton_handle_plugins(adapter);
            break;
        case NEU_REQ_ADD_NODE:
        case NEU_REQ_DEL_NODE: {
            neu_req_add_node_t *cmd = (neu_req_add_node_t *) &header[1];
            persister_singleton_handle_nodes(adapter, header->type, cmd->node);
            break;
        }
        case NEU_REQ_ADD_GROUP: {
            neu_req_add_group_t *cmd = (neu_req_add_group_t *) &header[1];
            persister_singleton_handle_grp_config(
                adapter, header->type, cmd->driver, cmd->group, cmd->interval);
            break;
        }
        case NEU_REQ_DEL_GROUP: {
            neu_req_del_group_t *cmd = (neu_req_del_group_t *) &header[1];
            persister_singleton_handle_grp_config(adapter, header->type,
                                                  cmd->driver, cmd->group, 0);
            break;
        }
        case NEU_REQ_ADD_TAG:
        case NEU_REQ_DEL_TAG:
        case NEU_REQ_UPDATE_TAG: {
            neu_req_add_tag_t *cmd = (neu_req_add_tag_t *) &header[1];
            persister_singleton_handle_datatags(adapter, cmd->driver,
                                                cmd->group);
            break;
        }
        case NEU_REQ_SUBSCRIBE_GROUP:
        case NEU_REQ_UNSUBSCRIBE_GROUP: {
            neu_req_subscribe_t *cmd = (neu_req_subscribe_t *) &header[1];
            persister_singleton_handle_subscriptions(adapter, cmd->app);

            break;
        }
        case NEU_REQ_NODE_SETTING: {
            neu_req_node_setting_t *cmd = (neu_req_node_setting_t *) &header[1];
            neu_persister_t *       persister = persister_singleton_get();

            neu_persister_store_adapter_setting(persister, cmd->node,
                                                cmd->setting);

            free(cmd->setting);
            break;
        }
        case NEU_REQ_NODE_CTL: {
            neu_req_node_ctl_t *cmd = (neu_req_node_ctl_t *) &header[1];
            persister_singleton_handle_nodes(adapter, NEU_REQ_ADD_NODE,
                                             cmd->node);
            break;
        }
        case NEU_REQ_READ_GROUP: {
            assert(adapter->plugin_info.module->type == NEU_NA_TYPE_DRIVER);
            neu_adapter_driver_read_group((neu_adapter_driver_t *) adapter,
                                          header);
            break;
        }
        case NEU_REQ_WRITE_TAG: {
            assert(adapter->plugin_info.module->type == NEU_NA_TYPE_DRIVER);
            neu_adapter_driver_write_tag((neu_adapter_driver_t *) adapter,
                                         header);
            break;
        }
        default:
            assert(false);
            break;
        }

        nng_msg_free(msg);
        break;
    case NEU_EVENT_IO_CLOSED:
    case NEU_EVENT_IO_HUP:
        nlog_warn("adapter %s, fd: %d, recv closed or hup.", adapter->name, fd);
        break;
    }

    return 0;
}

int neu_adapter_init(neu_adapter_t *adapter)
{
    int rv = nng_pair1_open(&adapter->nng.sock);

    neu_event_io_param_t param = {
        .usr_data = (void *) adapter,
        .cb       = adapter_loop,
    };

    assert(rv == 0);

    nng_socket_set_ms(adapter->nng.sock, NNG_OPT_SENDTIMEO, 100);
    nng_socket_get_int(adapter->nng.sock, NNG_OPT_RECVFD, &adapter->recv_fd);
    param.fd = adapter->recv_fd;

    adapter->nng_io = neu_event_add_io(adapter->events, param);

    if (adapter->plugin_info.module->type == NEU_NA_TYPE_DRIVER) {
        neu_adapter_driver_init((neu_adapter_driver_t *) adapter);
    }

    rv = nng_dial(adapter->nng.sock, neu_manager_get_url(adapter->manager),
                  &adapter->nng.dialer, 0);
    assert(rv == 0);

    neu_reqresp_head_t  header = { 0 };
    neu_req_node_init_t init   = { 0 };

    header.type = NEU_REQ_NODE_INIT;
    strcpy(init.node, adapter->name);

    nng_msg *out_msg = neu_msg_gen(&header, &init);

    nng_sendmsg(adapter->nng.sock, out_msg, 0);

    if (adapter->state == ADAPTER_STATE_IDLE) {
        adapter->state = ADAPTER_STATE_INIT;
    }
    adapter->plugin_info.module->intf_funs->init(adapter->plugin_info.plugin);
    return rv;
}

int neu_adapter_uninit(neu_adapter_t *adapter)
{
    int rv = 0;

    nng_dialer_close(adapter->nng.dialer);
    nng_close(adapter->nng.sock);

    if (adapter->plugin_info.module->type == NEU_NA_TYPE_DRIVER) {
        neu_adapter_driver_uninit((neu_adapter_driver_t *) adapter);
    }
    adapter->plugin_info.module->intf_funs->uninit(adapter->plugin_info.plugin);

    neu_event_del_io(adapter->events, adapter->nng_io);

    nlog_info("Stop the adapter(%s)", adapter->name);
    return rv;
}

int neu_adapter_start(neu_adapter_t *adapter)
{
    // TODO  start by send msg
    const neu_plugin_intf_funs_t *intf_funs =
        adapter->plugin_info.module->intf_funs;
    neu_err_code_e error = NEU_ERR_SUCCESS;

    nng_mtx_lock(adapter->nng.mtx);
    switch (adapter->state) {
    case ADAPTER_STATE_IDLE:
    case ADAPTER_STATE_INIT:
        if (strcmp(adapter->plugin_info.module->module_name, "ekuiper") != 0) {
            error = NEU_ERR_NODE_NOT_READY;
        }
        break;
    case ADAPTER_STATE_RUNNING:
        error = NEU_ERR_NODE_IS_RUNNING;
        break;
    case ADAPTER_STATE_READY:
    case ADAPTER_STATE_STOPPED:
        break;
    }
    nng_mtx_unlock(adapter->nng.mtx);

    if (error != NEU_ERR_SUCCESS) {
        return error;
    }

    error = intf_funs->start(adapter->plugin_info.plugin);
    if (error == NEU_ERR_SUCCESS) {
        nng_mtx_lock(adapter->nng.mtx);
        adapter->state = ADAPTER_STATE_RUNNING;
        nng_mtx_unlock(adapter->nng.mtx);
    }

    return error;
}

int neu_adapter_stop(neu_adapter_t *adapter)
{
    // TODO  stop by send msg

    const neu_plugin_intf_funs_t *intf_funs =
        adapter->plugin_info.module->intf_funs;
    neu_err_code_e error = NEU_ERR_SUCCESS;

    nng_mtx_lock(adapter->nng.mtx);
    switch (adapter->state) {
    case ADAPTER_STATE_IDLE:
    case ADAPTER_STATE_INIT:
    case ADAPTER_STATE_READY:
        error = NEU_ERR_NODE_NOT_RUNNING;
        break;
    case ADAPTER_STATE_STOPPED:
        error = NEU_ERR_NODE_IS_STOPED;
        break;
    case ADAPTER_STATE_RUNNING:
        break;
    }
    nng_mtx_unlock(adapter->nng.mtx);

    if (error != NEU_ERR_SUCCESS) {
        return error;
    }

    error = intf_funs->stop(adapter->plugin_info.plugin);
    if (error == NEU_ERR_SUCCESS) {
        nng_mtx_lock(adapter->nng.mtx);
        adapter->state = ADAPTER_STATE_STOPPED;
        nng_mtx_unlock(adapter->nng.mtx);
    }

    return error;
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

neu_adapter_type_e neu_adapter_get_type(neu_adapter_t *adapter)
{
    if (adapter == NULL) {
        return 0;
    }

    return (neu_adapter_type_e) adapter->plugin_info.module->type;
}

int neu_adapter_set_setting(neu_adapter_t *adapter, neu_config_t *config)
{
    int rv = -1;

    const neu_plugin_intf_funs_t *intf_funs;

    intf_funs = adapter->plugin_info.module->intf_funs;
    rv        = intf_funs->config(adapter->plugin_info.plugin, config);
    if (rv == 0) {
        adapter->setting.buf_len = config->buf_len;
        adapter->setting.type    = config->type;

        if (adapter->setting.buf != NULL)
            free(adapter->setting.buf);
        adapter->setting.buf = strdup(config->buf);
        if (adapter->state == ADAPTER_STATE_INIT) {
            adapter->state = ADAPTER_STATE_READY;
        }
    } else {
        rv = NEU_ERR_NODE_SETTING_INVALID;
    }

    return rv;
}

int neu_adapter_get_setting(neu_adapter_t *adapter, char **config)
{
    if (adapter == NULL) {
        nlog_error("Get Config adapter with NULL adapter");
        return NEU_ERR_NODE_NOT_EXIST;
    }

    if (adapter->setting.buf != NULL) {
        *config = strdup(adapter->setting.buf);
        return NEU_ERR_SUCCESS;
    }

    return NEU_ERR_NODE_SETTING_NOT_FOUND;
}

neu_plugin_state_t neu_adapter_get_state(neu_adapter_t *adapter)
{
    neu_plugin_state_t   state = { 0 };
    neu_plugin_common_t *common =
        neu_plugin_to_plugin_common(adapter->plugin_info.plugin);

    state.link    = common->link_state;
    state.running = neu_adapter_state_to_plugin_state(adapter);

    return state;
}

int neu_adapter_validate_tag(neu_adapter_t *adapter, neu_datatag_t *tag)
{
    const neu_plugin_intf_funs_t *intf_funs =
        adapter->plugin_info.module->intf_funs;
    neu_err_code_e error = NEU_ERR_SUCCESS;

    error = intf_funs->driver.validate_tag(adapter->plugin_info.plugin, tag);

    return error;
}

neu_event_timer_t *neu_adapter_add_timer(neu_adapter_t *         adapter,
                                         neu_event_timer_param_t param)
{
    return neu_event_add_timer(adapter->events, param);
}

void neu_adapter_del_timer(neu_adapter_t *adapter, neu_event_timer_t *timer)
{
    neu_event_del_timer(adapter->events, timer);
}
