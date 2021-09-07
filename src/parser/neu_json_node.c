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

#include "neu_json_node.h"
#include "utils/json.h"

#define NODE_TYPE "node_type"
#define ADAPTER_NAME "adapter_name"
#define PLUGIN_NAME "plugin_name"
#define NDOE_ID "node_id"
#define ID "id"
#define NAME "name"
#define NODES "nodes"

int neu_parse_decode_add_nodes_req(char *                           buf,
                                   struct neu_parse_add_nodes_req **result)
{
    struct neu_parse_add_nodes_req *req =
        calloc(1, sizeof(struct neu_parse_add_nodes_req));
    neu_json_elem_t elem[] = { {
                                   .name = NEU_PARSE_UUID,
                                   .t    = NEU_JSON_STR,
                               },
                               {
                                   .name = NODE_TYPE,
                                   .t    = NEU_JSON_INT,
                               },
                               {
                                   .name = ADAPTER_NAME,
                                   .t    = NEU_JSON_STR,
                               },
                               {
                                   .name = PLUGIN_NAME,
                                   .t    = NEU_JSON_STR,
                               } };

    int ret = neu_json_decode(buf, NEU_JSON_ELEM_SIZE(elem), elem);
    if (ret != 0) {
        free(req);
        return -1;
    }
    req->function     = NEU_PARSE_OP_ADD_NODES;
    req->uuid         = elem[0].v.val_str;
    req->node_type    = elem[1].v.val_int;
    req->adapter_name = elem[2].v.val_str;
    req->plugin_name  = elem[3].v.val_str;

    *result = req;

    return 0;
}

int neu_parse_encode_add_nodes_res(struct neu_parse_add_nodes_res *res,
                                   char **                         buf)
{
    neu_json_elem_t elems[] = { {
                                    .name      = NEU_PARSE_FUNCTION,
                                    .t         = NEU_JSON_INT,
                                    .v.val_int = NEU_PARSE_OP_ADD_NODES,
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

int neu_parse_decode_get_nodes_req(char *                           buf,
                                   struct neu_parse_get_nodes_req **result)
{
    struct neu_parse_get_nodes_req *req =
        calloc(1, sizeof(struct neu_parse_get_nodes_req));
    neu_json_elem_t elem[] = { {
                                   .name = NEU_PARSE_UUID,
                                   .t    = NEU_JSON_STR,
                               },
                               {
                                   .name = NODE_TYPE,
                                   .t    = NEU_JSON_INT,
                               } };

    int ret = neu_json_decode(buf, NEU_JSON_ELEM_SIZE(elem), elem);
    if (ret != 0) {
        free(req);
        return -1;
    }

    req->function  = NEU_PARSE_OP_GET_NODES;
    req->uuid      = elem[0].v.val_str;
    req->node_type = elem[1].v.val_int;

    *result = req;

    return 0;
}

int neu_parse_encode_get_nodes_res(struct neu_parse_get_nodes_res *res,
                                   char **                         buf)
{
    void *array = NULL;
    for (int i = 0; i < res->n_node; i++) {
        neu_json_elem_t nodes[] = { {
                                        .name      = NAME,
                                        .t         = NEU_JSON_STR,
                                        .v.val_str = res->nodes[i].name,
                                    },
                                    {
                                        .name      = ID,
                                        .t         = NEU_JSON_INT,
                                        .v.val_int = res->nodes[i].id,
                                    } };
        array                   = neu_json_encode_array(array, nodes, 2);
    }
    neu_json_elem_t elems[] = { {
                                    .name      = NEU_PARSE_FUNCTION,
                                    .t         = NEU_JSON_INT,
                                    .v.val_int = NEU_PARSE_OP_GET_NODES,
                                },
                                {
                                    .name      = NEU_PARSE_UUID,
                                    .t         = NEU_JSON_STR,
                                    .v.val_str = res->uuid,
                                },
                                {
                                    .name     = NODES,
                                    .t        = NEU_JSON_OBJECT,
                                    .v.object = array,
                                } };
    return neu_json_encode(elems, 3, buf);
}

int neu_parse_decode_delete_nodes_req(
    char *buf, struct neu_parse_delete_nodes_req **result)
{
    struct neu_parse_delete_nodes_req *req =
        calloc(1, sizeof(struct neu_parse_delete_nodes_req));
    neu_json_elem_t elem[] = { {
                                   .name = NEU_PARSE_UUID,
                                   .t    = NEU_JSON_STR,
                               },
                               {
                                   .name = NDOE_ID,
                                   .t    = NEU_JSON_INT,
                               } };

    int ret = neu_json_decode(buf, NEU_JSON_ELEM_SIZE(elem), elem);
    if (ret != 0) {
        free(req);
        return -1;
    }

    req->function = NEU_PARSE_OP_DELETE_NODES;
    req->uuid     = elem[0].v.val_str;
    req->node_id  = elem[1].v.val_int;

    *result = req;

    return 0;
}

int neu_parse_encode_delete_nodes_res(struct neu_parse_delete_nodes_res *res,
                                      char **                            buf)
{
    neu_json_elem_t elems[] = { {
                                    .name = NEU_PARSE_FUNCTION,
                                    .t    = NEU_JSON_INT,
                                },
                                {
                                    .name = NEU_PARSE_UUID,
                                    .t    = NEU_JSON_STR,
                                },
                                {
                                    .name = NEU_PARSE_ERROR,
                                    .t    = res->error,
                                } };

    return neu_json_encode(elems, 3, buf);
}

int neu_parse_decode_update_nodes_req(
    char *buf, struct neu_parse_update_nodes_req **result)
{
    struct neu_parse_update_nodes_req *req =
        calloc(1, sizeof(struct neu_parse_update_nodes_req));
    neu_json_elem_t elem[] = { {
                                   .name = NEU_PARSE_UUID,
                                   .t    = NEU_JSON_STR,
                               },
                               {
                                   .name = NODE_TYPE,
                                   .t    = NEU_JSON_INT,
                               },
                               {
                                   .name = ADAPTER_NAME,
                                   .t    = NEU_JSON_STR,
                               },
                               {
                                   .name = PLUGIN_NAME,
                                   .t    = NEU_JSON_STR,
                               } };

    int ret = neu_json_decode(buf, NEU_JSON_ELEM_SIZE(elem), elem);
    if (ret != 0) {
        free(req);
        return -1;
    }
    req->function     = NEU_PARSE_OP_UPDATE_NODES;
    req->uuid         = elem[0].v.val_str;
    req->node_type    = elem[1].v.val_int;
    req->adapter_name = elem[2].v.val_str;
    req->plugin_name  = elem[3].v.val_str;

    *result = req;

    return 0;
}

int neu_parse_encode_update_nodes_res(struct neu_parse_update_nodes_res *res,
                                      char **                            buf)
{
    neu_json_elem_t elems[] = {
        {
            .name      = NEU_PARSE_FUNCTION,
            .t         = NEU_JSON_INT,
            .v.val_int = NEU_PARSE_OP_UPDATE_NODES,
        },
        { .name = NEU_PARSE_UUID, .t = NEU_JSON_STR, .v.val_str = res->uuid },
        {
            .name      = NEU_PARSE_ERROR,
            .t         = NEU_JSON_INT,
            .v.val_int = res->error,
        }
    };

    return neu_json_encode(elems, 3, buf);
}
