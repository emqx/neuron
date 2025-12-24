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
#include "parser/neu_json_node.h"
#include "parser/neu_json_tag.h"

static void fini_add_tags_req(neu_json_add_tags_req_t *req)
{
    neu_json_tag_array_t arr = {
        .len  = req->n_tag,
        .tags = req->tags,
    };
    neu_json_decode_tag_array_fini(&arr);

    free(req->node);
    free(req->group);
}

static int decode_add_tags_req(void *json_obj, neu_json_add_tags_req_t *req)
{
    int ret = 0;

    neu_json_elem_t req_elems[] = {
        {
            .name = "driver",
            .t    = NEU_JSON_STR,
        },
        {
            .name = "group",
            .t    = NEU_JSON_STR,
        },
        {
            .name = "tags",
            .t    = NEU_JSON_OBJECT,
        },
    };
    ret = neu_json_decode_by_json(json_obj, NEU_JSON_ELEM_SIZE(req_elems),
                                  req_elems);
    if (0 != ret) {
        goto decode_fail;
    }

    neu_json_tag_array_t arr = { 0 };
    ret = neu_json_decode_tag_array_json(req_elems[2].v.val_object, &arr);
    if (0 != ret) {
        goto decode_fail;
    }

    req->node  = req_elems[0].v.val_str;
    req->group = req_elems[1].v.val_str;
    req->n_tag = arr.len;
    req->tags  = arr.tags;

    return 0;

decode_fail:
    free(req_elems[0].v.val_str);
    free(req_elems[1].v.val_str);
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

static int decode_subscribe_req(json_t *json_obj, neu_json_subscribe_req_t *req)
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
        goto error;
    }

    ret = neu_json_dump_key(json_obj, "params", &req->params, false);
    if (0 != ret) {
        goto error;
    }

    req->driver = req_elems[0].v.val_str;
    req->group  = req_elems[1].v.val_str;
    return 0;

error:
    free(req_elems[0].v.val_str);
    free(req_elems[1].v.val_str);
    return -1;
}

static inline void fini_subscribe_req(neu_json_subscribe_req_t *req)
{
    free(req->app);
    free(req->driver);
    free(req->group);
    free(req->params);
}

int neu_json_decode_global_config_req_subscriptions(
    void *json_obj, neu_json_global_config_req_subscriptions_t **result)
{
    int                                         n_subscription = 0;
    json_t *                                    sub_array      = NULL;
    neu_json_subscribe_req_t *                  p              = NULL;
    neu_json_global_config_req_subscriptions_t *req            = NULL;

    sub_array = json_object_get(json_obj, "subscriptions");
    if (!json_is_array(sub_array)) {
        nlog_error("no `subscriptions` array field");
        return -1;
    }

    for (size_t i = 0; i < json_array_size(sub_array); ++i) {
        json_t *grp_array =
            json_object_get(json_array_get(sub_array, i), "groups");
        if (!json_is_array(grp_array)) {
            nlog_error("no subscriptions `group` field");
            return -1;
        }
        n_subscription += json_array_size(grp_array);
    }

    req = calloc(1, sizeof(*req));
    if (NULL == req) {
        nlog_error("calloc fail");
        return -1;
    }

    req->n_subscription = n_subscription;
    req->subscriptions =
        calloc(req->n_subscription, sizeof(*req->subscriptions));
    if (NULL == req->subscriptions) {
        free(req);
        nlog_error("calloc fail");
        return -1;
    }

    p = req->subscriptions;
    for (size_t i = 0; i < json_array_size(sub_array); ++i) {
        json_t *sub_obj = json_array_get(sub_array, i);
        json_t *app_obj = json_object_get(sub_obj, "app");

        if (NULL == app_obj) {
            nlog_error("no subscriptions `app` field");
            goto error;
        }

        json_t *grp_array = json_object_get(sub_obj, "groups");
        for (size_t j = 0; j < json_array_size(grp_array); ++j) {
            p->app = strdup(json_string_value(app_obj));
            if (NULL == p->app) {
                nlog_error("strdup fail");
                goto error;
            }
            if (0 != decode_subscribe_req(json_array_get(grp_array, j), p)) {
                free(p->app);
                goto error;
            }
            ++p;
        }
    }

    *result = req;
    return 0;

error:
    while (--p >= req->subscriptions) {
        fini_subscribe_req(p);
    }
    free(req->subscriptions);
    free(req);
    return -1;
}

void neu_json_decode_global_config_req_subscriptions_free(
    neu_json_global_config_req_subscriptions_t *req_sub)
{
    if (NULL == req_sub) {
        return;
    }

    for (int i = 0; i < req_sub->n_subscription; ++i) {
        fini_subscribe_req(&req_sub->subscriptions[i]);
    }

    free(req_sub->subscriptions);
    free(req_sub);
}

