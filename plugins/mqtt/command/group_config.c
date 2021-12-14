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

#include "group_config.h"
#include "neu_subscribe.h"
#include "json/neu_json_error.h"

char *command_group_config_get(neu_plugin_t *plugin, neu_json_mqtt_t *mqtt,
                               neu_json_get_group_config_req_t *req)
{
    log_debug("Get group config uuid:%s, node id:%d", mqtt->uuid, req->node_id);
    neu_json_get_group_config_resp_t res   = { 0 };
    int                              index = 0;
    vector_t configs = neu_system_get_group_configs(plugin, req->node_id);

    res.n_group_config = configs.size;
    res.group_configs =
        calloc(res.n_group_config,
               sizeof(neu_json_get_group_config_resp_group_config_t));

    VECTOR_FOR_EACH(&configs, iter)
    {
        neu_taggrp_config_t *config =
            *(neu_taggrp_config_t **) iterator_get(&iter);

        res.group_configs[index].name =
            (char *) neu_taggrp_cfg_get_name(config);
        res.group_configs[index].interval = neu_taggrp_cfg_get_interval(config);
        res.group_configs[index].tag_count =
            neu_taggrp_cfg_get_datatag_ids(config)->size;
        res.group_configs[index].pipe_count =
            neu_taggrp_cfg_get_subpipes(config)->size;

        index += 1;
    }

    char *result = NULL;
    neu_json_encode_with_mqtt(&res, neu_json_encode_get_group_config_resp, mqtt,
                              neu_json_encode_mqtt_resp, &result);

    vector_uninit(&configs);
    free(res.group_configs);
    return result;
}

char *command_group_config_add(neu_plugin_t *plugin, neu_json_mqtt_t *mqtt,
                               neu_json_add_group_config_req_t *req)
{
    log_debug("Add group config uuid:%s, config:%s, node id:%d, "
              "read interval:%d",
              mqtt->uuid, req->name, req->node_id, req->interval);
    neu_taggrp_config_t *config = neu_taggrp_cfg_new(req->name);
    neu_taggrp_cfg_set_interval(config, req->interval);
    neu_json_error_resp_t error = { 0 };
    error.error = neu_system_add_group_config(plugin, req->node_id, config);

    if (NEU_ERR_SUCCESS != error.error) {
        neu_taggrp_cfg_free(config);
    }

    char *result = NULL;
    neu_json_encode_with_mqtt(&error, neu_json_encode_error_resp, mqtt,
                              neu_json_encode_mqtt_resp, &result);
    return result;
}

char *command_group_config_update(neu_plugin_t *plugin, neu_json_mqtt_t *mqtt,
                                  neu_json_update_group_config_req_t *req)
{
    log_debug("Update group config uuid:%s, config:%s, src node id:%d,"
              "read interval:%d",
              mqtt->uuid, req->name, req->node_id, req->interval);
    neu_json_error_resp_t error  = { 0 };
    char *                result = NULL;
    neu_taggrp_config_t * config =
        neu_system_find_group_config(plugin, req->node_id, req->name);
    if (NULL == config) {
        error.error = NEU_ERR_GRP_CONFIG_NOT_EXIST;
        neu_json_encode_with_mqtt(&error, neu_json_encode_error_resp, mqtt,
                                  neu_json_encode_mqtt_resp, &result);
        return result;
    }

    neu_taggrp_cfg_set_interval(config, req->interval);
    error.error = neu_system_update_group_config(plugin, req->node_id, config);
    neu_json_encode_with_mqtt(&error, neu_json_encode_error_resp, mqtt,
                              neu_json_encode_mqtt_resp, &result);
    return result;
}

