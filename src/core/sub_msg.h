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

#ifndef _NEU_SUB_MSG_H_
#define _NEU_SUB_MSG_H_

#include "define.h"
#include "utils/utextend.h"

#include "adapter.h"

typedef struct neu_sub_msg_mgr neu_sub_msg_mgr_t;

neu_sub_msg_mgr_t *neu_sub_msg_manager_create();
void               neu_sub_msg_manager_destroy(neu_sub_msg_mgr_t *mgr);

// app name  array
UT_array *neu_sub_msg_manager_get(neu_sub_msg_mgr_t *  mgr,
                                  neu_subscribe_type_e type);
void neu_sub_msg_manager_add(neu_sub_msg_mgr_t *mgr, neu_subscribe_type_e type,
                             const char *app);
void neu_sub_msg_manager_del(neu_sub_msg_mgr_t *mgr, neu_subscribe_type_e type,
                             const char *app);

#endif
