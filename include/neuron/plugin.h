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

#ifndef NEURON_PLUGIN_H
#define NEURON_PLUGIN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "utils/utextend.h"
#include "utils/zlog.h"

#include "event/event.h"

#include "adapter.h"
#include "define.h"
#include "type.h"

#define NEURON_PLUGIN_VER_1_0 100
#define NEURON_PLUGIN_VER_2_0 200

#define NEU_PLUGIN_REGISTER_METRIC(plugin, name, init) \
    plugin->common.adapter_callbacks->register_metric( \
        plugin->common.adapter, name, name##_HELP, name##_TYPE, init)

#define NEU_PLUGIN_UPDATE_METRIC(plugin, name, val, grp)                    \
    plugin->common.adapter_callbacks->update_metric(plugin->common.adapter, \
                                                    name, val, grp)

extern int64_t global_timestamp;

typedef struct neu_plugin_common {
    uint32_t magic;

    neu_adapter_t *            adapter;
    const adapter_callbacks_t *adapter_callbacks;
    char                       name[NEU_NODE_NAME_LEN];

    neu_node_link_state_e link_state;

    zlog_category_t *log;
} neu_plugin_common_t;

typedef struct neu_plugin neu_plugin_t;

typedef struct neu_plugin_group neu_plugin_group_t;
typedef void (*neu_plugin_group_free)(neu_plugin_group_t *pgp);
struct neu_plugin_group {
    char *    group_name;
    UT_array *tags;

    void *                user_data;
    neu_plugin_group_free group_free;
};

typedef int (*neu_plugin_tag_validator_t)(const neu_datatag_t *tag);

typedef struct {
    neu_datatag_t *tag;
    neu_value_u    value;
} neu_plugin_tag_value_t;

typedef struct neu_plugin_intf_funs {
    neu_plugin_t *(*open)(void);
    int (*close)(neu_plugin_t *plugin);
    int (*init)(neu_plugin_t *plugin, bool load);
    int (*uninit)(neu_plugin_t *plugin);
    int (*start)(neu_plugin_t *plugin);
    int (*stop)(neu_plugin_t *plugin);
    int (*setting)(neu_plugin_t *plugin, const char *setting);

    int (*request)(neu_plugin_t *plugin, neu_reqresp_head_t *head, void *data);

    union {
        struct {
            int (*validate_tag)(neu_plugin_t *plugin, neu_datatag_t *tag);
            int (*group_timer)(neu_plugin_t *plugin, neu_plugin_group_t *group);
            int (*group_sync)(neu_plugin_t *plugin, neu_plugin_group_t *group);
            int (*write_tag)(neu_plugin_t *plugin, void *req,
                             neu_datatag_t *tag, neu_value_u value);
            int (*write_tags)(
                neu_plugin_t *plugin, void *req,
                UT_array *tag_values); // UT_array {neu_datatag_t, neu_value_u}
            neu_plugin_tag_validator_t tag_validator;

            int (*load_tags)(
                neu_plugin_t *plugin, const char *group, neu_datatag_t *tags,
                int n_tag); // create tags using data from the database
            int (*add_tags)(neu_plugin_t *plugin, const char *group,
                            neu_datatag_t *tags,
                            int            n_tag); // create tags by API
            int (*del_tags)(neu_plugin_t *plugin, int n_tag);
        } driver;
    };

} neu_plugin_intf_funs_t;

typedef struct neu_plugin_module {
    const uint32_t                version;
    const char *                  schema;
    const char *                  module_name;
    const char *                  module_descr;
    const char *                  module_descr_zh;
    const neu_plugin_intf_funs_t *intf_funs;
    neu_node_type_e               type;
    neu_plugin_kind_e             kind;
    bool                          display;
    bool                          single;
    const char *                  single_name;
    neu_event_timer_type_e        timer_type;
    neu_tag_cache_type_e          cache_type;
} neu_plugin_module_t;

inline static neu_plugin_common_t *
neu_plugin_to_plugin_common(neu_plugin_t *plugin)
{
    return (neu_plugin_common_t *) plugin;
}

/**
 * @brief when creating a node, initialize the common parameter in
 *        the plugin.
 *
 * @param[in] common refers to common parameter in plugins.
 */
void neu_plugin_common_init(neu_plugin_common_t *common);

/**
 * @brief Check the common parameter in the plugin.
 *
 * @param[in] plugin represents plugin information.
 * @return  Returns true if the check is correct,false otherwise.
 */
bool neu_plugin_common_check(neu_plugin_t *plugin);

/**
 * @brief Encapsulate the request,convert the request into a message and send.
 *
 * @param[in] plugin
 * @param[in] head the request header function.
 * @param[in] data that different request headers correspond to different data.
 *
 * @return 0 for successful message processing, non-0 for message processing
 * failure.
 */
int neu_plugin_op(neu_plugin_t *plugin, neu_reqresp_head_t head, void *data);

#ifdef __cplusplus
}
#endif

#endif
