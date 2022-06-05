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
#include "core/message.h"
#include "driver/driver_internal.h"
#include "persist/persist.h"
#include "plugin.h"
#include "plugin_info.h"
#include "utils/log.h"

#define to_node_id(adapter, id) id;

#define _ADAPTER_RESP(adapter, cmd, ret_type, req_type, rv, resp_type_code, \
                      p_result, func)                                       \
    {                                                                       \
        neu_response_t *result;                                             \
        assert((cmd)->buf_len == sizeof(req_type));                         \
        result = malloc(sizeof(neu_response_t));                            \
        if (result == NULL) {                                               \
            nlog_error("Failed to allocate result for resp cmd");           \
            rv = -1;                                                        \
        } else {                                                            \
            req_type *req_cmd = (req_type *) (cmd)->buf;                    \
            (void) req_cmd;                                                 \
            { func };                                                       \
            result->resp_type = (resp_type_code);                           \
            result->req_id    = (cmd)->req_id;                              \
            result->buf_len   = sizeof(ret_type);                           \
            result->buf       = (void *) ret;                               \
            if ((p_result) != NULL) {                                       \
                *(p_result) = result;                                       \
            } else {                                                        \
                free(result);                                               \
            }                                                               \
        }                                                                   \
    }

#define ADAPTER_RESP_CODE(adapter, cmd, ret_type, req_type, rv,             \
                          resp_type_code, p_result, func)                   \
    {                                                                       \
        ret_type ret;                                                       \
        _ADAPTER_RESP(adapter, cmd, ret_type, req_type, rv, resp_type_code, \
                      p_result, func)                                       \
    }

#define ADAPTER_RESP_CMD(adapter, cmd, ret_type, req_type, rv, resp_type_code, \
                         p_result, func)                                       \
    {                                                                          \
        ret_type *ret;                                                         \
        _ADAPTER_RESP(adapter, cmd, ret_type, req_type, rv, resp_type_code,    \
                      p_result, func)                                          \
    }

static uint32_t adapter_get_req_id(neu_adapter_t *adapter);

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

    neu_persist_plugin_infos_free(plugin_infos);
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

// static int persist_adapter_info_cmp(const void *a, const void *b)
//{
// return strcmp(((neu_persist_adapter_info_t *) a)->name,
//((neu_persist_adapter_info_t *) b)->name);
//}

