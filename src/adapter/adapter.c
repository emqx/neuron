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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <nng/nng.h>
#include <nng/protocol/pair1/pair.h>
#include <nng/supplemental/util/platform.h>

#include "adapter.h"
#include "adapter_internal.h"
#include "config.h"
#include "core/message.h"
#include "core/neu_manager.h"
#include "core/neu_trans_buf.h"
#include "core/plugin_manager.h"
#include "driver/driver_internal.h"
#include "log.h"
#include "panic.h"
#include "persist/persist.h"
#include "plugin.h"
#include "plugin_info.h"
#include "subscribe.h"
#include "tag_group_config.h"

#define to_node_id(adapter, id) \
    neu_manager_adapter_id_to_node_id((adapter)->manager, id);

#define ADAPTER_SEND_MSG(adapter, cmd, rv, msg_type, cmd_type, reqresp_type, \
                         func)                                               \
    {                                                                        \
        size_t   msg_size = 0;                                               \
        nng_msg *msg      = NULL;                                            \
        msg_size          = msg_inplace_data_get_size(sizeof(cmd_type));     \
        (rv)              = nng_msg_alloc(&msg, msg_size);                   \
        if ((rv) == 0) {                                                     \
            message_t *   msg_ptr;                                           \
            cmd_type *    cmd_ptr;                                           \
            reqresp_type *reqresp_cmd;                                       \
            assert((cmd)->buf_len == sizeof(reqresp_type));                  \
            reqresp_cmd = (reqresp_type *) (cmd)->buf;                       \
            msg_ptr     = (message_t *) nng_msg_body(msg);                   \
            msg_inplace_data_init(msg_ptr, (msg_type), sizeof(cmd_type));    \
            cmd_ptr              = (cmd_type *) msg_get_buf_ptr(msg_ptr);    \
            cmd_ptr->sender_id   = to_node_id((adapter), (adapter)->id);     \
            cmd_ptr->dst_node_id = reqresp_cmd->dst_node_id;                 \
            cmd_ptr->grp_config  = reqresp_cmd->grp_config;                  \
            cmd_ptr->req_id      = cmd->req_id;                              \
            { func };                                                        \
            nng_sendmsg((adapter)->nng.sock, msg, 0);                        \
        }                                                                    \
    }

#define ADAPTER_SEND_BUF(adapter, rv, msg_type, src, len)      \
    {                                                          \
        size_t   msg_size = 0;                                 \
        nng_msg *msg      = NULL;                              \
        msg_size          = msg_inplace_data_get_size(len);    \
        (rv)              = nng_msg_alloc(&msg, msg_size);     \
        if ((rv) == 0) {                                       \
            message_t *msg_ptr;                                \
            msg_ptr = (message_t *) nng_msg_body(msg);         \
            msg_inplace_data_init(msg_ptr, (msg_type), (len)); \
            char *dst = msg_get_buf_ptr(msg_ptr);              \
            memcpy(dst, (src), len);                           \
            nng_sendmsg((adapter)->nng.sock, msg, 0);          \
        }                                                      \
    }

#define ADAPTER_SEND_NODE_EVENT(adapter, rv, msg_type, name) \
    {                                                        \
        size_t len = strlen(name) + 1;                       \
        ADAPTER_SEND_BUF(adapter, rv, msg_type, name, len);  \
    }

#define ADAPTER_SEND_PLUGIN_EVENT(adapter, rv, msg_type) \
    {                                                    \
        ADAPTER_SEND_BUF(adapter, rv, msg_type, "", 0);  \
    }

#define _ADAPTER_RESP(adapter, cmd, ret_type, req_type, rv, resp_type_code, \
                      p_result, func)                                       \
    {                                                                       \
        neu_response_t *result;                                             \
        assert((cmd)->buf_len == sizeof(req_type));                         \
        result = malloc(sizeof(neu_response_t));                            \
        if (result == NULL) {                                               \
            log_error("Failed to allocate result for resp cmd");            \
            rv = -1;                                                        \
        } else {                                                            \
            req_type *req_cmd = (req_type *) (cmd)->buf;                    \
            { func };                                                       \
            result->resp_type = (resp_type_code);                           \
            result->req_id    = (cmd)->req_id;                              \
            result->recver_id = to_node_id((adapter), (adapter)->id);       \
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
        log_error("can't create persister");
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
    vector_t *       plugin_infos = NULL;
    neu_persister_t *persister    = persister_singleton_get();

    int rv = neu_persister_load_plugins(persister, &plugin_infos);
    if (0 != rv) {
        log_error("%s failed to load plugin infos", adapter->name);
        return -1;
    }
    VECTOR_FOR_EACH(plugin_infos, iter)
    {
        char **                  plugin_info    = (char **) iterator_get(&iter);
        plugin_id_t              plugin_id      = {};
        neu_cmd_add_plugin_lib_t add_plugin_cmd = {
            .plugin_lib_name = *plugin_info,
        };

        rv = neu_manager_add_plugin_lib(adapter->manager, &add_plugin_cmd,
                                        &plugin_id);
        const char *ok_or_err = (0 == rv) ? "success" : "fail";
        log_info("%s load plugin %s , lib:%s", adapter->name, ok_or_err,
                 *plugin_info);
        if (0 != rv) {
            break;
        }
    }

    neu_persist_plugin_infos_free(plugin_infos);
    return rv;
}

static int persister_singleton_load_setting(neu_adapter_t *adapter,
                                            const char *   adapter_name,
                                            neu_node_id_t  node_id)
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
        log_error("%s %s load setting of %s", adapter->name, fail_or_ignore,
                  adapter_name);
        return rv;
    }

    rv = neu_manager_adapter_set_setting(adapter->manager, node_id, setting);
    const char *ok_or_err = (0 == rv) ? "success" : "fail";
    log_info("%s %s set setting of %s %s", adapter->name, ok_or_err,
             adapter_name, setting);

    free((char *) setting);

    return rv;
}

static int persister_singleton_load_datatags(neu_adapter_t *      adapter,
                                             const char *         adapter_name,
                                             neu_taggrp_config_t *grp_config,
                                             neu_datatag_table_t *tag_tbl)
{
    neu_persister_t *persister = persister_singleton_get();

    const char *grp_config_name = neu_taggrp_cfg_get_name(grp_config);
    vector_t *  datatag_infos   = NULL;
    int         rv = neu_persister_load_datatags(persister, adapter_name,
                                         grp_config_name, &datatag_infos);
    if (0 != rv) {
        const char *fail_or_ignore = "fail";
        if (NEU_ERR_ENOENT == rv) {
            // ignore no datatags
            rv             = 0;
            fail_or_ignore = "ignore";
        }
        log_error("%s %s load datatags of adapter:%s grp:%s", adapter->name,
                  fail_or_ignore, adapter_name, grp_config_name);
        return rv;
    }

    vector_t *ids = neu_taggrp_cfg_get_datatag_ids(grp_config);
    VECTOR_FOR_EACH(datatag_infos, iter)
    {
        neu_persist_datatag_info_t *p   = iterator_get(&iter);
        neu_datatag_t *             tag = neu_datatag_alloc_with_id(
            p->attribute, p->type, p->address, p->name, p->id);
        if (NULL == tag) {
            rv = NEU_ERR_ENOMEM;
            break;
        }

        if (neu_datatag_tbl_add(tag_tbl, tag)) {
            if (0 != vector_push_back(ids, &tag->id)) {
                rv = NEU_ERR_ENOMEM;
                break;
            }
        } else {
            neu_datatag_free(tag);
        }
    }

    neu_persist_datatag_infos_free(datatag_infos);

    return rv;
}

