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

#include "datatag.h"
#include "json/neu_json_error.h"

char *command_get_tags(neu_plugin_t *plugin, neu_json_mqtt_t *mqtt,
                       neu_json_get_tags_req_t *req)
{
    neu_json_error_resp_t error  = { 0 };
    char *                result = NULL;
    if (NULL == req) {
        error.error = NEU_ERR_PARAM_IS_WRONG;
        neu_json_encode_with_mqtt(&error, neu_json_encode_error_resp, mqtt,
                                  neu_json_encode_mqtt_resp, &result);
        return result;
    }

    log_info("Get tag list uuid:%s, node id:%d", mqtt->uuid, req->node_id);

    neu_datatag_table_t *table =
        neu_system_get_datatags_table(plugin, req->node_id);
    if (NULL == table) {
        error.error = NEU_ERR_NODE_NOT_EXIST;
        neu_json_encode_with_mqtt(&error, neu_json_encode_error_resp, mqtt,
                                  neu_json_encode_mqtt_resp, &result);
        return result;
    }

    vector_t grp_configs = neu_system_get_group_configs(plugin, req->node_id);
    neu_json_get_tags_resp_t res   = { 0 };
    int                      index = 0;

    VECTOR_FOR_EACH(&grp_configs, iter)
    {
        neu_taggrp_config_t *config =
            *(neu_taggrp_config_t **) iterator_get(&iter);
        vector_t *ids = neu_taggrp_cfg_get_datatag_ids(config);
        if (NULL != req->group_config_name &&
            strncmp(neu_taggrp_cfg_get_name(config), req->group_config_name,
                    strlen(req->group_config_name)) != 0) {
            continue;
        }

        res.n_tag += ids->size;
    }

    res.tags = calloc(res.n_tag, sizeof(neu_json_get_tags_resp_tag_t));

    VECTOR_FOR_EACH(&grp_configs, iter)
    {
        neu_taggrp_config_t *config =
            *(neu_taggrp_config_t **) iterator_get(&iter);
        const char *group_name = neu_taggrp_cfg_get_name(config);

        if (NULL != req->group_config_name &&
            strncmp(group_name, req->group_config_name,
                    strlen(req->group_config_name)) != 0) {
            continue;
        }

        vector_t *ids = neu_taggrp_cfg_get_datatag_ids(config);

        VECTOR_FOR_EACH(ids, iter_id)
        {
            neu_datatag_id_t *id  = (neu_datatag_id_t *) iterator_get(&iter_id);
            neu_datatag_t *   tag = neu_datatag_tbl_get(table, *id);

            res.tags[index].id                = *id;
            res.tags[index].name              = tag->name;
            res.tags[index].group_config_name = (char *) group_name;
            res.tags[index].type              = tag->type;
            res.tags[index].attribute         = tag->attribute;
            res.tags[index].address           = tag->addr_str;

            index += 1;
        }
    }

    neu_json_encode_with_mqtt(&res, neu_json_encode_get_tags_resp, mqtt,
                              neu_json_encode_mqtt_resp, &result);
    vector_uninit(&grp_configs);
    free(res.tags);
    return result;
}

char *command_add_tags(neu_plugin_t *plugin, neu_json_mqtt_t *mqtt,
                       neu_json_add_tags_req_t *req)
{
    neu_json_error_resp_t error  = { 0 };
    char *                result = NULL;
    if (NULL == req) {
        error.error = NEU_ERR_PARAM_IS_WRONG;
        neu_json_encode_with_mqtt(&error, neu_json_encode_error_resp, mqtt,
                                  neu_json_encode_mqtt_resp, &result);
        return result;
    }

    log_info("Add tags uuid:%s, node id:%d, group config name:%s", mqtt->uuid,
             req->node_id, req->group_config_name);

    neu_taggrp_config_t *new_config = NULL;
    neu_taggrp_config_t *config     = neu_system_find_group_config(
        plugin, req->node_id, req->group_config_name);
    neu_datatag_table_t *table =
        neu_system_get_datatags_table(plugin, req->node_id);

    if (NULL == table) {
        error.error = NEU_ERR_NODE_NOT_EXIST;
        neu_json_encode_with_mqtt(&error, neu_json_encode_error_resp, mqtt,
                                  neu_json_encode_mqtt_resp, &result);
        return result;
    }

    if (NULL == config) {
        new_config = neu_taggrp_cfg_new(req->group_config_name);
        neu_taggrp_cfg_set_interval(new_config, 10000);
    } else {
        new_config = neu_taggrp_cfg_clone(config);
    }

    vector_t *     ids  = neu_taggrp_cfg_get_datatag_ids(new_config);
    neu_err_code_e code = { 0 };

    for (int i = 0; i < req->n_tag; i++) {
        if (!neu_tag_check_attribute(req->tags[i].attribute)) {
            code = NEU_ERR_TAG_ATTRIBUTE_NOT_SUPPORT;
            continue;
        }

        neu_datatag_t *tag =
            neu_datatag_alloc(req->tags[i].attribute, req->tags[i].type,
                              req->tags[i].address, req->tags[i].name);

        if (NULL == tag) {
            code = NEU_ERR_ENOMEM;
            continue;
        }

        if (neu_datatag_tbl_add(table, tag)) {
            vector_push_back(ids, &tag->id);
        } else {
            log_warn_node(plugin,
                          "Add failed, tag name exists, uuid:%s, "
                          "node_id:%d, req_tag_name:%s",
                          mqtt->uuid, req->node_id, req->tags[i].name);
            code = NEU_ERR_TAG_NAME_EXIST;
            free(tag);
        }
    }

    int rv = 0;
    if (config != NULL) {
        rv = neu_system_update_group_config(plugin, req->node_id, new_config);
    } else {
        rv = neu_system_add_group_config(plugin, req->node_id, new_config);
    }

    if (0 != rv) {
        code = rv;
    }

    error.error = code;
    neu_json_encode_with_mqtt(&error, neu_json_encode_error_resp, mqtt,
                              neu_json_encode_mqtt_resp, &result);
    return result;
}

