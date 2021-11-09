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
#include "neu_subscribe.h"

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
    adapter_callbacks_t  cb_funs;
    neu_config_t         node_setting;
    vector_t             sub_grp_configs; // neu_sub_grp_config_t
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
    nng_dialer     dialer = { 0 };

    adapter = (neu_adapter_t *) arg;
    const char *manager_url;
    manager_url = neu_manager_get_url(adapter->manager);
    rv          = nng_dial(adapter->sock, manager_url, &dialer, 0);
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
            adapter->state = ADAPTER_STATE_STOPPED;
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
            neu_reqresp_data_t            data_req;
            data_req.grp_config = databuf_ptr->grp_config;
            void * buf          = core_databuf_get_ptr(databuf_ptr->databuf);
            size_t buf_len      = core_databuf_get_len(databuf_ptr->databuf);
            neu_dvalue_deserialize(buf, buf_len, &data_req.data_val);

            intf_funs    = adapter->plugin_module->intf_funs;
            req.req_id   = adapter_get_req_id(adapter);
            req.req_type = NEU_REQRESP_TRANS_DATA;
            req.buf_len  = sizeof(neu_reqresp_data_t);
            req.buf      = (void *) &data_req;
            intf_funs->request(adapter->plugin, &req);
            neu_dvalue_free(data_req.data_val);
            core_databuf_put(databuf_ptr->databuf);
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
            neu_request_t                 req;
            neu_reqresp_read_resp_t       read_resp;

            read_resp.grp_config = cmd_ptr->grp_config;
            void * buf           = core_databuf_get_ptr(cmd_ptr->databuf);
            size_t buf_len       = core_databuf_get_len(cmd_ptr->databuf);
            neu_dvalue_deserialize(buf, buf_len, &read_resp.data_val);

            intf_funs     = adapter->plugin_module->intf_funs;
            req.req_id    = cmd_ptr->req_id;
            req.req_type  = NEU_REQRESP_READ_RESP;
            req.sender_id = 0;
            req.buf_len   = sizeof(neu_reqresp_read_resp_t);
            req.buf       = (void *) &read_resp;
            intf_funs->request(adapter->plugin, &req);
            neu_dvalue_free(read_resp.data_val);
            core_databuf_put(cmd_ptr->databuf);
            break;
        }

        case MSG_CMD_WRITE_DATA: {
            write_data_cmd_t *cmd_ptr;
            cmd_ptr = (write_data_cmd_t *) msg_get_buf_ptr(pay_msg);

            const neu_plugin_intf_funs_t *intf_funs;
            neu_request_t                 req;
            neu_reqresp_write_t           write_req;

            write_req.grp_config  = cmd_ptr->grp_config;
            write_req.dst_node_id = cmd_ptr->dst_node_id;
            void * buf            = core_databuf_get_ptr(cmd_ptr->databuf);
            size_t buf_len        = core_databuf_get_len(cmd_ptr->databuf);
            neu_dvalue_deserialize(buf, buf_len, &write_req.data_val);

            intf_funs     = adapter->plugin_module->intf_funs;
            req.req_id    = cmd_ptr->req_id;
            req.req_type  = NEU_REQRESP_WRITE_DATA;
            req.sender_id = cmd_ptr->sender_id;
            req.buf_len   = sizeof(neu_reqresp_write_t);
            req.buf       = (void *) &write_req;
            intf_funs->request(adapter->plugin, &req);
            neu_dvalue_free(write_req.data_val);
            core_databuf_put(cmd_ptr->databuf);
            break;
        }

        case MSG_DATA_WRITE_RESP: {
            write_data_resp_t *cmd_ptr;
            cmd_ptr = (write_data_resp_t *) msg_get_buf_ptr(pay_msg);

            const neu_plugin_intf_funs_t *intf_funs;
            neu_request_t                 req;
            neu_reqresp_write_resp_t      write_resp;

            write_resp.grp_config = cmd_ptr->grp_config;
            void * buf            = core_databuf_get_ptr(cmd_ptr->databuf);
            size_t buf_len        = core_databuf_get_len(cmd_ptr->databuf);
            neu_dvalue_deserialize(buf, buf_len, &write_resp.data_val);

            intf_funs     = adapter->plugin_module->intf_funs;
            req.req_id    = cmd_ptr->req_id;
            req.req_type  = NEU_REQRESP_WRITE_RESP;
            req.sender_id = 0;
            req.buf_len   = sizeof(neu_reqresp_write_resp_t);
            req.buf       = (void *) &write_resp;
            intf_funs->request(adapter->plugin, &req);
            neu_dvalue_free(write_resp.data_val);
            core_databuf_put(cmd_ptr->databuf);
            break;
        }

        case MSG_CMD_EXIT_LOOP: {
            uint32_t exit_code;

            exit_code = *(uint32_t *) msg_get_buf_ptr(pay_msg);
            log_info("adapter(%s) exit loop by exit_code=%d", adapter->name,
                     exit_code);
            nng_mtx_lock(adapter->mtx);
            adapter->state = ADAPTER_STATE_STOPPED;
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
                             uint8_t *       buf;
                             size_t          buf_len;
                             core_databuf_t *databuf;

                             buf_len = neu_dvalue_serialize(
                                 reqresp_cmd->data_val, &buf);
                             databuf = core_databuf_new_with_buf(buf, buf_len);
                             cmd_ptr->databuf = databuf;
                         })
        break;
    }

    case NEU_REQRESP_ADD_NODE: {
        ADAPTER_RESP_CODE(adapter, cmd, intptr_t, neu_cmd_add_node_t, rv,
                          NEU_REQRESP_ERR_CODE, p_result, {
                              neu_node_id_t node_id;
                              ret = neu_manager_add_node(adapter->manager,
                                                         req_cmd, &node_id);
                          });
        break;
    }

    case NEU_REQRESP_DEL_NODE: {
        ADAPTER_RESP_CODE(adapter, cmd, intptr_t, neu_cmd_del_node_t, rv,
                          NEU_REQRESP_ERR_CODE, p_result, {
                              ret = neu_manager_del_node(adapter->manager,
                                                         req_cmd->node_id);
                          });
        break;
    }

    case NEU_REQRESP_UPDATE_NODE: {
        ADAPTER_RESP_CODE(adapter, cmd, intptr_t, neu_cmd_update_node_t, rv,
                          NEU_REQRESP_ERR_CODE, p_result, {
                              ret = NEU_ERR_SUCCESS;
                              rv  = neu_manager_update_node(adapter->manager,
                                                           req_cmd);
                              if (rv != 0) {
                                  ret = NEU_ERR_FAILURE;
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
                    rv = -1;
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
                    rv = -1;
                    break;
                }
            });

        break;
    }

    case NEU_REQRESP_ADD_GRP_CONFIG: {
        ADAPTER_RESP_CODE(
            adapter, cmd, intptr_t, neu_cmd_add_grp_config_t, rv,
            NEU_REQRESP_ERR_CODE, p_result,
            { ret = neu_manager_add_grp_config(adapter->manager, req_cmd); });
        break;
    }

    case NEU_REQRESP_DEL_GRP_CONFIG: {
        ADAPTER_RESP_CODE(adapter, cmd, intptr_t, neu_cmd_del_grp_config_t, rv,
                          NEU_REQRESP_ERR_CODE, p_result, {
                              ret = neu_manager_del_grp_config(
                                  adapter->manager, req_cmd->node_id,
                                  req_cmd->config_name);
                          });
        break;
    }

    case NEU_REQRESP_UPDATE_GRP_CONFIG: {
        ADAPTER_RESP_CODE(adapter, cmd, intptr_t, neu_cmd_update_grp_config_t,
                          rv, NEU_REQRESP_ERR_CODE, p_result, {
                              ret = neu_manager_update_grp_config(
                                  adapter->manager, req_cmd);
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
                              ret = NEU_ERR_SUCCESS;
                              if (neu_manager_add_plugin_lib(adapter->manager,
                                                             req_cmd,
                                                             &plugin_id) != 0) {
                                  ret = NEU_ERR_FAILURE;
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
                              }
                          });
        break;
    }

    case NEU_REQRESP_UPDATE_PLUGIN_LIB: {
        ADAPTER_RESP_CODE(adapter, cmd, intptr_t, neu_cmd_update_plugin_lib_t,
                          rv, NEU_REQRESP_ERR_CODE, p_result, {
                              ret = NEU_ERR_SUCCESS;
                              rv  = neu_manager_update_plugin_lib(
                                  adapter->manager, req_cmd);
                              if (rv != 0) {
                                  ret = NEU_ERR_FAILURE;
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

    case NEU_REQRESP_SET_NODE_SETTING: {
        ADAPTER_RESP_CODE(adapter, cmd, intptr_t, neu_cmd_set_node_setting_t,
                          rv, NEU_REQRESP_ERR_CODE, p_result, {
                              ret = neu_manager_adapter_set_setting(
                                  adapter->manager, req_cmd->node_id,
                                  req_cmd->setting);
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
        ADAPTER_RESP_CODE(adapter, cmd, intptr_t, neu_cmd_node_ctl_t, rv,
                          NEU_REQRESP_ERR_CODE, p_result, {
                              ret = neu_manager_adapter_ctl(adapter->manager,
                                                            req_cmd->node_id,
                                                            req_cmd->ctl);
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
        uint8_t *           buf;
        size_t              buf_len;
        neu_reqresp_data_t *neu_data;
        core_databuf_t *    databuf;

        assert(resp->buf_len == sizeof(neu_reqresp_data_t));
        neu_data = (neu_reqresp_data_t *) resp->buf;
        buf_len  = neu_dvalue_serialize(neu_data->data_val, &buf);
        databuf  = core_databuf_new_with_buf(buf, buf_len);

        msg_size = msg_inplace_data_get_size(sizeof(neuron_databuf_t));
        rv       = nng_msg_alloc(&trans_data_msg, msg_size);
        if (rv == 0) {
            message_t *       pay_msg;
            neuron_databuf_t *neu_databuf;

            pay_msg = (message_t *) nng_msg_body(trans_data_msg);
            msg_inplace_data_init(pay_msg, MSG_DATA_NEURON_DATABUF,
                                  sizeof(neuron_databuf_t));
            neu_databuf = (neuron_databuf_t *) msg_get_buf_ptr(pay_msg);
            neu_databuf->grp_config = neu_data->grp_config;
            neu_databuf->databuf    = databuf;
            nng_sendmsg(adapter->sock, trans_data_msg, 0);
        }
        neu_dvalue_free(neu_data->data_val);
        break;
    }

    case NEU_REQRESP_READ_RESP: {
        size_t                   msg_size;
        nng_msg *                read_resp_msg;
        uint8_t *                buf;
        size_t                   buf_len;
        neu_reqresp_read_resp_t *read_resp;
        core_databuf_t *         databuf;

        assert(resp->buf_len == sizeof(neu_reqresp_read_resp_t));
        read_resp = (neu_reqresp_read_resp_t *) resp->buf;
        buf_len   = neu_dvalue_serialize(read_resp->data_val, &buf);
        databuf   = core_databuf_new_with_buf(buf, buf_len);

        msg_size = msg_inplace_data_get_size(sizeof(read_data_resp_t));
        rv       = nng_msg_alloc(&read_resp_msg, msg_size);
        if (rv == 0) {
            message_t *       pay_msg;
            read_data_resp_t *read_data_resp;

            pay_msg = (message_t *) nng_msg_body(read_resp_msg);
            msg_inplace_data_init(pay_msg, MSG_DATA_READ_RESP,
                                  sizeof(read_data_resp_t));
            read_data_resp = (read_data_resp_t *) msg_get_buf_ptr(pay_msg);
            read_data_resp->grp_config = read_resp->grp_config;
            read_data_resp->databuf    = databuf;
            read_data_resp->recver_id  = resp->recver_id;
            read_data_resp->req_id     = resp->req_id;
            nng_sendmsg(adapter->sock, read_resp_msg, 0);
        }
        neu_dvalue_free(read_resp->data_val);
        break;
    }

    case NEU_REQRESP_WRITE_RESP: {
        size_t                    msg_size;
        nng_msg *                 write_resp_msg;
        uint8_t *                 buf;
        size_t                    buf_len;
        neu_reqresp_write_resp_t *write_resp;
        core_databuf_t *          databuf;

        assert(resp->buf_len == sizeof(neu_reqresp_write_resp_t));
        write_resp = (neu_reqresp_write_resp_t *) resp->buf;
        buf_len    = neu_dvalue_serialize(write_resp->data_val, &buf);
        databuf    = core_databuf_new_with_buf(buf, buf_len);

        msg_size = msg_inplace_data_get_size(sizeof(write_data_resp_t));
        rv       = nng_msg_alloc(&write_resp_msg, msg_size);
        if (rv == 0) {
            message_t *        pay_msg;
            write_data_resp_t *write_data_resp;

            pay_msg = (message_t *) nng_msg_body(write_resp_msg);
            msg_inplace_data_init(pay_msg, MSG_DATA_WRITE_RESP,
                                  sizeof(write_data_resp_t));
            write_data_resp = (write_data_resp_t *) msg_get_buf_ptr(pay_msg);
            write_data_resp->grp_config = write_resp->grp_config;
            write_data_resp->databuf    = databuf;
            write_data_resp->recver_id  = resp->recver_id;
            write_data_resp->req_id     = resp->req_id;
            nng_sendmsg(adapter->sock, write_resp_msg, 0);
        }
        neu_dvalue_free(write_resp->data_val);
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
    adapter->state = ADAPTER_STATE_IDLE;
    adapter->stop  = false;
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
    adapter->new_req_id  = 1;
    adapter->plugin_id   = info->plugin_id;
    adapter->plugin_kind = info->plugin_kind;
    adapter->manager     = manager;

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
    nng_mtx_lock(adapter->sub_grp_mtx);
    vector_uninit(&adapter->sub_grp_configs);
    nng_mtx_unlock(adapter->sub_grp_mtx);

    nng_mtx_free(adapter->mtx);
    nng_mtx_free(adapter->sub_grp_mtx);
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

    adapter->stop = false;
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
        return -1;
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
        }
    }

    return rv;
}

int neu_adapter_get_setting(neu_adapter_t *adapter, char **config)
{
    if (adapter == NULL) {
        log_error("Get Config adapter with NULL adapter");
        return -1;
    }

    if (adapter->node_setting.buf != NULL) {
        *config = strdup(adapter->node_setting.buf);
        return 0;
    }

    return -1;
}

neu_plugin_state_t neu_adapter_get_state(neu_adapter_t *adapter)
{
    neu_plugin_common_t *common = neu_plugin_to_plugin_common(adapter->plugin);

    return common->state;
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
        neu_sub_grp_config_t *sgc = calloc(1, sizeof(neu_sub_grp_config_t));
        sgc->node_id              = node_id;
        sgc->group_config_name    = name;

        vector_push_back(&adapter->sub_grp_configs, sgc);
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