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
#include <stdlib.h>

#include "adapter.h"
#include "errcodes.h"
#include "utils/log.h"

#include "subscribe.h"

typedef struct sub_elem_key {
    char driver[NEU_NODE_NAME_LEN];
    char group[NEU_GROUP_NAME_LEN];
} sub_elem_key_t;

typedef struct sub_elem {
    sub_elem_key_t key;

    UT_array *apps;

    UT_hash_handle hh;
} sub_elem_t;

static const UT_icd app_sub_icd = { sizeof(neu_app_subscribe_t), NULL, NULL,
                                    NULL };

struct neu_subscribe_mgr {
    sub_elem_t *ss;
};

neu_subscribe_mgr_t *neu_subscribe_manager_create()
{
    neu_subscribe_mgr_t *mgr = calloc(1, sizeof(neu_subscribe_mgr_t));

    return mgr;
}

void neu_subscribe_manager_destroy(neu_subscribe_mgr_t *mgr)
{
    sub_elem_t *el = NULL, *tmp = NULL;

    HASH_ITER(hh, mgr->ss, el, tmp)
    {
        HASH_DEL(mgr->ss, el);
        utarray_free(el->apps);
        free(el);
    }

    free(mgr);
}

UT_array *neu_subscribe_manager_find(neu_subscribe_mgr_t *mgr,
                                     const char *driver, const char *group)
{
    sub_elem_t *   find = NULL;
    sub_elem_key_t key  = { 0 };

    strncpy(key.driver, driver, sizeof(key.driver));
    strncpy(key.group, group, sizeof(key.group));

    HASH_FIND(hh, mgr->ss, &key, sizeof(sub_elem_key_t), find);

    if (find) {
        return utarray_clone(find->apps);
    } else {
        return NULL;
    }
}

UT_array *neu_subscribe_manager_find_by_driver(neu_subscribe_mgr_t *mgr,
                                               const char *         driver)
{
    sub_elem_t *el = NULL, *tmp = NULL;
    UT_array *  apps = NULL;

    utarray_new(apps, &app_sub_icd);

    HASH_ITER(hh, mgr->ss, el, tmp)
    {
        if (strcmp(driver, el->key.driver) == 0) {
            utarray_concat(apps, el->apps);
        }
    }

    return apps;
}

UT_array *neu_subscribe_manager_get(neu_subscribe_mgr_t *mgr, const char *app)
{
    sub_elem_t *el = NULL, *tmp = NULL;
    UT_array *  groups = NULL;
    UT_icd      icd = { sizeof(neu_resp_subscribe_info_t), NULL, NULL, NULL };

    utarray_new(groups, &icd);
    HASH_ITER(hh, mgr->ss, el, tmp)
    {
        utarray_foreach(el->apps, neu_app_subscribe_t *, sub_app)
        {
            if (strcmp(sub_app->app_name, app) == 0) {
                neu_resp_subscribe_info_t info = { 0 };

                strncpy(info.driver, el->key.driver, sizeof(info.driver));
                strncpy(info.app, app, sizeof(info.app));
                strncpy(info.group, el->key.group, sizeof(info.group));

                utarray_push_back(groups, &info);
            }
        }
    }

    return groups;
}

int neu_subscribe_manager_sub(neu_subscribe_mgr_t *mgr, const char *driver,
                              const char *app, const char *group, nng_pipe pipe)
{
    sub_elem_t *        find    = NULL;
    sub_elem_key_t      key     = { 0 };
    neu_app_subscribe_t app_sub = { 0 };

    strncpy(key.driver, driver, sizeof(key.driver));
    strncpy(key.group, group, sizeof(key.group));
    strncpy(app_sub.app_name, app, sizeof(app_sub.app_name));
    app_sub.pipe = pipe;

    HASH_FIND(hh, mgr->ss, &key, sizeof(sub_elem_key_t), find);

    if (find) {
        utarray_foreach(find->apps, neu_app_subscribe_t *, sub)
        {
            if (strcmp(sub->app_name, app) == 0) {
                return NEU_ERR_GROUP_ALREADY_SUBSCRIBED;
            }
        }
    } else {
        find = calloc(1, sizeof(sub_elem_t));
        utarray_new(find->apps, &app_sub_icd);
        find->key = key;
        HASH_ADD(hh, mgr->ss, key, sizeof(sub_elem_key_t), find);
    }

    utarray_push_back(find->apps, &app_sub);
    return NEU_ERR_SUCCESS;
}

int neu_subscribe_manager_unsub(neu_subscribe_mgr_t *mgr, const char *driver,
                                const char *app, const char *group)
{
    sub_elem_t *   find = NULL;
    sub_elem_key_t key  = { 0 };

    strncpy(key.driver, driver, sizeof(key.driver));
    strncpy(key.group, group, sizeof(key.group));

    HASH_FIND(hh, mgr->ss, &key, sizeof(sub_elem_key_t), find);

    if (find) {
        utarray_foreach(find->apps, neu_app_subscribe_t *, sub)
        {
            if (strcmp(sub->app_name, app) == 0) {
                utarray_erase(find->apps, utarray_eltidx(find->apps, sub), 1);
                return NEU_ERR_SUCCESS;
            }
        }
    }

    return NEU_ERR_GROUP_NOT_SUBSCRIBE;
}

void neu_subscribe_manager_remove(neu_subscribe_mgr_t *mgr, const char *driver,
                                  const char *group)
{
    sub_elem_t *el = NULL, *tmp = NULL;

    HASH_ITER(hh, mgr->ss, el, tmp)
    {
        if (strcmp(driver, el->key.driver) == 0 &&
            (group == NULL || strcmp(group, el->key.group) == 0)) {
            HASH_DEL(mgr->ss, el);
            utarray_free(el->apps);
            free(el);
        }
    }
}
