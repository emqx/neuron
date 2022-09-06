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

#ifndef _NEU_ADAPTER_H_
#define _NEU_ADAPTER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "define.h"
#include "tag.h"
#include "type.h"

/** Kinds of node statistics counters.
 *
 * Some counters are maintained by the neuron core, while others should be
 * maintained by the plugin code using the adapter callback `stat_acc`.
 */
typedef enum {
    // kinds of counters maintained by the plugins
    NEU_NODE_STAT_BYTES_SENT, // number of bytes sent
    NEU_NODE_STAT_BYTES_RECV, // number of bytes received
    NEU_NODE_STAT_MSGS_SENT,  // number of messages sent
    NEU_NODE_STAT_MSGS_RECV,  // number of messages received

    // kinds of counters maintained by the driver core
    NEU_NODE_STAT_TAG_TOT_CNT, // number of tag read including errors
    NEU_NODE_STAT_TAG_ERR_CNT, // number of tag read errors

    NEU_NODE_STAT_MAX,
} neu_node_stat_e;

static inline const char *neu_node_stat_string(neu_node_stat_e s)
{
    static const char *map[] = {
        [NEU_NODE_STAT_BYTES_SENT]  = "bytes_sent",
        [NEU_NODE_STAT_BYTES_RECV]  = "bytes_received",
        [NEU_NODE_STAT_MSGS_SENT]   = "messages_sent",
        [NEU_NODE_STAT_MSGS_RECV]   = "messages_received",
        [NEU_NODE_STAT_TAG_TOT_CNT] = "tag_total_count",
        [NEU_NODE_STAT_TAG_ERR_CNT] = "tag_error_count",
        [NEU_NODE_STAT_MAX]         = NULL,
    };

    return map[s];
}

typedef struct {
    neu_node_running_state_e running;
    neu_node_link_state_e    link;
} neu_node_state_t;

typedef enum neu_reqresp_type {
    NEU_RESP_ERROR,
    NEU_REQ_UPDATE_LICENSE,

    NEU_REQ_READ_GROUP,
    NEU_RESP_READ_GROUP,
    NEU_REQ_WRITE_TAG,

    NEU_REQ_SUBSCRIBE_GROUP,
    NEU_REQ_UNSUBSCRIBE_GROUP,
    NEU_REQ_GET_SUBSCRIBE_GROUP,
    NEU_RESP_GET_SUBSCRIBE_GROUP,
    NEU_REQ_GET_SUB_DRIVER_TAGS,
    NEU_RESP_GET_SUB_DRIVER_TAGS,

    NEU_REQ_NODE_INIT,
    NEU_REQ_NODE_UNINIT,
    NEU_RESP_NODE_UNINIT,
    NEU_REQ_ADD_NODE,
    NEU_REQ_DEL_NODE,
    NEU_REQ_GET_NODE,
    NEU_RESP_GET_NODE,
    NEU_REQ_NODE_SETTING,
    NEU_REQ_GET_NODE_SETTING,
    NEU_RESP_GET_NODE_SETTING,
    NEU_REQ_GET_NODE_STATE,
    NEU_RESP_GET_NODE_STATE,
    NEU_REQ_GET_NODES_STATE,
    NEU_RESP_GET_NODES_STATE,
    NEU_REQ_NODE_CTL,

    NEU_REQ_ADD_GROUP,
    NEU_REQ_DEL_GROUP,
    NEU_REQ_UPDATE_GROUP,
    NEU_REQ_GET_GROUP,
    NEU_RESP_GET_GROUP,
    NEU_REQ_GET_DRIVER_GROUP,
    NEU_RESP_GET_DRIVER_GROUP,

    NEU_REQ_ADD_TAG,
    NEU_RESP_ADD_TAG,
    NEU_REQ_DEL_TAG,
    NEU_REQ_UPDATE_TAG,
    NEU_RESP_UPDATE_TAG,
    NEU_REQ_GET_TAG,
    NEU_RESP_GET_TAG,

    NEU_REQ_ADD_PLUGIN,
    NEU_REQ_DEL_PLUGIN,
    NEU_REQ_GET_PLUGIN,
    NEU_RESP_GET_PLUGIN,

    NEU_REQRESP_TRANS_DATA,
    NEU_REQRESP_NODES_STATE,
    NEU_REQRESP_NODE_DELETED,
} neu_reqresp_type_e;

