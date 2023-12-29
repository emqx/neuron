/**
 * NEURON IIoT System for Industry 4.0
 * Copyright (C) 2020-2024 EMQ Technologies Co., Ltd All rights reserved.
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

#ifndef ADAPTER_MSG_Q_H
#define ADAPTER_MSG_Q_H

#include <stdint.h>

#include "base/msg_internal.h"
#include "msg.h"

typedef struct adapter_msg_q adapter_msg_q_t;

adapter_msg_q_t *adapter_msg_q_new(const char *name, uint32_t size);
void             adapter_msg_q_free(adapter_msg_q_t *q);

int      adapter_msg_q_push(adapter_msg_q_t *q, neu_msg_t *msg);
uint32_t adapter_msg_q_pop(adapter_msg_q_t *q, neu_msg_t **p_data);

#endif