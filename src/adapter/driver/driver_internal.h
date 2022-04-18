/**
 * NEURON IIoT System for Industry 4.0
 * Copyright (C) 2020-2021 EMQ Technologies Co., Ltd All rights reserved.
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

#ifndef _NEU_ADAPTER_DRIVER_INTERNAL_H_
#define _NEU_ADAPTER_DRIVER_INTERNAL_H_

#include "adapter.h"
#include "core/message.h"

neu_adapter_driver_t *neu_adapter_driver_create(neu_adapter_t *adapter);

void neu_adapter_driver_destroy(neu_adapter_driver_t *driver);
int  neu_adapter_driver_start(neu_adapter_driver_t *driver);
int  neu_adapter_driver_stop(neu_adapter_driver_t *driver);
int  neu_adapter_driver_init(neu_adapter_driver_t *driver);
int  neu_adapter_driver_uninit(neu_adapter_driver_t *driver);

void neu_adapter_driver_process_msg(neu_adapter_driver_t *driver,
                                    neu_request_t *       req);

#endif
