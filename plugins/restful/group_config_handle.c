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

#include "parser/neu_json_group_config.h"
#include "plugin.h"
#include "json/neu_json_error.h"
#include "json/neu_json_fn.h"

#include "handle.h"
#include "utils/http.h"

#include "group_config_handle.h"

void handle_add_group_config(nng_aio *aio)
{
    neu_plugin_t *plugin = neu_rest_get_plugin();
    NEU_PROCESS_HTTP_REQUEST_VALIDATE_JWT(
        aio, neu_json_add_group_config_req_t,
        neu_json_decode_add_group_config_req, {
            if (strlen(req->node) >= NEU_NODE_NAME_LEN) {
                CHECK_NODE_NAME_LENGTH_ERR;
            } else if (strlen(req->group) >= NEU_GROUP_NAME_LEN) {
                CHECK_GROUP_NAME_LENGTH_ERR;
            } else if (req->interval < NEU_GROUP_INTERVAL_LIMIT) {
                CHECK_GROUP_INTERVAL_ERR;
            } else {
                int                 ret    = 0;
                neu_reqresp_head_t  header = { 0 };
                neu_req_add_group_t cmd    = { 0 };
                header.ctx                 = aio;
                header.type                = NEU_REQ_ADD_GROUP;
                header.otel_trace_type     = NEU_OTEL_TRACE_TYPE_REST_COMM;
                strcpy(cmd.driver, req->node);
                strcpy(cmd.group, req->group);
                cmd.interval = req->interval;
                ret          = neu_plugin_op(plugin, header, &cmd);
                if (ret != 0) {
                    NEU_JSON_RESPONSE_ERROR(NEU_ERR_IS_BUSY, {
                        neu_http_response(aio, NEU_ERR_IS_BUSY, result_error);
                    });
                }
            }
        })
}

static inline int send_update_group(nng_aio *                           aio,
                                    neu_json_update_group_config_req_t *req)
{
    neu_plugin_t *plugin = neu_rest_get_plugin();

    if (strlen(req->node) >= NEU_NODE_NAME_LEN) {
        return NEU_ERR_NODE_NAME_TOO_LONG;
    } else if (strlen(req->group) >= NEU_GROUP_NAME_LEN) {
        return NEU_ERR_GROUP_NAME_TOO_LONG;
    } else if (req->set_interval &&
               (req->interval < NEU_GROUP_INTERVAL_LIMIT ||
                req->interval > UINT32_MAX)) {
        return NEU_ERR_GROUP_PARAMETER_INVALID;
    }

    // for backward compatibility,
    // `new_name` or `interval` (inclusive) should be provided
    if (!req->new_name && !req->set_interval) {
        return NEU_ERR_BODY_IS_WRONG;
    }

    neu_req_update_group_t cmd = { 0 };

    if (req->new_name) {
        // if `new_name` is provided, then it should be valid
        int len = strlen(req->new_name);
        if (0 == len) {
            return NEU_ERR_BODY_IS_WRONG;
        } else if (len >= NEU_GROUP_NAME_LEN) {
            return NEU_ERR_GROUP_NAME_TOO_LONG;
        }
        strcpy(cmd.new_name, req->new_name);
    } else {
        // if `new_name` is omitted, then keep the node name
        strcpy(cmd.new_name, req->group);
    }

    strcpy(cmd.driver, req->node);
    strcpy(cmd.group, req->group);
    cmd.interval = req->set_interval ? req->interval : 0;

    neu_reqresp_head_t header = {
        .ctx             = aio,
        .type            = NEU_REQ_UPDATE_GROUP,
        .otel_trace_type = NEU_OTEL_TRACE_TYPE_REST_COMM,
    };

    if (0 != neu_plugin_op(plugin, header, &cmd)) {
        return NEU_ERR_IS_BUSY;
    }

    return 0;
}

void handle_update_group(nng_aio *aio)
{
    NEU_PROCESS_HTTP_REQUEST_VALIDATE_JWT(
        aio, neu_json_update_group_config_req_t,
        neu_json_decode_update_group_config_req, {
            int ret = send_update_group(aio, req);
            if (ret != 0) {
                NEU_JSON_RESPONSE_ERROR(
                    ret, { neu_http_response(aio, ret, result_error); });
            }
        })
}

