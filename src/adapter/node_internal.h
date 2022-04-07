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

#ifndef _NEU_NODE_INTERNAL_H_
#define _NEU_NODE_INTERNAL_H_

#include <nng/nng.h>
#include <nng/supplemental/util/platform.h>

#include "node.h"
#include "node_msg.h"
#include "plugin.h"

struct neu_node {
    uint32_t id;
    char *   name;
    char *   config;

    neu_pluginp_t *plugin;

    neu_node_status_t status;

    struct {
        char *library;
        void *handle;

        neu_pluginp_module_t *module;
    } plugin_info;

    struct {
        nng_socket  socket;
        nng_thread *thrd;
        nng_mtx *   mtx;
    } nng;
};

typedef struct neu_node_msg_processor {
    neu_node_msg_type_e type;
    int (*processor)(void *node, neu_msg_request_t *req,
                     neu_msg_response_t *res);
} neu_node_msg_processor_t;

void *neu_node_create(const char *name, uint32_t id, neu_plugin_type_e type,
                      const char *library);
void  neu_node_destroy(void *node);
int   neu_node_start(void *node);
int   neu_node_stop(void *node);
int   neu_node_init(void *node);
int   neu_node_uninit(void *node);
int   neu_node_set_config(void *node, const char *config);
int   neu_node_get_config(void *node, char **config);
int   neu_node_get_status(void *node, neu_node_status_t *status);

const char *neu_node_get_name(void *node);

neu_nodep_type_e neu_node_get_type(void *node);

#endif
