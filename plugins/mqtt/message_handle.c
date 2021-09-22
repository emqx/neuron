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

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "message_handle.h"

#define UNUSED(x) (void) (x)
#define GROUP_CONFIGS_UINIT(configs)                                        \
    {                                                                       \
        VECTOR_FOR_EACH(&configs, iter)                                     \
        {                                                                   \
            neu_taggrp_config_t *cur_grp_config;                            \
            cur_grp_config = *(neu_taggrp_config_t **) iterator_get(&iter); \
            neu_taggrp_cfg_free(cur_grp_config);                            \
        }                                                                   \
        vector_uninit(&configs);                                            \
    }

static paho_client_t *g_paho;
static uint32_t       req_node_id;

static const char tags[5][20] = { "1!400001", "1!400003", "1!00001", "1!00002",
                                  "1!00003" };
static datatag_id_t ids[5];

void message_handle_set_paho_client(paho_client_t *paho)
{
    g_paho = paho;
}

static int send_mqtt_response(char *buff, const size_t len)
{
    client_error error = paho_client_publish(g_paho, "neuronlite/response", 0,
                                             (unsigned char *) buff, len);
    return error;
}

static int node_id_exist(vector_t *v, const uint32_t node_id)
{
    neu_node_info_t *node_info;
    VECTOR_FOR_EACH(v, iter)
    {
        node_info = (neu_node_info_t *) iterator_get(&iter);
        log_info("node info id:%ld, name:%s", node_info->node_id,
                 node_info->node_name);
        if (node_id == node_info->node_id) {
            return 0;
        }
    }
    return -1;
}

static int config_exist(vector_t *v, const char *config_name)
{
    neu_taggrp_config_t *config;

    VECTOR_FOR_EACH(v, iter)
    {
        config = *(neu_taggrp_config_t **) iterator_get(&iter);
        if (NULL == config) {
            continue;
        }

        if (0 == strcmp(config_name, neu_taggrp_cfg_get_name(config))) {
            return 0;
        }
    }
    return -1;
}

void message_handle_init_tags(neu_plugin_t *plugin)
{
    vector_t         nodes = neu_system_get_nodes(plugin, NEU_NODE_TYPE_DRIVER);
    neu_node_info_t *node_info;
    node_info = (neu_node_info_t *) vector_get(&nodes, 1); // modbus nodeid
    uint32_t dest_node_id = node_info->node_id;

    neu_datatag_table_t *table =
        neu_system_get_datatags_table(plugin, dest_node_id);
    if (NULL == table) {
        return;
    }

    neu_datatag_id_t id;
    neu_addr_str_t   addr_str = (char *) tags[0];
    id                        = 0;
    neu_datatag_t *datatag0   = malloc(sizeof(neu_datatag_t));
    datatag0->id              = id;
    datatag0->attribute       = NEU_ATTRIBUTETYPE_READ;
    datatag0->type            = NEU_DATATYPE_WORD;
    datatag0->addr_str        = addr_str;
    ids[0]                    = neu_datatag_tbl_add(table, datatag0);

    addr_str                = (char *) tags[1];
    id                      = 1;
    neu_datatag_t *datatag1 = malloc(sizeof(neu_datatag_t));
    datatag1->id            = id;
    datatag1->attribute     = NEU_ATTRIBUTETYPE_READ;
    datatag1->type          = NEU_DATATYPE_UWORD;
    datatag1->addr_str      = addr_str;
    ids[1]                  = neu_datatag_tbl_add(table, datatag1);

    addr_str                = (char *) tags[2];
    id                      = 2;
    neu_datatag_t *datatag2 = malloc(sizeof(neu_datatag_t));
    datatag2->id            = id;
    datatag2->attribute     = NEU_ATTRIBUTETYPE_READ;
    datatag2->type          = NEU_DATATYPE_BYTE;
    datatag2->addr_str      = addr_str;
    ids[2]                  = neu_datatag_tbl_add(table, datatag2);
}

