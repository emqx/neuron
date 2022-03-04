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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <nng/nng.h>
#include <nng/protocol/pair1/pair.h>
#include <nng/supplemental/util/platform.h>

#include "adapter_internal.h"
#include "config.h"
#include "core/message.h"
#include "core/neu_manager.h"
#include "core/neu_trans_buf.h"
#include "core/plugin_manager.h"
#include "neu_adapter.h"
#include "neu_log.h"
#include "neu_panic.h"
#include "neu_plugin.h"
#include "neu_plugin_info.h"
#include "neu_subscribe.h"
#include "neu_tag_group_config.h"
#include "persist/persist.h"

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
            nng_sendmsg((adapter)->sock, msg, 0);                            \
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
            nng_sendmsg((adapter)->sock, msg, 0);              \
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

typedef enum adapter_state {
    ADAPTER_STATE_IDLE = 0,
    ADAPTER_STATE_INIT,
    ADAPTER_STATE_READY,
    ADAPTER_STATE_RUNNING,
    ADAPTER_STATE_STOPPED,
} adapter_state_e;

struct neu_adapter {
    adapter_id_t         id;
    adapter_type_e       type;
    nng_mtx *            mtx;
    nng_mtx *            sub_grp_mtx;
    adapter_state_e      state;
    bool                 stop;
    char *               name;
    neu_manager_t *      manager;
    nng_pipe             pipe;
    nng_socket           sock;
    nng_thread *         thrd;
    uint32_t             new_req_id;
    plugin_id_t          plugin_id;
    plugin_kind_e        plugin_kind;
    void *               plugin_lib; // handle of dynamic lib
    neu_plugin_module_t *plugin_module;
    neu_plugin_t *       plugin;
    neu_trans_kind_e     trans_kind;
    adapter_callbacks_t  cb_funs;
    neu_config_t         node_setting;
    vector_t             sub_grp_configs; // neu_sub_grp_config_t
};

static uint32_t adapter_get_req_id(neu_adapter_t *adapter);

static neu_persister_t *g_persister_singleton = NULL;

