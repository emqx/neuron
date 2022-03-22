/**
 * NEURON IIoT System for Industry 4.0
 * Copyright (C) 2020-2021 EMQ Technologies Co., Ltd All rights reserved.
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
#include "neu_vector.h"
#include "persist/json/persist_json_adapter.h"
#include "persist/json/persist_json_datatags.h"
#include "persist/json/persist_json_group_configs.h"
#include "persist/json/persist_json_plugin.h"
#include "persist/json/persist_json_subs.h"

typedef neu_json_node_req_node_t     neu_persist_adapter_info_t;
typedef neu_json_plugin_req_plugin_t neu_persist_plugin_info_t;
typedef neu_json_datatag_req_tag_t   neu_persist_datatag_info_t;
typedef neu_json_group_configs_req_t neu_persist_group_config_info_t;
typedef neu_json_subscriptions_req_subscription_t
    neu_persist_subscription_info_t;

static inline void neu_persist_adapter_infos_free(vector_t *adapter_infos)
{
    VECTOR_FOR_EACH(adapter_infos, iter)
    {
        neu_persist_adapter_info_t *p =
            (neu_persist_adapter_info_t *) iterator_get(&iter);
        free(p->name);
        free(p->plugin_name);
    }
    vector_free(adapter_infos);
}

static inline void neu_persist_plugin_infos_free(vector_t *plugin_infos)
{
    VECTOR_FOR_EACH(plugin_infos, iter)
    {
        neu_persist_plugin_info_t *p =
            (neu_persist_plugin_info_t *) iterator_get(&iter);
        free(*p);
    }
    vector_free(plugin_infos);
}

static inline void
neu_persist_group_config_infos_free(vector_t *group_config_infos)
{
    VECTOR_FOR_EACH(group_config_infos, iter)
    {
        neu_persist_group_config_info_t *p =
            (neu_persist_group_config_info_t *) iterator_get(&iter);
        free(p->group_config_name);
        free(p->adapter_name);
    }
    vector_free(group_config_infos);
}

static inline void neu_persist_datatag_infos_free(vector_t *datatag_infos)
{
    VECTOR_FOR_EACH(datatag_infos, iter)
    {
        neu_persist_datatag_info_t *p =
            (neu_persist_datatag_info_t *) iterator_get(&iter);
        free(p->name);
        free(p->address);
    }
    vector_free(datatag_infos);
}

static inline void
neu_persist_subscription_infos_free(vector_t *subscription_infos)
{
    VECTOR_FOR_EACH(subscription_infos, iter)
    {
        neu_persist_subscription_info_t *p =
            (neu_persist_subscription_info_t *) iterator_get(&iter);
        free(p->src_adapter_name);
        free(p->sub_adapter_name);
        free(p->group_config_name);
    }
    vector_free(subscription_infos);
}

/**
 * Persister, provide methods to persist data */
typedef struct neu_persister neu_persister_t;

const char *neu_persister_get_persist_dir(neu_persister_t *persister);
const char *neu_persister_get_adapters_fname(neu_persister_t *persister);
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
 * Persist adapters.
 * @param persister                 persiter object.
 * @param adapter_infos             vector of neu_persist_adapter_info_t.
 * @return 0 on success, non-zero on failure
 */
int neu_persister_store_adapters(neu_persister_t *persister,
                                 vector_t *       adapter_infos);
/**
 * Load adapter infos.
 * @param persister                 persiter object.
 * @param[out] adapter_infos        used to return pointer to heap allocated
 * vector of neu_persist_adapter_info_t.
 * @return 0 on success, none-zero on failure
 */
int neu_persister_load_adapters(neu_persister_t *persister,
                                vector_t **      adapter_infos);
/**
 * Delete adapter.
 * @param persister                 persiter object.
 * @param adapter_name              name of the adapter to delete.
 * @return 0 on success, none-zero on failure
 */
int neu_persister_delete_adapter(neu_persister_t *persister,
                                 const char *     adapter_name);

/**
 * Update adapter.
 * @param persister                 persiter object.
 * @param adapter_name              name of the adapter to update.
 * @param new_name                  new name of the adapter.
 * @return 0 on success, none-zero on failure
 */