static inline void fini_node_setting(neu_json_node_setting_req_t *req)
{
    free(req->node);
    free(req->setting);
}

int neu_json_decode_global_config_req_settings(
    void *json_obj, neu_json_global_config_req_settings_t **result)
{
    json_t *settings_array = json_object_get(json_obj, "settings");
    if (!json_is_array(settings_array)) {
        nlog_error("no `settings` array field");
        return -1;
    }

    neu_json_global_config_req_settings_t *req = calloc(1, sizeof(*req));
    if (NULL == req) {
        nlog_error("calloc fail");
        return -1;
    }

    req->n_setting = json_array_size(settings_array);
    req->settings  = calloc(req->n_setting, sizeof(*req->settings));
    if (NULL == req->settings) {
        free(req);
        nlog_error("calloc fail");
        return -1;
    }

    neu_json_node_setting_req_t *p = req->settings;
    for (int i = 0; i < req->n_setting; ++i) {
        json_t *setting_obj = json_array_get(settings_array, i);
        json_t *node_obj    = json_object_get(setting_obj, "node");

        if (NULL == node_obj) {
            nlog_error("no settings `node` field");
            goto error;
        }

        p->node = strdup(json_string_value(node_obj));
        if (NULL == p->node) {
            nlog_error("strdup fail");
            goto error;
        }

        if (0 != neu_json_dump_key(setting_obj, "params", &p->setting, true)) {
            free(p->node);
            nlog_error("no settings `params` field");
            goto error;
        }
        ++p;
    }

    *result = req;
    return 0;

error:
    while (--p >= req->settings) {
        fini_node_setting(p);
    }
    free(req->settings);
    free(req);
    return -1;
}

void neu_json_decode_global_config_req_settings_free(
    neu_json_global_config_req_settings_t *req_settings)
{
    if (req_settings) {
        for (int i = 0; i < req_settings->n_setting; ++i) {
            fini_node_setting(&req_settings->settings[i]);
        }
        free(req_settings->settings);
        free(req_settings);
    }
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
        0 == neu_json_decode_global_config_req_tags(json_obj, &req->tags) &&
        0 ==
            neu_json_decode_global_config_req_subscriptions(
                json_obj, &req->subscriptions) &&
        0 ==
            neu_json_decode_global_config_req_settings(json_obj,
                                                       &req->settings)) {
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
        neu_json_decode_global_config_req_subscriptions_free(
            req->subscriptions);
        neu_json_decode_global_config_req_settings_free(req->settings);
        free(req);
    }
}

int neu_json_encode_driver(void *node_json, void *param)
{
    neu_json_driver_t *driver = param;

    if (!json_is_object((json_t *) node_json)) {
        return -1;
    }

    if (0 != neu_json_encode_add_node_req(node_json, &driver->node)) {
        return -1;
    }

    json_t *groups_json = json_array();
    if (NULL == groups_json ||
        0 != json_object_set_new(node_json, "groups", groups_json)) {
        return -1;
    }

    return neu_json_encode_gtag_array(groups_json, &driver->gtags);
}

int neu_json_decode_driver_json(void *node_json, neu_json_driver_t *driver_p)
{
    if (!json_is_object((json_t *) node_json)) {
        return -1;
    }

    if (0 != neu_json_decode_add_node_req_json(node_json, &driver_p->node)) {
        return -1;
    }

    // setting required
    if (NULL == driver_p->node.setting) {
        neu_json_decode_add_node_req_fini(&driver_p->node);
        return -1;
    }

    json_t *groups_json = json_object_get(node_json, "groups");
    if (NULL == groups_json) {
        return -1;
    }

    if (!json_is_array(groups_json)) {
        neu_json_decode_add_node_req_fini(&driver_p->node);
        return -1;
    }

    if (0 != neu_json_decode_gtag_array_json(groups_json, &driver_p->gtags)) {
        neu_json_decode_add_node_req_fini(&driver_p->node);
        return -1;
    }

    return 0;
}

void neu_json_driver_fini(neu_json_driver_t *req)
{
    neu_json_decode_add_node_req_fini(&req->node);
    neu_json_decode_gtag_array_fini(&req->gtags);
}

int neu_json_encode_driver_array(void *obj_json, void *param)
{
    neu_json_driver_array_t *arr = param;

    json_t *nodes_json = obj_json;
    if (!json_is_array(nodes_json)) {
        return -1;
    }

    for (int i = 0; i < arr->n_driver; ++i) {
        json_t *node_json = json_object();
        if (NULL == node_json ||
            0 != json_array_append_new(nodes_json, node_json)) {
            return -1;
        }
        if (0 != neu_json_encode_driver(node_json, &arr->drivers[i])) {
            return -1;
        }
    }

    return 0;
}

