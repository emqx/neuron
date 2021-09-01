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
static neu_plugin_common_t *g_common;
static paho_client_t *      g_paho;
static uint32_t *           g_event_id;

static const char tags[5][20] = { "1!400001", "1!400003", "1!00001", "1!00002",
                                  "1!00003" };
static datatag_id_t ids[5];

void message_handle_set_plugin_common(neu_plugin_common_t *common)
{
    g_common = common;
}

void message_handle_set_paho_client(paho_client_t *paho)
{
    g_paho = paho;
}

void message_handle_set_event_id(uint32_t *event_id)
{
    g_event_id = event_id;
}

static int message_handle_send(char *buff, const size_t len)
{
    client_error error = paho_client_publish2(g_paho, "neuronlite/response", 0,
                                              (unsigned char *) buff, len);
    return error;
}

static uint32_t plugin_get_event_id()
{
    uint32_t req_id;

    (*g_event_id)++;
    if (*g_event_id == 0) {
        *g_event_id = 1;
    }

    req_id = *g_event_id;
    return req_id;
}

static uint32_t plugin_get_nodes()
{
    int                        rv;
    const adapter_callbacks_t *adapter_callbacks;
    adapter_callbacks = g_common->adapter_callbacks;

    /* example of get nodes */
    neu_request_t        sync_cmd;
    neu_cmd_get_nodes_t  get_nodes_cmd;
    neu_response_t *     result = NULL;
    neu_reqresp_nodes_t *resp_nodes;

    get_nodes_cmd.node_type = NEU_NODE_TYPE_DRIVER;
    sync_cmd.req_type       = NEU_REQRESP_GET_NODES;
    sync_cmd.req_id         = plugin_get_event_id();
    sync_cmd.buf            = (void *) &get_nodes_cmd;
    sync_cmd.buf_len        = sizeof(neu_cmd_get_nodes_t);
    log_info("Get list of driver nodes");
    rv = adapter_callbacks->command(g_common->adapter, &sync_cmd, &result);
    if (rv < 0) {
        return 0;
    }

    neu_node_info_t *node_info;
    neu_node_id_t    dst_node_id;
    assert(result->buf_len == sizeof(neu_reqresp_nodes_t));
    resp_nodes = (neu_reqresp_nodes_t *) result->buf;
    node_info  = (neu_node_info_t *) vector_get(&resp_nodes->nodes, 1);

    dst_node_id = node_info->node_id;
    log_info("The first node(%d) of driver nodes list", dst_node_id);
    vector_uninit(&resp_nodes->nodes);
    free(resp_nodes);
    free(result);

    return dst_node_id;
}

static neu_taggrp_config_t *plugin_get_group(uint32_t dest_node_id)
{
    int                        rv;
    const adapter_callbacks_t *adapter_callbacks;
    adapter_callbacks = g_common->adapter_callbacks;

    /* example of get group configs */
    neu_request_t   sync_cmd;
    neu_response_t *result = NULL;

    neu_cmd_get_grp_configs_t  get_grps_cmd;
    neu_reqresp_grp_configs_t *resp_grps;
    result = NULL;

    get_grps_cmd.node_id = dest_node_id;
    sync_cmd.req_type    = NEU_REQRESP_GET_GRP_CONFIGS;
    sync_cmd.req_id      = plugin_get_event_id();
    sync_cmd.buf         = (void *) &get_grps_cmd;
    sync_cmd.buf_len     = sizeof(neu_cmd_get_grp_configs_t);
    log_info("Get list of group configs");
    rv = adapter_callbacks->command(g_common->adapter, &sync_cmd, &result);
    if (rv < 0) {
        return NULL;
    }

    neu_taggrp_config_t *grp_config;
    assert(result->buf_len == sizeof(neu_reqresp_grp_configs_t));
    resp_grps = (neu_reqresp_grp_configs_t *) result->buf;
    grp_config =
        *(neu_taggrp_config_t **) vector_get(&resp_grps->grp_configs, 0);

    neu_taggrp_cfg_ref(grp_config);

    log_info("The first group config(%s) of node(%d)",
             neu_taggrp_cfg_get_name(grp_config), dest_node_id);
    VECTOR_FOR_EACH(&resp_grps->grp_configs, iter)
    {
        neu_taggrp_config_t *cur_grp_config;
        cur_grp_config = *(neu_taggrp_config_t **) iterator_get(&iter);
        neu_taggrp_cfg_free(cur_grp_config);
    }
    vector_uninit(&resp_grps->grp_configs);
    free(resp_grps);
    free(result);

    return grp_config;
}

