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

#include "neu_json_plugin.h"
#include "utils/json.h"

#define KIND "kind"
#define NODE_TYPE "node_type"
#define PLUGIN_NAME "plugin_type"
#define PLUGIN_LIB_NAME "plugin_lib_name"
#define PLUGIN_ID "plugin_id"
#define PLUGIN_NAME "plugin_name"

int neu_parse_decode_add_plugin_req(char *                            buf,
                                    struct neu_parse_add_plugin_req **result)
{
    struct neu_parse_add_plugin_req *req =
        calloc(1, sizeof(struct neu_parse_add_plugin_req));
    neu_json_elem_t elem[] = { {
                                   .name = NEU_PARSE_UUID,
                                   .t    = NEU_JSON_STR,
                               },
                               {
                                   .name = KIND,
                                   .t    = NEU_JSON_INT,
                               },
                               {
                                   .name = NODE_TYPE,
                                   .t    = NEU_JSON_INT,
                               },
                               {
                                   .name = PLUGIN_NAME,
                                   .t    = NEU_JSON_STR,
                               },
                               {
                                   .name = PLUGIN_LIB_NAME,
                                   .t    = NEU_JSON_STR,
                               } };

    int ret = neu_json_decode(buf, NEU_JSON_ELEM_SIZE(elem), elem);
    if (ret != 0) {
        free(req);
        return -1;
    }

    req->function        = NEU_PARSE_OP_ADD_PLUGIN;
    req->uuid            = elem[0].v.val_str;
    req->kind            = elem[1].v.val_int;
    req->node_type       = elem[2].v.val_int;
    req->plugin_name     = elem[3].v.val_str;
    req->plugin_lib_name = elem[4].v.val_str;

    *result = req;

    return 0;
}

int neu_parse_encode_add_plugin_res(struct neu_parse_add_plugin_res *res,
                                    char **                          buf)
{
    neu_json_elem_t elems[] = { {
                                    .name      = NEU_PARSE_FUNCTION,
                                    .t         = NEU_JSON_INT,
                                    .v.val_int = NEU_PARSE_OP_ADD_PLUGIN,
                                },
                                {
                                    .name      = NEU_PARSE_UUID,
                                    .t         = NEU_JSON_STR,
                                    .v.val_str = res->uuid,
                                },
                                {
                                    .name      = NEU_PARSE_ERROR,
                                    .t         = NEU_JSON_INT,
                                    .v.val_int = res->error,
                                } };

    return neu_json_encode(elems, 3, buf);
}

int neu_parse_decode_delete_plugin_req(
    char *buf, struct neu_parse_delete_plugin_req **result)
{
    struct neu_parse_delete_plugin_req *req =
        calloc(1, sizeof(struct neu_parse_delete_plugin_req));
    neu_json_elem_t elem[] = { {
                                   .name = NEU_PARSE_UUID,
                                   .t    = NEU_JSON_STR,
                               },
                               {
                                   .name = PLUGIN_ID,
                                   .t    = NEU_JSON_INT,
                               } };

    int ret = neu_json_decode(buf, NEU_JSON_ELEM_SIZE(elem), elem);
    if (ret != 0) {
        free(req);
        return -1;
    }

    req->function  = NEU_PARSE_OP_DELETE_PLUGIN;
    req->uuid      = elem[0].v.val_str;
    req->plugin_id = elem[1].v.val_int;

    *result = req;

    return 0;
}

int neu_parse_encode_delete_plugin_res(struct neu_parse_delete_plugin_res *res,
                                       char **                             buf)
{
    neu_json_elem_t elems[] = { {
                                    .name      = NEU_PARSE_FUNCTION,
                                    .t         = NEU_JSON_INT,
                                    .v.val_int = NEU_PARSE_OP_DELETE_PLUGIN,
                                },
                                {
                                    .name      = NEU_PARSE_UUID,
                                    .t         = NEU_JSON_STR,
                                    .v.val_str = res->uuid,
                                },
                                {
                                    .name      = NEU_PARSE_ERROR,
                                    .t         = NEU_JSON_INT,
                                    .v.val_int = res->error,
                                } };

    return neu_json_encode(elems, 3, buf);
}

