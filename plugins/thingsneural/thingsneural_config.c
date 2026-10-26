/**
 * NEURON IIoT System for Industry 4.0
 * Copyright (C) 2020-2026 EMQ Technologies Co., Ltd All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 **/

#include <jansson.h>
#include <string.h>

#include "utils/asprintf.h"

#include "thingsneural_plugin.h"

static int copy_name(char *dst, size_t dst_len, const char *src)
{
    size_t len = 0;

    if (NULL == src || '\0' == src[0]) {
        return -1;
    }
    len = strlen(src);
    if (len >= dst_len) {
        return -1;
    }
    memcpy(dst, src, len + 1);
    return 0;
}

static char *build_topic(const char *suffix, const char *product_key,
                         const char *device_key)
{
    char *topic = NULL;
    if (neu_asprintf(&topic, "/iot/thing/%s/%s/property/%s", product_key,
                     device_key, suffix) <= 0) {
        return NULL;
    }
    return topic;
}

static void points_free(tn_point_t **points)
{
    tn_point_t *point = NULL;
    tn_point_t *tmp   = NULL;

    HASH_ITER(hh, *points, point, tmp)
    {
        HASH_DEL(*points, point);
        free(point);
    }
}

void tn_points_free(tn_point_t **points)
{
    points_free(points);
}

void tn_points_rename_node(tn_point_t *points, const char *node,
                           const char *new_name)
{
    tn_point_t *point = NULL;
    tn_point_t *tmp   = NULL;

    HASH_ITER(hh, points, point, tmp)
    {
        if (0 == strcmp(point->node, node)) {
            snprintf(point->node, sizeof(point->node), "%s", new_name);
        }
    }
}

void tn_points_rename_group(tn_point_t *points, const char *node,
                            const char *group, const char *new_name)
{
    tn_point_t *point = NULL;
    tn_point_t *tmp   = NULL;

    HASH_ITER(hh, points, point, tmp)
    {
        if (0 == strcmp(point->node, node) &&
            0 == strcmp(point->group, group)) {
            snprintf(point->group, sizeof(point->group), "%s", new_name);
        }
    }
}

static int parse_points(neu_plugin_t *plugin, const char *text,
                        tn_point_t **points)
{
    json_error_t error;
    json_t *     root  = NULL;
    size_t       index = 0;
    json_t *     item  = NULL;
    int          rv    = 0;

    *points = NULL;
    if (NULL == text || '\0' == text[0]) {
        return 0;
    }

    root = json_loads(text, 0, &error);
    if (NULL == root || !json_is_array(root)) {
        plog_error(plugin, "points is not a JSON array");
        json_decref(root);
        return -1;
    }

    json_array_foreach(root, index, item)
    {
        tn_point_t *point    = NULL;
        tn_point_t *existing = NULL;
        const char *property = NULL;
        const char *node     = NULL;
        const char *group    = NULL;
        const char *tag      = NULL;

        if (!json_is_object(item)) {
            plog_error(plugin, "points[%zu] is not an object", index);
            rv = -1;
            break;
        }
        property = json_string_value(json_object_get(item, "property"));
        node     = json_string_value(json_object_get(item, "node"));
        group    = json_string_value(json_object_get(item, "group"));
        tag      = json_string_value(json_object_get(item, "tag"));

        point = calloc(1, sizeof(*point));
        if (NULL == point ||
            0 !=
                copy_name(point->property, sizeof(point->property), property) ||
            0 != copy_name(point->node, sizeof(point->node), node) ||
            0 != copy_name(point->group, sizeof(point->group), group) ||
            0 != copy_name(point->tag, sizeof(point->tag), tag)) {
            plog_error(plugin, "points[%zu] missing property/node/group/tag",
                       index);
            free(point);
            rv = -1;
            break;
        }

        HASH_FIND_STR(*points, point->property, existing);
        if (NULL != existing) {
            plog_error(plugin, "duplicate property:%s", point->property);
            free(point);
            rv = -1;
            break;
        }
        HASH_ADD_STR(*points, property, point);
    }

    json_decref(root);
    if (0 != rv) {
        points_free(points);
    }
    return rv;
}

static void take_optional_string(neu_json_elem_t *elem, char **out)
{
    if (NULL == elem->v.val_str || '\0' == elem->v.val_str[0]) {
        free(elem->v.val_str);
        elem->v.val_str = NULL;
        return;
    }
    *out            = elem->v.val_str;
    elem->v.val_str = NULL;
}