void handle_del_group_config(nng_aio *aio)
{
    neu_plugin_t *plugin = neu_rest_get_plugin();
    NEU_PROCESS_HTTP_REQUEST_VALIDATE_JWT(
        aio, neu_json_del_group_config_req_t,
        neu_json_decode_del_group_config_req, {
            if (strlen(req->node) >= NEU_NODE_NAME_LEN) {
                CHECK_NODE_NAME_LENGTH_ERR;
            } else if (strlen(req->group) >= NEU_GROUP_NAME_LEN) {
                CHECK_GROUP_NAME_LENGTH_ERR;
            } else {
                int                 ret    = 0;
                neu_reqresp_head_t  header = { 0 };
                neu_req_del_group_t cmd    = { 0 };

                header.ctx             = aio;
                header.type            = NEU_REQ_DEL_GROUP;
                header.otel_trace_type = NEU_OTEL_TRACE_TYPE_REST_COMM;
                strcpy(cmd.driver, req->node);
                strcpy(cmd.group, req->group);
                ret = neu_plugin_op(plugin, header, &cmd);
                if (ret != 0) {
                    NEU_JSON_RESPONSE_ERROR(NEU_ERR_IS_BUSY, {
                        neu_http_response(aio, NEU_ERR_IS_BUSY, result_error);
                    });
                }
            }
        })
}

void handle_get_group_config(nng_aio *aio)
{
    neu_plugin_t *      plugin                       = neu_rest_get_plugin();
    char                node_name[NEU_NODE_NAME_LEN] = { 0 };
    int                 ret                          = 0;
    neu_req_get_group_t cmd                          = { 0 };
    neu_reqresp_head_t  header                       = {
        .ctx             = aio,
        .type            = NEU_REQ_GET_GROUP,
        .otel_trace_type = NEU_OTEL_TRACE_TYPE_REST_COMM,
    };

    NEU_VALIDATE_JWT(aio);

    neu_http_get_param_str(aio, "node", node_name, sizeof(node_name));
    strcpy(cmd.driver, node_name);
    if (strlen(cmd.driver) == 0) {
        header.type = NEU_REQ_GET_DRIVER_GROUP;
    }

    ret = neu_plugin_op(plugin, header, &cmd);
    if (ret != 0) {
        NEU_JSON_RESPONSE_ERROR(NEU_ERR_IS_BUSY, {
            neu_http_response(aio, NEU_ERR_IS_BUSY, result_error);
        });
    }
}

void handle_get_group_resp(nng_aio *aio, neu_resp_get_group_t *groups)
{
    neu_json_get_group_config_resp_t gconfig_res = { 0 };
    char *                           result      = NULL;

    gconfig_res.n_group_config = utarray_len(groups->groups);
    gconfig_res.group_configs =
        calloc(gconfig_res.n_group_config,
               sizeof(neu_json_get_group_config_resp_group_config_t));

    utarray_foreach(groups->groups, neu_resp_group_info_t *, group)
    {
        int index = utarray_eltidx(groups->groups, group);
        gconfig_res.group_configs[index].name      = group->name;
        gconfig_res.group_configs[index].interval  = group->interval;
        gconfig_res.group_configs[index].tag_count = group->tag_count;
    }

    neu_json_encode_by_fn(&gconfig_res, neu_json_encode_get_group_config_resp,
                          &result);

    neu_http_ok(aio, result);

    free(result);
    free(gconfig_res.group_configs);
    utarray_free(groups->groups);
}

void handle_get_driver_group_resp(nng_aio *                    aio,
                                  neu_resp_get_driver_group_t *groups)
{
    neu_json_get_driver_group_resp_t gconfig_res = { 0 };
    char *                           result      = NULL;

    gconfig_res.n_group = utarray_len(groups->groups);
    gconfig_res.groups  = calloc(gconfig_res.n_group,
                                sizeof(neu_json_get_driver_group_resp_group_t));

    utarray_foreach(groups->groups, neu_resp_driver_group_info_t *, group)
    {
        int index = utarray_eltidx(groups->groups, group);
        gconfig_res.groups[index].driver    = group->driver;
        gconfig_res.groups[index].group     = group->group;
        gconfig_res.groups[index].interval  = group->interval;
        gconfig_res.groups[index].tag_count = group->tag_count;
    }

    neu_json_encode_by_fn(&gconfig_res, neu_json_encode_get_driver_group_resp,
                          &result);

    neu_http_ok(aio, result);

    free(result);
    free(gconfig_res.groups);
    utarray_free(groups->groups);
}