char *command_update_tags(neu_plugin_t *plugin, neu_json_mqtt_t *mqtt,
                          neu_json_update_tags_req_t *req)
{
    neu_json_error_resp_t error  = { 0 };
    char *                result = NULL;
    if (NULL == req) {
        error.error = NEU_ERR_PARAM_IS_WRONG;
        neu_json_encode_with_mqtt(&error, neu_json_encode_error_resp, mqtt,
                                  neu_json_encode_mqtt_resp, &result);
        return result;
    }

    log_info("Update tags uuid:%s, node id:%d", mqtt->uuid, req->node_id);

    neu_datatag_table_t *table =
        neu_system_get_datatags_table(plugin, req->node_id);
    if (NULL == table) {
        error.error = NEU_ERR_NODE_NOT_EXIST;
        neu_json_encode_with_mqtt(&error, neu_json_encode_error_resp, mqtt,
                                  neu_json_encode_mqtt_resp, &result);
        return result;
    }

    neu_err_code_e code = { 0 };
    for (int i = 0; i < req->n_tag; i++) {
        neu_datatag_t *tag = neu_datatag_tbl_get(table, req->tags[i].id);

        if (tag == NULL) {
            code = NEU_ERR_TAG_NOT_EXIST;
            continue;
        }

        if (!neu_tag_check_attribute(req->tags[i].attribute)) {
            code = NEU_ERR_TAG_ATTRIBUTE_NOT_SUPPORT;
            continue;
        }

        tag = neu_datatag_alloc(req->tags[i].attribute, req->tags[i].type,
                                req->tags[i].address, req->tags[i].name);

        if (NULL == tag) {
            code = NEU_ERR_ENOMEM;
            continue;
        }

        if (0 != neu_datatag_tbl_update(table, req->tags[i].id, tag)) {
            log_warn_node(plugin,
                          "Update failed, tag name exists, uuid:%s, "
                          "node_id:%d, tag_id:%d, req_tag_name:%s",
                          mqtt->uuid, req->node_id, req->tags[i].id,
                          req->tags[i].name);
            code = NEU_ERR_TAG_NAME_EXIST;
            free(tag);
        }
    }

    error.error = code;
    neu_json_encode_with_mqtt(&error, neu_json_encode_error_resp, mqtt,
                              neu_json_encode_mqtt_resp, &result);
    return result;
}

char *command_delete_tags(neu_plugin_t *plugin, neu_json_mqtt_t *mqtt,
                          neu_json_del_tags_req_t *req)
{
    neu_json_error_resp_t error  = { 0 };
    char *                result = NULL;
    if (NULL == req) {
        error.error = NEU_ERR_PARAM_IS_WRONG;
        neu_json_encode_with_mqtt(&error, neu_json_encode_error_resp, mqtt,
                                  neu_json_encode_mqtt_resp, &result);
        return result;
    }

    log_info("Delete tags uuid:%s, node id:%d, group config:%s", mqtt->uuid,
             req->node_id, req->group_config_name);

    neu_taggrp_config_t *new_config = NULL;
    neu_taggrp_config_t *config     = neu_system_find_group_config(
        plugin, req->node_id, req->group_config_name);
    neu_datatag_table_t *table =
        neu_system_get_datatags_table(plugin, req->node_id);

    if (NULL == config || NULL == table) {
        error.error = NEU_ERR_NODE_NOT_EXIST;
        neu_json_encode_with_mqtt(&error, neu_json_encode_error_resp, mqtt,
                                  neu_json_encode_mqtt_resp, &result);
        return result;
    }

    new_config = neu_taggrp_cfg_clone(config);

    for (int i = 0; i < req->n_id; i++) {
        if (neu_datatag_tbl_remove(table, req->ids[i]) == 0) {
            vector_t *ids = neu_taggrp_cfg_get_datatag_ids(new_config);

            VECTOR_FOR_EACH(ids, iter)
            {
                neu_datatag_id_t *id = (neu_datatag_id_t *) iterator_get(&iter);
                if (*id == req->ids[i]) {
                    iterator_erase(ids, &iter);
                    break;
                }
            }
        }
    }

    int rv = neu_system_update_group_config(plugin, req->node_id, new_config);
    if (0 != rv) {
        error.error = rv;
    }

    neu_json_encode_with_mqtt(&error, neu_json_encode_error_resp, mqtt,
                              neu_json_encode_mqtt_resp, &result);
    return result;
}
