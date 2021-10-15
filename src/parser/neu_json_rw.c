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

#include "neu_json_rw.h"

int neu_parse_decode_read(char *buf, neu_parse_read_req_t **result)
{
    neu_parse_read_req_t *req    = calloc(1, sizeof(neu_parse_read_req_t));
    neu_json_elem_t       elem[] = {
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

    *result = req;

    return 0;
}

void neu_parse_decode_read_free(neu_parse_read_req_t *req)
{
    free(req->group_config_name);
    free(req);
}

int neu_parse_encode_read(void *json_object, void *param)
{
    neu_parse_read_res_t *res   = (neu_parse_read_res_t *) param;
    void *                array = NULL;

    for (int i = 0; i < res->n_tag; i++) {
        neu_json_elem_t tag[] = { {
                                      .name      = "id",
                                      .t         = NEU_JSON_INT,
                                      .v.val_int = res->tags[i].tag_id,

                                  },
                                  {
                                      .name = "value",
                                      .t    = res->tags[i].t,
                                      .v    = res->tags[i].value,
                                  } };
        array                 = neu_json_encode_array(array, tag, 2);
    }
    neu_json_elem_t elems[] = { {
        .name     = "tags",
        .t        = NEU_JSON_OBJECT,
        .v.object = array,
    } };

    return neu_json_encode_field(json_object, elems, 1);
}

int neu_parse_decode_write(char *buf, neu_parse_write_req_t **result)
{
    neu_parse_write_req_t *req    = calloc(1, sizeof(neu_parse_write_req_t));
    neu_json_elem_t        elem[] = { {
                                   .name = "node_id",
                                   .t    = NEU_JSON_INT,
                               },
                               {
                                   .name = "group_config_name",
                                   .t    = NEU_JSON_STR,
                               }

    };

    int ret = neu_json_decode(buf, NEU_JSON_ELEM_SIZE(elem), elem);
    if (ret != 0) {
        free(req);
        return -1;
    }
    req->node_id           = elem[0].v.val_int;
    req->group_config_name = elem[1].v.val_str;

    req->n_tag = neu_json_decode_array_size(buf, "tags");
    req->tags  = calloc(req->n_tag, sizeof(neu_parse_write_req_tag_t));
    for (int i = 0; i < req->n_tag; i++) {
        neu_json_elem_t celem[] = { {
                                        .name = "id",
                                        .t    = NEU_JSON_INT,
                                    },
                                    {
                                        .name = "value",
                                        .t    = NEU_JSON_UNDEFINE,
                                    } };

        neu_json_decode_array(buf, "tags", i, NEU_JSON_ELEM_SIZE(celem), celem);
        req->tags[i].tag_id = celem[0].v.val_int;
        switch (celem[1].t) {
        case NEU_JSON_INT:
            req->tags[i].value.val_int = celem[1].v.val_int;
            break;
        case NEU_JSON_BOOL:
            req->tags[i].value.val_bool = celem[1].v.val_bool;
            break;
        case NEU_JSON_DOUBLE:
            req->tags[i].value.val_double = celem[1].v.val_double;
            break;
        case NEU_JSON_STR:
            req->tags[i].value.val_str = celem[1].v.val_str;
            break;
        default:
            break;
        }
        req->tags[i].t = celem[1].t;
    }

    *result = req;

    return 0;
}

void neu_parse_decode_write_free(neu_parse_write_req_t *req)
{
    if (NULL != req->group_config_name) {
        free(req->group_config_name);
    }

    for (int i = 0; i < req->n_tag; i++) {
        if (req->tags[i].t == NEU_JSON_STR) {
            free(req->tags[i].value.val_str);
        }
    }

    free(req->tags);
    free(req);
}

neu_data_val_t *neu_parse_write_req_to_val(neu_parse_write_req_t *req)
{
    neu_data_val_t *   val = neu_dvalue_unit_new();
    neu_fixed_array_t *array =
        neu_fixed_array_new(req->n_tag, sizeof(neu_int_val_t));

    for (int i = 0; i < req->n_tag; i++) {
        neu_int_val_t   iv;
        neu_data_val_t *v;

        switch (req->tags[i].t) {
        case NEU_JSON_INT:
            v = neu_dvalue_new(NEU_DTYPE_INT64);
            neu_dvalue_set_int64(v, req->tags[i].value.val_int);
            break;
        case NEU_JSON_BIT:
            v = neu_dvalue_new(NEU_DTYPE_BIT);
            neu_dvalue_set_bit(v, req->tags[i].value.val_bit);
            break;
        case NEU_JSON_STR:
            v = neu_dvalue_new(NEU_DTYPE_CSTR);
            neu_dvalue_set_cstr(v, req->tags[i].value.val_str);
            break;
        case NEU_JSON_DOUBLE:
            v = neu_dvalue_new(NEU_DTYPE_DOUBLE);
            neu_dvalue_set_double(v, req->tags[i].value.val_double);
            break;
        case NEU_JSON_FLOAT:
            v = neu_dvalue_new(NEU_DTYPE_FLOAT);
            neu_dvalue_set_float(v, req->tags[i].value.val_float);
            break;
        case NEU_JSON_BOOL:
            v = neu_dvalue_new(NEU_DTYPE_BOOL);
            neu_dvalue_set_bool(v, req->tags[i].value.val_bool);
            break;
        default:
            assert(false);
            break;
        }

        neu_int_val_init(&iv, req->tags[i].tag_id, v);
        neu_fixed_array_set(array, i, &iv);
    }

    neu_dvalue_init_move_array(val, NEU_DTYPE_INT_VAL, array);
    return val;
}
