/**
 * NEURON IIoT System for Industry 4.0
 * Copyright (C) 2020-2022 EMQ Technologies Co., Ltd All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your) any later version.
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

#include "parser/neu_json_node.h"
#include "plugin.h"
#include "utils/log.h"
#include "json/neu_json_error.h"
#include "json/neu_json_fn.h"

#include "adapter.h"
#include "handle.h"
#include "utils/http.h"

#include "adapter_handle.h"

/**
 * 处理添加适配器的HTTP请求
 *
 * @param aio NNG异步I/O对象，包含HTTP请求和响应信息
 */
void handle_add_adapter(nng_aio *aio)
{
    neu_plugin_t *plugin = neu_rest_get_plugin();

    NEU_PROCESS_HTTP_REQUEST_VALIDATE_JWT(
        aio, neu_json_add_node_req_t, neu_json_decode_add_node_req, {
            if (strlen(req->name) >= NEU_NODE_NAME_LEN) {
                NEU_JSON_RESPONSE_ERROR(NEU_ERR_NODE_NAME_TOO_LONG, {
                    neu_http_response(aio, NEU_ERR_NODE_NAME_TOO_LONG,
                                      result_error);
                });

            } else {
                int                ret    = 0;
                neu_reqresp_head_t header = { 0 };
                neu_req_add_node_t cmd    = { 0 };

                header.ctx  = aio;
                header.type = NEU_REQ_ADD_NODE;
                strcpy(cmd.node, req->name);
                strcpy(cmd.plugin, req->plugin);
                ret = neu_plugin_op(plugin, header, &cmd);
                if (ret != 0) {
                    NEU_JSON_RESPONSE_ERROR(NEU_ERR_IS_BUSY, {
                        neu_http_response(aio, NEU_ERR_IS_BUSY, result_error);
                    });
                }
            }
        })
}

/**
 * 发送节点更新请求
 *
 * @param aio NNG异步I/O对象，包含HTTP请求和响应信息
 * @param req 更新节点的请求数据
 * @return 错误码，0表示成功，其他表示错误
 */
static inline int send_node_update_req(nng_aio *                   aio,
                                       neu_json_update_node_req_t *req)
{
    neu_plugin_t *plugin = neu_rest_get_plugin();

    int len = strlen(req->name);
    if (NEU_NODE_NAME_LEN <= len) {
        return NEU_ERR_NODE_NAME_TOO_LONG;
    }

    len = strlen(req->new_name);
    if (NEU_NODE_NAME_LEN <= len) {
        return NEU_ERR_NODE_NAME_TOO_LONG;
    } else if (0 == len) {
        return NEU_ERR_NODE_NAME_EMPTY;
    }

    neu_req_update_node_t cmd = { 0 };
    strcpy(cmd.node, req->name);
    strcpy(cmd.new_name, req->new_name);

    neu_reqresp_head_t header = {
        .ctx  = aio,
        .type = NEU_REQ_UPDATE_NODE,
    };

    if (0 != neu_plugin_op(plugin, header, &cmd)) {
        return NEU_ERR_IS_BUSY;
    }

    return 0;
}

/**
 * 处理更新适配器的HTTP请求
 *
 * @param aio NNG异步I/O对象，包含HTTP请求和响应信息
 */
void handle_update_adapter(nng_aio *aio)
{
    NEU_PROCESS_HTTP_REQUEST_VALIDATE_JWT(
        aio, neu_json_update_node_req_t, neu_json_decode_update_node_req, {
            int ret = send_node_update_req(aio, req);
            if (0 != ret) {
                NEU_JSON_RESPONSE_ERROR(
                    ret, { neu_http_response(aio, ret, result_error); });
            }
        })
}

/**
 * 处理删除适配器的HTTP请求
 *
 * @param aio NNG异步I/O对象，包含HTTP请求和响应信息
 */
void handle_del_adapter(nng_aio *aio)
{
    neu_plugin_t *plugin = neu_rest_get_plugin();

    NEU_PROCESS_HTTP_REQUEST_VALIDATE_JWT(
        aio, neu_json_del_node_req_t, neu_json_decode_del_node_req, {
            int                ret    = 0;
            neu_reqresp_head_t header = { 0 };
            neu_req_del_node_t cmd    = { 0 };

            header.ctx  = aio;
            header.type = NEU_REQ_DEL_NODE;
            strcpy(cmd.node, req->name);
            ret = neu_plugin_op(plugin, header, &cmd);
            if (ret != 0) {
                NEU_JSON_RESPONSE_ERROR(NEU_ERR_IS_BUSY, {
                    neu_http_response(aio, NEU_ERR_IS_BUSY, result_error);
                });
            }
        })
}

/**
 * 处理获取适配器的HTTP请求
 *
 * @param aio NNG异步I/O对象，包含HTTP请求和响应信息
 */
