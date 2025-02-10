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
#include <stdlib.h>
#include <string.h>

#include <jansson.h>

#include "json/json.h"

#include "json/neu_json_driver.h"

int neu_json_decode_driver_action_req_json(void *                    json_obj,
                                           neu_json_driver_action_t *req)
{
    int ret = 0;

    neu_json_elem_t req_elems[] = {
        {
            .name = "node",
            .t    = NEU_JSON_STR,
        },
        {
            .name = "action",
            .t    = NEU_JSON_STR,
        },
    };
    ret = neu_json_decode_by_json(json_obj, NEU_JSON_ELEM_SIZE(req_elems),
                                  req_elems);
    if (ret != 0) {
        return -1;
    }

    req->driver = req_elems[0].v.val_str;
    req->action = req_elems[1].v.val_str;

    return 0;
}

int neu_json_decode_driver_action_req(char *                     buf,
                                      neu_json_driver_action_t **result)
{
    int                       ret      = 0;
    void *                    json_obj = NULL;
    neu_json_driver_action_t *req = calloc(1, sizeof(neu_json_driver_action_t));
    if (req == NULL) {
        return -1;
    }

    json_obj = neu_json_decode_new(buf);
    if (NULL == json_obj) {
        free(req);
        return -1;
    }

    ret = neu_json_decode_driver_action_req_json(json_obj, req);
    if (0 == ret) {
        *result = req;
    } else {
        free(req);
    }

    neu_json_decode_free(json_obj);
    return ret;

    return 0;
}

void neu_json_decode_driver_action_req_free(neu_json_driver_action_t *req)
{
    if (req) {
        free(req->driver);
        free(req->action);
        free(req);
    }
}

int neu_json_decode_driver_directory_req(
    char *buf, neu_json_driver_directory_req_t **result)
{
    int   ret      = 0;
    void *json_obj = neu_json_decode_new(buf);

    neu_json_elem_t req_elems[] = {
        {
            .name = "node",
            .t    = NEU_JSON_STR,
        },
        {
            .name = "path",
            .t    = NEU_JSON_STR,
        },
    };

    ret = neu_json_decode_by_json(json_obj, NEU_JSON_ELEM_SIZE(req_elems),
                                  req_elems);
    if (ret != 0) {
        return -1;
    }

    *result = calloc(1, sizeof(neu_json_driver_directory_req_t));

    (*result)->driver = req_elems[0].v.val_str;
    (*result)->path   = req_elems[1].v.val_str;

    neu_json_decode_free(json_obj);
    return ret;
}

void neu_json_decode_driver_directory_req_free(
    neu_json_driver_directory_req_t *req)
{
    if (req) {
        free(req->driver);
        free(req->path);
        free(req);
    }
}

int neu_json_encode_driver_directory_resp(void *json_object, void *param)
{
    neu_json_driver_directory_resp_t *resp =
        (neu_json_driver_directory_resp_t *) param;
    void *array = neu_json_array();

    for (int i = 0; i < resp->n_files; i++) {
        neu_json_elem_t file_elems[] = {
            {
                .name      = "name",
                .t         = NEU_JSON_STR,
                .v.val_str = resp->files[i].name,
            },
            {
                .name      = "type",
                .t         = NEU_JSON_INT,
                .v.val_int = resp->files[i].ftype,
            },
            {
                .name      = "size",
                .t         = NEU_JSON_INT,
                .v.val_int = resp->files[i].size,
            },
            {
                .name      = "t",
                .t         = NEU_JSON_INT,
                .v.val_int = resp->files[i].timestamp,
            },
        };

        array = neu_json_encode_array(array, file_elems,
                                      NEU_JSON_ELEM_SIZE(file_elems));
    }

    neu_json_elem_t resp_elems[] = {
        {
            .name      = "error",
            .t         = NEU_JSON_INT,
            .v.val_int = resp->error,
        },
        {
            .name         = "files",
            .t            = NEU_JSON_OBJECT,
            .v.val_object = array,
        },
    };

    int ret = neu_json_encode_field(json_object, resp_elems,
                                    NEU_JSON_ELEM_SIZE(resp_elems));
    return ret;
}

