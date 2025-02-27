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

/*
 * DO NOT EDIT THIS FILE MANUALLY!
 * It was automatically generated by `json-autotype`.
 */

#include <stdlib.h>
#include <string.h>

#include <jansson.h>

#include "utils/log.h"
#include "json/json.h"

#include "neu_json_group_config.h"

int neu_json_encode_add_group_config_req(void *json_object, void *param)
{
    int                              ret         = 0;
    neu_json_add_group_config_req_t *req         = param;
    neu_json_elem_t                  req_elems[] = { {
                                        .name = "node",
                                        .t    = NEU_JSON_STR,
                                        .v.val_str = req->node,
                                    },
                                    {
                                        .name = "group",
                                        .t    = NEU_JSON_STR,
                                        .v.val_str = req->group,
                                    },
                                    {
                                        .name = "interval",
                                        .t    = NEU_JSON_INT,
                                        .v.val_int = req->interval,
                                    } };
    ret = neu_json_encode_field(json_object, req_elems,
                                NEU_JSON_ELEM_SIZE(req_elems));
    return ret;
}

int neu_json_decode_add_group_config_req(
    char *buf, neu_json_add_group_config_req_t **result)
{
    int                              ret      = 0;
    void *                           json_obj = NULL;
    neu_json_add_group_config_req_t *req =
        calloc(1, sizeof(neu_json_add_group_config_req_t));
    if (req == NULL) {
        return -1;
    }

    json_obj = neu_json_decode_new(buf);

    neu_json_elem_t req_elems[] = { {
                                        .name = "node",
                                        .t    = NEU_JSON_STR,
                                    },
                                    {
                                        .name = "group",
                                        .t    = NEU_JSON_STR,
                                    },
                                    {
                                        .name = "interval",
                                        .t    = NEU_JSON_INT,
                                    } };
    ret = neu_json_decode_by_json(json_obj, NEU_JSON_ELEM_SIZE(req_elems),
                                  req_elems);
    if (ret != 0) {
        goto decode_fail;
    }

    req->node     = req_elems[0].v.val_str;
    req->group    = req_elems[1].v.val_str;
    req->interval = req_elems[2].v.val_int;

    *result = req;
    goto decode_exit;

decode_fail:
    free(req);
    ret = -1;

decode_exit:
    if (json_obj != NULL) {
        neu_json_decode_free(json_obj);
    }
    return ret;
}

void neu_json_decode_add_group_config_req_free(
    neu_json_add_group_config_req_t *req)
{
    free(req->node);
    free(req->group);

    free(req);
}

int neu_json_encode_del_group_config_req(void *json_object, void *param)
{
    int                              ret         = 0;
    neu_json_del_group_config_req_t *req         = param;
    neu_json_elem_t                  req_elems[] = { {
                                        .name = "node",
                                        .t    = NEU_JSON_STR,
                                        .v.val_str = req->node,
                                    },
                                    {
                                        .name = "group",
                                        .t    = NEU_JSON_STR,
                                        .v.val_str = req->group,
                                    } };
    ret = neu_json_encode_field(json_object, req_elems,
                                NEU_JSON_ELEM_SIZE(req_elems));
    return ret;
}

int neu_json_decode_del_group_config_req(
    char *buf, neu_json_del_group_config_req_t **result)
{
    int                              ret      = 0;
    void *                           json_obj = NULL;
    neu_json_del_group_config_req_t *req =
        calloc(1, sizeof(neu_json_del_group_config_req_t));
    if (req == NULL) {
        return -1;
    }

    json_obj = neu_json_decode_new(buf);

    neu_json_elem_t req_elems[] = { {
                                        .name = "node",
                                        .t    = NEU_JSON_STR,
                                    },
                                    {
                                        .name = "group",
                                        .t    = NEU_JSON_STR,
                                    } };
    ret = neu_json_decode_by_json(json_obj, NEU_JSON_ELEM_SIZE(req_elems),
                                  req_elems);
    if (ret != 0) {
        goto decode_fail;
    }

    req->node  = req_elems[0].v.val_str;
    req->group = req_elems[1].v.val_str;

    *result = req;
    goto decode_exit;

decode_fail:
    free(req);
    ret = -1;

decode_exit:
    if (json_obj != NULL) {
        neu_json_decode_free(json_obj);
    }
    return ret;
}

void neu_json_decode_del_group_config_req_free(
    neu_json_del_group_config_req_t *req)
{
    free(req->node);
    free(req->group);

    free(req);
}

