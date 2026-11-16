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

#ifndef NEURON_PLUGIN_MQTT_V2_TEMPLATE_H
#define NEURON_PLUGIN_MQTT_V2_TEMPLATE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "utils/utarray.h"

typedef enum {
    // message template, rendered once per report of a group
    MQTT_V2_TMPL_DATA = 0,
    // rendered for every tag expanded by `${tags}`, `${tags_kv}` and
    // `${tags[name]}`
    MQTT_V2_TMPL_TAG = 1,
} mqtt_v2_tmpl_kind_e;

typedef struct mqtt_v2_tmpl mqtt_v2_tmpl_t;

typedef struct {
    const char *description;
    const char *unit;
} mqtt_v2_tag_info_t;

// looks up tag metadata that reported data does not carry
typedef bool (*mqtt_v2_tag_info_fn)(void *ctx, const char *tag,
                                    mqtt_v2_tag_info_t *info);

typedef struct {
    const char *          app;
    const char *          device;
    const char *          group;
    const char *          sub_topic;
    int64_t               timestamp;
    UT_array *            tags; // neu_resp_tag_value_meta_t
    bool                  upload_err;
    const mqtt_v2_tmpl_t *tag_tmpl;
    mqtt_v2_tag_info_fn   tag_info; // may be NULL
    void *                tag_info_ctx;
} mqtt_v2_render_ctx_t;

/**
 * Compile the JSON template `text`.
 *
 * Variables are written as `${name}`, quoted or not. On failure, a reason is
 * written to `err` and -1 is returned.
 */
int  mqtt_v2_tmpl_compile(const char *text, mqtt_v2_tmpl_kind_e kind,
                          mqtt_v2_tmpl_t **tmpl, char *err, size_t err_size);
void mqtt_v2_tmpl_free(mqtt_v2_tmpl_t *tmpl);

/**
 * Render a data template, returns a JSON string to be freed by the caller,
 * or NULL if nothing was rendered.
 */
char *mqtt_v2_tmpl_render(const mqtt_v2_tmpl_t *      tmpl,
                          const mqtt_v2_render_ctx_t *ctx);

// number of tags that `${tags}` expands to
size_t mqtt_v2_tags_count(UT_array *tags, bool upload_err);

/**
 * Substitute `${app}`, `${device}` and `${group}` in a topic template.
 *
 * Wildcard characters in the substituted names are replaced by `_`.
 */
char *mqtt_v2_topic_render(const char *tmpl, const char *app,
                           const char *device, const char *group);

#ifdef __cplusplus
}
#endif

#endif
