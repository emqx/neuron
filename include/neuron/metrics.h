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

#ifndef NEU_METRICS_H
#define NEU_METRICS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include <pthread.h>

#include "define.h"
#include "type.h"
#include "utils/rolling_counter.h"
#include "utils/utextend.h"
#include "utils/uthash.h"

extern int64_t global_timestamp;

typedef enum {
    NEU_METRIC_TYPE_COUNTER,
    NEU_METRIC_TYPE_GAUAGE,
    NEU_METRIC_TYPE_COUNTER_SET,
    NEU_METRIC_TYPE_ROLLING_COUNTER,
} neu_metric_type_e;

// node running state
#define NEU_METRIC_RUNNING_STATE "running_state"
#define NEU_METRIC_RUNNING_STATE_TYPE NEU_METRIC_TYPE_GAUAGE
#define NEU_METRIC_RUNNING_STATE_HELP "Node running state"

// node link state
#define NEU_METRIC_LINK_STATE "link_state"
#define NEU_METRIC_LINK_STATE_TYPE NEU_METRIC_TYPE_GAUAGE
#define NEU_METRIC_LINK_STATE_HELP "Node link state"

// last round trip time in millisends
#define NEU_METRIC_LAST_RTT_MS "last_rtt_ms"
#define NEU_METRIC_LAST_RTT_MS_TYPE NEU_METRIC_TYPE_GAUAGE
#define NEU_METRIC_LAST_RTT_MS_HELP \
    "Last request round trip time in milliseconds"
#define NEU_METRIC_LAST_RTT_MS_MAX 9999

// number of bytes sent
#define NEU_METRIC_SEND_BYTES "send_bytes"
#define NEU_METRIC_SEND_BYTES_TYPE NEU_METRIC_TYPE_COUNTER_SET
#define NEU_METRIC_SEND_BYTES_HELP "Total number of bytes sent"

// number of bytes received
#define NEU_METRIC_RECV_BYTES "recv_bytes"
#define NEU_METRIC_RECV_BYTES_TYPE NEU_METRIC_TYPE_COUNTER_SET
#define NEU_METRIC_RECV_BYTES_HELP "Total number of bytes received"

// maintained by neuron core
// number of tag read including errors
#define NEU_METRIC_TAG_READS_TOTAL "tag_reads_total"
#define NEU_METRIC_TAG_READS_TOTAL_TYPE NEU_METRIC_TYPE_COUNTER
#define NEU_METRIC_TAG_READS_TOTAL_HELP \
    "Total number of tag reads including errors"

// maintained by neuron core
// number of tag read errors
#define NEU_METRIC_TAG_READ_ERRORS_TOTAL "tag_read_errors_total"
#define NEU_METRIC_TAG_READ_ERRORS_TOTAL_TYPE NEU_METRIC_TYPE_COUNTER
#define NEU_METRIC_TAG_READ_ERRORS_TOTAL_HELP "Total number of tag read errors"

// maintained by neuron core
// number of tags in group
#define NEU_METRIC_TAGS_TOTAL "tags_total"
#define NEU_METRIC_TAGS_TOTAL_TYPE NEU_METRIC_TYPE_GAUAGE
#define NEU_METRIC_TAGS_TOTAL_HELP "Total number of tags in the node"

// maintained by neuron core
// number of tags in group
#define NEU_METRIC_GROUP_TAGS_TOTAL "group_tags_total"
#define NEU_METRIC_GROUP_TAGS_TOTAL_TYPE NEU_METRIC_TYPE_GAUAGE
#define NEU_METRIC_GROUP_TAGS_TOTAL_HELP "Total number of tags in the group"

// number of messages sent in last group timer
#define NEU_METRIC_GROUP_LAST_SEND_MSGS "group_last_send_msgs"
#define NEU_METRIC_GROUP_LAST_SEND_MSGS_TYPE NEU_METRIC_TYPE_GAUAGE
#define NEU_METRIC_GROUP_LAST_SEND_MSGS_HELP \
    "Number of messages sent on last group timer invocation"