static neu_datatag_table_t *plugin_get_data_tag(uint32_t dest_node_id)
{
    int                        rv;
    const adapter_callbacks_t *adapter_callbacks;
    adapter_callbacks = g_common->adapter_callbacks;

    /* get data tag*/
    neu_request_t   sync_cmd;
    neu_response_t *result = NULL;

    neu_cmd_get_datatags_t  get_tags_cmd;
    neu_reqresp_datatags_t *resp_datatags;

    result = NULL;

    get_tags_cmd.node_id = dest_node_id;
    sync_cmd.req_type    = NEU_REQRESP_GET_DATATAGS;
    sync_cmd.req_id      = plugin_get_event_id();
    sync_cmd.buf         = (void *) &get_tags_cmd;
    sync_cmd.buf_len     = sizeof(neu_cmd_get_datatags_t);

    rv = adapter_callbacks->command(g_common->adapter, &sync_cmd, &result);
    if (rv < 0) {
        return NULL;
    }

    assert(result->buf_len == sizeof(neu_reqresp_datatags_t));
    resp_datatags                    = (neu_reqresp_datatags_t *) result->buf;
    neu_datatag_table_t *datatag_tbl = resp_datatags->datatag_tbl;

    free(resp_datatags);
    free(result);
    return datatag_tbl;
}

