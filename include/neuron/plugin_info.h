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

#ifndef _PLUGIN_INFO_H
#define _PLUGIN_INFO_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct plugin_id {
    uint32_t id_val;
} plugin_id_t;

typedef enum plugin_kind {
    PLUGIN_KIND_STATIC = 0,
    PLUGIN_KIND_SYSTEM,
    PLUGIN_KIND_CUSTOM
} plugin_kind_e;

typedef enum neu_plugin_link_state {
    NEU_PLUGIN_LINK_STATE_DISCONNECTED = 0,
    NEU_PLUGIN_LINK_STATE_CONNECTING,
    NEU_PLUGIN_LINK_STATE_CONNECTED,
} neu_plugin_link_state_e;

typedef enum neu_plugin_running_state {
    NEU_PLUGIN_RUNNING_STATE_IDLE = 0,
    NEU_PLUGIN_RUNNING_STATE_INIT,
    NEU_PLUGIN_RUNNING_STATE_READY,
    NEU_PLUGIN_RUNNING_STATE_RUNNING,
    NEU_PLUGIN_RUNNING_STATE_STOPPED,
} neu_plugin_running_state_e;

typedef struct neu_plugin_state {
    neu_plugin_link_state_e    link;
    neu_plugin_running_state_e running;
} neu_plugin_state_t;

#ifdef __cplusplus
}
#endif

#endif