// maintained by neuron core
// milliseconds consumed in last group timer invocation
#define NEU_METRIC_GROUP_LAST_TIMER_MS "group_last_timer_ms"
#define NEU_METRIC_GROUP_LAST_TIMER_MS_TYPE NEU_METRIC_TYPE_GAUAGE
#define NEU_METRIC_GROUP_LAST_TIMER_MS_HELP \
    "Time in milliseconds consumed on last group timer invocation"

// maintained by neuron core
// group last error code
#define NEU_METRIC_GROUP_LAST_ERROR_CODE "group_last_error_code"
#define NEU_METRIC_GROUP_LAST_ERROR_CODE_TYPE NEU_METRIC_TYPE_GAUAGE
#define NEU_METRIC_GROUP_LAST_ERROR_CODE_HELP \
    "Last encountered error code in group data acquisition"

// maintained by neuron core
// group last error timestamp
#define NEU_METRIC_GROUP_LAST_ERROR_TS "group_last_error_timestamp_ms"
#define NEU_METRIC_GROUP_LAST_ERROR_TS_TYPE NEU_METRIC_TYPE_GAUAGE
#define NEU_METRIC_GROUP_LAST_ERROR_TS_HELP \
    "Timestamp (ms) of the last encountered error in group data acquisition"

// number of messages sent
#define NEU_METRIC_SEND_MSGS_TOTAL "send_msgs_total"
#define NEU_METRIC_SEND_MSGS_TOTAL_TYPE NEU_METRIC_TYPE_COUNTER
#define NEU_METRIC_SEND_MSGS_TOTAL_HELP "Total number of messages sent"

// number of errors encounter while sending messages
#define NEU_METRIC_SEND_MSG_ERRORS_TOTAL "send_msg_errors_total"
#define NEU_METRIC_SEND_MSG_ERRORS_TOTAL_TYPE NEU_METRIC_TYPE_COUNTER
#define NEU_METRIC_SEND_MSG_ERRORS_TOTAL_HELP \
    "Total number of errors sending messages"

// number of messages received
#define NEU_METRIC_RECV_MSGS_TOTAL "recv_msgs_total"
#define NEU_METRIC_RECV_MSGS_TOTAL_TYPE NEU_METRIC_TYPE_COUNTER
#define NEU_METRIC_RECV_MSGS_TOTAL_HELP "Total number of messages received"

typedef enum {
    NEU_METRICS_CATEGORY_GLOBAL,
    NEU_METRICS_CATEGORY_DRIVER,
    NEU_METRICS_CATEGORY_APP,
    NEU_METRICS_CATEGORY_ALL,
} neu_metrics_category_e;

// metric entry
typedef struct {
    const char *           name;  // NOTE: should points to string literal
    const char *           help;  // NOTE: should points to string literal
    neu_metric_type_e      type;  //
    uint64_t               value; //
    neu_rolling_counter_t *rcnt;  //
    UT_hash_handle         hh;    // ordered by name
} neu_metric_entry_t;

// group metrics
typedef struct {
    char *              name;    // group name
    neu_metric_entry_t *entries; // group metric entries
    UT_hash_handle      hh;      // ordered by name
} neu_group_metrics_t;

// node metrics
typedef struct {
    pthread_mutex_t      lock;          // spin lock
    neu_node_type_e      type;          // node type
    char *               name;          // node name
    neu_metric_entry_t * entries;       // node metric entries
    neu_group_metrics_t *group_metrics; // group metrics
    neu_adapter_t *      adapter;       //
    UT_hash_handle       hh;            // ordered by name
} neu_node_metrics_t;

typedef struct {
    char                distro[32];
    char                kernel[32];
    unsigned            cpu_percent;
    unsigned            cpu_cores;
    size_t              mem_total_bytes;
    size_t              mem_used_bytes;
    size_t              mem_cache_bytes;
    size_t              disk_size_gibibytes;
    size_t              disk_used_gibibytes;
    size_t              disk_avail_gibibytes;
    bool                core_dumped;
    uint64_t            uptime_seconds;
    size_t              north_nodes;
    size_t              north_running_nodes;
    size_t              north_disconnected_nodes;
    size_t              south_nodes;
    size_t              south_running_nodes;
    size_t              south_disconnected_nodes;
    neu_node_metrics_t *node_metrics;
    neu_metric_entry_t *registered_metrics;
} neu_metrics_t;

