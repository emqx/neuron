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
#ifndef _NEU_PLUGIN_MODBUS_POINT_H_
#define _NEU_PLUGIN_MODBUS_POINT_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include <neuron.h>

#include "modbus.h"

typedef struct modbus_point {
    uint8_t       slave_id;
    modbus_area_e area;
    uint16_t      start_address;
    uint16_t      n_register;

    neu_type_e                type;
    neu_datatag_addr_option_u option;
    char                      name[NEU_TAG_NAME_LEN];
} modbus_point_t;

typedef struct modbus_point_write {
    modbus_point_t point;
    neu_value_u    value;
} modbus_point_write_t;

int modbus_tag_to_point(const neu_datatag_t *tag, modbus_point_t *point);
int modbus_write_tag_to_point(const neu_plugin_tag_value_t *tag,
                              modbus_point_write_t *        point);

typedef struct modbus_read_cmd {
    uint8_t       slave_id;
    modbus_area_e area;
    uint16_t      start_address;
    uint16_t      n_register;

    UT_array *tags; // modbus_point_t ptr;
} modbus_read_cmd_t;

typedef struct modbus_read_cmd_sort {
    uint16_t           n_cmd;
    modbus_read_cmd_t *cmd;
} modbus_read_cmd_sort_t;

typedef struct modbus_write_cmd {
    uint8_t       slave_id;
    modbus_area_e area;
    uint16_t      start_address;
    uint16_t      n_register;
    uint8_t       n_byte;
    uint8_t *     bytes;

    UT_array *tags;
} modbus_write_cmd_t;

typedef struct modbus_write_cmd_sort {
    uint16_t            n_cmd;
    modbus_write_cmd_t *cmd;
} modbus_write_cmd_sort_t;

modbus_read_cmd_sort_t *modbus_tag_sort(UT_array *tags, uint16_t max_byte);
modbus_write_cmd_sort_t *
     modbus_write_tags_sort(UT_array *tags, modbus_endianess_64 endianess_64);
void modbus_tag_sort_free(modbus_read_cmd_sort_t *cs);

void modbus_convert_endianess_64(neu_value_u *       value,
                                 modbus_endianess_64 endianess_64);

#ifdef __cplusplus
}
#endif

#endif