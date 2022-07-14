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

#ifndef _NEU_PERSIST_PERSIST_
#define _NEU_PERSIST_PERSIST_

#ifdef __cplusplus
extern "C" {
#endif

#include "adapter/adapter_info.h"
#include "persist/json/persist_json_plugin.h"
#include "persist/json/persist_json_subs.h"

typedef neu_json_plugin_req_plugin_t neu_persist_plugin_info_t;

typedef struct {
    char *  name;
    int64_t type;
    char *  plugin_name;
    int64_t state;
} neu_persist_node_info_t;

typedef struct {
    uint32_t interval;
    char *   name;
} neu_persist_group_info_t;

typedef neu_json_subscriptions_req_subscription_t
    neu_persist_subscription_info_t;

static inline void neu_persist_plugin_infos_free(UT_array *plugin_infos)
{
    utarray_free(plugin_infos);
}

static inline void neu_persist_node_info_fini(neu_persist_node_info_t *info)
{
    free(info->name);
    free(info->plugin_name);
}

static inline void neu_persist_group_info_fini(neu_persist_group_info_t *info)
{
    free(info->name);
}

static inline void
neu_persist_subscription_infos_free(UT_array *subscription_infos)
{
    utarray_foreach(subscription_infos, neu_persist_subscription_info_t *, info)
    {
        free(info->group_config_name);
        free(info->src_adapter_name);
        free(info->sub_adapter_name);
    }
    utarray_free(subscription_infos);
}

/**
 * Persister, provide methods to persist data */
typedef struct neu_persister neu_persister_t;

const char *neu_persister_get_persist_dir(neu_persister_t *persister);
const char *neu_persister_get_plugins_fname(neu_persister_t *persister);

/**
 * Create persister.
 * @return Pointer to heap allocated neu_persister_t on success, NULL otherwise.
 */
neu_persister_t *neu_persister_create(const char *dir_name);
/**
 * Destroy perister.
 * @param persister                 persiter object to destroy.
 */
void neu_persister_destroy(neu_persister_t *persiter);

/**
 * Persist nodes.
 * @param persister                 persiter object.
 * @param node_info                 neu_persist_node_info_t.
 * @return 0 on success, non-zero on failure
 */
int neu_persister_store_node(neu_persister_t *        persister,
                             neu_persist_node_info_t *info);
/**
 * Load node infos.
 * @param persister                 persiter object.
 * @param[out] node_infos           used to return pointer to heap allocated
 *                                  vector of neu_persist_node_info_t.
 * @return 0 on success, none-zero on failure
 */
int neu_persister_load_nodes(neu_persister_t *persister, UT_array **node_infos);
/**
 * Delete node.
 * @param persister                 persiter object.
 * @param node_name                 name of the node to delete.
 * @return 0 on success, none-zero on failure
 */
int neu_persister_delete_node(neu_persister_t *persister,
                              const char *     node_name);

/**
 * Update node.
 * @param persister                 persiter object.
 * @param node_name                 name of the node to update.
 * @param new_name                  new name of the adapter.
 * @return 0 on success, none-zero on failure
 */
int neu_persister_update_node(neu_persister_t *persister, const char *node_name,
                              const char *new_name);

/**
 * Persist plugins.
 * @param persister                 persiter object.
 * @param plugin_infos              vector of neu_persist_plugin_info_t.
 * @return 0 on success, non-zero otherwise
 */
int neu_persister_store_plugins(neu_persister_t *persister,
                                UT_array *       plugin_infos);
/**
 * Load plugin infos.
 * @param persister                 persiter object.
 * @param[out] plugin_infos         used to return pointer to heap allocated
 * vector of neu_persist_plugin_info_t.
 * @return 0 on success, none-zero on failure
 */
int neu_persister_load_plugins(neu_persister_t *persister,
                               UT_array **      plugin_infos);

/**
 * Persist node tags.
 * @param persister                 persiter object.
 * @param driver_name               name of the driver who owns the tags
 * @param group_name                name of the group
 * @param tag                       the tag to store
 * @return 0 on success, non-zero otherwise
 */
int neu_persister_store_tag(neu_persister_t *persister, const char *driver_name,
                            const char *group_name, const neu_datatag_t *tag);

/**
 * Load node tag infos.
 * @param persister                 persiter object.
 * @param node_name                 name of the node who owns the tags
 * @param group_name                name of the group
 * @param[out] tag_infos            used to return pointer to heap allocated
 *                                  vector of neu_datatag_t
 * @return 0 on success, non-zero otherwise
 */
int neu_persister_load_tags(neu_persister_t *persister, const char *driver_name,
                            const char *group_name, UT_array **tag_infos);

/**
 * Update node tags.
 * @param persister                 persiter object.
 * @param driver_name               name of the driver who owns the tags
 * @param group_name                name of the group
 * @param tag                       the tag to update
 * @return 0 on success, non-zero otherwise
 */
int neu_persister_update_tag(neu_persister_t *persister,
                             const char *driver_name, const char *group_name,
                             const neu_datatag_t *tag);

/**
 * Delete node tags.
 * @param persister                 persiter object.
 * @param driver_name               name of the driver who owns the tags
 * @param group_name                name of the group
 * @param tag_name                  name of the tag
 * @return 0 on success, non-zero otherwise
 */
int neu_persister_delete_tag(neu_persister_t *persister,
                             const char *driver_name, const char *group_name,
                             const char *tag_name);

/**
 * Persist adapter subscriptions.
 * @param persister                 persiter object.
 * @param adapter_name              name of the adapter who owns the
 * subscriptions.
 * @param subscription_infos        vector of neu_persist_subscription_info_t.
 * @return 0 on success, non-zero otherwise
 */
int neu_persister_store_subscriptions(neu_persister_t *persister,
                                      const char *     adapter_name,
                                      UT_array *       subscription_infos);
/**
 * Load adapter subscription infos.
 * @param persister                 persiter object.
 * @param adapter_name              name of the adapter who owns the
 * subscriptions.
 * @param[out] subscription_infos   used to return pointer to heap allocated
 * vector of neu_persist_subscription_info_t.
 * @return 0 on success, non-zero otherwise
 */
int neu_persister_load_subscriptions(neu_persister_t *persister,
                                     const char *     adapter_name,
                                     UT_array **      subscription_infos);

/**
 * Persist group config.
 * @param persister                 persiter object.
 * @param driver_name               name of the driver who owns the group
 * @param group_info                group info to persist.
 * @return 0 on success, non-zero otherwise
 */
int neu_persister_store_group(neu_persister_t *         persister,
                              const char *              driver_name,
                              neu_persist_group_info_t *group_info);
/**
 * Load all group config infos under an adapter.
 * @param persister                 persiter object.
 * @param driver_name               name of the driver who owns the group
 * @param[out] group_infos          used to return pointer to heap allocated
 *                                  vector of neu_persist_group_info_t.
 * @return 0 on success, non-zero otherwise
 */
int neu_persister_load_groups(neu_persister_t *persister,
                              const char *driver_name, UT_array **group_infos);
/**
 * Delete group config.
 * @param persister                 persiter object.
 * @param driver_name               name of the driver who owns the group
 * @param group_name                name of the group.
 * @return 0 on success, none-zero on failure
 */
int neu_persister_delete_group(neu_persister_t *persister,
                               const char *driver_name, const char *group_name);

/**
 * Persist node setting.
 * @param persister                 persiter object.
 * @param adapter_name              name of the adapter who owns the setting.
 * @param setting                   node setting string.
 * @return 0 on success, non-zero otherwise
 */
int neu_persister_store_node_setting(neu_persister_t *persister,
                                     const char *     node_name,
                                     const char *     setting);
/**
 * Load node setting.
 * @param persister                 persiter object.
 * @param node_name                 name of the node.
 * @param[out] setting              used to return node setting string.
 * @return 0 on success, non-zero otherwise
 */
int neu_persister_load_node_setting(neu_persister_t *  persister,
                                    const char *       node_name,
                                    const char **const setting);
/**
 * Delete node setting.
 * @param persister                 persiter object.
 * @param node_name                 name of the node.
 * @return 0 on success, none-zero on failure
 */
int neu_persister_delete_node_setting(neu_persister_t *persister,
                                      const char *     node_name);

#ifdef __cplusplus
}
#endif

#endif