void message_handle_read(neu_plugin_t *plugin, neu_parse_mqtt_t *mqtt,
                         neu_parse_read_req_t *req)
{
    log_info("READ uuid:%s, node id:%u", mqtt->uuid, req->node_id);

    req_node_id    = req->node_id;
    vector_t nodes = neu_system_get_nodes(plugin, NEU_NODE_TYPE_DRIVER);
    int      rc    = node_id_exist(&nodes, req->node_id);
    vector_uninit(&nodes);
    if (0 != rc) {
        return;
    }

    vector_t configs = neu_system_get_group_configs(plugin, req->node_id);
    neu_taggrp_config_t *config;
    neu_taggrp_config_t *c;
    config          = *(neu_taggrp_config_t **) vector_get(&configs, 0);
    c               = (neu_taggrp_config_t *) neu_taggrp_cfg_ref(config);
    uint32_t req_id = neu_plugin_send_read_cmd(plugin, req->node_id, c);

    GROUP_CONFIGS_UINIT(configs);
    UNUSED(req_id);
}

static neu_datatag_t *get_datatag_by_id(neu_plugin_t *plugin, datatag_id_t id)
{
    neu_datatag_table_t *table =
        neu_system_get_datatags_table(plugin, req_node_id);
    if (NULL == table) {
        return NULL;
    }

    return neu_datatag_tbl_get(table, id);
}

void message_handle_read_result(neu_plugin_t *       plugin,
                                neu_taggrp_config_t *grp_config,
                                neu_variable_t *     variable)
{
    log_info("Group config name:%s", neu_taggrp_cfg_get_name(grp_config));

    if (NULL == variable) {
        return;
    }

    char *               result = NULL;
    neu_parse_read_res_t res    = { 0 };

    vector_t *ids = neu_taggrp_cfg_get_datatag_ids(grp_config);
    if (NULL == ids) {
        return;
    }

    //    res.error         = neu_variable_get_error(variable);
    res.n_tag         = ids->size;
    neu_variable_t *v = neu_variable_next(variable);

    res.tags = (struct neu_parse_read_res_tag *) calloc(
        res.n_tag, sizeof(struct neu_parse_read_res_tag));

    datatag_id_t tag_id;
    int          index = 0;
    VECTOR_FOR_EACH(ids, iter)
    {
        tag_id                 = *(datatag_id_t *) iterator_get(&iter);
        neu_datatag_t *datatag = get_datatag_by_id(plugin, tag_id);
        if (NULL == datatag) {
            continue;
        }

        if (NULL == v) {
            continue;
        }

        int type             = neu_variable_get_type(v);
        res.tags[index].name = strdup(datatag->name);

        switch (type) {
        case NEU_DATATYPE_BYTE: {
            int8_t value;
            neu_variable_get_byte(v, &value);
            res.tags[index].value.val_bit = value;
            break;
        }
        case NEU_DATATYPE_BOOLEAN: {
            bool value;
            neu_variable_get_boolean(v, &value);
            res.tags[index].value.val_bool = value;
            break;
        }
        case NEU_DATATYPE_WORD: {
            int16_t value;
            neu_variable_get_word(v, &value);
            res.tags[index].value.val_int = value;
            break;
        }
        case NEU_DATATYPE_DWORD: {
            int32_t value;
            neu_variable_get_dword(v, &value);
            res.tags[index].value.val_int = value;
            break;
        }
        case NEU_DATATYPE_STRING:
            break;
        case NEU_DATATYPE_FLOAT: {
            float value;
            neu_variable_get_float(v, &value);
            res.tags[index].value.val_float = value;
            break;
        }

        default:
            break;
        }

        v = neu_variable_next(v);
        index++;
    }

    neu_parse_mqtt_t mqtt = { .function = NEU_MQTT_OP_READ, .uuid = "123-456" };

    int rc = neu_json_encode_with_mqtt(&res, neu_parse_encode_read, &mqtt,
                                       neu_parse_encode_mqtt_param, &result);
    if (0 == rc) {
        send_mqtt_response(result, strlen(result));
    }

    int i;
    for (i = 0; i < res.n_tag; i++) {
        free(res.tags[i].name);
    }
    free(res.tags);
    free(result);
}

