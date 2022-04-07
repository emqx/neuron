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

#include "log.h"
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
        neu_json_decode_add_group_config_req, {
            neu_taggrp_config_t *gconfig = neu_taggrp_cfg_new(req->name);
            neu_taggrp_cfg_set_interval(gconfig, req->interval);

            NEU_JSON_RESPONSE_ERROR(
                neu_system_add_group_config(plugin, req->node_id, gconfig), {
                    http_response(aio, error_code.error, result_error);
                    if (error_code.error != NEU_ERR_SUCCESS) {
                        neu_taggrp_cfg_free(gconfig);
                    }
                })
        })
}

void handle_del_group_config(nng_aio *aio)
{
    neu_plugin_t *plugin = neu_rest_get_plugin();

    REST_PROCESS_HTTP_REQUEST_VALIDATE_JWT(
        aio, neu_json_del_group_config_req_t,
        neu_json_decode_del_group_config_req, {
            neu_taggrp_config_t *config =
                neu_system_find_group_config(plugin, req->node_id, req->name);
            neu_datatag_table_t *table =
                neu_system_get_datatags_table(plugin, req->node_id);

            if (config == NULL || table == NULL) {
                NEU_JSON_RESPONSE_ERROR(NEU_ERR_GRP_CONFIG_NOT_EXIST, {
                    http_response(aio, error_code.error, result_error);
                });
                return;
            }

            if (neu_taggrp_cfg_get_subpipes(config)->size != 0) {
                NEU_JSON_RESPONSE_ERROR(NEU_ERR_GRP_CONFIG_IN_USE, {
                    http_response(aio, error_code.error, result_error);
                });
                return;
            }

            vector_t *ids = neu_taggrp_cfg_get_datatag_ids(config);

            VECTOR_FOR_EACH(ids, iter)
            {
                neu_datatag_id_t *id = (neu_datatag_id_t *) iterator_get(&iter);
                neu_datatag_tbl_remove(table, *id);
            }

            NEU_JSON_RESPONSE_ERROR(
                neu_system_del_group_config(plugin, req->node_id, req->name),
                { http_response(aio, error_code.error, result_error); })
        })
}

void handle_update_group_config(nng_aio *aio)
{
    neu_plugin_t *plugin = neu_rest_get_plugin();

    REST_PROCESS_HTTP_REQUEST_VALIDATE_JWT(
        aio, neu_json_update_group_config_req_t,
        neu_json_decode_update_group_config_req, {
            neu_taggrp_config_t *config =
                neu_system_find_group_config(plugin, req->node_id, req->name);

            if (config == NULL) {
                NEU_JSON_RESPONSE_ERROR(NEU_ERR_GRP_CONFIG_NOT_EXIST, {
                    http_response(aio, error_code.error, result_error);
                })
                return;
            }

            neu_taggrp_cfg_set_interval(config, req->interval);

            NEU_JSON_RESPONSE_ERROR(
                neu_system_update_group_config(plugin, req->node_id, config),
                { http_response(aio, error_code.error, result_error); })
        })
}

void handle_get_group_config(nng_aio *aio)
{
    neu_plugin_t *plugin  = neu_rest_get_plugin();
    char *        result  = NULL;
    neu_node_id_t node_id = { 0 };

    VALIDATE_JWT(aio);

    if (http_get_param_node_id(aio, "node_id", &node_id) != 0) {
        NEU_JSON_RESPONSE_ERROR(NEU_ERR_PARAM_IS_WRONG, {
            http_response(aio, error_code.error, result_error);
        })
        return;
    }

    neu_json_get_group_config_resp_t gconfig_res = { 0 };
    int                              index       = 0;
    vector_t gconfigs = neu_system_get_group_configs(plugin, node_id);

    gconfig_res.n_group_config = gconfigs.size;
    gconfig_res.group_configs =
        calloc(gconfig_res.n_group_config,
               sizeof(neu_json_get_group_config_resp_group_config_t));

    VECTOR_FOR_EACH(&gconfigs, iter)
    {
        neu_taggrp_config_t *config =
            *(neu_taggrp_config_t **) iterator_get(&iter);

        gconfig_res.group_configs[index].name =
            (char *) neu_taggrp_cfg_get_name(config);
        gconfig_res.group_configs[index].interval =
            neu_taggrp_cfg_get_interval(config);
        gconfig_res.group_configs[index].tag_count =
            neu_taggrp_cfg_get_datatag_ids(config)->size;
        gconfig_res.group_configs[index].pipe_count =
            neu_taggrp_cfg_get_subpipes(config)->size;

        index += 1;
    }

    neu_json_encode_by_fn(&gconfig_res, neu_json_encode_get_group_config_resp,
                          &result);

    http_ok(aio, result);

    vector_uninit(&gconfigs);
    free(result);
    free(gconfig_res.group_configs);
}

