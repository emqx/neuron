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

#include "adapter.h"
#include "define.h"
#include "errcodes.h"
#include "plugin.h"
#include "utils/log.h"
#include "json/neu_json_error.h"
#include "json/neu_json_fn.h"

#include "adapter_handle.h"
#include "handle.h"
#include "parser/neu_json_global_config.h"
#include "parser/neu_json_node.h"
#include "utils/http.h"
#include "utils/set.h"
#include "utils/utextend.h"
#include "utils/uthash.h"

void handle_add_adapter(nng_aio *aio)
{
    neu_plugin_t *plugin = neu_rest_get_plugin();

    NEU_PROCESS_HTTP_REQUEST_VALIDATE_JWT(
        aio, neu_json_add_node_req_t, neu_json_decode_add_node_req, {
            if (strlen(req->name) >= NEU_NODE_NAME_LEN) {
                CHECK_NODE_NAME_LENGTH_ERR;
            } else if (strcmp(req->name, "monitor") == 0) {
                CHECK_NODE_MONITOR_ERR;
            } else {
                int                ret    = 0;
                neu_reqresp_head_t header = { 0 };
                neu_req_add_node_t cmd    = { 0 };
                header.otel_trace_type    = NEU_OTEL_TRACE_TYPE_REST_COMM;

                header.ctx  = aio;
                header.type = NEU_REQ_ADD_NODE;
                strcpy(cmd.node, req->name);
                strcpy(cmd.plugin, req->plugin);
                cmd.setting  = req->setting;
                req->setting = NULL; // moved
                ret          = neu_plugin_op(plugin, header, &cmd);
                if (ret != 0) {
                    neu_req_add_node_fini(&cmd);
                    NEU_JSON_RESPONSE_ERROR(NEU_ERR_IS_BUSY, {
                        neu_http_response(aio, NEU_ERR_IS_BUSY, result_error);
                    });
                }
            }
        })
}

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

    if (strcmp(req->new_name, "monitor") == 0 ||
        strcmp(req->name, "monitor") == 0) {
        return NEU_ERR_NODE_NOT_ALLOW_UPDATE;
    }

    neu_req_update_node_t cmd = { 0 };
    strcpy(cmd.node, req->name);
    strcpy(cmd.new_name, req->new_name);

    neu_reqresp_head_t header = {
        .ctx             = aio,
        .type            = NEU_REQ_UPDATE_NODE,
        .otel_trace_type = NEU_OTEL_TRACE_TYPE_REST_COMM,
    };

    if (0 != neu_plugin_op(plugin, header, &cmd)) {
        return NEU_ERR_IS_BUSY;
    }

    return 0;
}

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

void handle_del_adapter(nng_aio *aio)
{
    neu_plugin_t *plugin = neu_rest_get_plugin();

    NEU_PROCESS_HTTP_REQUEST_VALIDATE_JWT(
        aio, neu_json_del_node_req_t, neu_json_decode_del_node_req, {
            if (strlen(req->name) >= NEU_NODE_NAME_LEN) {
                CHECK_NODE_NAME_LENGTH_ERR;
            } else {
                int                ret    = 0;
                neu_reqresp_head_t header = { 0 };
                neu_req_del_node_t cmd    = { 0 };
                header.otel_trace_type    = NEU_OTEL_TRACE_TYPE_REST_COMM;

                header.ctx  = aio;
                header.type = NEU_REQ_DEL_NODE;
                strcpy(cmd.node, req->name);
                ret = neu_plugin_op(plugin, header, &cmd);
                if (ret != 0) {
                    NEU_JSON_RESPONSE_ERROR(NEU_ERR_IS_BUSY, {
                        neu_http_response(aio, NEU_ERR_IS_BUSY, result_error);
                    });
                }
            }
        })
}