static int write_command(neu_plugin_t *plugin, uint32_t dest_node_id,
                         neu_variable_t *data_var)
{
    vector_t nodes = neu_system_get_nodes(plugin, NEU_NODE_TYPE_DRIVER);
    int      rc    = node_id_exist(&nodes, dest_node_id);
    vector_uninit(&nodes);

    if (0 != rc) {
        return -1;
    }

    vector_t configs = neu_system_get_group_configs(plugin, dest_node_id);
    neu_taggrp_config_t *config;
    neu_taggrp_config_t *c;
    config = *(neu_taggrp_config_t **) vector_get(&configs, 0);
    c      = (neu_taggrp_config_t *) neu_taggrp_cfg_ref(config);

    uint32_t req_id =
        neu_plugin_send_write_cmd(plugin, dest_node_id, c, data_var);

    GROUP_CONFIGS_UINIT(configs);
    return req_id;
}

void message_handle_write(neu_plugin_t *plugin, neu_parse_mqtt_t *mqtt,
                          struct neu_parse_write_req *write_req)
{
    log_info("WRITE uuid:%s", mqtt->uuid);

    req_node_id          = write_req->node_id;
    neu_variable_t *head = NULL;
    head                 = neu_variable_create();
    neu_variable_set_error(head, 0);
    for (int i = 0; i < write_req->n_tag; i++) {
        enum neu_json_type   type  = write_req->tags[i].t;
        union neu_json_value value = write_req->tags[i].value;
        neu_variable_t *     v     = neu_variable_create();

        char key[16] = { 0 };
        sprintf(key, "%d", write_req->tags[i].tag_id);
        neu_variable_set_key(v, key);

        switch (type) {
        case NEU_JSON_INT:
            neu_variable_set_dword(v, value.val_int);
            break;
        case NEU_JSON_STR:
            neu_variable_set_string(v, value.val_str);
            break;
        case NEU_JSON_DOUBLE:
            neu_variable_set_double(v, value.val_double);
            break;
        case NEU_JSON_BOOL:
            neu_variable_set_boolean(v, value.val_bool);
            break;

        default:
            break;
        }
        neu_variable_add_item(head, v);
    }
    write_command(plugin, write_req->node_id, head->next);
    neu_variable_destroy(head);

    char *            json_str = NULL;
    neu_parse_error_t error    = { 0 };

    int rc = neu_json_encode_with_mqtt(&error, neu_parse_encode_error, mqtt,
                                       neu_parse_encode_mqtt_param, &json_str);
    if (0 == rc) {
        send_mqtt_response(json_str, strlen(json_str));
    }

    free(json_str);
}