static int persister_singleton_load_grp_and_tags(neu_adapter_t *adapter,
                                                 const char *   adapter_name,
                                                 neu_node_id_t  node_id)
{
    vector_t *       group_config_infos = NULL;
    neu_persister_t *persister          = persister_singleton_get();

    neu_datatag_table_t *tag_tbl =
        neu_manager_get_datatag_tbl(adapter->manager, node_id);
    if (NULL == tag_tbl) {
        log_error("%s fail get datatag table of %s", adapter->name,
                  adapter_name);
        return NEU_ERR_EINTERNAL;
    }

    int rv = neu_persister_load_group_configs(persister, adapter_name,
                                              &group_config_infos);
    if (0 != rv) {
        const char *fail_or_ignore = "fail";
        if (NEU_ERR_ENOENT == rv) {
            // ignore no group configs
            rv             = 0;
            fail_or_ignore = "ignore";
        }
        log_error("%s %s load grp config of %s", adapter->name, fail_or_ignore,
                  adapter_name);
        return rv;
    }

    VECTOR_FOR_EACH(group_config_infos, iter)
    {
        neu_persist_group_config_info_t *p = iterator_get(&iter);

        neu_taggrp_config_t *grp_config =
            neu_taggrp_cfg_new(p->group_config_name);
        if (NULL == grp_config) {
            rv = NEU_ERR_ENOMEM;
            break;
        }
        neu_taggrp_cfg_set_interval(grp_config, p->read_interval);

        rv = persister_singleton_load_datatags(adapter, adapter_name,
                                               grp_config, tag_tbl);
        if (0 != rv) {
            neu_taggrp_cfg_free(grp_config);
            break;
        }

        neu_cmd_add_grp_config_t cmd = {
            .node_id    = node_id,
            .grp_config = grp_config,
        };

        rv = neu_manager_add_grp_config(adapter->manager, &cmd);
        const char *ok_or_err = (0 == rv) ? "success" : "fail";
        log_info("%s %s load group config %s interval:%d", adapter->name,
                 ok_or_err, p->group_config_name, p->read_interval);
        if (0 != rv) {
            neu_taggrp_cfg_free(grp_config);
            break;
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
    vector_t *       sub_infos = NULL;

    neu_node_id_t dst_node_id = 0;
    rv = neu_manager_get_node_id_by_name(adapter->manager, adapter_name,
                                         &dst_node_id);
    if (0 != rv) {
        log_error("%s fail get node id by name: %s", adapter->name,
                  adapter_name);
        return rv;
    }

    rv = neu_persister_load_subscriptions(persister, adapter_name, &sub_infos);
    if (0 != rv) {
        const char *fail_or_ignore = "fail";
        if (NEU_ERR_ENOENT == rv) {
            // ignore no subscriptions
            rv             = 0;
            fail_or_ignore = "ignore";
        }
        log_error("%s %s load subscription infos of %s", adapter->name,
                  fail_or_ignore, adapter_name);
        return rv;
    }

    VECTOR_FOR_EACH(sub_infos, iter)
    {
        neu_persist_subscription_info_t *p = iterator_get(&iter);

        neu_node_id_t src_node_id = 0;
        rv = neu_manager_get_node_id_by_name(adapter->manager,
                                             p->src_adapter_name, &src_node_id);
        if (0 != rv) {
            log_error("%s fail get node id by name: %s", adapter->name,
                      p->src_adapter_name);
            break;
        }

        neu_taggrp_config_t *grp_config = NULL;
        rv = neu_manager_adapter_get_grp_config_ref_by_name(
            adapter->manager, src_node_id, p->group_config_name, &grp_config);
        if (0 != rv) {
            break;
        }

        subscribe_node_cmd_t cmd = {
            .grp_config  = grp_config,
            .dst_node_id = dst_node_id,
            .src_node_id = src_node_id,
            .sender_id   = adapter->id,
            .req_id      = adapter_get_req_id(adapter),
        };
        rv = neu_manager_subscribe_node(adapter->manager, &cmd);
        const char *ok_or_err = (0 == rv) ? "success" : "fail";
        log_info("%s %s load subscription dst:%s src:%s grp:%s", adapter->name,
                 ok_or_err, adapter_name, p->src_adapter_name,
                 p->group_config_name);

        neu_taggrp_cfg_free((neu_taggrp_config_t *) grp_config);
    }

    neu_persist_subscription_infos_free(sub_infos);

    return rv;
}

static int persist_adapter_info_cmp(const void *a, const void *b)
{
    return ((neu_persist_adapter_info_t *) a)->id -
        ((neu_persist_adapter_info_t *) b)->id;
}

static int persister_singleton_load_data(neu_adapter_t *adapter)
{
    vector_t *       adapter_infos = NULL;
    neu_persister_t *persister     = persister_singleton_get();

    // declaration for neu_manager_update_node_id since it is not exposed
    int neu_manager_update_node_id(neu_manager_t * manager,
                                   neu_node_id_t node_id,
                                   neu_node_id_t new_node_id);

    log_info("%s start persistence loading", adapter->name);

    int rv = persister_singleton_load_plugins(adapter);
    if (0 != rv) {
        return rv;
    }

    rv = neu_persister_load_adapters(persister, &adapter_infos);
    if (0 != rv) {
        log_error("%s failed to load adapter infos", adapter->name);
        goto error_load_adapters;
    }

    // sort by adapter id
    qsort(adapter_infos->data, adapter_infos->size,
          sizeof(neu_persist_adapter_info_t), persist_adapter_info_cmp);

    VECTOR_FOR_EACH(adapter_infos, iter)
    {
        neu_persist_adapter_info_t *adapter_info = iterator_get(&iter);
        neu_cmd_add_node_t          add_node_cmd = {
            .node_type    = adapter_info->type,
            .adapter_name = adapter_info->name,
            .plugin_name  = adapter_info->plugin_name,
        };

        neu_node_id_t node_id = 0;
        rv = neu_manager_add_node(adapter->manager, &add_node_cmd, &node_id);
        const char *ok_or_err = (0 == rv) ? "success" : "fail";
        log_info("%s load adapter %s type:%d, name:%s plugin:%s", adapter->name,
                 ok_or_err, adapter_info->type, adapter_info->name,
                 adapter_info->plugin_name);
        if (0 != rv) {
            goto error_add_adapters;
        }

        if (node_id != adapter_info->id) {
            // set to persisted adapter id
            neu_manager_update_node_id(adapter->manager, node_id,
                                       adapter_info->id);
            node_id = adapter_info->id;
        }

        rv = persister_singleton_load_setting(adapter, adapter_info->name,
                                              node_id);
        if (0 != rv) {
            goto error_add_adapters;
        }

        rv = persister_singleton_load_grp_and_tags(adapter, adapter_info->name,
                                                   node_id);
        if (0 != rv) {
            goto error_add_adapters;
        }

        if (ADAPTER_STATE_RUNNING == adapter_info->state) {
            rv = neu_manager_start_adapter_with_id(adapter->manager, node_id);
            if (0 != rv) {
                log_error("%s fail start adapter %s", adapter->name,
                          adapter_info->name);
                goto error_add_adapters;
            }
        }
    }

    VECTOR_FOR_EACH(adapter_infos, iter)
    {
        neu_persist_adapter_info_t *adapter_info = iterator_get(&iter);
        rv =
            persister_singleton_load_subscriptions(adapter, adapter_info->name);
        if (0 != rv) {
            break;
        }
    }

error_add_adapters:
    neu_persist_adapter_infos_free(adapter_infos);
error_load_adapters:
    return rv;
}

static int persister_singleton_handle_nodes(neu_adapter_t *adapter,
                                            msg_type_e     event,
                                            const char *   node_name)
{
    neu_persister_t *persister     = persister_singleton_get();
    vector_t *       adapter_infos = NULL;

    log_info("%s handling node event %d of %s", adapter->name, event,
             node_name);

    int rv =
        neu_manager_get_persist_adapter_infos(adapter->manager, &adapter_infos);
    if (0 != rv) {
        log_error("%s unable to get adapter infos", adapter->name);
        return rv;
    }

    // store the current set of adapter infos
    rv = neu_persister_store_adapters(persister, adapter_infos);
    if (0 != rv) {
        log_error("%s failed to store adapter infos", adapter->name);
    } else if (MSG_EVENT_DEL_NODE == event) {
        rv = neu_persister_delete_adapter(persister, node_name);
        if (0 != rv) {
            log_error("%s failed to del adapter %s", adapter->name, node_name);
        }
    }

    neu_persist_adapter_infos_free(adapter_infos);

    return rv;
}

static int
persister_singleton_handle_update_node(neu_adapter_t *          adapter,
                                       neu_event_update_node_t *event)
{
    neu_persister_t *persister     = persister_singleton_get();
    vector_t *       adapter_infos = NULL;

    log_info("%s handling update node event of %s", adapter->name,
             event->src_name);

    int rv =
        neu_manager_get_persist_adapter_infos(adapter->manager, &adapter_infos);
    if (0 != rv) {
        log_error("%s unable to get adapter infos", adapter->name);
        return rv;
    }

    // store the current set of adapter infos
    rv = neu_persister_store_adapters(persister, adapter_infos);
    if (0 == rv) {
        rv = neu_persister_update_adapter(persister, event->src_name,
                                          event->dst_name);
    } else {
        log_error("%s failed to store adapter infos", adapter->name);
    }

    neu_persist_adapter_infos_free(adapter_infos);

    return rv;
}

static int persister_singleton_handle_plugins(neu_adapter_t *adapter,
                                              msg_type_e     event)
{
    neu_persister_t *persister    = persister_singleton_get();
    vector_t *       plugin_infos = NULL;

    log_info("%s handling plugin event %d", adapter->name, event);

    int rv =
        neu_manager_get_persist_plugin_infos(adapter->manager, &plugin_infos);
    if (0 != rv) {
        log_error("%s unable to get plugin infos", adapter->name);
        return rv;
    }

    // store the current set of plugin infos
    rv = neu_persister_store_plugins(persister, plugin_infos);
    if (0 != rv) {
        log_error("%s failed to store plugins infos", adapter->name);
    }

    neu_persist_plugin_infos_free(plugin_infos);
    return rv;
}

static int persister_singleton_handle_grp_config(neu_adapter_t *adapter,
                                                 msg_type_e     event,
                                                 neu_node_id_t  node_id,
                                                 void *         arg)
{
    neu_persister_t *persister    = persister_singleton_get();
    char *           adapter_name = NULL;

    log_info("%s handling grp config event %d", adapter->name, event);

    int rv = neu_manager_get_node_name_by_id(adapter->manager, node_id,
                                             &adapter_name);
    if (0 != rv) {
        return rv;
    }

    if (MSG_EVENT_DEL_GRP_CONFIG != event) {
        neu_taggrp_config_t *           grp_config = arg;
        neu_persist_group_config_info_t info       = {
            .group_config_name = (char *) neu_taggrp_cfg_get_name(grp_config),
            .read_interval     = neu_taggrp_cfg_get_interval(grp_config),
            .adapter_name      = adapter_name,
        };
        rv = neu_persister_store_group_config(persister, adapter_name, &info);
        if (0 != rv) {
            log_error("% failed to store group config infos", adapter->name);
        }
    } else {
        const char *group_config_name = arg;
        rv = neu_persister_delete_group_config(persister, adapter_name,
                                               group_config_name);
        if (0 != rv) {
            log_error("% failed to del group config infos", adapter->name);
        }
    }

    free(adapter_name);
    return rv;
}

static int persister_singleton_handle_datatags(neu_adapter_t *adapter,
                                               neu_node_id_t  node_id,
                                               const char *   grp_config_name)
{
    neu_persister_t *persister    = persister_singleton_get();
    char *           adapter_name = NULL;
    int rv = neu_manager_get_node_name_by_id(adapter->manager, node_id,
                                             &adapter_name);
    if (0 != rv) {
        return rv;
    }

    vector_t *datatag_infos = NULL;
    rv = neu_manager_get_persist_datatag_infos(adapter->manager, node_id,
                                               grp_config_name, &datatag_infos);
    if (0 != rv) {
        log_error("%s fail get persist datatag infos", adapter->name);
        free(adapter_name);
        return rv;
    }

    rv = neu_persister_store_datatags(persister, adapter_name, grp_config_name,
                                      datatag_infos);
    if (0 != rv) {
        log_error("%s fail store adapter:% grp:%s datatag infos", adapter_name,
                  grp_config_name);
    }

    neu_persist_datatag_infos_free(datatag_infos);
    free(adapter_name);
    return rv;
}

static int persister_singleton_handle_subscriptions(neu_adapter_t *adapter,
                                                    neu_node_id_t  node_id)
{
    neu_persister_t *persister    = persister_singleton_get();
    char *           adapter_name = NULL;
    int rv = neu_manager_get_node_name_by_id(adapter->manager, node_id,
                                             &adapter_name);
    if (0 != rv) {
        return rv;
    }

    vector_t *sub_infos = NULL;
    rv = neu_manager_get_persist_subscription_infos(adapter->manager, node_id,
                                                    &sub_infos);
    if (0 != rv) {
        log_error("%s fail get persist subscription infos", adapter->name);
        free(adapter_name);
        return rv;
    }

    rv = neu_persister_store_subscriptions(persister, adapter_name, sub_infos);
    if (0 != rv) {
        log_error("%s fail store adapter:%s subscription infos", adapter_name,
                  adapter->name);
    }

    neu_persist_subscription_infos_free(sub_infos);
    free(adapter_name);

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
        log_warn("The adapter or command is NULL");
        return (-1);
    }

    if (adapter->state == ADAPTER_STATE_IDLE) {
        log_warn("The adapter loop not running");
        return -1;
    }

    log_trace("Get command from plugin %d, %s", cmd->req_type, adapter->name);
    switch (cmd->req_type) {
    case NEU_REQRESP_SUBSCRIBE_NODE: {
        ADAPTER_SEND_MSG(adapter, cmd, rv, MSG_CMD_SUBSCRIBE_NODE,
                         subscribe_node_cmd_t, neu_reqresp_subscribe_node_t,
                         { cmd_ptr->src_node_id = reqresp_cmd->src_node_id; });
        break;
    }

    case NEU_REQRESP_UNSUBSCRIBE_NODE: {
        ADAPTER_SEND_MSG(adapter, cmd, rv, MSG_CMD_UNSUBSCRIBE_NODE,
                         unsubscribe_node_cmd_t, neu_reqresp_unsubscribe_node_t,
                         { cmd_ptr->src_node_id = reqresp_cmd->src_node_id; });
        break;
    }

    case NEU_REQRESP_READ_DATA: {
        ADAPTER_SEND_MSG(adapter, cmd, rv, MSG_CMD_READ_DATA, read_data_cmd_t,
                         neu_reqresp_read_t, {});
        break;
    }

    case NEU_REQRESP_WRITE_DATA: {
        ADAPTER_SEND_MSG(adapter, cmd, rv, MSG_CMD_WRITE_DATA, write_data_cmd_t,
                         neu_reqresp_write_t, {
                             neu_trans_buf_t *trans_buf;

                             trans_buf = &cmd_ptr->trans_buf;
                             neu_trans_buf_init(trans_buf, adapter->trans_kind,
                                                reqresp_cmd->data_val);
                         })
        break;
    }

    case NEU_REQRESP_ADD_NODE: {
        ADAPTER_RESP_CODE(
            adapter, cmd, intptr_t, neu_cmd_add_node_t, rv,
            NEU_REQRESP_ERR_CODE, p_result, {
                neu_node_id_t node_id;
                ret = neu_manager_add_node(adapter->manager, req_cmd, &node_id);
                if (0 == ret) {
                    ADAPTER_SEND_NODE_EVENT(adapter, rv, MSG_EVENT_ADD_NODE,
                                            req_cmd->adapter_name);
                }
            });
        break;
    }

    case NEU_REQRESP_DEL_NODE: {
        ADAPTER_RESP_CODE(
            adapter, cmd, intptr_t, neu_cmd_del_node_t, rv,
            NEU_REQRESP_ERR_CODE, p_result, {
                char *name = NULL;
                ret        = neu_manager_get_node_name_by_id(adapter->manager,
                                                      req_cmd->node_id, &name);
                if (0 == ret) {
                    ret = neu_manager_del_node(adapter->manager,
                                               req_cmd->node_id);
                    if (0 == ret) {
                        ADAPTER_SEND_NODE_EVENT(adapter, rv, MSG_EVENT_DEL_NODE,
                                                name);
                    }
                }
                free(name);
            });
        break;
    }

    case NEU_REQRESP_GET_NODES: {
        ADAPTER_RESP_CMD(
            adapter, cmd, neu_reqresp_nodes_t, neu_cmd_get_nodes_t, rv,
            NEU_REQRESP_NODES, p_result, {
                ret = malloc(sizeof(neu_reqresp_nodes_t));
                if (ret == NULL) {
                    log_error("Failed to allocate result of get nodes");
                    rv = NEU_ERR_EINTERNAL;
                    free(result);
                    break;
                }

                vector_init(&ret->nodes, 8, sizeof(neu_node_info_t));
                rv = neu_manager_get_nodes(adapter->manager, req_cmd->node_type,
                                           &ret->nodes);
                if (rv < 0) {
                    free(result);
                    free(ret);
                    break;
                }
            });

        break;
    }

    case NEU_REQRESP_ADD_GRP_CONFIG: {
        ADAPTER_RESP_CODE(
            adapter, cmd, intptr_t, neu_cmd_add_grp_config_t, rv,
            NEU_REQRESP_ERR_CODE, p_result, {
                neu_taggrp_cfg_ref(req_cmd->grp_config);
                ret = neu_manager_add_grp_config(adapter->manager, req_cmd);
                if (0 == ret) {
                    ADAPTER_SEND_BUF(adapter, rv, MSG_EVENT_ADD_GRP_CONFIG,
                                     req_cmd, sizeof(*req_cmd));
                }
            });
        break;
    }

    case NEU_REQRESP_DEL_GRP_CONFIG: {
        ADAPTER_RESP_CODE(
            adapter, cmd, intptr_t, neu_cmd_del_grp_config_t, rv,
            NEU_REQRESP_ERR_CODE, p_result, {
                ret = neu_manager_del_grp_config(
                    adapter->manager, req_cmd->node_id, req_cmd->config_name);
                if (0 == ret) {
                    neu_cmd_del_grp_config_t event = {};
                    event.node_id                  = req_cmd->node_id;
                    event.config_name = strdup(req_cmd->config_name);
                    if (NULL != event.config_name) {
                        ADAPTER_SEND_BUF(adapter, rv, MSG_EVENT_DEL_GRP_CONFIG,
                                         &event, sizeof(event));
                    }
                }
            });
        break;
    }

    case NEU_REQRESP_UPDATE_GRP_CONFIG: {
        ADAPTER_RESP_CODE(
            adapter, cmd, intptr_t, neu_cmd_update_grp_config_t, rv,
            NEU_REQRESP_ERR_CODE, p_result, {
                neu_taggrp_cfg_ref(req_cmd->grp_config);
                ret = neu_manager_update_grp_config(adapter->manager, req_cmd);
                if (0 == ret) {
                    ADAPTER_SEND_BUF(adapter, rv, MSG_EVENT_UPDATE_GRP_CONFIG,
                                     req_cmd, sizeof(*req_cmd));
                }
            });
        break;
    }

    case NEU_REQRESP_GET_GRP_CONFIGS: {
        ADAPTER_RESP_CMD(
            adapter, cmd, neu_reqresp_grp_configs_t, neu_cmd_get_grp_configs_t,
            rv, NEU_REQRESP_GRP_CONFIGS, p_result, {
                ret = malloc(sizeof(neu_reqresp_grp_configs_t));
                if (ret == NULL) {
                    log_error("Failed to allocate result of get grp configs");
                    rv = -1;
                    free(result);
                    break;
                }

                vector_init(&ret->grp_configs, DEFAULT_TAG_GROUP_COUNT,
                            sizeof(neu_taggrp_config_t *));
                rv = neu_manager_get_grp_configs(
                    adapter->manager, req_cmd->node_id, &ret->grp_configs);
                if (rv < 0) {
                    free(result);
                    free(ret);
                    rv = -1;
                    break;
                }
            });
        break;
    }

    case NEU_REQRESP_ADD_PLUGIN_LIB: {
        ADAPTER_RESP_CODE(adapter, cmd, intptr_t, neu_cmd_add_plugin_lib_t, rv,
                          NEU_REQRESP_ERR_CODE, p_result, {
                              plugin_id_t plugin_id;
                              ret = neu_manager_add_plugin_lib(
                                  adapter->manager, req_cmd, &plugin_id);
                              if (ret == NEU_ERR_SUCCESS) {
                                  ADAPTER_SEND_PLUGIN_EVENT(
                                      adapter, rv, MSG_EVENT_ADD_PLUGIN);
                              }
                          });
        break;
    }

    case NEU_REQRESP_DEL_PLUGIN_LIB: {
        ADAPTER_RESP_CODE(adapter, cmd, intptr_t, neu_cmd_del_plugin_lib_t, rv,
                          NEU_REQRESP_ERR_CODE, p_result, {
                              ret = NEU_ERR_SUCCESS;
                              rv  = neu_manager_del_plugin_lib(
                                  adapter->manager, req_cmd->plugin_id);
                              if (rv != 0) {
                                  ret = NEU_ERR_FAILURE;
                              } else {
                                  ADAPTER_SEND_PLUGIN_EVENT(
                                      adapter, rv, MSG_EVENT_DEL_PLUGIN);
                              }
                          });
        break;
    }

    case NEU_REQRESP_UPDATE_PLUGIN_LIB: {
        ADAPTER_RESP_CODE(
            adapter, cmd, intptr_t, neu_cmd_update_plugin_lib_t, rv,
            NEU_REQRESP_ERR_CODE, p_result, {
                ret = NEU_ERR_SUCCESS;
                rv  = neu_manager_update_plugin_lib(adapter->manager, req_cmd);
                if (rv != 0) {
                    ret = NEU_ERR_FAILURE;
                } else {
                    ADAPTER_SEND_PLUGIN_EVENT(adapter, rv,
                                              MSG_EVENT_UPDATE_PLUGIN);
                }
            });
        break;
    }

    case NEU_REQRESP_GET_PLUGIN_LIBS: {
        ADAPTER_RESP_CMD(
            adapter, cmd, neu_reqresp_plugin_libs_t, neu_cmd_get_plugin_libs_t,
            rv, NEU_REQRESP_PLUGIN_LIBS, p_result, {
                (void) req_cmd;
                ret = malloc(sizeof(neu_reqresp_plugin_libs_t));
                if (ret == NULL) {
                    log_error("Failed to allocate result of get nodes");
                    rv = -1;
                    free(result);
                    break;
                }

                vector_init(&ret->plugin_libs, 8, sizeof(plugin_lib_info_t));
                rv = neu_manager_get_plugin_libs(adapter->manager,
                                                 &ret->plugin_libs);
                if (rv < 0) {
                    free(result);
                    free(ret);
                    rv = -1;
                    break;
                }
            });

        break;
    }

    case NEU_REQRESP_GET_DATATAGS: {
        ADAPTER_RESP_CMD(
            adapter, cmd, neu_reqresp_datatags_t, neu_cmd_get_datatags_t, rv,
            NEU_REQRESP_DATATAGS, p_result, {
                ret = malloc(sizeof(neu_reqresp_datatags_t));
                if (ret == NULL) {
                    log_error("Failed to allocate result of get datatags");
                    rv = -1;
                    free(result);
                    break;
                }
                ret->datatag_tbl = neu_manager_get_datatag_tbl(
                    adapter->manager, req_cmd->node_id);

                if (ret->datatag_tbl == NULL) {
                    free(result);
                    free(ret);
                    rv = -1;
                    break;
                }
            });
        break;
    }

    case NEU_REQRESP_SELF_NODE_ID: {
        ADAPTER_RESP_CMD(
            adapter, cmd, neu_reqresp_node_id_t, neu_cmd_self_node_id_t, rv,
            NEU_REQRESP_NODE_ID, p_result, {
                (void) req_cmd;
                ret = malloc(sizeof(neu_reqresp_node_id_t));
                if (ret == NULL) {
                    log_error("Failed to allocate result of get node id");
                    rv = -1;
                    free(result);
                    break;
                }

                ret->node_id = neu_manager_adapter_id_to_node_id(
                    adapter->manager, adapter->id);
            });
        break;
    }

    case NEU_REQRESP_GET_NODE_ID_BY_NAME: {
        ADAPTER_RESP_CMD(
            adapter, cmd, neu_reqresp_node_id_t, neu_cmd_get_node_id_by_name_t,
            rv, NEU_REQRESP_NODE_ID, p_result, {
                ret     = calloc(1, sizeof(neu_reqresp_node_id_t));
                int err = neu_manager_get_node_id_by_name(
                    adapter->manager, req_cmd->name, &ret->node_id);
                (void) err;
            })
        break;
    }

    case NEU_REQRESP_SELF_NODE_NAME: {
        ADAPTER_RESP_CMD(
            adapter, cmd, neu_reqresp_node_name_t, neu_cmd_self_node_name_t, rv,
            NEU_REQRESP_NODE_NAME, p_result, {
                (void) req_cmd;
                ret = malloc(sizeof(neu_reqresp_node_name_t));
                if (ret == NULL) {
                    log_error("Failed to allocate result of get node name");
                    rv = -1;
                    free(result);
                    break;
                }

                ret->node_name = adapter->name;
            });
        break;
    }

    case NEU_REQRESP_SET_NODE_SETTING: {
        ADAPTER_RESP_CODE(
            adapter, cmd, intptr_t, neu_cmd_set_node_setting_t, rv,
            NEU_REQRESP_ERR_CODE, p_result, {
                ret = neu_manager_adapter_set_setting(
                    adapter->manager, req_cmd->node_id, req_cmd->setting);
                if (ret == 0) {
                    neu_cmd_set_node_setting_t event = {};
                    event.node_id                    = req_cmd->node_id;
                    event.setting                    = strdup(req_cmd->setting);
                    if (NULL != event.setting) {
                        ADAPTER_SEND_BUF(adapter, rv,
                                         MSG_EVENT_SET_NODE_SETTING, &event,
                                         sizeof(event));
                    }
                }
            });
        break;
    }

    case NEU_REQRESP_GET_NODE_SETTING: {
        ADAPTER_RESP_CMD(
            adapter, cmd, neu_reqresp_node_setting_t,
            neu_cmd_get_node_setting_t, rv, NEU_REQRESP_GET_NODE_SETTING_RESP,
            p_result, {
                ret = calloc(1, sizeof(neu_reqresp_node_setting_t));

                ret->result = neu_manager_adapter_get_setting(
                    adapter->manager, req_cmd->node_id, &ret->setting);
            });

        break;
    }

    case NEU_REQRESP_GET_NODE_STATE: {
        ADAPTER_RESP_CMD(
            adapter, cmd, neu_reqresp_node_state_t, neu_cmd_get_node_state_t,
            rv, NEU_REQRESP_NODE_STATE, p_result, {
                ret = calloc(1, sizeof(neu_reqresp_node_state_t));

                ret->result = neu_manager_adapter_get_state(
                    adapter->manager, req_cmd->node_id, &ret->state);
            });
        break;
    }

    case NEU_REQRESP_NODE_CTL: {
        ADAPTER_RESP_CODE(
            adapter, cmd, intptr_t, neu_cmd_node_ctl_t, rv,
            NEU_REQRESP_ERR_CODE, p_result, {
                ret = neu_manager_adapter_ctl(adapter->manager,
                                              req_cmd->node_id, req_cmd->ctl);
                if (0 == ret) {
                    char *node_name = NULL;
                    rv              = neu_manager_get_node_name_by_id(
                        adapter->manager, req_cmd->node_id, &node_name);
                    if (0 == rv) {
                        ADAPTER_SEND_NODE_EVENT(adapter, rv, MSG_EVENT_ADD_NODE,
                                                node_name);
                        free(node_name);
                    }
                }
            });
        break;
    }

    case NEU_REQRESP_GET_SUB_GRP_CONFIGS: {
        ADAPTER_RESP_CMD(
            adapter, cmd, neu_reqresp_sub_grp_configs_t,
            neu_cmd_get_sub_grp_configs_t, rv, NEU_REQRESP_SUB_GRP_CONFIGS_RESP,
            p_result, {
                ret = calloc(1, sizeof(neu_reqresp_sub_grp_configs_t));

                ret->result = neu_manager_adapter_get_sub_grp_configs(
                    adapter->manager, req_cmd->node_id, &ret->sub_grp_configs);
            });
        break;
    }

    case NEU_REQRESP_VALIDATE_TAG: {
        ADAPTER_RESP_CODE(adapter, cmd, intptr_t, neu_cmd_validate_tag_t, rv,
                          NEU_REQRESP_ERR_CODE, p_result, {
                              ret = neu_manager_adapter_validate_tag(
                                  adapter->manager, req_cmd->node_id,
                                  req_cmd->tag);
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
        log_warn("The adapter or response is NULL");
        return (-1);
    }

    log_info("Get response from plugin %s", adapter->name);
    switch (resp->resp_type) {
    case NEU_REQRESP_TRANS_DATA: {
        size_t              msg_size;
        nng_msg *           trans_data_msg;
        neu_reqresp_data_t *neu_data;

        assert(resp->buf_len == sizeof(neu_reqresp_data_t));
        neu_data = (neu_reqresp_data_t *) resp->buf;

        msg_size = msg_inplace_data_get_size(sizeof(neuron_trans_data_t));
        rv       = nng_msg_alloc(&trans_data_msg, msg_size);
        if (rv == 0) {
            message_t *          pay_msg;
            neuron_trans_data_t *trans_data;
            neu_trans_buf_t *    trans_buf;

            pay_msg = (message_t *) nng_msg_body(trans_data_msg);
            msg_inplace_data_init(pay_msg, MSG_DATA_NEURON_TRANS_DATA,
                                  sizeof(neuron_trans_data_t));
            trans_data = (neuron_trans_data_t *) msg_get_buf_ptr(pay_msg);
            trans_data->grp_config = neu_data->grp_config;
            trans_data->sender_id  = to_node_id((adapter), (adapter)->id);
            trans_data->node_name  = adapter->name;
            trans_buf              = &trans_data->trans_buf;
            rv = neu_trans_buf_init(trans_buf, adapter->trans_kind,
                                    neu_data->data_val);
            if (rv != 0) {
                nng_msg_free(trans_data_msg);
                break;
            }
            nng_sendmsg(adapter->nng.sock, trans_data_msg, 0);
        }
        break;
    }

    case NEU_REQRESP_READ_RESP: {
        size_t                   msg_size;
        nng_msg *                read_resp_msg;
        neu_reqresp_read_resp_t *read_resp;

        assert(resp->buf_len == sizeof(neu_reqresp_read_resp_t));
        read_resp = (neu_reqresp_read_resp_t *) resp->buf;

        msg_size = msg_inplace_data_get_size(sizeof(read_data_resp_t));
        rv       = nng_msg_alloc(&read_resp_msg, msg_size);
        if (rv == 0) {
            message_t *       pay_msg;
            read_data_resp_t *read_data_resp;
            neu_trans_buf_t * trans_buf;

            pay_msg = (message_t *) nng_msg_body(read_resp_msg);
            msg_inplace_data_init(pay_msg, MSG_DATA_READ_RESP,
                                  sizeof(read_data_resp_t));
            read_data_resp = (read_data_resp_t *) msg_get_buf_ptr(pay_msg);
            read_data_resp->grp_config = read_resp->grp_config;
            read_data_resp->recver_id  = resp->recver_id;
            read_data_resp->req_id     = resp->req_id;
            read_data_resp->sender_id  = to_node_id((adapter), (adapter)->id);
            trans_buf                  = &read_data_resp->trans_buf;
            rv = neu_trans_buf_init(trans_buf, adapter->trans_kind,
                                    read_resp->data_val);
            if (rv != 0) {
                nng_msg_free(read_resp_msg);
                break;
            }
            nng_sendmsg(adapter->nng.sock, read_resp_msg, 0);
        }
        break;
    }

    case NEU_REQRESP_WRITE_RESP: {
        size_t                    msg_size;
        nng_msg *                 write_resp_msg;
        neu_reqresp_write_resp_t *write_resp;

        assert(resp->buf_len == sizeof(neu_reqresp_write_resp_t));
        write_resp = (neu_reqresp_write_resp_t *) resp->buf;

        msg_size = msg_inplace_data_get_size(sizeof(write_data_resp_t));
        rv       = nng_msg_alloc(&write_resp_msg, msg_size);
        if (rv == 0) {
            message_t *        pay_msg;
            write_data_resp_t *write_data_resp;
            neu_trans_buf_t *  trans_buf;

            pay_msg = (message_t *) nng_msg_body(write_resp_msg);
            msg_inplace_data_init(pay_msg, MSG_DATA_WRITE_RESP,
                                  sizeof(write_data_resp_t));
            write_data_resp = (write_data_resp_t *) msg_get_buf_ptr(pay_msg);
            write_data_resp->grp_config = write_resp->grp_config;
            write_data_resp->recver_id  = resp->recver_id;
            write_data_resp->req_id     = resp->req_id;
            write_data_resp->sender_id  = to_node_id((adapter), (adapter)->id);
            trans_buf                   = &write_data_resp->trans_buf;
            rv = neu_trans_buf_init(trans_buf, adapter->trans_kind,
                                    write_resp->data_val);
            if (rv != 0) {
                nng_msg_free(write_resp_msg);
                break;
            }
            nng_sendmsg(adapter->nng.sock, write_resp_msg, 0);
        }
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
        log_warn("The adapter or event is NULL");
        return (-1);
    }

    log_info("Get event notify from plugin");
    switch (event->type) {
    case NEU_EVENT_ADD_TAGS: {
        ADAPTER_SEND_BUF(adapter, rv, MSG_EVENT_ADD_TAGS, event->buf,
                         event->buf_len);
        break;
    }

    case NEU_EVENT_UPDATE_TAGS: {
        ADAPTER_SEND_BUF(adapter, rv, MSG_EVENT_UPDATE_TAGS, event->buf,
                         event->buf_len);
        break;
    }

    case NEU_EVENT_DEL_TAGS: {
        ADAPTER_SEND_BUF(adapter, rv, MSG_EVENT_ADD_TAGS, event->buf,
                         event->buf_len);
        break;
    }

    case NEU_EVENT_UPDATE_LICENSE: {
        ADAPTER_SEND_BUF(adapter, rv, MSG_EVENT_UPDATE_LICENSE, event->buf,
                         event->buf_len);
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
    neu_adapter_t *adapter = NULL;
    int            rv      = 0;

    assert(manager != NULL);
    assert(info->plugin_lib_name != NULL);
    assert(info->name != NULL);

    adapter          = calloc(1, sizeof(neu_adapter_t));
    adapter->cb_funs = callback_funs;
    switch (info->type) {
    case ADAPTER_TYPE_DRIVERX:
        adapter = (neu_adapter_t *) neu_adapter_driver_create(adapter);
        break;
    default:
        break;
    }

    adapter->plugin_info.handle =
        load_plugin_library((char *) info->plugin_lib_name, info->plugin_kind,
                            &adapter->plugin_info.module);
    assert(adapter->plugin_info.handle != NULL);

    adapter->id               = info->id;
    adapter->name             = strdup(info->name);
    adapter->state            = ADAPTER_STATE_IDLE;
    adapter->req_id           = 1;
    adapter->plugin_info.id   = info->plugin_id;
    adapter->plugin_info.kind = info->plugin_kind;
    adapter->manager          = manager;
    adapter->trans_kind       = NEURON_TRANS_DATAVAL;

    rv = nng_mtx_alloc(&adapter->nng.mtx);
    assert(rv == 0);
    rv = nng_mtx_alloc(&adapter->nng.sub_grp_mtx);
    assert(rv == 0);

    vector_init(&adapter->sub_grp_configs, 0, sizeof(neu_sub_grp_config_t));

    adapter->plugin_info.plugin = adapter->plugin_info.module->intf_funs->open(
        adapter, &adapter->cb_funs);
    assert(adapter->plugin_info.plugin != NULL);
    assert(neu_plugin_common_check(adapter->plugin_info.plugin));
    neu_plugin_common_t *common =
        neu_plugin_to_plugin_common(adapter->plugin_info.plugin);
    common->adapter           = adapter;
    common->adapter_callbacks = &adapter->cb_funs;
    common->link_state        = NEU_PLUGIN_LINK_STATE_DISCONNECTED;
    common->default_category  = zlog_get_category(adapter->name);

    char pc[NEU_ADAPTER_NAME_SIZE + 10] = { 0 };
    snprintf(pc, sizeof(pc), "%s-protocol", adapter->name);
    common->protocol_category = zlog_get_category(pc);

    adapter->events = neu_event_new();

    log_info("Success to create adapter: %s", adapter->name);

    return adapter;
}

void neu_adapter_destroy(neu_adapter_t *adapter)
{
    assert(adapter != NULL);

    if (adapter->plugin_info.module->type == NEU_NODE_TYPE_DRIVERX) {
        neu_adapter_driver_destroy((neu_adapter_driver_t *) adapter);
    }

    adapter->plugin_info.module->intf_funs->close(adapter->plugin_info.plugin);
    if (adapter->plugin_info.handle != NULL) {
        unload_plugin_library(adapter->plugin_info.handle,
                              adapter->plugin_info.kind);
    }
    if (adapter->name != NULL) {
        free(adapter->name);
    }
    if (NULL != adapter->setting.buf) {
        free(adapter->setting.buf);
    }
    nng_mtx_lock(adapter->nng.sub_grp_mtx);
    vector_uninit(&adapter->sub_grp_configs);
    nng_mtx_unlock(adapter->nng.sub_grp_mtx);

    nng_mtx_free(adapter->nng.mtx);
    nng_mtx_free(adapter->nng.sub_grp_mtx);
    neu_event_close(adapter->events);
    free(adapter);
}

int adapter_loop(enum neu_event_io_type type, int fd, void *usr_data)
{
    neu_adapter_t *adapter = (neu_adapter_t *) usr_data;
    int            rv      = 0;
    nng_msg *      msg;
    message_t *    pay_msg;
    msg_type_e     pay_msg_type;

    switch (type) {
    case NEU_EVENT_IO_READ:
        rv = nng_recvmsg(adapter->nng.sock, &msg, 0);
        if (rv == NNG_ECLOSED) {
            log_warn("nng socket closed.");
            return 0;
        }

        pay_msg      = nng_msg_body(msg);
        pay_msg_type = msg_get_type(pay_msg);

        switch (pay_msg_type) {
        case MSG_CMD_RESP_PONG: {
            char *buf_ptr = msg_get_buf_ptr(pay_msg);

            log_info("Adapter(%s) received pong: %s\n", adapter->name, buf_ptr);
            break;
        }
        case MSG_CMD_EXIT_LOOP: {
            uint32_t exit_code = *(uint32_t *) msg_get_buf_ptr(pay_msg);

            nng_mtx_lock(adapter->nng.mtx);
            adapter->state = ADAPTER_STATE_IDLE;
            nng_mtx_unlock(adapter->nng.mtx);

            log_info("adapter(%s) exit loop by exit_code=%d", adapter->name,
                     exit_code);
            break;
        }
        case MSG_DATA_NEURON_TRANS_DATA: {
            neuron_trans_data_t *trans_data;
            trans_data = (neuron_trans_data_t *) msg_get_buf_ptr(pay_msg);

            const neu_plugin_intf_funs_t *intf_funs;
            bool                          is_need_free;
            neu_trans_buf_t *             trans_buf;
            neu_request_t                 req;
            neu_reqresp_data_t            data_req;
            data_req.grp_config = trans_data->grp_config;
            trans_buf           = &trans_data->trans_buf;
            data_req.data_val =
                neu_trans_buf_get_data_val(trans_buf, &is_need_free);
            if (data_req.data_val == NULL) {
                neu_trans_buf_uninit(trans_buf);
                log_error("Failed to get data value from trans data");
                break;
            }

            intf_funs     = adapter->plugin_info.module->intf_funs;
            req.req_id    = adapter_get_req_id(adapter);
            req.req_type  = NEU_REQRESP_TRANS_DATA;
            req.buf_len   = sizeof(neu_reqresp_data_t);
            req.buf       = (void *) &data_req;
            req.sender_id = trans_data->sender_id;
            req.node_name = trans_data->node_name;
            intf_funs->request(adapter->plugin_info.plugin, &req);
            if (is_need_free) {
                neu_dvalue_free(data_req.data_val);
            }
            neu_trans_buf_uninit(trans_buf);
            break;
        }

        case MSG_CMD_PERSISTENCE_LOAD: {
            persister_singleton_load_data(adapter);
            break;
        }

        case MSG_EVENT_ADD_NODE:
            // fall through

        case MSG_EVENT_DEL_NODE: {
            const char *node_name = msg_get_buf_ptr(pay_msg);
            persister_singleton_handle_nodes(adapter, pay_msg_type, node_name);
            break;
        }

        case MSG_EVENT_UPDATE_NODE: {
            neu_event_update_node_t *event = msg_get_buf_ptr(pay_msg);
            persister_singleton_handle_update_node(adapter, event);
            free(event->src_name);
            free(event->dst_name);
            break;
        }

        case MSG_EVENT_SET_NODE_SETTING: {
            neu_cmd_set_node_setting_t *cmd       = msg_get_buf_ptr(pay_msg);
            neu_persister_t *           persister = persister_singleton_get();
            char *                      adapter_name = NULL;
            rv = neu_manager_get_node_name_by_id(adapter->manager, cmd->node_id,
                                                 &adapter_name);
            if (0 != rv) {
                free((char *) cmd->setting);
                break;
            }
            rv = neu_persister_store_adapter_setting(persister, adapter_name,
                                                     cmd->setting);
            if (0 != rv) {
                log_error("%s fail to store adapter:%s setting:%s",
                          adapter->name, adapter_name, cmd->setting);
            }
            free((char *) cmd->setting);
            free(adapter_name);
            break;
        }

        case MSG_EVENT_ADD_GRP_CONFIG: {
            neu_cmd_add_grp_config_t *cmd = msg_get_buf_ptr(pay_msg);
            persister_singleton_handle_grp_config(
                adapter, pay_msg_type, cmd->node_id, cmd->grp_config);
            neu_taggrp_cfg_free(cmd->grp_config);
            break;
        }

        case MSG_EVENT_UPDATE_GRP_CONFIG: {
            neu_cmd_update_grp_config_t *cmd = msg_get_buf_ptr(pay_msg);
            persister_singleton_handle_grp_config(
                adapter, pay_msg_type, cmd->node_id, cmd->grp_config);
            neu_taggrp_cfg_free(cmd->grp_config);
            break;
        }

        case MSG_EVENT_DEL_GRP_CONFIG: {
            neu_cmd_del_grp_config_t *cmd = msg_get_buf_ptr(pay_msg);
            persister_singleton_handle_grp_config(
                adapter, pay_msg_type, cmd->node_id, cmd->config_name);
            free(cmd->config_name);
            break;
        }

        case MSG_EVENT_SUBSCRIBE_NODE:
            // fall through

        case MSG_EVENT_UNSUBSCRIBE_NODE: {
            const neu_node_id_t *node_id_p = msg_get_buf_ptr(pay_msg);
            persister_singleton_handle_subscriptions(adapter, *node_id_p);
            break;
        }

        case MSG_EVENT_ADD_TAGS:
            // fall through

        case MSG_EVENT_UPDATE_TAGS:
            // fall through

        case MSG_EVENT_DEL_TAGS: {
            neu_event_tags_t *event = msg_get_buf_ptr(pay_msg);
            persister_singleton_handle_datatags(adapter, event->node_id,
                                                event->grp_config_name);
            free(event->grp_config_name);
            break;
        }

        case MSG_EVENT_ADD_PLUGIN:
            // fall through

        case MSG_EVENT_UPDATE_PLUGIN:
            // fall through

        case MSG_EVENT_DEL_PLUGIN: {
            persister_singleton_handle_plugins(adapter, pay_msg_type);
            break;
        }

        case MSG_EVENT_UPDATE_LICENSE: {
            const neu_plugin_intf_funs_t *intf_funs = NULL;
            neu_request_t                 req       = {};

            intf_funs     = adapter->plugin_info.module->intf_funs;
            req.req_id    = adapter_get_req_id(adapter);
            req.req_type  = NEU_REQRESP_UPDATE_LICENSE;
            req.sender_id = 0;
            req.buf_len   = 0;
            req.buf       = NULL;
            intf_funs->request(adapter->plugin_info.plugin, &req);
            break;
        }

        case MSG_CMD_SUBSCRIBE_NODE: {
            subscribe_node_cmd_t *cmd_ptr;
            cmd_ptr = (subscribe_node_cmd_t *) msg_get_buf_ptr(pay_msg);

            const neu_plugin_intf_funs_t *intf_funs;
            neu_request_t                 req;
            neu_reqresp_subscribe_node_t  sub_node_req;

            sub_node_req.grp_config  = cmd_ptr->grp_config;
            sub_node_req.dst_node_id = cmd_ptr->dst_node_id;
            sub_node_req.src_node_id = cmd_ptr->src_node_id;

            intf_funs     = adapter->plugin_info.module->intf_funs;
            req.req_id    = adapter_get_req_id(adapter);
            req.req_type  = NEU_REQRESP_SUBSCRIBE_NODE;
            req.sender_id = cmd_ptr->sender_id;
            req.buf_len   = sizeof(neu_reqresp_subscribe_node_t);
            req.buf       = (void *) &sub_node_req;
            if (adapter->plugin_info.module->type == NEU_NODE_TYPE_DRIVERX) {
                neu_adapter_driver_process_msg((neu_adapter_driver_t *) adapter,
                                               &req);
            } else {
                intf_funs->request(adapter->plugin_info.plugin, &req);
            }
            break;
        }

        case MSG_CMD_UNSUBSCRIBE_NODE: {
            unsubscribe_node_cmd_t *cmd_ptr;
            cmd_ptr = (unsubscribe_node_cmd_t *) msg_get_buf_ptr(pay_msg);

            const neu_plugin_intf_funs_t * intf_funs;
            neu_request_t                  req;
            neu_reqresp_unsubscribe_node_t unsub_node_req;

            unsub_node_req.grp_config  = cmd_ptr->grp_config;
            unsub_node_req.dst_node_id = cmd_ptr->dst_node_id;
            unsub_node_req.src_node_id = cmd_ptr->src_node_id;

            intf_funs     = adapter->plugin_info.module->intf_funs;
            req.req_id    = adapter_get_req_id(adapter);
            req.req_type  = NEU_REQRESP_UNSUBSCRIBE_NODE;
            req.sender_id = cmd_ptr->sender_id;
            req.buf_len   = sizeof(neu_reqresp_unsubscribe_node_t);
            req.buf       = (void *) &unsub_node_req;
            if (adapter->plugin_info.module->type == NEU_NODE_TYPE_DRIVERX) {
                neu_adapter_driver_process_msg((neu_adapter_driver_t *) adapter,
                                               &req);
            } else {
                intf_funs->request(adapter->plugin_info.plugin, &req);
            }
            break;
        }

        case MSG_CMD_READ_DATA: {
            read_data_cmd_t *cmd_ptr;
            cmd_ptr = (read_data_cmd_t *) msg_get_buf_ptr(pay_msg);

            const neu_plugin_intf_funs_t *intf_funs;
            neu_request_t                 req;
            neu_reqresp_read_t            read_req;

            read_req.grp_config  = cmd_ptr->grp_config;
            read_req.dst_node_id = cmd_ptr->dst_node_id;

            intf_funs     = adapter->plugin_info.module->intf_funs;
            req.req_id    = cmd_ptr->req_id;
            req.req_type  = NEU_REQRESP_READ_DATA;
            req.sender_id = cmd_ptr->sender_id;
            req.buf_len   = sizeof(neu_reqresp_read_t);
            req.buf       = (void *) &read_req;
            if (adapter->plugin_info.module->type == NEU_NODE_TYPE_DRIVERX) {
                neu_adapter_driver_process_msg((neu_adapter_driver_t *) adapter,
                                               &req);
            } else {
                intf_funs->request(adapter->plugin_info.plugin, &req);
            }
            break;
        }

        case MSG_DATA_READ_RESP: {
            read_data_resp_t *cmd_ptr;
            cmd_ptr = (read_data_resp_t *) msg_get_buf_ptr(pay_msg);

            const neu_plugin_intf_funs_t *intf_funs;
            bool                          is_need_free;
            neu_trans_buf_t *             trans_buf;
            neu_request_t                 req;
            neu_reqresp_read_resp_t       read_resp;

            read_resp.grp_config = cmd_ptr->grp_config;
            trans_buf            = &cmd_ptr->trans_buf;
            read_resp.data_val =
                neu_trans_buf_get_data_val(trans_buf, &is_need_free);
            if (read_resp.data_val == NULL) {
                neu_trans_buf_uninit(trans_buf);
                log_error("Failed to get data value from read response");
                break;
            }

            intf_funs     = adapter->plugin_info.module->intf_funs;
            req.req_id    = cmd_ptr->req_id;
            req.req_type  = NEU_REQRESP_READ_RESP;
            req.sender_id = cmd_ptr->sender_id;
            req.buf_len   = sizeof(neu_reqresp_read_resp_t);
            req.buf       = (void *) &read_resp;
            intf_funs->request(adapter->plugin_info.plugin, &req);
            if (is_need_free) {
                neu_dvalue_free(read_resp.data_val);
            }
            neu_trans_buf_uninit(trans_buf);
            break;
        }

        case MSG_CMD_WRITE_DATA: {
            write_data_cmd_t *cmd_ptr;
            cmd_ptr = (write_data_cmd_t *) msg_get_buf_ptr(pay_msg);

            const neu_plugin_intf_funs_t *intf_funs;
            bool                          is_need_free;
            neu_trans_buf_t *             trans_buf;
            neu_request_t                 req;
            neu_reqresp_write_t           write_req;

            write_req.grp_config  = cmd_ptr->grp_config;
            write_req.dst_node_id = cmd_ptr->dst_node_id;
            trans_buf             = &cmd_ptr->trans_buf;
            write_req.data_val =
                neu_trans_buf_get_data_val(trans_buf, &is_need_free);
            if (write_req.data_val == NULL) {
                neu_trans_buf_uninit(trans_buf);
                log_error("Failed to get data value from write request");
                break;
            }

            intf_funs     = adapter->plugin_info.module->intf_funs;
            req.req_id    = cmd_ptr->req_id;
            req.req_type  = NEU_REQRESP_WRITE_DATA;
            req.sender_id = cmd_ptr->sender_id;
            req.buf_len   = sizeof(neu_reqresp_write_t);
            req.buf       = (void *) &write_req;
            if (adapter->plugin_info.module->type == NEU_NODE_TYPE_DRIVERX) {
                neu_adapter_driver_process_msg((neu_adapter_driver_t *) adapter,
                                               &req);
            } else {
                intf_funs->request(adapter->plugin_info.plugin, &req);
            }
            if (is_need_free) {
                neu_dvalue_free(write_req.data_val);
            }
            neu_trans_buf_uninit(trans_buf);
            break;
        }

        case MSG_DATA_WRITE_RESP: {
            write_data_resp_t *cmd_ptr;
            cmd_ptr = (write_data_resp_t *) msg_get_buf_ptr(pay_msg);

            const neu_plugin_intf_funs_t *intf_funs;
            bool                          is_need_free;
            neu_trans_buf_t *             trans_buf;
            neu_request_t                 req;
            neu_reqresp_write_resp_t      write_resp;

            write_resp.grp_config = cmd_ptr->grp_config;
            trans_buf             = &cmd_ptr->trans_buf;
            write_resp.data_val =
                neu_trans_buf_get_data_val(trans_buf, &is_need_free);
            if (write_resp.data_val == NULL) {
                neu_trans_buf_uninit(trans_buf);
                log_error("Failed to get data value from write request");
                break;
            }

            intf_funs     = adapter->plugin_info.module->intf_funs;
            req.req_id    = cmd_ptr->req_id;
            req.req_type  = NEU_REQRESP_WRITE_RESP;
            req.sender_id = cmd_ptr->sender_id;
            req.buf_len   = sizeof(neu_reqresp_write_resp_t);
            req.buf       = (void *) &write_resp;
            intf_funs->request(adapter->plugin_info.plugin, &req);
            if (is_need_free) {
                neu_dvalue_free(write_resp.data_val);
            }
            neu_trans_buf_uninit(trans_buf);
            break;
        }
        default:
            log_warn("Receive a not supported message(type: %d)",
                     msg_get_type(pay_msg));
            break;
        }

        nng_msg_free(msg);

        break;
    case NEU_EVENT_IO_CLOSED:
    case NEU_EVENT_IO_HUP:
        log_warn("adapter %s, fd: %d, recv closed or hup.", adapter->name, fd);
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

    if (adapter->plugin_info.module->type == NEU_NODE_TYPE_DRIVERX) {
        neu_adapter_driver_init((neu_adapter_driver_t *) adapter);
    }

    rv = nng_dial(adapter->nng.sock, neu_manager_get_url(adapter->manager),
                  &adapter->nng.dialer, 0);
    assert(rv == 0);

    nng_msg *out_msg;
    size_t   msg_size;
    msg_size = msg_inplace_data_get_size(strlen(adapter->name) + 1);
    rv       = nng_msg_alloc(&out_msg, msg_size);
    if (rv == 0) {
        message_t *msg_ptr;
        char *     buf_ptr;
        msg_ptr = (message_t *) nng_msg_body(out_msg);
        msg_inplace_data_init(msg_ptr, MSG_EVENT_NODE_PING, msg_size);
        buf_ptr = msg_get_buf_ptr(msg_ptr);
        memcpy(buf_ptr, adapter->name, strlen(adapter->name));
        buf_ptr[strlen(adapter->name)] = 0;
        nng_sendmsg(adapter->nng.sock, out_msg, 0);
    }
    if (adapter->state == ADAPTER_STATE_IDLE) {
        adapter->state = ADAPTER_STATE_INIT;
    }
    adapter->plugin_info.module->intf_funs->init(adapter->plugin_info.plugin);
    return rv;
}

int neu_adapter_uninit(neu_adapter_t *adapter)
{
    int rv = 0;

    adapter->plugin_info.module->intf_funs->uninit(adapter->plugin_info.plugin);
    if (adapter->plugin_info.module->type == NEU_NODE_TYPE_DRIVERX) {
        neu_adapter_driver_uninit((neu_adapter_driver_t *) adapter);
    }

    neu_event_del_io(adapter->events, adapter->nng_io);

    nng_dialer_close(adapter->nng.dialer);
    nng_close(adapter->nng.sock);

    log_info("Stop the adapter(%s)", adapter->name);
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
        if (strncmp(adapter->plugin_info.module->module_name, "ekuiper",
                    strlen("ekuiper")) != 0) {
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

// NOTE: Do NOT expose this function in header files, use for persistence only!
neu_adapter_id_t neu_adapter_set_id(neu_adapter_t *adapter, neu_adapter_id_t id)
{
    if (adapter == NULL) {
        return 0;
    }

    neu_adapter_id_t old = adapter->id;
    adapter->id          = id;
    return old;
}

adapter_type_e neu_adapter_get_type(neu_adapter_t *adapter)
{
    if (adapter == NULL) {
        return 0;
    }

    return (adapter_type_e) adapter->plugin_info.module->type;
}

plugin_id_t neu_adapter_get_plugin_id(neu_adapter_t *adapter)
{
    if (adapter == NULL) {
        plugin_id_t id = { 0 };
        return id;
    }

    return adapter->plugin_info.id;
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
        log_error("Get Config adapter with NULL adapter");
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

void neu_adapter_add_sub_grp_config(neu_adapter_t *      adapter,
                                    neu_node_id_t        node_id,
                                    neu_taggrp_config_t *grp_config)
{
    bool  find = false;
    char *name = (char *) neu_taggrp_cfg_get_name(grp_config);

    nng_mtx_lock(adapter->nng.sub_grp_mtx);

    VECTOR_FOR_EACH(&adapter->sub_grp_configs, iter)
    {
        neu_sub_grp_config_t *sgc =
            (neu_sub_grp_config_t *) iterator_get(&iter);
        if (sgc->node_id == node_id &&
            strncmp(name, sgc->group_config_name, strlen(name)) == 0) {
            find = true;
            break;
        }
    }

    if (!find) {
        neu_sub_grp_config_t sgc;
        sgc.node_id           = node_id;
        sgc.group_config_name = name;

        vector_push_back(&adapter->sub_grp_configs, &sgc);
    }

    nng_mtx_unlock(adapter->nng.sub_grp_mtx);
}

void neu_adapter_del_sub_grp_config(neu_adapter_t *      adapter,
                                    neu_node_id_t        node_id,
                                    neu_taggrp_config_t *grp_config)
{
    char *name = (char *) neu_taggrp_cfg_get_name(grp_config);

    nng_mtx_lock(adapter->nng.sub_grp_mtx);

    VECTOR_FOR_EACH(&adapter->sub_grp_configs, iter)
    {
        neu_sub_grp_config_t *sgc =
            (neu_sub_grp_config_t *) iterator_get(&iter);
        if (sgc->node_id == node_id &&
            strncmp(name, sgc->group_config_name, strlen(name)) == 0) {

            iterator_erase(&adapter->sub_grp_configs, &iter);
            break;
        }
    }

    nng_mtx_unlock(adapter->nng.sub_grp_mtx);
}

vector_t *neu_adapter_get_sub_grp_configs(neu_adapter_t *adapter)
{
    vector_t *sgc = NULL;

    nng_mtx_lock(adapter->nng.sub_grp_mtx);
    sgc = vector_clone(&adapter->sub_grp_configs);
    nng_mtx_unlock(adapter->nng.sub_grp_mtx);

    return sgc;
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