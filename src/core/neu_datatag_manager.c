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

#include "neu_atomic_data.h"
#include "neu_log.h"
#include "neu_vector.h"
#include <nng/nng.h>
#include <nng/supplemental/util/platform.h>

#include "neu_adapter.h"
#include "neu_datatag_manager.h"
#include "neu_datatag_table.h"

#define DEFAULT_SUB_PIPE_COUNT 4
#define DEFAULT_DATATAG_IDS_COUNT 16

/**
 * A datatag group configuration
 */
struct neu_taggrp_config {
    neu_atomic_int ref_count;
    char *         config_name;
    uint32_t       read_interval;
    vector_t       sub_pipes;
    vector_t       datatag_ids;
};

struct neu_datatag_manager {
    nng_mtx *            mtx;
    neu_adapter_t *      adapter;
    vector_t             p_configs; // element is pointer of neu_taggrp_config_t
    neu_datatag_table_t *tag_table; // a table of datatags in the device
};

// Return SIZE_MAX if can't find a plugin
static size_t find_taggrp_config(vector_t *p_configs, const char *config_name)
{
    size_t               index = SIZE_MAX;
    neu_taggrp_config_t *grp_config;

    VECTOR_FOR_EACH(p_configs, iter)
    {
        grp_config = *(neu_taggrp_config_t **) iterator_get(&iter);
        if (strcmp(grp_config->config_name, config_name) == 0) {
            index = iterator_index(p_configs, &iter);
            break;
        }
    }

    return index;
}

static void free_all_taggrp_config(vector_t *p_configs)
{
    neu_taggrp_config_t *grp_config;

    VECTOR_FOR_EACH(p_configs, iter)
    {
        grp_config = *(neu_taggrp_config_t **) iterator_get(&iter);
        neu_taggrp_cfg_free(grp_config);
    }

    return;
}

neu_datatag_manager_t *neu_datatag_mng_create(neu_adapter_t *adapter)
{
    int                    rv;
    neu_datatag_manager_t *datatag_manager;

    if (adapter == NULL) {
        log_error("Get NULL adapter for create datatag manager");
        return NULL;
    }

    datatag_manager =
        (neu_datatag_manager_t *) malloc(sizeof(neu_datatag_manager_t));
    if (datatag_manager == NULL) {
        log_error("No memory to allocate datatag manager");
        return NULL;
    }

    rv = vector_init(&datatag_manager->p_configs, DEFAULT_TAG_GROUP_COUNT,
                     sizeof(neu_taggrp_config_t *));
    if (rv != 0) {
        log_error("Failed to initialize p_configs in datatag manager");
        goto init_p_configs_fail;
    }

    datatag_manager->adapter = adapter;
    if (nng_mtx_alloc(&datatag_manager->mtx) != 0) {
        log_error("Failed to initialize mutex in datatag manager");
        goto alloc_mtx_fail;
    }
    return datatag_manager;

alloc_mtx_fail:
    vector_uninit(&datatag_manager->p_configs);
init_p_configs_fail:
    free(datatag_manager);
    return NULL;
}

void neu_datatag_mng_destroy(neu_datatag_manager_t *datatag_manager)
{
    if (datatag_manager == NULL) {
        return;
    }

    nng_mtx_free(datatag_manager->mtx);
    free_all_taggrp_config(&datatag_manager->p_configs);
    vector_uninit(&datatag_manager->p_configs);
    free(datatag_manager);
    return;
}

int neu_datatag_mng_add_grp_config(neu_datatag_manager_t *datatag_manager,
                                   neu_taggrp_config_t *  grp_config)
{
    int    rv = -1;
    size_t index;

    nng_mtx_lock(datatag_manager->mtx);
    index = find_taggrp_config(&datatag_manager->p_configs,
                               grp_config->config_name);
    if (index == SIZE_MAX) {
        // neu_taggrp_cfg_anchor(grp_config);
        rv = vector_push_back(&datatag_manager->p_configs, &grp_config);
    }
    nng_mtx_unlock(datatag_manager->mtx);

    if (rv == 0) {
        log_info("Add group config: %s", grp_config->config_name);
    }
    return rv;
}

