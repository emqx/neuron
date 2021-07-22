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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "neu_vector.h"
#include "plugin_manager.h"

typedef struct plugin_reg_info {
} plugin_reg_info_t;

struct plugin_manager {
    const plugin_reg_info_t *builtin_plugins;
    vector_t                 reg_plugins;
};

// Add builtin plugins
//

plugin_manager_t *plugin_manager_create()
{
    plugin_manager_t *plugin_mng;

    plugin_mng = (plugin_manager_t *) malloc(sizeof(plugin_manager_t));
    if (plugin_mng == NULL) {
        return NULL;
    }

    return plugin_mng;
}

void plugin_manager_destroy(plugin_manager_t *plugin_mng)
{
    if (plugin_mng == NULL) {
        return;
    }

    free(plugin_mng);
}

plugin_id_t plugin_manager_reg_plugin(
    plugin_manager_t *plugin_mng, plugin_reg_info_t *info)
{
    plugin_id_t plugin_id;

    return plugin_id;
}

int plugin_manager_unreg_plugin(
    plugin_manager_t *plugin_mng, plugin_id_t plugin_id)
{
    int rv = 0;

    return rv;
}