void message_handle_get_tag_list(neu_plugin_t *plugin, neu_parse_mqtt_t *mqtt,
                                 neu_parse_get_tags_req_t *req)
{
    log_info("Get tag list uuid:%s", mqtt->uuid);
    char *                   result = NULL;
    neu_parse_get_tags_res_t res    = { 0 };
    neu_parse_error_t        error  = { 0 };

    uint32_t dest_node_id = req->node_id;
    vector_t nodes        = neu_system_get_nodes(plugin, NEU_NODE_TYPE_DRIVER);
    int      rc           = node_id_exist(&nodes, dest_node_id);
    vector_uninit(&nodes);

    if (0 != rc) {
        error.error = -1;
        rc = neu_json_encode_with_mqtt(&error, neu_parse_encode_error, mqtt,
                                       neu_parse_encode_mqtt_param, &result);
        if (0 == rc) {
            send_mqtt_response(result, strlen(result));
        }
        return;
    }

    neu_datatag_table_t *table =
        neu_system_get_datatags_table(plugin, dest_node_id);
    if (NULL == table) {
        error.error = -2;
        rc = neu_json_encode_with_mqtt(&error, neu_parse_encode_error, mqtt,
                                       neu_parse_encode_mqtt_param, &result);
        if (0 == rc) {
            send_mqtt_response(result, strlen(result));
        }
        return;
    }

    res.tags = (neu_parse_get_tags_res_tag_t *) calloc(
        5, sizeof(neu_parse_get_tags_res_tag_t));

    int i;
    for (i = 0; i < 5; i++) {
        neu_datatag_t *ret = NULL;

        ret = neu_datatag_tbl_get(table, ids[i]);
        if (NULL == ret) {
            continue;
        }

        res.tags[i].name      = strdup(ret->addr_str);
        res.tags[i].type      = 0;
        res.tags[i].address   = strdup(ret->addr_str);
        res.tags[i].attribute = 0;
    }

    rc = neu_json_encode_with_mqtt(&res, neu_parse_encode_get_tags, mqtt,
                                   neu_parse_encode_mqtt_param, &result);
    if (0 == rc) {
        send_mqtt_response(result, strlen(result));
    }

    for (i = 0; i < 5; i++) {
        free(res.tags[i].name);
        free(res.tags[i].address);
    }
    free(res.tags);
    free(result);
}

void message_handle_add_tag(neu_plugin_t *plugin, neu_parse_mqtt_t *mqtt,
                            neu_parse_add_tags_req_t *req)
{
    char *result = NULL;

    uint32_t dest_node_id = req->node_id;
    vector_t nodes        = neu_system_get_nodes(plugin, NEU_NODE_TYPE_DRIVER);
    int      rc           = node_id_exist(&nodes, dest_node_id);
    neu_parse_error_t error = { 0 };

    vector_uninit(&nodes);

    if (0 != rc) {
        error.error = -1;
        rc = neu_json_encode_with_mqtt(&error, neu_parse_encode_error, mqtt,
                                       neu_parse_encode_mqtt_param, &result);
        if (0 == rc) {
            send_mqtt_response(result, strlen(result));
        }
        free(result);
        return;
    }

    neu_datatag_table_t *table =
        neu_system_get_datatags_table(plugin, dest_node_id);
    if (NULL == table) {
        error.error = -1;
        rc = neu_json_encode_with_mqtt(&error, neu_parse_encode_error, mqtt,
                                       neu_parse_encode_mqtt_param, &result);
        if (0 == rc) {
            send_mqtt_response(result, strlen(result));
        }
        free(result);
        return;
    }

    for (int i = 0; i < req->n_tag; i++) {
        neu_datatag_t *datatag = malloc(sizeof(neu_datatag_t));
        datatag->name          = strdup(req->tags[i].name);
        datatag->attribute     = req->tags[i].attribute;
        datatag->type          = req->tags[i].type;
        datatag->addr_str      = strdup(req->tags[i].address);
        datatag_id_t id        = neu_datatag_tbl_add(table, datatag);

        log_info("Add datatag to the datatag table, id:%ld", id);
    }

    rc = neu_json_encode_with_mqtt(&error, neu_parse_encode_error, mqtt,
                                   neu_parse_encode_mqtt_param, &result);
    if (0 == rc) {
        send_mqtt_response(result, strlen(result));
    }
    free(result);
}

