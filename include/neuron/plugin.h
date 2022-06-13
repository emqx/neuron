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

#ifndef NEURON_PLUGIN_H
#define NEURON_PLUGIN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "utils/utarray.h"
#include "utils/zlog.h"

#include "adapter.h"
#include "type.h"

#define NEURON_PLUGIN_VER_1_0 100
#define NEURON_PLUGIN_VER_2_0 200

typedef struct neu_plugin_common {
    uint32_t                   magic;
    neu_plugin_link_state_e    link_state;
    neu_adapter_t *            adapter;
    const adapter_callbacks_t *adapter_callbacks;
    uint32_t                   event_id;
    zlog_category_t *          log;
} neu_plugin_common_t;

typedef struct neu_plugin neu_plugin_t;

typedef struct neu_plugin_group neu_plugin_group_t;
typedef void (*neu_plugin_group_free)(neu_plugin_group_t *pgp);
struct neu_plugin_group {
    char *    group_name;
    UT_array *tags;

    // the number of all tags under the node
    uint16_t tag_size;

    void *                user_data;
    neu_plugin_group_free group_free;
};

typedef struct neu_plugin_intf_funs {
    neu_plugin_t *(*open)(neu_adapter_t *            adapter,
                          const adapter_callbacks_t *callbacks);
    int (*close)(neu_plugin_t *plugin);
    int (*init)(neu_plugin_t *plugin);
    int (*uninit)(neu_plugin_t *plugin);
    int (*start)(neu_plugin_t *plugin);
    int (*stop)(neu_plugin_t *plugin);
    int (*config)(neu_plugin_t *plugin, neu_config_t *configs);

    int (*request)(neu_plugin_t *plugin, neu_reqresp_head_t *head, void *data);

    union {
        struct {
            int (*validate_tag)(neu_plugin_t *plugin, neu_datatag_t *tag);
            int (*group_timer)(neu_plugin_t *plugin, neu_plugin_group_t *group);
            int (*write_tag)(neu_plugin_t *plugin, void *req,
                             neu_datatag_t *tag, neu_value_u value);
        } driver;
    };

} neu_plugin_intf_funs_t;

typedef struct neu_plugin_module {
    const uint32_t                version;
    const char *                  module_name;
    const char *                  module_descr;
    const neu_plugin_intf_funs_t *intf_funs;
    neu_node_type_e               type;
    neu_plugin_kind_e             kind;
} neu_plugin_module_t;

inline static neu_plugin_common_t *
neu_plugin_to_plugin_common(neu_plugin_t *plugin)
{
    return (neu_plugin_common_t *) plugin;
}

void neu_plugin_common_init(neu_plugin_common_t *common);
bool neu_plugin_common_check(neu_plugin_t *plugin);

uint32_t neu_plugin_get_event_id(neu_plugin_t *plugin);

int neu_plugin_op(neu_plugin_t *plugin, neu_reqresp_head_t head, void *data);

#ifdef __cplusplus
}
#endif

#endif
