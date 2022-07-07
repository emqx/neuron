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

#ifndef LICENSE_H
#define LICENSE_H

#define LICENSE_DIR "config"
#define LICENSE_FNAME "neuron.lic"

// License type
// Type: string
//    - "trial"
//    - "retail"
//    - "small-enterprise"
#define LICENSE_KEY_TYPE "1.3.6.1.4.1.52509.10"

// Max number of nodes.
// Type: integer, non-negative
#define LICENSE_KEY_MAX_NODES "1.3.6.1.4.1.52509.11"

// Max number of tags per node.
// Type: integer, non-negative
#define LICENSE_KEY_MAX_NODE_TAGS "1.3.6.1.4.1.52509.12"

// Bit flag indicating which plugins are enabled.
// Type: integer
//
//   MSB                                LSB
//   ..., 63, 62, ...              3, 2, 1, 0
//   ^----many others--------------^  ^  ^  ^
//                                    |  |  |
//   opcua----------------------------+  |  |
//   modbus_rtu--------------------------+  |
//   modbus_plus_tcp------------------------+
//
#define LICENSE_KEY_PLUGIN_FLAG "1.3.6.1.4.1.52509.13"

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#include "errcodes.h"

typedef enum {
    LICENSE_TYPE_TRIAL  = 0, // trial
    LICENSE_TYPE_RETAIL = 1, // retail
    LICENSE_TYPE_SME    = 2, // small enterprise
    LICENSE_TYPE_MAX,
} license_type_e;

const char *license_type_str(license_type_e t);

typedef enum {
    PLUGIN_BIT_MODBUS_PLUS_TCP = 0,
    PLUGIN_BIT_MODBUS_RTU      = 1,
    PLUGIN_BIT_OPCUA           = 2,
    PLUGIN_BIT_S7COMM          = 3,
    PLUGIN_BIT_FINS            = 4,
    PLUGIN_BIT_QNA3E           = 5,
    PLUGIN_BIT_IEC60870_5_104  = 6,
    PLUGIN_BIT_KNX             = 7,
    PLUGIN_BIT_NONA11          = 8,
    PLUGIN_BIT_DLT645_2007     = 9,
    PLUGIN_BIT_BACNET          = 10,
    PLUGIN_BIT_MAX,
} plugin_bit_e;

const char *plugin_bit_str(plugin_bit_e);

// Representing a single license.
typedef struct {
    license_type_e type_;                 // license type
    char *         fname_;                // license file name
    uint64_t       since_;                // not before
    uint64_t       until_;                // not after
    uint32_t       max_nodes_;            // max number of nodes
    uint32_t       max_node_tags_;        // max number of tags per node
    uint8_t        plugin_flag_[200 / 8]; // plugin enable/disable bits
} license_t;

// Initialize to be expired with no file.
void license_init(license_t *license);
void license_fini(license_t *license);

// Create new expired license associated with no file.
license_t *license_new();
void       license_free(license_t *license);
// Read from license file.
int license_read(license_t *license, const char *license_fname);

// Get associated license file name.
static inline const char *license_get_file_name(const license_t *license)
{
    return license->fname_;
}

static inline license_type_e license_get_type(const license_t *license)
{
    return license->type_;
}

// Get license expiry date.
static inline uint64_t license_not_before(const license_t *license)
{
    return license->since_;
}

// Get license expiry date.
static inline uint64_t license_not_after(const license_t *license)
{
    return license->until_;
}

// Check if the license is expired according to the current time.
static inline bool license_is_expired(const license_t *license)
{
    uint64_t now = time(NULL);
    return now < license->since_ || license->until_ < now;
}

static inline uint64_t license_get_max_nodes(const license_t *license)
{
    return license->max_nodes_;
}

static inline uint64_t license_get_max_node_tags(const license_t *license)
{
    return license->max_node_tags_;
}

static inline bool license_is_plugin_enabled(const license_t *license,
                                             plugin_bit_e     bit)
{
    return license->plugin_flag_[bit >> 3] & (1 << (bit & 0x07));
}

#ifdef __cplusplus
}
#endif

#endif
