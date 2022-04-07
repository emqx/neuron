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
#include <string.h>
#include <unistd.h>

#include <nng/nng.h>
#include <nng/supplemental/util/platform.h>

#include "log.h"
#include "neu_vector.h"
#include "utils/atomic_data.h"

#include "datatag_table.h"
#include "tag_group_config.h"

#define DEFAULT_SUB_PIPE_COUNT 4
#define DEFAULT_DATATAG_IDS_COUNT 16

typedef struct neu_taggrp_config neu_taggrp_config_t;

struct neu_taggrp_config {
    neu_atomic_int  ref_count;
    neu_atomic_bool is_anchored;
    char *          config_name;
    uint32_t        read_interval;
    vector_t        sub_pipes;
    vector_t        datatag_ids;
};

neu_taggrp_config_t *neu_taggrp_cfg_new(char *config_name)
{
    int                  rv;
    neu_taggrp_config_t *grp_config;

    grp_config = (neu_taggrp_config_t *) malloc(sizeof(neu_taggrp_config_t));
    if (grp_config == NULL) {
        log_error("No memory to allocate datatag group config");
        return NULL;
    }

    grp_config->config_name = strdup(config_name);
    if (grp_config->config_name == NULL) {
        log_error("No memory to duplicate config name");
        goto set_config_name_fail;
    }

    neu_atomic_init(&grp_config->ref_count);
    neu_atomic_set(&grp_config->ref_count, 1);
    neu_atomic_init_bool(&grp_config->is_anchored);
    neu_atomic_set_bool(&grp_config->is_anchored, false);
    rv = vector_init(&grp_config->sub_pipes, DEFAULT_SUB_PIPE_COUNT,
                     sizeof(nng_pipe));
    if (rv != 0) {
        log_error("Failed to initialize subscribe pipes in tag group config");
        goto init_sub_pipes_fail;
    }

    rv = vector_init(&grp_config->datatag_ids, DEFAULT_DATATAG_IDS_COUNT,
                     sizeof(datatag_id_t));
    if (rv != 0) {
        log_error("Failed to initialize datatag ids in tag group config");
        goto init_datatag_ids_fail;
    }

    return grp_config;

init_datatag_ids_fail:
    vector_uninit(&grp_config->sub_pipes);
init_sub_pipes_fail:
    free(grp_config->config_name);
set_config_name_fail:
    free(grp_config);
    return NULL;
}

const neu_taggrp_config_t *neu_taggrp_cfg_ref(neu_taggrp_config_t *grp_config)
{
    if (grp_config == NULL) {
        return NULL;
    }

    neu_atomic_inc(&grp_config->ref_count);
    return grp_config;
}

static void do_free_taggrp_cfg(neu_taggrp_config_t *grp_config)
{
    vector_uninit(&grp_config->sub_pipes);
    vector_uninit(&grp_config->datatag_ids);
    if (grp_config->config_name != NULL) {
        free(grp_config->config_name);
    }

    free(grp_config);
    return;
}

void neu_taggrp_cfg_free(neu_taggrp_config_t *grp_config)
{
    if (grp_config == NULL) {
        return;
    }

    if (neu_atomic_dec_nv(&grp_config->ref_count) == 0) {
        do_free_taggrp_cfg(grp_config);
    }
}

neu_taggrp_config_t *neu_taggrp_cfg_clone(neu_taggrp_config_t *src_config)
{
    int                  rv;
    neu_taggrp_config_t *dst_config;

    if (src_config == NULL) {
        return NULL;
    }

    dst_config = neu_taggrp_cfg_new(src_config->config_name);
    if (dst_config == NULL) {
        return NULL;
    }

    dst_config->read_interval = src_config->read_interval;
    rv = vector_copy_assign(&dst_config->sub_pipes, &src_config->sub_pipes);
    if (rv != 0) {
        goto copy_sub_pipes_fail;
    }

    rv = vector_copy_assign(&dst_config->datatag_ids, &src_config->datatag_ids);
    if (rv != 0) {
        goto copy_datatag_ids_fail;
    }
    return dst_config;

copy_datatag_ids_fail:
    vector_uninit(&dst_config->datatag_ids);
copy_sub_pipes_fail:
    vector_uninit(&dst_config->sub_pipes);
    neu_taggrp_cfg_free(dst_config);
    return NULL;
}

void neu_taggrp_cfg_anchor(neu_taggrp_config_t *grp_config)
{
    if (grp_config == NULL) {
        return;
    }

    neu_atomic_set_bool(&grp_config->is_anchored, true);
}

void neu_taggrp_cfg_unanchor(neu_taggrp_config_t *grp_config)
{
    if (grp_config == NULL) {
        return;
    }

    neu_atomic_set_bool(&grp_config->is_anchored, false);
}

bool neu_taggrp_cfg_is_anchored(neu_taggrp_config_t *grp_config)
{
    if (grp_config == NULL) {
        return false;
    }

    return neu_atomic_get_bool(&grp_config->is_anchored);
}

const char *neu_taggrp_cfg_get_name(neu_taggrp_config_t *grp_config)
{
    if (grp_config == NULL) {
        return NULL;
    }

    return grp_config->config_name;
}

uint32_t neu_taggrp_cfg_get_interval(neu_taggrp_config_t *grp_config)
{
    if (grp_config == NULL) {
        return 0;
    }

    return grp_config->read_interval;
}

int neu_taggrp_cfg_set_interval(neu_taggrp_config_t *grp_config,
                                uint32_t             interval)
{
    if (grp_config == NULL) {
        return -1;
    }

    grp_config->read_interval = interval;
    return 0;
}

vector_t *neu_taggrp_cfg_get_subpipes(neu_taggrp_config_t *grp_config)
{
    if (grp_config == NULL) {
        return NULL;
    }

    return &grp_config->sub_pipes;
}

const vector_t *neu_taggrp_cfg_ref_subpipes(neu_taggrp_config_t *grp_config)
{
    if (grp_config == NULL) {
        return NULL;
    }

    return &grp_config->sub_pipes;
}

vector_t *neu_taggrp_cfg_get_datatag_ids(neu_taggrp_config_t *grp_config)
{
    if (grp_config == NULL) {
        return NULL;
    }

    return &grp_config->datatag_ids;
}

const vector_t *neu_taggrp_cfg_ref_datatag_ids(neu_taggrp_config_t *grp_config)
{
    if (grp_config == NULL) {
        return NULL;
    }

    return &grp_config->datatag_ids;
}
