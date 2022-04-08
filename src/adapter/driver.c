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
#include <assert.h>
#include <stdlib.h>

#include "event/event.h"
#include "utils/utarray.h"

#include "adapter.h"
#include "adapter_internal.h"
#include "cache.h"
#include "driver_internal.h"

struct neu_adapter_driver {
    neu_adapter_t adapter;

    neu_driver_cache_t *cache;

    neu_events_t *msg_events;
    neu_events_t *driver_events;
};
// read/write/subscribe/unsubscribe

struct subscribe_group {
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

void neu_adapter_driver_process_msg(neu_adapter_driver_t *driver,
                                    msg_type_e type, neu_request_t *req)
{
    (void) driver;
    switch (type) {
    case MSG_CMD_READ_DATA: {
        neu_reqresp_read_t *read_req = (neu_reqresp_read_t *) req->buf;

        (void) read_req->grp_config;

        break;
    }
    case MSG_CMD_WRITE_DATA:
        break;
    case MSG_CMD_SUBSCRIBE_NODE: {
        neu_reqresp_subscribe_node_t *sub_req =
            (neu_reqresp_subscribe_node_t *) req->buf;

        (void) sub_req->grp_config;

        break;
    }
    case MSG_CMD_UNSUBSCRIBE_NODE: {
        neu_reqresp_unsubscribe_node_t *unsub_req =
            (neu_reqresp_unsubscribe_node_t *) req->buf;

        (void) unsub_req->grp_config;

        break;
    }
    default:
        assert(1 == 0);
        break;
    }
}
