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

#include <stdlib.h>

#include <neuron.h>

#include "open62541_client.h"
#include "plugin_handle.h"

#define UNUSED(x) (void) (x)

static open62541_client_t *g_open62541_client;

void plugin_handle_set_open62541_client(open62541_client_t *client)
{
    g_open62541_client = client;
}

int plugin_handle_read(neu_plugin_t *plugin, vector_t *v)
{
    UNUSED(plugin);
    int rc = open62541_client_is_connected(g_open62541_client);
    if (0 != rc) {
        return -1;
    }

    rc = open62541_client_read(g_open62541_client, v);
    return 0;
}

int plugin_handle_write(neu_plugin_t *plugin, vector_t *v)
{
    UNUSED(plugin);
    int rc = open62541_client_is_connected(g_open62541_client);
    if (0 != rc) {
        return -1;
    }

    rc = open62541_client_write(g_open62541_client, v);
    return 0;
}