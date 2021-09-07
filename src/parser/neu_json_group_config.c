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

#define GROUP "group"
#define NAMES "names"
#define NAME "name"
#define TYPE "type"
#define TIMESTAMP "timestamp"
#define VALUE "value"
#define TAGS "tags"

int neu_paser_decode_get_group_config_req(
    char *buf, struct neu_paser_get_group_config_req **result)
{
    struct neu_paser_get_group_config_req *req =
        malloc(sizeof(struct neu_paser_get_group_config_req));
    memset(req, 0, sizeof(struct neu_paser_get_group_config_req));

    neu_json_elem_t elem[] = { {
                                   .name = NEU_PARSE_UUID,
                                   .t    = NEU_JSON_STR,
                               },
                               {
                                   .name = "nodeId",
                                   .t    = NEU_JSON_INT,
                               },
                               {
                                   .name = "groupConfig",
                                   .t    = NEU_JSON_STR,
                               }

    };

    int ret = neu_json_decode(buf, NEU_JSON_ELEM_SIZE(elem), elem);
    if (ret != 0) {
        free(req);
        return -1;
    }
    req->function = NEU_PARSE_OP_GET_GROUP_CONFIG;
    req->uuid     = elem[0].v.val_str;
    req->node_id  = elem[1].v.val_int;
    req->config   = elem[2].v.val_str;

    *result = req;
    return 0;
}

int neu_parse_encode_get_group_config_res(
    struct neu_paser_get_group_config_res *res, char **buf)
{
    void *array = NULL;
    for (int i = 0; i < res->n_config; i++) {
        neu_json_elem_t group[] = { {
                                        .name      = NAME,
                                        .t         = NEU_JSON_STR,
                                        .v.val_str = res->rows[i].name,
                                    },
                                    {
                                        .name      = "nodeID",
                                        .t         = NEU_JSON_INT,
                                        .v.val_int = res->rows[i].node_id,
                                    },
                                    {
                                        .name      = "readInterval",
                                        .t         = NEU_JSON_INT,
                                        .v.val_int = res->rows[i].read_interval,
                                    },
                                    {
                                        .name      = "pipeCount",
                                        .t         = NEU_JSON_INT,
                                        .v.val_int = res->rows[i].pipe_count,
                                    },
                                    {
                                        .name      = "tagCount",
                                        .t         = NEU_JSON_INT,
                                        .v.val_int = res->rows[i].tag_count,
                                    } };
        array                   = neu_json_encode_array(array, group, 5);
    }
    neu_json_elem_t elems[] = { {
                                    .name      = NEU_PARSE_FUNCTION,
                                    .t         = NEU_JSON_INT,
                                    .v.val_int = NEU_PARSE_OP_GET_GROUP_CONFIG,
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
                                },
                                {
                                    .name     = "groupConfigs",
                                    .t        = NEU_JSON_OBJECT,
                                    .v.object = array,
                                } };
    return neu_json_encode(elems, 4, buf);
}

void neu_parse_encode_get_group_config_free(
    struct neu_paser_get_group_config_req *req)
{
    if (NULL != req) {
        free(req->uuid);
        free(req->config);
    }
}

int neu_paser_decode_add_group_config_req(
    char *buf, struct neu_paser_add_group_config_req **result)
{
    struct neu_paser_add_group_config_req *req =
        malloc(sizeof(struct neu_paser_add_group_config_req));
    memset(req, 0, sizeof(struct neu_paser_add_group_config_req));

    neu_json_elem_t elem[] = { {
                                   .name = NEU_PARSE_UUID,
                                   .t    = NEU_JSON_STR,
                               },
                               {
                                   .name = "groupConfig",
                                   .t    = NEU_JSON_STR,
                               },
                               {
                                   .name = "srcNodeId",
                                   .t    = NEU_JSON_INT,
                               },
                               {
                                   .name = "dstNodeId",
                                   .t    = NEU_JSON_INT,
                               },
                               {
                                   .name = "readInterval",
                                   .t    = NEU_JSON_INT,
                               } };

    int ret = neu_json_decode(buf, NEU_JSON_ELEM_SIZE(elem), elem);
    if (ret != 0) {
        free(req);
        return -1;
    }
    req->function      = NEU_PARSE_OP_ADD_GROUP_CONFIG;
    req->uuid          = elem[0].v.val_str;
    req->config        = elem[1].v.val_str;
    req->src_node_id   = elem[2].v.val_int;
    req->dst_node_id   = elem[3].v.val_int;
    req->read_interval = elem[4].v.val_int;

    *result = req;
    return 0;
}