int neu_parse_decode_update_plugin_req(
    char *buf, struct neu_parse_update_plugin_req **result)
{
    struct neu_parse_update_plugin_req *req =
        calloc(1, sizeof(struct neu_parse_update_plugin_req));
    neu_json_elem_t elem[] = { {
                                   .name = NEU_PARSE_UUID,
                                   .t    = NEU_JSON_STR,
                               },
                               {
                                   .name = KIND,
                                   .t    = NEU_JSON_INT,
                               },
                               {
                                   .name = NODE_TYPE,
                                   .t    = NEU_JSON_INT,
                               },
                               {
                                   .name = PLUGIN_NAME,
                                   .t    = NEU_JSON_STR,
                               },
                               {
                                   .name = PLUGIN_LIB_NAME,
                                   .t    = NEU_JSON_STR,
                               } };

    int ret = neu_json_decode(buf, NEU_JSON_ELEM_SIZE(elem), elem);
    if (ret != 0) {
        free(req);
        return -1;
    }

    req->function        = NEU_PARSE_OP_UPDATE_PLUGIN;
    req->uuid            = elem[0].v.val_str;
    req->kind            = elem[1].v.val_int;
    req->node_type       = elem[2].v.val_int;
    req->plugin_name     = elem[3].v.val_str;
    req->plugin_lib_name = elem[4].v.val_str;

    *result = req;

    return 0;
}

int neu_parse_encode_update_plugin_res(struct neu_parse_update_plugin_res *res,
                                       char **                             buf)
{
    neu_json_elem_t elems[] = { {
                                    .name      = NEU_PARSE_FUNCTION,
                                    .t         = NEU_JSON_INT,
                                    .v.val_int = NEU_PARSE_OP_UPDATE_PLUGIN,
                                },
                                {
                                    .name      = NEU_PARSE_UUID,
                                    .t         = NEU_JSON_STR,
                                    .v.val_str = res->uuid,
                                },
                                {
                                    .name      = NEU_PARSE_ERROR,
                                    .t         = NEU_JSON_INT,
                                    .v.val_int = res->error,
                                } };

    return neu_json_encode(elems, 3, buf);
}

int neu_parse_decode_get_plugin_req(char *                            buf,
                                    struct neu_parse_get_plugin_req **result)
{
    struct neu_parse_get_plugin_req *req =
        calloc(1, sizeof(struct neu_parse_get_plugin_req));
    neu_json_elem_t elem[] = { {
        .name = NEU_PARSE_UUID,
        .t    = NEU_JSON_STR,
    } };

    int ret = neu_json_decode(buf, NEU_JSON_ELEM_SIZE(elem), elem);
    if (ret != 0) {
        free(req);
        return -1;
    }

    req->function = NEU_PARSE_OP_GET_PLUGIN;
    req->uuid     = elem[0].v.val_str;

    *result = req;

    return 0;
}

int neu_parse_encode_get_plugin_res(struct neu_parse_get_plugin_res *res,
                                    char **                          buf)
{
    neu_json_elem_t elems[] = { {
                                    .name      = NEU_PARSE_FUNCTION,
                                    .t         = NEU_JSON_INT,
                                    .v.val_int = res->function,
                                },
                                {
                                    .name      = NEU_PARSE_UUID,
                                    .t         = NEU_JSON_STR,
                                    .v.val_str = res->uuid,
                                },
                                {
                                    .name      = PLUGIN_ID,
                                    .t         = NEU_JSON_INT,
                                    .v.val_int = res->plugin_id,
                                },
                                {
                                    .name      = KIND,
                                    .t         = NEU_JSON_INT,
                                    .v.val_int = res->kind,
                                },
                                {
                                    .name      = NODE_TYPE,
                                    .t         = NEU_JSON_INT,
                                    .v.val_int = res->node_type,
                                },
                                {
                                    .name      = PLUGIN_NAME,
                                    .t         = NEU_JSON_STR,
                                    .v.val_str = res->plugin_name,
                                },
                                {
                                    .name      = PLUGIN_LIB_NAME,
                                    .t         = NEU_JSON_STR,
                                    .v.val_str = res->plugin_lib_name,
                                } };

    return neu_json_encode(elems, 7, buf);
}

void neu_parse_decode_add_plugin_free(struct neu_parse_add_plugin_req *req)
{
    free(req->uuid);
    free(req->plugin_name);
    free(req->plugin_lib_name);
}

void neu_parse_decode_delete_plugin_free(
    struct neu_parse_delete_plugin_req *req)
{
    free(req->uuid);
}

void neu_parse_decode_update_plugin_free(
    struct neu_parse_update_plugin_req *req)
{
    free(req->uuid);
    free(req->plugin_name);
    free(req->plugin_lib_name);
}

void neu_parse_decode_get_plugin_free(struct neu_parse_get_plugin_req *req)
{
    free(req->uuid);
}