void handle_get_adapter(nng_aio *aio)
{
    int  ret                              = 0;
    char plugin_name[NEU_PLUGIN_NAME_LEN] = { 0 };
    char node_name[NEU_NODE_NAME_LEN]     = { 0 };

    neu_plugin_t *     plugin    = neu_rest_get_plugin();
    neu_node_type_e    node_type = { 0 };
    neu_req_get_node_t cmd       = { 0 };
    neu_reqresp_head_t header    = {
        .ctx  = aio,
        .type = NEU_REQ_GET_NODE,
    };

    NEU_VALIDATE_JWT(aio);

    if (neu_http_get_param_node_type(aio, "type", &node_type) != 0) {
        NEU_JSON_RESPONSE_ERROR(NEU_ERR_PARAM_IS_WRONG, {
            neu_http_response(aio, error_code.error, result_error);
        })
        return;
    }

    if (neu_http_get_param_str(aio, "plugin", plugin_name,
                               sizeof(plugin_name)) > 0) {
        strcpy(cmd.plugin, plugin_name);
    }

    if (neu_http_get_param_str(aio, "node", node_name, sizeof(node_name)) > 0) {
        strcpy(cmd.node, node_name);
    }

    cmd.type = node_type;
    ret      = neu_plugin_op(plugin, header, &cmd);
    if (ret != 0) {
        NEU_JSON_RESPONSE_ERROR(NEU_ERR_IS_BUSY, {
            neu_http_response(aio, NEU_ERR_IS_BUSY, result_error);
        });
    }
}

/**
 * 处理获取适配器的响应
 *
 * @param aio NNG异步I/O对象，包含HTTP请求和响应信息
 * @param nodes 节点信息的数组
 */
void handle_get_adapter_resp(nng_aio *aio, neu_resp_get_node_t *nodes)
{
    neu_json_get_nodes_resp_t nodes_res = { 0 };
    char *                    result    = NULL;

    nodes_res.n_node = utarray_len(nodes->nodes);
    nodes_res.nodes =
        calloc(nodes_res.n_node, sizeof(neu_json_get_nodes_resp_node_t));

    utarray_foreach(nodes->nodes, neu_resp_node_info_t *, info)
    {
        int index = utarray_eltidx(nodes->nodes, info);

        nodes_res.nodes[index].name   = info->node;
        nodes_res.nodes[index].plugin = info->plugin;
    }

    neu_json_encode_by_fn(&nodes_res, neu_json_encode_get_nodes_resp, &result);

    neu_http_ok(aio, result);

    free(result);
    free(nodes_res.nodes);
    utarray_free(nodes->nodes);
}

/**
 * 处理设置节点配置的HTTP请求
 *
 * @param aio NNG异步I/O对象，包含HTTP请求和响应信息
 */
void handle_set_node_setting(nng_aio *aio)
{
    neu_plugin_t *plugin = neu_rest_get_plugin();

    NEU_PROCESS_HTTP_REQUEST_VALIDATE_JWT(
        aio, neu_json_node_setting_req_t, neu_json_decode_node_setting_req, {
            int                    ret    = 0;
            neu_reqresp_head_t     header = { 0 };
            neu_req_node_setting_t cmd    = { 0 };

            header.ctx  = aio;
            header.type = NEU_REQ_NODE_SETTING;
            strcpy(cmd.node, req->node);
            cmd.setting  = req->setting;
            req->setting = NULL; // ownership moved
            ret          = neu_plugin_op(plugin, header, &cmd);
            if (ret != 0) {
                NEU_JSON_RESPONSE_ERROR(NEU_ERR_IS_BUSY, {
                    neu_http_response(aio, NEU_ERR_IS_BUSY, result_error);
                });
            }
        })
}

/**
 * 处理获取节点配置的HTTP请求
 *
 * @param aio NNG异步I/O对象，包含HTTP请求和响应信息
 */
void handle_get_node_setting(nng_aio *aio)
{
    neu_plugin_t *             plugin = neu_rest_get_plugin();
    char                       node_name[NEU_NODE_NAME_LEN] = { 0 };
    int                        ret                          = 0;
    neu_reqresp_head_t         header                       = { 0 };
    neu_req_get_node_setting_t cmd                          = { 0 };

    NEU_VALIDATE_JWT(aio);

    if (neu_http_get_param_str(aio, "node", node_name, sizeof(node_name)) <=
        0) {
        NEU_JSON_RESPONSE_ERROR(NEU_ERR_PARAM_IS_WRONG, {
            neu_http_response(aio, error_code.error, result_error);
        })
        return;
    }

    header.ctx  = aio;
    header.type = NEU_REQ_GET_NODE_SETTING;
    strcpy(cmd.node, node_name);

    ret = neu_plugin_op(plugin, header, &cmd);
    if (ret != 0) {
        NEU_JSON_RESPONSE_ERROR(NEU_ERR_IS_BUSY, {
            neu_http_response(aio, NEU_ERR_IS_BUSY, result_error);
        });
    }
}

/**
 * 处理获取节点配置的响应
 *
 * @param aio NNG异步I/O对象，包含HTTP请求和响应信息
 * @param setting 节点配置信息
 */
