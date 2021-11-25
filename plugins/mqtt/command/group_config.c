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

static void all_group_configs_get(neu_plugin_t *plugin, const uint32_t node_id,
                                  neu_json_get_group_config_resp_t *res)
{
    vector_t nodes = neu_system_get_nodes(plugin, NEU_NODE_TYPE_DRIVER);
    int      rc    = common_node_id_exist(&nodes, node_id);
    vector_uninit(&nodes);
    if (0 != rc) {
        return;
    }

    vector_t configs = neu_system_get_group_configs(plugin, node_id);
    if (0 == configs.size) {
        GROUP_CONFIGS_UNINIT(configs);
        return;
    }

    res->n_group_config = configs.size;
    res->group_configs =
        (neu_json_get_group_config_resp_group_config_t *) malloc(
            sizeof(neu_json_get_group_config_resp_group_config_t) *
            configs.size);
    memset(res->group_configs, 0,
           sizeof(neu_json_get_group_config_resp_group_config_t) *
               configs.size);

    int                  count = 0;
    neu_taggrp_config_t *config;
    VECTOR_FOR_EACH(&configs, iter)
    {
        config = *(neu_taggrp_config_t **) iterator_get(&iter);
        if (NULL == config) {
            continue;
        }

        res->group_configs[count].name =
            strdup((char *) neu_taggrp_cfg_get_name(config));
        res->group_configs[count].interval =
            neu_taggrp_cfg_get_interval(config);

        vector_t *pipes = neu_taggrp_cfg_get_subpipes(config);
        if (NULL != pipes) {
            res->group_configs[count].pipe_count = pipes->size;
        }

        vector_t *ids = neu_taggrp_cfg_get_datatag_ids(config);
        if (NULL != ids) {
            res->group_configs[count].tag_count = ids->size;
        }

        count++;
    }
    GROUP_CONFIGS_UNINIT(configs);
}

static void all_group_config_res_free(neu_json_get_group_config_resp_t *res)
{
    if (NULL == res) {
        return;
    }

    if (NULL == res->group_configs) {
        return;
    }

    int i;
    for (i = 0; i < res->n_group_config; i++) {
        if (NULL == res->group_configs[i].name) {
            free(res->group_configs[i].name);
        }
    }
    free(res->group_configs);
}

char *command_group_config_get(neu_plugin_t *plugin, neu_json_mqtt_t *mqtt,
                               neu_json_get_group_config_req_t *req)
{
    log_debug("Get group config uuid:%s, node id:%d", mqtt->uuid, req->node_id);

    char *                           json_str = NULL;
    neu_json_get_group_config_resp_t res      = { 0 };
    all_group_configs_get(plugin, req->node_id, &res);
    neu_json_encode_with_mqtt(&res, neu_json_encode_get_group_config_resp, mqtt,
                              neu_json_encode_mqtt_resp, &json_str);
    all_group_config_res_free(&res);
    return json_str;
}

static int group_config_add(neu_plugin_t *plugin, const char *config_name,
                            const uint32_t node_id, const int read_interval)
{
    vector_t nodes = neu_system_get_nodes(plugin, NEU_NODE_TYPE_DRIVER);
    int      rc    = common_node_id_exist(&nodes, node_id);
    vector_uninit(&nodes);
    if (0 != rc) {
        return -1;
    }

    vector_t configs = neu_system_get_group_configs(plugin, node_id);
    rc               = common_config_exist(&configs, config_name);
    GROUP_CONFIGS_UNINIT(configs);

    if (0 == rc) {
        return -2;
    }

    neu_taggrp_config_t *grp_config = neu_taggrp_cfg_new((char *) config_name);
    if (NULL == grp_config) {
        return -3;
    }
    neu_taggrp_cfg_set_interval(grp_config, read_interval);
    intptr_t error = neu_system_add_group_config(plugin, node_id, grp_config);
    if (0 != error) {
        return -4;
    }
    return 0;
}

