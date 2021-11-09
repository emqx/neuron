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

typedef struct {
    char *           name;
    int              type;
    char *           address;
    int              attribute;
    char *           group_config_name;
    neu_datatag_id_t id;
} tag_t;

struct visit_context {
    neu_datatag_table_t *table;
    vector_t *           tags;
};

void visit_func(neu_taggrp_config_t *config, void *context)
{
    struct visit_context *ctx = (struct visit_context *) context;
    if (NULL == config) {
        return;
    }

    char *group_config_name = strdup((char *) neu_taggrp_cfg_get_name(config));
    vector_t *tag_ids       = neu_taggrp_cfg_get_datatag_ids(config);
    if (NULL == tag_ids) {
        return;
    }

    neu_datatag_id_t id;
    VECTOR_FOR_EACH(tag_ids, id_iter)
    {
        id = *(neu_datatag_id_t *) iterator_get(&id_iter);

        tag_t *tag = malloc(sizeof(tag_t));
        if (NULL == tag) {
            continue;
        }
        memset(tag, 0, sizeof(tag_t));

        neu_datatag_t *datatag = NULL;
        datatag                = neu_datatag_tbl_get(ctx->table, id);
        if (NULL == datatag) {
            continue;
        }

        tag->name              = strdup(datatag->name);
        tag->type              = datatag->type;
        tag->address           = strdup(datatag->addr_str);
        tag->attribute         = datatag->attribute;
        tag->group_config_name = strdup(group_config_name);
        tag->id                = id;

        log_debug("id:%u, config name:%s, "
                  "attr:%d, address:%s, type:%d, name:%s",
                  tag->id, tag->group_config_name, tag->attribute, tag->address,
                  tag->type, tag->name);

        vector_push_back(ctx->tags, tag);
    }
}

static int get_all_tag(neu_plugin_t *plugin, const uint32_t node_id,
                       vector_t *tags)
{
    int rc = common_has_node(plugin, node_id);
    if (0 != rc) {
        return -1;
    }

    vector_t configs = neu_system_get_group_configs(plugin, node_id);
    if (0 == configs.size) {
        GROUP_CONFIGS_UNINIT(configs);
        return -2;
    }

    neu_datatag_table_t *table = common_get_datatags_table(plugin, node_id);

    if (NULL == table) {
        GROUP_CONFIGS_UNINIT(configs);
        return -3;
    }

    struct visit_context ctx = { .table = table, .tags = tags };
    common_group_configs_foreach(plugin, node_id, &ctx, visit_func);

    return 0;
}

static int wrap_get_tags_response_json_object(vector_t *                tags,
                                              neu_parse_get_tags_res_t *json)
{
    json->n_tag = tags->size;
    json->tags  = malloc(tags->size * sizeof(neu_parse_get_tags_res_tag_t));
    memset(json->tags, 0, tags->size * sizeof(neu_parse_get_tags_res_tag_t));

    tag_t *tag  = NULL;
    int    size = tags->size;
    for (int i = 0; i < size; i++) {
        tag = vector_get(tags, i);
        if (NULL == tag) {
            continue;
        }

        json->tags[i].name              = tag->name;
        json->tags[i].type              = tag->type;
        json->tags[i].address           = tag->address;
        json->tags[i].attribute         = tag->attribute;
        json->tags[i].group_config_name = tag->group_config_name;
        json->tags[i].id                = tag->id;
    }

    return 0;
}

static void free_get_tags_response_object(vector_t *                tags,
                                          neu_parse_get_tags_res_t *json)
{
    tag_t *tag  = NULL;
    int    size = tags->size;
    for (int i = 0; i < size; i++) {
        tag = vector_get(tags, i);
        if (NULL == tag) {
            continue;
        }

        if (NULL != tag->name) {
            free(tag->name);
        }
        if (NULL != tag->address) {
            free(tag->address);
        }
        if (NULL != tag->group_config_name) {
            free(tag->group_config_name);
        }
    }

    if (NULL != json->tags) {
        free(json->tags);
    }
}

