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

#ifndef ADAPTER_INTERNAL_H
#define ADAPTER_INTERNAL_H

#include "define.h"
#include "event/event.h"
#include "plugin.h"

#include "adapter_info.h"
#include "core/manager.h"
#include "msg_q.h"

struct neu_adapter {
    char *name;
    char *setting;

    neu_node_running_state_e state;

    adapter_callbacks_t cb_funs;

    void *               handle;
    neu_plugin_module_t *module;
    neu_plugin_t *       plugin;

    neu_event_io_t *control_io;
    neu_event_io_t *trans_data_io;

    int control_fd;
    int trans_data_fd;

    adapter_msg_q_t *msg_q;
    pthread_t        consumer_tid;

    uint16_t trans_data_port;

    neu_events_t *events;

    neu_event_timer_t *timer_lev;
    int64_t            timestamp_lev;

    // metrics
    neu_node_metrics_t *metrics;
    int                 log_level;
};

typedef void (*adapter_handler)(neu_adapter_t *     adapter,
                                neu_reqresp_head_t *header);
typedef struct adapter_msg_handler {
    neu_reqresp_type_e type;
    adapter_handler    handler;
} adapter_msg_handler_t;

int  neu_adapter_error();
void neu_adapter_set_error(int error);

uint16_t neu_adapter_trans_data_port(neu_adapter_t *adapter);

neu_adapter_t *neu_adapter_create(neu_adapter_info_t *info, bool load);
bool neu_adapter_reset(neu_adapter_t *adapter, neu_adapter_info_t *info);
void neu_adapter_init(neu_adapter_t *adapter, neu_node_running_state_e state);

int neu_adapter_rename(neu_adapter_t *adapter, const char *new_name);

int neu_adapter_start(neu_adapter_t *adapter);
int neu_adapter_start_single(neu_adapter_t *adapter);
int neu_adapter_stop(neu_adapter_t *adapter);

neu_node_type_e      neu_adapter_get_type(neu_adapter_t *adapter);
neu_tag_cache_type_e neu_adapter_get_tag_cache_type(neu_adapter_t *adapter);

int  neu_adapter_uninit(neu_adapter_t *adapter);
void neu_adapter_destroy(neu_adapter_t *adapter);
void neu_adapter_handle_close(neu_adapter_t *adapter);

neu_event_timer_t *neu_adapter_add_timer(neu_adapter_t *         adapter,
                                         neu_event_timer_param_t param);
void neu_adapter_del_timer(neu_adapter_t *adapter, neu_event_timer_t *timer);

int neu_adapter_set_setting(neu_adapter_t *adapter, const char *config);
int neu_adapter_get_setting(neu_adapter_t *adapter, char **config);
neu_node_state_t neu_adapter_get_state(neu_adapter_t *adapter);

int neu_adapter_validate_tag(neu_adapter_t *adapter, neu_datatag_t *tag);

static inline void neu_adapter_reset_metrics(neu_adapter_t *adapter)
{
    if (NULL != adapter->metrics) {
        neu_node_metrics_reset(adapter->metrics);
    }
}

int  neu_adapter_register_group_metric(neu_adapter_t *adapter,
                                       const char *group_name, const char *name,
                                       const char *help, neu_metric_type_e type,
                                       uint64_t init);
int  neu_adapter_update_group_metric(neu_adapter_t *adapter,
                                     const char *   group_name,
                                     const char *metric_name, uint64_t n);
int  neu_adapter_metric_update_group_name(neu_adapter_t *adapter,
                                          const char *   group_name,
                                          const char *   new_group_name);
void neu_adapter_del_group_metrics(neu_adapter_t *adapter,
                                   const char *   group_name);

#endif
