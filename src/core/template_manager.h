/**
 * NEURON IIoT System for Industry 4.0
 * Copyright (C) 2021-2023 EMQ Technologies Co., Ltd All rights reserved.
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

#ifndef _NEU_TEMPLATE_MANAGER_H_
#define _NEU_TEMPLATE_MANAGER_H_

#include "base/template.h"
#include "core/plugin_manager.h"

typedef struct neu_template_manager_s neu_template_manager_t;

neu_template_manager_t *neu_template_manager_create();
void neu_template_manager_destroy(neu_template_manager_t *mgr);

int neu_template_manager_count(const neu_template_manager_t *mgr);

// NOTE: this function takes ownership of argument `tmpl` and `inst`
int neu_template_manager_add(neu_template_manager_t *mgr, neu_template_t *tmpl,
                             neu_plugin_instance_t *inst);

int neu_template_manager_del(neu_template_manager_t *mgr, const char *name);

// NOTE: ownership of the return template belongs to the manager.
//       user must not alter the name or plugin of the return template.
neu_template_t *neu_template_manager_find(const neu_template_manager_t *mgr,
                                          const char *                  name);

// NOTE: ownership of the return group belongs to the manager.
//       user must not alter the name of the return group.
int neu_template_manager_find_group(const neu_template_manager_t *mgr,
                                    const char *name, const char *group_name,
                                    neu_group_t **group_p);

// Iterate through templates and apply `cb` each.
// NOTE: `cb` must not alter the name or plugin of the template.
//       `cb` must not delete the template.
int neu_template_manager_for_each(neu_template_manager_t *mgr,
                                  int (*cb)(neu_template_t *tmpl, void *data),
                                  void *data);

void neu_template_manager_clear(neu_template_manager_t *mgr);

#endif