int neu_json_decode_driver_array_json(void *                   obj_json,
                                      neu_json_driver_array_t *arr)
{
    json_t *nodes_json = obj_json;
    if (!json_is_array(nodes_json)) {
        return -1;
    }

    int                n_driver = json_array_size(nodes_json);
    neu_json_driver_t *drivers  = calloc(n_driver, sizeof(*drivers));
    if (NULL == drivers) {
        return -1;
    }

    for (int i = 0; i < n_driver; ++i) {
        if (0 !=
            neu_json_decode_driver_json(json_array_get(nodes_json, i),
                                        &drivers[i])) {
            while (--i >= 0) {
                neu_json_driver_fini(&drivers[i]);
            }
            free(drivers);
            return -1;
        }
    }

    arr->n_driver = n_driver;
    arr->drivers  = drivers;
    return 0;
}

void neu_json_driver_array_fini(neu_json_driver_array_t *arr)
{
    for (int i = 0; i < arr->n_driver; ++i) {
        neu_json_driver_fini(&arr->drivers[i]);
    }
    free(arr->drivers);
}

int neu_json_encode_drivers_req(void *obj_json, void *param)
{
    json_t *root = obj_json;
    if (!json_is_object(root)) {
        return -1;
    }

    json_t *nodes_json = json_array();
    if (NULL == nodes_json ||
        0 != json_object_set_new(root, "nodes", nodes_json)) {
        return -1;
    }

    return neu_json_encode_driver_array(nodes_json, param);
}

int neu_json_decode_drivers_req(char *buf, neu_json_drivers_req_t **result)
{
    int                     ret = 0;
    neu_json_drivers_req_t *req = calloc(1, sizeof(*req));
    if (req == NULL) {
        return -1;
    }

    void *json_obj = neu_json_decode_new(buf);
    if (NULL == json_obj) {
        free(req);
        return -1;
    }

    json_t *nodes_json = json_object_get(json_obj, "nodes");
    if (!json_is_array(nodes_json)) {
        neu_json_decode_free(json_obj);
        free(req);
        return -1;
    }

    ret = neu_json_decode_driver_array_json(nodes_json, req);
    if (0 == ret) {
        *result = req;
    } else {
        free(req);
    }

    neu_json_decode_free(json_obj);
    return ret;
}

void neu_json_decode_drivers_req_free(neu_json_drivers_req_t *req)
{
    if (req) {
        neu_json_driver_array_fini(req);
        free(req);
    }
}

static void fini_app_group(neu_json_app_group_t *grp)
{
    free(grp->driver);
    free(grp->group);
    free(grp->params);
}

int neu_json_encode_app(void *node_json, void *param)
{
    neu_json_app_t *app = param;

    if (!json_is_object((json_t *) node_json)) {
        return -1;
    }

    if (0 != neu_json_encode_add_node_req(node_json, &app->node)) {
        return -1;
    }

    json_t *groups_json = json_array();
    if (NULL == groups_json ||
        0 != json_object_set_new(node_json, "groups", groups_json)) {
        return -1;
    }

    for (int i = 0; i < app->subscriptions.n_group; ++i) {
        json_t *grp_json = json_object();
        if (NULL == grp_json) {
            return -1;
        }

        neu_json_app_group_t *grp         = &app->subscriptions.groups[i];
        neu_json_elem_t       grp_elems[] = {
            {
                .name      = "driver",
                .t         = NEU_JSON_STR,
                .v.val_str = grp->driver,
            },
            {
                .name      = "group",
                .t         = NEU_JSON_STR,
                .v.val_str = grp->group,
            },
        };

        if (0 !=
            neu_json_encode_field(grp_json, grp_elems,
                                  NEU_JSON_ELEM_SIZE(grp_elems))) {
            json_decref(grp_json);
            return -1;
        }

        if (grp->params && strlen(grp->params) > 0) {
            if (0 !=
                neu_json_load_key(grp_json, "params", grp->params, false)) {
                json_decref(grp_json);
                return -1;
            }
        }

        if (0 != json_array_append_new(groups_json, grp_json)) {
            return -1;
        }
    }

    return 0;
}

