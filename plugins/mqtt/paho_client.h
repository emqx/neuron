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

#ifndef NEURON_PLUGIN_PAHO_CLIENT
#define NEURON_PLUGIN_PAHO_CLIENT

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

#include "mqtt_client.h"

typedef struct paho_client     paho_client_t;
typedef struct subscribe_tuple subscribe_tuple_t;

paho_client_t *paho_client_create(option_t *option, void *context);
client_error   paho_client_connect(paho_client_t *client);
client_error   paho_client_open(option_t *option, void *context,
                                paho_client_t **p_client);

client_error paho_client_is_connected(paho_client_t *client);

subscribe_tuple_t *paho_client_subscribe_create(paho_client_t *        client,
                                                const char *           topic,
                                                const int              qos,
                                                const subscribe_handle handle);
client_error       paho_client_subscribe_add(paho_client_t *    client,
                                             subscribe_tuple_t *tuple);
client_error       paho_client_subscribe_send(paho_client_t *    client,
                                              subscribe_tuple_t *tuple);
client_error paho_client_subscribe(paho_client_t *client, const char *topic,
                                   const int qos, subscribe_handle handle);

subscribe_tuple_t *paho_client_unsubscribe_create(paho_client_t *client,
                                                  const char *   topic);
client_error       paho_client_unsubscribe_send(paho_client_t *    client,
                                                subscribe_tuple_t *tuple);
client_error paho_client_unsubscribe(paho_client_t *client, const char *topic);

client_error paho_client_publish(paho_client_t *client, const char *topic,
                                 int qos, unsigned char *payload, size_t len);
client_error paho_client_close(paho_client_t *client);

#ifdef __cplusplus
}
#endif
#endif