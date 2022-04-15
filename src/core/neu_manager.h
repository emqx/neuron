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

#ifndef NEURON_MANAGER_H
#define NEURON_MANAGER_H

#include "adapter.h"
#include "adapter/adapter_info.h"

typedef struct neu_manager            neu_manager_t;
typedef struct subscribe_driver_cmd   subscribe_node_cmd_t;
typedef struct unsubscribe_driver_cmd unsubscribe_node_cmd_t;

typedef void (*bind_notify_fun)(nng_pipe p, nng_pipe_ev ev, void *arg);

neu_manager_t *neu_manager_create();
void           neu_manager_stop(neu_manager_t *manager);
void           neu_manager_destroy(neu_manager_t *manager);

const char *neu_manager_get_url(neu_manager_t *manager);
void        neu_manager_wait_ready(neu_manager_t *manager);
void        neu_manager_trigger_running(neu_manager_t *manager);

int neu_manager_add_node(neu_manager_t *manager, neu_cmd_add_node_t *cmd,
                         neu_node_id_t *p_node_id);
int neu_manager_del_node(neu_manager_t *manager, neu_node_id_t node_id);
int neu_manager_get_nodes(neu_manager_t *manager, neu_node_type_e node_type,
                          vector_t *result_nodes);
int neu_manager_get_node_name_by_id(neu_manager_t *manager,
                                    neu_node_id_t node_id, char **name);
int neu_manager_get_node_id_by_name(neu_manager_t *manager, const char *name,
                                    neu_node_id_t *node_id_p);
int neu_manager_subscribe_node(neu_manager_t *       manager,
                               subscribe_node_cmd_t *cmd);
int neu_manager_unsubscribe_node(neu_manager_t *         manager,
                                 unsubscribe_node_cmd_t *cmd);
int neu_manager_get_persist_subscription_infos(neu_manager_t *manager,
                                               neu_node_id_t  node_id,
                                               vector_t **    result);

int neu_manager_add_grp_config(neu_manager_t *           manager,
                               neu_cmd_add_grp_config_t *cmd);
int neu_manager_del_grp_config(neu_manager_t *manager, neu_node_id_t node_id,
                               const char *config_name);
int neu_manager_update_grp_config(neu_manager_t *              manager,
                                  neu_cmd_update_grp_config_t *cmd);
int neu_manager_get_grp_configs(neu_manager_t *manager, neu_node_id_t node_id,
                                vector_t *result_grp_configs);
int neu_manager_get_persist_datatag_infos(neu_manager_t *manager,
                                          neu_node_id_t  node_id,
                                          const char *   grp_config_name,
                                          vector_t **    result);

int neu_manager_add_plugin_lib(neu_manager_t *           manager,
                               neu_cmd_add_plugin_lib_t *cmd,
                               plugin_id_t *             p_plugin_id);
int neu_manager_del_plugin_lib(neu_manager_t *manager, plugin_id_t plugin_id);
int neu_manager_update_plugin_lib(neu_manager_t *              manager,
                                  neu_cmd_update_plugin_lib_t *cmd);
int neu_manager_get_plugin_libs(neu_manager_t *manager,
                                vector_t *     plugin_lib_infos);
int neu_manager_get_persist_plugin_infos(neu_manager_t *manager,
                                         vector_t **    result);

neu_datatag_table_t *neu_manager_get_datatag_tbl(neu_manager_t *manager,
                                                 neu_node_id_t  node_id);
neu_node_id_t        neu_manager_adapter_id_to_node_id(neu_manager_t *  manager,
                                                       neu_adapter_id_t adapter_id);
neu_adapter_id_t     neu_manager_adapter_id_from_node_id(neu_manager_t *manager,
                                                         neu_node_id_t  node_id);
int                  neu_manager_adapter_set_setting(neu_manager_t *manager,
                                                     neu_node_id_t node_id, const char *config);
int                  neu_manager_adapter_get_setting(neu_manager_t *manager,
                                                     neu_node_id_t node_id, char **config);
int neu_manager_adapter_get_state(neu_manager_t *manager, neu_node_id_t node_id,
                                  neu_plugin_state_t *state);
int neu_manager_adapter_ctl(neu_manager_t *manager, neu_node_id_t node_id,
                            neu_adapter_ctl_e ctl);
int neu_manager_get_persist_adapter_infos(neu_manager_t *manager,
                                          vector_t **    result);
int neu_manager_start_adapter(neu_adapter_t *adapter);
int neu_manager_start_adapter_with_id(neu_manager_t *manager,
                                      neu_node_id_t  node_id);
int neu_manager_stop_adapter(neu_adapter_t *adapter);
int neu_manager_init_adapter(neu_manager_t *manager, neu_adapter_t *adapter);
int neu_manager_uninit_adapter(neu_manager_t *manager, neu_adapter_t *adapter);
int neu_manager_adapter_get_sub_grp_configs(neu_manager_t *manager,
                                            neu_node_id_t  node_id,
                                            vector_t **    result_sgc);
int neu_manager_adapter_get_grp_config_ref_by_name(
    neu_manager_t *manager, neu_node_id_t node_id, const char *grp_config_name,
    neu_taggrp_config_t **p_grp_config);

int neu_manager_init_main_adapter(neu_manager_t * manager,
                                  bind_notify_fun bind_adapter,
                                  bind_notify_fun unbind_adapter);
int neu_manager_adapter_validate_tag(neu_manager_t *manager,
                                     neu_node_id_t  node_id,
                                     neu_datatag_t *tags);
#endif
