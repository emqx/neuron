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

#include "neu_adapter.h"
#include "neu_vector.h"

typedef struct neu_taggrp_config   neu_taggrp_config_t;
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

neu_taggrp_config_t *      neu_taggrp_cfg_new(char *config_name);
const neu_taggrp_config_t *neu_taggrp_cfg_ref(neu_taggrp_config_t *grp_config);
neu_taggrp_config_t *neu_taggrp_cfg_clone(neu_taggrp_config_t *src_config);
void                 neu_taggrp_cfg_free(neu_taggrp_config_t *grp_config);

uint32_t        neu_taggrp_cfg_get_interval(neu_taggrp_config_t *grp_config);
int             neu_taggrp_cfg_set_interval(neu_taggrp_config_t *grp_config,
                                            uint32_t             interval);
vector_t *      neu_taggrp_cfg_get_subpipes(neu_taggrp_config_t *grp_config);
const vector_t *neu_taggrp_cfg_ref_subpipes(neu_taggrp_config_t *grp_config);
vector_t *      neu_taggrp_cfg_get_datatag_ids(neu_taggrp_config_t *grp_config);
const vector_t *neu_taggrp_cfg_ref_datatag_ids(neu_taggrp_config_t *grp_config);
#endif