int neu_json_decode_app_json(void *node_json, neu_json_app_t *app_p)
{
    if (!json_is_object((json_t *) node_json)) {
        return -1;
    }

    if (0 != neu_json_decode_add_node_req_json(node_json, &app_p->node)) {
        return -1;
    }

    json_t *groups_json = json_object_get(node_json, "groups");
    if (NULL == groups_json) {
        app_p->subscriptions.n_group = 0;
        app_p->subscriptions.groups  = NULL;
        return 0;
    }

    if (!json_is_array(groups_json)) {
        neu_json_decode_add_node_req_fini(&app_p->node);
        return -1;
    }

    int n_group = json_array_size(groups_json);
    if (n_group <= 0) {
        app_p->subscriptions.n_group = 0;
        app_p->subscriptions.groups  = NULL;
        return 0;
    }

    neu_json_app_group_t *groups = calloc(n_group, sizeof(*groups));
    if (NULL == groups) {
        neu_json_decode_add_node_req_fini(&app_p->node);
        return -1;
    }

    for (int i = 0; i < n_group; ++i) {
        json_t *grp_json = json_array_get(groups_json, i);

        neu_json_elem_t grp_elems[] = {
            {
                .name = "driver",
                .t    = NEU_JSON_STR,
            },
            {
                .name = "group",
                .t    = NEU_JSON_STR,
            },
        };

        if (0 !=
            neu_json_decode_by_json(grp_json, NEU_JSON_ELEM_SIZE(grp_elems),
                                    grp_elems)) {
            for (int j = 0; j < i; ++j) {
                fini_app_group(&groups[j]);
            }
            free(groups);
            neu_json_decode_add_node_req_fini(&app_p->node);
            return -1;
        }

        groups[i].driver = grp_elems[0].v.val_str;
        groups[i].group  = grp_elems[1].v.val_str;
        neu_json_dump_key(grp_json, "params", &groups[i].params, false);
    }

    app_p->subscriptions.n_group = n_group;
    app_p->subscriptions.groups  = groups;

    return 0;
}

void neu_json_app_fini(neu_json_app_t *app)
{
    neu_json_decode_add_node_req_fini(&app->node);
    for (int i = 0; i < app->subscriptions.n_group; ++i) {
        fini_app_group(&app->subscriptions.groups[i]);
    }
    free(app->subscriptions.groups);
}

int neu_json_encode_app_array(void *obj_json, void *param)
{
    neu_json_app_array_t *arr = param;

    json_t *nodes_json = obj_json;
    if (!json_is_array(nodes_json)) {
        return -1;
    }

    for (int i = 0; i < arr->n_app; ++i) {
        json_t *node_json = json_object();
        if (NULL == node_json ||
            0 != json_array_append_new(nodes_json, node_json)) {
            return -1;
        }
        if (0 != neu_json_encode_app(node_json, &arr->apps[i])) {
            return -1;
        }
    }

    return 0;
}

int neu_json_decode_app_array_json(void *obj_json, neu_json_app_array_t *arr)
{
    json_t *nodes_json = obj_json;
    if (!json_is_array(nodes_json)) {
        return -1;
    }

    int             n_app = json_array_size(nodes_json);
    neu_json_app_t *apps  = calloc(n_app, sizeof(*apps));
    if (NULL == apps) {
        return -1;
    }

    for (int i = 0; i < n_app; ++i) {
        if (0 !=
            neu_json_decode_app_json(json_array_get(nodes_json, i), &apps[i])) {
            while (--i >= 0) {
                neu_json_app_fini(&apps[i]);
            }
            free(apps);
            return -1;
        }
    }

    arr->n_app = n_app;
    arr->apps  = apps;
    return 0;
}

void neu_json_app_array_fini(neu_json_app_array_t *arr)
{
    for (int i = 0; i < arr->n_app; ++i) {
        neu_json_app_fini(&arr->apps[i]);
    }
    free(arr->apps);
}

int neu_json_encode_apps_req(void *obj_json, void *param)
{
    json_t *root = obj_json;
    if (!json_is_object(root)) {
        return -1;
    }

    json_t *nodes_json = json_array();
    if (NULL == nodes_json ||
        0 != json_object_set_new(root, "nodes", nodes_json)) {
        return -1;
    }

    return neu_json_encode_app_array(nodes_json, param);
}

int neu_json_decode_apps_req(char *buf, neu_json_apps_req_t **result)
{
    int                  ret = 0;
    neu_json_apps_req_t *req = calloc(1, sizeof(*req));
    if (req == NULL) {
        return -1;
    }

    void *json_obj = neu_json_decode_new(buf);
    if (NULL == json_obj) {
        free(req);
        return -1;
    }

    json_t *nodes_json = json_object_get(json_obj, "nodes");
    if (!json_is_array(nodes_json)) {
        neu_json_decode_free(json_obj);
        free(req);
        return -1;
    }

    ret = neu_json_decode_app_array_json(nodes_json, req);
    if (0 == ret) {
        *result = req;
    } else {
        free(req);
    }

    neu_json_decode_free(json_obj);
    return ret;
}

void neu_json_decode_apps_req_free(neu_json_apps_req_t *req)
{
    if (req) {
        neu_json_app_array_fini(req);
        free(req);
    }
}
