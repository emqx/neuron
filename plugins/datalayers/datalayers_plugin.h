/**
 * NEURON IIoT System for Industry 4.0
 * Copyright (C) 2020-2022 EMQ Technologies Co., Ltd All rights reserved.
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

#ifndef NEURON_PLUGIN_DATALAYERS_H
#define NEURON_PLUGIN_DATALAYERS_H

#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif

#include "neuron.h"

#include "datalayers/flight_sql_client.h"
#include "datalayers_config.h"

typedef struct {
    char driver[NEU_NODE_NAME_LEN];
    char group[NEU_GROUP_NAME_LEN];
} route_key_t;

typedef struct {
    route_key_t key;

    UT_hash_handle hh;
} route_entry_t;

#define MAX_QUEUE_SIZE 1000

typedef struct db_write_task_s {
    UT_array *              int_tags;
    UT_array *              float_tags;
    UT_array *              bool_tags;
    UT_array *              string_tags;
    struct db_write_task_s *next;
    bool                    freed;
} db_write_task_t;

typedef struct {
    db_write_task_t *head;
    db_write_task_t *tail;
    int              size;
    int              max_size;
} task_queue_t;

struct neu_plugin {
    neu_plugin_common_t    common;
    datalayers_config_t    config;
    route_entry_t *        route_tbl;
    neu_datalayers_client *client;

    task_queue_t task_queue;
    pthread_t    consumer_thread;

    pthread_mutex_t plugin_mutex;
    bool            consumer_thread_stop_flag;

    int (*parse_config)(neu_plugin_t *plugin, const char *setting,
                        datalayers_config_t *config);
};

static inline void route_entry_free(route_entry_t *e)
{
    free(e);
}

static inline void route_tbl_free(route_entry_t *tbl)
{
    route_entry_t *e = NULL, *tmp = NULL;
    HASH_ITER(hh, tbl, e, tmp)
    {
        HASH_DEL(tbl, e);
        route_entry_free(e);
    }
}

static inline route_entry_t *
route_tbl_get(route_entry_t **tbl, const char *driver, const char *group)
{
    route_entry_t *find = NULL;
    route_key_t    key  = { 0 };

    strncpy(key.driver, driver, sizeof(key.driver));
    strncpy(key.group, group, sizeof(key.group));

    HASH_FIND(hh, *tbl, &key, sizeof(key), find);
    return find;
}

// NOTE: we take ownership of `topic`
static inline int route_tbl_add_new(route_entry_t **tbl, const char *driver,
                                    const char *group)
{
    route_entry_t *find = NULL;

    find = route_tbl_get(tbl, driver, group);
    if (find) {
        return NEU_ERR_GROUP_ALREADY_SUBSCRIBED;
    }

    find = calloc(1, sizeof(*find));
    if (NULL == find) {
        return NEU_ERR_EINTERNAL;
    }

    strncpy(find->key.driver, driver, sizeof(find->key.driver));
    strncpy(find->key.group, group, sizeof(find->key.group));
    HASH_ADD(hh, *tbl, key, sizeof(find->key), find);

    return 0;
}

static inline int route_tbl_update(route_entry_t **tbl, const char *driver,
                                   const char *group)
{
    route_entry_t *find = NULL;

    find = route_tbl_get(tbl, driver, group);
    if (NULL == find) {
        return NEU_ERR_GROUP_NOT_SUBSCRIBE;
    }

    return 0;
}

static inline void route_tbl_update_driver(route_entry_t **tbl,
                                           const char *    driver,
                                           const char *    new_name)
{
    route_entry_t *e = NULL, *tmp = NULL;
    HASH_ITER(hh, *tbl, e, tmp)
    {
        if (0 == strcmp(e->key.driver, driver)) {
            HASH_DEL(*tbl, e);
            strncpy(e->key.driver, new_name, sizeof(e->key.driver));
            HASH_ADD(hh, *tbl, key, sizeof(e->key), e);
        }
    }
}

static inline void route_tbl_update_group(route_entry_t **tbl,
                                          const char *driver, const char *group,
                                          const char *new_name)
{
    route_entry_t *e = route_tbl_get(tbl, driver, group);
    if (e) {
        HASH_DEL(*tbl, e);
        strncpy(e->key.group, new_name, sizeof(e->key.group));
        HASH_ADD(hh, *tbl, key, sizeof(e->key), e);
    }
}

static inline void route_tbl_del_driver(route_entry_t **tbl, const char *driver)
{
    route_entry_t *e = NULL, *tmp = NULL;
    HASH_ITER(hh, *tbl, e, tmp)
    {
        if (0 == strcmp(e->key.driver, driver)) {
            HASH_DEL(*tbl, e);
            route_entry_free(e);
        }
    }
}

static inline void route_tbl_del(route_entry_t **tbl, const char *driver,
                                 const char *group)
{
    route_entry_t *find = NULL;
    find                = route_tbl_get(tbl, driver, group);
    if (find) {
        HASH_DEL(*tbl, find);
        route_entry_free(find);
    }
}

#ifdef __cplusplus
}
#endif

#endif