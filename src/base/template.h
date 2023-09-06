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

#ifndef NEU_TEMPLATE_H
#define NEU_TEMPLATE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "group.h"
#include "plugin.h"
#include "tag.h"

typedef struct neu_template_s neu_template_t;

neu_template_t *neu_template_new(const char *name, const char *plugin);
void            neu_template_free(neu_template_t *tmpl);

void neu_template_set_tag_validator(neu_template_t *           tmpl,
                                    neu_plugin_tag_validator_t validator);

const char *neu_template_name(const neu_template_t *tmpl);
const char *neu_template_plugin(const neu_template_t *tmpl);

size_t neu_template_group_num(const neu_template_t *tmpl);

// NOTE: ownership of the return group belongs to the template.
//       user must not alter the name the return group.
neu_group_t *neu_template_get_group(const neu_template_t *tmpl,
                                    const char *          group);

int neu_template_add_group(neu_template_t *tmpl, const char *group,
                           uint32_t interval);
int neu_template_del_group(neu_template_t *tmpl, const char *group);
int neu_template_update_group_interval(neu_template_t *tmpl, const char *group,
                                       uint32_t interval);
int neu_template_update_group_name(neu_template_t *tmpl, const char *group,
                                   const char *new_name);

// Iterate through groups in the template and apply `cb` each.
// NOTE: `cb` must not alter the group name or delete the group.
int neu_template_for_each_group(neu_template_t *tmpl,
                                int (*cb)(neu_group_t *group, void *data),
                                void *data);

int neu_template_add_tag(neu_template_t *tmpl, const char *group,
                         const neu_datatag_t *tag);
int neu_template_update_tag(neu_template_t *tmpl, const char *group,
                            const neu_datatag_t *tag);

#ifdef __cplusplus
}
#endif

#endif