void neu_metrics_init();
void neu_metrics_add_node(const neu_adapter_t *adapter);
void neu_metrics_del_node(const neu_adapter_t *adapter);
int  neu_metrics_register_entry(const char *name, const char *help,
                                neu_metric_type_e type);
void neu_metrics_unregister_entry(const char *name);

typedef void (*neu_metrics_cb_t)(const neu_metrics_t *metrics, void *data);
void neu_metrics_visist(neu_metrics_cb_t cb, void *data);

static inline const char *neu_metric_type_str(neu_metric_type_e type)
{
    if (NEU_METRIC_TYPE_COUNTER == type) {
        return "counter";
    } else {
        return "gauge";
    }
}

int neu_metric_entries_add(neu_metric_entry_t **entries, const char *name,
                           const char *help, neu_metric_type_e type,
                           uint64_t init);

static inline void neu_metric_entry_free(neu_metric_entry_t *entry)
{
    if (NEU_METRIC_TYPE_ROLLING_COUNTER == entry->type) {
        neu_rolling_counter_free(entry->rcnt);
    }
    free(entry);
}

static inline void neu_group_metrics_free(neu_group_metrics_t *group_metrics)
{
    if (NULL == group_metrics) {
        return;
    }

    neu_metric_entry_t *e = NULL, *tmp = NULL;
    HASH_ITER(hh, group_metrics->entries, e, tmp)
    {
        HASH_DEL(group_metrics->entries, e);
        neu_metrics_unregister_entry(e->name);
        neu_metric_entry_free(e);
    }
    free(group_metrics->name);
    free(group_metrics);
}

static inline neu_node_metrics_t *
neu_node_metrics_new(neu_adapter_t *adapter, neu_node_type_e type, char *name)
{
    neu_node_metrics_t *node_metrics =
        (neu_node_metrics_t *) calloc(1, sizeof(*node_metrics));
    if (NULL != node_metrics) {
        pthread_mutex_init(&node_metrics->lock, NULL);
        node_metrics->type    = type;
        node_metrics->name    = name;
        node_metrics->adapter = adapter;
        // neu_metrics_add_node(adapter);
    }
    return node_metrics;
}

static inline void neu_node_metrics_free(neu_node_metrics_t *node_metrics)
{
    if (NULL == node_metrics) {
        return;
    }

    pthread_mutex_destroy(&node_metrics->lock);

    neu_metric_entry_t *e = NULL, *etmp = NULL;
    HASH_ITER(hh, node_metrics->entries, e, etmp)
    {
        HASH_DEL(node_metrics->entries, e);
        neu_metrics_unregister_entry(e->name);
        neu_metric_entry_free(e);
    }

    neu_group_metrics_t *g = NULL, *gtmp = NULL;
    HASH_ITER(hh, node_metrics->group_metrics, g, gtmp)
    {
        HASH_DEL(node_metrics->group_metrics, g);
        neu_group_metrics_free(g);
    }

    free(node_metrics);
}

static inline int neu_node_metrics_add(neu_node_metrics_t *node_metrics,
                                       const char *group_name, const char *name,
                                       const char *help, neu_metric_type_e type,
                                       uint64_t init)
{
    int rv = 0;

    if (0 > neu_metrics_register_entry(name, help, type)) {
        return -1;
    }

    pthread_mutex_lock(&node_metrics->lock);
    if (NULL == group_name) {
        rv = neu_metric_entries_add(&node_metrics->entries, name, help, type,
                                    init);
    } else {
        neu_group_metrics_t *group_metrics = NULL;
        HASH_FIND_STR(node_metrics->group_metrics, group_name, group_metrics);
        if (NULL != group_metrics) {
            rv = neu_metric_entries_add(&group_metrics->entries, name, help,
                                        type, init);
        } else {
            group_metrics =
                (neu_group_metrics_t *) calloc(1, sizeof(*group_metrics));
            if (NULL != group_metrics &&
                NULL != (group_metrics->name = strdup(group_name))) {
                HASH_ADD_STR(node_metrics->group_metrics, name, group_metrics);
                rv = neu_metric_entries_add(&group_metrics->entries, name, help,
                                            type, init);
            } else {
                free(group_metrics);
                rv = -1;
            }
        }
    }
    pthread_mutex_unlock(&node_metrics->lock);

    if (0 != rv) {
        neu_metrics_unregister_entry(name);
    }
    return rv;
}

