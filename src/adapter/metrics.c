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

#include <dirent.h>
#include <pthread.h>
#include <unistd.h>

#include "adapter.h"
#include "adapter_internal.h"
#include "metrics.h"
#include "utils/log.h"
#include "utils/time.h"

static pthread_rwlock_t g_metrics_mtx_ = PTHREAD_RWLOCK_INITIALIZER;
static neu_metrics_t    g_metrics_;
static uint64_t         g_start_ts_;

static bool has_core_dumps()
{
    DIR *dp = opendir("core");
    if (NULL == dp) {
        return false;
    }

    bool found = false;
    for (struct dirent *de = NULL; NULL != (de = readdir(dp));) {
        if (0 == strncmp("core", de->d_name, 4)) {
            found = true;
            break;
        }
    }

    closedir(dp);
    return found;
}

static inline void metrics_unregister_entry(const char *name)
{
    neu_metric_entry_t *e = NULL;
    HASH_FIND_STR(g_metrics_.registered_metrics, name, e);
    if (0 == --e->value) {
        HASH_DEL(g_metrics_.registered_metrics, e);
        nlog_info("del entry:%s", e->name);
        neu_metric_entry_free(e);
    }
}

int neu_metric_entries_add(neu_metric_entry_t **entries, const char *name,
                           const char *help, neu_metric_type_e type,
                           uint64_t init)
{
    neu_metric_entry_t *entry = NULL;
    HASH_FIND_STR(*entries, name, entry);
    if (NULL != entry) {
        if (entry->type != type || (0 != strcmp(entry->help, help))) {
            nlog_error("metric entry %s(%d, %s) conflicts with (%d, %s)", name,
                       entry->type, entry->help, type, help);
            return -1;
        }
        return 1;
    }

    entry = calloc(1, sizeof(*entry));
    if (NULL == entry) {
        return -1;
    }

    entry->name  = name;
    entry->type  = type;
    entry->help  = help;
    entry->value = init;
    HASH_ADD_STR(*entries, name, entry);
    return 0;
}

void neu_metrics_init()
{
    pthread_rwlock_wrlock(&g_metrics_mtx_);
    if (0 == g_start_ts_) {
        g_start_ts_ = neu_time_ms();
    }
    pthread_rwlock_unlock(&g_metrics_mtx_);
}

void neu_metrics_add_node(const neu_adapter_t *adapter)
{
    pthread_rwlock_wrlock(&g_metrics_mtx_);
    HASH_ADD_STR(g_metrics_.node_metrics, name, adapter->metrics);
    pthread_rwlock_unlock(&g_metrics_mtx_);
}

void neu_metrics_del_node(const neu_adapter_t *adapter)
{
    pthread_rwlock_wrlock(&g_metrics_mtx_);
    HASH_DEL(g_metrics_.node_metrics, adapter->metrics);
    neu_metric_entry_t *e = NULL;
    HASH_LOOP(hh, adapter->metrics->entries, e)
    {
        metrics_unregister_entry(e->name);
    }
    pthread_rwlock_unlock(&g_metrics_mtx_);
}

int neu_metrics_register_entry(const char *name, const char *help,
                               neu_metric_type_e type)
{
    int                 rv = 0;
    neu_metric_entry_t *e  = NULL;

    pthread_rwlock_wrlock(&g_metrics_mtx_);
    rv = neu_metric_entries_add(&g_metrics_.registered_metrics, name, help,
                                type, 1);
    if (1 == rv) {
        HASH_FIND_STR(g_metrics_.registered_metrics, name, e);
        ++e->value;
        rv = 0;
    }
    pthread_rwlock_unlock(&g_metrics_mtx_);
    return rv;
}

void neu_metrics_unregister_entry(const char *name)
{
    pthread_rwlock_wrlock(&g_metrics_mtx_);
    metrics_unregister_entry(name);
    pthread_rwlock_unlock(&g_metrics_mtx_);
}

void neu_metrics_visist(neu_metrics_cb_t cb, void *data)
{
    bool     core_dumped    = has_core_dumps();
    uint64_t uptime_seconds = (neu_time_ms() - g_start_ts_) / 1000;
    pthread_rwlock_rdlock(&g_metrics_mtx_);
    g_metrics_.core_dumped    = core_dumped;
    g_metrics_.uptime_seconds = uptime_seconds;

    g_metrics_.north_nodes              = 0;
    g_metrics_.north_running_nodes      = 0;
    g_metrics_.north_disconnected_nodes = 0;
    g_metrics_.south_nodes              = 0;
    g_metrics_.south_running_nodes      = 0;
    g_metrics_.south_disconnected_nodes = 0;

    neu_node_metrics_t *n;
    HASH_LOOP(hh, g_metrics_.node_metrics, n)
    {
        neu_plugin_common_t *common =
            neu_plugin_to_plugin_common(n->adapter->plugin);

        n->adapter->cb_funs.update_metric(n->adapter, NEU_METRIC_RUNNING_STATE,
                                          n->adapter->state, NULL);
        n->adapter->cb_funs.update_metric(n->adapter, NEU_METRIC_LINK_STATE,
                                          common->link_state, NULL);

        if (NEU_NA_TYPE_DRIVER == n->adapter->module->type) {
            ++g_metrics_.south_nodes;
            if (NEU_NODE_RUNNING_STATE_RUNNING == n->adapter->state) {
                ++g_metrics_.south_running_nodes;
            }
            if (NEU_NODE_LINK_STATE_DISCONNECTED == common->link_state) {
                ++g_metrics_.south_disconnected_nodes;
            }
        } else if (NEU_NA_TYPE_APP == n->adapter->module->type) {
            ++g_metrics_.north_nodes;
            if (NEU_NODE_RUNNING_STATE_RUNNING == n->adapter->state) {
                ++g_metrics_.north_running_nodes;
            }
            if (NEU_NODE_LINK_STATE_DISCONNECTED == common->link_state) {
                ++g_metrics_.north_disconnected_nodes;
            }
        }
    }

    cb(&g_metrics_, data);
    pthread_rwlock_unlock(&g_metrics_mtx_);
}