static inline int send_subscribe(nng_aio *aio, neu_reqresp_type_e type,
                                 neu_json_subscribe_req_t *req)
{
    neu_plugin_t *plugin = neu_rest_get_plugin();

    neu_reqresp_head_t header = {
        .ctx             = aio,
        .type            = type,
        .otel_trace_type = NEU_OTEL_TRACE_TYPE_REST_COMM,
    };

    if (strlen(req->app) >= NEU_NODE_NAME_LEN) {
        return NEU_ERR_NODE_NAME_TOO_LONG;
    }

    if (strlen(req->driver) >= NEU_NODE_NAME_LEN) {
        return NEU_ERR_NODE_NAME_TOO_LONG;
    }

    if (strlen(req->group) >= NEU_GROUP_NAME_LEN) {
        return NEU_ERR_GROUP_NAME_TOO_LONG;
    }

    neu_req_subscribe_t cmd = { 0 };
    strcpy(cmd.app, req->app);
    strcpy(cmd.driver, req->driver);
    strcpy(cmd.group, req->group);
    cmd.params  = req->params; // ownership moved
    req->params = NULL;

    if (0 != neu_plugin_op(plugin, header, &cmd)) {
        return NEU_ERR_IS_BUSY;
    }

    return 0;
}

void handle_grp_subscribe(nng_aio *aio)
{
    NEU_PROCESS_HTTP_REQUEST_VALIDATE_JWT(
        aio, neu_json_subscribe_req_t, neu_json_decode_subscribe_req, {
            int ret = send_subscribe(aio, NEU_REQ_SUBSCRIBE_GROUP, req);
            if (ret != 0) {
                NEU_JSON_RESPONSE_ERROR(
                    ret, { neu_http_response(aio, ret, result_error); });
            }
        })
}

void handle_grp_update_subscribe(nng_aio *aio)
{
    NEU_PROCESS_HTTP_REQUEST_VALIDATE_JWT(
        aio, neu_json_subscribe_req_t, neu_json_decode_subscribe_req, {
            int ret = send_subscribe(aio, NEU_REQ_UPDATE_SUBSCRIBE_GROUP, req);
            if (ret != 0) {
                NEU_JSON_RESPONSE_ERROR(
                    ret, { neu_http_response(aio, ret, result_error); });
            }
        })
}

void handle_grp_unsubscribe(nng_aio *aio)
{
    neu_plugin_t *plugin = neu_rest_get_plugin();
    NEU_PROCESS_HTTP_REQUEST_VALIDATE_JWT(
        aio, neu_json_unsubscribe_req_t, neu_json_decode_unsubscribe_req, {
            int                   ret    = 0;
            neu_req_unsubscribe_t cmd    = { 0 };
            neu_reqresp_head_t    header = { 0 };
            int                   err_type;
            header.ctx             = aio;
            header.type            = NEU_REQ_UNSUBSCRIBE_GROUP;
            header.otel_trace_type = NEU_OTEL_TRACE_TYPE_REST_COMM;

            if (strlen(req->app) >= NEU_NODE_NAME_LEN) {
                err_type = NEU_ERR_NODE_NAME_TOO_LONG;
                goto error;
            }

            if (strlen(req->driver) >= NEU_NODE_NAME_LEN) {
                err_type = NEU_ERR_NODE_NAME_TOO_LONG;
                goto error;
            }

            if (strlen(req->group) >= NEU_GROUP_NAME_LEN) {
                err_type = NEU_ERR_GROUP_NAME_TOO_LONG;
                goto error;
            }

            strcpy(cmd.app, req->app);
            strcpy(cmd.driver, req->driver);
            strcpy(cmd.group, req->group);
            ret = neu_plugin_op(plugin, header, &cmd);
            if (ret != 0) {
                NEU_JSON_RESPONSE_ERROR(NEU_ERR_IS_BUSY, {
                    neu_http_response(aio, NEU_ERR_IS_BUSY, result_error);
                });
            }
            goto success;

        error:
            NEU_JSON_RESPONSE_ERROR(
                err_type, { neu_http_response(aio, err_type, result_error); });
        success:;
        })
}