int neu_json_encode_get_group_config_resp(void *json_object, void *param)
{
    int                               ret = 0;
    neu_json_get_group_config_resp_t *resp =
        (neu_json_get_group_config_resp_t *) param;

    void *group_config_array = neu_json_array();
    neu_json_get_group_config_resp_group_config_t *p_group_config =
        resp->group_configs;
    for (int i = 0; i < resp->n_group_config; i++) {
        neu_json_elem_t group_config_elems[] = {
            {
                .name      = "tag_count",
                .t         = NEU_JSON_INT,
                .v.val_int = p_group_config->tag_count,
            },
            {
                .name      = "name",
                .t         = NEU_JSON_STR,
                .v.val_str = p_group_config->name,
            },
            {
                .name      = "interval",
                .t         = NEU_JSON_INT,
                .v.val_int = p_group_config->interval,
            }
        };
        group_config_array =
            neu_json_encode_array(group_config_array, group_config_elems,
                                  NEU_JSON_ELEM_SIZE(group_config_elems));
        p_group_config++;
    }

    neu_json_elem_t resp_elems[] = { {
        .name         = "groups",
        .t            = NEU_JSON_OBJECT,
        .v.val_object = group_config_array,
    } };
    ret = neu_json_encode_field(json_object, resp_elems,
                                NEU_JSON_ELEM_SIZE(resp_elems));

    return ret;
}

int neu_json_encode_get_driver_group_resp(void *json_object, void *param)
{
    int                               ret = 0;
    neu_json_get_driver_group_resp_t *resp =
        (neu_json_get_driver_group_resp_t *) param;

    void *                                  group_array = neu_json_array();
    neu_json_get_driver_group_resp_group_t *p_group     = resp->groups;
    for (int i = 0; i < resp->n_group; i++) {
        neu_json_elem_t group_elems[] = {
            {
                .name      = "driver",
                .t         = NEU_JSON_STR,
                .v.val_str = p_group->driver,
            },
            {
                .name      = "group",
                .t         = NEU_JSON_STR,
                .v.val_str = p_group->group,
            },
            {
                .name      = "tag_count",
                .t         = NEU_JSON_INT,
                .v.val_int = p_group->tag_count,
            },
            {
                .name      = "interval",
                .t         = NEU_JSON_INT,
                .v.val_int = p_group->interval,
            },
        };
        group_array = neu_json_encode_array(group_array, group_elems,
                                            NEU_JSON_ELEM_SIZE(group_elems));
        p_group++;
    }

    neu_json_elem_t resp_elems[] = { {
        .name         = "groups",
        .t            = NEU_JSON_OBJECT,
        .v.val_object = group_array,
    } };
    ret = neu_json_encode_field(json_object, resp_elems,
                                NEU_JSON_ELEM_SIZE(resp_elems));

    return ret;
}

int neu_json_decode_get_driver_group_resp_json(
    void *json_obj, neu_json_get_driver_group_resp_t **result)
{
    int                               ret       = 0;
    json_t *                          grp_array = NULL;
    neu_json_get_driver_group_resp_t *resp      = NULL;

    grp_array = json_object_get(json_obj, "groups");
    if (!json_is_array(grp_array)) {
        goto decode_fail;
    }

    resp = calloc(1, sizeof(*resp));
    if (resp == NULL) {
        goto decode_fail;
    }

    resp->n_group = json_array_size(grp_array);

    resp->groups = calloc(resp->n_group, sizeof(*resp->groups));
    if (NULL == resp->groups) {
        goto decode_fail;
    }

    for (int i = 0; i < resp->n_group; i++) {
        neu_json_elem_t group_elems[] = {
            {
                .name = "driver",
                .t    = NEU_JSON_STR,
            },
            {
                .name = "group",
                .t    = NEU_JSON_STR,
            },
            {
                .name = "interval",
                .t    = NEU_JSON_INT,
            },
            {
                .name = "tag_count",
                .t    = NEU_JSON_INT,
            },
        };

        ret = neu_json_decode_by_json(json_array_get(grp_array, i),
                                      NEU_JSON_ELEM_SIZE(group_elems),
                                      group_elems);

        // set the fields before check for easy clean up on error
        resp->groups[i].driver    = group_elems[0].v.val_str;
        resp->groups[i].group     = group_elems[1].v.val_str;
        resp->groups[i].interval  = group_elems[2].v.val_int;
        resp->groups[i].tag_count = group_elems[3].v.val_int;

        if (ret != 0) {
            goto decode_fail;
        }
    }

    *result = resp;
    goto decode_exit;

decode_fail:
    neu_json_decode_get_driver_group_resp_free(resp);
    ret = -1;

decode_exit:
    return ret;
}