char *command_group_config_add(neu_plugin_t *plugin, neu_json_mqtt_t *mqtt,
                               neu_json_add_group_config_req_t *req)
{
    log_debug("uuid:%s, config:%s, node id:%d, "
              "read interval:%d",
              mqtt->uuid, req->name, req->node_id, req->interval);

    int rc = group_config_add(plugin, req->name, req->node_id, req->interval);

    char *                json_str = NULL;
    neu_json_error_resp_t error    = { .error = rc };

    neu_json_encode_with_mqtt(&error, neu_json_encode_error_resp, mqtt,
                              neu_json_encode_mqtt_resp, &json_str);
    return json_str;
}

static int group_config_update(neu_plugin_t *plugin, const char *config_name,
                               const uint32_t node_id, const int read_interval)
{
    if (0 == node_id) {
        return -1;
    }

    vector_t nodes = neu_system_get_nodes(plugin, NEU_NODE_TYPE_DRIVER);
    int      rc    = common_node_id_exist(&nodes, node_id);
    vector_uninit(&nodes);
    if (0 != rc) {
        return -2;
    }

    vector_t configs = neu_system_get_group_configs(plugin, node_id);
    rc               = common_config_exist(&configs, config_name);
    GROUP_CONFIGS_UNINIT(configs);

    if (0 != rc) {
        return -4;
    }

    neu_taggrp_config_t *grp_config = neu_taggrp_cfg_new((char *) config_name);
    if (NULL == grp_config) {
        return -5;
    }
    neu_taggrp_cfg_set_interval(grp_config, read_interval);

    intptr_t error =
        neu_system_update_group_config(plugin, node_id, grp_config);
    if (0 != error) {
        return -6;
    }
    return 0;
}

char *command_group_config_update(neu_plugin_t *plugin, neu_json_mqtt_t *mqtt,
                                  neu_json_update_group_config_req_t *req)
{
    log_debug("uuid:%s, config:%s, src node id:%d,"
              "read interval:%d",
              mqtt->uuid, req->name, req->node_id, req->interval);

    int rc =
        group_config_update(plugin, req->name, req->node_id, req->interval);

    char *                json_str = NULL;
    neu_json_error_resp_t error    = { .error = rc };

    rc = neu_json_encode_with_mqtt(&error, neu_json_encode_error_resp, mqtt,
                                   neu_json_encode_mqtt_resp, &json_str);
    return json_str;
}

static int group_config_delete(neu_plugin_t *plugin, const char *config_name,
                               const uint32_t node_id)
{
    if (0 == node_id) {
        return -1;
    }

    vector_t nodes = neu_system_get_nodes(plugin, NEU_NODE_TYPE_DRIVER);
    int      rc    = common_node_id_exist(&nodes, node_id);
    vector_uninit(&nodes);
    if (0 != rc) {
        return -2;
    }

    vector_t configs = neu_system_get_group_configs(plugin, node_id);
    rc               = common_config_exist(&configs, config_name);
    GROUP_CONFIGS_UNINIT(configs);

    if (0 != rc) {
        return -4;
    }

    intptr_t error =
        neu_system_del_group_config(plugin, node_id, (char *) config_name);
    if (0 != error) {
        return -5;
    }
    return 0;
}

char *command_group_config_delete(neu_plugin_t *plugin, neu_json_mqtt_t *mqtt,
                                  neu_json_del_group_config_req_t *req)
{
    log_debug("uuid:%s, config:%s, src node id:%d", mqtt->uuid, req->name,
              req->node_id);
    int   rc       = group_config_delete(plugin, req->name, req->node_id);
    char *json_str = NULL;
    neu_json_error_resp_t error = { .error = rc };
    rc = neu_json_encode_with_mqtt(&error, neu_json_encode_error_resp, mqtt,
                                   neu_json_encode_mqtt_resp, &json_str);
    return json_str;
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
    free(sub_grp_configs.groups);

    char *result = NULL;
    neu_json_encode_with_mqtt(&sub_grp_configs,
                              neu_json_encode_get_subscribe_resp, mqtt,
                              neu_json_encode_mqtt_resp, &result);
    return result;
}
