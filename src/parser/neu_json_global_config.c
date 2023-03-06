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

#include <stdlib.h>
#include <string.h>

#include <jansson.h>

#include "tag.h"
#include "utils/log.h"
#include "json/json.h"

#include "neu_json_global_config.h"
#include "parser/neu_json_group_config.h"
#include "parser/neu_json_tag.h"

static void fini_add_tags_req(neu_json_add_tags_req_t *req)
{
    neu_json_add_tags_req_tag_t *p_tag = req->tags;
    for (int i = 0; i < req->n_tag; i++) {
        free(p_tag->description);
        free(p_tag->name);
        free(p_tag->address);
        if (NEU_JSON_STR == p_tag->t) {
            free(p_tag->value.val_str);
        }
        p_tag++;
    }

    free(req->tags);
    free(req->node);
    free(req->group);
}

static int decode_add_tags_req(void *json_obj, neu_json_add_tags_req_t *req)
{
    int                          ret   = 0;
    neu_json_add_tags_req_tag_t *p_tag = NULL;

    neu_json_elem_t req_elems[] = { {
                                        .name = "driver",
                                        .t    = NEU_JSON_STR,
                                    },
                                    {
                                        .name = "group",
                                        .t    = NEU_JSON_STR,
                                    } };
    ret = neu_json_decode_by_json(json_obj, NEU_JSON_ELEM_SIZE(req_elems),
                                  req_elems);
    // set the fields before check for easy clean up on error
    req->node  = req_elems[0].v.val_str;
    req->group = req_elems[1].v.val_str;
    if (0 != ret) {
        goto decode_fail;
    }

    req->n_tag = neu_json_decode_array_size_by_json(json_obj, "tags");
    if (req->n_tag <= 0) {
        goto decode_fail;
    }

    req->tags = calloc(req->n_tag, sizeof(*req->tags));
    if (NULL == req->tags) {
        goto decode_fail;
    }

    p_tag = req->tags;
    for (int i = 0; i < req->n_tag; i++) {
        neu_json_elem_t tag_elems[] = {
            {
                .name = "type",
                .t    = NEU_JSON_INT,
            },
            {
                .name = "name",
                .t    = NEU_JSON_STR,
            },
            {
                .name = "attribute",
                .t    = NEU_JSON_INT,
            },
            {
                .name = "address",
                .t    = NEU_JSON_STR,
            },
            {
                .name      = "decimal",
                .t         = NEU_JSON_DOUBLE,
                .attribute = NEU_JSON_ATTRIBUTE_OPTIONAL,
            },
            {
                .name      = "precision",
                .t         = NEU_JSON_INT,
                .attribute = NEU_JSON_ATTRIBUTE_OPTIONAL,
            },
            {
                .name      = "description",
                .t         = NEU_JSON_STR,
                .attribute = NEU_JSON_ATTRIBUTE_OPTIONAL,
            },
            {
                .name      = "value",
                .t         = NEU_JSON_VALUE,
                .attribute = NEU_JSON_ATTRIBUTE_OPTIONAL,
            },
        };

        ret = neu_json_decode_array_by_json(
            json_obj, "tags", i, NEU_JSON_ELEM_SIZE(tag_elems), tag_elems);

        // set the fields before check for easy clean up on error
        p_tag->type        = tag_elems[0].v.val_int;
        p_tag->name        = tag_elems[1].v.val_str;
        p_tag->attribute   = tag_elems[2].v.val_int;
        p_tag->address     = tag_elems[3].v.val_str;
        p_tag->decimal     = tag_elems[4].v.val_double;
        p_tag->precision   = tag_elems[5].v.val_int;
        p_tag->description = tag_elems[6].v.val_str;
        p_tag->t           = tag_elems[7].t;
        p_tag->value       = tag_elems[7].v;

        if (0 != ret) {
            goto decode_fail;
        }

        if (((NEU_ATTRIBUTE_STATIC & p_tag->attribute) &&
             NEU_JSON_UNDEFINE == p_tag->t) ||
            (!(NEU_ATTRIBUTE_STATIC & p_tag->attribute) &&
             NEU_JSON_UNDEFINE != p_tag->t)) {
            goto decode_fail;
        }

        p_tag++;
    }

    return 0;

decode_fail:
    fini_add_tags_req(req);
    memset(req, 0, sizeof(*req));
    return -1;
}

int neu_json_decode_global_config_req_tags(
    void *json_obj, neu_json_global_config_req_tags_t **result)
{
    int                                ret       = 0;
    json_t *                           tag_array = NULL;
    neu_json_global_config_req_tags_t *req       = NULL;

    tag_array = json_object_get(json_obj, "tags");
    if (!json_is_array(tag_array)) {
        nlog_error("no `tags` array field");
        goto decode_fail;
    }

    req = calloc(1, sizeof(*req));
    if (req == NULL) {
        nlog_error("calloc fail");
        goto decode_fail;
    }

    req->n_tag = json_array_size(tag_array);
    if (req->n_tag <= 0) {
        goto decode_fail;
    }

    req->tags = calloc(req->n_tag, sizeof(*req->tags));
    if (NULL == req->tags) {
        nlog_error("calloc fail");
        goto decode_fail;
    }

    for (int i = 0; i < req->n_tag; i++) {
        ret = decode_add_tags_req(json_array_get(tag_array, i), &req->tags[i]);
        if (ret != 0) {
            goto decode_fail;
        }
    }

    *result = req;
    goto decode_exit;

decode_fail:
    neu_json_decode_global_config_req_tags_free(req);
    ret = -1;

decode_exit:
    return ret;
}

void neu_json_decode_global_config_req_tags_free(
    neu_json_global_config_req_tags_t *req_tags)
{
    if (NULL == req_tags) {
        return;
    }

    for (int i = 0; i < req_tags->n_tag; ++i) {
        fini_add_tags_req(&req_tags->tags[i]);
    }

    free(req_tags->tags);
    free(req_tags);
}

int neu_json_decode_global_config_req(char *                         buf,
                                      neu_json_global_config_req_t **result)
{
    neu_json_global_config_req_t *req = calloc(1, sizeof(*req));
    if (NULL == req) {
        return -1;
    }

    void *json_obj = neu_json_decode_new(buf);
    if (NULL == json_obj) {
        free(req);
        return -1;
    }

    int ret = 0;
    if (0 == neu_json_decode_get_nodes_resp_json(json_obj, &req->nodes) &&
        0 ==
            neu_json_decode_get_driver_group_resp_json(json_obj,
                                                       &req->groups) &&
        0 == neu_json_decode_global_config_req_tags(json_obj, &req->tags)) {
        *result = req;
    } else {
        ret = -1;
        neu_json_decode_global_config_req_free(req);
    }

    neu_json_decode_free(json_obj);
    return ret;
}

void neu_json_decode_global_config_req_free(neu_json_global_config_req_t *req)
{
    if (req) {
        neu_json_decode_get_nodes_resp_free(req->nodes);
        neu_json_decode_get_driver_group_resp_free(req->groups);
        neu_json_decode_global_config_req_tags_free(req->tags);
        free(req);
    }
}
