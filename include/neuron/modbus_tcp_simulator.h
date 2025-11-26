/**
 * NEURON IIoT System for Industry 4.0
 * Copyright (C) 2020-2025 EMQ Technologies Co., Ltd All rights reserved.
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
#ifndef NEU_MODBUS_TCP_SIMULATOR_H
#define NEU_MODBUS_TCP_SIMULATOR_H

#include <stdbool.h>
#include <stdint.h>

int  neu_modbus_simulator_init(const char *config_dir);
void neu_modbus_simulator_fini(void);

int  neu_modbus_simulator_start(const char *ip, uint16_t port);
void neu_modbus_simulator_stop(void);
void neu_modbus_simulator_apply_persist(void);

typedef enum {
    NEU_SIM_TAG_NONE   = 0,
    NEU_SIM_TAG_SINE   = 1, // float, [-100,100], period 60s
    NEU_SIM_TAG_SAW    = 2, // int16, [0,100], period 100s
    NEU_SIM_TAG_SQUARE = 3, // int16, [-10,10], period 10s
    NEU_SIM_TAG_RANDOM = 4  // int16, [0,100]
} neu_sim_tag_type_e;

int neu_modbus_simulator_config_tags(const char **names, const char **addr_strs,
                                     const uint16_t *          addresses,
                                     const neu_sim_tag_type_e *types,
                                     int                       tag_count);

char *neu_modbus_simulator_export_drivers_json(void);
char *neu_modbus_simulator_list_tags_json(void);

typedef struct {
    bool     running;
    char     ip[64];
    uint16_t port;
    int      tag_count;
    int      error;
} neu_modbus_simulator_status_t;

void neu_modbus_simulator_get_status(neu_modbus_simulator_status_t *out);

#endif