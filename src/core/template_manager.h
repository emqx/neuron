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

typedef struct neu_template_manager_s neu_template_manager_t;

neu_template_manager_t *neu_template_manager_create();
void neu_template_manager_destroy(neu_template_manager_t *mgr);

int neu_template_manager_add(neu_template_manager_t *mgr, neu_template_t *tmpl);
int neu_template_manager_del(neu_template_manager_t *mgr, const char *name);

const neu_template_t *
neu_template_manager_find(const neu_template_manager_t *mgr, const char *name);

#endif
