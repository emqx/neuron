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

#ifndef NEURON_DATATAG_CONFIGS_H
#define NEURON_DATATAG_CONFIGS_H

#include "adapter.h"
#include "tag_group_config.h"

typedef struct neu_datatag_manager neu_datatag_manager_t;

neu_datatag_manager_t *neu_datatag_mng_create(neu_adapter_t *adapter);
void neu_datatag_mng_destroy(neu_datatag_manager_t *datatag_manager);
int  neu_datatag_mng_add_grp_config(neu_datatag_manager_t *datatag_manager,
                                    neu_taggrp_config_t *  grp_config);
int  neu_datatag_mng_del_grp_config(neu_datatag_manager_t *datatag_manager,
                                    const char *           config_name);
int  neu_datatag_mng_update_grp_config(neu_datatag_manager_t *datatag_manager,
                                       neu_taggrp_config_t *  new_grp_config);
const neu_taggrp_config_t *
neu_datatag_mng_ref_grp_config(neu_datatag_manager_t *datatag_manager,
                               const char *           config_name);
neu_taggrp_config_t *
    neu_datatag_mng_get_grp_config(neu_datatag_manager_t *datatag_manager,
                                   const char *           config_name);
int neu_datatag_mng_ref_all_grp_configs(neu_datatag_manager_t *datatag_manager,
                                        vector_t *             grp_configs);
neu_datatag_table_t *
neu_datatag_mng_get_datatag_tbl(neu_datatag_manager_t *datatag_manager);

#endif
