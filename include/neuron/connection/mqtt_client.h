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

#ifndef NEURON_CONNECTION_MQTT_CLIENT_H
#define NEURON_CONNECTION_MQTT_CLIENT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "utils/zlog.h"

// number of messages cached
#define NEU_METRIC_CACHED_MSGS_NUM "cached_msgs"
#define NEU_METRIC_CACHED_MSGS_NUM_TYPE NEU_METRIC_TYPE_GAUAGE
#define NEU_METRIC_CACHED_MSGS_NUM_HELP "Number of messages cached"

#define NEU_MQTT_CACHE_SYNC_INTERVAL_MIN 10
#define NEU_MQTT_CACHE_SYNC_INTERVAL_MAX 12000
#define NEU_MQTT_CACHE_SYNC_INTERVAL_DEFAULT 100

typedef enum {
    NEU_MQTT_VERSION_V31  = 3,
    NEU_MQTT_VERSION_V311 = 4,
    NEU_MQTT_VERSION_V5   = 5,
} neu_mqtt_version_e;

typedef enum {
    NEU_MQTT_QOS0,
    NEU_MQTT_QOS1,
    NEU_MQTT_QOS2,
} neu_mqtt_qos_e;

typedef struct neu_mqtt_client_s neu_mqtt_client_t;

typedef struct {
    char *traceparent;
    char *tracestate;
} trace_w3c_t;

/**
 * Check that `topic_filter` is a valid MQTT topic filter.
 */
bool neu_mqtt_topic_filter_is_valid(const char *topic_filter);

/**
 * Check the given `topic_filter` matches the given `topic_name`.
 *
 * @Precondition  `topic_filter` must be a valid MQTT topic filter.
 * @Precondition  `topic_name` must be a valid MQTT topic name.
 */
bool neu_mqtt_topic_filter_is_match(const char *topic_filter,
                                    const char *topic_name);

typedef void (*neu_mqtt_client_connection_cb_t)(void *data);
typedef void (*neu_mqtt_client_publish_cb_t)(int errcode, neu_mqtt_qos_e qos,
                                             char *topic, uint8_t *payload,
                                             uint32_t len, void *data);
typedef void (*neu_mqtt_client_subscribe_cb_t)(neu_mqtt_qos_e qos,
                                               const char *   topic,
                                               const uint8_t *payload,
                                               uint32_t len, void *data,
                                               trace_w3c_t *trace_w3c);

neu_mqtt_client_t *neu_mqtt_client_new(neu_mqtt_version_e version);
neu_mqtt_client_t *neu_mqtt_client_from_addr(const char *host, uint16_t port,
                                             neu_mqtt_version_e version);
void               neu_mqtt_client_free(neu_mqtt_client_t *client);

bool   neu_mqtt_client_is_open(neu_mqtt_client_t *client);
bool   neu_mqtt_client_is_connected(neu_mqtt_client_t *client);
size_t neu_mqtt_client_get_cached_msgs_num(neu_mqtt_client_t *client);

int  neu_mqtt_client_set_addr(neu_mqtt_client_t *client, const char *host,
                              uint16_t port);
int  neu_mqtt_client_set_id(neu_mqtt_client_t *client, const char *id);
int  neu_mqtt_client_set_will_msg(neu_mqtt_client_t *client, const char *topic,
                                  uint8_t *msg, uint32_t len, bool retain,
                                  uint8_t qos);
int  neu_mqtt_client_set_user(neu_mqtt_client_t *client, const char *username,
                              const char *password);
int  neu_mqtt_client_set_connect_cb(neu_mqtt_client_t *             client,
                                    neu_mqtt_client_connection_cb_t cb,
                                    void *                          data);
int  neu_mqtt_client_set_disconnect_cb(neu_mqtt_client_t *             client,
                                       neu_mqtt_client_connection_cb_t cb,
                                       void *                          data);
int  neu_mqtt_client_set_tls(neu_mqtt_client_t *client, bool enabled,
                             const char *ca, const char *cert, const char *key,
                             const char *keypass);
void neu_mqtt_client_remove_cache_db(neu_mqtt_client_t *client);
int  neu_mqtt_client_set_cache_size(neu_mqtt_client_t *client,
                                    size_t mem_size_bytes, size_t db_size_bytes);
// default to NEU_MQTT_CACHE_SYNC_INTERVAL_DEFAULT if not set
int neu_mqtt_client_set_cache_sync_interval(neu_mqtt_client_t *client,
                                            uint32_t           interval);
int neu_mqtt_client_set_zlog_category(neu_mqtt_client_t *client,
                                      zlog_category_t *  cat);

int neu_mqtt_client_open(neu_mqtt_client_t *client);
int neu_mqtt_client_close(neu_mqtt_client_t *client);

/** Publish a `qos` message with `len` bytes `payload` on `topic`.
 *
 * This function initiates sending a `PUBLISH` packet, and if successful
 * returns zero and the callback `cb` will be called once on completion of
 * message delivery, otherwise returns a nonzero value. Callback invocation
 * will be of the form `cb(errcode, qos, topic, payload, len, data)`, where
 * `errcode` is zero if delivery was successful and nonzero otherwise, and
 * the other arguments are exactly the same what you pass into this function.
 * You may set `cb` to NULL if you do not care about the result.
 */
int neu_mqtt_client_publish(neu_mqtt_client_t *client, neu_mqtt_qos_e qos,
                            char *topic, uint8_t *payload, uint32_t len,
                            void *data, neu_mqtt_client_publish_cb_t cb);

int neu_mqtt_client_publish_with_trace(neu_mqtt_client_t *client,
                                       neu_mqtt_qos_e qos, char *topic,
                                       uint8_t *payload, uint32_t len,
                                       void *                       data,
                                       neu_mqtt_client_publish_cb_t cb,
                                       const char *traceparent);

/** Subscribe to `topic` with service quality `qos`.
 *
 * This function tries to send a `SUBSCRIBE` packet with the given `qos` and
 * `topic`. If this was successful returns zero and the callback `cb` will be
 * called on each received message with the matching topic, otherwise a nonzero
 * value is returned.
 */
int neu_mqtt_client_subscribe(neu_mqtt_client_t *client, neu_mqtt_qos_e qos,
                              const char *topic, void *data,
                              neu_mqtt_client_subscribe_cb_t cb);

int neu_mqtt_client_unsubscribe(neu_mqtt_client_t *client, const char *topic);

bool neu_mqtt_client_check_version_change(neu_mqtt_client_t *client,
                                          neu_mqtt_version_e version);

#ifdef __cplusplus
}
#endif

#endif
