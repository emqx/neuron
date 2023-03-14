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

#include "base/template.h"
#include "utils/uthash.h"

#include "template_manager.h"

typedef struct {
    neu_template_t *tmpl;
    UT_hash_handle  hh;
} name_entry_t;

static inline void template_entry_free(name_entry_t *ent)
{
    neu_template_free(ent->tmpl);
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

    name_entry_t *ent = NULL, *tmp = NULL;
    HASH_ITER(hh, mgr->name_map, ent, tmp)
    {
        HASH_DEL(mgr->name_map, ent);
        template_entry_free(ent);
    }

    free(mgr);
}

int neu_template_manager_add(neu_template_manager_t *mgr, neu_template_t *tmpl)
{
    name_entry_t *ent = calloc(1, sizeof(*ent));
    if (NULL == ent) {
        return -1;
    }

    ent->tmpl        = tmpl;
    const char *name = neu_template_name(tmpl);
    HASH_ADD_KEYPTR(hh, mgr->name_map, name, strlen(name), ent);
    return 0;
}

int neu_template_manager_del(neu_template_manager_t *mgr, const char *name)
{
    name_entry_t *ent = NULL;
    HASH_FIND(hh, mgr->name_map, name, strlen(name), ent);
    if (NULL == ent) {
        return -1;
    }

    HASH_DEL(mgr->name_map, ent);
    template_entry_free(ent);
    return 0;
}

const neu_template_t *
neu_template_manager_find(const neu_template_manager_t *mgr, const char *name)
{
    name_entry_t *ent = NULL;
    HASH_FIND(hh, mgr->name_map, name, strlen(name), ent);
    return ent ? ent->tmpl : NULL;
}
