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
#include <string.h>

#include <nng/nng.h>
#include <nng/protocol/pair1/pair.h>
#include <nng/supplemental/util/platform.h>

#include "node.h"
#include "node_msg.h"
#include "plugin.h"

#include "core/plugin_manager.h"
#include "driver_internal.h"
#include "node_internal.h"

static neu_node_t *get_node(void *node);
static void        node_msg_loop(void *arg);

static const neu_node_callbacks_t callback_funs = {

};

void *neu_node_create(const char *name, uint32_t id, neu_plugin_type_e type,
                      const char *library)
{
    int         ret  = 0;
    neu_node_t *node = calloc(1, sizeof(neu_node_t));

    node->plugin_info.handle =
        load_plugin_library((char *) library, 1, &node->plugin_info.module);

    assert(node->plugin_info.handle != NULL);

    node->id                  = id;
    node->name                = strdup(name);
    node->plugin_info.library = strdup(library);

    node->status.link    = NEU_NODE_LINK_STATUS_DISCONNECTED;
    node->status.running = NEU_NODE_RUNNING_STATUS_IDLE;

    nng_mtx_alloc(&node->nng.mtx);
    ret = nng_pair1_open(&node->nng.socket);
    assert(ret == 0);

    node->plugin = node->plugin_info.module->funs->open(node, &callback_funs);

    switch (node->plugin_info.module->node_type) {
    case NEU_NODE_DRIVER:
        return neu_node_driver_create(node);
    case NEU_NODE_CONFIG_APP:
    case NEU_NODE_DATA_APP:
        break;
    }

    return NULL;
}

void neu_node_destroy(void *node)
{
    neu_node_t *nt = get_node(node);

    nng_close(nt->nng.socket);
    nt->plugin_info.module->funs->close(nt->plugin);
    unload_plugin_library(nt->plugin_info.handle, 1);

    switch (nt->plugin_info.module->node_type) {
    case NEU_NODE_DRIVER:
        neu_node_driver_destroy(node);
        break;
    case NEU_NODE_CONFIG_APP:
    case NEU_NODE_DATA_APP:
        break;
    }

    free(nt->name);
    free(nt->plugin_info.library);

    if (nt->config != NULL) {
        free(nt->config);
    }

    nng_mtx_free(nt->nng.mtx);
    free(node);
}

int neu_node_init(void *node)
{
    neu_node_t *nt     = get_node(node);
    int         ret    = nt->plugin_info.module->funs->init(nt->plugin);
    nt->status.running = NEU_NODE_RUNNING_STATUS_INIT;

    nng_thread_create(&nt->nng.thrd, node_msg_loop, node);

    return ret;
};

int neu_node_uninit(void *node)
{
    neu_node_t *nt = get_node(node);

    nt->plugin_info.module->funs->uninit(nt->plugin);

    nng_thread_destroy(nt->nng.thrd);

    return 0;
};

int neu_node_start(void *node)
{
    neu_node_t *   nt    = get_node(node);
    neu_err_code_e error = NEU_ERR_SUCCESS;

    // nng_mtx_lock(nt->nng.mtx);

    // nng_mtx_unlock(nt->nng.mtx);
    switch (nt->status.running) {
    case NEU_NODE_RUNNING_STATUS_IDLE:
    case NEU_NODE_RUNNING_STATUS_INIT:
        error = NEU_ERR_NODE_NOT_READY;
        break;
    case NEU_NODE_RUNNING_STATUS_RUNNING:
        error = NEU_ERR_NODE_IS_RUNNING;
        break;
    case NEU_NODE_RUNNING_STATUS_READY:
    case NEU_NODE_RUNNING_STATUS_STOPPED:
        break;
    }

    if (error == NEU_ERR_SUCCESS) {
        error = nt->plugin_info.module->funs->start(nt->plugin);
        if (error == NEU_ERR_SUCCESS) {
            nt->status.running = NEU_NODE_RUNNING_STATUS_RUNNING;
        }
    }

    return error;
}

int neu_node_stop(void *node)
{
    neu_node_t *   nt    = get_node(node);
    neu_err_code_e error = NEU_ERR_SUCCESS;

    // nng_mtx_lock(nt->nng.mtx);

    // nng_mtx_unlock(nt->nng.mtx);
    switch (nt->status.running) {
    case NEU_NODE_RUNNING_STATUS_IDLE:
    case NEU_NODE_RUNNING_STATUS_INIT:
    case NEU_NODE_RUNNING_STATUS_READY:
        error = NEU_ERR_NODE_NOT_RUNNING;
        break;
    case NEU_NODE_RUNNING_STATUS_STOPPED:
        error = NEU_ERR_NODE_IS_STOPED;
        break;
    case NEU_NODE_RUNNING_STATUS_RUNNING:
        break;
    }

    if (error == NEU_ERR_SUCCESS) {
        error = nt->plugin_info.module->funs->stop(nt->plugin);
        if (error == NEU_ERR_SUCCESS) {
            nt->status.running = NEU_NODE_RUNNING_STATUS_STOPPED;
        }
    }

    return error;
}

const char *neu_node_get_name(void *node)
{
    neu_node_t *nt = get_node(node);

    return strdup(nt->name);
}

int neu_node_set_config(void *node, const char *config)
{
    neu_node_t *nt = get_node(node);

    int ret = nt->plugin_info.module->funs->config(nt->plugin, config);
    if (ret == NEU_ERR_SUCCESS) {

        if (nt->config != NULL) {
            free(nt->config);
        }
        nt->config = strdup(config);

        if (nt->status.running == NEU_NODE_RUNNING_STATUS_INIT) {
            nt->status.running = NEU_NODE_RUNNING_STATUS_READY;
        }
    }

    return ret;
}

int neu_node_get_config(void *node, char **config)
{
    neu_node_t *nt = get_node(node);

    if (nt->config != NULL) {
        *config = strdup(nt->config);
        return NEU_ERR_SUCCESS;
    }

    return NEU_ERR_NODE_SETTING_NOT_FOUND;
}

int neu_node_get_status(void *node, neu_node_status_t *status)
{
    neu_node_t *nt = get_node(node);

    *status = nt->status;

    return NEU_ERR_SUCCESS;
}

neu_nodep_type_e neu_node_get_type(void *node)
{
    neu_node_t *nt = get_node(node);

    return nt->plugin_info.module->node_type;
}

static neu_node_t *get_node(void *node)
{
    return node;
}

static void node_msg_loop(void *arg)
{
    return NULL;
}