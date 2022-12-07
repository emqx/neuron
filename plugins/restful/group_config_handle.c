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

    REST_PROCESS_HTTP_REQUEST_VALIDATE_JWT(
        aio, neu_json_add_group_config_req_t,
        neu_json_decode_add_group_config_req, {
            if (strlen(req->group) >= NEU_GROUP_NAME_LEN) {
                NEU_JSON_RESPONSE_ERROR(NEU_ERR_GROUP_NAME_TOO_LONG, {
                    http_response(aio, NEU_ERR_GROUP_NAME_TOO_LONG,
                                  result_error);
                });
            } else {
                int                 ret    = 0;
                neu_reqresp_head_t  header = { 0 };
                neu_req_add_group_t cmd    = { 0 };

                header.ctx  = aio;
                header.type = NEU_REQ_ADD_GROUP;
                strcpy(cmd.driver, req->node);
                strcpy(cmd.group, req->group);
                cmd.interval = req->interval;
                ret          = neu_plugin_op(plugin, header, &cmd);
                if (ret != 0) {
                    NEU_JSON_RESPONSE_ERROR(NEU_ERR_IS_BUSY, {
                        http_response(aio, NEU_ERR_IS_BUSY, result_error);
                    });
                }
            }
        })
}

void handle_update_group(nng_aio *aio)
{
    neu_plugin_t *plugin = neu_rest_get_plugin();

    REST_PROCESS_HTTP_REQUEST_VALIDATE_JWT(
        aio, neu_json_update_group_req_t, neu_json_decode_add_group_config_req,
        {
            int                 ret    = 0;
            neu_reqresp_head_t  header = { 0 };
            neu_req_add_group_t cmd    = { 0 };

            header.ctx  = aio;
            header.type = NEU_REQ_UPDATE_GROUP;
            strcpy(cmd.driver, req->node);
            strcpy(cmd.group, req->group);
            cmd.interval = req->interval;
            ret          = neu_plugin_op(plugin, header, &cmd);
            if (ret != 0) {
                NEU_JSON_RESPONSE_ERROR(NEU_ERR_IS_BUSY, {
                    http_response(aio, NEU_ERR_IS_BUSY, result_error);
                });
            }
        })
}

void handle_del_group_config(nng_aio *aio)
{
    neu_plugin_t *plugin = neu_rest_get_plugin();

    REST_PROCESS_HTTP_REQUEST_VALIDATE_JWT(
        aio, neu_json_del_group_config_req_t,
        neu_json_decode_del_group_config_req, {
            int                 ret    = 0;
            neu_reqresp_head_t  header = { 0 };
            neu_req_del_group_t cmd    = { 0 };

            header.ctx  = aio;
            header.type = NEU_REQ_DEL_GROUP;
            strcpy(cmd.driver, req->node);
            strcpy(cmd.group, req->group);
            ret = neu_plugin_op(plugin, header, &cmd);
            if (ret != 0) {
                NEU_JSON_RESPONSE_ERROR(NEU_ERR_IS_BUSY, {
                    http_response(aio, NEU_ERR_IS_BUSY, result_error);
                });
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
        .ctx  = aio,
        .type = NEU_REQ_GET_GROUP,
    };

    VALIDATE_JWT(aio);

    http_get_param_str(aio, "node", node_name, sizeof(node_name));
    strcpy(cmd.driver, node_name);
    if (strlen(cmd.driver) == 0) {
        header.type = NEU_REQ_GET_DRIVER_GROUP;
    }

    ret = neu_plugin_op(plugin, header, &cmd);
    if (ret != 0) {
        NEU_JSON_RESPONSE_ERROR(NEU_ERR_IS_BUSY, {
            http_response(aio, NEU_ERR_IS_BUSY, result_error);
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

    http_ok(aio, result);

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

    http_ok(aio, result);

    free(result);
    free(gconfig_res.groups);
    utarray_free(groups->groups);
}

void handle_grp_subscribe(nng_aio *aio)
{
    neu_plugin_t *plugin = neu_rest_get_plugin();

    REST_PROCESS_HTTP_REQUEST_VALIDATE_JWT(
        aio, neu_json_subscribe_req_t, neu_json_decode_subscribe_req, {
            int                 ret    = 0;
            neu_req_subscribe_t cmd    = { 0 };
            neu_reqresp_head_t  header = { 0 };
            header.ctx                 = aio;
            header.type                = NEU_REQ_SUBSCRIBE_GROUP;
            strcpy(cmd.app, req->app);
            strcpy(cmd.driver, req->driver);
            strcpy(cmd.group, req->group);
            ret = neu_plugin_op(plugin, header, &cmd);
            if (ret != 0) {
                NEU_JSON_RESPONSE_ERROR(NEU_ERR_IS_BUSY, {
                    http_response(aio, NEU_ERR_IS_BUSY, result_error);
                });
            }
        })
}

void handle_grp_unsubscribe(nng_aio *aio)
{
    neu_plugin_t *plugin = neu_rest_get_plugin();

    REST_PROCESS_HTTP_REQUEST_VALIDATE_JWT(
        aio, neu_json_unsubscribe_req_t, neu_json_decode_unsubscribe_req, {
            int                   ret    = 0;
            neu_req_unsubscribe_t cmd    = { 0 };
            neu_reqresp_head_t    header = { 0 };
            header.ctx                   = aio;
            header.type                  = NEU_REQ_UNSUBSCRIBE_GROUP;
            strcpy(cmd.app, req->app);
            strcpy(cmd.driver, req->driver);
            strcpy(cmd.group, req->group);
            ret = neu_plugin_op(plugin, header, &cmd);
            if (ret != 0) {
                NEU_JSON_RESPONSE_ERROR(NEU_ERR_IS_BUSY, {
                    http_response(aio, NEU_ERR_IS_BUSY, result_error);
                });
            }
        })
}

void handle_grp_get_subscribe(nng_aio *aio)
{
    int                           ret    = 0;
    neu_plugin_t *                plugin = neu_rest_get_plugin();
    char                          node_name[NEU_NODE_NAME_LEN] = { 0 };
    neu_req_get_subscribe_group_t cmd                          = { 0 };
    neu_reqresp_head_t            header                       = {
        .ctx  = aio,
        .type = NEU_REQ_GET_SUBSCRIBE_GROUP,
    };

    VALIDATE_JWT(aio);

    if (http_get_param_str(aio, "app", node_name, sizeof(node_name)) <= 0) {
        NEU_JSON_RESPONSE_ERROR(NEU_ERR_PARAM_IS_WRONG, {
            http_response(aio, error_code.error, result_error);
        })
        return;
    }

    strcpy(cmd.app, node_name);
    ret = neu_plugin_op(plugin, header, &cmd);
    if (ret != 0) {
        NEU_JSON_RESPONSE_ERROR(NEU_ERR_IS_BUSY, {
            http_response(aio, NEU_ERR_IS_BUSY, result_error);
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
        sub_grp_configs.groups[index].driver = group->driver;
        sub_grp_configs.groups[index].group  = group->group;
    }

    neu_json_encode_by_fn(&sub_grp_configs, neu_json_encode_get_subscribe_resp,
                          &result);

    http_ok(aio, result);
    free(result);
    free(sub_grp_configs.groups);
    utarray_free(groups->groups);
}