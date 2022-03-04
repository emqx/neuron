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

// Max number of nodes.
// Type: integer, non-negative
#define NEURON_LIC_KEY_NODES_NUM_MAX "1.3.6.1.4.1.52509.11"

// Max number of tags globally.
// Type: integer, non-negative
#define NEURON_LIC_KEY_TAGS_NUM_MAX "1.3.6.1.4.1.52509.12"

// Bit flag indicating which plugins are enabled.
// Type: integer
//
//   MSB                                LSB
//    63, 62, ...              3, 2, 1, 0
//    ^----reserved------------^  ^  ^  ^
//                                |  |  |
//   opcua------------------------+  |  |
//   modbus_rtu----------------------+  |
//   modbus_tcp_plus--------------------+
//
#define NEURON_LIC_KEY_PLUGIN_FLAG "1.3.6.1.4.1.52509.13"

#define NEURON_LIC_FLAG_MODBUS_TCP_PLUS 0x0000000000000001
#define NEURON_LIC_FLAG_MODBUS_RTU 0x0000000000000002
#define NEURON_LIC_FLAG_OPCUA 0x0000000000000004

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
}
#endif

#endif // __NEU_LICENSE_H__