static inline int neu_node_metrics_update(neu_node_metrics_t *node_metrics,
                                          const char *        group,
                                          const char *metric_name, uint64_t n)
{
    neu_metric_entry_t *entry = NULL;

    pthread_mutex_lock(&node_metrics->lock);
    if (NULL == group) {
        HASH_FIND_STR(node_metrics->entries, metric_name, entry);
    } else if (NULL != node_metrics->group_metrics) {
        neu_group_metrics_t *g = NULL;
        HASH_FIND_STR(node_metrics->group_metrics, group, g);
        if (NULL != g) {
            HASH_FIND_STR(g->entries, metric_name, entry);
        }
    }

    if (NULL == entry) {
        pthread_mutex_unlock(&node_metrics->lock);
        return -1;
    }

    if (NEU_METRIC_TYPE_COUNTER == entry->type) {
        entry->value += n;
    } else if (NEU_METRIC_TYPE_ROLLING_COUNTER == entry->type) {
        entry->value =
            neu_rolling_counter_inc(entry->rcnt, global_timestamp, n);
    } else {
        entry->value = n;
    }
    pthread_mutex_unlock(&node_metrics->lock);

    return 0;
}

static inline void neu_node_metrics_reset(neu_node_metrics_t *node_metrics)
{
    neu_metric_entry_t *entry = NULL;

    pthread_mutex_lock(&node_metrics->lock);
    HASH_LOOP(hh, node_metrics->entries, entry)
    {
        entry->value = 0;
        if (NEU_METRIC_TYPE_ROLLING_COUNTER == entry->type) {
            neu_rolling_counter_reset(entry->rcnt);
        }
    }

    neu_group_metrics_t *g = NULL;
    HASH_LOOP(hh, node_metrics->group_metrics, g)
    {
        HASH_LOOP(hh, g->entries, entry)
        {
            entry->value = 0;
            if (NEU_METRIC_TYPE_ROLLING_COUNTER == entry->type) {
                neu_rolling_counter_reset(entry->rcnt);
            }
        }
    }
    pthread_mutex_unlock(&node_metrics->lock);
}

static inline int
neu_node_metrics_update_group(neu_node_metrics_t *node_metrics,
                              const char *        group_name,
                              const char *        new_group_name)
{
    int                  rv            = -1;
    neu_group_metrics_t *group_metrics = NULL;

    pthread_mutex_lock(&node_metrics->lock);
    HASH_FIND_STR(node_metrics->group_metrics, group_name, group_metrics);
    if (NULL != group_metrics) {
        char *name = strdup(new_group_name);
        if (NULL != name) {
            HASH_DEL(node_metrics->group_metrics, group_metrics);
            free(group_metrics->name);
            group_metrics->name = name;
            HASH_ADD_STR(node_metrics->group_metrics, name, group_metrics);
            rv = 0;
        }
    }
    pthread_mutex_unlock(&node_metrics->lock);

    return rv;
}

static inline void neu_node_metrics_del_group(neu_node_metrics_t *node_metrics,
                                              const char *        group_name)
{
    neu_group_metrics_t *gm = NULL;
    pthread_mutex_lock(&node_metrics->lock);
    HASH_FIND_STR(node_metrics->group_metrics, group_name, gm);
    if (NULL != gm) {
        HASH_DEL(node_metrics->group_metrics, gm);
        neu_group_metrics_free(gm);
    }
    pthread_mutex_unlock(&node_metrics->lock);
}

#ifdef __cplusplus
}
#endif

#endif
