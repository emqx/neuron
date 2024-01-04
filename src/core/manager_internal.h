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

#ifndef _NEU_MANAGER_INTERNAL_H_
#define _NEU_MANAGER_INTERNAL_H_

#include "event/event.h"
#include "persist/persist.h"

#include "node_manager.h"
#include "plugin_manager.h"
#include "subscribe.h"
#include "template_manager.h"

typedef struct neu_manager {
    int server_fd;

    neu_events_t *  events;
    neu_event_io_t *loop;

    neu_plugin_manager_t *  plugin_manager;
    neu_node_manager_t *    node_manager;
    neu_subscribe_mgr_t *   subscribe_manager;
    neu_template_manager_t *template_manager;

    neu_event_timer_t *timer_timestamp;

    int64_t timestamp_lev_manager;

    int log_level;
} neu_manager_t;

int       neu_manager_add_plugin(neu_manager_t *manager, const char *library);
int       neu_manager_del_plugin(neu_manager_t *manager, const char *plugin);
UT_array *neu_manager_get_plugins(neu_manager_t *manager);

int       neu_manager_add_node(neu_manager_t *manager, const char *node_name,
                               const char *             plugin_name,
                               neu_node_running_state_e state, bool load);
int       neu_manager_del_node(neu_manager_t *manager, const char *node_name);
UT_array *neu_manager_get_nodes(neu_manager_t *manager, int type,
                                const char *plugin, const char *node);
int       neu_manager_update_node_name(neu_manager_t *manager, const char *node,
                                       const char *new_name);
int neu_manager_update_group_name(neu_manager_t *manager, const char *driver,
                                  const char *group, const char *new_name);

int  neu_manager_add_template(neu_manager_t *manager, const char *name,
                              const char *plugin, uint16_t n_group,
                              neu_reqresp_template_group_t *groups);
int  neu_manager_del_template(neu_manager_t *manager, const char *name);
void neu_manager_clear_template(neu_manager_t *manager);
int  neu_manager_get_template(neu_manager_t *manager, const char *name,
                              neu_resp_get_template_t *resp);
int  neu_manager_get_templates(neu_manager_t *           manager,
                               neu_resp_get_templates_t *resp);
int  neu_manager_add_template_group(neu_manager_t *manager,
                                    const char *tmpl_name, const char *group,
                                    uint32_t interval);
int  neu_manager_update_template_group(neu_manager_t *                  manager,
                                       neu_req_update_template_group_t *req);
int  neu_manager_del_template_group(neu_manager_t *               manager,
                                    neu_req_del_template_group_t *req);
int  neu_manager_get_template_group(neu_manager_t *               manager,
                                    neu_req_get_template_group_t *req,
                                    UT_array **                   group_info_p);
int neu_manager_add_template_tags(neu_manager_t *manager, const char *tmpl_name,
                                  const char *group, uint16_t n_tag,
                                  neu_datatag_t *tags, uint16_t *index_p);
int neu_manager_update_template_tags(neu_manager_t *             manager,
                                     neu_req_add_template_tag_t *req,
                                     uint16_t *                  index_p);
int neu_manager_del_template_tags(neu_manager_t *             manager,
                                  neu_req_del_template_tag_t *req);
int neu_manager_get_template_tags(neu_manager_t *             manager,
                                  neu_req_get_template_tag_t *req,
                                  UT_array **                 tags_p);
int neu_manager_instantiate_template(neu_manager_t *manager,
                                     const char *   tmpl_name,
                                     const char *   node_name);
int neu_manager_instantiate_templates(neu_manager_t *           manager,
                                      neu_req_inst_templates_t *req);

UT_array *neu_manager_get_driver_group(neu_manager_t *manager);

int       neu_manager_subscribe(neu_manager_t *manager, const char *app,
                                const char *driver, const char *group,
                                const char *params, uint16_t *app_port);
int       neu_manager_update_subscribe(neu_manager_t *manager, const char *app,
                                       const char *driver, const char *group,
                                       const char *params);
int       neu_manager_send_subscribe(neu_manager_t *manager, const char *app,
                                     const char *driver, const char *group,
                                     uint16_t app_port, const char *params);
int       neu_manager_unsubscribe(neu_manager_t *manager, const char *app,
                                  const char *driver, const char *group);
UT_array *neu_manager_get_sub_group(neu_manager_t *manager, const char *app);
UT_array *neu_manager_get_sub_group_deep_copy(neu_manager_t *manager,
                                              const char *   app,
                                              const char *   driver,
                                              const char *   group);
int neu_manager_add_ndriver_map(neu_manager_t *manager, const char *ndriver,
                                const char *driver, const char *group);
int neu_manager_del_ndriver_map(neu_manager_t *manager, const char *ndriver,
                                const char *driver, const char *group);
int neu_manager_get_ndriver_maps(neu_manager_t *manager, const char *ndriver,
                                 UT_array **result);

int neu_manager_get_node_info(neu_manager_t *manager, const char *name,
                              neu_persist_node_info_t *info);
#endif