char *command_group_config_delete(neu_plugin_t *plugin, neu_json_mqtt_t *mqtt,
                                  neu_json_del_group_config_req_t *req)
{
    log_debug("Delete group config uuid:%s, config:%s, src node id:%d",
              mqtt->uuid, req->name, req->node_id);
    neu_json_error_resp_t error  = { 0 };
    char *                result = NULL;
    neu_taggrp_config_t * config =
        neu_system_find_group_config(plugin, req->node_id, req->name);
    neu_datatag_table_t *table =
        neu_system_get_datatags_table(plugin, req->node_id);

    if (NULL == config || NULL == table) {
        error.error = NEU_ERR_GRP_CONFIG_NOT_EXIST;
        neu_json_encode_with_mqtt(&error, neu_json_encode_error_resp, mqtt,
                                  neu_json_encode_mqtt_resp, &result);
        return result;
    }

    if (neu_taggrp_cfg_get_subpipes(config)->size != 0) {
        error.error = NEU_ERR_GRP_CONFIG_IN_USE;
        neu_json_encode_with_mqtt(&error, neu_json_encode_error_resp, mqtt,
                                  neu_json_encode_mqtt_resp, &result);
        return result;
    }

    vector_t *ids = neu_taggrp_cfg_get_datatag_ids(config);

    VECTOR_FOR_EACH(ids, iter)
    {
        neu_datatag_id_t *id = (neu_datatag_id_t *) iterator_get(&iter);
        neu_datatag_tbl_remove(table, *id);
    }

    error.error = neu_system_del_group_config(plugin, req->node_id, req->name);
    neu_json_encode_with_mqtt(&error, neu_json_encode_error_resp, mqtt,
                              neu_json_encode_mqtt_resp, &result);
    return result;
}

char *command_group_config_subscribe(neu_plugin_t *            plugin,
                                     neu_json_mqtt_t *         mqtt,
                                     neu_json_subscribe_req_t *req)
{
    log_info("Subscribe uuid:%s, config name:%s", mqtt->uuid, req->name);
    neu_json_error_resp_t error = { 0 };
    neu_taggrp_config_t * config =
        neu_system_find_group_config(plugin, req->src_node_id, req->name);
    if (NULL != config) {
        neu_plugin_send_subscribe_cmd(plugin, req->src_node_id,
                                      req->dst_node_id, config);
        error.error = NEU_ERR_SUCCESS;
    } else {
        error.error = NEU_ERR_GRP_CONFIG_NOT_EXIST;
    }

    char *result = NULL;
    neu_json_encode_with_mqtt(&error, neu_json_encode_error_resp, mqtt,
                              neu_json_encode_mqtt_resp, &result);
    return result;
}

char *command_group_config_unsubscribe(neu_plugin_t *              plugin,
                                       neu_json_mqtt_t *           mqtt,
                                       neu_json_unsubscribe_req_t *req)
{
    log_info("Unsubscribe uuid:%s, config name:%s", mqtt->uuid, req->name);
    neu_json_error_resp_t error = { 0 };
    neu_taggrp_config_t * config =
        neu_system_find_group_config(plugin, req->src_node_id, req->name);
    if (NULL != config) {
        neu_plugin_send_unsubscribe_cmd(plugin, req->src_node_id,
                                        req->dst_node_id, config);
        error.error = NEU_ERR_SUCCESS;
    } else {
        error.error = NEU_ERR_GRP_CONFIG_NOT_EXIST;
    }

    char *result = NULL;
    neu_json_encode_with_mqtt(&error, neu_json_encode_error_resp, mqtt,
                              neu_json_encode_mqtt_resp, &result);
    return result;
}

char *command_get_group_config_subscribes(neu_plugin_t *   plugin,
                                          neu_json_mqtt_t *mqtt,
                                          neu_json_get_group_config_req_t *req)
{
    neu_json_get_subscribe_resp_t sub_grp_configs = { 0 };
    int                           index           = 0;
    vector_t *configs = neu_system_get_sub_group_configs(plugin, req->node_id);

    sub_grp_configs.n_group = configs->size;
    sub_grp_configs.groups =
        calloc(configs->size, sizeof(neu_json_get_subscribe_resp_group_t));

    VECTOR_FOR_EACH(configs, iter)
    {
        neu_sub_grp_config_t *sgc =
            (neu_sub_grp_config_t *) iterator_get(&iter);

        sub_grp_configs.groups[index].node_id = sgc->node_id;
        sub_grp_configs.groups[index].group_config_name =
            sgc->group_config_name;

        index += 1;
    }

    vector_free(configs);

    char *result = NULL;
    neu_json_encode_with_mqtt(&sub_grp_configs,
                              neu_json_encode_get_subscribe_resp, mqtt,
                              neu_json_encode_mqtt_resp, &result);

    free(sub_grp_configs.groups);

    return result;
}