typedef enum {
    NEU_SUBSCRIBE_NODES_STATE = NEU_REQRESP_NODES_STATE,
} neu_subscribe_type_e;

static const char *neu_reqresp_type_string_t[] = {
    [NEU_RESP_ERROR]         = "NEU_RESP_ERROR",
    [NEU_REQ_UPDATE_LICENSE] = "NEU_REQ_UPDATE_LICENSE",

    [NEU_REQ_READ_GROUP]  = "NEU_REQ_READ_GROUP",
    [NEU_RESP_READ_GROUP] = "NEU_RESP_READ_GROUP",
    [NEU_REQ_WRITE_TAG]   = "NEU_REQ_WRITE_TAG",

    [NEU_REQ_SUBSCRIBE_GROUP]      = "NEU_REQ_SUBSCRIBE_GROUP",
    [NEU_REQ_UNSUBSCRIBE_GROUP]    = "NEU_REQ_UNSUBSCRIBE_GROUP",
    [NEU_REQ_GET_SUBSCRIBE_GROUP]  = "NEU_REQ_GET_SUBSCRIBE_GROUP",
    [NEU_RESP_GET_SUBSCRIBE_GROUP] = "NEU_RESP_GET_SUBSCRIBE_GROUP",
    [NEU_REQ_GET_SUB_DRIVER_TAGS]  = "NEU_REQ_GET_SUB_DRIVER_TAGS",
    [NEU_RESP_GET_SUB_DRIVER_TAGS] = "NEU_RESP_GET_SUB_DRIVER_TAGS",

    [NEU_REQ_NODE_INIT]         = "NEU_REQ_NODE_INIT",
    [NEU_REQ_NODE_UNINIT]       = "NEU_REQ_NODE_UNINIT",
    [NEU_RESP_NODE_UNINIT]      = "NEU_RESP_NODE_UNINIT",
    [NEU_REQ_ADD_NODE]          = "NEU_REQ_ADD_NODE",
    [NEU_REQ_DEL_NODE]          = "NEU_REQ_DEL_NODE",
    [NEU_REQ_GET_NODE]          = "NEU_REQ_GET_NODE",
    [NEU_RESP_GET_NODE]         = "NEU_RESP_GET_NODE",
    [NEU_REQ_NODE_SETTING]      = "NEU_REQ_NODE_SETTING",
    [NEU_REQ_GET_NODE_SETTING]  = "NEU_REQ_GET_NODE_SETTING",
    [NEU_RESP_GET_NODE_SETTING] = "NEU_RESP_GET_NODE_SETTING",
    [NEU_REQ_GET_NODE_STATE]    = "NEU_REQ_GET_NODE_STATE",
    [NEU_RESP_GET_NODE_STATE]   = "NEU_RESP_GET_NODE_STATE",
    [NEU_REQ_GET_NODES_STATE]   = "NEU_REQ_GET_NODES_STATE",
    [NEU_RESP_GET_NODES_STATE]  = "NEU_RESP_GET_NODES_STATE",
    [NEU_REQ_NODE_CTL]          = "NEU_REQ_NODE_CTL",

    [NEU_REQ_ADD_GROUP]         = "NEU_REQ_ADD_GROUP",
    [NEU_REQ_DEL_GROUP]         = "NEU_REQ_DEL_GROUP",
    [NEU_REQ_UPDATE_GROUP]      = "NEU_REQ_UPDATE_GROUP",
    [NEU_REQ_GET_GROUP]         = "NEU_REQ_GET_GROUP",
    [NEU_RESP_GET_GROUP]        = "NEU_RESP_GET_GROUP",
    [NEU_REQ_GET_DRIVER_GROUP]  = "NEU_REQ_GET_DRIVER_GROUP",
    [NEU_RESP_GET_DRIVER_GROUP] = "NEU_RESP_GET_DRIVER_GROUP",

    [NEU_REQ_ADD_TAG]     = "NEU_REQ_ADD_TAG",
    [NEU_RESP_ADD_TAG]    = "NEU_RESP_ADD_TAG",
    [NEU_REQ_DEL_TAG]     = "NEU_REQ_DEL_TAG",
    [NEU_REQ_UPDATE_TAG]  = "NEU_REQ_UPDATE_TAG",
    [NEU_RESP_UPDATE_TAG] = "NEU_RESP_UPDATE_TAG",
    [NEU_REQ_GET_TAG]     = "NEU_REQ_GET_TAG",
    [NEU_RESP_GET_TAG]    = "NEU_RESP_GET_TAG",

    [NEU_REQ_ADD_PLUGIN]  = "NEU_REQ_ADD_PLUGIN",
    [NEU_REQ_DEL_PLUGIN]  = "NEU_REQ_DEL_PLUGIN",
    [NEU_REQ_GET_PLUGIN]  = "NEU_REQ_GET_PLUGIN",
    [NEU_RESP_GET_PLUGIN] = "NEU_RESP_GET_PLUGIN",

    [NEU_REQRESP_TRANS_DATA]   = "NEU_REQRESP_TRANS_DATA",
    [NEU_REQRESP_NODES_STATE]  = "NEU_REQRESP_NODES_STATE",
    [NEU_REQRESP_NODE_DELETED] = "NEU_REQRESP_NODE_DELETED",
};