static void persister_singleton_init()
{
    const char *persistence_dir =
        neu_config_get_value(2, (char *) "persistence", (char *) "dir");
    if (NULL == persistence_dir) {
        log_error("can't get persistence dir from config");
        exit(EXIT_FAILURE);
    }

    neu_persister_t *persister = neu_persister_create(persistence_dir);
    if (NULL == persister) {
        log_error("can't create persister");
        exit(EXIT_FAILURE);
    }
    free((char *) persistence_dir);
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
        neu_persist_plugin_info_t *plugin_info    = iterator_get(&iter);
        plugin_id_t                plugin_id      = {};
        neu_cmd_add_plugin_lib_t   add_plugin_cmd = {
            .plugin_lib_name = plugin_info->plugin_lib_name,
        };

        rv = neu_manager_add_plugin_lib(adapter->manager, &add_plugin_cmd,
                                        &plugin_id);
        const char *ok_or_err = (0 == rv) ? "success" : "fail";
        log_info("%s load plugin %s type:%d, kind:%d name:%s lib:%s",
                 adapter->name, ok_or_err, plugin_info->adapter_type,
                 plugin_info->kind, plugin_info->name,
                 plugin_info->plugin_lib_name);
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
        neu_persist_datatag_info_t *p = iterator_get(&iter);
        neu_datatag_t *             tag =
            neu_datatag_alloc(p->attribute, p->type, p->address, p->name);
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

static int persister_singleton_load_data(neu_adapter_t *adapter)
{
    vector_t *       adapter_infos = NULL;
    neu_persister_t *persister     = persister_singleton_get();

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
    uint32_t req_id;

    adapter->new_req_id++;
    if (adapter->new_req_id == 0) {
        adapter->new_req_id = 1;
    }

    req_id = adapter->new_req_id;
    return req_id;
}

static void adapter_loop(void *arg)
{
    int            rv;
    nng_dialer     dialer  = { 0 };
    neu_adapter_t *adapter = (neu_adapter_t *) arg;

    const char *manager_url;
    manager_url = neu_manager_get_url(adapter->manager);
    rv          = nng_dial(adapter->sock, manager_url, &dialer, 0);
    if (rv != 0) {
        neu_panic("The adapter can't dial to %s", manager_url);
    }

    const char *adapter_str = adapter->name;
    nng_msg *   out_msg;
    size_t      msg_size;
    msg_size = msg_inplace_data_get_size(strlen(adapter_str) + 1);
    rv       = nng_msg_alloc(&out_msg, msg_size);
    if (rv == 0) {
        message_t *msg_ptr;
        char *     buf_ptr;
        msg_ptr = (message_t *) nng_msg_body(out_msg);
        msg_inplace_data_init(msg_ptr, MSG_EVENT_NODE_PING, msg_size);
        buf_ptr = msg_get_buf_ptr(msg_ptr);
        memcpy(buf_ptr, adapter_str, strlen(adapter_str));
        buf_ptr[strlen(adapter_str)] = 0;
        nng_sendmsg(adapter->sock, out_msg, 0);
    }

    while (1) {
        nng_msg *msg;

        nng_mtx_lock(adapter->mtx);
        if (adapter->stop) {
            adapter->state = ADAPTER_STATE_IDLE;
            nng_mtx_unlock(adapter->mtx);
            log_info("Exit loop of the adapter(%s)", adapter->name);
            break;
        }
        nng_mtx_unlock(adapter->mtx);

        rv = nng_recvmsg(adapter->sock, &msg, 0);
        if (rv != 0) {
            if (rv != NNG_ETIMEDOUT)
                log_warn("Manage pipe no message received %d", rv);
            continue;
        }

        message_t *pay_msg      = nng_msg_body(msg);
        msg_type_e pay_msg_type = msg_get_type(pay_msg);
        switch (pay_msg_type) {
        case MSG_CMD_RESP_PONG: {
            char *buf_ptr;

            buf_ptr = msg_get_buf_ptr(pay_msg);
            log_info("Adapter(%s) received pong: %s", adapter->name, buf_ptr);
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

            intf_funs     = adapter->plugin_module->intf_funs;
            req.req_id    = adapter_get_req_id(adapter);
            req.req_type  = NEU_REQRESP_TRANS_DATA;
            req.buf_len   = sizeof(neu_reqresp_data_t);
            req.buf       = (void *) &data_req;
            req.sender_id = trans_data->sender_id;
            intf_funs->request(adapter->plugin, &req);
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

        case MSG_EVENT_UPDATE_NODE:
            // fall through

        case MSG_EVENT_DEL_NODE: {
            const char *node_name = msg_get_buf_ptr(pay_msg);
            persister_singleton_handle_nodes(adapter, pay_msg_type, node_name);
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

        case MSG_EVENT_LIC_UPDATED: {
            neu_event_lic_updated_t *lic_event  = msg_get_buf_ptr(pay_msg);
            neu_reqresp_update_lic_t lic_update = {};
            lic_update.lic_fname                = lic_event->lic_fname;

            const neu_plugin_intf_funs_t *intf_funs;
            neu_request_t                 req;

            intf_funs     = adapter->plugin_module->intf_funs;
            req.req_id    = adapter_get_req_id(adapter);
            req.req_type  = NEU_REQRESP_UPDATE_LIC;
            req.sender_id = 0;
            req.buf_len   = sizeof(lic_update);
            req.buf       = &lic_update;
            intf_funs->request(adapter->plugin, &req);
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

            intf_funs     = adapter->plugin_module->intf_funs;
            req.req_id    = adapter_get_req_id(adapter);
            req.req_type  = NEU_REQRESP_SUBSCRIBE_NODE;
            req.sender_id = cmd_ptr->sender_id;
            req.buf_len   = sizeof(neu_reqresp_subscribe_node_t);
            req.buf       = (void *) &sub_node_req;
            intf_funs->request(adapter->plugin, &req);
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

            intf_funs     = adapter->plugin_module->intf_funs;
            req.req_id    = adapter_get_req_id(adapter);
            req.req_type  = NEU_REQRESP_UNSUBSCRIBE_NODE;
            req.sender_id = cmd_ptr->sender_id;
            req.buf_len   = sizeof(neu_reqresp_unsubscribe_node_t);
            req.buf       = (void *) &unsub_node_req;
            intf_funs->request(adapter->plugin, &req);
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

            intf_funs     = adapter->plugin_module->intf_funs;
            req.req_id    = cmd_ptr->req_id;
            req.req_type  = NEU_REQRESP_READ_DATA;
            req.sender_id = cmd_ptr->sender_id;
            req.buf_len   = sizeof(neu_reqresp_read_t);
            req.buf       = (void *) &read_req;
            intf_funs->request(adapter->plugin, &req);
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

            intf_funs     = adapter->plugin_module->intf_funs;
            req.req_id    = cmd_ptr->req_id;
            req.req_type  = NEU_REQRESP_READ_RESP;
            req.sender_id = 0;
            req.buf_len   = sizeof(neu_reqresp_read_resp_t);
            req.buf       = (void *) &read_resp;
            intf_funs->request(adapter->plugin, &req);
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

            intf_funs     = adapter->plugin_module->intf_funs;
            req.req_id    = cmd_ptr->req_id;
            req.req_type  = NEU_REQRESP_WRITE_DATA;
            req.sender_id = cmd_ptr->sender_id;
            req.buf_len   = sizeof(neu_reqresp_write_t);
            req.buf       = (void *) &write_req;
            intf_funs->request(adapter->plugin, &req);
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

            intf_funs     = adapter->plugin_module->intf_funs;
            req.req_id    = cmd_ptr->req_id;
            req.req_type  = NEU_REQRESP_WRITE_RESP;
            req.sender_id = 0;
            req.buf_len   = sizeof(neu_reqresp_write_resp_t);
            req.buf       = (void *) &write_resp;
            intf_funs->request(adapter->plugin, &req);
            if (is_need_free) {
                neu_dvalue_free(write_resp.data_val);
            }
            neu_trans_buf_uninit(trans_buf);
            break;
        }

        case MSG_CMD_GET_LIC_VAL: {
            get_lic_val_cmd_t *cmd_ptr = msg_get_buf_ptr(pay_msg);

            const neu_plugin_intf_funs_t *intf_funs;
            neu_request_t                 req;
            neu_reqresp_get_lic_val_t     lic_req;

            lic_req.lic_fname = cmd_ptr->lic_fname;
            lic_req.key_name  = cmd_ptr->key_name;

            intf_funs     = adapter->plugin_module->intf_funs;
            req.req_id    = cmd_ptr->req_id;
            req.req_type  = NEU_REQRESP_GET_LIC_VAL;
            req.sender_id = cmd_ptr->sender_id;
            req.buf_len   = sizeof(lic_req);
            req.buf       = (void *) &lic_req;
            intf_funs->request(adapter->plugin, &req);
            break;
        }

        case MSG_DATA_LIC_RESP: {
            lic_val_resp_t *lic_resp = msg_get_buf_ptr(pay_msg);

            const neu_plugin_intf_funs_t *intf_funs;
            neu_request_t                 req;
            neu_reqresp_lic_val_t         lic_val;

            lic_val.lic_fname = lic_resp->lic_fname;
            lic_val.key_name  = lic_resp->key_name;
            lic_val.val       = lic_resp->val; // don't take ownership

            intf_funs     = adapter->plugin_module->intf_funs;
            req.req_id    = lic_resp->req_id;
            req.req_type  = NEU_REQRESP_LIC_VAL;
            req.sender_id = 0;
            req.buf_len   = sizeof(lic_val);
            req.buf       = (void *) &lic_val;
            intf_funs->request(adapter->plugin, &req);
            neu_dvalue_free(lic_val.val);
            break;
        }

        case MSG_CMD_EXIT_LOOP: {
            uint32_t exit_code;

            exit_code = *(uint32_t *) msg_get_buf_ptr(pay_msg);
            log_info("adapter(%s) exit loop by exit_code=%d", adapter->name,
                     exit_code);
            nng_mtx_lock(adapter->mtx);
            adapter->state = ADAPTER_STATE_IDLE;
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

    nng_dialer_close(dialer);
    return;
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

    log_info("Get command from plugin %d, %s", cmd->req_type, adapter->name);
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

    case NEU_REQRESP_UPDATE_NODE: {
        ADAPTER_RESP_CODE(
            adapter, cmd, intptr_t, neu_cmd_update_node_t, rv,
            NEU_REQRESP_ERR_CODE, p_result, {
                ret = neu_manager_update_node(adapter->manager, req_cmd);
                if (0 == ret) {
                    ADAPTER_SEND_NODE_EVENT(adapter, rv, MSG_EVENT_UPDATE_NODE,
                                            req_cmd->node_name);
                }
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

                vector_init(&ret->nodes, DEFAULT_ADAPTER_REG_COUNT,
                            sizeof(neu_node_info_t));
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

                vector_init(&ret->plugin_libs, DEFAULT_ADAPTER_REG_COUNT,
                            sizeof(plugin_lib_info_t));
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
                        ADAPTER_SEND_NODE_EVENT(
                            adapter, rv, MSG_EVENT_UPDATE_NODE, node_name);
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

    case NEU_REQRESP_GET_LIC_VAL: {
        neu_reqresp_get_lic_val_t *lic_req = cmd->buf;
        get_lic_val_cmd_t          lic_cmd = {};
        lic_cmd.sender_id                  = to_node_id(adapter, adapter->id);
        lic_cmd.req_id                     = cmd->req_id;
        lic_cmd.lic_fname                  = lic_req->lic_fname;
        lic_cmd.key_name                   = lic_req->key_name;

        ADAPTER_SEND_BUF(adapter, rv, MSG_CMD_GET_LIC_VAL, &lic_cmd,
                         sizeof(lic_cmd));
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
            trans_buf              = &trans_data->trans_buf;
            rv = neu_trans_buf_init(trans_buf, adapter->trans_kind,
                                    neu_data->data_val);
            if (rv != 0) {
                nng_msg_free(trans_data_msg);
                break;
            }
            nng_sendmsg(adapter->sock, trans_data_msg, 0);
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
            trans_buf                  = &read_data_resp->trans_buf;
            rv = neu_trans_buf_init(trans_buf, adapter->trans_kind,
                                    read_resp->data_val);
            if (rv != 0) {
                nng_msg_free(read_resp_msg);
                break;
            }
            nng_sendmsg(adapter->sock, read_resp_msg, 0);
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
            trans_buf                   = &write_data_resp->trans_buf;
            rv = neu_trans_buf_init(trans_buf, adapter->trans_kind,
                                    write_resp->data_val);
            if (rv != 0) {
                nng_msg_free(write_resp_msg);
                break;
            }
            nng_sendmsg(adapter->sock, write_resp_msg, 0);
        }
        break;
    }

    case NEU_REQRESP_LIC_VAL: {
        neu_reqresp_lic_val_t *lic_val = resp->buf;
        assert(resp->buf_len == sizeof(*lic_val));

        lic_val_resp_t lic_resp;
        lic_resp.recver_id = resp->recver_id;
        lic_resp.req_id    = resp->req_id;
        lic_resp.lic_fname = lic_val->lic_fname;
        lic_resp.key_name  = lic_val->key_name;
        lic_resp.val       = lic_val->val; // ownership transferred

        nng_msg *msg = nng_msg_inplace_from_buf(MSG_DATA_LIC_RESP, &lic_resp,
                                                sizeof(lic_resp));
        if (NULL == msg) {
            log_error("unable to allocate NEU_REQRESP_LIC_VAL message.");
            neu_dvalue_free(lic_resp.val);
            break;
        }

        rv = nng_sendmsg(adapter->sock, msg, 0);
        if (0 != rv) {
            neu_dvalue_free(lic_resp.val);
            nng_msg_free(msg);
            log_error("fail to send NEU_REQRESP_LIC_VAL message.");
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

    case NEU_EVENT_LIC_UPDATED: {
        ADAPTER_SEND_BUF(adapter, rv, MSG_EVENT_LIC_UPDATED, event->buf,
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
    neu_adapter_t *adapter;
    nng_duration   recv_timeout = 100;

    if (manager == NULL) {
        log_error("Create adapter with NULL manager");
        return NULL;
    }

    adapter = calloc(1, sizeof(neu_adapter_t));
    if (adapter == NULL) {
        log_error("Out of memeory for create adapter");
        return NULL;
    }

    int rv;
    adapter->stop = false;
    if ((rv = nng_mtx_alloc(&adapter->mtx)) != 0) {
        log_error("Can't allocate mutex for adapter");
        free(adapter);
        return NULL;
    }

    if ((rv = nng_mtx_alloc(&adapter->sub_grp_mtx)) != 0) {
        log_error("Can't allocate mutex for adapter sub grp config");
        free(adapter);
        return NULL;
    }

    adapter->id          = info->id;
    adapter->type        = info->type;
    adapter->name        = strdup(info->name);
    adapter->state       = ADAPTER_STATE_IDLE;
    adapter->new_req_id  = 1;
    adapter->plugin_id   = info->plugin_id;
    adapter->plugin_kind = info->plugin_kind;
    adapter->manager     = manager;
    adapter->trans_kind  = NEURON_TRANS_DATAVAL;

    if (adapter->name == NULL || info->plugin_lib_name == NULL) {
        if (adapter->name != NULL) {
            free(adapter->name);
        }
        nng_mtx_free(adapter->mtx);
        free(adapter);
        log_error("Failed duplicate string for create adapter");
        return NULL;
    }

    vector_init(&adapter->sub_grp_configs, 0, sizeof(neu_sub_grp_config_t));

    void *               handle;
    neu_plugin_module_t *plugin_module;
    handle = load_plugin_library((char *) info->plugin_lib_name,
                                 adapter->plugin_kind, &plugin_module);
    if (handle == NULL) {
        neu_panic("Can't to load library(%s) for plugin(%s)",
                  info->plugin_lib_name, adapter->name);
    }

    neu_plugin_t *plugin;
    adapter->plugin_lib    = handle;
    adapter->plugin_module = plugin_module;
    plugin = plugin_module->intf_funs->open(adapter, &callback_funs);
    if (plugin == NULL) {
        neu_panic("Can't to open plugin(%s)", plugin_module->module_name);
    }

    if (!neu_plugin_common_check(plugin)) {
        neu_panic("Failed to check if plugin is valid, %s",
                  plugin_module->module_name);
    }

    adapter->plugin = plugin;

    rv = nng_pair1_open(&adapter->sock);
    if (rv != 0) {
        neu_panic("The adapter(%s) can't open pipe", adapter->name);
    }

    nng_setopt(adapter->sock, NNG_OPT_RECVTIMEO, &recv_timeout,
               sizeof(recv_timeout));

    log_info("Success to create adapter: %s", adapter->name);
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
        unload_plugin_library(adapter->plugin_lib, adapter->plugin_kind);
    }
    if (adapter->name != NULL) {
        free(adapter->name);
    }
    if (NULL != adapter->node_setting.buf) {
        free(adapter->node_setting.buf);
    }
    nng_mtx_lock(adapter->sub_grp_mtx);
    vector_uninit(&adapter->sub_grp_configs);
    nng_mtx_unlock(adapter->sub_grp_mtx);

    nng_mtx_free(adapter->mtx);
    nng_mtx_free(adapter->sub_grp_mtx);
    free(adapter);
    return;
}

int neu_adapter_init(neu_adapter_t *adapter)
{
    int rv = 0;

    if (adapter == NULL) {
        log_error("Start adapter with NULL adapter");
        return (-1);
    }

    if (adapter->plugin_module != NULL) {
        const neu_plugin_intf_funs_t *intf_funs;
        intf_funs = adapter->plugin_module->intf_funs;
        intf_funs->init(adapter->plugin);
    }

    adapter->stop  = false;
    adapter->state = ADAPTER_STATE_INIT;
    nng_thread_create(&adapter->thrd, adapter_loop, adapter);
    return rv;
}

int neu_adapter_uninit(neu_adapter_t *adapter)
{
    int rv = 0;

    if (adapter == NULL) {
        log_error("Stop adapter with NULL adapter");
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

int neu_adapter_start(neu_adapter_t *adapter)
{
    // TODO  start by send msg
    const neu_plugin_intf_funs_t *intf_funs = adapter->plugin_module->intf_funs;
    neu_err_code_e                error     = NEU_ERR_SUCCESS;

    nng_mtx_lock(adapter->mtx);
    switch (adapter->state) {
    case ADAPTER_STATE_IDLE:
    case ADAPTER_STATE_INIT:
        error = NEU_ERR_NODE_NOT_READY;
        break;
    case ADAPTER_STATE_RUNNING:
        error = NEU_ERR_NODE_IS_RUNNING;
        break;
    case ADAPTER_STATE_READY:
    case ADAPTER_STATE_STOPPED:
        break;
    }
    nng_mtx_unlock(adapter->mtx);

    if (error != NEU_ERR_SUCCESS) {
        return error;
    }

    error = intf_funs->start(adapter->plugin);
    if (error == NEU_ERR_SUCCESS) {
        nng_mtx_lock(adapter->mtx);
        adapter->state = ADAPTER_STATE_RUNNING;
        nng_mtx_unlock(adapter->mtx);
    }

    return error;
}

int neu_adapter_stop(neu_adapter_t *adapter)
{
    // TODO  stop by send msg

    const neu_plugin_intf_funs_t *intf_funs = adapter->plugin_module->intf_funs;
    neu_err_code_e                error     = NEU_ERR_SUCCESS;

    nng_mtx_lock(adapter->mtx);
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
    nng_mtx_unlock(adapter->mtx);

    if (error != NEU_ERR_SUCCESS) {
        return error;
    }

    error = intf_funs->stop(adapter->plugin);
    if (error == NEU_ERR_SUCCESS) {
        nng_mtx_lock(adapter->mtx);
        adapter->state = ADAPTER_STATE_STOPPED;
        nng_mtx_unlock(adapter->mtx);
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

void neu_adapter_rename(neu_adapter_t *adapter, const char *name)
{
    free(adapter->name);
    adapter->name = strdup(name);
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

adapter_type_e neu_adapter_get_type(neu_adapter_t *adapter)
{
    if (adapter == NULL) {
        return 0;
    }

    return adapter->type;
}

plugin_id_t neu_adapter_get_plugin_id(neu_adapter_t *adapter)
{
    if (adapter == NULL) {
        plugin_id_t id = { 0 };
        return id;
    }

    return adapter->plugin_id;
}

int neu_adapter_set_setting(neu_adapter_t *adapter, neu_config_t *config)
{
    int rv = -1;

    if (adapter == NULL) {
        log_error("Config adapter with NULL adapter");
        return NEU_ERR_NODE_NOT_EXIST;
    }

    if (adapter->plugin_module != NULL) {
        const neu_plugin_intf_funs_t *intf_funs;

        intf_funs = adapter->plugin_module->intf_funs;
        rv        = intf_funs->config(adapter->plugin, config);
        if (rv == 0) {
            adapter->node_setting.buf_len = config->buf_len;
            adapter->node_setting.type    = config->type;

            if (adapter->node_setting.buf != NULL)
                free(adapter->node_setting.buf);
            adapter->node_setting.buf = strdup(config->buf);
            if (adapter->state == ADAPTER_STATE_INIT) {
                adapter->state = ADAPTER_STATE_READY;
            }

        } else {
            rv = NEU_ERR_NODE_SETTING_INVALID;
        }
    }

    return rv;
}

int neu_adapter_get_setting(neu_adapter_t *adapter, char **config)
{
    if (adapter == NULL) {
        log_error("Get Config adapter with NULL adapter");
        return NEU_ERR_NODE_NOT_EXIST;
    }

    if (adapter->node_setting.buf != NULL) {
        *config = strdup(adapter->node_setting.buf);
        return NEU_ERR_SUCCESS;
    }

    return NEU_ERR_NODE_SETTING_NOT_FOUND;
}

neu_plugin_state_t neu_adapter_get_state(neu_adapter_t *adapter)
{
    neu_plugin_state_t   state  = { 0 };
    neu_plugin_common_t *common = neu_plugin_to_plugin_common(adapter->plugin);

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

    nng_mtx_lock(adapter->sub_grp_mtx);

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

    nng_mtx_unlock(adapter->sub_grp_mtx);
}

void neu_adapter_del_sub_grp_config(neu_adapter_t *      adapter,
                                    neu_node_id_t        node_id,
                                    neu_taggrp_config_t *grp_config)
{
    char *name = (char *) neu_taggrp_cfg_get_name(grp_config);

    nng_mtx_lock(adapter->sub_grp_mtx);

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

    nng_mtx_unlock(adapter->sub_grp_mtx);
}

vector_t *neu_adapter_get_sub_grp_configs(neu_adapter_t *adapter)
{
    vector_t *sgc = NULL;

    nng_mtx_lock(adapter->sub_grp_mtx);
    sgc = vector_clone(&adapter->sub_grp_configs);
    nng_mtx_unlock(adapter->sub_grp_mtx);

    return sgc;
}

int neu_adapter_validate_tag(neu_adapter_t *adapter, neu_datatag_t *tag)
{
    const neu_plugin_intf_funs_t *intf_funs = adapter->plugin_module->intf_funs;
    neu_err_code_e                error     = NEU_ERR_SUCCESS;

    error = intf_funs->validate_tag(adapter->plugin, tag);

    return error;
}