int neu_datatag_mng_del_grp_config(neu_datatag_manager_t *datatag_manager,
                                   const char *           config_name)
{
    int                  rv = -1;
    size_t               index;
    neu_taggrp_config_t *grp_config;

    nng_mtx_lock(datatag_manager->mtx);
    index = find_taggrp_config(&datatag_manager->p_configs, config_name);
    if (index != SIZE_MAX) {
        grp_config = *(neu_taggrp_config_t **) vector_get(
            &datatag_manager->p_configs, index);
        // neu_taggrp_cfg_unanchor(grp_config);
        neu_taggrp_cfg_free(grp_config);
        vector_erase(&datatag_manager->p_configs, index);
        rv = 0;
    }
    nng_mtx_unlock(datatag_manager->mtx);

    if (rv == 0) {
        log_info("Delete group config: %s", grp_config->config_name);
    }
    return rv;
}

int neu_datatag_mng_update_grp_config(neu_datatag_manager_t *datatag_manager,
                                      neu_taggrp_config_t *  new_grp_config)
{
    int                  rv = -1;
    size_t               index;
    neu_taggrp_config_t *grp_config;

    if (new_grp_config == NULL) {
        return -1;
    }

    nng_mtx_lock(datatag_manager->mtx);
    index = find_taggrp_config(&datatag_manager->p_configs,
                               new_grp_config->config_name);
    if (index != SIZE_MAX) {
        grp_config = *(neu_taggrp_config_t **) vector_get(
            &datatag_manager->p_configs, index);
        // neu_taggrp_cfg_unanchor(grp_config);
        neu_taggrp_cfg_free(grp_config);
        // neu_taggrp_cfg_anchor(new_grp_config);
        vector_assign(&datatag_manager->p_configs, index, new_grp_config);
        rv = 0;
    }
    nng_mtx_unlock(datatag_manager->mtx);

    if (rv == 0) {
        log_info("Update group config: %s", grp_config->config_name);
    }
    return rv;
}

const neu_taggrp_config_t *
neu_datatag_mng_ref_grp_config(neu_datatag_manager_t *datatag_manager,
                               const char *           config_name)
{
    int                        rv = -1;
    size_t                     index;
    neu_taggrp_config_t *      grp_config;
    const neu_taggrp_config_t *ref_grp_config = NULL;

    nng_mtx_lock(datatag_manager->mtx);
    index = find_taggrp_config(&datatag_manager->p_configs, config_name);
    if (index != SIZE_MAX) {
        grp_config = *(neu_taggrp_config_t **) vector_get(
            &datatag_manager->p_configs, index);
        ref_grp_config = neu_taggrp_cfg_ref(grp_config);
        rv             = 0;
    }
    nng_mtx_unlock(datatag_manager->mtx);
    return ref_grp_config;
}

neu_taggrp_config_t *
neu_datatag_mng_get_grp_config(neu_datatag_manager_t *datatag_manager,
                               const char *           config_name)
{
    size_t               index;
    neu_taggrp_config_t *grp_config     = NULL;
    neu_taggrp_config_t *ret_grp_config = NULL;

    nng_mtx_lock(datatag_manager->mtx);
    index = find_taggrp_config(&datatag_manager->p_configs, config_name);
    if (index != SIZE_MAX) {
        grp_config = *(neu_taggrp_config_t **) vector_get(
            &datatag_manager->p_configs, index);
        neu_taggrp_cfg_ref(grp_config);
    }
    nng_mtx_unlock(datatag_manager->mtx);

    if (grp_config != NULL) {
        ret_grp_config = neu_taggrp_cfg_clone(grp_config);
        if (ret_grp_config == NULL) {
            log_error("No memory to clone datatag group config");
        }
        neu_taggrp_cfg_free(grp_config);
    }

    return ret_grp_config;
}

int neu_datatag_mng_ref_all_grp_configs(neu_datatag_manager_t *datatag_manager,
                                        vector_t *             grp_configs)
{
    int                        rv = 0;
    neu_taggrp_config_t *      grp_config;
    const neu_taggrp_config_t *ref_grp_config;

    nng_mtx_lock(datatag_manager->mtx);
    VECTOR_FOR_EACH(&datatag_manager->p_configs, iter)
    {
        grp_config     = *(neu_taggrp_config_t **) iterator_get(&iter);
        ref_grp_config = neu_taggrp_cfg_ref(grp_config);
        vector_push_back(grp_configs, &ref_grp_config);
    }
    nng_mtx_unlock(datatag_manager->mtx);
    return rv;
}

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

    if (src_config != NULL) {
        return NULL;
    }

    dst_config = neu_taggrp_cfg_new(src_config->config_name);
    if (dst_config != NULL) {
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