int neu_parse_encode_add_group_config_res(
    struct neu_paser_group_config_res *res, char **buf)
{
    neu_json_elem_t elems[] = { {
                                    .name      = NEU_PARSE_FUNCTION,
                                    .t         = NEU_JSON_INT,
                                    .v.val_int = NEU_PARSE_OP_ADD_GROUP_CONFIG,
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

int neu_paser_decode_update_group_config_req(
    char *buf, struct neu_paser_update_group_config_req **result)
{
    struct neu_paser_update_group_config_req *req =
        malloc(sizeof(struct neu_paser_update_group_config_req));
    memset(req, 0, sizeof(struct neu_paser_update_group_config_req));

    neu_json_elem_t elem[] = { {
                                   .name = NEU_PARSE_UUID,
                                   .t    = NEU_JSON_STR,
                               },
                               {
                                   .name = "groupConfig",
                                   .t    = NEU_JSON_STR,
                               },
                               {
                                   .name = "srcNodeId",
                                   .t    = NEU_JSON_INT,
                               },
                               {
                                   .name = "dstNodeId",
                                   .t    = NEU_JSON_INT,
                               },
                               {
                                   .name = "readInterval",
                                   .t    = NEU_JSON_INT,
                               } };

    int ret = neu_json_decode(buf, NEU_JSON_ELEM_SIZE(elem), elem);
    if (ret != 0) {
        free(req);
        return -1;
    }
    req->function      = NEU_PARSE_OP_UPDATE_GROUP_CONFIG;
    req->uuid          = elem[0].v.val_str;
    req->config        = elem[1].v.val_str;
    req->src_node_id   = elem[2].v.val_int;
    req->dst_node_id   = elem[3].v.val_int;
    req->read_interval = elem[4].v.val_int;

    *result = req;
    return 0;
}

int neu_parse_encode_update_group_config_res(
    struct neu_paser_group_config_res *res, char **buf)
{
    neu_json_elem_t elems[] = { {
                                    .name = NEU_PARSE_FUNCTION,
                                    .t    = NEU_JSON_INT,
                                    .v.val_int =
                                        NEU_PARSE_OP_UPDATE_GROUP_CONFIG,
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

int neu_paser_decode_delete_group_config_req(
    char *buf, struct neu_paser_delete_group_config_req **result)
{
    struct neu_paser_delete_group_config_req *req =
        malloc(sizeof(struct neu_paser_delete_group_config_req));
    memset(req, 0, sizeof(struct neu_paser_delete_group_config_req));

    neu_json_elem_t elem[] = { {
                                   .name = NEU_PARSE_UUID,
                                   .t    = NEU_JSON_STR,
                               },
                               {
                                   .name = "groupConfig",
                                   .t    = NEU_JSON_STR,
                               },
                               {
                                   .name = "nodeId",
                                   .t    = NEU_JSON_INT,
                               }

    };

    int ret = neu_json_decode(buf, NEU_JSON_ELEM_SIZE(elem), elem);
    if (ret != 0) {
        free(req);
        return -1;
    }
    req->function = NEU_PARSE_OP_DELETE_GROUP_CONFIG;
    req->uuid     = elem[0].v.val_str;
    req->config   = elem[1].v.val_str;
    req->node_id  = elem[2].v.val_int;

    *result = req;
    return 0;
}

int neu_parse_encode_delete_group_config_res(
    struct neu_paser_group_config_res *res, char **buf)
{
    neu_json_elem_t elems[] = { {
                                    .name = NEU_PARSE_FUNCTION,
                                    .t    = NEU_JSON_INT,
                                    .v.val_int =
                                        NEU_PARSE_OP_DELETE_GROUP_CONFIG,
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

void neu_paser_decode_add_group_config_free(
    struct neu_paser_add_group_config_req *req)
{
    if (NULL != req) {
        free(req->uuid);
        free(req->config);
    }
}

void neu_paser_decode_update_group_config_free(
    struct neu_paser_update_group_config_req *req)
{
    if (NULL != req) {
        free(req->uuid);
        free(req->config);
    }
}

void neu_paser_decode_delete_group_config_free(
    struct neu_paser_delete_group_config_req *req)
{
    if (NULL != req) {
        free(req->uuid);
        free(req->config);
    }
}