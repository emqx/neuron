/**
 * NEURON IIoT System for Industry 4.0
 * Copyright (C) 2020-2022 EMQ Technologies Co., Ltd All rights reserved.
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

#include <jansson.h>

#include "neuron.h"
#include "utils/log.h"

#include "json_rw.h"
#include "plugin_ekuiper.h"

int json_encode_read_resp_header(void *json_object, void *param)
{
    int                      ret    = 0;
    json_read_resp_header_t *header = param;

    neu_json_elem_t resp_elems[] = { {
                                         .name      = "node_name",
                                         .t         = NEU_JSON_STR,
                                         .v.val_str = header->node_name,
                                     },
                                     {
                                         .name      = "group_name",
                                         .t         = NEU_JSON_STR,
                                         .v.val_str = header->group_name,
                                     },
                                     {
                                         .name      = "timestamp",
                                         .t         = NEU_JSON_INT,
                                         .v.val_int = header->timestamp,
                                     } };
    ret = neu_json_encode_field(json_object, resp_elems,
                                NEU_JSON_ELEM_SIZE(resp_elems));

    return ret;
}

int json_encode_read_resp_tags(void *json_object, void *param)
{
    int                       ret           = 0;
    json_read_resp_t *        resp          = param;
    neu_plugin_t *            plugin        = resp->plugin;
    neu_reqresp_trans_data_t *trans_data    = resp->trans_data;
    void *                    values_object = NULL;
    void *                    errors_object = NULL;
    void *                    metas_object  = NULL;

    metas_object = neu_json_encode_new();
    if (NULL == metas_object) {
        plog_error(plugin, "ekuiper cannot allocate json object");
        return -1;
    }

    values_object = neu_json_encode_new();
    if (NULL == values_object) {
        plog_error(plugin, "ekuiper cannot allocate json object");
        json_decref(metas_object);
        return -1;
    }
    errors_object = neu_json_encode_new();
    if (NULL == errors_object) {
        plog_error(plugin, "ekuiper cannot allocate json object");
        json_decref(values_object);
        json_decref(metas_object);
        return -1;
    }

    utarray_foreach(trans_data->tags, neu_resp_tag_value_meta_t *, tag_value)
    {
        neu_json_read_resp_tag_t json_tag = { 0 };

        neu_tag_value_to_json(tag_value, &json_tag);

        neu_json_elem_t tag_elem = {
            .name      = json_tag.name,
            .t         = json_tag.t,
            .v         = json_tag.value,
            .precision = tag_value->value.precision,
        };

        if (json_tag.n_meta > 0) {
            void *meta = neu_json_encode_new();
            for (int k = 0; k < json_tag.n_meta; k++) {
                neu_json_elem_t meta_elem = {
                    .name = json_tag.metas[k].name,
                    .t    = json_tag.metas[k].t,
                    .v    = json_tag.metas[k].value,
                };
                neu_json_encode_field(meta, &meta_elem, 1);
            }

            neu_json_elem_t meta_elem = {
                .name         = json_tag.name,
                .t            = NEU_JSON_OBJECT,
                .v.val_object = meta,
            };
            neu_json_encode_field(metas_object, &meta_elem, 1);
            free(json_tag.metas);
        }

        ret = neu_json_encode_field((0 != json_tag.error) ? errors_object
                                                          : values_object,
                                    &tag_elem, 1);
        if (0 != ret) {
            json_decref(errors_object);
            json_decref(values_object);
            json_decref(metas_object);
            return ret;
        }
    }

    neu_json_elem_t resp_elems[] = {
        {
            .name         = "values",
            .t            = NEU_JSON_OBJECT,
            .v.val_object = values_object,
        },
        {
            .name         = "errors",
            .t            = NEU_JSON_OBJECT,
            .v.val_object = errors_object,

        },
        {
            .name         = "metas",
            .t            = NEU_JSON_OBJECT,
            .v.val_object = metas_object,
        },
    };
    // steals `values_object` and `errors_object`
    ret = neu_json_encode_field(json_object, resp_elems,
                                NEU_JSON_ELEM_SIZE(resp_elems));

    return ret;
}

int json_encode_read_resp(void *json_object, void *param)
{
    int                       ret        = 0;
    json_read_resp_t *        resp       = param;
    neu_plugin_t *            plugin     = resp->plugin;
    neu_reqresp_trans_data_t *trans_data = resp->trans_data;

    json_read_resp_header_t header = { .group_name = trans_data->group,
                                       .node_name  = trans_data->driver,
                                       .timestamp  = global_timestamp };

    ret = json_encode_read_resp_header(json_object, &header);
    if (0 != ret) {
        plog_error(plugin,
                   "ekuiper fail encode data header node:%s group:%s, %" PRIu64,
                   header.node_name, header.group_name, header.timestamp);
        return ret;
    }

    ret = json_encode_read_resp_tags(json_object, param);

    return ret;
}

int json_decode_write_req(char *buf, size_t len, json_write_req_t **result)
{
    int               ret      = 0;
    void *            json_obj = NULL;
    json_write_req_t *req      = calloc(1, sizeof(json_write_req_t));
    if (req == NULL) {
        return -1;
    }

    json_obj = neu_json_decode_newb(buf, len);
    if (NULL == json_obj) {
        free(req);
        return -1;
    }

    neu_json_elem_t req_elems[] = {
        {
            .name = "node_name",
            .t    = NEU_JSON_STR,
        },
        {
            .name = "group_name",
            .t    = NEU_JSON_STR,
        },
        {
            .name = "tag_name",
            .t    = NEU_JSON_STR,
        },
        {
            .name = "value",
            .t    = NEU_JSON_VALUE,
        },
    };
    ret = neu_json_decode_by_json(json_obj, NEU_JSON_ELEM_SIZE(req_elems),
                                  req_elems);
    req->node_name  = req_elems[0].v.val_str;
    req->group_name = req_elems[1].v.val_str;
    req->tag_name   = req_elems[2].v.val_str;
    req->t          = req_elems[3].t;
    req->value      = req_elems[3].v;

    if (0 == ret) {
        *result = req;
    } else {
        json_decode_write_req_free(req);
        ret = -1;
    }

    neu_json_decode_free(json_obj);
    return ret;
}

void json_decode_write_req_free(json_write_req_t *req)
{
    if (NULL == req) {
        return;
    }

    free(req->group_name);
    free(req->node_name);
    free(req->tag_name);
    if (req->t == NEU_JSON_STR) {
        free(req->value.val_str);
    } else if (req->t == NEU_JSON_BYTES) {
        free(req->value.val_bytes.bytes);
    }

    free(req);
}
