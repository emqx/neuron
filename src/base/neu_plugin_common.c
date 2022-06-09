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

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "errcodes.h"
#include "plugin.h"

#define NEU_PLUGIN_MAGIC_NUMBER 0x43474d50 // a string "PMGC"

#define NEU_SELF_NODE_NAME_UNKNOWN "unknown-adapter"

uint32_t neu_plugin_get_event_id(neu_plugin_t *plugin)
{
    neu_plugin_common_t *common = neu_plugin_to_plugin_common(plugin);
    common->event_id += 2; // for avoid check event_id == 0
    return common->event_id;
}

void neu_plugin_common_init(neu_plugin_common_t *common)
{
    common->event_id = 1;
    common->magic    = NEU_PLUGIN_MAGIC_NUMBER;
}

bool neu_plugin_common_check(neu_plugin_t *plugin)
{
    return neu_plugin_to_plugin_common(plugin)->magic == NEU_PLUGIN_MAGIC_NUMBER
        ? true
        : false;
}

int neu_plugin_op(neu_plugin_t *plugin, neu_reqresp_head_t head, void *data)
{
    neu_plugin_common_t *plugin_common = neu_plugin_to_plugin_common(plugin);

    return plugin_common->adapter_callbacks->command(plugin_common->adapter,
                                                     head, data);
}