inline static const char *neu_reqresp_type_string(neu_reqresp_type_e type)
{
    return neu_reqresp_type_string_t[type];
}

typedef struct neu_reqresp_head {
    neu_reqresp_type_e type;
    void *             ctx;
    char               sender[NEU_NODE_NAME_LEN];
    char               receiver[NEU_NODE_NAME_LEN];
} neu_reqresp_head_t;

typedef struct neu_resp_error {
    int error;
} neu_resp_error_t;

typedef struct {
    char node[NEU_NODE_NAME_LEN];
    bool auto_start;
} neu_req_node_init_t, neu_req_node_uninit_t, neu_resp_node_uninit_t;

typedef struct neu_req_add_plugin {
    char library[NEU_PLUGIN_LIBRARY_LEN];
} neu_req_add_plugin_t;

typedef struct neu_req_del_plugin {
    char plugin[NEU_PLUGIN_NAME_LEN];
} neu_req_del_plugin_t;

typedef struct neu_req_get_plugin {
} neu_req_get_plugin_t;

typedef struct neu_resp_plugin_info {
    char name[NEU_PLUGIN_NAME_LEN];
    char description[NEU_PLUGIN_DESCRIPTION_LEN];
    char library[NEU_PLUGIN_LIBRARY_LEN];

    neu_plugin_kind_e kind;
    neu_node_type_e   type;

    bool single;
    bool display;
    char single_name[NEU_NODE_NAME_LEN];
} neu_resp_plugin_info_t;

typedef struct neu_resp_get_plugin {
    UT_array *plugins; // array neu_resp_plugin_info_t
} neu_resp_get_plugin_t;

typedef struct neu_req_add_node {
    char node[NEU_NODE_NAME_LEN];
    char plugin[NEU_PLUGIN_NAME_LEN];
} neu_req_add_node_t;

typedef struct neu_req_del_node {
    char node[NEU_NODE_NAME_LEN];
} neu_req_del_node_t;

typedef struct neu_req_get_node {
    neu_node_type_e type;
} neu_req_get_node_t;

typedef struct neu_resp_node_info {
    char node[NEU_NODE_NAME_LEN];
    char plugin[NEU_PLUGIN_NAME_LEN];
} neu_resp_node_info_t;

