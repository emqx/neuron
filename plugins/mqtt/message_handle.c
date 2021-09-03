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
#include <stdlib.h>

#include "message_handle.h"

#define UNUSED(x) (void) (x)
static paho_client_t *g_paho;

static const char tags[5][20] = { "1!400001", "1!400003", "1!00001", "1!00002",
                                  "1!00003" };
static datatag_id_t ids[5];

void message_handle_set_paho_client(paho_client_t *paho)
{
    g_paho = paho;
}

static int message_handle_send(char *buff, const size_t len)
{
    client_error error = paho_client_publish(g_paho, "neuronlite/response", 0,
                                             (unsigned char *) buff, len);
    return error;
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
    neu_address_t    datatag_address;
    memset(&datatag_address, 0, sizeof(neu_address_t));
    char *str_addr          = (char *) tags[0];
    id                      = 0;
    neu_datatag_t *datatag0 = malloc(sizeof(neu_datatag_t));
    datatag0->id            = id;
    datatag0->type          = NEU_ATTRIBUTETYPE_READ;
    datatag0->dataType      = NEU_DATATYPE_WORD;
    datatag0->address       = datatag_address;
    datatag0->addr_str      = str_addr;
    ids[0]                  = neu_datatag_tbl_add(table, datatag0);

    str_addr                = (char *) tags[1];
    id                      = 1;
    neu_datatag_t *datatag1 = malloc(sizeof(neu_datatag_t));
    datatag1->id            = id;
    datatag1->type          = NEU_ATTRIBUTETYPE_READ;
    datatag1->dataType      = NEU_DATATYPE_UWORD;
    datatag1->address       = datatag_address;
    datatag1->addr_str      = str_addr;
    ids[1]                  = neu_datatag_tbl_add(table, datatag1);

    str_addr                = (char *) tags[2];
    id                      = 2;
    neu_datatag_t *datatag2 = malloc(sizeof(neu_datatag_t));
    datatag2->id            = id;
    datatag2->type          = NEU_ATTRIBUTETYPE_READ;
    datatag2->dataType      = NEU_DATATYPE_BYTE;
    datatag2->address       = datatag_address;
    datatag2->addr_str      = str_addr;
    ids[2]                  = neu_datatag_tbl_add(table, datatag2);
}

void message_handle_read(neu_plugin_t *plugin, struct neu_parse_read_req *req)
{
    log_info("READ uuid:%s, group:%s", req->uuid, req->group);

    if (0 >= req->n_name) {
        return;
    }

    vector_t         nodes = neu_system_get_nodes(plugin, NEU_NODE_TYPE_DRIVER);
    neu_node_info_t *node_info;
    node_info =
        (neu_node_info_t *) vector_get(&nodes, 1); // modbus nodeid uint32_t
    neu_node_id_t dest_node_id = node_info->node_id;
    vector_t      configs = neu_system_get_group_configs(plugin, dest_node_id);

    neu_taggrp_config_t *config;
    neu_taggrp_config_t *c;
    config          = *(neu_taggrp_config_t **) vector_get(&configs, 0);
    c               = (neu_taggrp_config_t *) neu_taggrp_cfg_ref(config);
    uint32_t req_id = neu_plugin_send_read_cmd(plugin, dest_node_id, c);

    VECTOR_FOR_EACH(&configs, iter)
    {
        neu_taggrp_config_t *cur_grp_config;
        cur_grp_config = *(neu_taggrp_config_t **) iterator_get(&iter);
        neu_taggrp_cfg_free(cur_grp_config);
    }
    vector_uninit(&configs);
    UNUSED(req_id);
}

void message_handle_read_result(neu_variable_t *variable)
{
    if (NULL == variable) {
        return;
    }

    char *                    result = NULL;
    struct neu_parse_read_res res    = {
        .function = NEU_PARSE_OP_READ,
        .uuid     = (char *) "554f5fd8-f437-11eb-975c-7704b9e17821",
        .error    = 0,
        .n_tag    = 0,
    };

    size_t count = neu_variable_count(variable);
    if (0 == count) {
        return;
    }

    int n_tag = count - 1;
    res.tags  = (struct neu_parse_read_res_tag *) calloc(
        n_tag, sizeof(struct neu_parse_read_res_tag));

    int             index = 0;
    neu_variable_t *head  = variable;
    res.error             = neu_variable_get_error(head);
    res.n_tag             = count - 1;
    neu_variable_t *v     = NULL;
    for (v = variable->next; NULL != v; v = neu_variable_next(v)) {

        int type = neu_variable_get_type(v);

        if (NEU_DATATYPE_STRING == type) { }
        if (NEU_DATATYPE_BOOLEAN == type) {
            bool value;
            neu_variable_get_boolean(v, &value);

            res.tags[index].name      = strdup(tags[index]);
            res.tags[index].type      = 1;
            res.tags[index].timestamp = 0;
            res.tags[index].value     = value;
        }
        if (NEU_DATATYPE_WORD == type) {
            int16_t value;
            neu_variable_get_word(v, &value);

            res.tags[index].name      = strdup(tags[index]);
            res.tags[index].type      = 1;
            res.tags[index].timestamp = 0;
            res.tags[index].value     = value;
        }
        if (NEU_DATATYPE_UWORD == type) {
            uint16_t value;
            neu_variable_get_uword(v, &value);

            res.tags[index].name      = strdup(tags[index]);
            res.tags[index].type      = 1;
            res.tags[index].timestamp = 0;
            res.tags[index].value     = value;
        }
        if (NEU_DATATYPE_DOUBLE == type) {
            double value;
            neu_variable_get_double(v, &value);

            res.tags[index].name      = strdup(tags[index]);
            res.tags[index].type      = 1;
            res.tags[index].timestamp = 0;
            res.tags[index].value     = value;
        }
        if (NEU_DATATYPE_BYTE == type) {
            int8_t value;
            neu_variable_get_byte(v, &value);

            res.tags[index].name      = strdup(tags[index]);
            res.tags[index].type      = 1;
            res.tags[index].timestamp = 0;
            res.tags[index].value     = value;
        }
        index++;
    }

    int rc = neu_parse_encode(&res, &result);
    if (0 == rc) {
        message_handle_send(result, strlen(result));
    }

    int i;
    for (i = 0; i < n_tag; i++) {
        free(res.tags[i].name);
    }
    free(res.tags);
    free(result);
}

