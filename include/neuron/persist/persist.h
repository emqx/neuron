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

#include <sqlite3.h>

#include "adapter_info.h"
#include "persist/json/persist_json_plugin.h"

typedef neu_json_plugin_req_plugin_t neu_persist_plugin_info_t;

typedef struct {
    char *name;
    int   type;
    char *plugin_name;
    int   state;
} neu_persist_node_info_t;

typedef struct {
    uint32_t interval;
    char *   name;
    char *   context;
} neu_persist_group_info_t;

typedef struct {
    char *driver_name;
    char *group_name;
    char *params;
} neu_persist_subscription_info_t;

typedef struct {
    char *name;
    char *hash;
} neu_persist_user_info_t;

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
    if (info->context) {
        free(info->context);
    }
}

static inline void
neu_persist_subscription_info_fini(neu_persist_subscription_info_t *info)
{
    free(info->driver_name);
    free(info->group_name);
    free(info->params);
}

static inline void neu_persist_user_info_fini(neu_persist_user_info_t *info)
{
    free(info->name);
    free(info->hash);
}

/**
 * Create persister.
 * @return 0 on success, -1 otherwise.
 */
int neu_persister_create(const char *schema_dir);
/**
 * Destroy perister.
 */
void neu_persister_destroy();

sqlite3 *neu_persister_get_db();

/**
 * Persist nodes.
 * @param node_info                 neu_persist_node_info_t.
 * @return 0 on success, non-zero on failure
 */
int neu_persister_store_node(neu_persist_node_info_t *info);
/**
 * Load node infos.
 * @param[out] node_infos           used to return pointer to heap allocated
 *                                  vector of neu_persist_node_info_t.
 * @return 0 on success, none-zero on failure
 */
int neu_persister_load_nodes(UT_array **node_infos);
/**
 * Delete node.
 * @param node_name                 name of the node to delete.
 * @return 0 on success, none-zero on failure
 */
int neu_persister_delete_node(const char *node_name);

/**
 * Update node name.
 * @param node_name                 name of the node to update.
 * @param new_name                  new name of the adapter.
 * @return 0 on success, none-zero on failure
 */
int neu_persister_update_node(const char *node_name, const char *new_name);

/**
 * Update node state.
 * @param node_name                 name of the node to update.
 * @param state                     state of the adapter.
 * @return 0 on success, none-zero on failure
 */
int neu_persister_update_node_state(const char *node_name, int state);

/**
 * Persist plugins.
 * @param plugin_infos              vector of neu_persist_plugin_info_t.
 * @return 0 on success, non-zero otherwise
 */
int neu_persister_store_plugins(UT_array *plugin_infos);
/**
 * Load plugin infos.
 * @param[out] plugin_infos         used to return pointer to heap allocated
 * vector of neu_persist_plugin_info_t.
 * @return 0 on success, none-zero on failure
 */
int neu_persister_load_plugins(UT_array **plugin_infos);

/**
 * Persist node tags.
 * @param driver_name               name of the driver who owns the tags
 * @param group_name                name of the group
 * @param tag                       the tag to store
 * @return 0 on success, non-zero otherwise
 */
int neu_persister_store_tag(const char *driver_name, const char *group_name,
                            const neu_datatag_t *tag);

int neu_persister_store_tags(const char *driver_name, const char *group_name,
                             const neu_datatag_t *tags, size_t n);

/**
 * Load node tag infos.
 * @param node_name                 name of the node who owns the tags
 * @param group_name                name of the group
 * @param[out] tag_infos            used to return pointer to heap allocated
 *                                  vector of neu_datatag_t
 * @return 0 on success, non-zero otherwise
 */
int neu_persister_load_tags(const char *driver_name, const char *group_name,
                            UT_array **tag_infos);

/**
 * Update node tags.
 * @param driver_name               name of the driver who owns the tags
 * @param group_name                name of the group
 * @param tag                       the tag to update
 * @return 0 on success, non-zero otherwise
 */
int neu_persister_update_tag(const char *driver_name, const char *group_name,
                             const neu_datatag_t *tag);

/**
 * Update node tag value.
 * @param driver_name               name of the driver who owns the tags
 * @param group_name                name of the group
 * @param tag                       the tag to update
 * @return 0 on success, non-zero otherwise
 */
int neu_persister_update_tag_value(const char *         driver_name,
                                   const char *         group_name,
                                   const neu_datatag_t *tag);

/**
 * Delete node tags.
 * @param driver_name               name of the driver who owns the tags
 * @param group_name                name of the group
 * @param tag_name                  name of the tag
 * @return 0 on success, non-zero otherwise
 */
