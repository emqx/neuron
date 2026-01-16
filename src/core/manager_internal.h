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

#include "base/msg_internal.h"
#include "msg.h"
#include "node_manager.h"
#include "plugin_manager.h"
#include "subscribe.h"
#include "utils/log.h"

struct neu_manager {
    int server_fd;

    neu_events_t *  events;
    neu_event_io_t *loop;

    neu_plugin_manager_t *plugin_manager;
    neu_node_manager_t *  node_manager;
    neu_subscribe_mgr_t * subscribe_manager;

    neu_event_timer_t *timer_timestamp;

    int64_t timestamp_lev_manager;

    int log_level;
};

int       neu_manager_add_plugin(neu_manager_t *manager, const char *library);
int       neu_manager_del_plugin(neu_manager_t *manager, const char *plugin);
UT_array *neu_manager_get_plugins(neu_manager_t *manager);

int       neu_manager_add_node(neu_manager_t *manager, const char *node_name,
                               const char *plugin_name, const char *setting,
                               const char *tags, neu_node_running_state_e state,
                               bool load);
int       neu_manager_del_node(neu_manager_t *manager, const char *node_name);
UT_array *neu_manager_get_nodes(neu_manager_t *manager, int type,
                                const char *plugin, const char *node,
                                bool sort_delay, bool q_state, int state,
                                bool q_link, int link,
                                const char *q_group_name);
int       neu_manager_update_node_name(neu_manager_t *manager, const char *node,
                                       const char *new_name);
int neu_manager_update_group_name(neu_manager_t *manager, const char *driver,
                                  const char *group, const char *new_name);

UT_array *neu_manager_get_driver_group(neu_manager_t *manager);

int       neu_manager_subscribe(neu_manager_t *manager, const char *app,
                                const char *driver, const char *group,
                                const char *params, const char *static_tags,
                                uint16_t *app_port);
int       neu_manager_update_subscribe(neu_manager_t *manager, const char *app,
                                       const char *driver, const char *group,
                                       const char *params, const char *static_tags);
int       neu_manager_send_subscribe(neu_manager_t *manager, const char *app,
                                     const char *driver, const char *group,
                                     uint16_t app_port, const char *params,
                                     const char *static_tags);
int       neu_manager_unsubscribe(neu_manager_t *manager, const char *app,
                                  const char *driver, const char *group);
UT_array *neu_manager_get_sub_group(neu_manager_t *manager, const char *app);
UT_array *neu_manager_get_sub_group_deep_copy(neu_manager_t *manager,
                                              const char *   app,
                                              const char *   driver,
                                              const char *   group);
UT_array *neu_manager_get_driver_groups(neu_manager_t *manager, const char *app,
                                        const char *name);

int neu_manager_get_node_info(neu_manager_t *manager, const char *name,
                              neu_persist_node_info_t *info);

int neu_manager_add_drivers(neu_manager_t *         manager,
                            neu_req_driver_array_t *req);

inline static void forward_msg(neu_manager_t *     manager,
                               neu_reqresp_head_t *header, const char *node)
{
    struct sockaddr_un addr =
        neu_node_manager_get_addr(manager->node_manager, node);

    neu_reqresp_type_e t                           = header->type;
    char               receiver[NEU_NODE_NAME_LEN] = { 0 };
    strncpy(receiver, header->receiver, sizeof(receiver));

    neu_msg_t *msg = (neu_msg_t *) header;
    int        ret = neu_send_msg_to(manager->server_fd, &addr, msg);
    if (0 == ret) {
        nlog_info("forward msg %s to %s", neu_reqresp_type_string(t), receiver);
    } else {
        nlog_warn("forward msg %s to node (%s)%s %s %s fail",
                  neu_reqresp_type_string(t), receiver, &addr.sun_path[1], node,
                  header->sender);
        neu_msg_free(msg);
    }
}

#endif
