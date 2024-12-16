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

#ifndef NEU_PERSIST_SQLITE_PERSISTER
#define NEU_PERSIST_SQLITE_PERSISTER

#ifdef __cplusplus
extern "C" {
#endif

#include "persist/persist_impl.h"

typedef struct {
    struct neu_persister_vtbl_s *vtbl;
    sqlite3 *                    db;
} neu_sqlite_persister_t;

neu_persister_t *neu_sqlite_persister_create(const char *schema_dir);

void  neu_sqlite_persister_destroy(neu_persister_t *self);
void *neu_sqlite_persister_native_handle(neu_persister_t *self);

int neu_sqlite_persister_store_node(neu_persister_t *        self,
                                    neu_persist_node_info_t *info);
int neu_sqlite_persister_load_nodes(neu_persister_t *self,
                                    UT_array **      node_infos);
int neu_sqlite_persister_delete_node(neu_persister_t *self,
                                     const char *     node_name);
int neu_sqlite_persister_update_node(neu_persister_t *self,
                                     const char *     node_name,
                                     const char *     new_name);
int neu_sqlite_persister_update_node_state(neu_persister_t *self,
                                           const char *node_name, int state);
int neu_sqlite_persister_store_tag(neu_persister_t *    self,
                                   const char *         driver_name,
                                   const char *         group_name,
                                   const neu_datatag_t *tag);
int neu_sqlite_persister_store_tags(neu_persister_t *    self,
                                    const char *         driver_name,
                                    const char *         group_name,
                                    const neu_datatag_t *tags, size_t n);
int neu_sqlite_persister_load_tags(neu_persister_t *self,
                                   const char *     driver_name,
                                   const char *     group_name,
                                   UT_array **      tag_infos);
int neu_sqlite_persister_update_tag(neu_persister_t *    self,
                                    const char *         driver_name,
                                    const char *         group_name,
                                    const neu_datatag_t *tag);
int neu_sqlite_persister_update_tag_value(neu_persister_t *    self,
                                          const char *         driver_name,
                                          const char *         group_name,
                                          const neu_datatag_t *tag);
int neu_sqlite_persister_delete_tag(neu_persister_t *self,
                                    const char *     driver_name,
                                    const char *     group_name,
                                    const char *     tag_name);
int neu_sqlite_persister_store_subscription(neu_persister_t *self,
                                            const char *     app_name,
                                            const char *     driver_name,
                                            const char *     group_name,
                                            const char *     params);
int neu_sqlite_persister_update_subscription(neu_persister_t *self,
                                             const char *     app_name,
                                             const char *     driver_name,
                                             const char *     group_name,
                                             const char *     params);
int neu_sqlite_persister_load_subscriptions(neu_persister_t *self,
                                            const char *     app_name,
                                            UT_array **subscription_infos);
int neu_sqlite_persister_delete_subscription(neu_persister_t *self,
                                             const char *     app_name,
                                             const char *     driver_name,
                                             const char *     group_name);
int neu_sqlite_persister_store_group(neu_persister_t *         self,
                                     const char *              driver_name,
                                     neu_persist_group_info_t *group_info,
                                     const char *              context);
int neu_sqlite_persister_update_group(neu_persister_t *         self,
                                      const char *              driver_name,
                                      const char *              group_name,
                                      neu_persist_group_info_t *group_info);
int neu_sqlite_persister_load_groups(neu_persister_t *self,
                                     const char *     driver_name,
                                     UT_array **      group_infos);
int neu_sqlite_persister_delete_group(neu_persister_t *self,
                                      const char *     driver_name,
                                      const char *     group_name);
int neu_sqlite_persister_store_node_setting(neu_persister_t *self,
                                            const char *     node_name,
                                            const char *     setting);
int neu_sqlite_persister_load_node_setting(neu_persister_t *  self,
                                           const char *       node_name,
                                           const char **const setting);
int neu_sqlite_persister_delete_node_setting(neu_persister_t *self,
                                             const char *     node_name);
int neu_sqlite_persister_load_users(neu_persister_t *self,
                                    UT_array **      user_infos);
int neu_sqlite_persister_store_user(neu_persister_t *              self,
                                    const neu_persist_user_info_t *user);
int neu_sqlite_persister_update_user(neu_persister_t *              self,
                                     const neu_persist_user_info_t *user);
int neu_sqlite_persister_load_user(neu_persister_t *self, const char *user_name,
                                   neu_persist_user_info_t **user_p);
int neu_sqlite_persister_delete_user(neu_persister_t *self,
                                     const char *     user_name);

#ifdef __cplusplus
}
#endif

#endif
