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

#include "neu_json_tag.h"
#include "utils/json.h"

#define TAGS "tags"
#define NAME "name"
#define TYPE "type"
#define ADDRESS "address"
#define ATTRIBUTE "attribute"
#define GROUP_CONFIG_NAME "group_config_name"
#define NODE_ID "node_id"
#define TAG_ID "tag_id"
#define TAG_IDS "tag_ids"

int neu_parse_decode_add_tags_req(char *                          buf,
                                  struct neu_parse_add_tags_req **result)
{
    struct neu_parse_add_tags_req *req =
        calloc(1, sizeof(struct neu_parse_add_tags_req));
    neu_json_elem_t elem[] = {
        {
            .name = NEU_PARSE_UUID,
            .t    = NEU_JSON_STR,
        },
        {
            .name = NODE_ID,
            .t    = NEU_JSON_INT,
        },
        {
            .name = GROUP_CONFIG_NAME,
            .t    = NEU_JSON_STR,
        },
    };

    int ret = neu_json_decode(buf, NEU_JSON_ELEM_SIZE(elem), elem);
    if (ret != 0) {
        free(req);
        return -1;
    }
    req->function          = NEU_PARSE_OP_ADD_TAGS;
    req->uuid              = elem[0].v.val_str;
    req->node_id           = elem[1].v.val_int;
    req->group_config_name = elem[2].v.val_str;

    req->n_tag = neu_json_decode_array_size(buf, TAGS);
    req->tags  = calloc(req->n_tag, sizeof(struct neu_parse_add_tags_req_tag));
    for (int i = 0; i < req->n_tag; i++) {
        neu_json_elem_t celem[] = { {
                                        .name = NAME,
                                        .t    = NEU_JSON_STR,
                                    },
                                    {
                                        .name = TYPE,
                                        .t    = NEU_JSON_INT,
                                    },
                                    {
                                        .name = ADDRESS,
                                        .t    = NEU_JSON_STR,
                                    },
                                    {
                                        .name = ATTRIBUTE,
                                        .t    = NEU_JSON_INT,
                                    } };

        neu_json_decode_array(buf, TAGS, i, NEU_JSON_ELEM_SIZE(celem), celem);
        req->tags[i].name      = celem[0].v.val_str;
        req->tags[i].type      = celem[1].v.val_int;
        req->tags[i].address   = celem[2].v.val_str;
        req->tags[i].attribute = celem[3].v.val_int;
    }

    *result = req;

    return 0;
}