static int persister_singleton_load_data(neu_adapter_t *adapter)
{
    UT_array *       adapter_infos = NULL;
    neu_persister_t *persister     = persister_singleton_get();

    // declaration for neu_manager_update_node_id since it is not exposed
    int neu_manager_update_node_id(neu_manager_t * manager,
                                   neu_node_id_t node_id,
                                   neu_node_id_t new_node_id);

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

static int persister_singleton_handle_nodes(neu_adapter_t *adapter,
                                            msg_type_e     event,
                                            const char *   node_name)
{
    int                        rv           = 0;
    neu_persister_t *          persister    = persister_singleton_get();
    neu_persist_adapter_info_t adapter_info = {};

    nlog_info("%s handling node event %d of %s", adapter->name, event,
              node_name);

    if (MSG_NODE_DEL == event) {
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

static int persister_singleton_handle_plugins(neu_adapter_t *adapter,
                                              msg_type_e     event)
{
    neu_persister_t *persister    = persister_singleton_get();
    UT_array *       plugin_infos = NULL;

    nlog_info("%s handling plugin event %d", adapter->name, event);

    plugin_infos = neu_manager_get_plugin_info(adapter->manager);

    // store the current set of plugin infos
    int rv = neu_persister_store_plugins(persister, plugin_infos);
    if (0 != rv) {
        nlog_error("%s failed to store plugins infos", adapter->name);
    }

    neu_persist_plugin_infos_free(plugin_infos);
    return rv;
}

static int persister_singleton_handle_grp_config(neu_adapter_t *adapter,
                                                 msg_type_e     event,
                                                 char *node_name, char *group,
                                                 uint32_t interval)
{
    neu_persister_t *persister = persister_singleton_get();
    int              rv        = 0;

    nlog_info("%s handling grp config event %d", adapter->name, event);

    if (MSG_GROUP_DEL != event) {
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

static uint32_t adapter_get_req_id(neu_adapter_t *adapter)
{
    return adapter->req_id += 2;
}

static int adapter_command(neu_adapter_t *adapter, neu_request_t *cmd,
                           neu_response_t **p_result)
{
    int rv = 0;

    if (adapter == NULL || cmd == NULL) {
        nlog_warn("The adapter or command is NULL");
        return (-1);
    }

    if (adapter->state == ADAPTER_STATE_IDLE) {
        nlog_warn("The adapter loop not running");
        return -1;
    }

    nlog_debug("Get command from plugin %d, %s", cmd->req_type, adapter->name);
    switch (cmd->req_type) {
    case NEU_REQRESP_READ_DATA: {
        nng_msg *           out_msg = NULL;
        neu_reqresp_read_t *req     = (neu_reqresp_read_t *) cmd->buf;
        msg_group_read_t *  read    = (msg_group_read_t *) msg_new(
            &out_msg, MSG_GROUP_READ, sizeof(msg_group_read_t));

        strcpy(read->driver, req->driver);
        strcpy(read->group, req->group);
        strcpy(read->reader, adapter->name);
        read->ctx = req->ctx;

        nng_sendmsg(adapter->nng.sock, out_msg, 0);
        break;
    }
    case NEU_REQRESP_WRITE_DATA: {
        nng_msg *            out_msg = NULL;
        neu_reqresp_write_t *req     = (neu_reqresp_write_t *) cmd->buf;
        msg_tag_write_t *    write   = (msg_tag_write_t *) msg_new(
            &out_msg, MSG_TAG_WRITE, sizeof(msg_tag_write_t));

        strcpy(write->writer, adapter->name);
        strcpy(write->driver, req->driver);
        strcpy(write->group, req->group);
        strcpy(write->tag, req->tag);
        write->value = req->value;
        write->ctx   = req->ctx;

        nng_sendmsg(adapter->nng.sock, out_msg, 0);
        break;
    }

    case NEU_REQRESP_SUBSCRIBE_NODE: {
        ADAPTER_RESP_CODE(adapter, cmd, intptr_t, neu_reqresp_subscribe_node_t,
                          rv, NEU_REQRESP_ERR_CODE, p_result, {
                              ret = neu_manager_subscribe(
                                  adapter->manager, req_cmd->app,
                                  req_cmd->driver, req_cmd->group);
                              if (ret == 0) {
                                  nng_msg *              out_msg = NULL;
                                  msg_group_subscribe_t *sub =
                                      (msg_group_subscribe_t *) msg_new(
                                          &out_msg, MSG_GROUP_SUBSCRIBE,
                                          sizeof(msg_group_subscribe_t));

                                  strcpy(sub->app, req_cmd->app);
                                  strcpy(sub->driver, req_cmd->driver);
                                  strcpy(sub->group, req_cmd->group);

                                  nng_sendmsg(adapter->nng.sock, out_msg, 0);
                              }
                          });
        break;
    }
    case NEU_REQRESP_UNSUBSCRIBE_NODE: {
        ADAPTER_RESP_CODE(
            adapter, cmd, intptr_t, neu_reqresp_unsubscribe_node_t, rv,
            NEU_REQRESP_ERR_CODE, p_result, {
                ret = neu_manager_unsubscribe(adapter->manager, req_cmd->app,
                                              req_cmd->driver, req_cmd->group);
                if (ret == 0) {
                    nng_msg *                out_msg = NULL;
                    msg_group_unsubscribe_t *sub =
                        (msg_group_unsubscribe_t *) msg_new(
                            &out_msg, MSG_GROUP_UNSUBSCRIBE,
                            sizeof(msg_group_unsubscribe_t));

                    strcpy(sub->app, req_cmd->app);
                    strcpy(sub->driver, req_cmd->driver);
                    strcpy(sub->group, req_cmd->group);

                    nng_sendmsg(adapter->nng.sock, out_msg, 0);
                }
            });
        break;
    }

    case NEU_REQRESP_ADD_NODE: {
        ADAPTER_RESP_CODE(
            adapter, cmd, intptr_t, neu_cmd_add_node_t, rv,
            NEU_REQRESP_ERR_CODE, p_result, {
                ret = neu_manager_add_node(adapter->manager,
                                           req_cmd->adapter_name,
                                           req_cmd->plugin_name);
                if (0 == ret) {
                    nng_msg *       out_msg = NULL;
                    msg_add_node_t *add     = (msg_add_node_t *) msg_new(
                        &out_msg, MSG_NODE_ADD, sizeof(msg_add_node_t));
                    strcpy(add->node, req_cmd->adapter_name);
                    strcpy(add->plugin, req_cmd->plugin_name);
                    nng_sendmsg(adapter->nng.sock, out_msg, 0);
                }
            });
        break;
    }

    case NEU_REQRESP_DEL_NODE: {
        ADAPTER_RESP_CODE(
            adapter, cmd, intptr_t, neu_cmd_del_node_t, rv,
            NEU_REQRESP_ERR_CODE, p_result, {
                ret = neu_manager_del_node(adapter->manager, req_cmd->name);
                if (0 == ret) {
                    nng_msg *       out_msg = NULL;
                    msg_del_node_t *add     = (msg_del_node_t *) msg_new(
                        &out_msg, MSG_NODE_DEL, sizeof(msg_del_node_t));
                    strcpy(add->node, req_cmd->name);
                    nng_sendmsg(adapter->nng.sock, out_msg, 0);
                }
            });
        break;
    }

    case NEU_REQRESP_GET_NODES: {
        ADAPTER_RESP_CMD(adapter, cmd, neu_reqresp_nodes_t, neu_cmd_get_nodes_t,
                         rv, NEU_REQRESP_NODES, p_result, {
                             ret        = malloc(sizeof(neu_reqresp_nodes_t));
                             ret->nodes = neu_manager_get_nodes(
                                 adapter->manager, req_cmd->node_type);
                         });

        break;
    }

    case NEU_REQRESP_ADD_GRP_CONFIG: {
        ADAPTER_RESP_CODE(
            adapter, cmd, intptr_t, neu_cmd_add_grp_config_t, rv,
            NEU_REQRESP_ERR_CODE, p_result, {
                ret =
                    neu_manager_add_group(adapter->manager, req_cmd->node_name,
                                          req_cmd->name, req_cmd->interval);
                if (0 == ret) {
                    nng_msg *        out_msg = NULL;
                    msg_add_group_t *add     = (msg_add_group_t *) msg_new(
                        &out_msg, MSG_GROUP_ADD, sizeof(msg_add_group_t));

                    strcpy(add->driver, req_cmd->node_name);
                    strcpy(add->group, req_cmd->name);
                    add->interval = req_cmd->interval;

                    nng_sendmsg(adapter->nng.sock, out_msg, 0);
                }
            });
        break;
    }

    case NEU_REQRESP_DEL_GRP_CONFIG: {
        ADAPTER_RESP_CODE(
            adapter, cmd, intptr_t, neu_cmd_del_grp_config_t, rv,
            NEU_REQRESP_ERR_CODE, p_result, {
                ret = neu_manager_del_group(adapter->manager,
                                            req_cmd->node_name, req_cmd->name);
                if (0 == ret) {
                    nng_msg *        out_msg = NULL;
                    msg_del_group_t *add     = (msg_del_group_t *) msg_new(
                        &out_msg, MSG_GROUP_DEL, sizeof(msg_del_group_t));

                    strcpy(add->driver, req_cmd->node_name);
                    strcpy(add->group, req_cmd->name);

                    nng_sendmsg(adapter->nng.sock, out_msg, 0);
                }
            });
        break;
    }

    case NEU_REQRESP_UPDATE_GRP_CONFIG: {
        ADAPTER_RESP_CODE(adapter, cmd, intptr_t, neu_cmd_update_grp_config_t,
                          rv, NEU_REQRESP_ERR_CODE, p_result, { ret = 0; });
        break;
    }

    case NEU_REQRESP_GET_GRP_CONFIGS: {
        ADAPTER_RESP_CMD(
            adapter, cmd, neu_reqresp_grp_configs_t, neu_cmd_get_grp_configs_t,
            rv, NEU_REQRESP_GRP_CONFIGS, p_result, {
                ret        = malloc(sizeof(neu_reqresp_grp_configs_t));
                ret->error = neu_manager_get_group(
                    adapter->manager, req_cmd->node_name, &ret->groups);
            });
        break;
    }

    case NEU_REQRESP_ADD_TAGS: {
        ADAPTER_RESP_CMD(
            adapter, cmd, neu_reqresp_add_tag_resp_t, neu_reqresp_add_tag_t, rv,
            NEU_REQRESP_ADD_TAGS, p_result, {
                ret        = malloc(sizeof(neu_reqresp_add_tag_resp_t));
                ret->error = neu_manager_add_tag(
                    adapter->manager, req_cmd->node, req_cmd->group,
                    req_cmd->n_tag, req_cmd->tags, &ret->index);
            });

        break;
    }
    case NEU_REQRESP_DEL_TAGS: {
        ADAPTER_RESP_CMD(adapter, cmd, neu_reqresp_del_tag_resp_t,
                         neu_reqresp_del_tag_t, rv, NEU_REQRESP_DEL_TAGS,
                         p_result, {
                             ret = malloc(sizeof(neu_reqresp_del_tag_resp_t));
                             ret->error = neu_manager_del_tag(
                                 adapter->manager, req_cmd->node,
                                 req_cmd->group, req_cmd->n_tag, req_cmd->tags);
                         });
        break;
    }
    case NEU_REQRESP_UPDATE_TAGS: {
        ADAPTER_RESP_CMD(
            adapter, cmd, neu_reqresp_update_tag_resp_t,
            neu_reqresp_update_tag_t, rv, NEU_REQRESP_UPDATE_TAGS, p_result, {
                ret        = malloc(sizeof(neu_reqresp_update_tag_resp_t));
                ret->error = neu_manager_update_tag(
                    adapter->manager, req_cmd->node, req_cmd->group,
                    req_cmd->n_tag, req_cmd->tags, &ret->index);
            });
        break;
    }
    case NEU_REQRESP_GET_TAGS: {
        ADAPTER_RESP_CMD(
            adapter, cmd, neu_reqresp_get_tags_resp_t, neu_reqresp_get_tags_t,
            rv, NEU_REQRESP_GET_TAGS, p_result, {
                ret = malloc(sizeof(neu_reqresp_get_tags_resp_t));
                ret->error =
                    neu_manager_get_tag(adapter->manager, req_cmd->node,
                                        req_cmd->group, &ret->tags);
            });
        break;
    }

    case NEU_REQRESP_ADD_PLUGIN_LIB: {
        ADAPTER_RESP_CODE(
            adapter, cmd, intptr_t, neu_cmd_add_plugin_lib_t, rv,
            NEU_REQRESP_ERR_CODE, p_result, {
                ret = neu_manager_add_plugin(adapter->manager,
                                             req_cmd->plugin_lib_name);
                if (ret == NEU_ERR_SUCCESS) {
                    nng_msg *         out_msg = NULL;
                    msg_add_plugin_t *add     = (msg_add_plugin_t *) msg_new(
                        &out_msg, MSG_PLUGIN_ADD, sizeof(msg_add_plugin_t));

                    strcpy(add->library, req_cmd->plugin_lib_name);

                    nng_sendmsg(adapter->nng.sock, out_msg, 0);
                }
            });
        break;
    }

    case NEU_REQRESP_DEL_PLUGIN_LIB: {
        ADAPTER_RESP_CODE(
            adapter, cmd, intptr_t, neu_cmd_del_plugin_lib_t, rv,
            NEU_REQRESP_ERR_CODE, p_result, {
                ret = NEU_ERR_SUCCESS;
                rv  = neu_manager_del_plugin(adapter->manager, req_cmd->name);
                if (rv != 0) {
                    ret = NEU_ERR_FAILURE;
                } else {
                    nng_msg *         out_msg = NULL;
                    msg_del_plugin_t *add     = (msg_del_plugin_t *) msg_new(
                        &out_msg, MSG_PLUGIN_DEL, sizeof(msg_del_plugin_t));

                    strcpy(add->plugin, req_cmd->name);

                    nng_sendmsg(adapter->nng.sock, out_msg, 0);
                }
            });
        break;
    }

    case NEU_REQRESP_GET_PLUGIN_LIBS: {
        ADAPTER_RESP_CMD(
            adapter, cmd, neu_reqresp_plugin_libs_t, neu_cmd_get_plugin_libs_t,
            rv, NEU_REQRESP_PLUGIN_LIBS, p_result, {
                ret              = malloc(sizeof(neu_reqresp_plugin_libs_t));
                ret->plugin_libs = neu_manager_get_plugins(adapter->manager);
                if (rv < 0) {
                    free(result);
                    free(ret);
                    rv = -1;
                    break;
                }
            });

        break;
    }

    case NEU_REQRESP_SET_NODE_SETTING: {
        ADAPTER_RESP_CODE(
            adapter, cmd, intptr_t, neu_cmd_set_node_setting_t, rv,
            NEU_REQRESP_ERR_CODE, p_result, {
                ret = neu_manager_node_setting(
                    adapter->manager, req_cmd->node_name, req_cmd->setting);
                if (ret == 0) {
                    nng_msg *           out_msg = NULL;
                    msg_node_setting_t *add = (msg_node_setting_t *) msg_new(
                        &out_msg, MSG_NODE_SETTING, sizeof(msg_node_setting_t));

                    strcpy(add->node, req_cmd->node_name);
                    add->setting = strdup(req_cmd->setting);

                    nng_sendmsg(adapter->nng.sock, out_msg, 0);
                }
            });
        break;
    }

    case NEU_REQRESP_GET_NODE_SETTING: {
        ADAPTER_RESP_CMD(
            adapter, cmd, neu_reqresp_node_setting_t,
            neu_cmd_get_node_setting_t, rv, NEU_REQRESP_GET_NODE_SETTING_RESP,
            p_result, {
                ret         = calloc(1, sizeof(neu_reqresp_node_setting_t));
                ret->result = neu_manager_node_get_setting(
                    adapter->manager, req_cmd->node_name, &ret->setting);
            });

        break;
    }

    case NEU_REQRESP_GET_NODE_STATE: {
        ADAPTER_RESP_CMD(
            adapter, cmd, neu_reqresp_node_state_t, neu_cmd_get_node_state_t,
            rv, NEU_REQRESP_NODE_STATE, p_result, {
                ret         = calloc(1, sizeof(neu_reqresp_node_state_t));
                ret->result = neu_manager_get_node_state(
                    adapter->manager, req_cmd->node_name, &ret->state);
            });
        break;
    }

    case NEU_REQRESP_NODE_CTL: {
        ADAPTER_RESP_CODE(
            adapter, cmd, intptr_t, neu_cmd_node_ctl_t, rv,
            NEU_REQRESP_ERR_CODE, p_result, {
                ret = neu_manager_node_ctl(adapter->manager, req_cmd->node_name,
                                           req_cmd->ctl);
                if (0 == ret) {
                    nng_msg *       out_msg = NULL;
                    msg_add_node_t *add     = (msg_add_node_t *) msg_new(
                        &out_msg, MSG_NODE_ADD, sizeof(msg_add_node_t));
                    strcpy(add->node, req_cmd->node_name);
                    nng_sendmsg(adapter->nng.sock, out_msg, 0);
                }
            });
        break;
    }

    case NEU_REQRESP_GET_SUB_GRP_CONFIGS: {
        ADAPTER_RESP_CMD(
            adapter, cmd, neu_reqresp_sub_grp_configs_t,
            neu_cmd_get_sub_grp_configs_t, rv, NEU_REQRESP_SUB_GRP_CONFIGS_RESP,
            p_result, {
                ret         = calloc(1, sizeof(neu_reqresp_sub_grp_configs_t));
                ret->groups = neu_manager_get_sub_group(adapter->manager,
                                                        req_cmd->node_name);
            });
        break;
    }

    default:
        rv = -1;
        break;
    }

    return rv;
}

static int adapter_response(neu_adapter_t *adapter, neu_response_t *resp)
{
    int rv = 0;

    if (adapter == NULL || resp == NULL) {
        nlog_warn("The adapter or response is NULL");
        return (-1);
    }

    nlog_info("Get response from plugin %s, type: %d", adapter->name,
              resp->resp_type);
    switch (resp->resp_type) {
    case NEU_REQRESP_TRANS_DATA: {
        size_t              msg_size = 0;
        nng_msg *           trans_data_msg;
        neu_reqresp_data_t *neu_data;

        neu_data = (neu_reqresp_data_t *) resp->buf;
        msg_size = sizeof(msg_trans_data_t) +
            neu_data->n_data * sizeof(neu_tag_data_t);
        msg_trans_data_t *trans_data = (msg_trans_data_t *) msg_new(
            &trans_data_msg, MSG_TRANS_DATA, msg_size);
        memcpy(&trans_data->data, neu_data,
               sizeof(neu_reqresp_data_t) +
                   neu_data->n_data * sizeof(neu_tag_data_t));

        nng_sendmsg(adapter->nng.sock, trans_data_msg, 0);
        break;
    }

    case NEU_REQRESP_READ_RESP: {
        nng_msg *                out_msg;
        neu_reqresp_read_resp_t *read_resp =
            (neu_reqresp_read_resp_t *) resp->buf;
        msg_group_read_resp_t *mresp = (msg_group_read_resp_t *) msg_new(
            &out_msg, MSG_GROUP_READ_RESP,
            sizeof(msg_group_read_resp_t) +
                read_resp->n_data * sizeof(neu_tag_data_t));

        strcpy(mresp->reader, resp->node_name);
        memcpy(&mresp->data, read_resp,
               sizeof(neu_reqresp_read_resp_t) +
                   read_resp->n_data * sizeof(neu_tag_data_t));

        nng_sendmsg(adapter->nng.sock, out_msg, 0);
        break;
    }
    case NEU_REQRESP_WRITE_RESP: {
        nng_msg *                 out_msg;
        neu_reqresp_write_resp_t *write_resp =
            (neu_reqresp_write_resp_t *) resp->buf;
        msg_tag_write_resp_t *mresp = (msg_tag_write_resp_t *) msg_new(
            &out_msg, MSG_TAG_WRITE_RESP, sizeof(msg_tag_write_resp_t));

        strcpy(mresp->writer, resp->node_name);
        mresp->ctx   = write_resp->ctx;
        mresp->error = write_resp->error;

        nng_sendmsg(adapter->nng.sock, out_msg, 0);
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

    if (adapter == NULL || event == NULL) {
        nlog_warn("The adapter or event is NULL");
        return (-1);
    }

    nlog_info("Get event notify from plugin");
    switch (event->type) {
    case NEU_EVENT_ADD_TAGS:
    case NEU_EVENT_DEL_TAGS:
    case NEU_EVENT_UPDATE_TAGS: {
        neu_event_tags_t *tags_event = (neu_event_tags_t *) event->buf;
        nng_msg *         out_msg    = NULL;
        msg_add_tag_t *   cmd = (msg_add_tag_t *) msg_new(&out_msg, MSG_TAG_ADD,
                                                       sizeof(msg_add_tag_t));
        strcpy(cmd->driver, tags_event->node_name);
        strcpy(cmd->group, tags_event->group_name);
        nng_sendmsg(adapter->nng.sock, out_msg, 0);
        break;
    }

    case NEU_EVENT_UPDATE_LICENSE: {
        nng_msg *             out_msg = NULL;
        msg_license_update_t *cmd     = (msg_license_update_t *) msg_new(
            &out_msg, MSG_UPDATE_LICENSE, sizeof(msg_license_update_t));
        (void) cmd;

        nng_sendmsg(adapter->nng.sock, out_msg, 0);
        break;
    }

    default:
        break;
    }

    return rv;
}

// clang-format off
static const adapter_callbacks_t callback_funs = {
    .command      = adapter_command,
    .response     = adapter_response,
    .event_notify = adapter_event_notify
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
    neu_adapter_t *adapter = (neu_adapter_t *) usr_data;
    int            rv      = 0;
    nng_msg *      msg     = NULL;
    msg_header_t * header  = NULL;

    switch (type) {
    case NEU_EVENT_IO_READ:
        rv = nng_recvmsg(adapter->nng.sock, &msg, 0);
        if (rv == NNG_ECLOSED) {
            nlog_warn("nng socket closed.");
            return 0;
        }
        header = (msg_header_t *) nng_msg_body(msg);

        switch (header->type) {
        case MSG_PERSISTENCE_LOAD:
            persister_singleton_load_data(adapter);
            break;
        case MSG_TRANS_DATA: {
            const neu_plugin_intf_funs_t *intf_funs =
                adapter->plugin_info.module->intf_funs;
            msg_trans_data_t *trans_data = (msg_trans_data_t *) header;

            neu_request_t req = {
                .req_id    = 0,
                .req_type  = NEU_REQRESP_TRANS_DATA,
                .buf_len   = sizeof(neu_reqresp_data_t),
                .buf       = (void *) &trans_data->data,
                .node_name = trans_data->data.driver,
            };

            intf_funs->request(adapter->plugin_info.plugin, &req);
            break;
        }
        case MSG_GROUP_SUBSCRIBE:
        case MSG_GROUP_UNSUBSCRIBE: {
            msg_group_subscribe_t *cmd = (msg_group_subscribe_t *) header;

            persister_singleton_handle_subscriptions(adapter, cmd->app);
            break;
        }
        case MSG_NODE_ADD:
        case MSG_NODE_DEL: {
            msg_add_node_t *cmd = (msg_add_node_t *) header;
            persister_singleton_handle_nodes(adapter, header->type, cmd->node);
            break;
        }
        case MSG_NODE_SETTING: {
            msg_node_setting_t *cmd       = (msg_node_setting_t *) header;
            neu_persister_t *   persister = persister_singleton_get();
            neu_persister_store_adapter_setting(persister, cmd->node,
                                                cmd->setting);
            free(cmd->setting);
            break;
        }
        case MSG_GROUP_ADD: {
            msg_add_group_t *cmd = (msg_add_group_t *) header;

            persister_singleton_handle_grp_config(
                adapter, header->type, cmd->driver, cmd->group, cmd->interval);
            break;
        }
        case MSG_GROUP_DEL: {
            msg_del_group_t *cmd = (msg_del_group_t *) header;
            persister_singleton_handle_grp_config(adapter, header->type,
                                                  cmd->driver, cmd->group, 0);
            break;
        }
        case MSG_TAG_ADD:
        case MSG_TAG_DEL:
        case MSG_TAG_UPDATE: {
            msg_add_tag_t *cmd = (msg_add_tag_t *) header;
            persister_singleton_handle_datatags(adapter, cmd->driver,
                                                cmd->group);
            break;
        }
        case MSG_PLUGIN_ADD:
        case MSG_PLUGIN_DEL: {
            persister_singleton_handle_plugins(adapter, header->type);
            break;
        }
        case MSG_UPDATE_LICENSE: {
            const neu_plugin_intf_funs_t *intf_funs = NULL;
            neu_request_t                 req       = { 0 };

            intf_funs    = adapter->plugin_info.module->intf_funs;
            req.req_id   = adapter_get_req_id(adapter);
            req.req_type = NEU_REQRESP_UPDATE_LICENSE;
            req.buf_len  = 0;
            req.buf      = NULL;
            intf_funs->request(adapter->plugin_info.plugin, &req);
            break;
        }

        case MSG_GROUP_READ: {
            msg_group_read_t * cmd      = (msg_group_read_t *) header;
            neu_request_t      req      = { 0 };
            neu_reqresp_read_t read_req = { 0 };

            req.req_type  = NEU_REQRESP_READ_DATA;
            req.node_name = cmd->reader;
            req.ctx       = cmd->ctx;
            req.buf_len   = sizeof(neu_reqresp_read_t);
            req.buf       = (void *) &read_req;

            read_req.ctx = cmd->ctx;
            strcpy(read_req.driver, cmd->driver);
            strcpy(read_req.group, cmd->group);

            if (adapter->plugin_info.module->type == NEU_NA_TYPE_DRIVER) {
                neu_adapter_driver_process_msg((neu_adapter_driver_t *) adapter,
                                               &req);
            } else {
                assert(false);
            }
            break;
        }
        case MSG_GROUP_READ_RESP: {
            const neu_plugin_intf_funs_t *intf_funs =
                adapter->plugin_info.module->intf_funs;
            msg_group_read_resp_t *cmd = (msg_group_read_resp_t *) header;
            neu_request_t          req = {
                .req_id    = 0,
                .req_type  = NEU_REQRESP_READ_RESP,
                .buf_len   = sizeof(neu_reqresp_read_resp_t),
                .buf       = (void *) &cmd->data,
                .node_name = cmd->data.driver,
            };

            intf_funs->request(adapter->plugin_info.plugin, &req);
            break;
        }
        case MSG_TAG_WRITE: {
            msg_tag_write_t *   cmd       = (msg_tag_write_t *) header;
            neu_request_t       req       = { 0 };
            neu_reqresp_write_t write_req = { 0 };

            req.req_type  = NEU_REQRESP_WRITE_DATA;
            req.node_name = cmd->writer;
            req.ctx       = cmd->ctx;
            req.buf_len   = sizeof(neu_reqresp_write_t);
            req.buf       = (void *) &write_req;

            write_req.ctx = cmd->ctx;
            strcpy(write_req.driver, cmd->driver);
            strcpy(write_req.group, cmd->group);
            strcpy(write_req.tag, cmd->tag);
            write_req.value = cmd->value;

            if (adapter->plugin_info.module->type == NEU_NA_TYPE_DRIVER) {
                neu_adapter_driver_process_msg((neu_adapter_driver_t *) adapter,
                                               &req);
            } else {
                assert(false);
            }
            break;
        }
        case MSG_TAG_WRITE_RESP: {
            const neu_plugin_intf_funs_t *intf_funs =
                adapter->plugin_info.module->intf_funs;
            msg_tag_write_resp_t *   cmd  = (msg_tag_write_resp_t *) header;
            neu_reqresp_write_resp_t resp = { 0 };
            neu_request_t            req  = {
                .req_id    = 0,
                .req_type  = NEU_REQRESP_WRITE_RESP,
                .buf_len   = sizeof(neu_reqresp_write_resp_t),
                .buf       = (void *) &resp,
                .node_name = cmd->writer,
            };

            resp.ctx   = cmd->ctx;
            resp.error = cmd->error;

            intf_funs->request(adapter->plugin_info.plugin, &req);
            break;
        }
        default:
            nlog_warn("Receive a not supported message(type: %d)",
                      header->type);
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

    nng_socket_get_int(adapter->nng.sock, NNG_OPT_RECVFD, &adapter->recv_fd);
    param.fd = adapter->recv_fd;

    adapter->nng_io = neu_event_add_io(adapter->events, param);

    if (adapter->plugin_info.module->type == NEU_NA_TYPE_DRIVER) {
        neu_adapter_driver_init((neu_adapter_driver_t *) adapter);
    }

    rv = nng_dial(adapter->nng.sock, neu_manager_get_url(adapter->manager),
                  &adapter->nng.dialer, 0);
    assert(rv == 0);

    nng_msg *        out_msg;
    msg_node_init_t *ping =
        msg_new(&out_msg, MSG_NODE_INIT, sizeof(msg_node_init_t));

    strcpy(ping->node, adapter->name);
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

    if (adapter->plugin_info.module->type == NEU_NA_TYPE_DRIVER) {
        neu_adapter_driver_uninit((neu_adapter_driver_t *) adapter);
    }
    adapter->plugin_info.module->intf_funs->uninit(adapter->plugin_info.plugin);

    neu_event_del_io(adapter->events, adapter->nng_io);

    nng_dialer_close(adapter->nng.dialer);
    nng_close(adapter->nng.sock);

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

neu_adapter_id_t neu_adapter_get_id(neu_adapter_t *adapter)
{
    if (adapter == NULL) {
        return 0;
    }

    return adapter->id;
}

// NOTE: Do NOT expose this function in header files, use for persistence
// only!
neu_adapter_id_t neu_adapter_set_id(neu_adapter_t *adapter, neu_adapter_id_t id)
{
    if (adapter == NULL) {
        return 0;
    }

    neu_adapter_id_t old = adapter->id;
    adapter->id          = id;
    return old;
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

    error = intf_funs->validate_tag(adapter->plugin_info.plugin, tag);

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
