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
#include "json/neu_json_tag.h"

int neu_parse_decode_add_tags(char *buf, neu_parse_add_tags_req_t **result)
{
    neu_parse_add_tags_req_t *req = calloc(1, sizeof(neu_parse_add_tags_req_t));
    neu_json_elem_t           elem[] = {
        {
            .name = "node_id",
            .t    = NEU_JSON_INT,
        },
        {
            .name = "group_config_name",
            .t    = NEU_JSON_STR,
        },
    };

    int ret = neu_json_decode(buf, NEU_JSON_ELEM_SIZE(elem), elem);
    if (ret != 0) {
        free(req);
        return -1;
    }
    req->node_id           = elem[0].v.val_int;
    req->group_config_name = elem[1].v.val_str;

    req->n_tag = neu_json_decode_array_size(buf, "tags");
    req->tags  = calloc(req->n_tag, sizeof(neu_parse_add_tags_req_tag_t));
    for (int i = 0; i < req->n_tag; i++) {
        neu_json_elem_t celem[] = { {
                                        .name = "name",
                                        .t    = NEU_JSON_STR,
                                    },
                                    {
                                        .name = "type",
                                        .t    = NEU_JSON_INT,
                                    },
                                    {
                                        .name = "address",
                                        .t    = NEU_JSON_STR,
                                    },
                                    {
                                        .name = "attribute",
                                        .t    = NEU_JSON_INT,
                                    } };

        neu_json_decode_array(buf, "tags", i, NEU_JSON_ELEM_SIZE(celem), celem);
        req->tags[i].name      = celem[0].v.val_str;
        req->tags[i].type      = celem[1].v.val_int;
        req->tags[i].address   = celem[2].v.val_str;
        req->tags[i].attribute = celem[3].v.val_int;
    }

    *result = req;

    return 0;
}

void neu_parse_decode_add_tags_free(neu_parse_add_tags_req_t *req)
{
    free(req->group_config_name);
    for (int i = 0; i < req->n_tag; i++) {
        free(req->tags[i].name);
        free(req->tags[i].address);
    }

    free(req->tags);
    free(req);
}

int neu_parse_decode_del_tags(char *buf, neu_parse_del_tags_req_t **result)
{
    neu_parse_del_tags_req_t *req = calloc(1, sizeof(neu_parse_del_tags_req_t));
    neu_json_elem_t           elem[] = {
        {
            .name = "node_id",
            .t    = NEU_JSON_INT,
        },
        {
            .name = "group_config_name",
            .t    = NEU_JSON_STR,
        },
    };

    int ret = neu_json_decode(buf, NEU_JSON_ELEM_SIZE(elem), elem);
    if (ret != 0) {
        free(req);
        return -1;
    }
    req->node_id           = elem[0].v.val_int;
    req->group_config_name = elem[1].v.val_str;

    req->n_tag_id = neu_json_decode_array_size(buf, "ids");
    req->tag_ids  = calloc(req->n_tag_id, sizeof(uint32_t));
    for (int i = 0; i < req->n_tag_id; i++) {
        neu_json_elem_t celem[] = {
            {
                .name = NULL,
                .t    = NEU_JSON_INT,
            },
        };

        neu_json_decode_array(buf, "ids", i, NEU_JSON_ELEM_SIZE(celem), celem);
        req->tag_ids[i] = celem[0].v.val_int;
    }

    *result = req;

    return 0;
}

void neu_parse_decode_del_tags_free(neu_parse_del_tags_req_t *req)
{
    free(req->group_config_name);
    free(req->tag_ids);
    free(req);
}