int tn_config_parse(neu_plugin_t *plugin, const char *setting,
                    tn_config_t *config, tn_point_t **points)
{
    int             rv        = 0;
    neu_json_elem_t version   = { .name      = "version",
                                .t         = NEU_JSON_INT,
                                .v.val_int = 4,
                                .attribute = NEU_JSON_ATTRIBUTE_OPTIONAL };
    neu_json_elem_t qos       = { .name      = "qos",
                            .t         = NEU_JSON_INT,
                            .attribute = NEU_JSON_ATTRIBUTE_OPTIONAL };
    neu_json_elem_t client_id = { .name = "client-id", .t = NEU_JSON_STR };
    neu_json_elem_t host      = { .name = "host", .t = NEU_JSON_STR };
    neu_json_elem_t port      = { .name = "port", .t = NEU_JSON_INT };
    neu_json_elem_t username  = { .name      = "username",
                                 .t         = NEU_JSON_STR,
                                 .attribute = NEU_JSON_ATTRIBUTE_OPTIONAL };
    neu_json_elem_t password  = { .name      = "password",
                                 .t         = NEU_JSON_STR,
                                 .attribute = NEU_JSON_ATTRIBUTE_OPTIONAL };
    neu_json_elem_t ssl       = { .name      = "ssl",
                            .t         = NEU_JSON_BOOL,
                            .attribute = NEU_JSON_ATTRIBUTE_OPTIONAL };
    neu_json_elem_t product   = { .name = "product-key", .t = NEU_JSON_STR };
    neu_json_elem_t device    = { .name = "device-key", .t = NEU_JSON_STR };
    neu_json_elem_t points_el = { .name = "points", .t = NEU_JSON_STR };

    memset(config, 0, sizeof(*config));
    *points = NULL;

    if (0 !=
        neu_parse_param(setting, NULL, 9, &version, &qos, &client_id, &host,
                        &port, &product, &device, &points_el, &ssl)) {
        plog_error(plugin, "parse required setting fail");
        rv = -1;
        goto end;
    }
    neu_parse_param(setting, NULL, 2, &username, &password);

    if (port.v.val_int <= 0 || port.v.val_int > 65535 || qos.v.val_int < 0 ||
        qos.v.val_int > 2 ||
        (version.v.val_int != NEU_MQTT_VERSION_V311 &&
         version.v.val_int != NEU_MQTT_VERSION_V5)) {
        plog_error(plugin, "invalid port, qos or mqtt version");
        rv = -1;
        goto end;
    }

    config->version     = version.v.val_int;
    config->qos         = qos.v.val_int;
    config->port        = (uint16_t) port.v.val_int;
    config->ssl         = ssl.v.val_bool;
    config->client_id   = client_id.v.val_str;
    config->host        = host.v.val_str;
    config->product_key = product.v.val_str;
    config->device_key  = device.v.val_str;
    client_id.v.val_str = NULL;
    host.v.val_str      = NULL;
    product.v.val_str   = NULL;
    device.v.val_str    = NULL;
    take_optional_string(&username, &config->username);
    take_optional_string(&password, &config->password);

    config->report_topic =
        build_topic("report/request", config->product_key, config->device_key);
    config->set_req_topic =
        build_topic("set/request", config->product_key, config->device_key);
    config->set_resp_topic =
        build_topic("set/response", config->product_key, config->device_key);
    if (NULL == config->report_topic || NULL == config->set_req_topic ||
        NULL == config->set_resp_topic) {
        rv = -1;
        goto end;
    }

    if (0 != parse_points(plugin, points_el.v.val_str, points)) {
        rv = -1;
        goto end;
    }

end:
    free(client_id.v.val_str);
    free(host.v.val_str);
    free(username.v.val_str);
    free(password.v.val_str);
    free(product.v.val_str);
    free(device.v.val_str);
    free(points_el.v.val_str);
    if (0 != rv) {
        tn_config_fini(config);
        points_free(points);
    }
    return rv;
}

void tn_config_fini(tn_config_t *config)
{
    free(config->client_id);
    free(config->host);
    free(config->username);
    free(config->password);
    free(config->product_key);
    free(config->device_key);
    free(config->report_topic);
    free(config->set_req_topic);
    free(config->set_resp_topic);
    memset(config, 0, sizeof(*config));
}