int neu_json_decode_driver_fup_open_req(char *                           buf,
                                        neu_json_driver_fup_open_req_t **result)
{
    int   ret      = 0;
    void *json_obj = neu_json_decode_new(buf);

    neu_json_elem_t req_elems[] = {
        {
            .name = "node",
            .t    = NEU_JSON_STR,
        },
        {
            .name = "path",
            .t    = NEU_JSON_STR,
        },
    };

    ret = neu_json_decode_by_json(json_obj, NEU_JSON_ELEM_SIZE(req_elems),
                                  req_elems);
    if (ret != 0) {
        return -1;
    }

    *result = calloc(1, sizeof(neu_json_driver_fup_open_req_t));

    (*result)->driver = req_elems[0].v.val_str;
    (*result)->path   = req_elems[1].v.val_str;

    neu_json_decode_free(json_obj);
    return ret;
}

void neu_json_decode_driver_fup_open_req_free(
    neu_json_driver_fup_open_req_t *req)
{
    if (req) {
        free(req->driver);
        free(req->path);
        free(req);
    }
}

int neu_json_encode_driver_fup_open_resp(void *json_object, void *param)
{
    neu_json_driver_fup_open_resp_t *resp =
        (neu_json_driver_fup_open_resp_t *) param;

    neu_json_elem_t resp_elems[] = {
        {
            .name      = "error",
            .t         = NEU_JSON_INT,
            .v.val_int = resp->error,
        },
        {
            .name      = "size",
            .t         = NEU_JSON_INT,
            .v.val_int = resp->size,
        },
    };

    int ret = neu_json_encode_field(json_object, resp_elems,
                                    NEU_JSON_ELEM_SIZE(resp_elems));
    return ret;
}

int neu_json_decode_driver_fdown_open_req(
    char *buf, neu_json_driver_fdown_open_req_t **result)
{
    int   ret      = 0;
    void *json_obj = neu_json_decode_new(buf);

    neu_json_elem_t req_elems[] = {
        {
            .name = "node",
            .t    = NEU_JSON_STR,
        },
        {
            .name = "src path",
            .t    = NEU_JSON_STR,
        },
        {
            .name = "dst path",
            .t    = NEU_JSON_STR,
        },
        {
            .name = "size",
            .t    = NEU_JSON_INT,
        },
    };

    ret = neu_json_decode_by_json(json_obj, NEU_JSON_ELEM_SIZE(req_elems),
                                  req_elems);
    if (ret != 0) {
        return -1;
    }

    *result = calloc(1, sizeof(neu_json_driver_fdown_open_req_t));

    (*result)->driver   = req_elems[0].v.val_str;
    (*result)->src_path = req_elems[1].v.val_str;
    (*result)->dst_path = req_elems[2].v.val_str;
    (*result)->size     = req_elems[3].v.val_int;

    neu_json_decode_free(json_obj);
    return ret;
}

void neu_json_decode_driver_fdown_open_req_free(
    neu_json_driver_fdown_open_req_t *req)
{
    if (req) {
        free(req->driver);
        free(req->src_path);
        free(req->dst_path);
        free(req);
    }
}

int neu_json_encode_driver_fdown_open_resp(void *json_object, void *param)
{
    neu_json_driver_fdown_open_resp_t *resp =
        (neu_json_driver_fdown_open_resp_t *) param;

    neu_json_elem_t resp_elems[] = {
        {
            .name      = "error",
            .t         = NEU_JSON_INT,
            .v.val_int = resp->error,
        },
    };

    int ret = neu_json_encode_field(json_object, resp_elems,
                                    NEU_JSON_ELEM_SIZE(resp_elems));
    return ret;
}

