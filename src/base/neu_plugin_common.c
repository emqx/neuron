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

#define NEU_PLUGIN_MAGIC_NUMBER 0x43474d50 // a string "PMGC"

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

#define PLUGIN_SEND_CMD(plugin, type, req_buff, event_id)                 \
    {                                                                     \
        neu_request_t        cmd = { 0 };                                 \
        neu_plugin_common_t *plugin_common =                              \
            neu_plugin_to_plugin_common(plugin);                          \
        cmd.req_type = (type);                                            \
        cmd.req_id   = plugin_get_event_id(plugin_common);                \
        cmd.buf      = (void *) &(req_buff);                              \
        cmd.buf_len  = sizeof(req_buff);                                  \
        (event_id)   = cmd.req_id;                                        \
        plugin_common->adapter_callbacks->command(plugin_common->adapter, \
                                                  &(cmd), NULL);          \
    }

#define PLUGIN_SEND_RESPONSE(plugin, type, id, resp_buff)                  \
    {                                                                      \
        neu_response_t       resp = { 0 };                                 \
        neu_plugin_common_t *plugin_common =                               \
            neu_plugin_to_plugin_common(plugin);                           \
        resp.resp_type = (type);                                           \
        resp.req_id    = (id);                                             \
        resp.buf       = (void *) &(resp_buff);                            \
        resp.buf_len   = sizeof(resp_buff);                                \
        plugin_common->adapter_callbacks->response(plugin_common->adapter, \
                                                   &resp);                 \
    }

#define PLUGIN_SEND_EVENT(plugin, event_type, event_buff)                      \
    {                                                                          \
        neu_event_notify_t   event = { 0 };                                    \
        neu_plugin_common_t *plugin_common =                                   \
            neu_plugin_to_plugin_common(plugin);                               \
        event.event_id = plugin_get_event_id(plugin_common);                   \
        event.type     = (event_type);                                         \
        event.buf      = (void *) &(event_buff);                               \
        event.buf_len  = sizeof(event_buff);                                   \
        plugin_common->adapter_callbacks->event_notify(plugin_common->adapter, \
                                                       &event);                \
    }

static uint32_t plugin_get_event_id(neu_plugin_common_t *common)
{
    common->event_id += 2; // for avoid check event_id == 0
    return common->event_id;
}

void neu_plugin_common_init(neu_plugin_common_t *common)
{
    common->event_id = 1;
    common->magic    = NEU_PLUGIN_MAGIC_NUMBER;
}

bool neu_plugin_common_check(neu_plugin_t *plugin)
{
    return neu_plugin_to_plugin_common(plugin)->magic == NEU_PLUGIN_MAGIC_NUMBER
        ? true
        : false;
}

neu_datatag_table_t *neu_system_get_datatags_table(neu_plugin_t *plugin,
                                                   neu_node_id_t ndoe_id)
{
    neu_datatag_table_t *  tag_table    = NULL;
    neu_cmd_get_datatags_t get_tags_cmd = { .node_id = ndoe_id };

    PLUGIN_CALL_CMD(plugin, NEU_REQRESP_GET_DATATAGS, get_tags_cmd,
                    neu_reqresp_datatags_t, { tag_table = resp->datatag_tbl; });
    return tag_table;
}

