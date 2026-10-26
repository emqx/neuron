/**
 * NEURON IIoT System for Industry 4.0
 * Copyright (C) 2020-2026 EMQ Technologies Co., Ltd All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 **/

#ifndef NEURON_PLUGIN_THINGSNEURAL_CONFIG_H
#define NEURON_PLUGIN_THINGSNEURAL_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

#include "connection/mqtt_client.h"
#include "neuron.h"

struct tn_point;

typedef struct {
    neu_mqtt_version_e version;
    neu_mqtt_qos_e     qos;
    uint16_t           port;
    bool               ssl;
    char *             client_id;
    char *             host;
    char *             username;
    char *             password;
    char *             product_key;
    char *             device_key;
    char *             report_topic;
    char *             set_req_topic;
    char *             set_resp_topic;
} tn_config_t;

int  tn_config_parse(neu_plugin_t *plugin, const char *setting,
                     tn_config_t *config, struct tn_point **points);
void tn_config_fini(tn_config_t *config);

#ifdef __cplusplus
}
#endif

#endif
