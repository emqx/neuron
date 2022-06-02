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

#include <stdlib.h>

#include "vector.h"

#include "plugin.h"
#include "subscribe.h"
#include "json/neu_json_error.h"
#include "json/neu_json_fn.h"
#include "json/neu_json_group_config.h"

#include "handle.h"
#include "http.h"

#include "group_config_handle.h"

void handle_add_group_config(nng_aio *aio)
{
    neu_plugin_t *plugin = neu_rest_get_plugin();

    REST_PROCESS_HTTP_REQUEST_VALIDATE_JWT(
        aio, neu_json_add_group_config_req_t,
        neu_json_decode_add_group_config_req,
        { NEU_JSON_RESPONSE_ERROR(
            neu_system_add_group_config(plugin, req->node_name, req->name,
                                        req->interval),
            { http_response(aio, error_code.error, result_error); }) })
}

void handle_del_group_config(nng_aio *aio)
{
    neu_plugin_t *plugin = neu_rest_get_plugin();

    REST_PROCESS_HTTP_REQUEST_VALIDATE_JWT(
        aio, neu_json_del_group_config_req_t,
        neu_json_decode_del_group_config_req,
        { NEU_JSON_RESPONSE_ERROR(
            neu_system_del_group_config(plugin, req->node_name, req->name),
            { http_response(aio, error_code.error, result_error); }) })
}

void handle_update_group_config(nng_aio *aio)
{
    neu_plugin_t *plugin = neu_rest_get_plugin();

    REST_PROCESS_HTTP_REQUEST_VALIDATE_JWT(
        aio, neu_json_update_group_config_req_t,
        neu_json_decode_update_group_config_req,
        { NEU_JSON_RESPONSE_ERROR(
            neu_system_update_group_config(plugin, req->node_name, req->name,
                                           req->interval),
            { http_response(aio, error_code.error, result_error); }) })
}

void handle_get_group_config(nng_aio *aio)
{
    neu_plugin_t *plugin                       = neu_rest_get_plugin();
    char *        result                       = NULL;
    char          node_name[NEU_NODE_NAME_LEN] = { 0 };

    VALIDATE_JWT(aio);

    if (http_get_param_str(aio, "node_name", node_name, sizeof(node_name)) <=
        0) {
        NEU_JSON_RESPONSE_ERROR(NEU_ERR_PARAM_IS_WRONG, {
            http_response(aio, error_code.error, result_error);
        })
        return;
    }

    neu_json_get_group_config_resp_t gconfig_res = { 0 };
    int                              index       = 0;
    UT_array *                       groups      = NULL;
    int error = neu_system_get_group_configs(plugin, node_name, &groups);

    if (error != NEU_ERR_SUCCESS) {
        NEU_JSON_RESPONSE_ERROR(
            error, { http_response(aio, error_code.error, result_error); })
        return;
    }

    gconfig_res.n_group_config = utarray_len(groups);
    gconfig_res.group_configs =
        calloc(gconfig_res.n_group_config,
               sizeof(neu_json_get_group_config_resp_group_config_t));

    utarray_foreach(groups, neu_group_info_t *, group)
    {
        gconfig_res.group_configs[index].name      = group->name;
        gconfig_res.group_configs[index].interval  = group->interval;
        gconfig_res.group_configs[index].tag_count = group->tag_count;

        index += 1;
    }

    neu_json_encode_by_fn(&gconfig_res, neu_json_encode_get_group_config_resp,
                          &result);

    http_ok(aio, result);

    free(result);
    free(gconfig_res.group_configs);
    utarray_free(groups);
}

void handle_grp_subscribe(nng_aio *aio)
{
    neu_plugin_t *plugin = neu_rest_get_plugin();

    REST_PROCESS_HTTP_REQUEST_VALIDATE_JWT(
        aio, neu_json_subscribe_req_t, neu_json_decode_subscribe_req,
        { NEU_JSON_RESPONSE_ERROR(
            neu_plugin_send_subscribe_cmd(plugin, req->app_name,
                                          req->driver_name, req->name),
            { http_response(aio, error_code.error, result_error); }) })
}

void handle_grp_unsubscribe(nng_aio *aio)
{
    neu_plugin_t *plugin = neu_rest_get_plugin();

    REST_PROCESS_HTTP_REQUEST_VALIDATE_JWT(
        aio, neu_json_unsubscribe_req_t, neu_json_decode_unsubscribe_req,
        { NEU_JSON_RESPONSE_ERROR(
            neu_plugin_send_unsubscribe_cmd(plugin, req->app_name,
                                            req->driver_name, req->name),
            { http_response(aio, error_code.error, result_error); }) })
}

void handle_grp_get_subscribe(nng_aio *aio)
{
    neu_plugin_t *                plugin          = neu_rest_get_plugin();
    char *                        result          = NULL;
    neu_json_get_subscribe_resp_t sub_grp_configs = { 0 };
    int                           index           = 0;
    char                          node_name[NEU_NODE_NAME_LEN] = { 0 };

    VALIDATE_JWT(aio);

    if (http_get_param_str(aio, "node_name", node_name, sizeof(node_name)) <=
        0) {
        NEU_JSON_RESPONSE_ERROR(NEU_ERR_PARAM_IS_WRONG, {
            http_response(aio, error_code.error, result_error);
        })
        return;
    }

    UT_array *groups = neu_system_get_sub_group_configs(plugin, node_name);

    sub_grp_configs.n_group = utarray_len(groups);
    sub_grp_configs.groups  = calloc(
        sub_grp_configs.n_group, sizeof(neu_json_get_subscribe_resp_group_t));

    utarray_foreach(groups, neu_subscribe_info_t *, group)
    {
        sub_grp_configs.groups[index].node_name  = group->driver;
        sub_grp_configs.groups[index].group_name = group->group;

        index += 1;
    }

    neu_json_encode_by_fn(&sub_grp_configs, neu_json_encode_get_subscribe_resp,
                          &result);

    http_ok(aio, result);
    free(result);
    free(sub_grp_configs.groups);
    utarray_free(groups);
}