char *command_get_tags(neu_plugin_t *plugin, neu_parse_mqtt_t *mqtt,
                       neu_parse_get_tags_req_t *req)
{
    log_info("Get tag list uuid:%s, node id:%d", mqtt->uuid, req->node_id);
    char *                   json_str = NULL;
    neu_parse_get_tags_res_t res      = { 0 };
    neu_parse_error_t        error    = { 0 };

    uint32_t node_id = req->node_id;
    vector_t tags;
    vector_init(&tags, 0, sizeof(tag_t));
    int rc = get_all_tag(plugin, node_id, &tags);

    if (0 != rc) {
        error.error = rc;
        rc = neu_json_encode_with_mqtt(&error, neu_parse_encode_error, mqtt,
                                       neu_parse_encode_mqtt_param, &json_str);
        if (0 == rc) {
            return json_str;
        }
        return NULL;
    }

    rc = wrap_get_tags_response_json_object(&tags, &res);
    rc = neu_json_encode_with_mqtt(&res, neu_parse_encode_get_tags, mqtt,
                                   neu_parse_encode_mqtt_param, &json_str);

    free_get_tags_response_object(&tags, &res);
    vector_uninit(&tags);

    if (0 == rc) {
        return json_str;
    }

    return NULL;
}

char *command_add_tags(neu_plugin_t *plugin, neu_parse_mqtt_t *mqtt,
                       neu_parse_add_tags_req_t *req)
{
    log_info("Add tags uuid:%s, node id:%d, group config name:%s", mqtt->uuid,
             req->node_id, req->group_config_name);

    char *            json_str = NULL;
    uint32_t          node_id  = req->node_id;
    neu_parse_error_t error    = { 0 };

    int rc = common_has_node(plugin, node_id);
    if (0 != rc) {
        error.error = -1;
        rc = neu_json_encode_with_mqtt(&error, neu_parse_encode_error, mqtt,
                                       neu_parse_encode_mqtt_param, &json_str);
        if (0 == rc) {
            return json_str;
        }
        return NULL;
    }

    neu_taggrp_config_t *config;
    config = common_get_group_config(plugin, node_id, req->group_config_name);
    if (NULL == config) {
        error.error = -2;
        rc = neu_json_encode_with_mqtt(&error, neu_parse_encode_error, mqtt,
                                       neu_parse_encode_mqtt_param, &json_str);
        if (0 == rc) {
            return json_str;
        }
        return NULL;
    }

    neu_datatag_table_t *table = common_get_datatags_table(plugin, node_id);
    if (NULL == table) {
        error.error = -3;
        rc = neu_json_encode_with_mqtt(&error, neu_parse_encode_error, mqtt,
                                       neu_parse_encode_mqtt_param, &json_str);
        if (0 == rc) {
            return json_str;
        }
        return NULL;
    }

    for (int i = 0; i < req->n_tag; i++) {
        neu_datatag_t *datatag = malloc(sizeof(neu_datatag_t));
        datatag->name          = strdup(req->tags[i].name);
        datatag->attribute     = req->tags[i].attribute;
        datatag->type          = req->tags[i].type;
        datatag->addr_str      = strdup(req->tags[i].address);
        datatag_id_t id        = neu_datatag_tbl_add(table, datatag);

        log_info("Add datatag to the datatag table, id:%ld", id);
        if (0 != id) {
            vector_t *ids    = neu_taggrp_cfg_get_datatag_ids(config);
            int *     tag_id = malloc(sizeof(uint32_t));
            *tag_id          = id;
            rc               = vector_push_back(ids, tag_id);
            log_info("Add datatag id to the config:%s, id:%ld, error code:%d",
                     req->group_config_name, id, rc);
        }
    }

    rc = neu_json_encode_with_mqtt(&error, neu_parse_encode_error, mqtt,
                                   neu_parse_encode_mqtt_param, &json_str);
    if (0 == rc) {
        return json_str;
    }

    return NULL;
}

