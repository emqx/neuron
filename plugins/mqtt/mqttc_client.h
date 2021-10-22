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

#ifndef NEURON_PLUGIN_MQTTC_CLIENT
#define NEURON_PLUGIN_MQTTC_CLIENT

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

#include "mqtt_client.h"
#include "mqttc_client.h"

typedef struct mqttc_client mqttc_client_t;

client_error_e mqttc_client_open(option_t *option, void *context,
                                 mqttc_client_t **p_client);
client_error_e mqttc_client_is_connected(mqttc_client_t *client);
client_error_e mqttc_client_subscribe(mqttc_client_t *client, const char *topic,
                                      const int qos, subscribe_handle handle);
client_error_e mqttc_client_unsubscribe(mqttc_client_t *client,
                                        const char *    topic);
client_error_e mqttc_client_publish(mqttc_client_t *client, const char *topic,
                                    int qos, unsigned char *payload,
                                    size_t len);
client_error_e mqttc_client_close(mqttc_client_t *client);

#ifdef __cplusplus
}
#endif
#endif