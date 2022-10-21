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
    for (n = g_metrics_.node_metrics; NULL != n; n = n->hh.next) {
        neu_plugin_common_t *common =
            neu_plugin_to_plugin_common(n->adapter->plugin);

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
