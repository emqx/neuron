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

#ifndef _DATABUF_H_
#define _DATABUF_H_

typedef struct core_databuf core_databuf_t;

/**
 * Get a shared databuf, it will increase reference count of databuf
 */
core_databuf_t* core_databuf_get(core_databuf_t* databuf);

/**
 * Put a shared databuf, it will decrease reference count of databuf
 */
void core_databuf_put(core_databuf_t* databuf);

size_t core_databuf_get_len(core_databuf_t* databuf);
void* core_databuf_peek_ptr(core_databuf_t* databuf);

#endif
