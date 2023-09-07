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

#include "errcodes.h"
#include "group.h"
#include "utils/utarray.h"
#include "utils/uthash.h"

#include "template.h"

typedef struct {
    neu_group_t *  group;
    UT_hash_handle hh;
} group_entry_t;

struct neu_template_s {
    char *                     name;
    char *                     plugin;
    neu_plugin_tag_validator_t tag_validator;
    group_entry_t *            groups;
};

static inline void group_entry_free(group_entry_t *ent)
{
    neu_group_destroy(ent->group);
    free(ent);
}

neu_template_t *neu_template_new(const char *name, const char *plugin)
{
    neu_template_t *tmpl = calloc(1, sizeof(*tmpl));
    if (NULL == tmpl) {
        return NULL;
    }

    tmpl->name = strdup(name);
    if (NULL == tmpl->name) {
        free(tmpl);
        return NULL;
    }

    tmpl->plugin = strdup(plugin);
    if (NULL == tmpl->plugin) {
        free(tmpl->name);
        free(tmpl);
        return NULL;
    }

    return tmpl;
}

void neu_template_free(neu_template_t *tmpl)
{
    if (tmpl) {
        group_entry_t *ent = NULL, *tmp = NULL;
        HASH_ITER(hh, tmpl->groups, ent, tmp)
        {
            HASH_DEL(tmpl->groups, ent);
            group_entry_free(ent);
        }

        free(tmpl->name);
        free(tmpl->plugin);
        free(tmpl);
    }
}

void neu_template_set_tag_validator(neu_template_t *           tmpl,
                                    neu_plugin_tag_validator_t validator)
{
    tmpl->tag_validator = validator;
}

const char *neu_template_name(const neu_template_t *tmpl)
{
    return tmpl->name;
}

const char *neu_template_plugin(const neu_template_t *tmpl)
{
    return tmpl->plugin;
}

neu_group_t *neu_template_get_group(const neu_template_t *tmpl,
                                    const char *          group)
{
    group_entry_t *ent = NULL;
    HASH_FIND(hh, tmpl->groups, group, strlen(group), ent);
    return ent ? ent->group : NULL;
}

int neu_template_add_group(neu_template_t *tmpl, const char *group,
                           uint32_t interval)
{
    group_entry_t *ent = NULL;

    HASH_FIND(hh, tmpl->groups, group, strlen(group), ent);
    if (ent) {
        return NEU_ERR_GROUP_EXIST;
    }

    ent = calloc(1, sizeof(*ent));
    if (NULL == ent) {
        return NEU_ERR_EINTERNAL;
    }

    ent->group = neu_group_new(group, interval);
    if (NULL == ent->group) {
        free(ent);
        return NEU_ERR_EINTERNAL;
    }

    HASH_ADD_KEYPTR(hh, tmpl->groups, neu_group_get_name(ent->group),
                    strlen(group), ent);

    return 0;
}

int neu_template_del_group(neu_template_t *tmpl, const char *group)
{
    group_entry_t *ent = NULL;
    HASH_FIND(hh, tmpl->groups, group, strlen(group), ent);
    if (NULL == ent) {
        return NEU_ERR_GROUP_NOT_EXIST;
    }

    HASH_DEL(tmpl->groups, ent);
    group_entry_free(ent);
    return 0;
}

int neu_template_update_group_interval(neu_template_t *tmpl, const char *group,
                                       uint32_t interval)
{
    group_entry_t *ent = NULL;
    HASH_FIND(hh, tmpl->groups, group, strlen(group), ent);
    if (NULL == ent) {
        return NEU_ERR_GROUP_NOT_EXIST;
    }

    return neu_group_update(ent->group, interval);
}

int neu_template_update_group_name(neu_template_t *tmpl, const char *group,
                                   const char *new_name)
{
    if (0 == strcmp(group, new_name)) {
        return 0;
    }

    if (NULL != neu_template_get_group(tmpl, new_name)) {
        return NEU_ERR_GROUP_EXIST;
    }

    group_entry_t *ent = NULL;
    HASH_FIND(hh, tmpl->groups, group, strlen(group), ent);
    if (NULL == ent) {
        return NEU_ERR_GROUP_NOT_EXIST;
    }

    HASH_DEL(tmpl->groups, ent);
    int rv   = neu_group_set_name(ent->group, new_name);
    new_name = neu_group_get_name(ent->group);
    HASH_ADD_KEYPTR(hh, tmpl->groups, new_name, strlen(new_name), ent);

    return rv;
}

size_t neu_template_group_num(const neu_template_t *tmpl)
{
    return HASH_COUNT(tmpl->groups);
}

int neu_template_for_each_group(neu_template_t *tmpl,
                                int (*cb)(neu_group_t *group, void *data),
                                void *data)
{
    int            rv  = 0;
    group_entry_t *ent = NULL, *tmp = NULL;
    HASH_ITER(hh, tmpl->groups, ent, tmp)
    {
        if (0 != (rv = cb(ent->group, data))) {
            break;
        }
    }

    return rv;
}

int neu_template_add_tag(neu_template_t *tmpl, const char *group,
                         const neu_datatag_t *tag)
{
    int            ret = 0;
    group_entry_t *ent = NULL;
    HASH_FIND(hh, tmpl->groups, group, strlen(group), ent);
    if (NULL == ent) {
        return NEU_ERR_GROUP_NOT_EXIST;
    }

    if (NULL == tmpl->tag_validator) {
        return NEU_ERR_PLUGIN_NOT_SUPPORT_TEMPLATE;
    }

    if (0 != (ret = tmpl->tag_validator(tag))) {
        return ret;
    }

    return neu_group_add_tag(ent->group, tag);
}

int neu_template_update_tag(neu_template_t *tmpl, const char *group,
                            const neu_datatag_t *tag)
{
    int ret = 0;

    group_entry_t *ent = NULL;
    HASH_FIND(hh, tmpl->groups, group, strlen(group), ent);
    if (NULL == ent) {
        return NEU_ERR_GROUP_NOT_EXIST;
    }

    if (NULL == tmpl->tag_validator) {
        return NEU_ERR_PLUGIN_NOT_SUPPORT_TEMPLATE;
    }

    if (0 != (ret = tmpl->tag_validator(tag))) {
        return ret;
    }

    return neu_group_update_tag(ent->group, tag);
}