void handle_get_node_setting_resp(nng_aio *                    aio,
                                  neu_resp_get_node_setting_t *setting)
{
    char *                           json_str = NULL;
    neu_json_get_node_setting_resp_t resp     = {
        .node    = setting->node,
        .setting = setting->setting,
    };

    if (0 !=
        neu_json_encode_by_fn(&resp, neu_json_encode_get_node_setting_resp,
                              &json_str)) {
        NEU_JSON_RESPONSE_ERROR(NEU_ERR_EINTERNAL, {
            neu_http_response(aio, error_code.error, result_error);
        })
    } else {
        neu_http_ok(aio, json_str);
    }

    free(setting->setting);
    free(json_str);
}

/**
 * 处理节点控制的HTTP请求
 *
 * @param aio NNG异步I/O对象，包含HTTP请求和响应信息
 */
void handle_node_ctl(nng_aio *aio)
{
    neu_plugin_t *plugin = neu_rest_get_plugin();

    NEU_PROCESS_HTTP_REQUEST_VALIDATE_JWT(
        aio, neu_json_node_ctl_req_t, neu_json_decode_node_ctl_req, {
            int                ret    = 0;
            neu_reqresp_head_t header = { 0 };
            neu_req_node_ctl_t cmd    = { 0 };

            header.ctx  = aio;
            header.type = NEU_REQ_NODE_CTL;
            strcpy(cmd.node, req->node);
            cmd.ctl = req->cmd;

            ret = neu_plugin_op(plugin, header, &cmd);
            if (ret != 0) {
                NEU_JSON_RESPONSE_ERROR(NEU_ERR_IS_BUSY, {
                    neu_http_response(aio, NEU_ERR_IS_BUSY, result_error);
                });
            }
        })
}

/**
 * 处理获取节点状态的HTTP请求
 *
 * @param aio NNG异步I/O对象，包含HTTP请求和响应信息
 */
void handle_get_node_state(nng_aio *aio)
{
    neu_plugin_t *           plugin = neu_rest_get_plugin();
    char                     node_name[NEU_NODE_NAME_LEN] = { 0 };
    int                      ret                          = 0;
    neu_reqresp_head_t       header                       = { 0 };
    neu_req_get_node_state_t cmd                          = { 0 };

    NEU_VALIDATE_JWT(aio);

    header.ctx = aio;

    if (neu_http_get_param_str(aio, "node", node_name, sizeof(node_name)) <=
        0) {
        header.type = NEU_REQ_GET_NODES_STATE;
    } else {
        header.type = NEU_REQ_GET_NODE_STATE;
        strcpy(cmd.node, node_name);
    }

    ret = neu_plugin_op(plugin, header, &cmd);
    if (ret != 0) {
        NEU_JSON_RESPONSE_ERROR(NEU_ERR_IS_BUSY, {
            neu_http_response(aio, NEU_ERR_IS_BUSY, result_error);
        });
    }
}

/**
 * 处理获取单个节点状态的响应
 *
 * @param aio NNG异步I/O对象，包含HTTP请求和响应信息
 * @param state 节点状态信息
 */
void handle_get_node_state_resp(nng_aio *aio, neu_resp_get_node_state_t *state)
{
    neu_json_get_node_state_resp_t res    = { 0 };
    char *                         result = NULL;

    res.link       = state->state.link;
    res.running    = state->state.running;
    res.rtt        = state->rtt;
    res.log_level  = (char *) log_level_to_str(state->state.log_level);
    res.core_level = (char *) log_level_to_str(state->core_level);

    neu_json_encode_by_fn(&res, neu_json_encode_get_node_state_resp, &result);

    neu_http_ok(aio, result);
    free(result);
}

/**
 * 处理获取多个节点状态的响应
 *
 * @param aio NNG异步I/O对象，包含HTTP请求和响应信息
 * @param states 多个节点状态信息
 */
void handle_get_nodes_state_resp(nng_aio *                   aio,
                                 neu_resp_get_nodes_state_t *states)
{
    neu_json_get_nodes_state_resp_t states_res = { 0 };
    char *                          result     = NULL;

    states_res.n_node = utarray_len(states->states);
    states_res.nodes =
        calloc(states_res.n_node, sizeof(neu_json_get_nodes_state_t));

    utarray_foreach(states->states, neu_nodes_state_t *, state)
    {
        int index                       = utarray_eltidx(states->states, state);
        states_res.nodes[index].name    = state->node;
        states_res.nodes[index].running = state->state.running;
        states_res.nodes[index].link    = state->state.link;
        states_res.nodes[index].rtt     = state->rtt;
        states_res.nodes[index].log_level =
            (char *) log_level_to_str(state->state.log_level);
    }

    states_res.core_level = (char *) log_level_to_str(states->core_level);

    neu_json_encode_by_fn(&states_res, neu_json_encode_get_nodes_state_resp,
                          &result);

    neu_http_ok(aio, result);

    free(result);
    free(states_res.nodes);
    utarray_free(states->states);
}