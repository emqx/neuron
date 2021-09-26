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
#include <string.h>

#include "utils/json.h"

#include "neu_json_group_config.h"

int neu_parse_decode_add_group_config(char *                             buf,
                                      neu_parse_add_group_config_req_t **result)
{
    neu_parse_add_group_config_req_t *req =
        malloc(sizeof(neu_parse_add_group_config_req_t));
    memset(req, 0, sizeof(neu_parse_add_group_config_req_t));

    neu_json_elem_t elem[] = {
        {
            .name = "node_id",
            .t    = NEU_JSON_INT,
        },
        {
            .name = "interval",
            .t    = NEU_JSON_INT,
        },
        {
            .name = "name",
            .t    = NEU_JSON_STR,
        },
    };

    int ret = neu_json_decode(buf, NEU_JSON_ELEM_SIZE(elem), elem);
    if (ret != 0) {
        free(req);
        return -1;
    }
    req->node_id  = elem[0].v.val_int;
    req->interval = elem[1].v.val_int;
    req->name     = elem[2].v.val_str;

    *result = req;
    return 0;
}

void neu_parse_decode_add_group_config_free(
    neu_parse_add_group_config_req_t *req)
{
    free(req->name);
    free(req);
}

int neu_parse_decode_update_group_config(
    char *buf, neu_parse_update_group_config_req_t **result)
{
    neu_parse_update_group_config_req_t *req =
        malloc(sizeof(neu_parse_update_group_config_req_t));
    memset(req, 0, sizeof(neu_parse_update_group_config_req_t));

    neu_json_elem_t elem[] = { {
                                   .name = "name",
                                   .t    = NEU_JSON_STR,
                               },
                               {
                                   .name = "node_id",
                                   .t    = NEU_JSON_INT,
                               },
                               {
                                   .name = "interval",
                                   .t    = NEU_JSON_INT,
                               } };

    int ret = neu_json_decode(buf, NEU_JSON_ELEM_SIZE(elem), elem);
    if (ret != 0) {
        free(req);
        return -1;
    }
    req->name     = elem[0].v.val_str;
    req->node_id  = elem[1].v.val_int;
    req->interval = elem[2].v.val_int;

    *result = req;
    return 0;
}

void neu_parse_decode_update_group_config_free(
    neu_parse_update_group_config_req_t *req)
{
    free(req->name);
    free(req);
}

int neu_parse_decode_del_group_config(char *                             buf,
                                      neu_parse_del_group_config_req_t **result)
{
    neu_parse_del_group_config_req_t *req =
        malloc(sizeof(neu_parse_del_group_config_req_t));
    memset(req, 0, sizeof(neu_parse_del_group_config_req_t));

    neu_json_elem_t elem[] = { {
                                   .name = "name",
                                   .t    = NEU_JSON_STR,
                               },
                               {
                                   .name = "node_id",
                                   .t    = NEU_JSON_INT,
                               }

    };

    int ret = neu_json_decode(buf, NEU_JSON_ELEM_SIZE(elem), elem);
    if (ret != 0) {
        free(req);
        return -1;
    }
    req->name    = elem[0].v.val_str;
    req->node_id = elem[1].v.val_int;

    *result = req;
    return 0;
}

void neu_parse_decode_del_group_config_free(
    neu_parse_del_group_config_req_t *req)
{
    free(req->name);
    free(req);
}

int neu_parse_decode_get_group_config(char *                             buf,
                                      neu_parse_get_group_config_req_t **result)
{
    neu_parse_get_group_config_req_t *req =
        malloc(sizeof(neu_parse_get_group_config_req_t));
    memset(req, 0, sizeof(neu_parse_get_group_config_req_t));

    neu_json_elem_t elem[] = {
        {
            .name = "node_id",
            .t    = NEU_JSON_INT,
        },
        {
            .name = "name",
            .t    = NEU_JSON_STR,
        },
    };

    int ret = neu_json_decode(buf, NEU_JSON_ELEM_SIZE(elem), elem);
    if (ret != 0) {
        free(req);
        return -1;
    }

    req->node_id = elem[0].v.val_int;
    req->name    = elem[1].v.val_str;

    *result = req;
    return 0;
}

void neu_parse_decode_get_group_config_free(
    neu_parse_get_group_config_req_t *req)
{
    free(req->name);
    free(req);
}

int neu_parse_encode_get_group_config(void *json_object, void *param)
{
    neu_parse_get_group_config_res_t *res =
        (neu_parse_get_group_config_res_t *) param;
    void *array = NULL;

    for (int i = 0; i < res->n_config; i++) {
        neu_json_elem_t group[] = { {
                                        .name      = "name",
                                        .t         = NEU_JSON_STR,
                                        .v.val_str = res->rows[i].name,
                                    },
                                    {
                                        .name      = "interval",
                                        .t         = NEU_JSON_INT,
                                        .v.val_int = res->rows[i].interval,
                                    },
                                    {
                                        .name      = "pipe_count",
                                        .t         = NEU_JSON_INT,
                                        .v.val_int = res->rows[i].pipe_count,
                                    },
                                    {
                                        .name      = "tag_count",
                                        .t         = NEU_JSON_INT,
                                        .v.val_int = res->rows[i].tag_count,
                                    } };
        array                   = neu_json_encode_array(array, group, 4);
    }
    neu_json_elem_t elems[] = { {
        .name     = "group_configs",
        .t        = NEU_JSON_OBJECT,
        .v.object = array,
    } };

    return neu_json_encode_field(json_object, elems, 1);
}