static int plugin_adapter_write_command(neu_plugin_t *  plugin,
                                        neu_variable_t *data_var)
{
    vector_t         nodes = neu_system_get_nodes(plugin, NEU_NODE_TYPE_DRIVER);
    neu_node_info_t *node_info;
    node_info =
        (neu_node_info_t *) vector_get(&nodes, 1); // modbus nodeid uint32_t
    neu_node_id_t dest_node_id = node_info->node_id;
    vector_t      configs = neu_system_get_group_configs(plugin, dest_node_id);

    neu_taggrp_config_t *config;
    neu_taggrp_config_t *c;
    config = *(neu_taggrp_config_t **) vector_get(&configs, 0);
    c      = (neu_taggrp_config_t *) neu_taggrp_cfg_ref(config);

    uint32_t req_id =
        neu_plugin_send_write_cmd(plugin, dest_node_id, c, data_var);

    VECTOR_FOR_EACH(&configs, iter)
    {
        neu_taggrp_config_t *cur_grp_config;
        cur_grp_config = *(neu_taggrp_config_t **) iterator_get(&iter);
        neu_taggrp_cfg_free(cur_grp_config);
    }
    vector_uninit(&configs);
    return req_id;
}

void message_handle_write(neu_plugin_t *              plugin,
                          struct neu_parse_write_req *write_req)
{
    log_info("WRITE uuid:%s, group:%s", write_req->uuid, write_req->group);

    neu_variable_t *head = NULL;
    head                 = neu_variable_create();
    neu_variable_set_error(head, 0);
    for (int i = 0; i < write_req->n_tag; i++) {
        // enum neu_json_type   type  = write_req->tags[i].t;
        union neu_json_value value = write_req->tags[i].value;
        neu_variable_t *     v     = neu_variable_create();

        if (0 == i) {
            neu_variable_set_uword(v, value.val_int);
        }

        if (1 == i) {
            neu_variable_set_uword(v, value.val_int);
        }
        if (2 == i) {
            neu_variable_set_byte(v, value.val_int);
        }
        if (3 == i) {
            neu_variable_set_byte(v, value.val_int);
        }
        if (4 == i) {
            neu_variable_set_byte(v, value.val_int);
        }

        neu_variable_add_item(head, v);
    }

    plugin_adapter_write_command(plugin, head->next);
    neu_variable_destroy(head);
}

void message_handle_get_tag_list(neu_plugin_t *                 plugin,
                                 struct neu_parse_get_tags_req *req)
{
    log_info("Get tag list uuid:%s", req->uuid);

    vector_t         nodes = neu_system_get_nodes(plugin, NEU_NODE_TYPE_DRIVER);
    neu_node_info_t *node_info;
    node_info =
        (neu_node_info_t *) vector_get(&nodes, 1); // modbus nodeid uint32_t
    neu_node_id_t dest_node_id = node_info->node_id;

    neu_datatag_table_t *table =
        neu_system_get_datatags_table(plugin, dest_node_id);
    if (NULL == table) {
        return;
    }

    char *                        result = NULL;
    struct neu_parse_get_tags_res res    = {
        .function = NEU_PARSE_OP_GET_TAGS,
        .uuid     = (char *) "554f5fd8-f437-11eb-975c-7704b9e17821",
        .error    = 0,
        .n_tag    = 5,
        .tags     = NULL
    };

    res.tags = (struct neu_parse_get_tags_res_tag *) calloc(
        5, sizeof(struct neu_parse_get_tags_res_tag));