int neu_persister_update_adapter(neu_persister_t *persister,
                                 const char *     adapter_name,
                                 const char *     new_name);

/**
 * Persist plugins.
 * @param persister                 persiter object.
 * @param plugin_infos              vector of neu_persist_plugin_info_t.
 * @return 0 on success, non-zero otherwise
 */
int neu_persister_store_plugins(neu_persister_t *persister,
                                vector_t *       plugin_infos);
/**
 * Load plugin infos.
 * @param persister                 persiter object.
 * @param[out] plugin_infos         used to return pointer to heap allocated
 * vector of neu_persist_plugin_info_t.
 * @return 0 on success, none-zero on failure
 */
int neu_persister_load_plugins(neu_persister_t *persister,
                               vector_t **      plugin_infos);

/**
 * Persist adapter datatags.
 * @param persister                 persiter object.
 * @param adapter_name              name of the adapter who owns the datatags.
 * @param group_config_name         name of the group config.
 * @param datatag_infos             vector of neu_persist_datatag_info_t.
 * @return 0 on success, non-zero otherwise
 */
int neu_persister_store_datatags(neu_persister_t *persister,
                                 const char *     adapter_name,
                                 const char *     group_config_name,
                                 vector_t *       datatag_infos);
/**
 * Load adapter datatag infos.
 * @param persister                 persiter object.
 * @param adapter_name              name of the adapter who owns the datatags.
 * @param group_config_name         name of the group config.
 * @param[out] datatag_infos        used to return pointer to heap allocated
 * vector of neu_persist_datatag_info_t.
 * @return 0 on success, non-zero otherwise
 */
int neu_persister_load_datatags(neu_persister_t *persister,
                                const char *     adapter_name,
                                const char *     group_config_name,
                                vector_t **      datatag_infos);

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
                                      vector_t *       subscription_infos);
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
                                     vector_t **      subscription_infos);

/**
 * Persist group config.
 * @param persister                 persiter object.
 * @param adapter_name              name of the adapter who owns the group
 * config.
 * @param group_config_info         group config info to persist.
 * @return 0 on success, non-zero otherwise
 */
int neu_persister_store_group_config(
    neu_persister_t *persister, const char *adapter_name,
    neu_persist_group_config_info_t *group_config_info);
/**
 * Load all group config infos under an adapter.
 * @param persister                 persiter object.
 * @param adapter_name              name of the adapter who owns the group
 * configs.
 * @param[out] group_config_infos   used to return pointer to heap allocated
 * vector of neu_persist_group_config_info_t.
 * @return 0 on success, non-zero otherwise
 */
int neu_persister_load_group_configs(neu_persister_t *persister,
                                     const char *     adapter_name,
                                     vector_t **      group_config_infos);
/**
 * Delete group config.
 * @param persister                 persiter object.
 * @param adapter_name              name of the adapter who owns the group
 * config.
 * @param group_config_name         name of the group config.
 * @return 0 on success, none-zero on failure
 */
int neu_persister_delete_group_config(neu_persister_t *persister,
                                      const char *     adapter_name,
                                      const char *     group_config_name);

/**
 * Persist adapter setting.
 * @param persister                 persiter object.
 * @param adapter_name              name of the adapter who owns the setting.
 * @param setting                   adapter setting string.
 * @return 0 on success, non-zero otherwise
 */
int neu_persister_store_adapter_setting(neu_persister_t *persister,
                                        const char *     adapter_name,
                                        const char *     setting);
/**
 * Load adapter setting.
 * @param persister                 persiter object.
 * @param adapter_name              name of the adapter.
 * @param[out] setting              used to return adapter setting string.
 * @return 0 on success, non-zero otherwise
 */
int neu_persister_load_adapter_setting(neu_persister_t *  persister,
                                       const char *       adapter_name,
                                       const char **const setting);
/**
 * Delete adapter setting.
 * @param persister                 persiter object.
 * @param adapter_name              name of the adapter.
 * @return 0 on success, none-zero on failure
 */
int neu_persister_delete_adapter_setting(neu_persister_t *persister,
                                         const char *     adapter_name);

#ifdef __cplusplus
}
#endif

#endif