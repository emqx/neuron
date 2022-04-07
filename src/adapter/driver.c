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
#include <stdlib.h>

#include "event/event.h"

#include "cache.h"
#include "node.h"
#include "node_internal.h"

struct neu_node_driver {
    neu_node_t node;

    neu_driver_cache_t *cache;

    neu_event_ctx_t *msg_events;
    neu_event_ctx_t *driver_events;
};

neu_node_driver_t *neu_node_driver_create(neu_node_t *node)
{
    neu_node_driver_t *driver = calloc(1, sizeof(neu_node_driver_t));

    driver->node = *node;
    free(node);

    driver->cache         = neu_driver_cache_new();
    driver->msg_events    = neu_event_new();
    driver->driver_events = neu_event_new();

    return driver;
}

void neu_node_driver_destroy(neu_node_driver_t *driver)
{
    neu_event_close(driver->driver_events);
    neu_event_close(driver->msg_events);
    neu_driver_cache_destroy(driver->cache);
}

int neu_node_driver_start(neu_node_driver_t *driver)
{
    (void) driver;
    return 0;
}

int neu_node_driver_stop(neu_node_driver_t *driver)
{
    (void) driver;
    return 0;
}

int neu_node_driver_init(neu_node_driver_t *driver)
{
    (void) driver;
    return 0;
}

int neu_node_driver_uninit(neu_node_driver_t *driver)
{
    (void) driver;
    return 0;
}

void neu_node_driver_process_msg(neu_node_driver_t *driver)
{
    (void) driver;
}