int neu_json_decode_driver_fup_data_req(char *                           buf,
                                        neu_json_driver_fup_data_req_t **result)
{
    int   ret      = 0;
    void *json_obj = neu_json_decode_new(buf);

    neu_json_elem_t req_elems[] = {
        {
            .name = "node",
            .t    = NEU_JSON_STR,
        },
        {
            .name = "path",
            .t    = NEU_JSON_STR,
        },
    };

    ret = neu_json_decode_by_json(json_obj, NEU_JSON_ELEM_SIZE(req_elems),
                                  req_elems);
    if (ret != 0) {
        return -1;
    }

    *result = calloc(1, sizeof(neu_json_driver_fup_data_req_t));

    (*result)->driver = req_elems[0].v.val_str;
    (*result)->path   = req_elems[1].v.val_str;

    neu_json_decode_free(json_obj);
    return ret;
}

void neu_json_decode_driver_fup_data_req_free(
    neu_json_driver_fup_data_req_t *req)
{
    if (req) {
        free(req->driver);
        free(req->path);
        free(req);
    }
}

int neu_json_encode_driver_fup_data_resp(void *json_object, void *param)
{
    neu_json_driver_fup_data_resp_t *resp =
        (neu_json_driver_fup_data_resp_t *) param;
    void *array = neu_json_array();

    for (int i = 0; i < resp->len; i++) {
        neu_json_elem_t elem = {
            .name      = "data",
            .t         = NEU_JSON_INT,
            .v.val_int = resp->data[i],
        };
        array = neu_json_encode_array_value(array, &elem, 1);
    }

    neu_json_elem_t resp_elems[] = {
        {
            .name      = "error",
            .t         = NEU_JSON_INT,
            .v.val_int = resp->error,
        },
        {
            .name       = "more",
            .t          = NEU_JSON_BOOL,
            .v.val_bool = resp->more,
        },
        {
            .name         = "data",
            .t            = NEU_JSON_OBJECT,
            .v.val_object = array,
        },
    };

    int ret = neu_json_encode_field(json_object, resp_elems,
                                    NEU_JSON_ELEM_SIZE(resp_elems));
    return ret;
}

int neu_json_encode_driver_fdown_data_resp(void *json_object, void *param)
{
    neu_json_driver_fdown_data_resp_t *resp =
        (neu_json_driver_fdown_data_resp_t *) param;

    neu_json_elem_t resp_elems[] = {
        {
            .name      = "node",
            .t         = NEU_JSON_STR,
            .v.val_str = resp->driver,
        },
        {
            .name      = "path",
            .t         = NEU_JSON_STR,
            .v.val_str = resp->src_path,
        },
    };

    int ret = neu_json_encode_field(json_object, resp_elems,
                                    NEU_JSON_ELEM_SIZE(resp_elems));
    return ret;
}

int neu_json_decode_driver_fdown_data_req(
    char *buf, neu_json_driver_fdown_data_req_t **result)
{
    int   ret      = 0;
    void *json_obj = neu_json_decode_new(buf);

    neu_json_elem_t req_elems[] = {
        {
            .name = "node",
            .t    = NEU_JSON_STR,
        },
        {
            .name = "src path",
            .t    = NEU_JSON_STR,
        },
        {
            .name = "more",
            .t    = NEU_JSON_BOOL,
        },
        {
            .name = "data",
            .t    = NEU_JSON_ARRAY_UINT8,
        },
    };

    ret = neu_json_decode_by_json(json_obj, NEU_JSON_ELEM_SIZE(req_elems),
                                  req_elems);
    if (ret != 0) {
        return -1;
    }

    *result = calloc(1, sizeof(neu_json_driver_fdown_data_req_t));

    (*result)->driver   = req_elems[0].v.val_str;
    (*result)->src_path = req_elems[1].v.val_str;
    (*result)->more     = req_elems[2].v.val_bool;
    (*result)->data     = req_elems[3].v.val_array_uint8.u8s;
    (*result)->len      = req_elems[3].v.val_array_uint8.length;

    neu_json_decode_free(json_obj);
    return ret;
}

void neu_json_decode_driver_fdown_data_req_free(
    neu_json_driver_fdown_data_req_t *req)
{
    if (req) {
        free(req->driver);
        free(req->src_path);
        free(req->data);
        free(req);
    }
}