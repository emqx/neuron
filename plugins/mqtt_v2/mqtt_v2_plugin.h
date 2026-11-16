/**
 * NEURON IIoT System for Industry 4.0
 * Copyright (C) 2020-2026 EMQ Technologies Co., Ltd All rights reserved.
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

#ifndef NEURON_PLUGIN_MQTT_V2_PLUGIN_H
#define NEURON_PLUGIN_MQTT_V2_PLUGIN_H

#ifdef __cplusplus
extern "C" {
#endif

#include <pthread.h>

#include "connection/mqtt_client.h"
#include "neuron.h"
#include "utils/uthash.h"

#include "mqtt_v2_config.h"
#include "mqtt_v2_write.h"

// metadata of a subscribed tag, which reported data does not carry
typedef struct {
    char *          name;
    char *          description;
    char *          unit;
    neu_attribute_e attribute;
    UT_hash_handle  hh;
} mqtt_v2_tag_t;

typedef struct {
    char           name[NEU_GROUP_NAME_LEN];
    char *         topic_tmpl; // data topic of the subscription, may be NULL
    char *         topic;      // `topic_tmpl` rendered
    mqtt_v2_tag_t *tags;
    UT_hash_handle hh;
} mqtt_v2_group_t;

typedef struct {
    char             name[NEU_NODE_NAME_LEN];
    mqtt_v2_group_t *groups; // sorted by name
    char *           state_topic;
    char *           data_topic;
    char *           write_req_topic;
    char *           write_resp_topic;
    // subscribed on the current client, NULL if not. Unsubscribing a topic
    // the client does not know crashes the client while connected.
    char *           write_sub_topic;
    bool             state_known;
    neu_node_state_t state;
    bool             state_published;
    UT_hash_handle   hh;
} mqtt_v2_device_t;

typedef struct mqtt_v2_write_ctx {
    uint32_t            magic;
    mqtt_v2_write_req_t req;
    char                device[NEU_NODE_NAME_LEN];
    char *              resp_topic;
    char (*groups)[NEU_GROUP_NAME_LEN]; // group of each value
    bool                      retried;
    struct mqtt_v2_write_ctx *next;
} mqtt_v2_write_ctx_t;

struct neu_plugin {
    neu_plugin_common_t common;

    // the core calls request() on two threads, trans data comes on its own.
    // `life_mtx` serializes the entry points, so that a client rebuild is
    // never seen half done. Taken before `mtx`, never in client callbacks.
    pthread_mutex_t    life_mtx;
    mqtt_v2_config_t   config;
    neu_events_t *     events;
    neu_event_timer_t *timer;
    uint32_t           ticks;
    int64_t            cache_metric_update_ts;

    // MQTT client callbacks run on other threads, `mtx` guards every field
    // below. Never close the client while holding `mtx`, closing waits for
    // the callbacks.
    pthread_mutex_t      mtx;
    neu_mqtt_client_t *  client;
    bool                 started;
    bool                 deleted;
    char                 app[NEU_NODE_NAME_LEN]; // name the topics use
    char *               prefix;
    char *               state_topic;
    mqtt_v2_device_t *   devices;
    UT_array *           stale_topics; // retained states to clear on connect
    int64_t              tags_refresh_ts;
    bool                 tags_refreshing;
    mqtt_v2_write_ctx_t *pending; // writes waiting for tag metadata
};

#ifdef __cplusplus
}
#endif

#endif
