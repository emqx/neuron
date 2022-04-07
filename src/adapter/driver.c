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

#include "adapter.h"
#include "adapter_internal.h"
#include "cache.h"

struct neu_adapter_driver {
    neu_adapter_t adapter;

    neu_driver_cache_t *cache;

    neu_event_ctx_t *msg_events;
    neu_event_ctx_t *driver_events;
};

neu_adapter_driver_t *neu_adapter_driver_create(neu_adapter_t *adapter)
{
    neu_adapter_driver_t *driver = calloc(1, sizeof(neu_adapter_driver_t));

    driver->adapter = *adapter;
    free(adapter);

    driver->cache         = neu_driver_cache_new();
    driver->msg_events    = neu_event_new();
    driver->driver_events = neu_event_new();

    return driver;
}

void neu_adapter_driver_destroy(neu_adapter_driver_t *driver)
{
    neu_event_close(driver->driver_events);
    neu_event_close(driver->msg_events);
    neu_driver_cache_destroy(driver->cache);
}

int neu_adapter_driver_start(neu_adapter_driver_t *driver)
{
    (void) driver;
    return 0;
}

int neu_adapter_driver_stop(neu_adapter_driver_t *driver)
{
    (void) driver;
    return 0;
}

int neu_adapter_driver_init(neu_adapter_driver_t *driver)
{
    (void) driver;
    return 0;
}

int neu_adapter_driver_uninit(neu_adapter_driver_t *driver)
{
    (void) driver;
    return 0;
}

void neu_adapter_driver_process_msg(neu_adapter_driver_t *driver)
{
    (void) driver;
}
