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

#define NEURON_LOG_LABEL "neu_datatag_manager"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "log.h"
#include "neu_vector.h"

#include "adapter.h"
#include "neu_datatag_manager.h"

/**
 * A datatag group configuration
 */
struct neu_datatag_manager {
    pthread_mutex_t      mtx;
    neu_adapter_t *      adapter;
    vector_t             p_configs; // element is pointer of neu_taggrp_config_t
    neu_datatag_table_t *tag_table; // a table of datatags in the device
};

// Return SIZE_MAX if can't find a plugin
static size_t find_taggrp_config(vector_t *p_configs, const char *config_name)
{
    size_t               index = SIZE_MAX;
    neu_taggrp_config_t *grp_config;
    const char *         cur_config_name;

    VECTOR_FOR_EACH(p_configs, iter)
    {
        grp_config      = *(neu_taggrp_config_t **) iterator_get(&iter);
        cur_config_name = neu_taggrp_cfg_get_name(grp_config);
        if (strcmp(cur_config_name, config_name) == 0) {
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

    neu_datatag_table_t *tag_table;
    tag_table = neu_datatag_tbl_create();
    if (tag_table == NULL) {
        log_error("Failed to create datatag table in datatag manager");
        goto create_tag_table_fail;
    }
    datatag_manager->tag_table = tag_table;

    datatag_manager->adapter = adapter;
    if (pthread_mutex_init(&datatag_manager->mtx, NULL) != 0) {
        log_error("Failed to initialize mutex in datatag manager");
        goto alloc_mtx_fail;
    }
    return datatag_manager;

alloc_mtx_fail:
    neu_datatag_tbl_destroy(tag_table);
create_tag_table_fail:
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

    pthread_mutex_destroy(&datatag_manager->mtx);
    neu_datatag_tbl_destroy(datatag_manager->tag_table);
    free_all_taggrp_config(&datatag_manager->p_configs);
    vector_uninit(&datatag_manager->p_configs);
    free(datatag_manager);
    return;
}

int neu_datatag_mng_add_grp_config(neu_datatag_manager_t *datatag_manager,
                                   neu_taggrp_config_t *  grp_config)
{
    int    rv = NEU_ERR_GRP_CONFIG_CONFLICT;
    size_t index;

    pthread_mutex_lock(&datatag_manager->mtx);
    index = find_taggrp_config(&datatag_manager->p_configs,
                               neu_taggrp_cfg_get_name(grp_config));
    if (index == SIZE_MAX) {
        neu_taggrp_cfg_anchor(grp_config);
        rv = vector_push_back(&datatag_manager->p_configs, &grp_config);
    }
    pthread_mutex_unlock(&datatag_manager->mtx);

    if (rv == NEU_ERR_SUCCESS) {
        log_info("Add group config: %s", neu_taggrp_cfg_get_name(grp_config));
    }
    return rv;
}

int neu_datatag_mng_del_grp_config(neu_datatag_manager_t *datatag_manager,
                                   const char *           config_name)
{
    int                  rv = -1;
    size_t               index;
    neu_taggrp_config_t *grp_config;

    pthread_mutex_lock(&datatag_manager->mtx);
    index = find_taggrp_config(&datatag_manager->p_configs, config_name);
    if (index != SIZE_MAX) {
        grp_config = *(neu_taggrp_config_t **) vector_get(
            &datatag_manager->p_configs, index);
        neu_taggrp_cfg_unanchor(grp_config);
        neu_taggrp_cfg_free(grp_config);
        vector_erase(&datatag_manager->p_configs, index);
        rv = 0;
    }
    pthread_mutex_unlock(&datatag_manager->mtx);

    if (rv == 0) {
        log_info("Delete group config: %s", config_name);
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

    pthread_mutex_lock(&datatag_manager->mtx);
    index = find_taggrp_config(&datatag_manager->p_configs,
                               neu_taggrp_cfg_get_name(new_grp_config));
    if (index != SIZE_MAX) {
        grp_config = *(neu_taggrp_config_t **) vector_get(
            &datatag_manager->p_configs, index);
        neu_taggrp_cfg_unanchor(grp_config);
        neu_taggrp_cfg_free(grp_config);
        neu_taggrp_cfg_anchor(new_grp_config);
        vector_assign(&datatag_manager->p_configs, index, &new_grp_config);
        rv = 0;
    }
    pthread_mutex_unlock(&datatag_manager->mtx);

    if (rv == 0) {
        log_info("Update group config: %s",
                 neu_taggrp_cfg_get_name(new_grp_config));
    }
    return rv;
}

const neu_taggrp_config_t *
neu_datatag_mng_ref_grp_config(neu_datatag_manager_t *datatag_manager,
                               const char *           config_name)
{
    size_t                     index;
    neu_taggrp_config_t *      grp_config;
    const neu_taggrp_config_t *ref_grp_config = NULL;

    pthread_mutex_lock(&datatag_manager->mtx);
    index = find_taggrp_config(&datatag_manager->p_configs, config_name);
    if (index != SIZE_MAX) {
        grp_config = *(neu_taggrp_config_t **) vector_get(
            &datatag_manager->p_configs, index);
        ref_grp_config = neu_taggrp_cfg_ref(grp_config);
    }
    pthread_mutex_unlock(&datatag_manager->mtx);
    return ref_grp_config;
}

neu_taggrp_config_t *
neu_datatag_mng_get_grp_config(neu_datatag_manager_t *datatag_manager,
                               const char *           config_name)
{
    size_t               index;
    neu_taggrp_config_t *grp_config     = NULL;
    neu_taggrp_config_t *ret_grp_config = NULL;

    pthread_mutex_lock(&datatag_manager->mtx);
    index = find_taggrp_config(&datatag_manager->p_configs, config_name);
    if (index != SIZE_MAX) {
        grp_config = *(neu_taggrp_config_t **) vector_get(
            &datatag_manager->p_configs, index);
        neu_taggrp_cfg_ref(grp_config);
    }
    pthread_mutex_unlock(&datatag_manager->mtx);

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

    pthread_mutex_lock(&datatag_manager->mtx);
    VECTOR_FOR_EACH(&datatag_manager->p_configs, iter)
    {
        grp_config     = *(neu_taggrp_config_t **) iterator_get(&iter);
        ref_grp_config = neu_taggrp_cfg_ref(grp_config);
        vector_push_back(grp_configs, &ref_grp_config);
    }
    pthread_mutex_unlock(&datatag_manager->mtx);
    return rv;
}

neu_datatag_table_t *
neu_datatag_mng_get_datatag_tbl(neu_datatag_manager_t *datatag_manager)
{
    if (datatag_manager == NULL) {
        return NULL;
    }

    return datatag_manager->tag_table;
}