int neu_persister_delete_tag(const char *driver_name, const char *group_name,
                             const char *tag_name);

/**
 * Persist subscriptions.
 * @param app_name                  name of the app node
 * @param driver_name               name of the driver node
 * @param group_name                name of the group
 * @param params                    subscription params
 * @return 0 on success, non-zero otherwise
 */
int neu_persister_store_subscription(const char *app_name,
                                     const char *driver_name,
                                     const char *group_name,
                                     const char *params);

/**
 * Update subscriptions.
 * @param app_name                  name of the app node
 * @param driver_name               name of the driver node
 * @param group_name                name of the group
 * @param params                    subscription params
 * @return 0 on success, non-zero otherwise
 */
int neu_persister_update_subscription(const char *app_name,
                                      const char *driver_name,
                                      const char *group_name,
                                      const char *params);

/**
 * Load adapter subscriptions.
 * @param app_name                  name of the app node
 * @param[out] subscription_infos   used to return pointer to heap allocated
 *                                  vector of neu_persist_subscription_info_t.
 * @return 0 on success, non-zero otherwise
 */
int neu_persister_load_subscriptions(const char *app_name,
                                     UT_array ** subscription_infos);

/**
 * Persist subscriptions.
 * @param app_name                  name of the app node
 * @param driver_name               name of the driver node
 * @param group_name                name of the group
 * @return 0 on success, non-zero otherwise
 */
int neu_persister_delete_subscription(const char *app_name,
                                      const char *driver_name,
                                      const char *group_name);

/**
 * Persist group config.
 * @param driver_name               name of the driver who owns the group
 * @param group_info                group info to persist.
 * @return 0 on success, non-zero otherwise
 */
int neu_persister_store_group(const char *              driver_name,
                              neu_persist_group_info_t *group_info,
                              const char *              context);

/**
 * Update group config.
 * @param driver_name               name of the driver who owns the group
 * @param group_info                group info to persist.
 * @return 0 on success, non-zero otherwise
 */
int neu_persister_update_group(const char *driver_name, const char *group_name,
                               neu_persist_group_info_t *group_info);

/**
 * Load all group config infos under an adapter.
 * @param driver_name               name of the driver who owns the group
 * @param[out] group_infos          used to return pointer to heap allocated
 *                                  vector of neu_persist_group_info_t.
 * @return 0 on success, non-zero otherwise
 */
int neu_persister_load_groups(const char *driver_name, UT_array **group_infos);
/**
 * Delete group config.
 * @param driver_name               name of the driver who owns the group
 * @param group_name                name of the group.
 * @return 0 on success, none-zero on failure
 */
int neu_persister_delete_group(const char *driver_name, const char *group_name);

/**
 * Persist node setting.
 * @param adapter_name              name of the adapter who owns the setting.
 * @param setting                   node setting string.
 * @return 0 on success, non-zero otherwise
 */
int neu_persister_store_node_setting(const char *node_name,
                                     const char *setting);
/**
 * Load node setting.
 * @param node_name                 name of the node.
 * @param[out] setting              used to return node setting string.
 * @return 0 on success, non-zero otherwise
 */
int neu_persister_load_node_setting(const char *       node_name,
                                    const char **const setting);
/**
 * Delete node setting.
 * @param node_name                 name of the node.
 * @return 0 on success, none-zero on failure
 */
int neu_persister_delete_node_setting(const char *node_name);

/**
 * Load all user infos.
 * @param[out] user_infos           used to return pointer to heap allocated
 *                                  vector of neu_persist_user_info_t.
 * @return 0 on success, non-zero otherwise
 */
int neu_persister_load_users(UT_array **user_infos);

/**
 * Save user info.
 * @param user                      user info
 * @return 0 on success, none-zero on failure
 */
int neu_persister_store_user(const neu_persist_user_info_t *user);

/**
 * Update user info.
 * @param user                      user info
 * @return 0 on success, none-zero on failure
 */
int neu_persister_update_user(const neu_persist_user_info_t *user);

/**
 * Load user info.
 * @param user_name                 name of the user.
 * @param user_p                    output parameter of user info
 * @return 0 on success, none-zero on failure
 */
int neu_persister_load_user(const char *              user_name,
                            neu_persist_user_info_t **user_p);

/**
 * Delete user info.
 * @param user_name                 name of the user.
 * @return 0 on success, none-zero on failure
 */
int neu_persister_delete_user(const char *user_name);

char *neu_persister_save_file_tmp(const char *file_data, uint32_t len,
                                  const char *suffix);

bool neu_persister_library_exists(const char *library);

#ifdef __cplusplus
}
#endif

#endif
