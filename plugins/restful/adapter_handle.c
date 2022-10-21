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

#include "parser/neu_json_node.h"
#include "plugin.h"
#include "utils/log.h"
#include "json/neu_json_error.h"
#include "json/neu_json_fn.h"

#include "adapter.h"
#include "handle.h"
#include "http.h"

#include "adapter_handle.h"

void handle_add_adapter(nng_aio *aio)
{
    neu_plugin_t *plugin = neu_rest_get_plugin();

    REST_PROCESS_HTTP_REQUEST_VALIDATE_JWT(
        aio, neu_json_add_node_req_t, neu_json_decode_add_node_req, {
            if (strlen(req->name) > NEU_NODE_NAME_LEN) {
                NEU_JSON_RESPONSE_ERROR(NEU_ERR_NODE_NAME_TOO_LONG, {
                    http_response(aio, NEU_ERR_NODE_NAME_TOO_LONG,
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
                        http_response(aio, NEU_ERR_IS_BUSY, result_error);
                    });
                }
            }
        })
}

void handle_del_adapter(nng_aio *aio)
{
    neu_plugin_t *plugin = neu_rest_get_plugin();

    REST_PROCESS_HTTP_REQUEST_VALIDATE_JWT(
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
                    http_response(aio, NEU_ERR_IS_BUSY, result_error);
                });
            }
        })
}

void handle_get_adapter(nng_aio *aio)
{
    int                ret       = 0;
    neu_plugin_t *     plugin    = neu_rest_get_plugin();
    neu_node_type_e    node_type = { 0 };
    neu_req_get_node_t cmd       = { 0 };
    neu_reqresp_head_t header    = {
        .ctx  = aio,
        .type = NEU_REQ_GET_NODE,
    };

    VALIDATE_JWT(aio);

    if (http_get_param_node_type(aio, "type", &node_type) != 0) {
        NEU_JSON_RESPONSE_ERROR(NEU_ERR_PARAM_IS_WRONG, {
            http_response(aio, error_code.error, result_error);
        })
        return;
    }

    cmd.type = node_type;
    ret      = neu_plugin_op(plugin, header, &cmd);
    if (ret != 0) {
        NEU_JSON_RESPONSE_ERROR(NEU_ERR_IS_BUSY, {
            http_response(aio, NEU_ERR_IS_BUSY, result_error);
        });
    }
}

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

    http_ok(aio, result);

    free(result);
    free(nodes_res.nodes);
    utarray_free(nodes->nodes);
}

void handle_set_node_setting(nng_aio *aio)
{
    neu_plugin_t *plugin = neu_rest_get_plugin();

    REST_PROCESS_HTTP_REQUEST_VALIDATE_JWT(
        aio, neu_json_node_setting_req_t, neu_json_decode_node_setting_req, {
            char *config_buf = calloc(req_data_size + 1, sizeof(char));
            int   ret        = 0;
            neu_reqresp_head_t     header = { 0 };
            neu_req_node_setting_t cmd    = { 0 };

            memcpy(config_buf, req_data, req_data_size);
            header.ctx  = aio;
            header.type = NEU_REQ_NODE_SETTING;
            strcpy(cmd.node, req->node);
            cmd.setting = config_buf;
            ret         = neu_plugin_op(plugin, header, &cmd);
            if (ret != 0) {
                NEU_JSON_RESPONSE_ERROR(NEU_ERR_IS_BUSY, {
                    http_response(aio, NEU_ERR_IS_BUSY, result_error);
                });
            }
        })
}

void handle_get_node_setting(nng_aio *aio)
{
    neu_plugin_t *             plugin = neu_rest_get_plugin();
    char                       node_name[NEU_NODE_NAME_LEN] = { 0 };
    int                        ret                          = 0;
    neu_reqresp_head_t         header                       = { 0 };
    neu_req_get_node_setting_t cmd                          = { 0 };

    VALIDATE_JWT(aio);

    if (http_get_param_str(aio, "node", node_name, sizeof(node_name)) <= 0) {
        NEU_JSON_RESPONSE_ERROR(NEU_ERR_PARAM_IS_WRONG, {
            http_response(aio, error_code.error, result_error);
        })
        return;
    }

    header.ctx  = aio;
    header.type = NEU_REQ_GET_NODE_SETTING;
    strcpy(cmd.ndoe, node_name);

    ret = neu_plugin_op(plugin, header, &cmd);
    if (ret != 0) {
        NEU_JSON_RESPONSE_ERROR(NEU_ERR_IS_BUSY, {
            http_response(aio, NEU_ERR_IS_BUSY, result_error);
        });
    }
}

void handle_get_node_setting_resp(nng_aio *                    aio,
                                  neu_resp_get_node_setting_t *setting)
{
    http_ok(aio, setting->setting);
    free(setting->setting);
}

void handle_node_ctl(nng_aio *aio)
{
    neu_plugin_t *plugin = neu_rest_get_plugin();

    REST_PROCESS_HTTP_REQUEST_VALIDATE_JWT(
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
                    http_response(aio, NEU_ERR_IS_BUSY, result_error);
                });
            }
        })
}

void handle_get_node_state(nng_aio *aio)
{
    neu_plugin_t *           plugin = neu_rest_get_plugin();
    char                     node_name[NEU_NODE_NAME_LEN] = { 0 };
    int                      ret                          = 0;
    neu_reqresp_head_t       header                       = { 0 };
    neu_req_get_node_state_t cmd                          = { 0 };

    VALIDATE_JWT(aio);

    header.ctx = aio;

    if (http_get_param_str(aio, "node", node_name, sizeof(node_name)) <= 0) {
        header.type = NEU_REQ_GET_NODES_STATE;
    } else {
        header.type = NEU_REQ_GET_NODE_STATE;
        strcpy(cmd.node, node_name);
    }

    ret = neu_plugin_op(plugin, header, &cmd);
    if (ret != 0) {
        NEU_JSON_RESPONSE_ERROR(NEU_ERR_IS_BUSY, {
            http_response(aio, NEU_ERR_IS_BUSY, result_error);
        });
    }
}

void handle_get_node_state_resp(nng_aio *aio, neu_resp_get_node_state_t *state)
{
    neu_json_get_node_state_resp_t res    = { 0 };
    char *                         result = NULL;

    res.link    = state->state.link;
    res.running = state->state.running;
    res.rtt     = state->rtt;

    neu_json_encode_by_fn(&res, neu_json_encode_get_node_state_resp, &result);

    http_ok(aio, result);
    free(result);
}

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
    }

    neu_json_encode_by_fn(&states_res, neu_json_encode_get_nodes_state_resp,
                          &result);

    http_ok(aio, result);

    free(result);
    free(states_res.nodes);
    utarray_free(states->states);
}