/**
 * NEURON IIoT System for Industry 4.0
 * Copyright (C) 2020-2024 EMQ Technologies Co., Ltd All rights reserved.
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

#ifndef DEMO_APP_PLUGIN_H
#define DEMO_APP_PLUGIN_H

#ifdef __cplusplus
extern "C" {
#endif

#include <neuron.h>

// Demo app plugin structure
struct neu_plugin {
    neu_plugin_common_t common;
};

// Plugin interface functions
extern const neu_plugin_intf_funs_t demo_app_plugin_intf_funs;

#ifdef __cplusplus
}
#endif

#endif // DEMO_APP_PLUGIN_H