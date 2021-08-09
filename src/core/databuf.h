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
 * Create a shared databuf with provided buffer, the ownership of buffer will
 * move into shared databuf.
 * The caller don't free the buffer.
 */
core_databuf_t *core_databuf_new_with_buf(void *buf, size_t len);

/**
 * Get a shared databuf, it will increase reference count of databuf
 */
core_databuf_t *core_databuf_get(core_databuf_t *databuf);

/**
 * Put a shared databuf, it will decrease reference count of databuf.
 * If the reference count of databuf is 0, then free databuf and buffer in it.
 */
core_databuf_t *core_databuf_put(core_databuf_t *databuf);

/**
 * Get length of the buffer in databuf
 */
size_t core_databuf_get_len(core_databuf_t *databuf);

/**
 * Replace original buffer pointer in databuf with new buffer pointer, the
 * original buffer should be free.
 */
core_databuf_t *core_databuf_set_buf(core_databuf_t *databuf, void *buf,
                                     size_t len);
/**
 * Get a buffer pointer in databuf
 */
void *core_databuf_get_ptr(core_databuf_t *databuf);

/**
 * Print buf content in hex.
 */
char *core_databuf_dump(core_databuf_t *databuf);

#endif