void handle_get_adapter(nng_aio *aio)
{
    int  ret                              = 0;
    char plugin_name[NEU_PLUGIN_NAME_LEN] = { 0 };
    char node_name[NEU_NODE_NAME_LEN]     = { 0 };

    neu_plugin_t *     plugin    = neu_rest_get_plugin();
    neu_node_type_e    node_type = { 0 };
    neu_req_get_node_t cmd       = { 0 };
    neu_reqresp_head_t header    = {
        .ctx             = aio,
        .type            = NEU_REQ_GET_NODE,
        .otel_trace_type = NEU_OTEL_TRACE_TYPE_REST_COMM,
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

void handle_set_node_setting(nng_aio *aio)
{
    neu_plugin_t *plugin = neu_rest_get_plugin();

    NEU_PROCESS_HTTP_REQUEST_VALIDATE_JWT(
        aio, neu_json_node_setting_req_t, neu_json_decode_node_setting_req, {
            if (strlen(req->node) >= NEU_NODE_NAME_LEN) {
                CHECK_NODE_NAME_LENGTH_ERR;
            } else {
                int                    ret    = 0;
                neu_reqresp_head_t     header = { 0 };
                neu_req_node_setting_t cmd    = { 0 };
                header.otel_trace_type        = NEU_OTEL_TRACE_TYPE_REST_COMM;

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

    NEU_VALIDATE_JWT(aio);

    if (neu_http_get_param_str(aio, "node", node_name, sizeof(node_name)) <=
        0) {
        NEU_JSON_RESPONSE_ERROR(NEU_ERR_PARAM_IS_WRONG, {
            neu_http_response(aio, error_code.error, result_error);
        })
        return;
    }

    header.ctx             = aio;
    header.type            = NEU_REQ_GET_NODE_SETTING;
    header.otel_trace_type = NEU_OTEL_TRACE_TYPE_REST_COMM;
    strcpy(cmd.node, node_name);

    ret = neu_plugin_op(plugin, header, &cmd);
    if (ret != 0) {
        NEU_JSON_RESPONSE_ERROR(NEU_ERR_IS_BUSY, {
            neu_http_response(aio, NEU_ERR_IS_BUSY, result_error);
        });
    }
}

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

void handle_node_ctl(nng_aio *aio)
{
    neu_plugin_t *plugin = neu_rest_get_plugin();

    NEU_PROCESS_HTTP_REQUEST_VALIDATE_JWT(
        aio, neu_json_node_ctl_req_t, neu_json_decode_node_ctl_req, {
            if (strlen(req->node) >= NEU_NODE_NAME_LEN) {
                CHECK_NODE_NAME_LENGTH_ERR;
            } else {
                int                ret    = 0;
                neu_reqresp_head_t header = { 0 };
                neu_req_node_ctl_t cmd    = { 0 };
                header.otel_trace_type    = NEU_OTEL_TRACE_TYPE_REST_COMM;

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

    NEU_VALIDATE_JWT(aio);

    header.ctx             = aio;
    header.otel_trace_type = NEU_OTEL_TRACE_TYPE_REST_COMM;

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

void handle_get_node_state_resp(nng_aio *aio, neu_resp_get_node_state_t *state)
{
    neu_json_get_node_state_resp_t res    = { 0 };
    char *                         result = NULL;

    res.link            = state->state.link;
    res.running         = state->state.running;
    res.rtt             = state->rtt;
    res.sub_group_count = state->sub_group_count;
    res.is_driver       = state->is_driver;
    res.log_level       = (char *) log_level_to_str(state->state.log_level);
    res.core_level      = (char *) log_level_to_str(state->core_level);

    neu_json_encode_by_fn(&res, neu_json_encode_get_node_state_resp, &result);

    neu_http_ok(aio, result);
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
        states_res.nodes[index].sub_group_count = state->sub_group_count;
        states_res.nodes[index].is_driver       = state->is_driver;
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

static int json_to_gdatatags(neu_json_gtag_array_t *gtag_array,
                             neu_gdatatag_t **      gdatatags_p)
{
    neu_gdatatag_t *gdatatags = calloc(gtag_array->len, sizeof(neu_gdatatag_t));
    if (NULL == gdatatags) {
        return NEU_ERR_EINTERNAL;
    }

    for (int i = 0; i < gtag_array->len; i++) {
        strcpy(gdatatags[i].group, gtag_array->gtags[i].group);
        gdatatags[i].n_tag    = gtag_array->gtags[i].n_tag;
        gdatatags[i].interval = gtag_array->gtags[i].interval;
        gdatatags[i].tags =
            calloc(gtag_array->gtags[i].n_tag, sizeof(neu_datatag_t));

        for (int j = 0; j < gtag_array->gtags[i].n_tag; j++) {
            gdatatags[i].tags[j].attribute =
                gtag_array->gtags[i].tags[j].attribute;
            gdatatags[i].tags[j].type = gtag_array->gtags[i].tags[j].type;
            gdatatags[i].tags[j].precision =
                gtag_array->gtags[i].tags[j].precision;
            gdatatags[i].tags[j].decimal = gtag_array->gtags[i].tags[j].decimal;
            gdatatags[i].tags[j].bias    = gtag_array->gtags[i].tags[j].bias;
            gdatatags[i].tags[j].address = gtag_array->gtags[i].tags[j].address;
            gdatatags[i].tags[j].name    = gtag_array->gtags[i].tags[j].name;
            if (gtag_array->gtags[i].tags[j].description != NULL) {
                gdatatags[i].tags[j].description =
                    gtag_array->gtags[i].tags[j].description;
            } else {
                gdatatags[i].tags[j].description = strdup("");
            }

            gtag_array->gtags[i].tags[j].address     = NULL; // moved
            gtag_array->gtags[i].tags[j].name        = NULL; // moved
            gtag_array->gtags[i].tags[j].description = NULL; // moved
        }
    }

    *gdatatags_p = gdatatags;
    return 0;
}

static int send_drivers(nng_aio *aio, neu_json_drivers_req_t *req)
{
    int           ret    = 0;
    neu_plugin_t *plugin = neu_rest_get_plugin();

    // fast check
    neu_strset_t node_seen  = NULL;
    neu_strset_t group_seen = NULL;
    neu_strset_t tag_seen   = NULL;
    for (int i = 0; i < req->n_driver; ++i) {
        neu_json_driver_t *driver = &req->drivers[i];
        if (strlen(driver->node.name) >= NEU_NODE_NAME_LEN) {
            ret = NEU_ERR_NODE_NAME_TOO_LONG;
            goto check_end;
        }
        if (strlen(driver->node.plugin) >= NEU_PLUGIN_NAME_LEN) {
            ret = NEU_ERR_PLUGIN_NAME_TOO_LONG;
            goto check_end;
        }
        if (driver->gtags.len > NEU_GROUP_MAX_PER_NODE) {
            ret = NEU_ERR_GROUP_MAX_GROUPS;
            goto check_end;
        }
        if (1 != neu_strset_add(&node_seen, driver->node.name)) {
            // duplicate node name
            ret = NEU_ERR_BODY_IS_WRONG;
            goto check_end;
        }
        for (int j = 0; j < driver->gtags.len; j++) {
            if (strlen(driver->gtags.gtags[j].group) >= NEU_GROUP_NAME_LEN) {
                ret = NEU_ERR_GROUP_NAME_TOO_LONG;
                goto check_end;
            }
            if (driver->gtags.gtags[j].interval < NEU_DEFAULT_GROUP_INTERVAL) {
                ret = NEU_ERR_GROUP_PARAMETER_INVALID;
                goto check_end;
            }
            if (1 !=
                neu_strset_add(&group_seen, driver->gtags.gtags[j].group)) {
                // duplicate group name
                ret = NEU_ERR_BODY_IS_WRONG;
                goto check_end;
            }
            for (int k = 0; k < driver->gtags.gtags[j].n_tag; k++) {
                if (strlen(driver->gtags.gtags[j].tags[k].name) >=
                    NEU_TAG_NAME_LEN) {
                    ret = NEU_ERR_TAG_NAME_TOO_LONG;
                    goto check_end;
                }

                if (strlen(driver->gtags.gtags[j].tags[k].address) >=
                    NEU_TAG_ADDRESS_LEN) {
                    ret = NEU_ERR_TAG_ADDRESS_TOO_LONG;
                    goto check_end;
                }

                if (strlen(driver->gtags.gtags[j].tags[k].description) >=
                    NEU_TAG_DESCRIPTION_LEN) {
                    ret = NEU_ERR_TAG_DESCRIPTION_TOO_LONG;
                    goto check_end;
                }

                if (driver->gtags.gtags[j].tags[k].precision >
                    NEU_TAG_FLOAG_PRECISION_MAX) {
                    ret = NEU_ERR_TAG_PRECISION_INVALID;
                    goto check_end;
                }

                if (1 !=
                    neu_strset_add(&tag_seen,
                                   driver->gtags.gtags[j].tags[k].name)) {
                    // duplicate tag name
                    ret = NEU_ERR_BODY_IS_WRONG;
                    goto check_end;
                }
            }
            neu_strset_free(&tag_seen);
        }
        neu_strset_free(&group_seen);
    }
check_end:
    neu_strset_free(&node_seen);
    neu_strset_free(&group_seen);
    neu_strset_free(&tag_seen);
    if (0 != ret) {
        return ret;
    }

    neu_reqresp_head_t header = {
        .ctx             = aio,
        .type            = NEU_REQ_ADD_DRIVERS,
        .otel_trace_type = NEU_OTEL_TRACE_TYPE_REST_COMM,
    };

    neu_req_driver_array_t cmd = { 0 };
    cmd.drivers                = calloc(req->n_driver, sizeof(cmd.drivers[0]));
    if (NULL == cmd.drivers) {
        return NEU_ERR_EINTERNAL;
    }

    for (int i = 0; i < req->n_driver; ++i) {
        neu_json_driver_t *driver = &req->drivers[i];
        cmd.drivers[i].node       = driver->node.name;
        cmd.drivers[i].plugin     = driver->node.plugin;
        cmd.drivers[i].setting    = driver->node.setting;
        cmd.drivers[i].n_group    = driver->gtags.len;
        driver->node.name         = NULL; // moved
        driver->node.plugin       = NULL; // moved
        driver->node.setting      = NULL; // moved
        cmd.n_driver += 1;

        ret = json_to_gdatatags(&driver->gtags, &cmd.drivers[i].groups);
        if (0 != ret) {
            neu_req_driver_array_fini(&cmd);
            return ret;
        }
    }

    if (0 != neu_plugin_op(plugin, header, &cmd)) {
        neu_req_driver_array_fini(&cmd);
        return NEU_ERR_IS_BUSY;
    }

    return 0;
}

void handle_put_drivers(nng_aio *aio)
{
    NEU_PROCESS_HTTP_REQUEST_VALIDATE_JWT(
        aio, neu_json_drivers_req_t, neu_json_decode_drivers_req, {
            int ret = send_drivers(aio, req);
            if (ret != 0) {
                NEU_JSON_RESPONSE_ERROR(
                    ret, { neu_http_response(aio, ret, result_error); });
            }
        })
}