void handle_grp_subscribe(nng_aio *aio)
{
    neu_plugin_t *plugin = neu_rest_get_plugin();

    REST_PROCESS_HTTP_REQUEST_VALIDATE_JWT(
        aio, neu_json_subscribe_req_t, neu_json_decode_subscribe_req, {
            neu_taggrp_config_t *config = neu_system_find_group_config(
                plugin, req->src_node_id, req->name);

            if (config == NULL) {
                NEU_JSON_RESPONSE_ERROR(NEU_ERR_GRP_CONFIG_NOT_EXIST, {
                    http_response(aio, error_code.error, result_error);
                })
            } else {
                neu_plugin_send_subscribe_cmd(plugin, req->src_node_id,
                                              req->dst_node_id, config);
                NEU_JSON_RESPONSE_ERROR(NEU_ERR_SUCCESS, {
                    http_response(aio, error_code.error, result_error);
                })
            }
        })
}

void handle_grp_unsubscribe(nng_aio *aio)
{
    neu_plugin_t *plugin = neu_rest_get_plugin();

    REST_PROCESS_HTTP_REQUEST_VALIDATE_JWT(
        aio, neu_json_unsubscribe_req_t, neu_json_decode_unsubscribe_req, {
            neu_taggrp_config_t *config = neu_system_find_group_config(
                plugin, req->src_node_id, req->name);

            if (config == NULL) {
                NEU_JSON_RESPONSE_ERROR(NEU_ERR_GRP_CONFIG_NOT_EXIST, {
                    http_response(aio, error_code.error, result_error);
                })
            } else {
                neu_plugin_send_unsubscribe_cmd(plugin, req->src_node_id,
                                                req->dst_node_id, config);
                NEU_JSON_RESPONSE_ERROR(NEU_ERR_SUCCESS, {
                    http_response(aio, error_code.error, result_error);
                })
            }
        })
}

void handle_grp_get_subscribe(nng_aio *aio)
{
    neu_plugin_t *                plugin          = neu_rest_get_plugin();
    char *                        result          = NULL;
    neu_node_id_t                 node_id         = 0;
    neu_json_get_subscribe_resp_t sub_grp_configs = { 0 };
    int                           index           = 0;

    VALIDATE_JWT(aio);

    if (http_get_param_node_id(aio, "node_id", &node_id) != 0) {
        NEU_JSON_RESPONSE_ERROR(NEU_ERR_PARAM_IS_WRONG, {
            http_response(aio, error_code.error, result_error);
        })
        return;
    }

    vector_t *gconfigs = neu_system_get_sub_group_configs(plugin, node_id);

    sub_grp_configs.n_group = gconfigs->size;
    sub_grp_configs.groups =
        calloc(gconfigs->size, sizeof(neu_json_get_subscribe_resp_group_t));

    VECTOR_FOR_EACH(gconfigs, iter)
    {
        neu_sub_grp_config_t *sgc =
            (neu_sub_grp_config_t *) iterator_get(&iter);

        sub_grp_configs.groups[index].node_id = sgc->node_id;
        sub_grp_configs.groups[index].group_config_name =
            sgc->group_config_name;

        index += 1;
    }

    neu_json_encode_by_fn(&sub_grp_configs, neu_json_encode_get_subscribe_resp,
                          &result);

    http_ok(aio, result);
    vector_free(gconfigs);
    free(result);
    free(sub_grp_configs.groups);
}