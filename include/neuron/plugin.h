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

#ifndef NEURON_PLUGIN_H
#define NEURON_PLUGIN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "utils/utarray.h"
#include "utils/zlog.h"

#include "adapter.h"
#include "tag_group_config.h"
#include "type.h"

#define NEURON_PLUGIN_VER_1_0 100
#define NEURON_PLUGIN_VER_2_0 200

typedef struct neu_plugin_common {
    uint32_t                   magic;
    neu_plugin_link_state_e    link_state;
    neu_adapter_t *            adapter;
    const adapter_callbacks_t *adapter_callbacks;
    uint32_t                   event_id;
    zlog_category_t *          default_category;
    zlog_category_t *          protocol_category;
} neu_plugin_common_t;

typedef struct neu_plugin neu_plugin_t;

typedef struct neu_plugin_group neu_plugin_group_t;
typedef void (*neu_plugin_group_free)(neu_plugin_group_t *pgp);
struct neu_plugin_group {
    char *    group_name;
    UT_array *tags;

    void *                user_data;
    neu_plugin_group_free group_free;
};

typedef struct neu_plugin_intf_funs {
    neu_plugin_t *(*open)(neu_adapter_t *            adapter,
                          const adapter_callbacks_t *callbacks);
    int (*close)(neu_plugin_t *plugin);
    int (*init)(neu_plugin_t *plugin);
    int (*uninit)(neu_plugin_t *plugin);
    int (*start)(neu_plugin_t *plugin);
    int (*stop)(neu_plugin_t *plugin);
    int (*config)(neu_plugin_t *plugin, neu_config_t *configs);

    int (*request)(neu_plugin_t *plugin, neu_request_t *req);
    int (*event_reply)(neu_plugin_t *plugin, neu_event_reply_t *reply);
    int (*validate_tag)(neu_plugin_t *plugin, neu_datatag_t *tag);

    union {
        struct {
            int (*validate_tag)(neu_plugin_t *plugin, neu_datatag_t *tag);
            int (*group_timer)(neu_plugin_t *plugin, neu_plugin_group_t *group);
            int (*write_tag)(neu_plugin_t *plugin, void *req,
                             neu_datatag_t *tag, neu_value_u value);
        } driver;
    };

} neu_plugin_intf_funs_t;

typedef struct neu_plugin_module {
    const uint32_t                version;
    const char *                  module_name;
    const char *                  module_descr;
    const neu_plugin_intf_funs_t *intf_funs;
    neu_node_type_e               type;
    plugin_kind_e                 kind;
} neu_plugin_module_t;

inline static neu_plugin_common_t *
neu_plugin_to_plugin_common(neu_plugin_t *plugin)
{
    return (neu_plugin_common_t *) plugin;
}

void neu_plugin_common_init(neu_plugin_common_t *common);
bool neu_plugin_common_check(neu_plugin_t *plugin);

uint32_t neu_plugin_get_event_id(neu_plugin_t *plugin);

neu_datatag_table_t *neu_system_get_datatags_table(neu_plugin_t *plugin,
                                                   neu_node_id_t ndoe_id);
intptr_t neu_system_add_node(neu_plugin_t *plugin, neu_node_type_e node_type,
                             const char *adapter_name, const char *plugin_name);
intptr_t neu_system_del_node(neu_plugin_t *plugin, neu_node_id_t node_id);
// uninit vector
vector_t neu_system_get_nodes(neu_plugin_t *plugin, neu_node_type_e node_type);
intptr_t neu_system_add_group_config(neu_plugin_t *       plugin,
                                     neu_node_id_t        node_id,
                                     neu_taggrp_config_t *grp_config);
intptr_t neu_system_del_group_config(neu_plugin_t *plugin,
                                     neu_node_id_t node_id, char *config_name);
intptr_t neu_system_update_group_config(neu_plugin_t *       plugin,
                                        neu_node_id_t        node_id,
                                        neu_taggrp_config_t *grp_config);
neu_taggrp_config_t *neu_system_find_group_config(neu_plugin_t *plugin,
                                                  neu_node_id_t node_id,
                                                  const char *  name);
