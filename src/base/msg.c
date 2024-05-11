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

#include "msg.h"
#include "utils/log.h"

#include "msg_internal.h"

void neu_msg_gen(neu_reqresp_head_t *header, void *data)
{
    size_t data_size = neu_reqresp_size(header->type);
    assert(header->len >= sizeof(neu_reqresp_head_t) + data_size);
    if (data && &header[1] != data) {
        memcpy((uint8_t *) &header[1], data, data_size);
    }
}