void neu_json_decode_get_driver_group_resp_free(
    neu_json_get_driver_group_resp_t *resp)
{
    if (resp) {
        for (int i = 0; i < resp->n_group; ++i) {
            free(resp->groups[i].driver);
            free(resp->groups[i].group);
        }
        free(resp->groups);
        free(resp);
    }
}

int neu_json_encode_get_subscribe_resp(void *object, void *param)
{
    int                            ret = 0;
    neu_json_get_subscribe_resp_t *resp =
        (neu_json_get_subscribe_resp_t *) param;

    void *                               group_array = neu_json_array();
    neu_json_get_subscribe_resp_group_t *p_group     = resp->groups;
    for (int i = 0; i < resp->n_group; i++) {
        json_t *ob = json_object();

        // rely on lib jansson to check for NULL pointers
        json_object_set_new(ob, "driver", json_string(p_group->driver));
        json_object_set_new(ob, "group", json_string(p_group->group));

        if (p_group->params) {
            json_t *t = json_loads(p_group->params, 0, NULL);
            json_t *p = json_object_get(t, "params");
            json_object_set(ob, "params", p);
            json_decref(t);
        }

        if (p_group->static_tags) {
            json_t *t = json_loads(p_group->static_tags, 0, NULL);
            json_t *p = json_object_get(t, "static_tags");
            json_object_set(ob, "static_tags", p);
            json_decref(t);
        }

        json_array_append_new(group_array, ob);

        p_group++;
    }

    neu_json_elem_t resp_elems[] = { {
        .name         = "groups",
        .t            = NEU_JSON_OBJECT,
        .v.val_object = group_array,
    } };
    ret                          = neu_json_encode_field(object, resp_elems,
                                NEU_JSON_ELEM_SIZE(resp_elems));

    return ret;
}

static inline int dump_params(void *root, char **const result)
{
    return neu_json_dump_key(root, "params", result, false);
}

int neu_json_encode_subscribe_req(void *json_object, void *param)
{
    int                       ret = 0;
    neu_json_subscribe_req_t *req = param;

    neu_json_elem_t req_elems[] = { {
                                        .name      = "app",
                                        .t         = NEU_JSON_STR,
                                        .v.val_str = req->app,
                                    },
                                    {
                                        .name      = "group",
                                        .t         = NEU_JSON_STR,
                                        .v.val_str = req->group,
                                    },
                                    {
                                        .name      = "driver",
                                        .t         = NEU_JSON_STR,
                                        .v.val_str = req->driver,
                                    },
                                    {
                                        .name         = "params",
                                        .t            = NEU_JSON_OBJECT,
                                        .v.val_object = req->params
                                            ? json_loads(req->params, 0, NULL)
                                            : NULL,
                                    } };
    ret                         = neu_json_encode_field(json_object, req_elems,
                                NEU_JSON_ELEM_SIZE(req_elems));

    return ret;
}

int neu_json_decode_subscribe_req(char *buf, neu_json_subscribe_req_t **result)
{
    int                       ret      = 0;
    void *                    json_obj = NULL;
    neu_json_subscribe_req_t *req = calloc(1, sizeof(neu_json_subscribe_req_t));
    if (req == NULL) {
        return -1;
    }

    json_obj = neu_json_decode_new(buf);

    neu_json_elem_t req_elems[] = { {
                                        .name = "app",
                                        .t    = NEU_JSON_STR,
                                    },
                                    {
                                        .name = "group",
                                        .t    = NEU_JSON_STR,
                                    },
                                    {
                                        .name = "driver",
                                        .t    = NEU_JSON_STR,
                                    } };
    ret = neu_json_decode_by_json(json_obj, NEU_JSON_ELEM_SIZE(req_elems),
                                  req_elems);
    if (ret != 0) {
        goto decode_fail;
    }

    req->app    = req_elems[0].v.val_str;
    req->group  = req_elems[1].v.val_str;
    req->driver = req_elems[2].v.val_str;

    ret = dump_params(json_obj, &req->params);
    if (0 != ret) {
        goto decode_fail;
    }

    ret = neu_json_dump_key(json_obj, "static_tags", &req->static_tags, false);
    if (0 != ret) {
        goto decode_fail;
    }

    *result = req;
    goto decode_exit;

decode_fail:
    free(req);
    ret = -1;

decode_exit:
    if (json_obj != NULL) {
        neu_json_decode_free(json_obj);
    }
    return ret;
}

