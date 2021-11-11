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

#include <stdlib.h>

#include "json/json.h"

#include "json/neu_json_plugin.h"

int neu_parse_decode_add_plugin(char *buf, neu_parse_add_plugin_req_t **result)
{
    neu_parse_add_plugin_req_t *req =
        calloc(1, sizeof(neu_parse_add_plugin_req_t));
    neu_json_elem_t elem[] = { {
                                   .name = "kind",
                                   .t    = NEU_JSON_INT,
                               },
                               {
                                   .name = "node_type",
                                   .t    = NEU_JSON_INT,
                               },
                               {
                                   .name = "name",
                                   .t    = NEU_JSON_STR,
                               },
                               {
                                   .name = "lib_name",
                                   .t    = NEU_JSON_STR,
                               } };

    int ret = neu_json_decode(buf, NEU_JSON_ELEM_SIZE(elem), elem);
    if (ret != 0) {
        free(req);
        return -1;
    }

    req->kind            = elem[0].v.val_int;
    req->node_type       = elem[1].v.val_int;
    req->plugin_name     = elem[2].v.val_str;
    req->plugin_lib_name = elem[3].v.val_str;

    *result = req;

    return 0;
}

void neu_parse_decode_add_plugin_free(neu_parse_add_plugin_req_t *req)
{
    free(req->plugin_name);
    free(req->plugin_lib_name);
    free(req);
}

int neu_parse_decode_del_plugin(char *buf, neu_parse_del_plugin_req_t **result)
{
    neu_parse_del_plugin_req_t *req =
        calloc(1, sizeof(neu_parse_del_plugin_req_t));
    neu_json_elem_t elem[] = { {
        .name = "id",
        .t    = NEU_JSON_INT,
    } };

    int ret = neu_json_decode(buf, NEU_JSON_ELEM_SIZE(elem), elem);
    if (ret != 0) {
        free(req);
        return -1;
    }

    req->plugin_id = elem[0].v.val_int;

    *result = req;

    return 0;
}

void neu_parse_decode_del_plugin_free(neu_parse_del_plugin_req_t *req)
{
    free(req);
}

int neu_parse_decode_update_plugin(char *                          buf,
                                   neu_parse_update_plugin_req_t **result)
{
    neu_parse_update_plugin_req_t *req =
        calloc(1, sizeof(neu_parse_update_plugin_req_t));
    neu_json_elem_t elem[] = { {
                                   .name = "kind",
                                   .t    = NEU_JSON_INT,
                               },
                               {
                                   .name = "node_type",
                                   .t    = NEU_JSON_INT,
                               },
                               {
                                   .name = "name",
                                   .t    = NEU_JSON_STR,
                               },
                               {
                                   .name = "lib_name",
                                   .t    = NEU_JSON_STR,
                               } };

    int ret = neu_json_decode(buf, NEU_JSON_ELEM_SIZE(elem), elem);
    if (ret != 0) {
        free(req);
        return -1;
    }

    req->kind            = elem[0].v.val_int;
    req->node_type       = elem[1].v.val_int;
    req->plugin_name     = elem[2].v.val_str;
    req->plugin_lib_name = elem[3].v.val_str;

    *result = req;

    return 0;
}

void neu_parse_decode_update_plugin_free(neu_parse_update_plugin_req_t *req)
{
    free(req->plugin_name);
    free(req->plugin_lib_name);
    free(req);
}

int neu_parse_decode_get_plugins(char *                        buf,
                                 neu_parse_get_plugins_req_t **result)
{
    (void) buf;
    neu_parse_get_plugins_req_t *req =
        calloc(1, sizeof(neu_parse_get_plugins_req_t));

    *result = req;

    return 0;
}

void neu_parse_decode_get_plugins_free(neu_parse_get_plugins_req_t *req)
{
    free(req);
}

int neu_parse_encode_get_plugins(void *json_object, void *param)
{
    neu_parse_get_plugins_res_t *res   = (neu_parse_get_plugins_res_t *) param;
    void *                       array = NULL;

    for (int i = 0; i < res->n_plugin; i++) {
        neu_json_elem_t plugin_libs[] = {
            {
                .name      = "id",
                .t         = NEU_JSON_INT,
                .v.val_int = res->plugin_libs[i].plugin_id,
            },
            {
                .name      = "kind",
                .t         = NEU_JSON_INT,
                .v.val_int = res->plugin_libs[i].kind,
            },
            {
                .name      = "node_type",
                .t         = NEU_JSON_INT,
                .v.val_int = res->plugin_libs[i].node_type,
            },
            {
                .name      = "name",
                .t         = NEU_JSON_STR,
                .v.val_str = res->plugin_libs[i].plugin_name,
            },
            {
                .name      = "lib_name",
                .t         = NEU_JSON_STR,
                .v.val_str = res->plugin_libs[i].plugin_lib_name,
            }
        };
        array = neu_json_encode_array(array, plugin_libs, 5);
    }
    neu_json_elem_t elems[] = { {
        .name         = "plugin_libs",
        .t            = NEU_JSON_OBJECT,
        .v.val_object = array,
    } };

    return neu_json_encode_field(json_object, elems, 1);
}