intptr_t neu_system_add_node(neu_plugin_t *plugin, neu_node_type_e node_type,
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

intptr_t neu_system_del_node(neu_plugin_t *plugin, neu_node_id_t node_id)
{
    intptr_t           errorcode    = -1;
    neu_cmd_del_node_t node_del_cmd = { 0 };

    node_del_cmd.node_id = node_id;

    PLUGIN_CALL_CMD(plugin, NEU_REQRESP_DEL_NODE, node_del_cmd, intptr_t,
                    { errorcode = (intptr_t) resp; })

    return errorcode;
}

intptr_t neu_system_update_node(neu_plugin_t *plugin, neu_node_type_e node_type,
                                const char *adapter_name,
                                const char *plugin_name)
{
    intptr_t              errorcode       = -1;
    neu_cmd_update_node_t node_update_cmd = { 0 };

    node_update_cmd.adapter_name = adapter_name;
    node_update_cmd.plugin_name  = plugin_name;
    node_update_cmd.node_type    = node_type;

    PLUGIN_CALL_CMD(plugin, NEU_REQRESP_UPDATE_NODE, node_update_cmd, intptr_t,
                    { errorcode = (intptr_t) resp; })

    return errorcode;
}

vector_t neu_system_get_nodes(neu_plugin_t *plugin, neu_node_type_e node_type)
{
    vector_t            nodes         = { 0 };
    neu_cmd_get_nodes_t get_nodes_cmd = { 0 };

    get_nodes_cmd.node_type = node_type;

    PLUGIN_CALL_CMD(plugin, NEU_REQRESP_GET_NODES, get_nodes_cmd,
                    neu_reqresp_nodes_t, { nodes = resp->nodes; })

    return nodes;
}

intptr_t neu_system_add_group_config(neu_plugin_t *       plugin,
                                     neu_node_id_t        node_id,
                                     neu_taggrp_config_t *grp_config)
{
    intptr_t                 errorcode          = -1;
    neu_cmd_add_grp_config_t grp_config_add_cmd = { 0 };

    grp_config_add_cmd.grp_config = grp_config;
    grp_config_add_cmd.node_id    = node_id;

    PLUGIN_CALL_CMD(plugin, NEU_REQRESP_ADD_GRP_CONFIG, grp_config_add_cmd,
                    intptr_t, { errorcode = (intptr_t) resp; })

    return errorcode;
}

intptr_t neu_system_del_group_config(neu_plugin_t *plugin,
                                     neu_node_id_t node_id, char *config_name)
{
    intptr_t                 errorcode          = -1;
    neu_cmd_del_grp_config_t grp_config_del_cmd = { 0 };

    grp_config_del_cmd.node_id     = node_id;
    grp_config_del_cmd.config_name = config_name;

    PLUGIN_CALL_CMD(plugin, NEU_REQRESP_DEL_GRP_CONFIG, grp_config_del_cmd,
                    intptr_t, { errorcode = (intptr_t) resp; })

    return errorcode;
}

intptr_t neu_system_update_group_config(neu_plugin_t *       plugin,
                                        neu_node_id_t        node_id,
                                        neu_taggrp_config_t *grp_config)
{
    intptr_t                    errorcode             = -1;
    neu_cmd_update_grp_config_t grp_config_update_cmd = { 0 };

    grp_config_update_cmd.node_id    = node_id;
    grp_config_update_cmd.grp_config = grp_config;

    PLUGIN_CALL_CMD(plugin, NEU_REQRESP_UPDATE_GRP_CONFIG,
                    grp_config_update_cmd, intptr_t,
                    { errorcode = (intptr_t) resp; })

    return errorcode;
}

vector_t neu_system_get_group_configs(neu_plugin_t *plugin,
                                      neu_node_id_t node_id)
{
    vector_t                  group_configs = { 0 };
    neu_cmd_get_grp_configs_t get_grps_cmd  = { 0 };

    get_grps_cmd.node_id = node_id;

    PLUGIN_CALL_CMD(plugin, NEU_REQRESP_GET_GRP_CONFIGS, get_grps_cmd,
                    neu_reqresp_grp_configs_t,
                    { group_configs = resp->grp_configs; })

    return group_configs;
}

uint32_t neu_plugin_send_read_cmd(neu_plugin_t *plugin, neu_node_id_t node_id,
                                  neu_taggrp_config_t *grp_configs)
{
    uint32_t           event_id = 0;
    neu_reqresp_read_t read_req = { 0 };

    read_req.grp_config  = grp_configs;
    read_req.dst_node_id = node_id;

    PLUGIN_SEND_CMD(plugin, NEU_REQRESP_READ_DATA, read_req, event_id)

    return event_id;
}

uint32_t neu_plugin_send_write_cmd(neu_plugin_t *plugin, neu_node_id_t node_id,
                                   neu_taggrp_config_t *grp_configs,
                                   neu_data_val_t *     data)
{
    uint32_t            event_id  = 0;
    neu_reqresp_write_t write_req = { 0 };

    write_req.grp_config  = grp_configs;
    write_req.dst_node_id = node_id;
    write_req.data_val    = data;

    PLUGIN_SEND_CMD(plugin, NEU_REQRESP_WRITE_DATA, write_req, event_id)

    return event_id;
}

void neu_plugin_response_trans_data(neu_plugin_t *       plugin,
                                    neu_taggrp_config_t *grp_config,
                                    neu_data_val_t *data, uint32_t event_id)
{
    neu_reqresp_data_t data_resp = { 0 };

    data_resp.grp_config = grp_config;
    data_resp.data_val   = data;

    PLUGIN_SEND_RESPONSE(plugin, NEU_REQRESP_TRANS_DATA, event_id, data_resp)
}

neu_node_id_t neu_plugin_self_node_id(neu_plugin_t *plugin)
{
    neu_cmd_self_node_id_t self_node_id;
    neu_node_id_t          node_id = { 0 };

    PLUGIN_CALL_CMD(plugin, NEU_REQRESP_SELF_NODE_ID, self_node_id,
                    neu_reqresp_node_id_t, { node_id = resp->node_id; })

    return node_id;
}

intptr_t neu_system_add_plugin(neu_plugin_t *plugin, plugin_kind_e kind,
                               neu_node_type_e node_type,
                               const char *    plugin_name,
                               const char *    plugin_lib_name)
{
    intptr_t                 errorcode      = -1;
    neu_cmd_add_plugin_lib_t add_plugin_cmd = { 0 };

    add_plugin_cmd.plugin_kind     = kind;
    add_plugin_cmd.node_type       = node_type;
    add_plugin_cmd.plugin_name     = plugin_name;
    add_plugin_cmd.plugin_lib_name = plugin_lib_name;

    PLUGIN_CALL_CMD(plugin, NEU_REQRESP_ADD_PLUGIN_LIB, add_plugin_cmd,
                    intptr_t, { errorcode = (intptr_t) resp; })

    return errorcode;
}

intptr_t neu_system_del_plugin(neu_plugin_t *plugin, plugin_id_t plugin_id)
{
    intptr_t                 errorcode      = -1;
    neu_cmd_del_plugin_lib_t del_plugin_cmd = { 0 };

    del_plugin_cmd.plugin_id = plugin_id;

    PLUGIN_CALL_CMD(plugin, NEU_REQRESP_DEL_PLUGIN_LIB, del_plugin_cmd,
                    intptr_t, { errorcode = (intptr_t) resp; })

    return errorcode;
}

intptr_t neu_system_update_plugin(neu_plugin_t *plugin, plugin_kind_e kind,
                                  neu_node_type_e node_type,
                                  const char *    plugin_name,
                                  const char *    plugin_lib_name)
{
    intptr_t                    errorcode         = -1;
    neu_cmd_update_plugin_lib_t update_plugin_cmd = { 0 };

    update_plugin_cmd.plugin_kind     = kind;
    update_plugin_cmd.node_type       = node_type;
    update_plugin_cmd.plugin_name     = plugin_name;
    update_plugin_cmd.plugin_lib_name = plugin_lib_name;

    PLUGIN_CALL_CMD(plugin, NEU_REQRESP_UPDATE_PLUGIN_LIB, update_plugin_cmd,
                    intptr_t, { errorcode = (intptr_t) resp; })

    return errorcode;
}
// uninit vector
vector_t neu_system_get_plugin(neu_plugin_t *plugin)
{
    vector_t                  plugin_libs    = { 0 };
    neu_cmd_get_plugin_libs_t get_plugin_cmd = { 0 };

    PLUGIN_CALL_CMD(plugin, NEU_REQRESP_GET_PLUGIN_LIBS, get_plugin_cmd,
                    neu_reqresp_plugin_libs_t,
                    { plugin_libs = resp->plugin_libs; })

    return plugin_libs;
}

neu_taggrp_config_t *neu_system_find_group_config(neu_plugin_t *plugin,
                                                  neu_node_id_t node_id,
                                                  const char *  name)
{
    vector_t grp_configs = neu_system_get_group_configs(plugin, node_id);
    neu_taggrp_config_t *find_config = NULL;

    VECTOR_FOR_EACH(&grp_configs, iter)
    {
        neu_taggrp_config_t *config =
            *(neu_taggrp_config_t **) iterator_get(&iter);
        if (strncmp(neu_taggrp_cfg_get_name(config), name, strlen(name)) == 0) {
            find_config = config;
            break;
        }
    }

    vector_uninit(&grp_configs);

    return find_config;
}