static void get_group_config_by_name(neu_plugin_t * plugin,
                                     const uint32_t node_id,
                                     const char *   config_name,
                                     neu_parse_get_group_config_res_t *res)
{
    vector_t nodes = neu_system_get_nodes(plugin, NEU_NODE_TYPE_DRIVER);
    int      rc    = node_id_exist(&nodes, node_id);
    if (0 != rc) {
        vector_uninit(&nodes);
        return;
    }

    int      count   = 0;
    vector_t configs = neu_system_get_group_configs(plugin, node_id);
    if (0 == configs.size) {
        GROUP_CONFIGS_UINIT(configs);
        return;
    }

    neu_taggrp_config_t *config;
    VECTOR_FOR_EACH(&configs, iter)
    {
        config = *(neu_taggrp_config_t **) iterator_get(&iter);
        if (NULL == config) {
            continue;
        }
        if (0 != strcmp(config_name, neu_taggrp_cfg_get_name(config))) {
            continue;
        }

        if (NULL == res->rows) {
            res->rows = (neu_parse_get_group_config_res_row_t *) malloc(
                sizeof(neu_parse_get_group_config_res_row_t));
        }
        if (NULL != res->rows) {
            res->rows = realloc(
                res->rows,
                (count + 1) * sizeof(neu_parse_get_group_config_res_row_t));
        }

        res->rows[count].name =
            strdup((char *) neu_taggrp_cfg_get_name(config));
        res->rows[count].interval = neu_taggrp_cfg_get_interval(config);

        vector_t *pipes = neu_taggrp_cfg_get_subpipes(config);
        if (NULL != pipes) {
            res->rows[count].pipe_count = pipes->size;
        }

        vector_t *ids = neu_taggrp_cfg_get_datatag_ids(config);
        if (NULL != ids) {
            res->rows[count].tag_count = ids->size;
        }

        count++;
    }
    res->n_config = count;

    GROUP_CONFIGS_UINIT(configs);
}

static void get_group_config_all(neu_plugin_t *plugin, const uint32_t node_id,
                                 neu_parse_get_group_config_res_t *res)
{
    vector_t nodes = neu_system_get_nodes(plugin, NEU_NODE_TYPE_DRIVER);
    int      rc    = node_id_exist(&nodes, node_id);
    if (0 != rc) {
        vector_uninit(&nodes);
        return;
    }

    int      count   = 0;
    vector_t configs = neu_system_get_group_configs(plugin, node_id);
    if (0 == configs.size) {
        GROUP_CONFIGS_UINIT(configs);
        return;
    }

    neu_taggrp_config_t *config;
    VECTOR_FOR_EACH(&configs, iter)
    {
        config = *(neu_taggrp_config_t **) iterator_get(&iter);
        if (NULL == config) {
            continue;
        }

        if (NULL == res->rows) {
            res->rows = (neu_parse_get_group_config_res_row_t *) malloc(
                sizeof(neu_parse_get_group_config_res_row_t));
        }
        if (NULL != res->rows) {
            res->rows = realloc(
                res->rows,
                (count + 1) * sizeof(neu_parse_get_group_config_res_row_t));
        }

        res->rows[count].name =
            strdup((char *) neu_taggrp_cfg_get_name(config));
        res->rows[count].interval = neu_taggrp_cfg_get_interval(config);

        vector_t *pipes = neu_taggrp_cfg_get_subpipes(config);
        if (NULL != pipes) {
            res->rows[count].pipe_count = pipes->size;
        }

        vector_t *ids = neu_taggrp_cfg_get_datatag_ids(config);
        if (NULL != ids) {
            res->rows[count].tag_count = ids->size;
        }

        count++;
    }
    res->n_config = count;
    GROUP_CONFIGS_UINIT(configs);
}

void message_handle_get_group_config(neu_plugin_t *                    plugin,
                                     neu_parse_mqtt_t *                mqtt,
                                     neu_parse_get_group_config_req_t *req)
{
    log_debug("uuid:%s, node id:%d, config:%s", mqtt->uuid, req->node_id,
              req->name);

    char *                           json_str = NULL;
    neu_parse_get_group_config_res_t res      = { 0 };

    if (NULL != req->name && 0 != strlen(req->name)) {
        get_group_config_by_name(plugin, req->node_id, req->name, &res);
    } else {
        get_group_config_all(plugin, req->node_id, &res);
    }

    int rc =
        neu_json_encode_with_mqtt(&res, neu_parse_encode_get_group_config, mqtt,
                                  neu_parse_encode_mqtt_param, &json_str);
    if (0 == rc) {
        send_mqtt_response(json_str, strlen(json_str));
    }

    int i;
    for (i = 0; i < res.n_config; i++) {
        free(res.rows[i].name);
    }
    free(res.rows);
    free(json_str);
}