typedef struct neu_resp_get_node {
    UT_array *nodes; // array neu_resp_node_info_t
} neu_resp_get_node_t;

typedef struct {
    char     driver[NEU_NODE_NAME_LEN];
    char     group[NEU_GROUP_NAME_LEN];
    uint32_t interval;
} neu_req_add_group_t, neu_req_update_group_t;

typedef struct neu_req_del_group {
    char driver[NEU_NODE_NAME_LEN];
    char group[NEU_GROUP_NAME_LEN];
} neu_req_del_group_t;

typedef struct neu_req_get_group {
    char driver[NEU_NODE_NAME_LEN];
} neu_req_get_group_t;

typedef struct neu_resp_group_info {
    char     name[NEU_GROUP_NAME_LEN];
    uint16_t tag_count;
    uint32_t interval;
} neu_resp_group_info_t;

typedef struct neu_resp_get_group {
    UT_array *groups; // array neu_resp_group_info_t
} neu_resp_get_group_t;

typedef struct {
    char     driver[NEU_NODE_NAME_LEN];
    char     group[NEU_GROUP_NAME_LEN];
    uint16_t tag_count;
    uint32_t interval;
} neu_resp_driver_group_info_t;

typedef struct {
    UT_array *groups; // array neu_resp_driver_group_info_t
} neu_resp_get_driver_group_t;

typedef struct {
    char           driver[NEU_NODE_NAME_LEN];
    char           group[NEU_GROUP_NAME_LEN];
    uint16_t       n_tag;
    neu_datatag_t *tags;
} neu_req_add_tag_t, neu_req_update_tag_t;

typedef struct {
    uint16_t index;
    int      error;
} neu_resp_add_tag_t, neu_resp_update_tag_t;

typedef struct neu_req_del_tag {
    char     driver[NEU_NODE_NAME_LEN];
    char     group[NEU_GROUP_NAME_LEN];
    uint16_t n_tag;
    char **  tags;
} neu_req_del_tag_t;

typedef struct neu_req_get_tag {
    char driver[NEU_NODE_NAME_LEN];
    char group[NEU_GROUP_NAME_LEN];
} neu_req_get_tag_t;

typedef struct neu_resp_get_tag {
    UT_array *tags; // array neu_datatag_t
} neu_resp_get_tag_t;

typedef struct {
    char app[NEU_NODE_NAME_LEN];
    char driver[NEU_NODE_NAME_LEN];
    char group[NEU_GROUP_NAME_LEN];
} neu_req_subscribe_t, neu_req_unsubscribe_t;

typedef struct neu_req_get_subscribe_group {
    char app[NEU_NODE_NAME_LEN];
} neu_req_get_subscribe_group_t, neu_req_get_sub_driver_tags_t;

typedef struct neu_resp_subscribe_info {
    char app[NEU_NODE_NAME_LEN];
    char driver[NEU_NODE_NAME_LEN];
    char group[NEU_GROUP_NAME_LEN];
} neu_resp_subscribe_info_t;

typedef struct {
    char      driver[NEU_NODE_NAME_LEN];
    char      group[NEU_GROUP_NAME_LEN];
    UT_array *tags;
} neu_resp_get_sub_driver_tags_info_t;
inline static UT_icd *neu_resp_get_sub_driver_tags_info_icd()
{
    static UT_icd icd = { sizeof(neu_resp_get_sub_driver_tags_info_t), NULL,
                          NULL, NULL };
    return &icd;
}

typedef struct {
    UT_array *infos; // array neu_resp_get_sub_driver_tags_info_t
} neu_resp_get_sub_driver_tags_t;

typedef struct neu_resp_get_subscribe_group {
    UT_array *groups; // array neu_resp_subscribe_info_t
} neu_resp_get_subscribe_group_t;

typedef struct neu_req_node_setting {
    char  node[NEU_NODE_NAME_LEN];
    char *setting;
} neu_req_node_setting_t;

typedef struct neu_req_get_node_setting {
    char ndoe[NEU_NODE_NAME_LEN];
} neu_req_get_node_setting_t;

