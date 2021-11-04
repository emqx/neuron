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

#ifndef NEURON_PLUGIN_MQTT_COMMON
#define NEURON_PLUGIN_MQTT_COMMON
#ifdef __cplusplus
extern "C" {
#endif

#include <neuron.h>

#include "json/neu_json_fn.h"
#include "json/neu_json_group_config.h"
#include "json/neu_json_mqtt.h"
#include "json/neu_json_node.h"
#include "json/neu_json_rw.h"
#include "json/neu_json_tag.h"

#define UNUSED(x) (void) (x)

#define GROUP_CONFIGS_UNINIT(configs)                                       \
    {                                                                       \
        VECTOR_FOR_EACH(&configs, iter)                                     \
        {                                                                   \
            neu_taggrp_config_t *cur_grp_config;                            \
            cur_grp_config = *(neu_taggrp_config_t **) iterator_get(&iter); \
            neu_taggrp_cfg_free(cur_grp_config);                            \
        }                                                                   \
        vector_uninit(&configs);                                            \
    }

int common_node_id_exist(vector_t *v, const uint32_t node_id);
int common_config_exist(vector_t *v, const char *config_name);

int common_has_node(neu_plugin_t *plugin, const uint32_t node_id);

int common_has_group_config(neu_plugin_t *plugin, const uint32_t node_id,
                            const char *group_config_name);
neu_taggrp_config_t *common_get_group_config(neu_plugin_t * plugin,
                                             const uint32_t node_id,
                                             const char *   group_config_name);
void common_group_configs_foreach(neu_plugin_t *plugin, const uint32_t node_id,
                                  void *context,
                                  void  visit_func(neu_taggrp_config_t *,
                                                  void *));

neu_datatag_table_t *common_get_datatags_table(neu_plugin_t * plugin,
                                               const uint32_t node_id);

#ifdef __cplusplus
}
#endif
#endif