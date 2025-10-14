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

#ifndef NEURON_EVENT_H
#define NEURON_EVENT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct neu_events neu_events_t;

/**
 * @brief Creat a new event.
 * When an event is created, a corresponding thread is created, and both
 * io_event and timer_event in this event are scheduled for processing in this
 * thread.
 * @return the newly created event.
 */
neu_events_t *neu_event_new(const char *name);

/**
 * @brief Close a event.
 *
 * @param[in] events
 * @return 0 on successful shutdown.
 */
int neu_event_close(neu_events_t *events);

typedef struct neu_event_timer neu_event_timer_t;
typedef int (*neu_event_timer_callback)(void *usr_data);

typedef enum neu_event_timer_type {
    NEU_EVENT_TIMER_BLOCK   = 0,
    NEU_EVENT_TIMER_NOBLOCK = 1,
} neu_event_timer_type_e;

typedef struct neu_event_timer_param {
    // timer trigger period
    int64_t second;
    int64_t millisecond;
    // Parameters passed to callback when timer fires
    void *usr_data;
    // Callback function that fires every time the timer fires
    neu_event_timer_callback cb;
    neu_event_timer_type_e   type;
} neu_event_timer_param_t;

/**
 * @brief Add a timer to the event.
 *
 * @param[in] events
 * @param[in] timer Parameters when creating timer.
 * @return The added event timer.
 */
neu_event_timer_t *neu_event_add_timer(neu_events_t *          events,
                                       neu_event_timer_param_t timer);

/**
 * @brief Remove timer from event.
 *
 * @param[in] events
 * @param[in] timer
 * @return 0 on success.
 */
int neu_event_del_timer(neu_events_t *events, neu_event_timer_t *timer);

enum neu_event_io_type {
    NEU_EVENT_IO_READ   = 0x1,
    NEU_EVENT_IO_CLOSED = 0x2,
    NEU_EVENT_IO_HUP    = 0x3,
};
typedef struct neu_event_io neu_event_io_t;
typedef int (*neu_event_io_callback)(enum neu_event_io_type type, int fd,
                                     void *usr_data);

typedef struct neu_event_io_param {
    // fd that fires io_event.
    int fd;
    // Arguments passed to the neu_event_io_callback callback function when the
    // io_event is fired.
    void *usr_data;
    // The neu_event_io_callback callback function is triggered when io_event is
    // triggered.
    neu_event_io_callback cb;
} neu_event_io_param_t;

/**
 * @brief Add io_event to the event.
 *
 * @param[in] events
 * @param[in] io
 * @return  The added event io.
 */
neu_event_io_t *neu_event_add_io(neu_events_t *events, neu_event_io_param_t io);

/**
 * @brief Delete io_event from event.
 *
 * @param[in] events
 * @param[in] io
 * @return 0 on success.
 */
int neu_event_del_io(neu_events_t *events, neu_event_io_t *io);

#ifdef __cplusplus
}
#endif

#endif