static int add_config(neu_plugin_t *plugin, const char *config_name,
                      const uint32_t src_node_id, const uint32_t dst_node_id,
                      const int read_interval)
{
    if (0 == src_node_id || 0 == dst_node_id) {
        return -1;
    }

    vector_t nodes = neu_system_get_nodes(plugin, NEU_NODE_TYPE_DRIVER);
    int      rc    = node_id_exist(&nodes, src_node_id);
    if (0 != rc) {
        vector_uninit(&nodes);
        return -2;
    }
    rc = node_id_exist(&nodes, dst_node_id);
    if (0 != rc) {
        vector_uninit(&nodes);
        return -3;
    }
    vector_uninit(&nodes);

    vector_t configs = neu_system_get_group_configs(plugin, src_node_id);
    rc               = config_exist(&configs, config_name);
    VECTOR_FOR_EACH(&configs, iter)
    {
        neu_taggrp_config_t *cur_grp_config;
        cur_grp_config = *(neu_taggrp_config_t **) iterator_get(&iter);
        neu_taggrp_cfg_free(cur_grp_config);
    }
    vector_uninit(&configs);

    if (0 == rc) {
        return -4;
    }

    neu_taggrp_config_t *grp_config = neu_taggrp_cfg_new((char *) config_name);
    if (NULL == grp_config) {
        return -5;
    }
    neu_taggrp_cfg_set_interval(grp_config, read_interval);
    intptr_t error = neu_system_add_group_config(plugin, src_node_id,
                                                 dst_node_id, grp_config);
    if (0 != error) {
        return -6;
    }
    return 0;
}

void message_handle_add_group_config(neu_plugin_t *                    plugin,
                                     neu_parse_mqtt_t *                mqtt,
                                     neu_parse_add_group_config_req_t *req)
{
    log_debug("uuid:%s, config:%s, src node id:%d, dst node id:%d, "
              "read interval:%d",
              mqtt->uuid, req->name, req->src_node_id, req->dst_node_id,
              req->interval);

    int rc = add_config(plugin, req->name, req->src_node_id, req->dst_node_id,
                        req->interval);

    char *            json_str = NULL;
    neu_parse_error_t error    = { .error = rc };

    rc = neu_json_encode_with_mqtt(&error, neu_parse_encode_error, mqtt,
                                   neu_parse_encode_mqtt_param, &json_str);
    if (0 == rc) {
        send_mqtt_response(json_str, strlen(json_str));
    }

    free(json_str);
}

static int update_config(neu_plugin_t *plugin, const char *config_name,
                         const uint32_t src_node_id, const uint32_t dst_node_id,
                         const int read_interval)
{
    if (0 == src_node_id || 0 == dst_node_id) {
        return -1;
    }

    vector_t nodes = neu_system_get_nodes(plugin, NEU_NODE_TYPE_DRIVER);
    int      rc    = node_id_exist(&nodes, src_node_id);
    if (0 != rc) {
        vector_uninit(&nodes);
        return -2;
    }
    rc = node_id_exist(&nodes, dst_node_id);
    if (0 != rc) {
        vector_uninit(&nodes);
        return -3;
    }
    vector_uninit(&nodes);

    vector_t configs = neu_system_get_group_configs(plugin, src_node_id);
    rc               = config_exist(&configs, config_name);
    VECTOR_FOR_EACH(&configs, iter)
    {
        neu_taggrp_config_t *cur_grp_config;
        cur_grp_config = *(neu_taggrp_config_t **) iterator_get(&iter);
        neu_taggrp_cfg_free(cur_grp_config);
    }
    vector_uninit(&configs);

    if (0 != rc) {
        return -4;
    }

    neu_taggrp_config_t *grp_config = neu_taggrp_cfg_new((char *) config_name);
    if (NULL == grp_config) {
        return -5;
    }
    neu_taggrp_cfg_set_interval(grp_config, read_interval);

    intptr_t error = neu_system_update_group_config(plugin, src_node_id,
                                                    dst_node_id, grp_config);
    if (0 != error) {
        return -6;
    }
    return 0;
}