int neu_parse_encode_add_tags_res(struct neu_parse_add_tags_res *res,
                                  char **                        buf)
{
    neu_json_elem_t elems[] = { {
                                    .name      = NEU_PARSE_FUNCTION,
                                    .t         = NEU_JSON_INT,
                                    .v.val_int = NEU_PARSE_OP_ADD_TAGS,
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

int neu_parse_decode_delete_tags_req(char *                             buf,
                                     struct neu_parse_delete_tags_req **result)
{
    struct neu_parse_delete_tags_req *req =
        calloc(1, sizeof(struct neu_parse_delete_tags_req));
    neu_json_elem_t elem[] = {
        {
            .name = NEU_PARSE_UUID,
            .t    = NEU_JSON_STR,
        },
        {
            .name = NODE_ID,
            .t    = NEU_JSON_INT,
        },
        {
            .name = GROUP_CONFIG_NAME,
            .t    = NEU_JSON_STR,
        },
    };

    int ret = neu_json_decode(buf, NEU_JSON_ELEM_SIZE(elem), elem);
    if (ret != 0) {
        free(req);
        return -1;
    }
    req->function          = NEU_PARSE_OP_DELETE_TAGS;
    req->uuid              = elem[0].v.val_str;
    req->node_id           = elem[1].v.val_int;
    req->group_config_name = elem[2].v.val_str;

    req->n_tag_id = neu_json_decode_array_size(buf, TAG_IDS);
    req->tag_ids  = calloc(req->n_tag_id, sizeof(uint32_t));
    for (int i = 0; i < req->n_tag_id; i++) {
        neu_json_elem_t celem[] = {
            {
                .name = NULL,
                .t    = NEU_JSON_INT,
            },
        };

        neu_json_decode_array(buf, TAG_IDS, i, NEU_JSON_ELEM_SIZE(celem),
                              celem);
        req->tag_ids[i] = celem[0].v.val_int;
    }

    *result = req;

    return 0;
}

int neu_parse_encode_delete_tags_res(struct neu_parse_delete_tags_res *res,
                                     char **                           buf)
{
    neu_json_elem_t elems[] = { {
                                    .name      = NEU_PARSE_FUNCTION,
                                    .t         = NEU_JSON_INT,
                                    .v.val_int = NEU_PARSE_OP_DELETE_TAGS,
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

int neu_parse_decode_get_tags_req(char *                          buf,
                                  struct neu_parse_get_tags_req **result)
{
    struct neu_parse_get_tags_req *req =
        calloc(1, sizeof(struct neu_parse_get_tags_req));
    neu_json_elem_t elem[] = {
        {
            .name = NEU_PARSE_UUID,
            .t    = NEU_JSON_STR,

        },
        {
            .name = NODE_ID,
            .t    = NEU_JSON_INT,
        },
    };

    int ret = neu_json_decode(buf, NEU_JSON_ELEM_SIZE(elem), elem);
    if (ret != 0) {
        free(req);
        return -1;
    }
    req->function = NEU_PARSE_OP_GET_TAGS;
    req->uuid     = elem[0].v.val_str;
    req->node_id  = elem[1].v.val_int;

    *result = req;

    return 0;
}

int neu_parse_encode_get_tags_res(struct neu_parse_get_tags_res *res,
                                  char **                        buf)
{
    void *array = NULL;
    for (int i = 0; i < res->n_tag; i++) {
        neu_json_elem_t tag[] = {
            {
                .name      = NAME,
                .t         = NEU_JSON_STR,
                .v.val_str = res->tags[i].name,
            },
            {
                .name      = TYPE,
                .t         = NEU_JSON_INT,
                .v.val_int = res->tags[i].type,
            },
            {
                .name      = ADDRESS,
                .t         = NEU_JSON_STR,
                .v.val_str = res->tags[i].address,
            },
            {
                .name      = ATTRIBUTE,
                .t         = NEU_JSON_INT,
                .v.val_int = res->tags[i].attribute,
            },
            {
                .name      = GROUP_CONFIG_NAME,
                .t         = NEU_JSON_STR,
                .v.val_str = res->tags[i].group_config_name,
            },
            {
                .name      = TAG_ID,
                .t         = NEU_JSON_INT,
                .v.val_int = res->tags[i].id,
            },
        };
        array = neu_json_encode_array(array, tag, 5);
    }
    neu_json_elem_t elems[] = { {
                                    .name      = NEU_PARSE_FUNCTION,
                                    .t         = NEU_JSON_INT,
                                    .v.val_int = NEU_PARSE_OP_GET_TAGS,
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
                                    .name     = TAGS,
                                    .t        = NEU_JSON_OBJECT,
                                    .v.object = array,
                                } };
    return neu_json_encode(elems, 4, buf);
}

int neu_parse_decode_update_tags_req(char *                             buf,
                                     struct neu_parse_update_tags_req **result)
{
    struct neu_parse_update_tags_req *req =
        calloc(1, sizeof(struct neu_parse_update_tags_req));
    neu_json_elem_t elem[] = {
        {
            .name = NEU_PARSE_UUID,
            .t    = NEU_JSON_STR,
        },
        {
            .name = NODE_ID,
            .t    = NEU_JSON_INT,
        },
    };
    int ret = neu_json_decode(buf, NEU_JSON_ELEM_SIZE(elem), elem);
    if (ret != 0) {
        free(req);
        return -1;
    }
    req->function = NEU_PARSE_OP_UPDATE_TAGS;
    req->uuid     = elem[0].v.val_str;
    req->node_id  = elem[1].v.val_int;

    req->n_tag = neu_json_decode_array_size(buf, TAGS);
    req->tags =
        calloc(req->n_tag, sizeof(struct neu_parse_update_tags_req_tag));
    for (int i = 0; i < req->n_tag; i++) {
        neu_json_elem_t celem[] = { {
                                        .name = NAME,
                                        .t    = NEU_JSON_STR,
                                    },
                                    {
                                        .name = TYPE,
                                        .t    = NEU_JSON_INT,
                                    },
                                    {
                                        .name = ADDRESS,
                                        .t    = NEU_JSON_STR,
                                    },
                                    {
                                        .name = ATTRIBUTE,
                                        .t    = NEU_JSON_INT,
                                    },
                                    {
                                        .name = TAG_ID,
                                        .t    = NEU_JSON_INT,
                                    } };

        neu_json_decode_array(buf, TAGS, i, NEU_JSON_ELEM_SIZE(celem), celem);

        req->tags[i].name      = celem[0].v.val_str;
        req->tags[i].type      = celem[1].v.val_int;
        req->tags[i].address   = celem[2].v.val_str;
        req->tags[i].attribute = celem[3].v.val_int;
        req->tags[i].tag_id    = celem[4].v.val_int;
    }

    *result = req;

    return 0;
}

int neu_parse_encode_update_tags_res(struct neu_parse_update_tags_res *res,
                                     char **                           buf)
{
    neu_json_elem_t elems[] = { {
                                    .name      = NEU_PARSE_FUNCTION,
                                    .t         = NEU_JSON_INT,
                                    .v.val_int = NEU_PARSE_OP_UPDATE_TAGS,
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

void neu_parse_decode_get_tags_free(struct neu_parse_get_tags_req *req)
{
    free(req->uuid);
}

void neu_parse_decode_update_tags_free(struct neu_parse_update_tags_req *req)
{
    free(req->uuid);
    for (int i = 0; i < req->n_tag; i++) {
        free(req->tags[i].address);
        free(req->tags[i].name);
    }
    free(req->tags);
}

void neu_parse_decode_add_tags_free(struct neu_parse_add_tags_req *req)
{
    free(req->uuid);
    free(req->group_config_name);
    for (int i = 0; i < req->n_tag; i++) {
        free(req->tags[i].name);
        free(req->tags[i].address);
    }

    free(req->tags);
}

void neu_parse_decode_delete_tags_free(struct neu_parse_delete_tags_req *req)
{
    free(req->uuid);
    free(req->group_config_name);
    free(req->tag_ids);
}