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

#ifndef _NEU_PLUGIN_H_
#define _NEU_PLUGIN_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "utils/utarray.h"

#include "node.h"
#include "tag.h"

typedef enum {
    NEU_PLUGIN_SYSTEM = 1,
    NEU_PLUGIN_CUSTOM = 2,
} neu_plugin_type_e;

typedef struct neu_pluginp neu_pluginp_t;
typedef struct neu_plugin_group {
    char *    group_name;
    UT_array *tag_names;
    void *    ctx;
} neu_plugin_group_t;

typedef struct neu_pluginp_funs {
    neu_pluginp_t *(*open)(neu_node_t *                node,
                           const neu_node_callbacks_t *callbacks);
    void (*close)(neu_pluginp_t *plugin);
    int (*init)(neu_pluginp_t *plugin);
    int (*uninit)(neu_pluginp_t *plugin);
    int (*start)(neu_pluginp_t *plugin);
    int (*stop)(neu_pluginp_t *plugin);
    int (*config)(neu_pluginp_t *plugin, const char *config);
    int (*asyn_recv)(neu_pluginp_t *plugin, uint16_t n_byte, uint8_t *bytes);

    union {
        struct {
            int (*validate_tag)(neu_pluginp_t *plugin, neu_tag_t *tag);
            void (*add_group)(neu_pluginp_t *plugin, neu_plugin_group_t *new);
            void (*del_group)(neu_pluginp_t *plugin, neu_plugin_group_t *old);
            void (*group_group)(neu_pluginp_t *plugin, neu_plugin_group_t *old,
                                neu_plugin_group_t *new);
            int (*group_timer)(neu_pluginp_t *     plugin,
                               neu_plugin_group_t *group);
            int (*write_tag)(neu_pluginp_t *plugin, neu_tag_t *tag,
                             neu_value_u value);
        } driver;

        struct {
        } config_app;

        struct {
        } data_app;
    };
} neu_pluginp_funs_t;

typedef struct neu_pluginp_module {
    const uint32_t            version;
    const char *              module_name;
    const char *              module_description;
    neu_nodep_type_e          node_type;
    const neu_pluginp_funs_t *funs;
    neu_plugin_type_e         plugin_type;
} neu_pluginp_module_t;

#ifdef __cplusplus
}
#endif

#endif