typedef struct neu_resp_get_node_setting {
    char *setting;
} neu_resp_get_node_setting_t;

typedef enum neu_adapter_ctl {
    NEU_ADAPTER_CTL_START = 0,
    NEU_ADAPTER_CTL_STOP,
} neu_adapter_ctl_e;

typedef struct neu_req_node_ctl {
    char              node[NEU_NODE_NAME_LEN];
    neu_adapter_ctl_e ctl;
} neu_req_node_ctl_t;

typedef struct neu_req_get_node_state {
    char node[NEU_NODE_NAME_LEN];
} neu_req_get_node_state_t;

typedef struct neu_resp_get_node_state {
    neu_node_state_t state;
} neu_resp_get_node_state_t;

typedef struct neu_req_get_nodes_state {
} neu_req_get_nodes_state_t;

typedef struct {
    char             node[NEU_NODE_NAME_LEN];
    neu_node_state_t state;
} neu_nodes_state_t;

inline static UT_icd neu_nodes_state_t_icd()
{
    UT_icd icd = { sizeof(neu_nodes_state_t), NULL, NULL, NULL };

    return icd;
}
typedef struct {
    UT_array *states; // array of neu_nodes_state_t
} neu_resp_get_nodes_state_t, neu_reqresp_nodes_state_t;

typedef struct neu_req_update_license {
} neu_req_update_license_t;

typedef struct neu_req_read_group {
    char driver[NEU_NODE_NAME_LEN];
    char group[NEU_GROUP_NAME_LEN];
} neu_req_read_group_t;

typedef struct neu_req_write_tag {
    char         driver[NEU_NODE_NAME_LEN];
    char         group[NEU_GROUP_NAME_LEN];
    char         tag[NEU_TAG_NAME_LEN];
    neu_dvalue_t value;
} neu_req_write_tag_t;

typedef struct neu_resp_tag_value {
    char         tag[NEU_TAG_NAME_LEN];
    neu_dvalue_t value;
} neu_resp_tag_value_t;

typedef struct {
    char                  driver[NEU_NODE_NAME_LEN];
    char                  group[NEU_GROUP_NAME_LEN];
    uint16_t              n_tag;
    neu_resp_tag_value_t *tags;
} neu_resp_read_group_t;

typedef struct {
    char                 driver[NEU_NODE_NAME_LEN];
    char                 group[NEU_GROUP_NAME_LEN];
    uint16_t             n_tag;
    neu_resp_tag_value_t tags[];
} neu_reqresp_trans_data_t;

typedef struct {
    char node[NEU_NODE_NAME_LEN];
} neu_reqresp_node_deleted_t;

void *neu_msg_gen(neu_reqresp_head_t *header, void *data);

inline static void neu_msg_exchange(neu_reqresp_head_t *header)
{
    char tmp[NEU_NODE_NAME_LEN] = { 0 };

    strcpy(tmp, header->sender);

    memset(header->sender, 0, sizeof(header->sender));
    strcpy(header->sender, header->receiver);

    memset(header->receiver, 0, sizeof(header->receiver));
    strcpy(header->receiver, tmp);
}

typedef struct neu_adapter        neu_adapter_t;
typedef struct neu_adapter_driver neu_adapter_driver_t;
typedef struct neu_adapter_app    neu_adapter_app_t;

typedef struct adapter_callbacks {
    int (*command)(neu_adapter_t *adapter, neu_reqresp_head_t head, void *data);
    int (*response)(neu_adapter_t *adapter, neu_reqresp_head_t *head,
                    void *data);
    void (*stat_acc)(neu_adapter_t *adapter, neu_node_stat_e s, uint64_t n);

    union {
        struct {
            void (*update)(neu_adapter_t *adapter, const char *group,
                           const char *tag, neu_dvalue_t value);
            void (*write_response)(neu_adapter_t *adapter, void *req,
                                   int error);
        } driver;
    };
} adapter_callbacks_t;

#ifdef __cplusplus
}
#endif

#endif