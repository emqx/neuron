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

#ifndef NEURON_PLUGIN_MQTT_H
#define NEURON_PLUGIN_MQTT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "connection/mqtt_client.h"
#include "neuron.h"

#include "mqtt_config.h"

typedef struct {
    char driver[NEU_NODE_NAME_LEN];
    char group[NEU_GROUP_NAME_LEN];
} route_key_t;

typedef struct {
    route_key_t key;

    char *topic;

    UT_hash_handle hh;
} route_entry_t;

struct neu_plugin {
    neu_plugin_common_t common;
    mqtt_config_t       config;
    neu_mqtt_client_t * client;
    int64_t             cache_metric_update_ts;
    char *              read_req_topic;
    char *              read_resp_topic;
    route_entry_t *     route_tbl;

    int (*parse_config)(neu_plugin_t *plugin, const char *setting,
                        mqtt_config_t *config);
    int (*subscribe)(neu_plugin_t *plugin, const mqtt_config_t *config);
    int (*unsubscribe)(neu_plugin_t *plugin, const mqtt_config_t *config);
};

static inline void route_entry_free(route_entry_t *e)
{
    free(e->topic);
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
                                    const char *group, char *topic)
{
    route_entry_t *find = NULL;

    find = route_tbl_get(tbl, driver, group);
    if (find) {
        free(topic);
        return NEU_ERR_GROUP_ALREADY_SUBSCRIBED;
    }

    find = calloc(1, sizeof(*find));
    if (NULL == find) {
        free(topic);
        return NEU_ERR_EINTERNAL;
    }

    strncpy(find->key.driver, driver, sizeof(find->key.driver));
    strncpy(find->key.group, group, sizeof(find->key.group));
    find->topic = topic;
    HASH_ADD(hh, *tbl, key, sizeof(find->key), find);

    return 0;
}

// NOTE: we take ownership of `topic`
static inline int route_tbl_update(route_entry_t **tbl, const char *driver,
                                   const char *group, char *topic)
{
    route_entry_t *find = NULL;

    find = route_tbl_get(tbl, driver, group);
    if (NULL == find) {
        free(topic);
        return NEU_ERR_GROUP_NOT_SUBSCRIBE;
    }

    free(find->topic);
    find->topic = topic;

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