void message_handle_init_tags()
{
    uint32_t             dest_node_id = plugin_get_nodes();
    neu_datatag_table_t *data_tag     = plugin_get_data_tag(dest_node_id);
    if (NULL == data_tag) {
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
    datatag0->str_addr      = str_addr;
    ids[0]                  = neu_datatag_tbl_add(data_tag, datatag0);

    str_addr                = (char *) tags[1];
    id                      = 1;
    neu_datatag_t *datatag1 = malloc(sizeof(neu_datatag_t));
    datatag1->id            = id;
    datatag1->type          = NEU_ATTRIBUTETYPE_READ;
    datatag1->dataType      = NEU_DATATYPE_UWORD;
    datatag1->address       = datatag_address;
    datatag1->str_addr      = str_addr;
    ids[1]                  = neu_datatag_tbl_add(data_tag, datatag1);

    str_addr                = (char *) tags[2];
    id                      = 2;
    neu_datatag_t *datatag2 = malloc(sizeof(neu_datatag_t));
    datatag2->id            = id;
    datatag2->type          = NEU_ATTRIBUTETYPE_READ;
    datatag2->dataType      = NEU_DATATYPE_BYTE;
    datatag2->address       = datatag_address;
    datatag2->str_addr      = str_addr;
    ids[2]                  = neu_datatag_tbl_add(data_tag, datatag2);
}

static int plugin_adapter_read_command(const char *group)
{
    uint32_t             dest_node_id = plugin_get_nodes();
    neu_taggrp_config_t *group_config = plugin_get_group(dest_node_id);

    const adapter_callbacks_t *adapter_callbacks;
    adapter_callbacks = g_common->adapter_callbacks;

    neu_request_t      cmd;
    neu_reqresp_read_t read_req;

    read_req.grp_config =
        (neu_taggrp_config_t *) neu_taggrp_cfg_ref(group_config);
    read_req.dst_node_id = dest_node_id;

    cmd.req_type = NEU_REQRESP_READ_DATA;
    cmd.req_id   = plugin_get_event_id();
    cmd.buf      = (void *) &read_req;
    cmd.buf_len  = sizeof(neu_reqresp_read_t);
    log_info("Send a read command, dst_node_id:%d, req id:%d",
             read_req.dst_node_id, cmd.req_id);
    adapter_callbacks->command(g_common->adapter, &cmd, NULL);
    UNUSED(group);
    return cmd.req_id;
}

void message_handle_read(struct neu_parse_read_req *req)
{
    log_info("READ uuid:%s, group:%s", req->uuid, req->group);

    if (0 >= req->n_name) {
        return;
    }
    plugin_adapter_read_command(req->group);
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

static int plugin_adapter_write_command(neu_variable_t *data_var)
{
    uint32_t             dest_node_id = plugin_get_nodes();
    neu_taggrp_config_t *group_config = plugin_get_group(dest_node_id);

    const adapter_callbacks_t *adapter_callbacks;
    adapter_callbacks = g_common->adapter_callbacks;

    neu_request_t       cmd1;
    neu_reqresp_write_t write_req;

    write_req.grp_config =
        (neu_taggrp_config_t *) neu_taggrp_cfg_ref(group_config);
    write_req.dst_node_id = dest_node_id;
    write_req.addr        = 3;
    write_req.data_var    = data_var;
    cmd1.req_type         = NEU_REQRESP_WRITE_DATA;
    cmd1.req_id           = plugin_get_event_id();
    cmd1.buf              = (void *) &write_req;
    cmd1.buf_len          = sizeof(neu_reqresp_write_t);
    log_info("Send a write command");
    adapter_callbacks->command(g_common->adapter, &cmd1, NULL);
    return cmd1.req_id;
}

void message_handle_write_handle(struct neu_parse_write_req *write_req)
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

    plugin_adapter_write_command(head->next);
    neu_variable_destroy(head);
}

void message_handle_get_tag_list_handle(struct neu_parse_get_tags_req *req)
{
    log_info("Get tag list uuid:%s", req->uuid);

    uint32_t             dest_node_id = plugin_get_nodes();
    neu_datatag_table_t *data_tag     = plugin_get_data_tag(dest_node_id);

    if (NULL == data_tag) {
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

        ret = neu_datatag_tbl_get(data_tag, ids[i]);
        if (NULL == ret) {
            continue;
        }

        res.tags[i].name    = strdup(ret->str_addr);
        res.tags[i].type    = 0;
        res.tags[i].decimal = 1;
        res.tags[i].address = strdup(ret->str_addr);
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

void message_handle_add_tag_handle(struct neu_parse_add_tags_req *req)
{
    UNUSED(req);
    uint32_t             dest_node_id = plugin_get_nodes();
    neu_datatag_table_t *data_tag     = plugin_get_data_tag(dest_node_id);
    if (NULL == data_tag) {
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
    datatag0->str_addr      = str_addr;
    ids[3]                  = neu_datatag_tbl_add(data_tag, datatag0);

    str_addr = (char *) tags[4];
    id       = 4;

    neu_datatag_t *datatag1 = malloc(sizeof(neu_datatag_t));
    datatag1->id            = id;
    datatag1->type          = NEU_ATTRIBUTETYPE_READ;
    datatag1->dataType      = NEU_DATATYPE_BYTE;
    datatag1->address       = datatag_address;
    datatag1->str_addr      = str_addr;
    ids[4]                  = neu_datatag_tbl_add(data_tag, datatag1);

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

void message_handle_read_tag_group_list_handle(
    struct neu_paser_read_tag_group_list_req *req)
{
    UNUSED(req);

    int                        rv;
    const adapter_callbacks_t *adapter_callbacks;
    adapter_callbacks = g_common->adapter_callbacks;

    /* get nodes */
    neu_request_t        sync_cmd;
    neu_cmd_get_nodes_t  get_nodes_cmd;
    neu_response_t *     result = NULL;
    neu_reqresp_nodes_t *resp_nodes;

    get_nodes_cmd.node_type = NEU_NODE_TYPE_DRIVER;
    sync_cmd.req_type       = NEU_REQRESP_GET_NODES;
    sync_cmd.req_id         = plugin_get_event_id();
    sync_cmd.buf            = (void *) &get_nodes_cmd;
    sync_cmd.buf_len        = sizeof(neu_cmd_get_nodes_t);
    rv = adapter_callbacks->command(g_common->adapter, &sync_cmd, &result);
    if (rv < 0) {
        return;
    }

    assert(result->buf_len == sizeof(neu_reqresp_nodes_t));
    resp_nodes = (neu_reqresp_nodes_t *) result->buf;

    char *                                   json_str = NULL;
    struct neu_paser_read_tag_group_list_res res      = {
        .function = NEU_PARSE_OP_READ_TAG_GROUP_LIST,
        .uuid     = (char *) "1d892fe8-f37e-11eb-9a34-932aa06a28f3",
        .error    = 0,
        .n_group  = 3,
        .names    = NULL
    };

    int              count = 0;
    neu_node_info_t *n;
    VECTOR_FOR_EACH(&resp_nodes->nodes, iter)
    {
        n = (neu_node_info_t *) iterator_get(&iter);

        neu_request_t              sync_cmd;
        neu_response_t *           result = NULL;
        neu_cmd_get_grp_configs_t  get_grps_cmd;
        neu_reqresp_grp_configs_t *resp_grps;
        result               = NULL;
        get_grps_cmd.node_id = n->node_id;
        sync_cmd.req_type    = NEU_REQRESP_GET_GRP_CONFIGS;
        sync_cmd.req_id      = plugin_get_event_id();
        sync_cmd.buf         = (void *) &get_grps_cmd;
        sync_cmd.buf_len     = sizeof(neu_cmd_get_grp_configs_t);
        rv = adapter_callbacks->command(g_common->adapter, &sync_cmd, &result);
        if (rv < 0) {
            continue;
        }

        assert(result->buf_len == sizeof(neu_reqresp_grp_configs_t));
        resp_grps = (neu_reqresp_grp_configs_t *) result->buf;
        neu_taggrp_config_t *c;
        VECTOR_FOR_EACH(&resp_grps->grp_configs, iter)
        {
            c = *(neu_taggrp_config_t **) iterator_get(&iter);
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

            res.names[count].name = strdup((char *) neu_taggrp_cfg_get_name(c));
            count++;
        }
        res.n_group = count;

        VECTOR_FOR_EACH(&resp_grps->grp_configs, iter)
        {
            neu_taggrp_config_t *cur_grp_config;
            cur_grp_config = *(neu_taggrp_config_t **) iterator_get(&iter);
            neu_taggrp_cfg_free(cur_grp_config);
        }
        vector_uninit(&resp_grps->grp_configs);
        free(resp_grps);
        free(result);
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

    vector_uninit(&resp_nodes->nodes);
    free(resp_nodes);
    free(result);
}