neu_taggrp_config_t *neu_system_ref_group_config(neu_plugin_t *plugin,
                                                 neu_node_id_t node_id,
                                                 const char *  name);
// uninit vector
vector_t      neu_system_get_group_configs(neu_plugin_t *plugin,
                                           neu_node_id_t node_id);
uint32_t      neu_plugin_send_unsubscribe_cmd(neu_plugin_t *       plugin,
                                              neu_node_id_t        src_node_id,
                                              neu_node_id_t        dst_node_id,
                                              neu_taggrp_config_t *grp_config);
uint32_t      neu_plugin_send_subscribe_cmd(neu_plugin_t *       plugin,
                                            neu_node_id_t        src_node_id,
                                            neu_node_id_t        dst_node_id,
                                            neu_taggrp_config_t *grp_config);
void          neu_plugin_send_read_cmd(neu_plugin_t *plugin, uint32_t event_id,
                                       neu_node_id_t        node_id,
                                       neu_taggrp_config_t *grp_configs);
void          neu_plugin_send_write_cmd(neu_plugin_t *plugin, uint32_t event_id,
                                        neu_node_id_t        node_id,
                                        neu_taggrp_config_t *grp_configs,
                                        neu_data_val_t *     data);
void          neu_plugin_response_trans_data(neu_plugin_t *       plugin,
                                             neu_taggrp_config_t *grp_config,
                                             neu_data_val_t *data, uint32_t event_id);
neu_node_id_t neu_plugin_self_node_id(neu_plugin_t *plugin);
const char *  neu_plugin_self_node_name(neu_plugin_t *plugin);
neu_node_id_t neu_plugin_get_node_id_by_node_name(neu_plugin_t *plugin,
                                                  const char *  node_name);

intptr_t neu_system_add_plugin(neu_plugin_t *plugin,
                               const char *  plugin_lib_name);

int neu_plugin_notify_event_add_tags(neu_plugin_t *plugin, uint32_t event_id,
                                     neu_node_id_t node_id,
                                     const char *  grp_config_name);
int neu_plugin_notify_event_del_tags(neu_plugin_t *plugin, uint32_t event_id,
                                     neu_node_id_t node_id,
                                     const char *  grp_config_name);
int neu_plugin_notify_event_update_tags(neu_plugin_t *plugin, uint32_t event_id,
                                        neu_node_id_t node_id,
                                        const char *  grp_config_name);
int neu_plugin_notify_event_update_license(neu_plugin_t *plugin,
                                           uint32_t      event_id);

intptr_t neu_system_del_plugin(neu_plugin_t *plugin, plugin_id_t plugin_id);
intptr_t neu_system_update_plugin(neu_plugin_t *plugin, plugin_kind_e kind,
                                  neu_node_type_e node_type,
                                  const char *    plugin_name,
                                  const char *    plugin_lib_name);
// uninit vector
vector_t neu_system_get_plugin(neu_plugin_t *plugin);

int neu_plugin_tag_count_by_attribute(neu_taggrp_config_t *grp_config,
                                      neu_datatag_table_t *tag_table,
                                      neu_attribute_e      attribute);

intptr_t neu_plugin_set_node_setting(neu_plugin_t *plugin,
                                     neu_node_id_t node_id,
                                     const char *  setting);
int32_t neu_plugin_get_node_setting(neu_plugin_t *plugin, neu_node_id_t node_id,
                                    char **setting);
int32_t neu_plugin_get_node_state(neu_plugin_t *plugin, neu_node_id_t node_id,
                                  neu_plugin_state_t *state);
intptr_t  neu_plugin_node_ctl(neu_plugin_t *plugin, neu_node_id_t node_id,
                              neu_adapter_ctl_e ctl);
vector_t *neu_system_get_sub_group_configs(neu_plugin_t *plugin,
                                           neu_node_id_t node_id);
intptr_t  neu_plugin_validate_tag(neu_plugin_t *plugin, neu_node_id_t node_id,
                                  neu_datatag_t *tag);
#ifdef __cplusplus
}
#endif

#endif