int neu_parse_decode_update_tags(char *                        buf,
                                 neu_parse_update_tags_req_t **result)
{
    neu_parse_update_tags_req_t *req =
        calloc(1, sizeof(neu_parse_update_tags_req_t));
    neu_json_elem_t elem[] = {
        {
            .name = "node_id",
            .t    = NEU_JSON_INT,
        },
    };
    int ret = neu_json_decode(buf, NEU_JSON_ELEM_SIZE(elem), elem);
    if (ret != 0) {
        free(req);
        return -1;
    }
    req->node_id = elem[0].v.val_int;

    req->n_tag = neu_json_decode_array_size(buf, "tags");
    req->tags  = calloc(req->n_tag, sizeof(neu_parse_update_tags_req_tag_t));
    for (int i = 0; i < req->n_tag; i++) {
        neu_json_elem_t celem[] = { {
                                        .name = "name",
                                        .t    = NEU_JSON_STR,
                                    },
                                    {
                                        .name = "type",
                                        .t    = NEU_JSON_INT,
                                    },
                                    {
                                        .name = "address",
                                        .t    = NEU_JSON_STR,
                                    },
                                    {
                                        .name = "attribute",
                                        .t    = NEU_JSON_INT,
                                    },
                                    {
                                        .name = "id",
                                        .t    = NEU_JSON_INT,
                                    } };

        neu_json_decode_array(buf, "tags", i, NEU_JSON_ELEM_SIZE(celem), celem);

        req->tags[i].name      = celem[0].v.val_str;
        req->tags[i].type      = celem[1].v.val_int;
        req->tags[i].address   = celem[2].v.val_str;
        req->tags[i].attribute = celem[3].v.val_int;
        req->tags[i].tag_id    = celem[4].v.val_int;
    }

    *result = req;

    return 0;
}

void neu_parse_decode_update_tags_free(neu_parse_update_tags_req_t *req)
{
    for (int i = 0; i < req->n_tag; i++) {
        free(req->tags[i].address);
        free(req->tags[i].name);
    }
    free(req->tags);
    free(req);
}

int neu_parse_decode_get_tags(char *buf, neu_parse_get_tags_req_t **result)
{
    neu_parse_get_tags_req_t *req = calloc(1, sizeof(neu_parse_get_tags_req_t));
    neu_json_elem_t           elem[] = {
        {
            .name = "node_id",
            .t    = NEU_JSON_INT,
        },
    };

    int ret = neu_json_decode(buf, NEU_JSON_ELEM_SIZE(elem), elem);
    if (ret != 0) {
        free(req);
        return -1;
    }
    req->node_id = elem[0].v.val_int;

    *result = req;

    return 0;
}

void neu_parse_decode_get_tags_free(neu_parse_get_tags_req_t *req)
{
    free(req);
}

int neu_parse_encode_get_tags(void *json_object, void *param)
{
    neu_parse_get_tags_res_t *res   = (neu_parse_get_tags_res_t *) param;
    void *                    array = NULL;

    for (int i = 0; i < res->n_tag; i++) {
        neu_json_elem_t tag[] = {
            {
                .name      = "name",
                .t         = NEU_JSON_STR,
                .v.val_str = res->tags[i].name,
            },
            {
                .name      = "type",
                .t         = NEU_JSON_INT,
                .v.val_int = res->tags[i].type,
            },
            {
                .name      = "address",
                .t         = NEU_JSON_STR,
                .v.val_str = res->tags[i].address,
            },
            {
                .name      = "attribute",
                .t         = NEU_JSON_INT,
                .v.val_int = res->tags[i].attribute,
            },
            {
                .name      = "group_config_name",
                .t         = NEU_JSON_STR,
                .v.val_str = res->tags[i].group_config_name,
            },
            {
                .name      = "id",
                .t         = NEU_JSON_INT,
                .v.val_int = res->tags[i].id,
            },
        };
        array = neu_json_encode_array(array, tag, 6);
    }
    neu_json_elem_t elems[] = { {
        .name     = "tags",
        .t        = NEU_JSON_OBJECT,
        .v.object = array,
    } };

    return neu_json_encode_field(json_object, elems, 1);
}
