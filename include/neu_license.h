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

#ifndef __NEU_LICENSE_H__
#define __NEU_LICENSE_H__

#define NEURON_LIC_DIR "cert"
#define NEURON_LIC_FNAME "neuron.lic"

// License type
// Type: string
#define NEURON_LIC_KEY_LICENSE_TYPE "1.3.6.1.4.1.52509.10"

// Max number of plugins.
// Type: integer, non-negative
#define NEURON_LIC_KEY_PLUGINS_NUM_MAX "1.3.6.1.4.1.52509.11"

// Max number of nodes.
// Type: integer, non-negative
#define NEURON_LIC_KEY_NODES_NUM_MAX "1.3.6.1.4.1.52509.12"

// Max number of tags globally.
// Type: integer, non-negative
#define NEURON_LIC_KEY_TAGS_NUM_MAX "1.3.6.1.4.1.52509.13"

// If modbus_plus_tcp plugin enabled.
// Type: integer, 0 | 1
#define NEURON_LIC_KEY_MODBUS_PLUS_TCP_ENABLED "1.3.6.1.4.1.52509.14"

// If modbus_rtu plugin enabled
// Type: integer, 0 | 1
#define NEURON_LIC_KEY_MODBUS_RTU_ENABLED "1.3.6.1.4.1.52509.15"

// If opcua plugin enabled
// Type: integer, 0 | 1
#define NEURON_LIC_KEY_OPCUA_ENABLED "1.3.6.1.4.1.52509.16"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
}
#endif

#endif // __NEU_LICENSE_H__