static inline int send_subscribe_groups(nng_aio *                        aio,
                                        neu_json_subscribe_groups_req_t *req)
{
    neu_plugin_t *plugin = neu_rest_get_plugin();

    neu_reqresp_head_t header = {
        .ctx             = aio,
        .type            = NEU_REQ_SUBSCRIBE_GROUPS,
        .otel_trace_type = NEU_OTEL_TRACE_TYPE_REST_COMM,
    };

    if (strlen(req->app) >= NEU_NODE_NAME_LEN) {
        return NEU_ERR_NODE_NAME_TOO_LONG;
    }

    for (int i = 0; i < req->n_group; i++) {
        if (strlen(req->groups[i].driver) >= NEU_NODE_NAME_LEN) {
            return NEU_ERR_NODE_NAME_TOO_LONG;
        }

        if (strlen(req->groups[i].group) >= NEU_GROUP_NAME_LEN) {
            return NEU_ERR_GROUP_NAME_TOO_LONG;
        }
    }

    neu_req_subscribe_groups_t cmd = {
        .app     = req->app,
        .n_group = req->n_group,
        .groups  = (neu_req_subscribe_group_info_t *) req->groups,
    };

    if (0 != neu_plugin_op(plugin, header, &cmd)) {
        return NEU_ERR_IS_BUSY;
    }

    // ownership moved
    req->app     = NULL;
    req->n_group = 0;
    req->groups  = NULL;

    return 0;
}

void handle_grp_subscribes(nng_aio *aio)
{
    NEU_PROCESS_HTTP_REQUEST_VALIDATE_JWT(
        aio, neu_json_subscribe_groups_req_t,
        neu_json_decode_subscribe_groups_req, {
            int ret = send_subscribe_groups(aio, req);
            if (0 != ret) {
                NEU_JSON_RESPONSE_ERROR(
                    ret, { neu_http_response(aio, ret, result_error); });
            }
        })
}

void handle_grp_get_subscribe(nng_aio *aio)
{
    int                           ret    = 0;
    neu_plugin_t *                plugin = neu_rest_get_plugin();
    neu_req_get_subscribe_group_t cmd    = { 0 };
    neu_reqresp_head_t            header = {
        .ctx             = aio,
        .type            = NEU_REQ_GET_SUBSCRIBE_GROUP,
        .otel_trace_type = NEU_OTEL_TRACE_TYPE_REST_COMM,
    };

    NEU_VALIDATE_JWT(aio);

    // required parameter
    ret = neu_http_get_param_str(aio, "app", cmd.app, sizeof(cmd.app));
    if (ret <= 0 || (size_t) ret == sizeof(cmd.driver)) {
        NEU_JSON_RESPONSE_ERROR(NEU_ERR_PARAM_IS_WRONG, {
            neu_http_response(aio, error_code.error, result_error);
        })
        return;
    }

    // optional parameter
    ret = neu_http_get_param_str(aio, "driver", cmd.driver, sizeof(cmd.driver));
    if (-1 == ret || (size_t) ret == sizeof(cmd.driver)) {
        NEU_JSON_RESPONSE_ERROR(NEU_ERR_PARAM_IS_WRONG, {
            neu_http_response(aio, error_code.error, result_error);
        })
        return;
    }

    // optional parameter
    ret = neu_http_get_param_str(aio, "group", cmd.group, sizeof(cmd.group));
    if (-1 == ret || (size_t) ret == sizeof(cmd.driver)) {
        NEU_JSON_RESPONSE_ERROR(NEU_ERR_PARAM_IS_WRONG, {
            neu_http_response(aio, error_code.error, result_error);
        })
        return;
    }

    ret = neu_plugin_op(plugin, header, &cmd);
    if (ret != 0) {
        NEU_JSON_RESPONSE_ERROR(NEU_ERR_IS_BUSY, {
            neu_http_response(aio, NEU_ERR_IS_BUSY, result_error);
        });
    }
}

void handle_grp_get_subscribe_resp(nng_aio *                       aio,
                                   neu_resp_get_subscribe_group_t *groups)
{
    char *                        result          = NULL;
    neu_json_get_subscribe_resp_t sub_grp_configs = { 0 };

    sub_grp_configs.n_group = utarray_len(groups->groups);
    sub_grp_configs.groups  = calloc(
        sub_grp_configs.n_group, sizeof(neu_json_get_subscribe_resp_group_t));

    utarray_foreach(groups->groups, neu_resp_subscribe_info_t *, group)
    {
        int index = utarray_eltidx(groups->groups, group);
        sub_grp_configs.groups[index].driver      = group->driver;
        sub_grp_configs.groups[index].group       = group->group;
        sub_grp_configs.groups[index].params      = group->params;
        sub_grp_configs.groups[index].static_tags = group->static_tags;
    }

    neu_json_encode_by_fn(&sub_grp_configs, neu_json_encode_get_subscribe_resp,
                          &result);

    neu_http_ok(aio, result);
    free(result);
    free(sub_grp_configs.groups);
    utarray_free(groups->groups);
}