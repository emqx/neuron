/**
 * NEURON IIoT System for Industry 4.0
 * Copyright (C) 2020-2026 EMQ Technologies Co., Ltd All rights reserved.
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

#include "utils/asprintf.h"

#include "mqtt_v2_device.h"
#include "mqtt_v2_template.h"

static char *device_topic(const char *prefix, const char *device,
                          const char *suffix)
{
    char *name  = mqtt_v2_topic_render("${device}", NULL, device, NULL);
    char *topic = NULL;

    if (NULL != name) {
        neu_asprintf(&topic, "%s/%s/%s", prefix, name, suffix);
        free(name);
    }
    return topic;
}

static void tags_free(mqtt_v2_tag_t *tags)
{
    mqtt_v2_tag_t *tag = NULL, *tmp = NULL;

    HASH_ITER(hh, tags, tag, tmp)
    {
        HASH_DEL(tags, tag);
        free(tag->name);
        free(tag->description);
        free(tag->unit);
        free(tag);
    }
}

static void group_free(mqtt_v2_group_t *group)
{
    free(group->topic_tmpl);
    free(group->topic);
    tags_free(group->tags);
    free(group);
}

static int group_cmp(const mqtt_v2_group_t *a, const mqtt_v2_group_t *b)
{
    return strcmp(a->name, b->name);
}

mqtt_v2_device_t *mqtt_v2_device_new(const char *name, const char *app,
                                     const char *prefix)
{
    mqtt_v2_device_t *device = calloc(1, sizeof(*device));

    if (NULL == device) {
        return NULL;
    }

    strncpy(device->name, name, sizeof(device->name) - 1);
    if (NULL != prefix && 0 != mqtt_v2_device_set_topics(device, app, prefix)) {
        mqtt_v2_device_free(device);
        return NULL;
    }

    return device;
}

void mqtt_v2_device_free(mqtt_v2_device_t *device)
{
    mqtt_v2_group_t *group = NULL, *tmp = NULL;

    HASH_ITER(hh, device->groups, group, tmp)
    {
        HASH_DEL(device->groups, group);
        group_free(group);
    }

    free(device->state_topic);
    free(device->data_topic);
    free(device->write_req_topic);
    free(device->write_resp_topic);
    free(device->write_sub_topic);
    free(device);
}

int mqtt_v2_device_set_topics(mqtt_v2_device_t *device, const char *app,
                              const char *prefix)
{
    char *state = device_topic(prefix, device->name, "state");
    char *data  = device_topic(prefix, device->name, "data");
    char *req   = device_topic(prefix, device->name, "write/request");
    char *resp  = device_topic(prefix, device->name, "write/response");

    if (NULL == state || NULL == data || NULL == req || NULL == resp) {
        free(state);
        free(data);
        free(req);
        free(resp);
        return -1;
    }

    free(device->state_topic);
    free(device->data_topic);
    free(device->write_req_topic);
    free(device->write_resp_topic);
    device->state_topic      = state;
    device->data_topic       = data;
    device->write_req_topic  = req;
    device->write_resp_topic = resp;

    mqtt_v2_group_t *group = NULL, *tmp = NULL;
    HASH_ITER(hh, device->groups, group, tmp)
    {
        if (NULL == group->topic_tmpl) {
            continue;
        }

        char *topic = mqtt_v2_topic_render(group->topic_tmpl, app, device->name,
                                           group->name);
        if (NULL == topic) {
            return -1;
        }
        free(group->topic);
        group->topic = topic;
    }

    return 0;
}

char *mqtt_v2_device_state_payload(const mqtt_v2_device_t *device,
                                   int64_t                 timestamp)
{
    json_t *root    = json_object();
    char *  payload = NULL;

    if (NULL == root) {
        return NULL;
    }

    json_object_set_new(root, "timestamp", json_integer(timestamp));
    json_object_set_new(root, "name", json_string(device->name));
    json_object_set_new(root, "state", json_integer(device->state.link));
    json_object_set_new(root, "running", json_integer(device->state.running));

    payload = json_dumps(root, 0);
    json_decref(root);
    return payload;
}

mqtt_v2_group_t *mqtt_v2_device_find_group(mqtt_v2_device_t *device,
                                           const char *      name)
{
    mqtt_v2_group_t *group = NULL;

    HASH_FIND_STR(device->groups, name, group);
    return group;
}

mqtt_v2_group_t *mqtt_v2_device_add_group(mqtt_v2_device_t *device,
                                          const char *      name)
{
    mqtt_v2_group_t *group = calloc(1, sizeof(*group));

    if (NULL == group) {
        return NULL;
    }

    strncpy(group->name, name, sizeof(group->name) - 1);
    HASH_ADD_STR(device->groups, name, group);
    HASH_SRT(hh, device->groups, group_cmp);
    return group;
}

void mqtt_v2_device_del_group(mqtt_v2_device_t *device, mqtt_v2_group_t *group)
{
    HASH_DEL(device->groups, group);
    group_free(group);
}

int mqtt_v2_device_rename_group(mqtt_v2_device_t *device,
                                mqtt_v2_group_t *group, const char *new_name,
                                const char *app)
{
    HASH_DEL(device->groups, group);
    memset(group->name, 0, sizeof(group->name));
    strncpy(group->name, new_name, sizeof(group->name) - 1);
    HASH_ADD_STR(device->groups, name, group);
    HASH_SRT(hh, device->groups, group_cmp);

    if (NULL == group->topic_tmpl) {
        return 0;
    }

    char *topic =
        mqtt_v2_topic_render(group->topic_tmpl, app, device->name, group->name);
    if (NULL == topic) {
        return -1;
    }
    free(group->topic);
    group->topic = topic;
    return 0;
}

int mqtt_v2_group_set_topic(mqtt_v2_group_t *group, char *topic_tmpl,
                            const char *app, const char *device)
{
    char *topic = NULL;

    if (NULL != topic_tmpl) {
        topic = mqtt_v2_topic_render(topic_tmpl, app, device, group->name);
        if (NULL == topic) {
            free(topic_tmpl);
            return -1;
        }
    }

    free(group->topic_tmpl);
    free(group->topic);
    group->topic_tmpl = topic_tmpl;
    group->topic      = topic;
    return 0;
}

void mqtt_v2_group_set_tags(mqtt_v2_group_t *group, UT_array *tags)
{
    tags_free(group->tags);
    group->tags = NULL;

    utarray_foreach(tags, neu_datatag_t *, datatag)
    {
        mqtt_v2_tag_t *tag = calloc(1, sizeof(*tag));
        if (NULL == tag) {
            return;
        }

        tag->name = strdup(datatag->name);
        tag->description =
            datatag->description ? strdup(datatag->description) : NULL;
        tag->unit      = datatag->unit ? strdup(datatag->unit) : NULL;
        tag->attribute = datatag->attribute;
        if (NULL == tag->name) {
            free(tag->description);
            free(tag->unit);
            free(tag);
            return;
        }

        HASH_ADD_KEYPTR(hh, group->tags, tag->name, strlen(tag->name), tag);
    }
}

mqtt_v2_tag_t *mqtt_v2_group_find_tag(mqtt_v2_group_t *group, const char *name)
{
    mqtt_v2_tag_t *tag = NULL;

    HASH_FIND_STR(group->tags, name, tag);
    return tag;
}
