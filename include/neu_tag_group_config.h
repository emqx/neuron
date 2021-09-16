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

#ifndef NEURON_TAG_GROUP_CONFIG_H
#define NEURON_TAG_GROUP_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

#include "neu_vector.h"

typedef struct neu_taggrp_config neu_taggrp_config_t;

neu_taggrp_config_t *      neu_taggrp_cfg_new(char *config_name);
const neu_taggrp_config_t *neu_taggrp_cfg_ref(neu_taggrp_config_t *grp_config);
neu_taggrp_config_t *neu_taggrp_cfg_clone(neu_taggrp_config_t *src_config);
void                 neu_taggrp_cfg_free(neu_taggrp_config_t *grp_config);

void neu_taggrp_cfg_anchor(neu_taggrp_config_t *grp_config);
void neu_taggrp_cfg_unanchor(neu_taggrp_config_t *grp_config);
bool neu_taggrp_cfg_is_anchored(neu_taggrp_config_t *grp_config);

const char *    neu_taggrp_cfg_get_name(neu_taggrp_config_t *grp_config);
uint32_t        neu_taggrp_cfg_get_interval(neu_taggrp_config_t *grp_config);
int             neu_taggrp_cfg_set_interval(neu_taggrp_config_t *grp_config,
                                            uint32_t             interval);
vector_t *      neu_taggrp_cfg_get_subpipes(neu_taggrp_config_t *grp_config);
const vector_t *neu_taggrp_cfg_ref_subpipes(neu_taggrp_config_t *grp_config);
vector_t *      neu_taggrp_cfg_get_datatag_ids(neu_taggrp_config_t *grp_config);
const vector_t *neu_taggrp_cfg_ref_datatag_ids(neu_taggrp_config_t *grp_config);

#ifdef __cplusplus
}
#endif

#endif
