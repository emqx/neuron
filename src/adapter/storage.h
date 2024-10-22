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

#ifndef ADAPTER_STORAGE_H
#define ADAPTER_STORAGE_H

#include "persist/persist.h"

#include "adapter_internal.h"

void adapter_storage_state(const char *node, neu_node_running_state_e state);
void adapter_storage_setting(const char *node, const char *setting);
void adapter_storage_add_group(const char *node, const char *group,
                               uint32_t interval, void *context);
void adapter_storage_update_group(const char *node, const char *group,
                                  const char *new_name, uint32_t interval);
void adapter_storage_del_group(const char *node, const char *group);
void adapter_storage_add_tag(const char *node, const char *group,
                             const neu_datatag_t *tag);
void adapter_storage_add_tags(const char *node, const char *group,
                              const neu_datatag_t *tags, size_t n);
void adapter_storage_update_tag(const char *node, const char *group,
                                const neu_datatag_t *tag);
void adapter_storage_update_tag_value(const char *node, const char *group,
                                      const neu_datatag_t *tag);
void adapter_storage_del_tag(const char *node, const char *group,
                             const char *name);

int adapter_load_setting(const char *node, char **setting);
int adapter_load_group_and_tag(neu_adapter_driver_t *driver);

#endif
