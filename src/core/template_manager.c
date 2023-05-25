/**
 * NEURON IIoT System for Industry 4.0
 * Copyright (C) 2020-2023 EMQ Technologies Co., Ltd All rights reserved.
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

#include <dlfcn.h>

#include "base/template.h"
#include "errcodes.h"
#include "utils/uthash.h"

#include "template_manager.h"

typedef struct {
    neu_template_t *       tmpl;
    neu_plugin_instance_t *inst;
    UT_hash_handle         hh_name;
} name_entry_t;

static inline void free_plugin_instance(neu_plugin_instance_t *inst)
{
    dlclose(inst->handle);
    free(inst);
}

static inline void template_entry_free(name_entry_t *ent)
{
    neu_template_free(ent->tmpl);
    free_plugin_instance(ent->inst);
    free(ent);
}

struct neu_template_manager_s {
    name_entry_t *name_map;
};

neu_template_manager_t *neu_template_manager_create()
{
    neu_template_manager_t *mgr = calloc(1, sizeof(*mgr));
    return mgr;
}

void neu_template_manager_destroy(neu_template_manager_t *mgr)
{
    if (NULL == mgr) {
        return;
    }

    neu_template_manager_clear(mgr);

    free(mgr);
}

int neu_template_manager_add(neu_template_manager_t *mgr, neu_template_t *tmpl,
                             neu_plugin_instance_t *inst)
{
    name_entry_t *ent  = NULL;
    const char *  name = neu_template_name(tmpl);

    HASH_FIND(hh_name, mgr->name_map, name, strlen(name), ent);
    if (ent) {
        neu_template_free(tmpl);
        free_plugin_instance(inst);
        return NEU_ERR_TEMPLATE_EXIST;
    }

    ent = calloc(1, sizeof(*ent));
    if (NULL == ent) {
        neu_template_free(tmpl);
        free_plugin_instance(inst);
        return NEU_ERR_EINTERNAL;
    }

    ent->tmpl = tmpl;
    ent->inst = inst;
    HASH_ADD_KEYPTR(hh_name, mgr->name_map, name, strlen(name), ent);
    return 0;
}

int neu_template_manager_del(neu_template_manager_t *mgr, const char *name)
{
    name_entry_t *ent = NULL;
    HASH_FIND(hh_name, mgr->name_map, name, strlen(name), ent);
    if (NULL == ent) {
        return NEU_ERR_TEMPLATE_NOT_FOUND;
    }

    HASH_DELETE(hh_name, mgr->name_map, ent);
    template_entry_free(ent);
    return 0;
}

int neu_template_manager_count(const neu_template_manager_t *mgr)
{
    return HASH_CNT(hh_name, mgr->name_map);
}

neu_template_t *neu_template_manager_find(const neu_template_manager_t *mgr,
                                          const char *                  name)
{
    name_entry_t *ent = NULL;
    HASH_FIND(hh_name, mgr->name_map, name, strlen(name), ent);
    return ent ? ent->tmpl : NULL;
}

int neu_template_manager_find_group(const neu_template_manager_t *mgr,
                                    const char *name, const char *group_name,
                                    neu_group_t **group_p)
{
    neu_template_t *tmpl = neu_template_manager_find(mgr, name);
    if (NULL == tmpl) {
        return NEU_ERR_TEMPLATE_NOT_FOUND;
    }

    neu_group_t *grp = neu_template_get_group(tmpl, group_name);
    if (NULL == grp) {
        return NEU_ERR_GROUP_NOT_EXIST;
    }

    if (group_p) {
        *group_p = grp;
    }

    return 0;
}

int neu_template_manager_for_each(neu_template_manager_t *mgr,
                                  int (*cb)(neu_template_t *tmpl, void *data),
                                  void *data)
{
    int           rv  = 0;
    name_entry_t *ent = NULL, *tmp = NULL;

    HASH_ITER(hh_name, mgr->name_map, ent, tmp)
    {
        if (0 != (rv = cb(ent->tmpl, data))) {
            break;
        }
    }

    return rv;
}

void neu_template_manager_clear(neu_template_manager_t *mgr)
{
    name_entry_t *ent = NULL, *tmp = NULL;
    HASH_ITER(hh_name, mgr->name_map, ent, tmp)
    {
        HASH_DELETE(hh_name, mgr->name_map, ent);
        template_entry_free(ent);
    }
}
