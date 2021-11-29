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

#ifndef NEURON_EVENT_H
#define NEURON_EVENT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct neu_event_ctx neu_event_ctx_t;

neu_event_ctx_t *neu_event_new(void);
int              neu_event_close(neu_event_ctx_t *events);

typedef struct neu_event_timer_ctx neu_event_timer_ctx_t;
typedef int (*neu_event_timer_callback)(void *usr_data);

typedef struct neu_event_timer {
    int64_t                  second;
    int64_t                  millisecond;
    void *                   usr_data;
    neu_event_timer_callback cb;
} neu_event_timer_t;

neu_event_timer_ctx_t *neu_event_add_timer(neu_event_ctx_t * events,
                                           neu_event_timer_t timer);
int neu_event_del_timer(neu_event_ctx_t *events, neu_event_timer_ctx_t *timer);

enum neu_event_io_type {
    NEU_EVENT_IO_READ       = 0x1,
    NEU_EVENT_IO_PEER_CLOSE = 0x2,
};
typedef struct neu_event_io_ctx neu_event_io_ctx_t;
typedef int (*neu_event_io_callback)(enum neu_event_io_type type, int fd,
                                     void *usr_data);

typedef struct neu_event_io {
    int                   type;
    int                   fd;
    void *                usr_data;
    neu_event_io_callback cb;
} neu_event_io_t;

neu_event_io_ctx_t *neu_event_add_io(neu_event_ctx_t *events,
                                     neu_event_io_t   io);
int neu_event_del_io(neu_event_ctx_t *events, neu_event_io_ctx_t *io);

#ifdef __cplusplus
}
#endif

#endif
