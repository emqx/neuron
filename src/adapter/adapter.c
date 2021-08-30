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
#include "core/message.h"
#include "core/neu_manager.h"
#include "core/plugin_manager.h"
#include "neu_adapter.h"
#include "neu_log.h"
#include "neu_panic.h"
#include "neu_plugin.h"

typedef enum adapter_state {
    ADAPTER_STATE_NULL,
    ADAPTER_STATE_IDLE,
    ADAPTER_STATE_STARTING,
    ADAPTER_STATE_RUNNING,
    ADAPTER_STATE_STOPPING,
} adapter_state_e;

struct neu_adapter {
    adapter_id_t         id;
    adapter_type_e       type;
    nng_mtx *            mtx;
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
    char *               plugin_lib_name;
    void *               plugin_lib; // handle of dynamic lib
    neu_plugin_module_t *plugin_module;
    neu_plugin_t *       plugin;
    adapter_callbacks_t  cb_funs;
};

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
    neu_adapter_t *adapter;

    adapter = (neu_adapter_t *) arg;
    const char *manager_url;
    manager_url = neu_manager_get_url(adapter->manager);
    rv          = nng_dial(adapter->sock, manager_url, NULL, 0);
    if (rv != 0) {
        neu_panic("The adapter can't dial to %s", manager_url);
    }

    nng_mtx_lock(adapter->mtx);
    adapter->state = ADAPTER_STATE_IDLE;
    nng_mtx_unlock(adapter->mtx);

    const char *adapter_str = "adapter started";
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
            adapter->state = ADAPTER_STATE_NULL;
            nng_mtx_unlock(adapter->mtx);
            log_info("Exit loop of the adapter(%s)", adapter->name);
            break;
        }
        nng_mtx_unlock(adapter->mtx);

        rv = nng_recvmsg(adapter->sock, &msg, 0);
        if (rv != 0) {
            log_warn("Manage pipe no message received");
            continue;
        }

        message_t *pay_msg;
        pay_msg = nng_msg_body(msg);
        switch (msg_get_type(pay_msg)) {
        case MSG_CMD_RESP_PONG: {
            bool  need_config;
            char *buf_ptr;

            need_config = false;
            buf_ptr     = msg_get_buf_ptr(pay_msg);
            log_info("Adapter(%s) received pong: %s", adapter->name, buf_ptr);
            nng_mtx_lock(adapter->mtx);
            if (adapter->state == ADAPTER_STATE_IDLE) {
                need_config    = true;
                adapter->state = ADAPTER_STATE_RUNNING;
            }
            nng_mtx_unlock(adapter->mtx);

            if (need_config) {
                const neu_plugin_intf_funs_t *intf_funs;
                neu_config_t                  config;

                config.type    = NEU_CONFIG_UNKNOW;
                config.buf_len = 0;
                config.buf     = NULL;
                intf_funs      = adapter->plugin_module->intf_funs;
                intf_funs->config(adapter->plugin, &config);
            }
            break;
        }

        case MSG_DATA_NEURON_DATABUF: {
            neuron_databuf_t *databuf_ptr;
            databuf_ptr = (neuron_databuf_t *) msg_get_buf_ptr(pay_msg);

            const neu_plugin_intf_funs_t *intf_funs;
            neu_request_t                 req;
            if (adapter->plugin_module) {
                neu_reqresp_data_t data_req;
                data_req.grp_config = databuf_ptr->grp_config;
                void * buf        = core_databuf_get_ptr(databuf_ptr->databuf);
                size_t buf_len    = core_databuf_get_len(databuf_ptr->databuf);
                data_req.data_var = neu_variable_deserialize(buf, buf_len);

                intf_funs    = adapter->plugin_module->intf_funs;
                req.req_id   = adapter_get_req_id(adapter);
                req.req_type = NEU_REQRESP_TRANS_DATA;
                req.buf_len  = sizeof(neu_reqresp_data_t);
                req.buf      = (void *) &data_req;
                intf_funs->request(adapter->plugin, &req);
                // TODO: free the data_var of neu_variable_t
            } else {
                neu_taggrp_cfg_free(databuf_ptr->grp_config);
            }
            core_databuf_put(databuf_ptr->databuf);
            break;
        }

        case MSG_CMD_READ_DATA: {
            read_data_cmd_t *cmd_ptr;
            cmd_ptr = (read_data_cmd_t *) msg_get_buf_ptr(pay_msg);

            const neu_plugin_intf_funs_t *intf_funs;
            neu_request_t                 req;
            if (adapter->plugin_module) {
                neu_reqresp_read_t read_req;
                read_req.grp_config  = cmd_ptr->grp_config;
                read_req.dst_node_id = cmd_ptr->dst_node_id;
                read_req.addr        = cmd_ptr->addr;

                intf_funs    = adapter->plugin_module->intf_funs;
                req.req_id   = adapter_get_req_id(adapter);
                req.req_type = NEU_REQRESP_READ_DATA;
                req.buf_len  = sizeof(neu_reqresp_read_t);
                req.buf      = (void *) &read_req;
                intf_funs->request(adapter->plugin, &req);
            } else {
                neu_taggrp_cfg_free(cmd_ptr->grp_config);
            }
            break;
        }

        case MSG_CMD_WRITE_DATA: {
            write_data_cmd_t *cmd_ptr;
            cmd_ptr = (write_data_cmd_t *) msg_get_buf_ptr(pay_msg);

            const neu_plugin_intf_funs_t *intf_funs;
            neu_request_t                 req;
            if (adapter->plugin_module) {
                neu_reqresp_write_t write_req;
                write_req.grp_config  = cmd_ptr->grp_config;
                write_req.dst_node_id = cmd_ptr->dst_node_id;
                write_req.addr        = cmd_ptr->addr;
                void * buf            = core_databuf_get_ptr(cmd_ptr->databuf);
                size_t buf_len        = core_databuf_get_len(cmd_ptr->databuf);
                write_req.data_var    = neu_variable_deserialize(buf, buf_len);

                intf_funs    = adapter->plugin_module->intf_funs;
                req.req_id   = adapter_get_req_id(adapter);
                req.req_type = NEU_REQRESP_WRITE_DATA;
                req.buf_len  = sizeof(neu_reqresp_write_t);
                req.buf      = (void *) &write_req;
                intf_funs->request(adapter->plugin, &req);
                // TODO: free the data_var of neu_variable_t
            } else {
                neu_taggrp_cfg_free(cmd_ptr->grp_config);
            }
            core_databuf_put(cmd_ptr->databuf);
            break;
        }

        case MSG_CMD_EXIT_LOOP: {
            uint32_t exit_code;

            exit_code = *(uint32_t *) msg_get_buf_ptr(pay_msg);
            log_info("adapter(%s) exit loop by exit_code=%d", adapter->name,
                     exit_code);
            nng_mtx_lock(adapter->mtx);
            adapter->state = ADAPTER_STATE_NULL;
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

    log_info("Get command from plugin");
    switch (cmd->req_type) {
    case NEU_REQRESP_READ_DATA: {
        size_t   msg_size;
        nng_msg *read_msg;
        msg_size = msg_inplace_data_get_size(sizeof(read_data_cmd_t));
        rv       = nng_msg_alloc(&read_msg, msg_size);
        if (rv == 0) {
            message_t *         msg_ptr;
            read_data_cmd_t *   cmd_ptr;
            neu_reqresp_read_t *read_cmd;

            assert(cmd->buf_len == sizeof(neu_reqresp_read_t));
            read_cmd = (neu_reqresp_read_t *) cmd->buf;
            msg_ptr  = (message_t *) nng_msg_body(read_msg);
            msg_inplace_data_init(msg_ptr, MSG_CMD_READ_DATA,
                                  sizeof(read_data_cmd_t));

            cmd_ptr              = (read_data_cmd_t *) msg_get_buf_ptr(msg_ptr);
            cmd_ptr->sender_id   = adapter->id;
            cmd_ptr->dst_node_id = read_cmd->dst_node_id;
            cmd_ptr->grp_config  = read_cmd->grp_config;
            cmd_ptr->addr        = read_cmd->addr;
            nng_sendmsg(adapter->sock, read_msg, 0);
        }
        break;
    }

    case NEU_REQRESP_WRITE_DATA: {
        size_t   msg_size;
        nng_msg *write_msg;
        msg_size = msg_inplace_data_get_size(sizeof(write_data_cmd_t));
        rv       = nng_msg_alloc(&write_msg, msg_size);
        if (rv == 0) {
            message_t *          msg_ptr;
            write_data_cmd_t *   cmd_ptr;
            neu_reqresp_write_t *write_cmd;
            void *               buf;
            size_t               buf_len;
            core_databuf_t *     databuf;

            assert(cmd->buf_len == sizeof(neu_reqresp_write_t));
            write_cmd = (neu_reqresp_write_t *) cmd->buf;
            buf_len   = neu_variable_serialize(write_cmd->data_var, &buf);
            databuf   = core_databuf_new_with_buf(buf, buf_len);
            msg_ptr   = (message_t *) nng_msg_body(write_msg);
            msg_inplace_data_init(msg_ptr, MSG_CMD_WRITE_DATA,
                                  sizeof(write_data_cmd_t));

            cmd_ptr            = (write_data_cmd_t *) msg_get_buf_ptr(msg_ptr);
            cmd_ptr->sender_id = adapter->id;
            cmd_ptr->dst_node_id = write_cmd->dst_node_id;
            cmd_ptr->grp_config  = write_cmd->grp_config;
            cmd_ptr->addr        = write_cmd->addr;
            cmd_ptr->databuf     = databuf;
            nng_sendmsg(adapter->sock, write_msg, 0);
        }
        break;
    }

    case NEU_REQRESP_ADD_NODE: {
        intptr_t        ret_code;
        neu_node_id_t   node_id;
        neu_response_t *result;

        assert(cmd->buf_len == sizeof(neu_cmd_add_node_t));
        ret_code = NEU_ERR_SUCCESS;
        result   = malloc(sizeof(neu_response_t));
        if (result == NULL) {
            log_error("Failed to allocate result for add nodes");
            rv = -1;
            break;
        }

        neu_cmd_add_node_t *node_add_cmd;
        node_add_cmd = (neu_cmd_add_node_t *) cmd->buf;
        node_id      = neu_manager_add_node(adapter->manager, node_add_cmd);
        if (node_id == 0) {
            ret_code = NEU_ERR_FAILURE;
        }

        result->resp_type = NEU_REQRESP_ERR_CODE;
        result->req_id    = cmd->req_id;
        result->buf_len   = sizeof(intptr_t);
        result->buf       = (void *) ret_code;
        if (p_result != NULL) {
            *p_result = result;
        }
        break;
    }

    case NEU_REQRESP_DEL_NODE: {
        intptr_t        ret_code;
        neu_response_t *result;

        assert(cmd->buf_len == sizeof(neu_cmd_del_node_t));
        ret_code = NEU_ERR_SUCCESS;
        result   = malloc(sizeof(neu_response_t));
        if (result == NULL) {
            log_error("Failed to allocate result for add nodes");
            rv = -1;
            break;
        }

        neu_cmd_del_node_t *node_del_cmd;
        node_del_cmd = (neu_cmd_del_node_t *) cmd->buf;
        rv = neu_manager_del_node(adapter->manager, node_del_cmd->node_id);
        if (rv != 0) {
            ret_code = NEU_ERR_FAILURE;
        }

        result->resp_type = NEU_REQRESP_ERR_CODE;
        result->req_id    = cmd->req_id;
        result->buf_len   = sizeof(intptr_t);
        result->buf       = (void *) ret_code;
        if (p_result != NULL) {
            *p_result = result;
        }
        break;
    }

    case NEU_REQRESP_UPDATE_NODE: {
        intptr_t        ret_code;
        neu_response_t *result;

        assert(cmd->buf_len == sizeof(neu_cmd_update_node_t));
        ret_code = NEU_ERR_SUCCESS;
        result   = malloc(sizeof(neu_response_t));
        if (result == NULL) {
            log_error("Failed to allocate result for update nodes");
            rv = -1;
            break;
        }

        neu_cmd_update_node_t *node_update_cmd;
        node_update_cmd = (neu_cmd_update_node_t *) cmd->buf;
        rv = neu_manager_update_node(adapter->manager, node_update_cmd);
        if (rv != 0) {
            ret_code = NEU_ERR_FAILURE;
        }

        result->resp_type = NEU_REQRESP_ERR_CODE;
        result->req_id    = cmd->req_id;
        result->buf_len   = sizeof(intptr_t);
        result->buf       = (void *) ret_code;
        if (p_result != NULL) {
            *p_result = result;
        }
        break;
    }

    case NEU_REQRESP_GET_NODES: {
        neu_response_t *     result;
        neu_reqresp_nodes_t *resp_nodes;

        assert(cmd->buf_len == sizeof(neu_cmd_get_nodes_t));
        resp_nodes = malloc(sizeof(neu_reqresp_nodes_t));
        if (resp_nodes == NULL) {
            log_error("Failed to allocate result of group configs");
            rv = -1;
            break;
        }
        result = malloc(sizeof(neu_response_t));
        if (result == NULL) {
            log_error("Failed to allocate result for get nodes");
            free(resp_nodes);
            rv = -1;
            break;
        }

        neu_cmd_get_nodes_t *nodes_cmd;
        nodes_cmd = (neu_cmd_get_nodes_t *) cmd->buf;
        vector_init(&resp_nodes->nodes, DEFAULT_ADAPTER_REG_COUNT,
                    sizeof(neu_node_info_t));
        rv = neu_manager_get_nodes(adapter->manager, nodes_cmd->node_type,
                                   &resp_nodes->nodes);
        if (rv < 0) {
            free(result);
            free(resp_nodes);
            rv = -1;
            break;
        }
        result->resp_type = NEU_REQRESP_NODES;
        result->req_id    = cmd->req_id;
        result->buf_len   = sizeof(neu_reqresp_nodes_t);
        result->buf       = resp_nodes;
        if (p_result != NULL) {
            *p_result = result;
        }
        break;
    }

    case NEU_REQRESP_ADD_GRP_CONFIG: {
        intptr_t        ret_code;
        neu_response_t *result;

        assert(cmd->buf_len == sizeof(neu_cmd_add_grp_config_t));
        ret_code = NEU_ERR_SUCCESS;
        result   = malloc(sizeof(neu_response_t));
        if (result == NULL) {
            log_error("Failed to allocate result for add group config");
            rv = -1;
            break;
        }

        neu_cmd_add_grp_config_t *config_add_cmd;
        config_add_cmd = (neu_cmd_add_grp_config_t *) cmd->buf;
        rv = neu_manager_add_grp_config(adapter->manager, config_add_cmd);
        if (rv != 0) {
            ret_code = NEU_ERR_FAILURE;
        }

        result->resp_type = NEU_REQRESP_ERR_CODE;
        result->req_id    = cmd->req_id;
        result->buf_len   = sizeof(intptr_t);
        result->buf       = (void *) ret_code;
        if (p_result != NULL) {
            *p_result = result;
        }
        break;
    }

    case NEU_REQRESP_DEL_GRP_CONFIG: {
        intptr_t        ret_code;
        neu_response_t *result;

        assert(cmd->buf_len == sizeof(neu_cmd_del_grp_config_t));
        ret_code = NEU_ERR_SUCCESS;
        result   = malloc(sizeof(neu_response_t));
        if (result == NULL) {
            log_error("Failed to allocate result for del group config");
            rv = -1;
            break;
        }

        neu_cmd_del_grp_config_t *config_del_cmd;
        config_del_cmd = (neu_cmd_del_grp_config_t *) cmd->buf;
        rv             = neu_manager_del_grp_config(adapter->manager,
                                        config_del_cmd->node_id,
                                        config_del_cmd->config_name);
        if (rv != 0) {
            ret_code = NEU_ERR_FAILURE;
        }

        result->resp_type = NEU_REQRESP_ERR_CODE;
        result->req_id    = cmd->req_id;
        result->buf_len   = sizeof(intptr_t);
        result->buf       = (void *) ret_code;
        if (p_result != NULL) {
            *p_result = result;
        }
        break;
    }

    case NEU_REQRESP_UPDATE_GRP_CONFIG: {
        intptr_t        ret_code;
        neu_response_t *result;

        assert(cmd->buf_len == sizeof(neu_cmd_update_grp_config_t));
        ret_code = NEU_ERR_SUCCESS;
        result   = malloc(sizeof(neu_response_t));
        if (result == NULL) {
            log_error("Failed to allocate result for update group config");
            rv = -1;
            break;
        }

        neu_cmd_update_grp_config_t *config_update_cmd;
        config_update_cmd = (neu_cmd_update_grp_config_t *) cmd->buf;
        rv = neu_manager_update_grp_config(adapter->manager, config_update_cmd);
        if (rv != 0) {
            ret_code = NEU_ERR_FAILURE;
        }

        result->resp_type = NEU_REQRESP_ERR_CODE;
        result->req_id    = cmd->req_id;
        result->buf_len   = sizeof(intptr_t);
        result->buf       = (void *) ret_code;
        if (p_result != NULL) {
            *p_result = result;
        }
        break;
    }

    case NEU_REQRESP_GET_GRP_CONFIGS: {
        neu_response_t *           result;
        neu_reqresp_grp_configs_t *resp_grp_configs;

        assert(cmd->buf_len == sizeof(neu_cmd_get_grp_configs_t));
        resp_grp_configs = malloc(sizeof(neu_reqresp_grp_configs_t));
        if (resp_grp_configs == NULL) {
            log_error("Failed to allocate result of group configs");
            rv = -1;
            break;
        }
        result = malloc(sizeof(neu_response_t));
        if (result == NULL) {
            log_error("Failed to allocate result for get group configs");
            free(resp_grp_configs);
            rv = -1;
            break;
        }

        neu_cmd_get_grp_configs_t *configs_cmd;
        configs_cmd = (neu_cmd_get_grp_configs_t *) cmd->buf;
        vector_init(&resp_grp_configs->grp_configs, DEFAULT_TAG_GROUP_COUNT,
                    sizeof(neu_taggrp_config_t *));
        rv = neu_manager_get_grp_configs(adapter->manager, configs_cmd->node_id,
                                         &resp_grp_configs->grp_configs);
        if (rv < 0) {
            free(result);
            free(resp_grp_configs);
            rv = -1;
            break;
        }
        result->resp_type = NEU_REQRESP_GRP_CONFIGS;
        result->req_id    = cmd->req_id;
        result->buf_len   = sizeof(neu_reqresp_grp_configs_t);
        result->buf       = resp_grp_configs;
        if (p_result != NULL) {
            *p_result = result;
        }
        break;
    }

    case NEU_REQRESP_GET_DATATAGS: {
        neu_response_t *        result;
        neu_reqresp_datatags_t *resp_datatags;

        assert(cmd->buf_len == sizeof(neu_cmd_get_datatags_t));
        resp_datatags = malloc(sizeof(neu_reqresp_datatags_t));
        if (resp_datatags == NULL) {
            log_error("Failed to allocate result of datatags");
            rv = -1;
            break;
        }
        result = malloc(sizeof(neu_response_t));
        if (result == NULL) {
            log_error("Failed to allocate result for get datatags");
            free(resp_datatags);
            rv = -1;
            break;
        }

        neu_cmd_get_datatags_t *datatags_cmd;
        neu_datatag_table_t *   tag_table;
        datatags_cmd = (neu_cmd_get_datatags_t *) cmd->buf;
        tag_table    = neu_manager_get_datatag_tbl(adapter->manager,
                                                datatags_cmd->node_id);
        if (tag_table == NULL) {
            free(result);
            free(resp_datatags);
            rv = -1;
            break;
        }
        resp_datatags->datatag_tbl = tag_table;

        result->resp_type = NEU_REQRESP_DATATAGS;
        result->req_id    = cmd->req_id;
        result->buf_len   = sizeof(neu_reqresp_datatags_t);
        result->buf       = resp_datatags;
        if (p_result != NULL) {
            *p_result = result;
        }
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

    log_info("Get response from plugin");
    switch (resp->resp_type) {
    case NEU_REQRESP_TRANS_DATA: {
        size_t              msg_size;
        nng_msg *           read_msg;
        void *              buf;
        size_t              buf_len;
        neu_reqresp_data_t *neu_data;
        core_databuf_t *    databuf;

        assert(resp->buf_len == sizeof(neu_reqresp_data_t));
        neu_data = (neu_reqresp_data_t *) resp->buf;
        buf_len  = neu_variable_serialize(neu_data->data_var, &buf);
        databuf  = core_databuf_new_with_buf(buf, buf_len);

        msg_size = msg_inplace_data_get_size(sizeof(neuron_databuf_t));
        rv       = nng_msg_alloc(&read_msg, msg_size);
        if (rv == 0) {
            message_t *       pay_msg;
            neuron_databuf_t *neu_databuf;

            pay_msg = (message_t *) nng_msg_body(read_msg);
            msg_inplace_data_init(pay_msg, MSG_DATA_NEURON_DATABUF,
                                  sizeof(neuron_databuf_t));
            neu_databuf = (neuron_databuf_t *) msg_get_buf_ptr(pay_msg);
            neu_databuf->grp_config = neu_data->grp_config;
            neu_databuf->databuf    = databuf;
            nng_sendmsg(adapter->sock, read_msg, 0);
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

    if (manager == NULL) {
        log_error("Create adapter with NULL manager");
        return NULL;
    }

    adapter = malloc(sizeof(neu_adapter_t));
    if (adapter == NULL) {
        log_error("Out of memeory for create adapter");
        return NULL;
    }

    int rv;
    adapter->state = ADAPTER_STATE_NULL;
    adapter->stop  = false;
    if ((rv = nng_mtx_alloc(&adapter->mtx)) != 0) {
        log_error("Can't allocate mutex for adapter");
        free(adapter);
        return NULL;
    }

    adapter->id          = info->id;
    adapter->type        = info->type;
    adapter->name        = strdup(info->name);
    adapter->new_req_id  = 1;
    adapter->plugin_id   = info->plugin_id;
    adapter->plugin_kind = info->plugin_kind;
    adapter->manager     = manager;

    adapter->plugin_lib_name = strdup(info->plugin_lib_name);
    if (adapter->name == NULL || adapter->plugin_lib_name == NULL) {
        if (adapter->name != NULL) {
            free(adapter->name);
        }
        if (adapter->plugin_lib_name != NULL) {
            free(adapter->plugin_lib_name);
        }
        nng_mtx_free(adapter->mtx);
        free(adapter);
        log_error("Failed duplicate string for create adapter");
        return NULL;
    }

    void *               handle;
    neu_plugin_module_t *plugin_module;
    handle = load_plugin_library(adapter->plugin_lib_name, adapter->plugin_kind,
                                 &plugin_module);
    if (handle == NULL) {
        neu_panic("Can't to load library(%s) for plugin(%s)",
                  adapter->plugin_lib_name, adapter->name);
    }

    neu_plugin_t *plugin;
    adapter->plugin_lib    = handle;
    adapter->plugin_module = plugin_module;
    plugin = plugin_module->intf_funs->open(adapter, &callback_funs);
    if (plugin == NULL) {
        neu_panic("Can't to open plugin(%s)", plugin_module->module_name);
    }
    adapter->plugin = plugin;

    rv = nng_pair1_open(&adapter->sock);
    if (rv != 0) {
        neu_panic("The adapter(%s) can't open pipe", adapter->name);
    }

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
    if (adapter->plugin_lib_name != NULL) {
        free(adapter->plugin_lib_name);
    }
    nng_mtx_free(adapter->mtx);
    free(adapter);
    return;
}

int neu_adapter_start(neu_adapter_t *adapter)
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

    nng_thread_create(&adapter->thrd, adapter_loop, adapter);
    return rv;
}

int neu_adapter_stop(neu_adapter_t *adapter)
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
