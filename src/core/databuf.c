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

#include <memory.h>
#include <stdio.h>
#include <stdlib.h>

#include "core/databuf.h"
#include "utils/atomic_data.h"

struct core_databuf {
    neu_atomic_int ref_count;
    size_t         len;
    void *         buf;
};

core_databuf_t *core_databuf_new_with_buf(void *buf, size_t len)
{
    core_databuf_t *databuf;

    if (len == 0) {
        return NULL;
    }

    databuf = (core_databuf_t *) malloc(sizeof(core_databuf_t));
    if (databuf == NULL) {
        return NULL;
    }

    neu_atomic_init(&databuf->ref_count);
    neu_atomic_set(&databuf->ref_count, 1);
    databuf->len = len;
    databuf->buf = buf;
    return databuf;
}

core_databuf_t *core_databuf_get(core_databuf_t *databuf)
{
    if (databuf == NULL) {
        return NULL;
    }

    neu_atomic_inc(&databuf->ref_count);
    return databuf;
}

core_databuf_t *core_databuf_put(core_databuf_t *databuf)
{
    if (databuf == NULL) {
        return NULL;
    }

    if (neu_atomic_dec_nv(&databuf->ref_count) == 0) {
        if (databuf->buf != NULL) {
            free(databuf->buf);
        }
        free(databuf);
        return NULL;
    }

    return databuf;
}

size_t core_databuf_get_len(core_databuf_t *databuf)
{
    if (databuf == NULL) {
        return 0;
    }

    return databuf->len;
}

core_databuf_t *core_databuf_set_buf(core_databuf_t *databuf, void *buf,
                                     size_t len)
{
    if (databuf == NULL) {
        return NULL;
    }

    if (databuf->buf != NULL) {
        free(databuf->buf);
    }

    databuf->len = len;
    databuf->buf = buf;
    return databuf;
}

void *core_databuf_get_ptr(core_databuf_t *databuf)
{
    if (databuf == NULL) {
        return NULL;
    }

    return (void *) databuf->buf;
}

char *core_databuf_dump(core_databuf_t *databuf)
{
    static char buf_content[2048] = { 0 };
    char *      buf               = databuf->buf;

    memset(buf_content, 0, sizeof(buf_content));

    for (size_t i = 0; i < databuf->len && i < sizeof(buf_content); i++) {
        snprintf(buf_content + i, sizeof(buf_content) - i, "%02X ", buf[i]);
    }

    return buf_content;
}