void message_handle_update_group_config(
    neu_plugin_t *plugin, neu_parse_mqtt_t *mqtt,
    neu_parse_update_group_config_req_t *req)
{
    log_debug("uuid:%s, config:%s, src node id:%d, dst node id:%d, "
              "read interval:%d",
              mqtt->uuid, req->name, req->src_node_id, req->dst_node_id,
              req->interval);

    int rc = update_config(plugin, req->name, req->src_node_id,
                           req->dst_node_id, req->interval);

    char *            json_str = NULL;
    neu_parse_error_t error    = { .error = rc };

    rc = neu_json_encode_with_mqtt(&error, neu_parse_encode_error, mqtt,
                                   neu_parse_encode_mqtt_param, &json_str);
    if (0 == rc) {
        send_mqtt_response(json_str, strlen(json_str));
    }

    free(json_str);
}

static int delete_config(neu_plugin_t *plugin, const char *config_name,
                         const uint32_t node_id)
{
    if (0 == node_id) {
        return -1;
    }

    vector_t nodes = neu_system_get_nodes(plugin, NEU_NODE_TYPE_DRIVER);
    int      rc    = node_id_exist(&nodes, node_id);
    if (0 != rc) {
        vector_uninit(&nodes);
        return -2;
    }
    rc = node_id_exist(&nodes, node_id);
    if (0 != rc) {
        vector_uninit(&nodes);
        return -3;
    }
    vector_uninit(&nodes);

    vector_t configs = neu_system_get_group_configs(plugin, node_id);
    rc               = config_exist(&configs, config_name);
    VECTOR_FOR_EACH(&configs, iter)
    {
        neu_taggrp_config_t *cur_grp_config;
        cur_grp_config = *(neu_taggrp_config_t **) iterator_get(&iter);
        neu_taggrp_cfg_free(cur_grp_config);
    }
    vector_uninit(&configs);

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

void message_handle_delete_group_config(neu_plugin_t *    plugin,
                                        neu_parse_mqtt_t *mqtt,
                                        neu_parse_del_group_config_req_t *req)
{
    log_debug("uuid:%s, config:%s, src node id:%d", mqtt->uuid, req->name,
              req->node_id);

    int rc = delete_config(plugin, req->name, req->node_id);

    char *            json_str = NULL;
    neu_parse_error_t error    = { .error = rc };

    rc = neu_json_encode_with_mqtt(&error, neu_parse_encode_error, mqtt,
                                   neu_parse_encode_mqtt_param, &json_str);
    if (0 == rc) {
        send_mqtt_response(json_str, strlen(json_str));
    }

    free(json_str);
}

static neu_taggrp_config_t *get_group_config(vector_t *  v,
                                             const char *config_name)
{
    neu_taggrp_config_t *config;

    VECTOR_FOR_EACH(v, iter)
    {
        config = *(neu_taggrp_config_t **) iterator_get(&iter);
        if (NULL == config) {
            continue;
        }

        if (0 == strcmp(config_name, neu_taggrp_cfg_get_name(config))) {
            return config;
        }
    }
    return NULL;
}

static int datatag_id_exist(vector_t *v, const uint32_t id, int *index)
{
    *index = 0;
    datatag_id_t *datatag_id;
    VECTOR_FOR_EACH(v, iter)
    {
        datatag_id = (datatag_id_t *) iterator_get(&iter);
        if (id == *datatag_id) {
            return 0;
        }
        (*index)++;
    }

    *index = -1;
    return -1;
}

// static int add_datatag_ids(neu_plugin_t *                    plugin,
// struct neu_parse_add_tag_ids_req *req)
//{
// if (0 == req->node_id) {
// return -1;
//}

// vector_t nodes = neu_system_get_nodes(plugin, NEU_NODE_TYPE_DRIVER);
// int      rc    = node_id_exist(&nodes, req->node_id);
// if (0 != rc) {
// vector_uninit(&nodes);
// return -2;
//}
// vector_uninit(&nodes);

// vector_t configs = neu_system_get_group_configs(plugin, req->node_id);
// neu_taggrp_config_t *config = get_group_config(&configs, req->config);
// if (NULL == config) {
// GROUP_CONFIGS_UINIT(configs);
// return -3;
//}

// vector_t *v = neu_taggrp_cfg_get_datatag_ids(config);
// if (NULL == v) {
// GROUP_CONFIGS_UINIT(configs);
// return -4;
//}

// int exist;
// for (int i = 0; i < req->n_id; i++) {
// int index;
// exist = datatag_id_exist(v, req->rows[i].datatag_id, &index);
// if (0 == exist) {
// continue;
//}
// uint32_t *new_datatag_id = malloc(sizeof(uint32_t));
//*new_datatag_id          = req->rows[i].datatag_id;
// vector_push_back(v, new_datatag_id);
//}

// GROUP_CONFIGS_UINIT(configs);
// return 0;
//}

// void message_handle_add_datatag_ids(neu_plugin_t *                    plugin,
// struct neu_parse_add_tag_ids_req *req)
//{
// char *                           json_str = NULL;
// struct neu_parse_add_tag_ids_res res      = {
//.function = NEU_PARSE_OP_ADD_DATATAG_IDS_CONFIG,
//.uuid     = req->uuid,
//};

// int error = add_datatag_ids(plugin, req);
// res.error = error;

// int rc = neu_parse_encode(&res, &json_str);
// if (0 == rc) {
// send_mqtt_response(json_str, strlen(json_str));
// free(json_str);
//}
//}

// static int delete_datatag_ids(neu_plugin_t *                       plugin,
// struct neu_parse_delete_tag_ids_req *req)
//{
// if (0 == req->node_id) {
// return -1;
//}

// vector_t nodes = neu_system_get_nodes(plugin, NEU_NODE_TYPE_DRIVER);
// int      rc    = node_id_exist(&nodes, req->node_id);
// if (0 != rc) {
// vector_uninit(&nodes);
// return -2;
//}
// vector_uninit(&nodes);

// vector_t configs = neu_system_get_group_configs(plugin, req->node_id);
// neu_taggrp_config_t *config = get_group_config(&configs, req->config);
// if (NULL == config) {
// GROUP_CONFIGS_UINIT(configs);
// return -3;
//}

// vector_t *v = neu_taggrp_cfg_get_datatag_ids(config);
// if (NULL == v) {
// GROUP_CONFIGS_UINIT(configs);
// return -4;
//}

// int exist = 0;
// for (int i = 0; i < req->n_id; i++) {
// int index;
// exist = datatag_id_exist(v, req->rows[i].datatag_id, &index);
// if (0 == exist) {
// vector_erase(v, index);
//}
//}

// GROUP_CONFIGS_UINIT(configs);
// return 0;
//}

// void message_handle_delete_datatag_ids(neu_plugin_t *plugin,
// struct neu_parse_delete_tag_ids_req *req)
//{
// char *                              json_str = NULL;
// struct neu_parse_delete_tag_ids_res res      = {
//.function = NEU_PARSE_OP_DELETE_DATATAG_IDS_CONFIG,
//.uuid     = req->uuid,
//};

// int error = delete_datatag_ids(plugin, req);
// res.error = error;

// int rc = neu_parse_encode(&res, &json_str);
// if (0 == rc) {
// send_mqtt_response(json_str, strlen(json_str));
// free(json_str);
//}
//}
