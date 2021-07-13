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

#include <stdio.h>
#include <stdlib.h>

#include "neu_atomic_data.h"
#include "core/databuf.h"

struct core_databuf {
	neu_atomic_int ref_count;
	size_t	 len;
	char     buf[0];
};

core_databuf_t* core_databuf_alloc(size_t size)
{
	core_databuf_t* databuf;
	
	databuf = (core_databuf_t*)malloc(sizeof(core_databuf_t) + size);
	if (databuf == NULL) {
		return NULL;
	}

	neu_atomic_init(&databuf->ref_count);
	neu_atomic_set(&databuf->ref_count, 1);
	return databuf;
}

core_databuf_t* core_databuf_get(core_databuf_t* databuf)
{
	if (databuf == NULL) {
		return NULL;
	}

	neu_atomic_inc(&databuf->ref_count);
	return databuf;
}

void core_databuf_put(core_databuf_t* databuf)
{
	if (databuf == NULL) {
		return;
	}

	if (neu_atomic_dec_nv(&databuf->ref_count) == 0) {
		free(databuf);
	}
	return;
}

size_t core_databuf_get_len(core_databuf_t* databuf)
{
	if (databuf == NULL) {
		return 0;
	}
	
	return databuf->len;
}

void* core_databuf_peek_ptr(core_databuf_t* databuf)
{
	if (databuf == NULL) {
		return NULL;
	}
	
	return (void*)databuf->buf;
}