void neu_json_decode_subscribe_req_free(neu_json_subscribe_req_t *req)
{
    free(req->app);
    free(req->driver);
    free(req->group);
    free(req->params);
    if (req->static_tags) {
        free(req->static_tags);
    }

    free(req);
}

int neu_json_encode_unsubscribe_req(void *json_object, void *param)
{
    int                         ret         = 0;
    neu_json_unsubscribe_req_t *req         = param;
    neu_json_elem_t             req_elems[] = { {
                                        .name      = "app",
                                        .t         = NEU_JSON_STR,
                                        .v.val_str = req->app,
                                    },
                                    {
                                        .name      = "group",
                                        .t         = NEU_JSON_STR,
                                        .v.val_str = req->group,
                                    },
                                    {
                                        .name      = "driver",
                                        .t         = NEU_JSON_STR,
                                        .v.val_str = req->driver,
                                    } };
    ret = neu_json_encode_field(json_object, req_elems,
                                NEU_JSON_ELEM_SIZE(req_elems));
    return ret;
}

int neu_json_decode_unsubscribe_req(char *                       buf,
                                    neu_json_unsubscribe_req_t **result)
{
    int                         ret      = 0;
    void *                      json_obj = NULL;
    neu_json_unsubscribe_req_t *req =
        calloc(1, sizeof(neu_json_unsubscribe_req_t));
    if (req == NULL) {
        return -1;
    }

    json_obj = neu_json_decode_new(buf);

    neu_json_elem_t req_elems[] = { {
                                        .name = "app",
                                        .t    = NEU_JSON_STR,
                                    },
                                    {
                                        .name = "group",
                                        .t    = NEU_JSON_STR,
                                    },
                                    {
                                        .name = "driver",
                                        .t    = NEU_JSON_STR,
                                    } };
    ret = neu_json_decode_by_json(json_obj, NEU_JSON_ELEM_SIZE(req_elems),
                                  req_elems);
    if (ret != 0) {
        goto decode_fail;
    }

    req->app    = req_elems[0].v.val_str;
    req->group  = req_elems[1].v.val_str;
    req->driver = req_elems[2].v.val_str;

    *result = req;
    goto decode_exit;

decode_fail:
    free(req);
    ret = -1;

decode_exit:
    if (json_obj != NULL) {
        neu_json_decode_free(json_obj);
    }
    return ret;
}

void neu_json_decode_unsubscribe_req_free(neu_json_unsubscribe_req_t *req)
{

    free(req->app);
    free(req->driver);
    free(req->group);

    free(req);
}

int neu_json_encode_update_group_config_req(void *json_object, void *param)
{
    int                                 ret         = 0;
    neu_json_update_group_config_req_t *req         = param;
    neu_json_elem_t                     req_elems[] = { {
                                        .name = "node",
                                        .t = NEU_JSON_STR,
                                        .v.val_str = req->node,
                                    },
                                    {
                                        .name = "group",
                                        .t = NEU_JSON_STR,
                                        .v.val_str = req->group,
                                    },
                                    {
                                        .name = "new_name",
                                        .t = NEU_JSON_STR,
                                        .v.val_str = req->new_name,
                                    } };
    ret = neu_json_encode_field(json_object, req_elems,
                                NEU_JSON_ELEM_SIZE(req_elems));
    if (0 != ret) {
        return ret;
    }
    if (req->set_interval) {
        neu_json_elem_t interval_elem = {
            .name      = "interval",
            .t         = NEU_JSON_INT,
            .v.val_int = req->interval,
        };
        ret = neu_json_encode_field(json_object, &interval_elem, 1);
    }
    return ret;
}

