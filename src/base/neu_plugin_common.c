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
#include <string.h>

#include "neu_plugin.h"

static uint32_t plugin_get_event_id(neu_plugin_common_t *common);

#define PLUGIN_CALL_CMD(plugin, type, req_buff, resp_struct, func)            \
    {                                                                         \
        neu_request_t        cmd    = { 0 };                                  \
        neu_response_t *     result = NULL;                                   \
        neu_plugin_common_t *plugin_common =                                  \
            neu_plugin_to_plugin_common(plugin);                              \
        cmd.req_type = (type);                                                \
        cmd.req_id   = plugin_get_event_id(plugin_common);                    \
        cmd.buf      = (void *) &(req_buff);                                  \
        cmd.buf_len  = sizeof(req_buff);                                      \
        if (plugin_common->adapter_callbacks->command(                        \
                plugin_common->adapter, &(cmd), &(result)) == 0) {            \
            assert((result)->buf_len == sizeof(resp_struct));                 \
            resp_struct *resp = NULL;                                         \
            resp              = (resp_struct *) (result)->buf;                \
            { func };                                                         \
            if (strncmp("intptr_t", #resp_struct, strlen("intptr_t")) != 0) { \
                free(resp);                                                   \
            }                                                                 \
            free(result);                                                     \
        }                                                                     \
    }

static uint32_t plugin_get_event_id(neu_plugin_common_t *common)
{
    common->event_id += 3;
    return common->event_id;
}

void neu_plugin_common_init(neu_plugin_common_t *common)
{
    common->magic = 0xffffffff;
}

int neu_plugin_common_check(neu_plugin_t *plugin)
{
    return neu_plugin_to_plugin_common(plugin)->magic == 0xffffffff ? 0 : -1;
}

neu_datatag_table_t *neu_plugin_get_datatags_table(neu_plugin_t *plugin,
                                                   neu_node_id_t ndoe_id)
{
    neu_datatag_table_t *  tag_table    = NULL;
    neu_cmd_get_datatags_t get_tags_cmd = { .node_id = ndoe_id };

    PLUGIN_CALL_CMD(plugin, NEU_REQRESP_GET_DATATAGS, get_tags_cmd,
                    neu_reqresp_datatags_t, { tag_table = resp->datatag_tbl; });
    return tag_table;
}

intptr_t neu_plugin_add_node(neu_plugin_t *plugin, neu_node_type_e node_type,
                             const char *adapter_name, const char *plugin_name)
{
    intptr_t           errorcode    = -1;
    neu_cmd_add_node_t node_add_cmd = { 0 };

    node_add_cmd.node_type    = node_type;
    node_add_cmd.adapter_name = adapter_name;
    node_add_cmd.plugin_name  = plugin_name;

    PLUGIN_CALL_CMD(plugin, NEU_REQRESP_ADD_NODE, node_add_cmd, intptr_t,
                    { errorcode = (intptr_t) resp; })

    return errorcode;
}

vector_t *neu_plugin_get_nodes(neu_plugin_t *plugin, neu_node_type_e node_type)
{
    vector_t *          nodes         = NULL;
    neu_cmd_get_nodes_t get_nodes_cmd = { 0 };

    get_nodes_cmd.node_type = NEU_NODE_TYPE_DRIVER;

    PLUGIN_CALL_CMD(plugin, NEU_REQRESP_GET_NODES, get_nodes_cmd,
                    neu_reqresp_nodes_t, { nodes = &resp->nodes; })

    return nodes;
}