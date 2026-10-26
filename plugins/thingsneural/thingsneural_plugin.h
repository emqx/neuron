/**
 * NEURON IIoT System for Industry 4.0
 * Copyright (C) 2020-2026 EMQ Technologies Co., Ltd All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 **/

#ifndef NEURON_PLUGIN_THINGSNEURAL_H
#define NEURON_PLUGIN_THINGSNEURAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <pthread.h>

#include "neuron.h"

#include "thingsneural_config.h"

typedef struct tn_point {
    char           property[NEU_TAG_NAME_LEN];
    char           node[NEU_NODE_NAME_LEN];
    char           group[NEU_GROUP_NAME_LEN];
    char           tag[NEU_TAG_NAME_LEN];
    UT_hash_handle hh;
} tn_point_t;

struct neu_plugin {
    neu_plugin_common_t common;
    tn_config_t         config;
    neu_mqtt_client_t * client;
    tn_point_t *        points;
    pthread_mutex_t     lock;
    uint64_t            msg_seq;
};

void tn_points_free(tn_point_t **points);
void tn_points_rename_node(tn_point_t *points, const char *node,
                           const char *new_name);
void tn_points_rename_group(tn_point_t *points, const char *node,
                            const char *group, const char *new_name);

#ifdef __cplusplus
}
#endif

#endif