    int i;
    for (i = 0; i < 5; i++) {
        neu_datatag_t *ret = NULL;

        ret = neu_datatag_tbl_get(table, ids[i]);
        if (NULL == ret) {
            continue;
        }

        res.tags[i].name    = strdup(ret->addr_str);
        res.tags[i].type    = 0;
        res.tags[i].decimal = 1;
        res.tags[i].address = strdup(ret->addr_str);
        res.tags[i].flag    = 0;
    }

    int rc = neu_parse_encode(&res, &result);
    if (0 == rc) {
        message_handle_send(result, strlen(result));
    }

    for (i = 0; i < 5; i++) {
        free(res.tags[i].name);
        free(res.tags[i].address);
    }
    free(res.tags);
    free(result);
}

void message_handle_add_tag(neu_plugin_t *                 plugin,
                            struct neu_parse_add_tags_req *req)
{
    UNUSED(req);
    vector_t         nodes = neu_system_get_nodes(plugin, NEU_NODE_TYPE_DRIVER);
    neu_node_info_t *node_info;
    node_info =
        (neu_node_info_t *) vector_get(&nodes, 1); // modbus nodeid uint32_t
    neu_node_id_t dest_node_id = node_info->node_id;

    neu_datatag_table_t *table =
        neu_system_get_datatags_table(plugin, dest_node_id);
    if (NULL == table) {
        return;
    }

    neu_datatag_id_t id;
    neu_address_t    datatag_address;
    memset(&datatag_address, 0, sizeof(neu_address_t));
    char *str_addr = (char *) tags[3];
    id             = 3;

    neu_datatag_t *datatag0 = malloc(sizeof(neu_datatag_t));
    datatag0->id            = id;
    datatag0->type          = NEU_ATTRIBUTETYPE_READ;
    datatag0->dataType      = NEU_DATATYPE_BYTE;
    datatag0->address       = datatag_address;
    datatag0->addr_str      = str_addr;
    ids[3]                  = neu_datatag_tbl_add(table, datatag0);

    str_addr = (char *) tags[4];
    id       = 4;

    neu_datatag_t *datatag1 = malloc(sizeof(neu_datatag_t));
    datatag1->id            = id;
    datatag1->type          = NEU_ATTRIBUTETYPE_READ;
    datatag1->dataType      = NEU_DATATYPE_BYTE;
    datatag1->address       = datatag_address;
    datatag1->addr_str      = str_addr;
    ids[4]                  = neu_datatag_tbl_add(table, datatag1);

    char *                        result = NULL;
    struct neu_parse_add_tags_res res    = {
        .function = NEU_PARSE_OP_ADD_TAGS,
        .uuid     = (char *) "554f5fd8-f437-11eb-975c-7704b9e17821",
        .error    = 0,
    };

    int rc = neu_parse_encode(&res, &result);
    if (0 == rc) {
        message_handle_send(result, strlen(result));
    }

    free(result);
}

void message_handle_read_tag_group_list(
    neu_plugin_t *plugin, struct neu_paser_read_tag_group_list_req *req)
{
    UNUSED(req);
    vector_t nodes = neu_system_get_nodes(plugin, NEU_NODE_TYPE_DRIVER);

    char *                                   json_str = NULL;
    struct neu_paser_read_tag_group_list_res res      = {
        .function = NEU_PARSE_OP_READ_TAG_GROUP_LIST,
        .uuid     = (char *) "1d892fe8-f37e-11eb-9a34-932aa06a28f3",
        .error    = 0,
        .n_group  = 3,
        .names    = NULL
    };

    neu_node_info_t *node_info;
    int              count = 0;
    VECTOR_FOR_EACH(&nodes, iter)
    {
        node_info = (neu_node_info_t *) iterator_get(&iter);
        vector_t configs =
            neu_system_get_group_configs(plugin, node_info->node_id);
        neu_taggrp_config_t *config;
        VECTOR_FOR_EACH(&configs, iter)
        {
            config = *(neu_taggrp_config_t **) iterator_get(&iter);
            if (NULL == res.names) {
                res.names =
                    (struct neu_paser_read_tag_group_list_name *) malloc(
                        sizeof(struct neu_paser_read_tag_group_list_name));
            }
            if (NULL != res.names) {
                res.names = realloc(
                    res.names,
                    (count + 1) *
                        sizeof(struct neu_paser_read_tag_group_list_name));
            }

            res.names[count].name =
                strdup((char *) neu_taggrp_cfg_get_name(config));
            count++;
        }
        res.n_group = count;

        VECTOR_FOR_EACH(&configs, iter)
        {
            neu_taggrp_config_t *cur_grp_config;
            cur_grp_config = *(neu_taggrp_config_t **) iterator_get(&iter);
            neu_taggrp_cfg_free(cur_grp_config);
        }
        vector_uninit(&configs);
    }

    int rc = neu_parse_encode(&res, &json_str);
    if (0 == rc) {
        message_handle_send(json_str, strlen(json_str));
    }

    int i;
    for (i = 0; i < count; i++) {
        free(res.names[i].name);
    }
    free(res.names);
    free(json_str);
    vector_uninit(&nodes);
}