int neu_json_decode_update_group_config_req(
    char *buf, neu_json_update_group_config_req_t **result)
{
    int ret = 0;

    neu_json_update_group_config_req_t *req = calloc(1, sizeof(*req));
    if (req == NULL) {
        return -1;
    }

    void *json_obj = neu_json_decode_new(buf);
    if (NULL == json_obj) {
        free(req);
        return -1;
    }

    neu_json_elem_t req_elems[] = {
        {
            .name = "node",
            .t    = NEU_JSON_STR,
        },
        {
            .name = "group",
            .t    = NEU_JSON_STR,
        },
        {
            .name      = "new_name",
            .t         = NEU_JSON_STR,
            .attribute = NEU_JSON_ATTRIBUTE_OPTIONAL,
        },
    };
    ret       = neu_json_decode_by_json(json_obj, NEU_JSON_ELEM_SIZE(req_elems),
                                  req_elems);
    req->node = req_elems[0].v.val_str;
    req->group    = req_elems[1].v.val_str;
    req->new_name = req_elems[2].v.val_str;
    if (0 != ret) {
        goto error;
    }

    json_t *json_interval = json_object_get(json_obj, "interval");
    if (NULL != json_interval) {
        if (!json_is_integer(json_interval)) {
            nlog_error("decode interval is not integer");
            goto error;
        }
        req->set_interval = true;
        req->interval     = json_integer_value(json_interval);
    } else if (NULL == req->new_name) {
        // at least one of `new_name` or `interval` should be provided
        goto error;
    }

    *result = req;
    neu_json_decode_free(json_obj);
    return 0;

error:
    neu_json_decode_update_group_config_req_free(req);
    neu_json_decode_free(json_obj);
    return -1;
}

void neu_json_decode_update_group_config_req_free(
    neu_json_update_group_config_req_t *req)
{
    free(req->node);
    free(req->group);
    free(req->new_name);

    free(req);
}

int neu_json_decode_subscribe_groups_info_json(
    void *json_obj, neu_json_subscribe_groups_info_t *info)
{
    neu_json_elem_t req_elems[] = {
        {
            .name = "driver",
            .t    = NEU_JSON_STR,
        },
        {
            .name = "group",
            .t    = NEU_JSON_STR,
        },
    };
    int ret = neu_json_decode_by_json(json_obj, NEU_JSON_ELEM_SIZE(req_elems),
                                      req_elems);
    if (ret != 0) {
        free(req_elems[0].v.val_str);
        free(req_elems[1].v.val_str);
        return -1;
    }

    info->driver = req_elems[0].v.val_str;
    info->group  = req_elems[1].v.val_str;

    ret = dump_params(json_obj, &info->params);
    if (0 != ret) {
        free(req_elems[0].v.val_str);
        free(req_elems[1].v.val_str);
        return -1;
    }

    ret = neu_json_dump_key(json_obj, "static_tags", &info->static_tags, false);
    if (0 != ret) {
        free(req_elems[0].v.val_str);
        free(req_elems[1].v.val_str);
        return -1;
    }

    return 0;
}

void neu_json_decode_subscribe_groups_info_fini(
    neu_json_subscribe_groups_info_t *req)
{
    free(req->driver);
    free(req->group);
    free(req->params);
}

int neu_json_decode_subscribe_groups_req(
    char *buf, neu_json_subscribe_groups_req_t **result)
{
    int                               ret      = 0;
    void *                            json_obj = NULL;
    neu_json_subscribe_groups_req_t * req      = NULL;
    neu_json_subscribe_groups_info_t *groups   = NULL;

    req = calloc(1, sizeof(*req));
    if (req == NULL) {
        return -1;
    }

    json_obj = neu_json_decode_new(buf);
    if (NULL == json_obj) {
        free(req);
        return -1;
    }

    neu_json_elem_t req_elems[] = {
        {
            .name = "app",
            .t    = NEU_JSON_STR,
        },
        {
            .name = "groups",
            .t    = NEU_JSON_OBJECT,
        },
    };
    ret = neu_json_decode_by_json(json_obj, NEU_JSON_ELEM_SIZE(req_elems),
                                  req_elems);
    if (0 != ret) {
        goto error;
    }

    json_t *group_arr = req_elems[1].v.val_object;
    if (!json_is_array(group_arr)) {
        goto error;
    }

    int n_group = json_array_size(group_arr);
    groups      = calloc(n_group, sizeof(groups[0]));
    if (NULL == groups) {
        goto error;
    }

    for (int i = 0; i < n_group; ++i) {
        void *info = json_array_get(group_arr, i);
        if (0 != neu_json_decode_subscribe_groups_info_json(info, &groups[i])) {
            while (--i >= 0) {
                neu_json_decode_subscribe_groups_info_fini(&groups[i]);
            }
            goto error;
        }
    }

    req->app     = req_elems[0].v.val_str;
    req->n_group = n_group;
    req->groups  = groups;
    *result      = req;
    neu_json_decode_free(json_obj);
    return 0;

error:
    free(req_elems[0].v.val_str);
    free(groups);
    free(req);
    neu_json_decode_free(json_obj);
    return -1;
}

void neu_json_decode_subscribe_groups_req_free(
    neu_json_subscribe_groups_req_t *req)
{
    if (req) {
        free(req);
    }
}