char *command_update_tags(neu_plugin_t *plugin, neu_parse_mqtt_t *mqtt,
                          neu_parse_update_tags_req_t *req)
{
    log_info("Update tags uuid:%s, node id:%d", mqtt->uuid, req->node_id);

    char *            json_str = NULL;
    uint32_t          node_id  = req->node_id;
    neu_parse_error_t error    = { 0 };

    int rc = common_has_node(plugin, node_id);
    if (0 != rc) {
        error.error = -1;
        rc = neu_json_encode_with_mqtt(&error, neu_parse_encode_error, mqtt,
                                       neu_parse_encode_mqtt_param, &json_str);
        if (0 == rc) {
            return json_str;
        }
        return NULL;
    }

    neu_datatag_table_t *table = common_get_datatags_table(plugin, node_id);
    if (NULL == table) {
        error.error = -3;
        rc = neu_json_encode_with_mqtt(&error, neu_parse_encode_error, mqtt,
                                       neu_parse_encode_mqtt_param, &json_str);
        if (0 == rc) {
            return json_str;
        }
        return NULL;
    }

    for (int i = 0; i < req->n_tag; i++) {
        neu_datatag_t *datatag =
            neu_datatag_tbl_get(table, req->tags[i].tag_id);

        if (NULL == datatag) {
            continue;
        }

        free(datatag->name);
        free(datatag->addr_str);

        datatag->id        = req->tags[i].tag_id;
        datatag->name      = strdup(req->tags[i].name);
        datatag->attribute = req->tags[i].attribute;
        datatag->type      = req->tags[i].type;
        datatag->addr_str  = strdup(req->tags[i].address);
        rc = neu_datatag_tbl_update(table, req->tags[i].tag_id, datatag);

        log_info("Update datatag to the datatag table, id:%ld", datatag->id);
    }

    rc = neu_json_encode_with_mqtt(&error, neu_parse_encode_error, mqtt,
                                   neu_parse_encode_mqtt_param, &json_str);

    if (0 == rc) {
        return json_str;
    }

    return NULL;
}

char *command_delete_tags(neu_plugin_t *plugin, neu_parse_mqtt_t *mqtt,
                          neu_parse_del_tags_req_t *req)
{
    log_info("Delete tags uuid:%s, node id:%d, group config:%s", mqtt->uuid,
             req->node_id, req->group_config_name);

    char *            json_str = NULL;
    uint32_t          node_id  = req->node_id;
    neu_parse_error_t error    = { 0 };

    int rc = common_has_node(plugin, node_id);
    if (0 != rc) {
        error.error = -1;
        rc = neu_json_encode_with_mqtt(&error, neu_parse_encode_error, mqtt,
                                       neu_parse_encode_mqtt_param, &json_str);
        if (0 == rc) {
            return json_str;
        }
        return NULL;
    }

    neu_taggrp_config_t *config =
        neu_system_find_group_config(plugin, node_id, req->group_config_name);
    if (NULL == config) {
        error.error = -2;
        rc = neu_json_encode_with_mqtt(&error, neu_parse_encode_error, mqtt,
                                       neu_parse_encode_mqtt_param, &json_str);
        if (0 == rc) {
            return json_str;
        }
        return NULL;
    }

    neu_datatag_table_t *table =
        neu_system_get_datatags_table(plugin, req->node_id);
    if (NULL == table) {
        error.error = -3;
        rc = neu_json_encode_with_mqtt(&error, neu_parse_encode_error, mqtt,
                                       neu_parse_encode_mqtt_param, &json_str);
        if (0 == rc) {
            return json_str;
        }
        return NULL;
    }

    for (int i = 0; i < req->n_tag_id; i++) {
        rc = neu_datatag_tbl_remove(table, req->tag_ids[i]);
        if (0 == rc) {
            vector_t *ids = neu_taggrp_cfg_get_datatag_ids(config);
            VECTOR_FOR_EACH(ids, iter)
            {
                neu_datatag_id_t *id = (neu_datatag_id_t *) iterator_get(&iter);
                if (*id == req->tag_ids[i]) {
                    iterator_erase(ids, &iter);
                    break;
                }
            }
        }
    }

    rc = neu_json_encode_with_mqtt(&error, neu_parse_encode_error, mqtt,
                                   neu_parse_encode_mqtt_param, &json_str);
    if (0 == rc) {
        return json_str;
    }

    return NULL;
}