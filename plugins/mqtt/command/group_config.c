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
#include "json/neu_json_error.h"

static void get_all_group_configs(neu_plugin_t *plugin, const uint32_t node_id,
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

static void
free_parse_get_group_config_res(neu_json_get_group_config_resp_t *res)
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

char *command_get_group_configs(neu_plugin_t *plugin, neu_parse_mqtt_t *mqtt,
                                neu_json_get_group_config_req_t *req)
{
    log_debug("uuid:%s, node id:%d", mqtt->uuid, req->node_id);

    char *                           json_str = NULL;
    neu_json_get_group_config_resp_t res      = { 0 };
    get_all_group_configs(plugin, req->node_id, &res);
    neu_json_encode_with_mqtt(&res, neu_json_encode_get_group_config_resp, mqtt,
                              neu_parse_encode_mqtt_param, &json_str);
    free_parse_get_group_config_res(&res);

    return json_str;
}

static int add_group_config(neu_plugin_t *plugin, const char *config_name,
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

char *command_add_group_config(neu_plugin_t *plugin, neu_parse_mqtt_t *mqtt,
                               neu_json_add_group_config_req_t *req)
{
    log_debug("uuid:%s, config:%s, node id:%d, "
              "read interval:%d",
              mqtt->uuid, req->name, req->node_id, req->interval);

    int rc = add_group_config(plugin, req->name, req->node_id, req->interval);

    char *            json_str = NULL;
    neu_parse_error_t error    = { .error = rc };

    neu_json_encode_with_mqtt(&error, neu_parse_encode_error, mqtt,
                              neu_parse_encode_mqtt_param, &json_str);
    return json_str;
}

static int update_group_config(neu_plugin_t *plugin, const char *config_name,
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

char *command_update_group_config(neu_plugin_t *plugin, neu_parse_mqtt_t *mqtt,
                                  neu_json_update_group_config_req_t *req)
{
    log_debug("uuid:%s, config:%s, src node id:%d,"
              "read interval:%d",
              mqtt->uuid, req->name, req->node_id, req->interval);

    int rc =
        update_group_config(plugin, req->name, req->node_id, req->interval);

    char *            json_str = NULL;
    neu_parse_error_t error    = { .error = rc };

    rc = neu_json_encode_with_mqtt(&error, neu_parse_encode_error, mqtt,
                                   neu_parse_encode_mqtt_param, &json_str);
    return json_str;
}

static int delete_group_config(neu_plugin_t *plugin, const char *config_name,
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

char *command_delete_group_config(neu_plugin_t *plugin, neu_parse_mqtt_t *mqtt,
                                  neu_json_del_group_config_req_t *req)
{
    log_debug("uuid:%s, config:%s, src node id:%d", mqtt->uuid, req->name,
              req->node_id);

    int rc = delete_group_config(plugin, req->name, req->node_id);

    char *            json_str = NULL;
    neu_parse_error_t error    = { .error = rc };

    rc = neu_json_encode_with_mqtt(&error, neu_parse_encode_error, mqtt,
                                   neu_parse_encode_mqtt_param, &json_str);
    return json_str;
}
