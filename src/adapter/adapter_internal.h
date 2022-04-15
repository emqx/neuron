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

#ifndef ADAPTER_INTERNAL_H
#define ADAPTER_INTERNAL_H

#include <nng/nng.h>
#include <nng/protocol/pair1/pair.h>
#include <nng/supplemental/util/platform.h>

#include "event/event.h"
#include "plugin.h"
#include "plugin_info.h"

#include "adapter_info.h"
#include "core/databuf.h"
#include "core/neu_manager.h"
#include "core/neu_trans_buf.h"
#include "plugin_info.h"

typedef enum adapter_state {
    ADAPTER_STATE_IDLE = 0,
    ADAPTER_STATE_INIT,
    ADAPTER_STATE_READY,
    ADAPTER_STATE_RUNNING,
    ADAPTER_STATE_STOPPED,
} adapter_state_e;

struct neu_adapter {
    neu_adapter_id_t id;
    adapter_state_e  state;
    char *           name;
    neu_manager_t *  manager;

    uint32_t req_id;

    neu_trans_kind_e    trans_kind;
    adapter_callbacks_t cb_funs;
    neu_config_t        setting;
    vector_t            sub_grp_configs; // neu_sub_grp_config_t

    struct {
        plugin_id_t          id;
        plugin_kind_e        kind;
        void *               handle; // handle of dynamic lib
        neu_plugin_module_t *module;
        neu_plugin_t *       plugin;
    } plugin_info;

    struct {
        nng_socket sock;
        nng_dialer dialer;
        nng_mtx *  mtx;
        nng_mtx *  sub_grp_mtx;
    } nng;

    neu_events_t *  events;
    neu_event_io_t *nng_io;
    int             recv_fd;
};

neu_adapter_t *neu_adapter_create(neu_adapter_info_t *info,
                                  neu_manager_t *     manager);
void           neu_adapter_destroy(neu_adapter_t *adapter);
int            neu_adapter_start(neu_adapter_t *adapter);
int            neu_adapter_stop(neu_adapter_t *adapter);
int            neu_adapter_init(neu_adapter_t *adapter);
int            neu_adapter_uninit(neu_adapter_t *adapter);

neu_event_timer_t *neu_adapter_add_timer(neu_adapter_t *         adapter,
                                         neu_event_timer_param_t param);
void neu_adapter_del_timer(neu_adapter_t *adapter, neu_event_timer_t *timer);

const char *     neu_adapter_get_name(neu_adapter_t *adapter);
neu_manager_t *  neu_adapter_get_manager(neu_adapter_t *adapter);
neu_adapter_id_t neu_adapter_get_id(neu_adapter_t *adapter);
adapter_type_e   neu_adapter_get_type(neu_adapter_t *adapter);
plugin_id_t      neu_adapter_get_plugin_id(neu_adapter_t *adapter);
int neu_adapter_set_setting(neu_adapter_t *adapter, neu_config_t *config);
int neu_adapter_get_setting(neu_adapter_t *adapter, char **config);
neu_plugin_state_t neu_adapter_get_state(neu_adapter_t *adapter);
void               neu_adapter_add_sub_grp_config(neu_adapter_t *      adapter,
                                                  neu_node_id_t        node_id,
                                                  neu_taggrp_config_t *grp_config);
void               neu_adapter_del_sub_grp_config(neu_adapter_t *      adapter,
                                                  neu_node_id_t        node_id,
                                                  neu_taggrp_config_t *grp_config);
vector_t *         neu_adapter_get_sub_grp_configs(neu_adapter_t *adapter);
neu_plugin_running_state_e
    neu_adapter_state_to_plugin_state(neu_adapter_t *adapter);
int neu_adapter_validate_tag(neu_adapter_t *adapter, neu_datatag_t *tag);

#endif
