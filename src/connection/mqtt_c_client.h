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

#ifndef NEURON_NEU_MQTT_C_CLIENT
#define NEURON_NEU_MQTT_C_CLIENT

#ifdef __cplusplus
extern "C" {
#endif

#include "connection/mqtt_client_intf.h"
#include <stddef.h>
#include <stdint.h>

typedef struct mqtt_c_client mqtt_c_client_t;

neu_err_code_e mqtt_c_client_open(mqtt_c_client_t **       p_client,
                                  const neu_mqtt_option_t *option,
                                  void *                   context);
neu_err_code_e mqtt_c_client_is_connected(mqtt_c_client_t *client);
neu_err_code_e mqtt_c_client_subscribe(mqtt_c_client_t *client,
                                       const char *topic, const int qos,
                                       neu_subscribe_handle handle);
neu_err_code_e mqtt_c_client_unsubscribe(mqtt_c_client_t *client,
                                         const char *     topic);
neu_err_code_e mqtt_c_client_publish(mqtt_c_client_t *client, const char *topic,
                                     int qos, unsigned char *payload,
                                     size_t len);
neu_err_code_e mqtt_c_client_suspend(mqtt_c_client_t *client);
neu_err_code_e mqtt_c_client_continue(mqtt_c_client_t *client);
neu_err_code_e mqtt_c_client_close(mqtt_c_client_t *client);

#ifdef __cplusplus
}
#endif
#endif