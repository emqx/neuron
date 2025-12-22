/**
 * NEURON IIoT System for Industry 4.0
 * Copyright (C) 2020-2023 EMQ Technologies Co., Ltd All rights reserved.
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

#ifndef _NEU_JSON_API_NEU_JSON_GLOBAL_CONFIG_H_
#define _NEU_JSON_API_NEU_JSON_GLOBAL_CONFIG_H_

#include "json/json.h"

#include "parser/neu_json_group_config.h"
#include "parser/neu_json_node.h"
#include "parser/neu_json_tag.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int                      n_tag;
    neu_json_add_tags_req_t *tags;
} neu_json_global_config_req_tags_t;

int neu_json_decode_global_config_req_tags(
    void *json_obj, neu_json_global_config_req_tags_t **result);
void neu_json_decode_global_config_req_tags_free(
    neu_json_global_config_req_tags_t *tags);

typedef struct {
    int                       n_subscription;
    neu_json_subscribe_req_t *subscriptions;
} neu_json_global_config_req_subscriptions_t;

int neu_json_decode_global_config_req_subscriptions(
    void *json_obj, neu_json_global_config_req_subscriptions_t **result);
void neu_json_decode_global_config_req_subscriptions_free(
    neu_json_global_config_req_subscriptions_t *req_sub);

typedef struct {
    int                          n_setting;
    neu_json_node_setting_req_t *settings;
} neu_json_global_config_req_settings_t;

int neu_json_decode_global_config_req_settings(
    void *json_obj, neu_json_global_config_req_settings_t **result);
void neu_json_decode_global_config_req_settings_free(
    neu_json_global_config_req_settings_t *req_settings);

typedef struct {
    neu_json_get_nodes_resp_t *                 nodes;
    neu_json_get_driver_group_resp_t *          groups;
    neu_json_global_config_req_tags_t *         tags;
    neu_json_global_config_req_subscriptions_t *subscriptions;
    neu_json_global_config_req_settings_t *     settings;
} neu_json_global_config_req_t;

int  neu_json_decode_global_config_req(char *                         buf,
                                       neu_json_global_config_req_t **result);
void neu_json_decode_global_config_req_free(neu_json_global_config_req_t *req);

typedef struct {
    neu_json_add_node_req_t node;
    neu_json_gtag_array_t   gtags;
} neu_json_driver_t;

int  neu_json_encode_driver(void *node_json, void *param);
int  neu_json_decode_driver_json(void *node_json, neu_json_driver_t *driver_p);
void neu_json_driver_fini(neu_json_driver_t *req);

typedef struct {
    int                n_driver;
    neu_json_driver_t *drivers;
} neu_json_driver_array_t;

int  neu_json_encode_driver_array(void *obj_json, void *param);
int  neu_json_decode_driver_array_json(void *                   obj_json,
                                       neu_json_driver_array_t *arr);
void neu_json_driver_array_fini(neu_json_driver_array_t *arr);

typedef neu_json_driver_array_t neu_json_drivers_req_t;

int  neu_json_encode_drivers_req(void *obj_json, void *param);
int  neu_json_decode_drivers_req(char *buf, neu_json_drivers_req_t **result);
void neu_json_decode_drivers_req_free(neu_json_drivers_req_t *req);

typedef struct {
    char *driver;
    char *group;
    char *params;
} neu_json_app_group_t;

typedef struct {
    int                   n_group;
    neu_json_app_group_t *groups;
} neu_json_app_group_array_t;

typedef struct {
    neu_json_add_node_req_t    node;
    neu_json_app_group_array_t subscriptions;
} neu_json_app_t;

int  neu_json_encode_app(void *node_json, void *param);
int  neu_json_decode_app_json(void *node_json, neu_json_app_t *app_p);
void neu_json_app_fini(neu_json_app_t *app);

typedef struct {
    int             n_app;
    neu_json_app_t *apps;
} neu_json_app_array_t;

int  neu_json_encode_app_array(void *obj_json, void *param);
int  neu_json_decode_app_array_json(void *obj_json, neu_json_app_array_t *arr);
void neu_json_app_array_fini(neu_json_app_array_t *arr);

typedef neu_json_app_array_t neu_json_apps_req_t;

int  neu_json_encode_apps_req(void *obj_json, void *param);
int  neu_json_decode_apps_req(char *buf, neu_json_apps_req_t **result);
void neu_json_decode_apps_req_free(neu_json_apps_req_t *req);

#ifdef __cplusplus
}
#endif

#endif
