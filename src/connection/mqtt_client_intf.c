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

#include "connection/mqtt_client_intf.h"
#ifdef USE_NNG_MQTT
#include "mqtt_nng_client.h"
#else
#include "mqtt_c_client.h"
#endif

neu_err_code_e neu_mqtt_client_open(neu_mqtt_client_t *      p_client,
                                    const neu_mqtt_option_t *option,
                                    void *                   context)
{
#ifdef USE_NNG_MQTT
    return mqtt_nng_client_open((mqtt_nng_client_t **) p_client, option,
                                context);
#else
    return mqtt_c_client_open((mqtt_c_client_t **) p_client, option, context);
#endif
}

neu_err_code_e neu_mqtt_client_is_connected(neu_mqtt_client_t client)
{
#ifdef USE_NNG_MQTT
    return mqtt_nng_client_is_connected(client);
#else
    return mqtt_c_client_is_connected(client);
#endif
}

neu_err_code_e neu_mqtt_client_subscribe(neu_mqtt_client_t client,
                                         const char *topic, const int qos,
                                         neu_subscribe_handle handle)
{
#ifdef USE_NNG_MQTT
    return mqtt_nng_client_subscribe(client, topic, qos, handle);
#else
    return mqtt_c_client_subscribe(client, topic, qos, handle);
#endif
}

neu_err_code_e neu_mqtt_client_unsubscribe(neu_mqtt_client_t client,
                                           const char *      topic)
{
#ifdef USE_NNG_MQTT
    return mqtt_nng_client_unsubscribe(client, topic);
#else
    return mqtt_c_client_unsubscribe(client, topic);
#endif
}

neu_err_code_e neu_mqtt_client_publish(neu_mqtt_client_t client,
                                       const char *topic, int qos,
                                       unsigned char *payload, size_t len)
{
#ifdef USE_NNG_MQTT
    return mqtt_nng_client_publish(client, topic, qos, payload, len);
#else
    return mqtt_c_client_publish(client, topic, qos, payload, len);
#endif
}

neu_err_code_e neu_mqtt_client_suspend(neu_mqtt_client_t client)
{
#ifdef USE_NNG_MQTT
    return mqtt_nng_client_suspend(client);
#else
    return mqtt_c_client_suspend(client);
#endif
}

neu_err_code_e neu_mqtt_client_continue(neu_mqtt_client_t client)
{
#ifdef USE_NNG_MQTT
    return mqtt_nng_client_continue(client);
#else
    return mqtt_c_client_continue(client);
#endif
}

neu_err_code_e neu_mqtt_client_close(neu_mqtt_client_t client)
{
#ifdef USE_NNG_MQTT
    return mqtt_nng_client_close(client);
#else
    return mqtt_c_client_close(client);
#endif
}
