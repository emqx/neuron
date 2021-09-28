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

#include <stdio.h>
#include <stdlib.h>

#include "common.h"

int common_node_id_exist(vector_t *v, const uint32_t node_id)
{
    neu_node_info_t *node_info;
    VECTOR_FOR_EACH(v, iter)
    {
        node_info = (neu_node_info_t *) iterator_get(&iter);
        log_info("node info id:%ld, name:%s", node_info->node_id,
                 node_info->node_name);
        if (node_id == node_info->node_id) {
            return 0;
        }
    }
    return -1;
}

int common_config_exist(vector_t *v, const char *config_name)
{
    neu_taggrp_config_t *config;

    VECTOR_FOR_EACH(v, iter)
    {
        config = *(neu_taggrp_config_t **) iterator_get(&iter);
        if (NULL == config) {
            continue;
        }

        if (0 == strcmp(config_name, neu_taggrp_cfg_get_name(config))) {
            return 0;
        }
    }
    return -1;
}
